package com.raydose.netshield.ui.theme

import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

/**
 * 工控平板横屏规格（与 adb / 产品型号一致）。
 *
 * - NS-T100：1280×800（16:10）
 * - NS-T130：1920×1080（16:9）
 *
 * 均以 **横屏** 为默认方向；Compose 中 [LocalConfiguration.screenWidthDp] 为较短边换算后的逻辑宽。
 */
enum class TabletFormFactor {
    /** NS-T100，10 寸，1280×800 */
    Compact,
    /** NS-T130，13 寸，1920×1080 */
    Expanded,
}

object ScreenSpec {
    const val T100_WIDTH_PX = 1280
    const val T100_HEIGHT_PX = 800
    const val T130_WIDTH_PX = 1920
    const val T130_HEIGHT_PX = 1080

    /** 低于此逻辑宽度视为 T100 档布局 */
    val compactMaxWidthDp = 960.dp

    fun formFactor(screenWidthDp: Int): TabletFormFactor =
        if (screenWidthDp < compactMaxWidthDp.value.toInt()) {
            TabletFormFactor.Compact
        } else {
            TabletFormFactor.Expanded
        }

    /** 主页探头卡片：左边距占屏宽比例 */
    const val HOME_CARD_START_FRACTION = 0.068f

    /** 主页探头卡片：宽度占屏宽比例 */
    const val HOME_CARD_WIDTH_FRACTION = 0.824f

    /** 主页探头卡片：高度占屏高比例（1920 原型约 49.8%） */
    const val HOME_CARD_HEIGHT_FRACTION = 0.498f

    /** 右侧预留给翻页箭头 / 侧滑提示的宽度比例 */
    const val HOME_SIDE_HINT_FRACTION = 0.045f

    /** 主页左右内容边距（NetShield / 日期 / 状态图标对齐） */
    val homeHorizontalPadding = 42.dp

    /** 卡片相对上方区域再下移 */
    val homeCardTopGap = 36.dp

    /** 底部门状态 / 留言板下方留白（约为屏高 6%，在当前位置与底边中间） */
    const val HOME_FOOTER_BOTTOM_FRACTION = 0.06f

    /** 主页时间字号 */
    const val HOME_TIME_SP = 48

    /** 主页日期字号（比时间小一号） */
    const val HOME_DATE_SP = 40

    /** 主页右侧本机环境参数字号（比时间小一号） */
    const val HOME_HOST_ENV_SP = 40

    /** 主页右侧本机环境参数区域高度（VerticalPager，每次 2 行） */
    val homeHostEnvPanelHeight = 128.dp

    /** 主页右侧本机环境参数每页显示条数 */
    const val HOME_HOST_ENV_ITEMS_PER_PAGE = 2

    /** 主页卡片辐射量字号 */
    const val HOME_DOSE_SP = 228

    /** 主页卡片辐射量单位字号 */
    const val HOME_DOSE_UNIT_SP = 32

    /** 主页卡片底部环境参数字号 */
    const val HOME_CARD_ENV_SP = 32

    /** 辐射量相对卡片中心下移 */
    val homeDoseOffsetY = 20.dp

    /** 辐射量数值与单位间距 */
    val homeDoseUnitGap = 28.dp

    /** 主页时钟字号等可按档位缩放 */
    fun timeTextSp(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> 32
        TabletFormFactor.Expanded -> 36
    }

    /** 设置页「同时显示从机卡片数」默认值 */
    fun defaultSimultaneousProbeCards(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> 1
        TabletFormFactor.Expanded -> 1
    }

    /** 主页侧栏可用区宽度 = 屏宽 − 卡片左距 − 卡片宽度（卡片右缘到屏右缘） */
    fun homeSideDrawerWidthFraction(): Float =
        1f - HOME_CARD_START_FRACTION - HOME_CARD_WIDTH_FRACTION

    /** 侧栏高度 = 卡片高度 × 此比例（0.8 → 比卡片矮 20%，上下各留 10%） */
    const val SIDE_DRAWER_HEIGHT_RATIO_OF_CARD = 0.8f

    /** 侧栏顶部相对卡片顶的下移比例（与高度比例配合，使侧栏在卡片内垂直居中） */
    const val SIDE_DRAWER_TOP_INSET_RATIO_OF_CARD = 0.1f

    /** 侧栏实际宽度 = 可用间隙宽度 × 此比例（略缩，避免贴住卡片） */
    const val SIDE_DRAWER_WIDTH_RATIO_OF_GAP = 0.82f

    /** 侧边栏图标尺寸 */
    val sideDrawerIconSize = 44.dp

    /** 侧边栏文案字号 */
    const val SIDE_DRAWER_LABEL_SP = 15

    /** 顶部下拉面板高度占屏高比例（image10 约 45%，用于跟手滑动行程） */
    /** 设置页首行：各探头名称 + 剂量摘要，占整屏高度比例 */
    const val SETTINGS_PROBE_SUMMARY_HEIGHT_FRACTION = 0.15f

    const val STATUS_BAR_PANEL_HEIGHT_FRACTION = 0.45f

    /** 下拉展开时下半屏主页缩略区域占屏高比例 */
    fun statusBarThumbnailHeightFraction(): Float = 1f - STATUS_BAR_PANEL_HEIGHT_FRACTION

    /** 顶部下拉手势触发区高度 */
    val statusBarGestureZoneHeight = 96.dp

    /** 下拉面板：WiFi/蓝牙标签字号 */
    fun statusBarInfoLabelSp(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> 18
        TabletFormFactor.Expanded -> 22
    }

    /** 下拉面板：WiFi/蓝牙内容字号 */
    fun statusBarInfoValueSp(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> 22
        TabletFormFactor.Expanded -> 26
    }

    /** 下拉面板：已连接设备名称 */
    fun statusBarDeviceNameSp(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> 20
        TabletFormFactor.Expanded -> 24
    }

    /** 下拉面板：已连接设备 IP */
    fun statusBarDeviceIpSp(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> 16
        TabletFormFactor.Expanded -> 20
    }

    /** 下拉面板：日志时间 */
    fun statusBarLogTimeSp(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> 16
        TabletFormFactor.Expanded -> 20
    }

    /** 下拉面板：日志正文 */
    fun statusBarLogMessageSp(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> 20
        TabletFormFactor.Expanded -> 24
    }

    /** 下拉面板：空状态提示 */
    fun statusBarPlaceholderSp(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> 18
        TabletFormFactor.Expanded -> 22
    }

    /** 下拉面板：日志行图标 */
    fun statusBarLogIconSize(formFactor: TabletFormFactor): Dp = when (formFactor) {
        TabletFormFactor.Compact -> 24.dp
        TabletFormFactor.Expanded -> 28.dp
    }

    /** 下拉面板：密码显隐图标（emoji） */
    fun statusBarPasswordToggleSp(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> 22
        TabletFormFactor.Expanded -> 26
    }
}

@Composable
fun rememberTabletFormFactor(): TabletFormFactor {
    val widthDp = LocalConfiguration.current.screenWidthDp
    return ScreenSpec.formFactor(widthDp)
}

@Composable
fun rememberScreenWidthDp(): Int = LocalConfiguration.current.screenWidthDp
