---
name: "protocol-doc-extractor"
description: "Extracts and converts Office documents (.xlsx, .docx) to structured markdown. Invoke when user needs to read protocol docs, extract tables, or convert document content to text format."
---

# 协议文档提取技能

## 技能概述

本技能用于从 Office 文档（.xlsx、.docx）中提取协议文档内容，并转换为结构化的 Markdown 格式。

## 支持的文档格式

### Excel (.xlsx)
- 读取多个工作表（Sheet）
- 提取表格数据和单元格内容
- 保留行列关系和数据结构
- 处理合并单元格和格式化数据

### Word (.docx)
- 提取文档正文内容
- 解析标题层级结构
- 提取列表和表格
- 保留文本格式信息

## 文档位置

项目协议文档位于：
```
g:\Desktop\RAW_I_V20260324_mod_net_protocol _mod_self\docs\
├── 辐射报警仪协议命令和寄存器表 07.xlsx
└── 辐射报警仪协议说明 07.docx
```

## 提取方法

### 方法 1: Python + openpyxl（推荐用于 Excel）

```python
from openpyxl import load_workbook

# 加载工作簿
wb = load_workbook('辐射报警仪协议命令和寄存器表 07.xlsx', data_only=True)

# 遍历所有工作表
for sheet_name in wb.sheetnames:
    ws = wb[sheet_name]
    
    # 提取数据
    for row in ws.iter_rows(values_only=True):
        print(row)
```

### 方法 2: Python + python-docx（推荐用于 Word）

```python
from docx import Document

# 加载文档
doc = Document('辐射报警仪协议说明 07.docx')

# 提取段落
for para in doc.paragraphs:
    print(para.text)

# 提取表格
for table in doc.tables:
    for row in table.rows:
        cells = [cell.text for cell in row.cells]
        print(cells)
```

### 方法 3: 使用 pandoc 命令行工具

```bash
# Word 转 Markdown
pandoc docs/辐射报警仪协议说明 07.docx -o docs/protocol.md

# Excel 转 CSV（逐个工作表）
pandoc docs/辐射报警仪协议命令和寄存器表 07.xlsx -o docs/sheet1.csv
```

### 方法 4: 使用 LibreOffice 命令行

```bash
# 转换为 PDF
libreoffice --headless --convert-to pdf docs/*.docx

# 转换为文本
libreoffice --headless --convert-to txt docs/*.docx
```

## 提取的内容结构

### 从 Excel 提取

#### 工作表：具体命令
包含所有协议命令的详细信息：
- 命令接口类型（网口/串口/Can）
- 命令功能描述
- 通信方向（主机→从机/从机→主机）
- 数据帧格式（地址、功能码、数据、CRC）
- 示例数据

#### 工作表：寄存器表
包含所有寄存器的映射关系：
- 寄存器地址（十进制/十六进制）
- 寄存器数量
- 数据类型（uint8/uint16/uint32/char[]）
- 变量名
- 功能描述
- 读写权限
- 单位和缩放因子

#### 工作表：协议
包含协议格式说明：
- 功能码定义
- 帧格式说明
- 错误码定义
- 地址分配规则

### 从 Word 提取

#### 章节结构
1. 协议格式
   - 功能码 0x06/0x16/0x86
   - 功能码 0x10/0x20/0x90
   - 功能码 0x05/0x15/0x85
   - 功能码 0x03/0x13/0x83
   - 功能码 0x23/0x25（主动上传）

2. 地址定义
   - 主设备地址
   - 从设备地址
   - 转接板地址

3. 硬件接口
   - 网口通信（UDP/组播）
   - 串口通信

## 输出格式

### Markdown 表格格式

```markdown
| 地址 | 数量 | 类型 | 变量名 | 功能 | 单位 | 访问 |
|-----|------|-----|--------|------|------|------|
| 1 | 2 | uint32 | dose_rate | 辐射量 | uSv/h*100 | R |
| 3 | 2 | int32 | temp | 温度 | ℃*10 | R |
```

### 代码块格式

```
功能码 0x06 单寄存器写入:
[地址 1 字节][功能码 1 字节][寄存器地址 2 字节][寄存器值 N 字节][CRC 2 字节]
```

### 列表格式

- bit0: 辐射上阈值报警
- bit1: 辐射下阈值报警
- bit2: 辐射检测离线
- bit3: 保留

## 使用场景

1. **文档转换**: 将 Office 文档转换为 Markdown 便于版本控制
2. **内容提取**: 提取特定的寄存器表或命令列表
3. **代码生成**: 基于提取的协议数据自动生成代码
4. **文档对比**: 比较不同版本的协议文档差异
5. **快速查询**: 创建结构化的查询索引

## 工具依赖

### Python 包
```bash
pip install openpyxl python-docx pandas
```

### 命令行工具
```bash
# Ubuntu/Debian
sudo apt install pandoc libreoffice

# Windows (使用 Chocolatey)
choco install pandoc libreoffice
```

## 提取脚本示例

### extract_protocol.py

```python
#!/usr/bin/env python3
"""
辐射报警仪协议文档提取脚本
"""

from openpyxl import load_workbook
from docx import Document
import json

def extract_excel(filepath):
    """提取 Excel 文档内容"""
    wb = load_workbook(filepath, data_only=True)
    result = {}
    
    for sheet_name in wb.sheetnames:
        ws = wb[sheet_name]
        data = []
        
        for row in ws.iter_rows(values_only=True):
            data.append(row)
        
        result[sheet_name] = data
    
    return result

def extract_word(filepath):
    """提取 Word 文档内容"""
    doc = Document(filepath)
    content = {
        'paragraphs': [],
        'tables': []
    }
    
    for para in doc.paragraphs:
        content['paragraphs'].append(para.text)
    
    for idx, table in enumerate(doc.tables):
        table_data = []
        for row in table.rows:
            table_data.append([cell.text for cell in row.cells])
        content['tables'].append({
            'index': idx,
            'data': table_data
        })
    
    return content

if __name__ == '__main__':
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python extract_protocol.py <filepath>")
        sys.exit(1)
    
    filepath = sys.argv[1]
    
    if filepath.endswith('.xlsx'):
        data = extract_excel(filepath)
    elif filepath.endswith('.docx'):
        data = extract_word(filepath)
    else:
        print("Unsupported file format")
        sys.exit(1)
    
    print(json.dumps(data, indent=2, ensure_ascii=False))
```

## 注意事项

1. **编码问题**: 中文文档需要使用 UTF-8 编码
2. **格式保留**: 复杂格式（如合并单元格）可能需要特殊处理
3. **数据验证**: 提取后需要人工验证数据准确性
4. **版本管理**: 建议将提取的 Markdown 纳入版本控制
5. **批量处理**: 可以使用脚本批量处理多个文档

## 相关资源

- [openpyxl 文档](https://openpyxl.readthedocs.io/)
- [python-docx 文档](https://python-docx.readthedocs.io/)
- [pandoc 官网](https://pandoc.org/)
- [Modbus 协议规范](https://modbus.org/specs.php)
