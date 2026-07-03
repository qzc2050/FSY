package com.raydose.netshield.model

/** App 对探头的下发通道：串口（经 zjb/CAN）或网口 TCP。 */
enum class ProbeCommandLink {
  SERIAL,
  NETWORK,
}
