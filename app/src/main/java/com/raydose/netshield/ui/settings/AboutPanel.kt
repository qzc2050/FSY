package com.raydose.netshield.ui.settings

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier

data class AboutDeviceInfo(
    val productName: String,
    val hostModel: String,
    val serialNumber: String,
    val softwareVersion: String,
    val hardwareVersion: String,
)

@Composable
fun AboutPanel(
    info: AboutDeviceInfo,
    modifier: Modifier = Modifier,
) {
    SettingsScrollContent(modifier = modifier.fillMaxSize()) {
        SettingsCard {
            AboutInfoRow("产品名称", info.productName)
            AboutInfoRow("主机型号", info.hostModel)
            AboutInfoRow("主机序列号", info.serialNumber)
            AboutInfoRow("软件版本", info.softwareVersion)
            AboutInfoRow("硬件版本", info.hardwareVersion)
        }
    }
}

@Composable
private fun AboutInfoRow(label: String, value: String) {
    SettingsTextFieldRow(label = label, value = value, readOnly = true, onValueChange = {})
}
