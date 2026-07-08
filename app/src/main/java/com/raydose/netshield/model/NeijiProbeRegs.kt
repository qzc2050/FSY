package com.raydose.netshield.model

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

    /** uint32 阈值 / 使能 / control2 占 2 个 holding reg（4 字节） */
    const val U32_REG_COUNT = 2
}
