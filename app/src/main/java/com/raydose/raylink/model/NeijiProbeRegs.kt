package com.raydose.raylink.model

/**
 * Neiji 内机 Modbus 寄存器（与固件 fsy_regmap.h / device_config 一致）。
 * 协议帧内地址为十六进制表号：50→0x32，52→0x34，82→0x52，122→0x7A。
 */
object NeijiProbeRegs {
    const val DOSE_HI_TH = 0x0032
    const val DOSE_LO_TH = 0x0034
    const val ALARM_ENABLE = 0x0052
    const val ALARM_VOLUME = 0x007A
    /** reg123：bit13 光报警，bit14 背光，bit15 外置报警在线（只读） */
    const val CONTROL_BIT2 = 0x007B
    /** reg94：RTC 时间 8B `[年%100,月,日,时,分,秒,0,0]`，写满 4 reg 生效 */
    const val TIME = 0x005E
    const val TIME_REG_COUNT = 4

    /** 实时五分钟 0x23 start */
    const val FIVE_MIN_REALTIME = 0x001E
    /** 历史五分钟回传 0x23 start（reg36） */
    const val FIVE_MIN_HISTORY = 0x0024
    /** 历史查询起止：reg108，一次写 8 reg / 16B */
    const val HIST_QUERY_START = 0x006C
    const val HIST_QUERY_REG_COUNT = 8

    /** uint32 阈值 / 使能 / control2 占 2 个 holding reg（4 字节） */
    const val U32_REG_COUNT = 2

    /** reg98：软件版本 ASCII，10 reg / 20B（与 fsy_regmap.h / ZJB 0x62 同址） */
    const val SOFTWARE_VERSION = 0x0062
    const val SOFTWARE_VERSION_REGS = 10
}
