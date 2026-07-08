#include "protocol.h"

#include "usart.h"
#include "can.h"
#include "config_flash.h"
#include "protec_protocol.h"
#include "ota.h"
#include "main.h"

#include <string.h>
#include <stdio.h>

/* 1=USART1 打印 LoRa 完整帧 hex（CRC 通过后）；接 App 时改 0 */
#define LORA_RX_LOG           0
#define LORA_LOG_RX_HEX_MAX   64U

#if LORA_RX_LOG
static void Lora_LogFrame(const uint8_t *data, uint16_t len)
{
  uint16_t i;
  uint16_t show;

  if ((data == NULL) || (len == 0U))
  {
    return;
  }

  show = len;
  if (show > LORA_LOG_RX_HEX_MAX)
  {
    show = LORA_LOG_RX_HEX_MAX;
  }

  printf("[LORA] frame addr=%02X %u bytes:", (unsigned)data[0], (unsigned)len);
  for (i = 0U; i < show; i++)
  {
    printf(" %02X", (unsigned)data[i]);
  }
  if (len > show)
  {
    printf(" ...");
  }
  printf("\r\n");
}
#endif

/* 使用 0xEF 作为本机协议地址（低字节），后续也可用 g_config.address */
#define PROTOCOL_ADDR          0xEFU

/* 与 s_rx_buf / CAN 组包缓存一致的最大 RTU 帧长 */
#define PROTOCOL_RTU_MAX_FRAME_LEN    256U
/* Protocol_RtuFrameLen：长度尚未能判定（等更多字节） */
#define PROTOCOL_RTU_FL_NEED_MORE     0xFFFFU

/* control_bit2 对应的寄存器地址（十进制 123 -> 0x007B） */
#define REG_CONTROL_BIT2_DEC   123U
#define REG_CONTROL_BIT2_LO    ((uint8_t)(REG_CONTROL_BIT2_DEC & 0xFFU))
#define REG_CONTROL_BIT2_HI    ((uint8_t)((REG_CONTROL_BIT2_DEC >> 8) & 0xFFU))

/* 重启寄存器地址（十进制 120 -> 0x0078），写入 0x0001 触发重启 */
#define REG_REBOOT_DEC         120U
#define REG_REBOOT_LO          ((uint8_t)(REG_REBOOT_DEC & 0xFFU))
#define REG_REBOOT_HI          ((uint8_t)((REG_REBOOT_DEC >> 8) & 0xFFU))

/* 序列号寄存器起始地址（十进制 86 -> 0x0056），长度 8 个寄存器 = 16 字节 */
#define REG_SERIALNUM_DEC      86U
#define REG_SERIALNUM_LO       ((uint8_t)(REG_SERIALNUM_DEC & 0xFFU))
#define REG_SERIALNUM_HI       ((uint8_t)((REG_SERIALNUM_DEC >> 8) & 0xFFU))
#define REG_SERIALNUM_QTY      8U
#define REG_SERIALNUM_BYTES    16U

/* 软件版本号寄存器起始地址（十进制 98 -> 0x0062），长度 10 个寄存器 = 20 字节 */
#define REG_SWVER_DEC          98U
#define REG_SWVER_LO           ((uint8_t)(REG_SWVER_DEC & 0xFFU))
#define REG_SWVER_HI           ((uint8_t)((REG_SWVER_DEC >> 8) & 0xFFU))
#define REG_SWVER_QTY          10U
#define REG_SWVER_BYTES        20U

/* OTA 数据包最大 128 字节数据 + 帧头尾 = 137 字节，留余量用 256 */
static uint8_t s_rx_buf[256];
static uint16_t s_rx_len = 0U;
static uint8_t s_lora_rx_buf[256];
static uint16_t s_lora_rx_len = 0U;

static uint16_t Protocol_CalcCrc(const uint8_t *buf, uint16_t len)
{
  uint16_t crc = 0xFFFFU;
  uint16_t i;
  uint8_t j;

  for (i = 0U; i < len; i++)
  {
    crc ^= (uint16_t)buf[i];
    for (j = 0U; j < 8U; j++)
    {
      if ((crc & 0x0001U) != 0U)
      {
        crc >>= 1;
        crc ^= 0xA001U;
      }
      else
      {
        crc >>= 1;
      }
    }
  }

  return crc;
}

/* 按协议整理文档：请求/应答/主动上传/错误应答，计算整帧长度（含 CRC）。
 * 返回 PROTOCOL_RTU_FL_NEED_MORE：字节不够，还不能算 frame_len。
 * 返回 0：不认识的功能码，应丢首字节重同步。
 */
static uint16_t Protocol_RtuFrameLen(const uint8_t *buf, uint16_t avail)
{
  if (avail < 2U)
  {
    return PROTOCOL_RTU_FL_NEED_MORE;
  }

  uint8_t func = buf[1];

  switch (func)
  {
  /* 主机请求 */
  case 0x03U:
  case 0x05U:
  case 0x06U:
  /* 从机应答（写/读单寄存器、写多寄存器、错误应答） */
  case 0x15U:
  case 0x16U:
  case 0x20U:
  case 0x25U:
  case 0x83U:
  case 0x85U:
  case 0x86U:
  case 0x90U:
    return 8U;
  default:
    break;
  }

  /* 写多个寄存器请求 */
  if (func == 0x10U)
  {
    if (avail < 9U)
    {
      return PROTOCOL_RTU_FL_NEED_MORE;
    }
    uint16_t byte_cnt = buf[6];
    uint16_t fl = (uint16_t)(7U + byte_cnt + 2U);
    if (fl > PROTOCOL_RTU_MAX_FRAME_LEN)
    {
      return 0U;
    }
    return fl;
  }

  /* 读多个应答 / 主动上传（0x13 / 0x23）：[byte_count][start_lo][start_hi][data][crc] */
  if ((func == 0x13U) || (func == 0x23U))
  {
    if (avail < 3U)
    {
      return PROTOCOL_RTU_FL_NEED_MORE;
    }
    uint16_t byte_cnt = buf[2];
    uint16_t fl = (uint16_t)(7U + byte_cnt);
    if (fl > PROTOCOL_RTU_MAX_FRAME_LEN)
    {
      return 0U;
    }
    return fl;
  }

  return 0U;
}

static void Protocol_Send(uint8_t *buf, uint16_t len)
{
  (void)USART1_Tx(buf, len, 100U);
}

static void Protocol_ForwardFromLora(const uint8_t *buf, uint16_t len);
static void Protocol_HandleWriteMultiple(const uint8_t *frame, uint16_t len);
static void Protocol_HandleWriteSingle(const uint8_t *frame, uint16_t len);
static void Protocol_HandleReadMultiple(const uint8_t *frame, uint16_t len);

static void Protocol_LoraParseStream(void)
{
  for (;;)
  {
    uint16_t frame_len = Protocol_RtuFrameLen(s_lora_rx_buf, s_lora_rx_len);
    if (frame_len == PROTOCOL_RTU_FL_NEED_MORE)
    {
      return;
    }
    if (frame_len == 0U)
    {
      if (s_lora_rx_len < 1U)
      {
        return;
      }
      memmove(s_lora_rx_buf, &s_lora_rx_buf[1], s_lora_rx_len - 1U);
      s_lora_rx_len--;
      continue;
    }
    if (frame_len > sizeof(s_lora_rx_buf))
    {
      memmove(s_lora_rx_buf, &s_lora_rx_buf[1], s_lora_rx_len - 1U);
      s_lora_rx_len--;
      continue;
    }

    uint8_t addr = s_lora_rx_buf[0];
    uint8_t func = s_lora_rx_buf[1];

    if (s_lora_rx_len < frame_len)
    {
      return;
    }

    uint16_t crc_calc = Protocol_CalcCrc(s_lora_rx_buf, (uint16_t)(frame_len - 2U));
    uint16_t crc_recv = (uint16_t)s_lora_rx_buf[frame_len - 2U] |
                        ((uint16_t)s_lora_rx_buf[frame_len - 1U] << 8);
    if (crc_calc != crc_recv)
    {
      memmove(s_lora_rx_buf, &s_lora_rx_buf[1], s_lora_rx_len - 1U);
      s_lora_rx_len--;
      continue;
    }

#if LORA_RX_LOG
    Lora_LogFrame(s_lora_rx_buf, frame_len);
#endif

    if (addr != (uint8_t)PROTOCOL_ADDR)
    {
      Protocol_ForwardFromLora(s_lora_rx_buf, frame_len);
    }
    else if (func == 0x10U)
    {
      Protocol_HandleWriteMultiple(s_lora_rx_buf, frame_len);
    }
    else if (func == 0x06U)
    {
      Protocol_HandleWriteSingle(s_lora_rx_buf, frame_len);
    }
    else if (func == 0x03U)
    {
      Protocol_HandleReadMultiple(s_lora_rx_buf, frame_len);
    }

    if (s_lora_rx_len > frame_len)
    {
      memmove(s_lora_rx_buf, &s_lora_rx_buf[frame_len], s_lora_rx_len - frame_len);
      s_lora_rx_len = (uint16_t)(s_lora_rx_len - frame_len);
    }
    else
    {
      s_lora_rx_len = 0U;
      return;
    }
  }
}

/* ---------------- CAN Rx -> 组帧 -> USART1 转发 ---------------- */
#define CAN_RX_QUEUE_SIZE       64U
#define CAN_RX_MAX_SLAVES       5U
#define CAN_RX_CACHE_SIZE       256U
#define CAN_RX_STALE_TIMEOUT_MS 200U

typedef struct
{
  uint16_t stdId;
  uint8_t dlc;
  uint8_t data[8];
  uint32_t tick;
} CanRxItem;

typedef struct
{
  uint8_t addr; /* 0 表示未占用 */
  uint8_t buf[CAN_RX_CACHE_SIZE];
  uint16_t len;
  uint32_t last_tick;
} CanSlaveCache;

static volatile uint16_t s_can_rx_head = 0U;
static volatile uint16_t s_can_rx_tail = 0U;
static CanRxItem s_can_rx_q[CAN_RX_QUEUE_SIZE];
static CanSlaveCache s_can_cache[CAN_RX_MAX_SLAVES];

static uint8_t CanRxQ_Pop(CanRxItem *out)
{
  if (s_can_rx_tail == s_can_rx_head)
  {
    return 0U;
  }
  *out = s_can_rx_q[s_can_rx_tail];
  s_can_rx_tail = (uint16_t)((s_can_rx_tail + 1U) % CAN_RX_QUEUE_SIZE);
  return 1U;
}

static CanSlaveCache *CanCache_GetOrAlloc(uint8_t addr)
{
  uint8_t i;
  CanSlaveCache *free_slot = NULL;

  for (i = 0U; i < CAN_RX_MAX_SLAVES; i++)
  {
    if (s_can_cache[i].addr == addr)
      return &s_can_cache[i];
    if (s_can_cache[i].addr == 0U && free_slot == NULL)
      free_slot = &s_can_cache[i];
  }

  if (free_slot != NULL)
  {
    free_slot->addr = addr;
    free_slot->len = 0U;
    free_slot->last_tick = HAL_GetTick();
    return free_slot;
  }

  /* 没有空位：复用“最久未更新”的槽位 */
  {
    uint8_t oldest = 0U;
    uint32_t oldest_tick = s_can_cache[0].last_tick;
    for (i = 1U; i < CAN_RX_MAX_SLAVES; i++)
    {
      if (s_can_cache[i].last_tick < oldest_tick)
      {
        oldest_tick = s_can_cache[i].last_tick;
        oldest = i;
      }
    }
    s_can_cache[oldest].addr = addr;
    s_can_cache[oldest].len = 0U;
    s_can_cache[oldest].last_tick = HAL_GetTick();
    return &s_can_cache[oldest];
  }
}

static void CanCache_Append(CanSlaveCache *c, const uint8_t *data, uint8_t dlc, uint32_t tick)
{
  if (c == NULL || dlc == 0U)
  {
    return;
  }
  c->last_tick = tick;

  if ((uint32_t)c->len + (uint32_t)dlc > (uint32_t)sizeof(c->buf))
  {
    /* 溢出：直接丢弃旧缓存，重新开始对齐 */
    c->len = 0U;
  }
  memcpy(&c->buf[c->len], data, dlc);
  c->len = (uint16_t)(c->len + dlc);
}

static void CanCache_ClearStale(void)
{
  uint32_t now = HAL_GetTick();
  uint8_t i;
  for (i = 0U; i < CAN_RX_MAX_SLAVES; i++)
  {
    if (s_can_cache[i].addr != 0U && s_can_cache[i].len != 0U)
    {
      if ((now - s_can_cache[i].last_tick) > CAN_RX_STALE_TIMEOUT_MS)
      {
        s_can_cache[i].len = 0U;
      }
    }
  }
}

static void CanCache_TryParseAndForward(CanSlaveCache *c)
{
  for (;;)
  {
    uint16_t frame_len = Protocol_RtuFrameLen(c->buf, c->len);
    if (frame_len == PROTOCOL_RTU_FL_NEED_MORE)
    {
      return;
    }
    if (frame_len == 0U)
    {
      if (c->len < 1U)
      {
        return;
      }
      memmove(c->buf, &c->buf[1], c->len - 1U);
      c->len--;
      continue;
    }
    if (frame_len > sizeof(c->buf))
    {
      memmove(c->buf, &c->buf[1], c->len - 1U);
      c->len--;
      continue;
    }

    if (c->len < frame_len)
    {
      return;
    }

    uint16_t crc_calc = Protocol_CalcCrc(c->buf, (uint16_t)(frame_len - 2U));
    uint16_t crc_recv = (uint16_t)c->buf[frame_len - 2U] |
                        ((uint16_t)c->buf[frame_len - 1U] << 8);
    if (crc_calc != crc_recv)
    {
      memmove(c->buf, &c->buf[1], c->len - 1U);
      c->len--;
      continue;
    }

    /* CRC 通过：CAN 始终转发；LoRa 转发看 bit9 */
    {
      uint8_t addr = c->buf[0];
      (void)USART1_Tx(c->buf, frame_len, 100U);
      if ((addr != (uint8_t)PROTOCOL_ADDR) && Config_LoraEnabled())
      {
        (void)USART2_Tx(c->buf, frame_len, 100U);
      }
    }

    if (c->len > frame_len)
    {
      memmove(c->buf, &c->buf[frame_len], c->len - frame_len);
      c->len = (uint16_t)(c->len - frame_len);
    }
    else
    {
      c->len = 0U;
      return;
    }
  }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_cb)
{
  CAN_RxHeaderTypeDef rxHeader;
  uint8_t data[8];
  uint16_t next;

  if (hcan_cb != &hcan)
  {
    return;
  }

  if (HAL_CAN_GetRxMessage(hcan_cb, CAN_RX_FIFO0, &rxHeader, data) != HAL_OK)
  {
    return;
  }

  if (rxHeader.IDE != CAN_ID_STD)
  {
    return;
  }

  next = (uint16_t)((s_can_rx_head + 1U) % CAN_RX_QUEUE_SIZE);
  if (next == s_can_rx_tail)
  {
    return;
  }

  s_can_rx_q[s_can_rx_head].stdId = (uint16_t)rxHeader.StdId;
  s_can_rx_q[s_can_rx_head].dlc = (uint8_t)rxHeader.DLC;
  memcpy(s_can_rx_q[s_can_rx_head].data, data, 8U);
  s_can_rx_q[s_can_rx_head].tick = HAL_GetTick();
  s_can_rx_head = next;
}

void Protocol_OnCanFrames(void)
{
  CanRxItem item;
  uint16_t loops = 0U;

  CanCache_ClearStale();

  while (CanRxQ_Pop(&item) != 0U)
  {
    uint8_t addr = (uint8_t)(item.stdId & 0xFFU);
    CanSlaveCache *c = CanCache_GetOrAlloc(addr);
    CanCache_Append(c, item.data, item.dlc, item.tick);
    CanCache_TryParseAndForward(c);

    loops++;
    if (loops > 128U)
    {
      break;
    }
  }
}

/* 转发到 CAN：StdId = RTU addr，8 字节切片 */
static void Protocol_ForwardToCan(const uint8_t *buf, uint16_t len)
{
  if (len < 1U)
  {
    return;
  }

  uint8_t addr = buf[0];

  CAN_TxHeaderTypeDef txHeader;
  uint32_t txMailbox;
  uint8_t data[8];

  txHeader.StdId = (uint32_t)addr;
  txHeader.ExtId = 0U;
  txHeader.IDE = CAN_ID_STD;
  txHeader.RTR = CAN_RTR_DATA;
  txHeader.TransmitGlobalTime = DISABLE;

  uint16_t offset = 0U;
  while (offset < len)
  {
    uint8_t dlc = (uint8_t)(((len - offset) > 8U) ? 8U : (len - offset));
    memset(data, 0, sizeof(data));
    memcpy(data, &buf[offset], dlc);

    txHeader.DLC = dlc;
    (void)HAL_CAN_AddTxMessage(&hcan, &txHeader, data, &txMailbox);
    offset = (uint16_t)(offset + dlc);
    if (offset < len)
    {
      HAL_Delay(1U);
    }
  }
}

/* App/CAN → LoRa + CAN */
static void Protocol_ForwardFromApp(const uint8_t *buf, uint16_t len)
{
  if (Config_LoraEnabled())
  {
    (void)USART2_Tx(buf, len, 100U);
  }
  Protocol_ForwardToCan(buf, len);
}

/* LoRa → App + CAN */
static void Protocol_ForwardFromLora(const uint8_t *buf, uint16_t len)
{
  if (!Config_LoraEnabled())
  {
    return;
  }

  (void)USART1_Tx(buf, len, 100U);
  Protocol_ForwardToCan(buf, len);
}

static void Protocol_HandleWriteMultiple(const uint8_t *frame, uint16_t len)
{
  if (len < 11U)
  {
    return;
  }

  uint8_t addr = frame[0];
  uint8_t func = frame[1];
  uint8_t reg_lo = frame[2];
  uint8_t reg_hi = frame[3];
  uint8_t qty_lo = frame[4];
  uint8_t qty_hi = frame[5];
  uint8_t byte_cnt = frame[6];

  (void)addr;
  (void)func;

  if ((uint16_t)byte_cnt + 9U != len)
  {
    return;
  }

  /* 写 control_bit2: 起始寄存器 REG_CONTROL_BIT2_DEC, 数量2, 字节数4 */
  if ((reg_lo == REG_CONTROL_BIT2_LO) &&
      (reg_hi == REG_CONTROL_BIT2_HI) &&
      (qty_lo == 0x02U) &&
      (qty_hi == 0x00U) &&
      (byte_cnt == 4U))
  {
    uint32_t v = (uint32_t)frame[7] |
                 ((uint32_t)frame[8] << 8) |
                 ((uint32_t)frame[9] << 16) |
                 ((uint32_t)frame[10] << 24);

    g_config.control_bit2 = v;
    Config_Save();
    Config_ApplyIoOutputs();

    /* 应答帧：地址, 0x20, 起始寄存器, 数量, CRC */
    uint8_t resp[8];
    uint16_t crc;

    resp[0] = PROTOCOL_ADDR;
    resp[1] = 0x20U;
    resp[2] = reg_lo;
    resp[3] = reg_hi;
    resp[4] = qty_lo;
    resp[5] = qty_hi;
    crc = Protocol_CalcCrc(resp, 6U);
    resp[6] = (uint8_t)(crc & 0xFFU);
    resp[7] = (uint8_t)((crc >> 8) & 0xFFU);

    Protocol_Send(resp, 8U);
  }
  /* 写序列号 serialnum: 起始寄存器 REG_SERIALNUM_DEC, 数量 REG_SERIALNUM_QTY, 字节数 REG_SERIALNUM_BYTES */
  else if ((reg_lo == REG_SERIALNUM_LO) && (reg_hi == REG_SERIALNUM_HI) &&
           (qty_lo == REG_SERIALNUM_QTY) && (qty_hi == 0x00U) &&
           (byte_cnt == REG_SERIALNUM_BYTES))
  {
    memcpy(g_config.serialnum, &frame[7], 16U);
    Config_Save();

    uint8_t resp[8];
    uint16_t crc;

    resp[0] = PROTOCOL_ADDR;
    resp[1] = 0x20U;
    resp[2] = reg_lo;
    resp[3] = reg_hi;
    resp[4] = qty_lo;
    resp[5] = qty_hi;
    crc = Protocol_CalcCrc(resp, 6U);
    resp[6] = (uint8_t)(crc & 0xFFU);
    resp[7] = (uint8_t)((crc >> 8) & 0xFFU);

    Protocol_Send(resp, 8U);
  }
  /* ---- OTA: 开始升级 (REG_OTA_START=200, qty=2, byte_cnt=4, data=total_size) ---- */
  else if ((reg_lo == (uint8_t)(REG_OTA_START & 0xFFU)) &&
           (reg_hi == (uint8_t)((REG_OTA_START >> 8) & 0xFFU)) &&
           (qty_lo == 0x02U) && (qty_hi == 0x00U) && (byte_cnt == 4U))
  {
    uint32_t total = (uint32_t)frame[7]  |
                     ((uint32_t)frame[8]  << 8)  |
                     ((uint32_t)frame[9]  << 16) |
                     ((uint32_t)frame[10] << 24);

    uint8_t resp[8];
    uint16_t crc;
    uint8_t result_byte;

    if (OTA_StartSession(total) == 0)
    {
      result_byte = 0x00U;  /* 成功 */
    }
    else
    {
      result_byte = 0x01U;  /* 失败 */
    }

    resp[0] = PROTOCOL_ADDR;
    resp[1] = 0x20U;
    resp[2] = reg_lo;
    resp[3] = reg_hi;
    resp[4] = qty_lo;
    resp[5] = qty_hi;
    crc = Protocol_CalcCrc(resp, 6U);
    resp[6] = (uint8_t)(crc & 0xFFU);
    resp[7] = (uint8_t)((crc >> 8) & 0xFFU);
    (void)result_byte;
    Protocol_Send(resp, 8U);
  }
  /* ---- OTA: 结束升级 (REG_OTA_DONE=202, qty=2, byte_cnt=4, data=crc32) ---- */
  else if ((reg_lo == (uint8_t)(REG_OTA_DONE & 0xFFU)) &&
           (reg_hi == (uint8_t)((REG_OTA_DONE >> 8) & 0xFFU)) &&
           (qty_lo == 0x02U) && (qty_hi == 0x00U) && (byte_cnt == 4U))
  {
    uint32_t crc32 = (uint32_t)frame[7]  |
                     ((uint32_t)frame[8]  << 8)  |
                     ((uint32_t)frame[9]  << 16) |
                     ((uint32_t)frame[10] << 24);

    uint8_t resp[8];
    uint16_t crc;

    resp[0] = PROTOCOL_ADDR;
    resp[1] = 0x20U;
    resp[2] = reg_lo;
    resp[3] = reg_hi;
    resp[4] = qty_lo;
    resp[5] = qty_hi;
    crc = Protocol_CalcCrc(resp, 6U);
    resp[6] = (uint8_t)(crc & 0xFFU);
    resp[7] = (uint8_t)((crc >> 8) & 0xFFU);
    Protocol_Send(resp, 8U);

    if (OTA_Finish(crc32) == 0)
    {
      /* 校验通过，OTA Flag 已写入，延迟后重启进入 Bootloader */
      HAL_Delay(50U);
      NVIC_SystemReset();
    }
    /* 校验失败：状态机变 ERROR，上位机下次读 REG_OTA_STATUS 可得知 */
  }
  /* ---- OTA: 数据包 (REG_OTA_DATA=208, qty=1~64, byte_cnt=2~128) ---- */
  else if ((reg_lo == (uint8_t)(REG_OTA_DATA & 0xFFU)) &&
           (reg_hi == (uint8_t)((REG_OTA_DATA >> 8) & 0xFFU)) &&
           (qty_hi == 0x00U) && (qty_lo >= 1U) && (qty_lo <= 64U) &&
           (byte_cnt == (uint8_t)(qty_lo * 2U)))
  {
    uint8_t resp[8];
    uint16_t crc;

    (void)OTA_WriteChunk(&frame[7], (uint16_t)byte_cnt);

    resp[0] = PROTOCOL_ADDR;
    resp[1] = 0x20U;
    resp[2] = reg_lo;
    resp[3] = reg_hi;
    resp[4] = qty_lo;
    resp[5] = qty_hi;
    crc = Protocol_CalcCrc(resp, 6U);
    resp[6] = (uint8_t)(crc & 0xFFU);
    resp[7] = (uint8_t)((crc >> 8) & 0xFFU);
    Protocol_Send(resp, 8U);
  }
}

static void Protocol_HandleReadMultiple(const uint8_t *frame, uint16_t len)
{
  if (len < 8U)
  {
    return;
  }

  uint8_t addr = frame[0];
  uint8_t func = frame[1];
  uint8_t reg_lo = frame[2];
  uint8_t reg_hi = frame[3];
  uint8_t qty_lo = frame[4];
  uint8_t qty_hi = frame[5];

  (void)addr;
  (void)func;

  /* 读 control_bit2: 起始寄存器 REG_CONTROL_BIT2_DEC, 数量2 */
  if ((reg_lo == REG_CONTROL_BIT2_LO) &&
      (reg_hi == REG_CONTROL_BIT2_HI) &&
      (qty_lo == 0x02U) &&
      (qty_hi == 0x00U))
  {
    uint8_t resp[16];
    uint16_t crc;
    uint32_t v = g_config.control_bit2;

    resp[0] = PROTOCOL_ADDR;
    resp[1] = 0x13U;  /* 读多个寄存器返回 */
    resp[2] = 0x04U;  /* 字节数 */
    resp[3] = reg_lo;
    resp[4] = reg_hi;
    resp[5] = (uint8_t)(v & 0xFFU);
    resp[6] = (uint8_t)((v >> 8) & 0xFFU);
    resp[7] = (uint8_t)((v >> 16) & 0xFFU);
    resp[8] = (uint8_t)((v >> 24) & 0xFFU);

    crc = Protocol_CalcCrc(resp, 9U);
    resp[9]  = (uint8_t)(crc & 0xFFU);
    resp[10] = (uint8_t)((crc >> 8) & 0xFFU);

    Protocol_Send(resp, 11U);
  }
  /* 读序列号: 起始寄存器 REG_SERIALNUM_DEC, 数量 REG_SERIALNUM_QTY, 返回 16 字节 serialnum */
  else if ((reg_lo == REG_SERIALNUM_LO) && (reg_hi == REG_SERIALNUM_HI) &&
           (qty_lo == REG_SERIALNUM_QTY) && (qty_hi == 0x00U))
  {
    uint8_t resp[32];
    uint16_t crc;

    resp[0] = PROTOCOL_ADDR;
    resp[1] = 0x13U;
    resp[2] = 0x10U;  /* 16 字节 */
    resp[3] = reg_lo;
    resp[4] = reg_hi;
    memcpy(&resp[5], g_config.serialnum, 16U);

    crc = Protocol_CalcCrc(resp, 21U);
    resp[21] = (uint8_t)(crc & 0xFFU);
    resp[22] = (uint8_t)((crc >> 8) & 0xFFU);

    Protocol_Send(resp, 23U);
  }
  /* 读软件版本号: 起始寄存器 REG_SWVER_DEC, 数量 REG_SWVER_QTY, 返回 20 字节 */
  else if ((reg_lo == REG_SWVER_LO) && (reg_hi == REG_SWVER_HI) &&
           (qty_lo == REG_SWVER_QTY) && (qty_hi == 0x00U))
  {
    uint8_t resp[40];
    uint16_t crc;
    uint8_t i;

    resp[0] = PROTOCOL_ADDR;
    resp[1] = 0x13U;
    resp[2] = 0x14U;  /* 20 字节 */
    resp[3] = reg_lo;
    resp[4] = reg_hi;

    for (i = 0U; i < 20U; i++)
    {
      resp[5U + i] = (uint8_t)g_sw_version[i];
    }

    crc = Protocol_CalcCrc(resp, 25U);
    resp[25] = (uint8_t)(crc & 0xFFU);
    resp[26] = (uint8_t)((crc >> 8) & 0xFFU);

    Protocol_Send(resp, 27U);
  }
  /* ---- OTA 状态读取: REG_OTA_STATUS=204, qty=4
   *  返回: state(2regs=uint32) + written_bytes(2regs=uint32) = 8 字节
   * ---- */
  else if ((reg_lo == (uint8_t)(REG_OTA_STATUS & 0xFFU)) &&
           (reg_hi == (uint8_t)((REG_OTA_STATUS >> 8) & 0xFFU)) &&
           (qty_lo == 0x04U) && (qty_hi == 0x00U))
  {
    uint8_t resp[16];
    uint16_t crc;
    uint32_t state   = (uint32_t)OTA_GetState();
    uint32_t written = OTA_GetWrittenBytes();

    resp[0]  = PROTOCOL_ADDR;
    resp[1]  = 0x13U;
    resp[2]  = 0x08U;   /* 8 字节 */
    resp[3]  = reg_lo;
    resp[4]  = reg_hi;
    resp[5]  = (uint8_t)(state & 0xFFU);
    resp[6]  = (uint8_t)((state >> 8) & 0xFFU);
    resp[7]  = (uint8_t)((state >> 16) & 0xFFU);
    resp[8]  = (uint8_t)((state >> 24) & 0xFFU);
    resp[9]  = (uint8_t)(written & 0xFFU);
    resp[10] = (uint8_t)((written >> 8) & 0xFFU);
    resp[11] = (uint8_t)((written >> 16) & 0xFFU);
    resp[12] = (uint8_t)((written >> 24) & 0xFFU);

    crc = Protocol_CalcCrc(resp, 13U);
    resp[13] = (uint8_t)(crc & 0xFFU);
    resp[14] = (uint8_t)((crc >> 8) & 0xFFU);

    Protocol_Send(resp, 15U);
  }
}

static void Protocol_HandleWriteSingle(const uint8_t *frame, uint16_t len)
{
  if (len != 8U)
  {
    return;
  }

  uint8_t addr   = frame[0];
  uint8_t func   = frame[1];
  uint8_t reg_lo = frame[2];
  uint8_t reg_hi = frame[3];
  uint8_t val_lo = frame[4];
  uint8_t val_hi = frame[5];

  (void)addr;
  (void)func;

  /* 仅处理重启寄存器：地址 0x0078，写入 0x0001 */
  if ((reg_lo == REG_REBOOT_LO) &&
      (reg_hi == REG_REBOOT_HI) &&
      (val_lo == 0x01U) &&
      (val_hi == 0x00U))
  {
    uint8_t resp[8];
    uint16_t crc;

    resp[0] = PROTOCOL_ADDR;
    resp[1] = 0x16U;  /* 写单寄存器返回 */
    resp[2] = reg_lo;
    resp[3] = reg_hi;
    resp[4] = val_lo;
    resp[5] = val_hi;

    crc = Protocol_CalcCrc(resp, 6U);
    resp[6] = (uint8_t)(crc & 0xFFU);
    resp[7] = (uint8_t)((crc >> 8) & 0xFFU);

    Protocol_Send(resp, 8U);

    /* 确保应答发出后再重启 */
    HAL_Delay(10U);
    NVIC_SystemReset();
  }
}

void Protocol_OnUart1Bytes(void)
{
  uint16_t n;
  uint8_t tmp[32];

  /* 从环形缓冲取出尽可能多的数据 */
  for (;;)
  {
    n = USART1_Rx_GetCount();
    if (n == 0U)
    {
      break;
    }
    if (n > sizeof(tmp))
    {
      n = sizeof(tmp);
    }
    n = USART1_Rx_Read(tmp, n);
    if ((s_rx_len + n) > sizeof(s_rx_buf))
    {
      s_rx_len = 0U;
    }
    memcpy(&s_rx_buf[s_rx_len], tmp, n);
    s_rx_len = (uint16_t)(s_rx_len + n);
  }

  /* 尝试解析帧：首字节为从机地址，非 0xEF 则整帧转发，0xEF 则本地处理 */
  for (;;)
  {
    uint16_t frame_len = Protocol_RtuFrameLen(s_rx_buf, s_rx_len);
    if (frame_len == PROTOCOL_RTU_FL_NEED_MORE)
    {
      return;
    }
    if (frame_len == 0U)
    {
      if (s_rx_len < 1U)
      {
        return;
      }
      memmove(s_rx_buf, &s_rx_buf[1], s_rx_len - 1U);
      s_rx_len--;
      continue;
    }
    if (frame_len > sizeof(s_rx_buf))
    {
      memmove(s_rx_buf, &s_rx_buf[1], s_rx_len - 1U);
      s_rx_len--;
      continue;
    }

    uint8_t addr = s_rx_buf[0];
    uint8_t func = s_rx_buf[1];

    if (s_rx_len < frame_len)
    {
      return;
    }

    /* 先校验 CRC：只有 CRC 正确的帧才处理 / 转发 */
    uint16_t crc_calc = Protocol_CalcCrc(s_rx_buf, (uint16_t)(frame_len - 2U));
    uint16_t crc_recv = (uint16_t)s_rx_buf[frame_len - 2U] |
                        ((uint16_t)s_rx_buf[frame_len - 1U] << 8);
    if (crc_calc != crc_recv)
    {
      memmove(s_rx_buf, &s_rx_buf[1], s_rx_len - 1U);
      s_rx_len--;
      continue;
    }

    /* 非本机地址：整帧转发到 LoRa + CAN，不做本地解析 */
    if (addr != (uint8_t)PROTOCOL_ADDR)
    {
      Protocol_ForwardFromApp(s_rx_buf, frame_len);
      if (s_rx_len > frame_len)
      {
        memmove(s_rx_buf, &s_rx_buf[frame_len], s_rx_len - frame_len);
        s_rx_len = (uint16_t)(s_rx_len - frame_len);
      }
      else
      {
        s_rx_len = 0U;
        return;
      }
      continue;
    }

    /* 本机地址 0xEF：CRC 已经通过，做本地功能处理 */

    if (func == 0x10U)
    {
      Protocol_HandleWriteMultiple(s_rx_buf, frame_len);
    }
    else if (func == 0x06U)
    {
      Protocol_HandleWriteSingle(s_rx_buf, frame_len);
    }
    else if (func == 0x03U)
    {
      Protocol_HandleReadMultiple(s_rx_buf, frame_len);
    }

    if (s_rx_len > frame_len)
    {
      memmove(s_rx_buf, &s_rx_buf[frame_len], s_rx_len - frame_len);
      s_rx_len = (uint16_t)(s_rx_len - frame_len);
    }
    else
    {
      s_rx_len = 0U;
      return;
    }
  }
}

void Protocol_OnUart2Bytes(void)
{
  uint16_t n;
  uint8_t tmp[64];

  for (;;)
  {
    n = USART2_Rx_GetCount();
    if (n == 0U)
    {
      break;
    }
    if (n > sizeof(tmp))
    {
      n = sizeof(tmp);
    }
    n = USART2_Rx_Read(tmp, n);
    if ((s_lora_rx_len + n) > sizeof(s_lora_rx_buf))
    {
      s_lora_rx_len = 0U;
    }
    memcpy(&s_lora_rx_buf[s_lora_rx_len], tmp, n);
    s_lora_rx_len = (uint16_t)(s_lora_rx_len + n);
    Protocol_LoraParseStream();
  }
}

