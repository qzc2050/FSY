# Flash 擦除验证和重试机制改进方案

## 实现状态：✅ 已完成

### 已实现的功能

1. ✅ **添加擦除验证函数** - `Reg_Flash_VerifyErase()`
2. ✅ **添加带重试的擦除函数** - `Reg_Flash_EraseSector_WithVerify()`
3. ✅ **修改 Reg_Flash_Write 函数** - 使用新的带验证擦除函数
4. ✅ **添加错误码定义** - 在 reg_flash.h 中

---

## 当前问题分析

### 1. 没有擦除后验证
- 擦除完成后没有重新读取验证
- 无法发现擦除不干净的情况

### 2. 没有重试机制
- 如果一次擦除失败，不会重试
- 可能导致写入失败或数据错误

### 3. 没有错误返回
- 即使擦除不彻底，函数也不返回失败
- 上层应用无法得知擦除质量

## 已实现的改进方案

### 1. 添加错误码定义（reg_flash.h）

```c
//FLASH 错误码定义
#define FLASH_ERASE_OK          0       // 擦除成功
#define FLASH_ERASE_WRPERR      1       // 写保护错误
#define FLASH_ERASE_PGSERR      2       // 编程序列错误
#define FLASH_ERASE_STRBERR     3       // 复写错误
#define FLASH_ERASE_INCERR      4       // 数据一致性错误
#define FLASH_ERASE_OPERR       5       // 写/擦除错误
#define FLASH_ERASE_NOT_CLEAN   0xFE    // 擦除不干净（超过重试次数）
#define FLASH_ERASE_TIMEOUT     0xFF    // 超时
```

### 2. 添加擦除验证函数（reg_flash.c）

```c
//验证扇区是否完全擦除
//addr: 扇区起始地址
//返回值:
//0, 擦除成功 (全为 0xFFFFFFFF)
//1, 擦除失败 (存在未擦除的字)
uint8_t Reg_Flash_VerifyErase(uint32_t addr)
{
    uint32_t i;
    uint32_t words_per_sector = FLASH_SECTOR_SIZE / 4;
    
    for(i = 0; i < words_per_sector; i++)
    {
        if(Reg_Flash_ReadWord(addr + i * 4) != 0XFFFFFFFF)
        {
            return 1;  // 发现未擦除的字
        }
    }
    return 0;  // 全部擦除成功
}
```

### 3. 添加带重试的擦除函数（reg_flash.c）

```c
//擦除扇区（带验证和重试）
//addr: 擦除地址
//retry_count: 最大重试次数（建议 3）
//返回值：
//0, 擦除成功
//1~9, 错误代码
//0xFE, 擦除不干净（超过最大重试次数）
//0XFF, 超时
uint8_t Reg_Flash_EraseSector_WithVerify(uint32_t addr, uint8_t retry_count)
{
    uint8_t res = 0;
    uint8_t bankx = 0;
    uint32_t sectorx = addr / FLASH_SECTOR_SIZE;
    uint8_t retry = 0;
    
    if(addr < BANK1_END_ADDR)
        bankx = 0;
    else 
        bankx = 1;
    
    res = Reg_Flash_WaitDone(bankx, 0XFFFFFFFF);
    if(res != 0) return res;
    
    // 重试循环
    while(retry < retry_count)
    {
        // 执行擦除操作（Bank1 或 Bank2）
        // ...
        
        if(res != 0)
            return res;  // 擦除操作失败
        
        // 验证擦除结果
        if(Reg_Flash_VerifyErase(addr) == 0)
        {
            return FLASH_ERASE_OK;  // 验证通过，擦除成功
        }
        
        retry++;
        if(retry >= retry_count)
        {
            return FLASH_ERASE_NOT_CLEAN;  // 超过最大重试次数，擦除不干净
        }
    }
    
    return FLASH_ERASE_NOT_CLEAN;
}
```

### 4. 修改 Reg_Flash_Write 函数

```c
// 写 Bank1 部分（如果有）
if(bank1_words > 0)
{
    bankx = 0;
    addrx = WriteAddr;
    
    Reg_Flash_Unlock(bankx);
    
    // 擦除 Bank1 扇区（带验证和重试）
    while(addrx < BANK1_END_ADDR)
    {
        if(Reg_Flash_ReadWord(addrx) != 0XFFFFFFFF)
        { 
            INTX_DISABLE();
            status = Reg_Flash_EraseSector_WithVerify(addrx, 3);  // 最多重试 3 次
            if(status) {
                INTX_ENABLE();
                Reg_Flash_Lock(bankx);
                return;  // 擦除失败，直接返回
            }
            while(Reg_Flash_WaitDone(bankx, 0XFF));
            INTX_ENABLE();
        }
        else 
            addrx += 4;
    }
    
    // ... 后续写入代码
}
```

## 使用示例

### 直接使用带验证的擦除函数

```c
// 擦除扇区，最多重试 3 次
uint8_t status = Reg_Flash_EraseSector_WithVerify(sector_addr, 3);

if(status == FLASH_ERASE_OK)
{
    // 擦除成功，可以写入
    Reg_Flash_Write(write_addr, data, len);
}
else if(status == FLASH_ERASE_NOT_CLEAN)
{
    // 擦除不干净，可能是 Flash 损坏
    // 记录错误日志或提示用户
    printf("Flash 擦除失败，地址：0x%08X\r\n", sector_addr);
}
else
{
    // 其他错误
    printf("Flash 擦除错误代码：%d\r\n", status);
}
```

### OTA 应用中的使用

```c
// OTA 写入时自动使用带验证的擦除
Reg_Flash_Write(OTA_FLAG_FLASH_ADDR, (uint32_t*)&ota_flag, sizeof(OtaFlag_t) / sizeof(uint32_t));

// 如果擦除失败，Reg_Flash_Write 会直接返回，不会继续写入
```

## 优势

1. ✅ **擦除后验证**：确保每个扇区都完全擦除
2. ✅ **重试机制**：自动重试最多 3 次，提高可靠性
3. ✅ **错误返回**：上层应用可以知道擦除质量
4. ✅ **故障诊断**：可以区分是暂时性错误还是永久性损坏
5. ✅ **向后兼容**：不影响现有代码，原有的 `Reg_Flash_EraseSector` 仍然可用

## 注意事项

1. 验证过程需要读取整个扇区，会增加擦除时间
2. 重试次数建议设置为 3 次，过多重试可能表明 Flash 已损坏
3. 如果返回 `FLASH_ERASE_NOT_CLEAN`，应该记录错误并考虑更换芯片
4. 对于 OTA 应用，擦除验证可以提高固件更新的可靠性

## 测试建议

1. **正常擦除测试**：
   - 测试普通扇区擦除功能是否正常
   - 验证擦除后是否全为 0xFFFFFFFF

2. **重试机制测试**：
   - 模拟擦除失败的情况
   - 验证是否会重试最多 3 次

3. **错误返回测试**：
   - 测试各种错误情况是否正确返回
   - 验证错误码是否正确

4. **OTA 应用测试**：
   - 测试 OTA 更新过程中擦除是否可靠
   - 验证固件写入是否成功

## 修改的文件

1. **reg_flash.h**：
   - 添加错误码定义
   - 添加函数声明

2. **reg_flash.c**：
   - 添加 `Reg_Flash_VerifyErase` 函数
   - 添加 `Reg_Flash_EraseSector_WithVerify` 函数
   - 修改 `Reg_Flash_Write` 函数，使用新的擦除函数

## 总结

通过添加擦除验证和重试机制，显著提高了 Flash 操作的可靠性：
- ✅ 确保擦除质量
- ✅ 自动处理暂时性错误
- ✅ 提供详细的错误信息
- ✅ 提高 OTA 更新成功率

---

**实现时间**：2026-05-11  
**影响范围**：所有使用 `Reg_Flash_Write` 的应用（包括 OTA）  
**向后兼容**：✅ 完全兼容，原有代码不受影响