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

    /** 低于此逻辑宽度视为 T100 档布局（仅作兜底；优先用 [formFactor] 长短边判定） */
    val compactMaxWidthDp = 960.dp

    /** 短边 ≤820dp 且长边 ≤1320dp → NS-T100（1280×800） */
    val compactShortSideMaxDp = 820.dp
    val compactLongSideMaxDp = 1320.dp

    fun formFactor(screenWidthDp: Int, screenHeightDp: Int = screenWidthDp): TabletFormFactor {
        val shortSide = minOf(screenWidthDp, screenHeightDp)
        val longSide = maxOf(screenWidthDp, screenHeightDp)
        return if (shortSide <= compactShortSideMaxDp.value.toInt() &&
            longSide <= compactLongSideMaxDp.value.toInt()
        ) {
            TabletFormFactor.Compact
        } else if (screenWidthDp < compactMaxWidthDp.value.toInt()) {
            TabletFormFactor.Compact
        } else {
            TabletFormFactor.Expanded
        }
    }

    /** 主页探头卡片：左边距占屏宽比例 */
    const val HOME_CARD_START_FRACTION = 0.068f

    /** 主页探头卡片：宽度占屏宽比例 */
    const val HOME_CARD_WIDTH_FRACTION = 0.824f

    /** 主页探头卡片：高度占屏高比例（1920 原型约 49.8%） */
    const val HOME_CARD_HEIGHT_FRACTION = 0.498f

    /** 待机页：下区（探头+留言）占屏高比例（门状态已并入顶区） */
    const val STANDBY_BOTTOM_SECTION_FRACTION = 0.75f

    /** 待机页：顶区（顶栏+日期时间+门状态+本机环境）占屏高比例 */
    const val STANDBY_HEADER_SECTION_FRACTION = 0.25f

    /** 10 寸：顶区增高，容纳居中日期/时间 + 下方环境横排 */
    const val STANDBY_HEADER_SECTION_FRACTION_COMPACT = 0.34f

    fun standbyHeaderSectionFraction(formFactor: TabletFormFactor): Float = when (formFactor) {
        TabletFormFactor.Compact -> STANDBY_HEADER_SECTION_FRACTION_COMPACT
        TabletFormFactor.Expanded -> STANDBY_HEADER_SECTION_FRACTION
    }

    fun standbyBottomSectionFraction(formFactor: TabletFormFactor): Float =
        1f - standbyHeaderSectionFraction(formFactor)

    /** 待机页日期字号 */
    fun standbyDateSp(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> 28
        TabletFormFactor.Expanded -> HOME_DATE_SP
    }

    /** 待机页时间字号 */
    fun standbyTimeSp(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> 36
        TabletFormFactor.Expanded -> HOME_TIME_SP
    }

    /** 顶栏系统名字号（NetShield） */
    const val HOME_TOP_BAR_TITLE_SP = 26

    /** 待机页横排环境参数字号（与顶栏 NetShield 一致） */
    const val STANDBY_HOST_ENV_ROW_SP = HOME_TOP_BAR_TITLE_SP

    /** 待机页环境横排项间距 */
    fun standbyHostEnvRowGap(formFactor: TabletFormFactor): Dp = when (formFactor) {
        TabletFormFactor.Compact -> 14.dp
        TabletFormFactor.Expanded -> 28.dp
    }

    /** 待机页探头列宽占下区 Row 比例（留言 1/3） */
    const val STANDBY_PROBE_COLUMN_WEIGHT = 2f

    /** 待机页留言列宽占下区 Row 比例 */
    const val STANDBY_MESSAGE_COLUMN_WEIGHT = 1f

    /** 待机探头名行距卡片顶（与 [SlaveProbeCard] 顶区 16.dp + 名称行 4.dp 一致） */
    val standbyProbeNameRowTopInset = 20.dp

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

    /** 监测组件「滚动」模式：多探头时每页停留时长（毫秒） */
    const val PROBE_CARD_AUTO_SCROLL_INTERVAL_MS = 5_000L

    /** 待机页：多探头自动翻页停留时长（毫秒） */
    const val STANDBY_PROBE_AUTO_SCROLL_INTERVAL_MS = 2_000L

    /** 主页留言栏：按每行填充比例分档的停留时长（毫秒）；多行总时长 = 各行之和 */
    const val MESSAGE_TICKER_DWELL_MS_SHORT = 3_000L   // 该行 < 单行 1/4
    const val MESSAGE_TICKER_DWELL_MS_MEDIUM = 5_000L  // 该行 < 单行 1/2
    const val MESSAGE_TICKER_DWELL_MS_LONG = 7_000L    // 该行 < 单行 3/4
    const val MESSAGE_TICKER_DWELL_MS_FULL = 10_000L   // 该行 ≥ 单行 3/4（满行）

    /** 主页留言栏：多条轮播切换动画时长（毫秒，LED 式上滚） */
    const val MESSAGE_TICKER_SLIDE_MS = 500

    /** 主页留言栏：回收态可见行数（长留言逐行切换，每次仅 1 行） */
    const val MESSAGE_TICKER_VISIBLE_LINES = 1

    /** 主页留言栏：正文行高（sp） */
    const val MESSAGE_TICKER_LINE_HEIGHT_SP = 22

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

    /** 10 寸等矮屏：侧栏略高于卡片（106%），保证四项文字可读 */
    const val SIDE_DRAWER_HEIGHT_RATIO_COMPACT_SCREEN = 1.06f

    /** 低于此屏高（dp）视为矮屏侧栏布局 */
    val compactScreenMaxHeightDp = 820.dp

    fun sideDrawerHeightRatioOfCard(screenHeight: Dp): Float =
        if (screenHeight <= compactScreenMaxHeightDp) {
            SIDE_DRAWER_HEIGHT_RATIO_COMPACT_SCREEN
        } else {
            SIDE_DRAWER_HEIGHT_RATIO_OF_CARD
        }

    /** 106% 高度时相对卡片垂直居中： (1 - 1.06) / 2 */
    fun sideDrawerTopInsetRatioOfCard(screenHeight: Dp): Float =
        if (screenHeight <= compactScreenMaxHeightDp) -0.03f else SIDE_DRAWER_TOP_INSET_RATIO_OF_CARD

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

    /** 下拉缩略区三行：顶栏 / 主内容 / 底留白 */
    const val STATUS_BAR_THUMBNAIL_ROW1_FRACTION = 0.10f
    const val STATUS_BAR_THUMBNAIL_ROW2_FRACTION = 0.85f
    const val STATUS_BAR_THUMBNAIL_ROW3_FRACTION = 0.05f

    /** 缩略区各行内容整体下移占该行高度比例 */
    const val STATUS_BAR_THUMBNAIL_ROW_TOP_INSET_FRACTION = 0.05f

    /** 第二行内容（日期+探头）整体下移（同 [STATUS_BAR_THUMBNAIL_ROW_TOP_INSET_FRACTION]） */
    const val STATUS_BAR_THUMBNAIL_ROW2_TOP_INSET_FRACTION = STATUS_BAR_THUMBNAIL_ROW_TOP_INSET_FRACTION

    /** 下拉缩略顶栏：门 / 留言 / 图标 横向占比 */
    const val STATUS_BAR_THUMBNAIL_DOOR_FRACTION = 0.25f
    const val STATUS_BAR_THUMBNAIL_MESSAGE_FRACTION = 0.50f
    const val STATUS_BAR_THUMBNAIL_ICONS_FRACTION = 0.25f

    /** 下拉缩略主内容：环境信息 / 探头卡片 */
    const val STATUS_BAR_THUMBNAIL_ENV_FRACTION = 1f / 3f
    const val STATUS_BAR_THUMBNAIL_PROBE_FRACTION = 2f / 3f

    /** 下拉缩略探头区高度（占第二行可用高度） */
    const val STATUS_BAR_THUMBNAIL_PROBE_HEIGHT_FRACTION = 1f

    /** 下拉缩略区字号（按缩略视口直接排版，不再整页缩放） */
    const val STATUS_BAR_THUMBNAIL_DATE_SP = 24
    const val STATUS_BAR_THUMBNAIL_TIME_SP = 32
    const val STATUS_BAR_THUMBNAIL_ENV_SP = 28
    const val STATUS_BAR_THUMBNAIL_ENV_SP_COMPACT = 24
    const val STATUS_BAR_THUMBNAIL_MESSAGE_SP = 14

    /** 下拉缩略左侧本机环境字号：10 寸略小 */
    fun statusBarThumbnailEnvSp(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> STATUS_BAR_THUMBNAIL_ENV_SP_COMPACT
        TabletFormFactor.Expanded -> STATUS_BAR_THUMBNAIL_ENV_SP
    }

    /** 与探头卡片 Full 布局底部环境栏 [envWeight] 一致，左列气压行与之对齐 */
    const val STATUS_BAR_THUMBNAIL_PROBE_ENV_WEIGHT = 0.26f

    /** 下拉缩略环境列：环境参数行间距 */
    val statusBarThumbnailEnvLineSpacing = 10.dp

    /** 下拉缩略主内容：左环境信息 / 右探头卡片 */
    const val STATUS_BAR_THUMBNAIL_LEFT_FRACTION = STATUS_BAR_THUMBNAIL_ENV_FRACTION

    /** 顶部下拉手势触发区高度 */
    val statusBarGestureZoneHeight = 96.dp

    /** 顶栏下拉箭头点击热区（不可见，小于 [statusBarGestureZoneHeight] 的全宽下拉带） */
    val homePullDownHintTapWidth = 88.dp
    val homePullDownHintTapHeight = 56.dp

    /** 右侧侧滑箭头点击热区（不可见，小于卡片区侧滑带） */
    val homeSideSwipeHintTapWidth = 56.dp
    val homeSideSwipeHintTapHeight = 96.dp

    /** 原型：扁宽线条 chevron 可视尺寸（无按钮底） */
    val homePullDownHintVisualWidth = 64.dp
    val homePullDownHintVisualHeight = 16.dp
    val homeSideSwipeHintVisualWidth = 16.dp
    val homeSideSwipeHintVisualHeight = 64.dp

    /**
     * 主页引导箭头距屏幕对应边缘的内边距。
     * 下拉 chevron 在 [statusBarGestureZoneHeight] 内垂直居中时的顶距 = 侧滑 chevron 的右距。
     */
    val homeHintEdgeInset: Dp
        get() = (statusBarGestureZoneHeight - homePullDownHintTapHeight) / 2 +
            (homePullDownHintTapHeight - homePullDownHintVisualHeight) / 2

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

    /** 添加探头弹窗：10 寸屏宽 1/2，13 寸屏宽 1/3 */
    const val ADD_PROBE_DIALOG_WIDTH_FRACTION_COMPACT = 0.5f
    const val ADD_PROBE_DIALOG_WIDTH_FRACTION_EXPANDED = 1f / 3f

    fun addProbeDialogWidthFraction(formFactor: TabletFormFactor): Float = when (formFactor) {
        TabletFormFactor.Compact -> ADD_PROBE_DIALOG_WIDTH_FRACTION_COMPACT
        TabletFormFactor.Expanded -> ADD_PROBE_DIALOG_WIDTH_FRACTION_EXPANDED
    }

    /** 待机页留言字号 */
    fun standbyMessageSp(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> 24
        TabletFormFactor.Expanded -> 28
    }

    /** 待机页「留言」标题字号（略小于正文） */
    fun standbyMessageTitleSp(formFactor: TabletFormFactor): Int = when (formFactor) {
        TabletFormFactor.Compact -> 18
        TabletFormFactor.Expanded -> 20
    }
}

@Composable
fun rememberTabletFormFactor(): TabletFormFactor {
    val cfg = androidx.compose.ui.platform.LocalConfiguration.current
    return ScreenSpec.formFactor(cfg.screenWidthDp, cfg.screenHeightDp)
}

@Composable
fun rememberScreenWidthDp(): Int = LocalConfiguration.current.screenWidthDp
