#include "net_tcp.h"

#include "network_cmd.h"
#include "net_config.h"
#include "ota.h"
#include "socket.h"
#include "w5500.h"
#include "w5500_dhcp.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include <stdio.h>
#include <string.h>

static SemaphoreHandle_t s_net_tx_mutex = NULL;
static uint8_t g_tcp_sock_ready = 0;
static uint8_t s_phy_link_up = 1;
static uint32_t s_sock_con_ms = 0;
static uint32_t s_listen_retry_ms = 0;
static uint8_t s_sock_prev_st = SOCK_CLOSED;

/** 曾建立会话时打印断开；同 reason 1 秒内去重 */
static void net_tcp_log_disconn(const char *reason, uint8_t prev_st, uint8_t sr)
{
    static uint32_t s_last_ms;
    static char s_last_reason[12];
    uint32_t now = HAL_GetTick();

    if ((prev_st != SOCK_ESTABLISHED) && (prev_st != SOCK_CLOSE_WAIT)) {
        return;
    }
    if ((s_last_reason[0] != '\0') &&
        (strncmp(s_last_reason, reason, sizeof(s_last_reason)) == 0) &&
        ((now - s_last_ms) < 1000U)) {
        return;
    }
    (void)snprintf(s_last_reason, sizeof(s_last_reason), "%s", reason);
    s_last_ms = now;
    printf("[TCP] disconnected reason=%s prev=0x%02X sr=0x%02X\r\n",
           reason, (unsigned)prev_st, (unsigned)sr);
}

static void net_tcp_log_connected(uint8_t sr)
{
    printf("[TCP] connected sr=0x%02X\r\n", (unsigned)sr);
}

/** 发送失败但 socket 可能仍 ESTABLISHED；1 秒去重 */
static void net_tcp_log_send_fail(uint8_t sr)
{
    static uint32_t s_last_ms;
    uint32_t now = HAL_GetTick();

    if ((s_last_ms != 0U) && ((now - s_last_ms) < 1000U)) {
        return;
    }
    s_last_ms = now;
    printf("[TCP] send fail sr=0x%02X (conn may still be up)\r\n", (unsigned)sr);
}

static void net_tx_mutex_init(void)
{
    if (s_net_tx_mutex == NULL) {
        s_net_tx_mutex = xSemaphoreCreateMutex();
    }
}

static bool net_local_ip_valid(void)
{
    uint8_t ip[4];

    W5500_Get_Active_IP(ip);
    return W5500_Is_IP_Valid_Buf(ip);
}

static bool net_tcp_wait_sock_init(uint8_t sock)
{
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < NET_TCP_SOCK_INIT_WAIT_MS) {
        if (getSn_SR(sock) == SOCK_INIT) {
            return true;
        }
    }
    return (getSn_SR(sock) == SOCK_INIT);
}

static bool net_tcp_listen(uint8_t sock, uint16_t port, bool reopen)
{
    uint8_t sr;

    if (reopen) {
        close(sock);
    } else {
        printf("Opening Socket %u TCP port %u...\r\n", (unsigned)sock, (unsigned)port);
    }

    if (socket(sock, Sn_MR_TCP, port, 0x00) != 1) {
        printf("[TCP] Socket %u open failed sr=0x%02X\r\n",
               (unsigned)sock, (unsigned)getSn_SR(sock));
        return false;
    }

    if (!net_tcp_wait_sock_init(sock)) {
        printf("[TCP] Socket %u not SOCK_INIT after open sr=0x%02X\r\n",
               (unsigned)sock, (unsigned)getSn_SR(sock));
        return false;
    }

    if (!listen(sock)) {
        printf("[TCP] Socket %u listen cmd failed port %u sr=0x%02X\r\n",
               (unsigned)sock, (unsigned)port, (unsigned)getSn_SR(sock));
        return false;
    }

    sr = getSn_SR(sock);
    if (sr != SOCK_LISTEN) {
        printf("[TCP] Socket %u not LISTEN after listen port %u sr=0x%02X\r\n",
               (unsigned)sock, (unsigned)port, (unsigned)sr);
        return false;
    }

    if (!reopen) {
        printf("[TCP] Socket %u listen OK port %u sr=0x%02X\r\n",
               (unsigned)sock, (unsigned)port, (unsigned)sr);
    }
    return true;
}

static bool net_tcp_ensure_listen(void)
{
    uint8_t sr = getSn_SR(SETTING_SOCKET_NUM);

    if (sr == SOCK_LISTEN) {
        g_tcp_sock_ready = 1;
        return true;
    }

    /* PHY 短抖恢复时连接可能仍在：保留 ESTABLISHED，勿强制拆成 listen */
    if (sr == SOCK_ESTABLISHED) {
        g_tcp_sock_ready = 1;
        s_sock_prev_st = sr;
        return true;
    }

    if (net_tcp_listen(SETTING_SOCKET_NUM, SETTING_SOCKET_PORT, true)) {
        g_tcp_sock_ready = 1;
        s_sock_prev_st = getSn_SR(SETTING_SOCKET_NUM);
        return true;
    }

    g_tcp_sock_ready = 0;
    return false;
}

static void net_tcp_recover_listen(uint8_t sock, uint16_t port)
{
    uint8_t st = getSn_SR(sock);

    if (st == SOCK_ESTABLISHED || st == SOCK_CLOSE_WAIT) {
        disconnect(sock);
    }
    if (st != SOCK_CLOSED && st != SOCK_INIT && st != SOCK_LISTEN) {
        close(sock);
    }

    if (getSn_SR(sock) != SOCK_LISTEN) {
        if (net_tcp_listen(sock, port, true)) {
            g_tcp_sock_ready = 1;
        } else {
            g_tcp_sock_ready = 0;
        }
    }

    s_sock_prev_st = getSn_SR(sock);
}

static void net_phy_link_maintain(void)
{
    bool rising = false;
    bool falling = false;
    uint8_t link_up = W5500_PhyLink_DebouncedPoll(&rising, &falling);
    uint8_t phy_raw;
    uint8_t grace;
    uint8_t tcp_sr;

    if (!rising && !falling) {
        return;
    }

    phy_raw = getPHYCFGR();
    grace = W5500_Is_NetBootGrace() ? 1U : 0U;
    tcp_sr = getSn_SR(SETTING_SOCKET_NUM);
#if !NET_STATUS_LOG
    (void)phy_raw;
    (void)tcp_sr;
#endif

    /*
     * 宽限期内仍可打印 down/up（NET_STATUS_LOG=1）；
     * 仅跳过「关 TCP」动作，避免冷启动误杀连接。
     */
    if (falling) {
#if NET_STATUS_LOG
        printf("PHY link down raw=0x%02X grace=%u tcp_sr=0x%02X\r\n",
               (unsigned)phy_raw, (unsigned)grace, (unsigned)tcp_sr);
#endif
        if (grace != 0U && net_local_ip_valid()) {
            s_phy_link_up = link_up;
            return;
        }
        /* OTA 传包期间禁止因 PHY 抖动拆 TCP，否则 App 会 DATA 超时 */
        if (OTA_IsRealtimeMuted() != 0U) {
            s_phy_link_up = link_up;
            return;
        }
        s_phy_link_up = link_up;
        if ((tcp_sr == SOCK_ESTABLISHED) || (tcp_sr == SOCK_CLOSE_WAIT)) {
            net_tcp_log_disconn("PHY_DOWN", tcp_sr, tcp_sr);
        }
        close(SETTING_SOCKET_NUM);
        g_tcp_sock_ready = 0;
        return;
    }

    if (rising) {
#if NET_STATUS_LOG
        printf("PHY link up raw=0x%02X grace=%u tcp_sr=0x%02X\r\n",
               (unsigned)phy_raw, (unsigned)grace, (unsigned)tcp_sr);
#endif
        s_phy_link_up = link_up;
        if (net_local_ip_valid()) {
            tcp_sr = getSn_SR(SETTING_SOCKET_NUM);
            if (tcp_sr == SOCK_ESTABLISHED) {
                /* 短抖：会话还在，继续发 0x23，避免 App 被迫重连 */
                g_tcp_sock_ready = 1;
                s_sock_prev_st = tcp_sr;
#if NET_STATUS_LOG
                printf("[TCP] keep ESTABLISHED after PHY up\r\n");
#endif
            } else {
                (void)net_tcp_ensure_listen();
            }
        }
    }
}

static uint16_t net_tcp_socket_recv(uint8_t sock, uint8_t *rdata, uint16_t cap)
{
    uint16_t rx_size;
    uint16_t n;

    if ((rdata == NULL) || (cap == 0U) || (getSn_SR(sock) != SOCK_ESTABLISHED)) {
        return 0;
    }

    rx_size = getSn_RX_RSR(sock);
    if (rx_size == 0U) {
        return 0;
    }

    /* 勿整包丢弃：超过 cap 时只取前 cap，剩余下次 PollRx 再取 */
    n = (rx_size > cap) ? cap : rx_size;
    n = recv(sock, rdata, n);
    return (n > cap) ? cap : n;
}

static void net_tcp_socket_maintain(uint8_t sock, uint16_t port)
{
    uint8_t status = getSn_SR(sock);
    uint8_t ir = getSn_IR(sock);
    uint32_t now = HAL_GetTick();
    uint8_t prev_st = s_sock_prev_st;

    if ((ir & Sn_IR_CON) && (status == SOCK_ESTABLISHED) && (prev_st != SOCK_ESTABLISHED)) {
        setSn_IR(sock, Sn_IR_CON);
        s_sock_con_ms = now;
        net_tcp_log_connected(status);
    } else if (ir & Sn_IR_CON) {
        setSn_IR(sock, Sn_IR_CON);
    }

    if (ir & Sn_IR_DISCON) {
        net_tcp_log_disconn("DISCON", prev_st, status);
    }
    if (ir & Sn_IR_TIMEOUT) {
        net_tcp_log_disconn("TIMEOUT", prev_st, status);
    }
    if (ir & (Sn_IR_DISCON | Sn_IR_TIMEOUT)) {
        setSn_IR(sock, ir & (Sn_IR_DISCON | Sn_IR_TIMEOUT));
        net_tcp_recover_listen(sock, port);
        return;
    }

    status = getSn_SR(sock);
    switch (status) {
    case SOCK_LISTEN:
    case SOCK_ESTABLISHED:
        break;

    case SOCK_CLOSE_WAIT:
        if (prev_st == SOCK_ESTABLISHED) {
            net_tcp_log_disconn("CLOSE_WAIT", prev_st, status);
            disconnect(sock);
        } else if (prev_st == SOCK_CLOSE_WAIT) {
            net_tcp_recover_listen(sock, port);
        }
        break;

    case SOCK_CLOSING:
    case SOCK_FIN_WAIT:
    case SOCK_TIME_WAIT:
    case SOCK_LAST_ACK:
        if (prev_st != status) {
            net_tcp_log_disconn("CLOSING", prev_st, status);
            close(sock);
        } else {
            net_tcp_recover_listen(sock, port);
        }
        break;

    case SOCK_CLOSED:
    case SOCK_INIT:
        if (net_tcp_listen(sock, port, true)) {
            g_tcp_sock_ready = 1;
        } else {
            g_tcp_sock_ready = 0;
        }
        break;

    case SOCK_SYNSENT:
    case SOCK_SYNRECV:
        if (prev_st != status) {
            s_sock_con_ms = now;
        } else if ((now - s_sock_con_ms) >= NET_TCP_SYN_HOLD_MS) {
            net_tcp_log_disconn("SYN_TIMEOUT", prev_st, status);
            net_tcp_recover_listen(sock, port);
        }
        break;

    default:
        break;
    }

    s_sock_prev_st = getSn_SR(sock);
}

bool Net_Tcp_Init(void)
{
    net_tx_mutex_init();
    g_tcp_sock_ready = 0;

    if (net_tcp_listen(SETTING_SOCKET_NUM, SETTING_SOCKET_PORT, false)) {
        g_tcp_sock_ready = 1;
    }

    s_phy_link_up = (getPHYCFGR() & LINK) ? 1u : 0u;
    s_sock_con_ms = 0;
    s_sock_prev_st = SOCK_CLOSED;

    if (g_tcp_sock_ready) {
        printf("TCP listen port %u (socket %u)\r\n",
               (unsigned)SETTING_SOCKET_PORT, (unsigned)SETTING_SOCKET_NUM);
    }
    return (g_tcp_sock_ready != 0U);
}

void Net_Tcp_RebindOnIpChange(void)
{
    if (!net_local_ip_valid()) {
        printf("[TCP] skip listen rebound: IP invalid\r\n");
        g_tcp_sock_ready = 0;
        return;
    }

    s_sock_con_ms = 0;
    s_sock_prev_st = SOCK_CLOSED;
    s_listen_retry_ms = HAL_GetTick();

    if (net_tcp_ensure_listen()) {
        printf("[TCP] listen rebound OK port %u sr=0x%02X\r\n",
               (unsigned)SETTING_SOCKET_PORT,
               (unsigned)getSn_SR(SETTING_SOCKET_NUM));
    } else {
        printf("[TCP] listen rebound FAILED port %u sr=0x%02X\r\n",
               (unsigned)SETTING_SOCKET_PORT,
               (unsigned)getSn_SR(SETTING_SOCKET_NUM));
    }
}

void Net_Tcp_DeInit(void)
{
    if (g_tcp_sock_ready) {
        close(SETTING_SOCKET_NUM);
        g_tcp_sock_ready = 0;
        printf("TCP socket %u closed\r\n", (unsigned)SETTING_SOCKET_NUM);
    }
}

void Net_Tcp_PeriodicMaintain(void)
{
    uint32_t now;
    uint8_t sr;

    if (W5500_Is_Network_Recovering()) {
        return;
    }

    /* OTA：只保活已建立连接，不做 listen 强拉/PHY 维护，避免误杀会话 */
    if (OTA_IsRealtimeMuted() != 0U) {
        sr = getSn_SR(SETTING_SOCKET_NUM);
        if (sr == SOCK_ESTABLISHED) {
            g_tcp_sock_ready = 1;
            s_sock_prev_st = sr;
            return;
        }
        if (sr == SOCK_CLOSE_WAIT) {
            net_tcp_log_disconn("CLOSE_WAIT", s_sock_prev_st, sr);
            net_tcp_recover_listen(SETTING_SOCKET_NUM, SETTING_SOCKET_PORT);
        }
        return;
    }

    net_phy_link_maintain();

    if (!net_local_ip_valid()) {
        g_tcp_sock_ready = 0;
        return;
    }

    if (!g_tcp_sock_ready || (getSn_SR(SETTING_SOCKET_NUM) != SOCK_LISTEN &&
                              getSn_SR(SETTING_SOCKET_NUM) != SOCK_ESTABLISHED)) {
        now = HAL_GetTick();
        if ((now - s_listen_retry_ms) >= NET_TCP_LISTEN_RETRY_MS) {
            s_listen_retry_ms = now;
            if (net_tcp_ensure_listen()) {
                printf("[TCP] listen recovered port %u sr=0x%02X\r\n",
                       (unsigned)SETTING_SOCKET_PORT,
                       (unsigned)getSn_SR(SETTING_SOCKET_NUM));
            }
        }
        if (!g_tcp_sock_ready) {
            return;
        }
    }

    net_tcp_socket_maintain(SETTING_SOCKET_NUM, SETTING_SOCKET_PORT);
}

bool Net_Tcp_IsConnected(void)
{
    return (g_tcp_sock_ready != 0U) &&
           (getSn_SR(SETTING_SOCKET_NUM) == SOCK_ESTABLISHED);
}

int Net_Tcp_Write(const uint8_t *data, uint16_t len)
{
#if NET_STATUS_LOG
    static uint32_t s_fail_log_tick = 0U;
    const char *why = NULL;
    uint32_t now;
#endif
    uint16_t sent;
    bool ok = false;

    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    if (W5500_Is_Network_Recovering()) {
#if NET_STATUS_LOG
        why = "recovering";
#endif
        goto fail_log;
    }

    if (!Net_Tcp_IsConnected()) {
#if NET_STATUS_LOG
        why = "not_conn";
#endif
        goto fail_log;
    }

    net_tx_mutex_init();
    if (s_net_tx_mutex != NULL) {
        if (xSemaphoreTake(s_net_tx_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
#if NET_STATUS_LOG
            why = "mutex";
#endif
            goto fail_log;
        }
    }

    sent = send(SETTING_SOCKET_NUM, (uint8_t *)data, len);
    if (sent == len) {
        ok = true;
    } else {
#if NET_STATUS_LOG
        why = "send_err";
#endif
        if (Net_Tcp_IsConnected()) {
            net_tcp_log_send_fail(getSn_SR(SETTING_SOCKET_NUM));
        }
        net_tcp_socket_maintain(SETTING_SOCKET_NUM, SETTING_SOCKET_PORT);
    }

    if (s_net_tx_mutex != NULL) {
        (void)xSemaphoreGive(s_net_tx_mutex);
    }

    if (ok) {
        return (int)len;
    }

fail_log:
#if NET_STATUS_LOG
    now = HAL_GetTick();
    if ((why != NULL) &&
        ((s_fail_log_tick == 0U) || ((now - s_fail_log_tick) >= 1000U))) {
        uint8_t phy_raw = getPHYCFGR();

        s_fail_log_tick = now;
        printf("[TCP] tx skip why=%s sr=0x%02X phy=0x%02X link=%u\r\n",
               why,
               (unsigned)getSn_SR(SETTING_SOCKET_NUM),
               (unsigned)phy_raw,
               (unsigned)((phy_raw & LINK) ? 1U : 0U));
    }
#endif
    return -1;
}

void Net_Tcp_PollRx(UartRingBuf *rx_ring)
{
    static uint8_t scratch[NET_TCP_RX_CAP];
    uint16_t n;
    uint16_t i;

    if (rx_ring == NULL) {
        return;
    }

    if (W5500_Is_Network_Recovering()) {
        return;
    }

    Net_Tcp_PeriodicMaintain();

    if (!g_tcp_sock_ready || (getSn_SR(SETTING_SOCKET_NUM) != SOCK_ESTABLISHED)) {
        return;
    }

    n = net_tcp_socket_recv(SETTING_SOCKET_NUM, scratch, sizeof(scratch));
    for (i = 0; i < n; i++) {
        if (!UartRingBuf_Push(rx_ring, scratch[i])) {
            break;
        }
    }

    if (getSn_IR(SETTING_SOCKET_NUM) & Sn_IR_TIMEOUT) {
        setSn_IR(SETTING_SOCKET_NUM, Sn_IR_TIMEOUT);
        net_tcp_socket_maintain(SETTING_SOCKET_NUM, SETTING_SOCKET_PORT);
    }
}
