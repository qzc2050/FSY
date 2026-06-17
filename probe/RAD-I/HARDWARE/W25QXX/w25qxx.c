#include "w25qxx.h"
#include "quadspi.h"
#include "main.h"


/**
 * @brief  初始化 W25Qxx QSPI Flash
 * @retval QSPI 存储器状态
 */
uint8_t W25Qx_QSPI_Init(void)
{
	QSPI_CommandTypeDef s_command;
	uint8_t value = W25QxJV_FSR_QE;

	/* 复位 QSPI 存储器 */
	if (W25Qx_QSPI_ResetMemory() != QSPI_OK)
		return QSPI_NOT_SUPPORTED;

	/* 使能写入操作 */
	if (W25Qx_QSPI_WriteEnable() != QSPI_OK)
		return QSPI_ERROR;
    
	/* 配置状态寄存器 2，使能 QE（四线使能），启用 IO2/IO3 作为数据线 */
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction = WRITE_STATUS_REG2_CMD;
	s_command.AddressMode = QSPI_ADDRESS_NONE;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode = QSPI_DATA_1_LINE;
	s_command.DummyCycles = 0;
	s_command.NbData = 1;
	s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    
	/* 发送写状态寄存器命令 */
	if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE)!= HAL_OK)
		return QSPI_ERROR;
    
	/* 发送待写入的状态寄存器数据 */
	if (HAL_QSPI_Transmit(&hqspi, &value, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;
    
	/* 自动轮询，等待器件准备就绪 */
	if (W25Qx_QSPI_AutoPollingMemReady(W25QxJV_SUBSECTOR_ERASE_MAX_TIME) != QSPI_OK)
		return QSPI_ERROR;

	/* 为 W25Q256 配置为 4 字节地址模式，其它容量直接使用 3 字节地址 */
	if (sFLASH_ID == 0XEF4019)
		if (W25Qx_QSPI_Addr_Mode_Init() != QSPI_OK)
            return QSPI_ERROR;

    printf("id: %x\r\n", W25Qx_QSPI_FLASH_ReadID());
        
	return QSPI_OK;
}

/**
 * @brief  地址模式初始化，将 3 字节地址模式切换为 4 字节地址模式（如需要）
 * @retval QSPI 存储器状态
 */
static uint8_t W25Qx_QSPI_Addr_Mode_Init(void)
{
	uint8_t reg;
	QSPI_CommandTypeDef s_command;
    
	/* 配置读取状态寄存器 3 的命令 */
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction = READ_STATUS_REG3_CMD;
	s_command.AddressMode = QSPI_ADDRESS_NONE;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode = QSPI_DATA_1_LINE;
	s_command.DummyCycles = 0;
	s_command.NbData = 1;
	s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

	/* 发送读取命令 */
	if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;

	/* 接收寄存器数据 */
	if (HAL_QSPI_Receive(&hqspi, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;

	/* 判断当前地址模式 */
	if ((reg & W25Q256FV_FSR_4ByteAddrMode) == 1)    // 已经是 4 字节模式
		return QSPI_OK;
	else    // 当前是 3 字节模式
	{
		/* 发送进入 4 字节地址模式命令 */
		s_command.Instruction = Enter_4Byte_Addr_Mode_CMD;
		s_command.DataMode = QSPI_DATA_NONE;
 
		/* 发送命令 */
		if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
			return QSPI_ERROR;
 
		/* 自动轮询等待操作完成 */
		if (W25Qx_QSPI_AutoPollingMemReady(W25QxJV_SUBSECTOR_ERASE_MAX_TIME) != QSPI_OK)
			return QSPI_ERROR;
		return QSPI_OK;
	}
}
 
/**
 * @brief  对 QSPI Flash 进行快速读取（四线高速读）
 * @param  pData: 数据接收缓冲区指针
 * @param  ReadAddr: 读取起始地址
 * @param  Size: 读取数据长度（字节数）
 * @retval QSPI 存储器状态
 */
uint8_t W25Qx_QSPI_FastRead(uint8_t *pData, uint32_t ReadAddr, uint32_t Size)
{
	QSPI_CommandTypeDef s_command;
 
	if(Size == 0)
        return QSPI_OK;
 
	/* 配置高速读命令参数 */
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction = QUAD_INOUT_FAST_READ_CMD;
	s_command.AddressMode = QSPI_ADDRESS_4_LINES;
	s_command.AddressSize = QSPI_ADDRESS_24_BITS;
	s_command.Address = ReadAddr;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode = QSPI_DATA_4_LINES;
	s_command.DummyCycles = 6;
	s_command.NbData = Size;
	s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
 
	/* 发送读命令 */
	if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;
 
	/* 接收数据 */
	if (HAL_QSPI_Receive(&hqspi, pData, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;

	return QSPI_OK;
}
 
/**
 * @brief  对 QSPI Flash 进行普通读取（单线慢速读）
 * @note   该命令一般仅在 QSPI 时钟 <= 50MHz 下使用，否则可能读数不可靠
 * @param  pData: 数据接收缓冲区指针
 * @param  ReadAddr: 读取起始地址
 * @param  Size: 读取数据长度（字节数）
 * @retval QSPI 存储器状态
 */
uint8_t W25Qx_QSPI_Read(uint8_t *pData, uint32_t ReadAddr, uint32_t Size)
{
	QSPI_CommandTypeDef s_command;
	/* 配置普通读命令参数 */
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction = READ_CMD;
	s_command.AddressMode = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize = QSPI_ADDRESS_24_BITS;
	s_command.Address = ReadAddr;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode = QSPI_DATA_1_LINE;
	s_command.DummyCycles = 0;
	s_command.NbData = Size;
	s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
 
	/* 发送读命令 */
	if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE)!= HAL_OK)
		return QSPI_ERROR;
 
	/* 接收数据 */
	if (HAL_QSPI_Receive(&hqspi, pData, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;

	return QSPI_OK;
}
 
/**
 * @brief  对 QSPI Flash 写入任意长度数据（自动按页拆分写入）
 * @param  pData: 待写入数据缓冲区指针
 * @param  WriteAddr: 写入起始地址
 * @param  Size: 写入数据长度（字节数）
 * @retval QSPI 存储器状态
 */
uint8_t W25Qx_QSPI_Write(uint8_t *pData, uint32_t WriteAddr, uint32_t Size)
{
	QSPI_CommandTypeDef s_command;
	uint32_t end_addr, current_size, current_addr;
    
	/* 计算起始地址到当前页末尾的剩余空间 */
	current_addr = 0;
 
	while (current_addr <= WriteAddr)
		current_addr += W25QxJV_PAGE_SIZE;
	current_size = current_addr - WriteAddr;
 
	/* 如果总写入长度小于当前页剩余空间，则本次只写入 Size 字节 */
	if (current_size > Size)
		current_size = Size;
 
	/* 初始化当前写地址与结束地址 */
	current_addr = WriteAddr;
	end_addr = WriteAddr + Size;
 
	/* 填充写页命令结构体的公共部分 */
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction = QUAD_INPUT_PAGE_PROG_CMD;
	s_command.AddressMode = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize = QSPI_ADDRESS_24_BITS;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode = QSPI_DATA_4_LINES;
	s_command.DummyCycles = 0;
	s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
 
	/* 按页执行写入 */
	do
	{
		s_command.Address = current_addr;
		s_command.NbData = current_size;

		/* 使能写入 */
		if (W25Qx_QSPI_WriteEnable() != QSPI_OK)
			return QSPI_ERROR;
 
		/* 发送写命令 */
		if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
			return QSPI_ERROR;
 
		/* 发送数据 */
		if (HAL_QSPI_Transmit(&hqspi, pData, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
			return QSPI_ERROR;
 
		/* 自动轮询等待本页写入完成 */
		if (W25Qx_QSPI_AutoPollingMemReady(HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != QSPI_OK)
			return QSPI_ERROR;
 
		/* 更新到下一页的地址和本次写入大小 */
		current_addr += current_size;
		pData += current_size;
		current_size = ((current_addr + W25QxJV_PAGE_SIZE) > end_addr) ?
						(end_addr - current_addr) : W25QxJV_PAGE_SIZE;
	} while (current_addr < end_addr);
	return QSPI_OK;
}
 
/**
 * @brief  擦除 QSPI Flash 指定扇区（或块）
 * @param  BlockAddress: 需要擦除的扇区/块起始地址
 * @retval QSPI 存储器状态
 */
uint8_t W25Qx_QSPI_Erase_Block(uint32_t BlockAddress)
{
	QSPI_CommandTypeDef s_command;
	/* 配置扇区擦除命令 */
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction = SECTOR_ERASE_CMD;
	s_command.AddressMode = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize = QSPI_ADDRESS_24_BITS;
	s_command.Address = BlockAddress;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode = QSPI_DATA_NONE;
	s_command.DummyCycles = 0;
	s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
 
	/* 使能写入 */
	if (W25Qx_QSPI_WriteEnable() != QSPI_OK)
		return QSPI_ERROR;
 
	/* 发送擦除命令 */
	if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;
 
	/* 自动轮询等待擦除完成 */
	if (W25Qx_QSPI_AutoPollingMemReady(W25QxJV_SUBSECTOR_ERASE_MAX_TIME) != QSPI_OK)
		return QSPI_ERROR;
	return QSPI_OK;
}
 
/**
 * @brief  整片擦除 QSPI Flash
 * @retval QSPI 存储器状态
 */
uint8_t W25Qx_QSPI_Erase_Chip(void)
{
	QSPI_CommandTypeDef s_command;
	/* 配置整片擦除命令 */
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction = CHIP_ERASE_CMD;
	s_command.AddressMode = QSPI_ADDRESS_NONE;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode = QSPI_DATA_NONE;
	s_command.DummyCycles = 0;
	s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
 
	/* 使能写入 */
	if (W25Qx_QSPI_WriteEnable() != QSPI_OK)
		return QSPI_ERROR;

	/* 发送擦除命令 */
	if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;

	/* 自动轮询等待整片擦除完成 */
	if (W25Qx_QSPI_AutoPollingMemReady(W25QxJV_BULK_ERASE_MAX_TIME) != QSPI_OK)
		return QSPI_ERROR;
	return QSPI_OK;
}
 
/**
 * @brief  读取 QSPI Flash 当前状态
 * @retval QSPI 存储器状态（QSPI_OK/QSPI_BUSY/QSPI_ERROR）
 */
uint8_t W25Qx_QSPI_GetStatus(void)
{
	QSPI_CommandTypeDef s_command;
	uint8_t reg;
	/* 配置读取状态寄存器 1 的命令 */
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction = READ_STATUS_REG1_CMD;
	s_command.AddressMode = QSPI_ADDRESS_NONE;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode = QSPI_DATA_1_LINE;
	s_command.DummyCycles = 0;
	s_command.NbData = 1;
	s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
 
	/* 发送命令 */
	if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;

	/* 读取一个状态字节 */
	if (HAL_QSPI_Receive(&hqspi, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;

	/* 判断忙标志位 */
	if ((reg & W25QxJV_FSR_BUSY) != 0)
		return QSPI_BUSY;
	else
		return QSPI_OK;
}
 
/**
 * @brief  获取 QSPI Flash 的参数信息
 * @param  pInfo: 信息结构体指针
 * @retval QSPI 存储器状态
 */
uint8_t W25Qx_QSPI_GetInfo(QSPI_Info *pInfo)
{
	/* 填充存储器信息结构体 */
	pInfo->FlashSize = W25QxJV_FLASH_SIZE;
	pInfo->EraseSectorSize = W25QxJV_SUBSECTOR_SIZE;
	pInfo->EraseSectorsNumber = (W25QxJV_FLASH_SIZE / W25QxJV_SUBSECTOR_SIZE);
	pInfo->ProgPageSize = W25QxJV_PAGE_SIZE;
	pInfo->ProgPagesNumber = (W25QxJV_FLASH_SIZE / W25QxJV_PAGE_SIZE);
	return QSPI_OK;
}
 
/**
 * @brief  复位 QSPI Flash
 * @param  hqspi: QSPI 句柄
 * @retval QSPI 存储器状态
 */
static uint8_t W25Qx_QSPI_ResetMemory()
{
	QSPI_CommandTypeDef s_command;
	/* 配置复位使能命令（单线） */
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction = RESET_ENABLE_CMD;
	s_command.AddressMode = QSPI_ADDRESS_NONE;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode = QSPI_DATA_NONE;
	s_command.DummyCycles = 0;
	s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
 
	/* 发送复位使能命令 */
	if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;
 
	/* 发送复位命令 */
	s_command.Instruction = RESET_MEMORY_CMD;
	if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;
 
	s_command.InstructionMode = QSPI_INSTRUCTION_4_LINES;
	s_command.Instruction = RESET_ENABLE_CMD;
 
	/* 再次发送复位使能命令 */
	if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;
 
	/* 再次发送复位命令 */
	s_command.Instruction = RESET_MEMORY_CMD;
	if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;
 
	W25Qx_QSPI_Delay(1);
 
	/* 自动轮询等待器件复位完成 */
	if (W25Qx_QSPI_AutoPollingMemReady(HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != QSPI_OK)
		return QSPI_ERROR;
	return QSPI_OK;
}
 
/**
 * @brief  发送写使能命令并等待其生效
 * @param  hqspi: QSPI 句柄
 * @retval QSPI 存储器状态
 */
static uint8_t W25Qx_QSPI_WriteEnable()
{
	QSPI_CommandTypeDef s_command;
	QSPI_AutoPollingTypeDef s_config;
	/* 发送写使能命令 */
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction = WRITE_ENABLE_CMD;
	s_command.AddressMode = QSPI_ADDRESS_NONE;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode = QSPI_DATA_NONE;
	s_command.DummyCycles = 0;
	s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
	if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;

	/* 使用自动轮询等待 WEL 位置位 */
	s_config.Match = W25QxJV_FSR_WREN;
	s_config.Mask = W25QxJV_FSR_WREN;
	s_config.MatchMode = QSPI_MATCH_MODE_AND;
	s_config.StatusBytesSize = 1;
	s_config.Interval = 0x10;
	s_config.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE;
 
	s_command.Instruction = READ_STATUS_REG1_CMD;
	s_command.DataMode = QSPI_DATA_1_LINE;
	s_command.NbData = 1;
 
	if (HAL_QSPI_AutoPolling(&hqspi, &s_command, &s_config, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
		return QSPI_ERROR;

	return QSPI_OK;
}
 
/**
 * @brief  轮询等待 Flash 不再忙（SR 的 BUSY 位清零）
 * @param  hqspi: QSPI 句柄
 * @param  Timeout 超时时间
 * @retval QSPI 存储器状态
 */
static uint8_t W25Qx_QSPI_AutoPollingMemReady(uint32_t Timeout)
{
	QSPI_CommandTypeDef s_command;
	QSPI_AutoPollingTypeDef s_config;
	/* 配置自动轮询命令，等待 BUSY 位清零 */
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction = READ_STATUS_REG1_CMD;
	s_command.AddressMode = QSPI_ADDRESS_NONE;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode = QSPI_DATA_1_LINE;
	s_command.DummyCycles = 0;
	s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
 
	s_config.Match = 0x00;
	s_config.Mask = W25QxJV_FSR_BUSY;
	s_config.MatchMode = QSPI_MATCH_MODE_AND;
	s_config.StatusBytesSize = 1;
	s_config.Interval = 0x10;
	s_config.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE;
 
	if (HAL_QSPI_AutoPolling(&hqspi, &s_command, &s_config, Timeout) != HAL_OK)
		return QSPI_ERROR;
	return QSPI_OK;
}
 
/**
 * @brief  读取 Flash JEDEC ID
 * @retval 24 位 JEDEC ID（高字节为厂商 ID）
 */
uint32_t W25Qx_QSPI_FLASH_ReadID(void)
{
	QSPI_CommandTypeDef s_command;
	uint32_t Temp = 0;
	uint8_t pData[3];
	/* 配置读取 JEDEC ID 命令 */
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction = READ_JEDEC_ID_CMD;
	s_command.AddressMode = QSPI_ADDRESS_NONE;
	s_command.DataMode = QSPI_DATA_1_LINE;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DummyCycles = 0;
	s_command.NbData = 3;
	s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
 
	if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		printf("QSPI_FLASH_ReadID ERROR!!!....\r\n");
		/* 根据需要在此处添加错误处理 */
		while (1)
		{

		}
	}
	if (HAL_QSPI_Receive(&hqspi, pData, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		printf("QSPI_FLASH_ReadID ERROR!!!....\r\n");
		/* 根据需要在此处添加错误处理 */
		while (1)
		{

		}
	}
//    SCB_CleanDCache_by_Addr((void *)pData, 3);
	Temp = (pData[2] | pData[1] << 8) | (pData[0] << 16);
	return Temp;
}
 
/**
 * @brief  读取 Flash Device ID
 * @retval 16 位 Device ID
 */
uint32_t W25Qx_QSPI_FLASH_ReadDeviceID(void)
{
	QSPI_CommandTypeDef s_command;
	uint32_t Temp = 0;
	uint8_t pData[3];
	/* 配置读取 Manufacturer/Device ID 命令 */
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction = READ_ID_CMD;
	s_command.AddressMode = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize = QSPI_ADDRESS_24_BITS;
	s_command.Address = 0x000000;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode = QSPI_DATA_1_LINE;
	s_command.DummyCycles = 0;
	s_command.NbData = 2;
	s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
 
	if (HAL_QSPI_Command(&hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		printf("QSPI_FLASH_ReadDeviceID ERROR!!!....\r\n");
		/* 根据需要在此处添加错误处理 */
		while (1)
		{

		}
	}
	if (HAL_QSPI_Receive(&hqspi, pData, HAL_QPSI_TIMEOUT_DEFAULT_VALUE)	!= HAL_OK)
	{
		printf("QSPI_FLASH_ReadDeviceID ERROR!!! ....\r\n");
		/* 根据需要在此处添加错误处理 */
		while (1)
		{

		}
	}
//    SCB_CleanDCache_by_Addr((void *)pData, 2);
	Temp = pData[1] | (pData[0] << 8);
 
	return Temp;
}

static void W25Qx_QSPI_Delay(uint32_t ms)
{
	HAL_Delay(ms);
}

/**
  * @brief  Configure the QSPI in memory-mapped mode
  * @retval QSPI memory status
  */
uint32_t QSPI_EnableMemoryMappedMode(QSPI_HandleTypeDef *QSPIHandle)
{
  QSPI_CommandTypeDef      s_command;
  QSPI_MemoryMappedTypeDef s_mem_mapped_cfg;
 
  /* Configure the command for the read instruction */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = QUAD_INOUT_FAST_READ_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_4_LINES;
  s_command.AddressSize       = QSPI_ADDRESS_24_BITS;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = 6;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_HALF_CLK_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
 
  /* Configure the memory mapped mode */
  s_mem_mapped_cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
  s_mem_mapped_cfg.TimeOutPeriod     = 0;
 
  return HAL_QSPI_MemoryMapped(QSPIHandle, &s_command, &s_mem_mapped_cfg);
}

/**
 * @brief  W25Qxx 全芯片读写检测
 * @note   顺序遍历整片 Flash（按扇区擦除、写入与校验），打印详细测试结果
 * @retval 无
 */
void W25Qx_QSPI_Test(void)
{
    uint32_t flash_id = 0;
    uint32_t device_id = 0;
    uint8_t  write_buf[4096];
    uint8_t  read_buf[4096];
    uint32_t i;
    uint32_t error_count = 0;
    uint32_t total_error_bytes = 0;
    uint32_t sector_index;
    uint32_t passed_sectors = 0;
    uint8_t  status;
    QSPI_Info flash_info;

    printf("\r\n");
    printf("========================================\r\n");
    printf("     W25Q64JVZEIQ Flash Test Start      \r\n");
    printf("========================================\r\n");
    
    printf("\r\n[Step 1] Reading JEDEC ID...\r\n");
    flash_id = W25Qx_QSPI_FLASH_ReadID();
    printf("  JEDEC ID: 0x%06X\r\n", flash_id);
    
    if(flash_id == 0xEF4017)
    {
        printf("  Manufacturer: Winbond (EF)\r\n");
        printf("  Memory Type:  0x40\r\n");
        printf("  Capacity:     64Mbit (0x17)\r\n");
        printf("  [PASS] JEDEC ID matches W25Q64JVZEIQ\r\n");
    }
    else if(flash_id == 0x000000 || flash_id == 0xFFFFFF)
    {
        printf("  [FAIL] Invalid JEDEC ID - Check QSPI connection!\r\n");
        return;
    }
    else
    {
        printf("  [WARN] JEDEC ID mismatch! Expected: 0xEF4017\r\n");
    }
    
    printf("\r\n[Step 2] Reading Device ID...\r\n");
    device_id = W25Qx_QSPI_FLASH_ReadDeviceID();
    printf("  Device ID: 0x%04X\r\n", device_id);
    if(device_id == 0x4017)
    {
        printf("  [PASS] Device ID correct\r\n");
    }
    else
    {
        printf("  [WARN] Device ID: 0x%04X\r\n", device_id);
    }
    
    printf("\r\n[Step 3] Getting Flash Info...\r\n");
    W25Qx_QSPI_GetInfo(&flash_info);
    printf("  Flash Size: %ld Bytes (%ld MB)\r\n", flash_info.FlashSize, flash_info.FlashSize / (1024 * 1024));
    printf("  Sector Size: %ld Bytes\r\n", flash_info.EraseSectorSize);
    printf("  Sector Count: %ld\r\n", flash_info.EraseSectorsNumber);
    printf("  Page Size: %ld Bytes\r\n", flash_info.ProgPageSize);
    printf("  Page Count: %ld\r\n", flash_info.ProgPagesNumber);
    
    printf("\r\n[Step 4] Checking Flash Status...\r\n");
    status = W25Qx_QSPI_GetStatus();
    if(status == QSPI_OK)
    {
        printf("  [PASS] Flash is ready\r\n");
    }
    else if(status == QSPI_BUSY)
    {
        printf("  [WARN] Flash is busy, waiting...\r\n");
        HAL_Delay(100);
    }
    else
    {
        printf("  [FAIL] Flash status error\r\n");
    }
    
    printf("\r\n[Step 5] Preparing test data...\r\n");
    printf("  将完整 Flash 按扇区进行擦除/写入/校验测试\r\n");

    if (flash_info.EraseSectorSize > sizeof(write_buf))
    {
        printf("  [WARN] 扇区大小(%ld)大于测试缓冲区(%d)，仅测试每扇区前 %d 字节\r\n",
               flash_info.EraseSectorSize, (int)sizeof(write_buf), (int)sizeof(write_buf));
    }

    /* 遍历整片 Flash，按扇区测试 */
    printf("\r\n[Step 6] Sector-wise full flash test...\r\n");
    for (sector_index = 0; sector_index < flash_info.EraseSectorsNumber; sector_index++)
    {
        uint32_t sector_addr = sector_index * flash_info.EraseSectorSize;
        uint32_t test_size = flash_info.EraseSectorSize;

        if (test_size > sizeof(write_buf))
        {
            test_size = sizeof(write_buf);
        }

        printf("  -> Sector %lu / %lu at 0x%06lX\r\n",
               (unsigned long)(sector_index + 1),
               (unsigned long)flash_info.EraseSectorsNumber,
               (unsigned long)sector_addr);

        /* 准备本扇区测试数据（简单花纹，带入扇区号） */
        for (i = 0; i < test_size; i++)
        {
            write_buf[i] = (uint8_t)((i + sector_index) & 0xFF);
        }

        /* 擦除扇区 */
        if (W25Qx_QSPI_Erase_Block(sector_addr) != QSPI_OK)
        {
            printf("    [FAIL] Erase sector failed!\r\n");
            continue;
        }

        /* 验证擦除（是否为 0xFF） */
        memset(read_buf, 0, test_size);
        if (W25Qx_QSPI_Read(read_buf, sector_addr, test_size) != QSPI_OK)
        {
            printf("    [FAIL] Read after erase failed!\r\n");
            continue;
        }

        error_count = 0;
        for (i = 0; i < test_size; i++)
        {
            if (read_buf[i] != 0xFF)
            {
                error_count++;
            }
        }
        if (error_count != 0)
        {
            printf("    [FAIL] %ld bytes are not 0xFF after erase\r\n", error_count);
            total_error_bytes += error_count;
            continue;
        }

        /* 写入测试数据 */
        if (W25Qx_QSPI_Write(write_buf, sector_addr, test_size) != QSPI_OK)
        {
            printf("    [FAIL] Write failed!\r\n");
            continue;
        }

        /* 读回并校验 */
        memset(read_buf, 0, test_size);
        if (W25Qx_QSPI_Read(read_buf, sector_addr, test_size) != QSPI_OK)
        {
            printf("    [FAIL] Read failed!\r\n");
            continue;
        }

        error_count = 0;
        for (i = 0; i < test_size; i++)
        {
            if (read_buf[i] != write_buf[i])
            {
                error_count++;
            }
        }

        if (error_count == 0)
        {
            passed_sectors++;
            printf("    [PASS] Sector verified OK\r\n");
        }
        else
        {
            total_error_bytes += error_count;
            printf("    [FAIL] %ld/%ld bytes mismatch\r\n", error_count, test_size);
        }
    }

    printf("\r\n[Summary] Full flash test result:\r\n");
    printf("  Total sectors   : %ld\r\n", flash_info.EraseSectorsNumber);
    printf("  Passed sectors  : %lu\r\n", (unsigned long)passed_sectors);
    printf("  Failed sectors  : %ld\r\n", flash_info.EraseSectorsNumber - passed_sectors);
    printf("  Total bad bytes : %lu\r\n", (unsigned long)total_error_bytes);

    printf("\r\n========================================\r\n");
    printf("     W25Q64JVZEIQ Flash Test Complete   \r\n");
    printf("========================================\r\n\r\n");
}
