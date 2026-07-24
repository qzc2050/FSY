package com.raydose.raylink.net

import com.raydose.raylink.model.NeijiProbeRegs

/** 0x13 读应答是否匹配指定的探头配置寄存器读请求 */
fun ParsedFsyFrame.matchesManageConfigRead(expectedReg: Int, deviceAddr: Int): Boolean {
    if (!crcOk || func != 0x13 || addr != deviceAddr) return false
    return when (expectedReg) {
        NeijiProbeRegs.DOSE_HI_TH -> doseHiX100 != null
        NeijiProbeRegs.DOSE_LO_TH -> doseLoX100 != null
        NeijiProbeRegs.ALARM_ENABLE -> alarmEnableValue != null
        NeijiProbeRegs.ALARM_VOLUME -> controlBit1Volume != null
        NeijiProbeRegs.CONTROL_BIT2 -> controlBit2Value != null
        NeijiProbeRegs.SOFTWARE_VERSION -> !deviceVersion.isNullOrBlank()
        NeijiProbeRegs.PRODUCT_MODEL -> !deviceModel.isNullOrBlank()
        else -> false
    }
}
