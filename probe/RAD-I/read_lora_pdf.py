import pypdf

reader = pypdf.PdfReader('f:/Raydose_Gitee/RAD-I/docs/LORA-E32-433T20S.pdf')
print(f'总页数：{len(reader.pages)}')

for i in range(len(reader.pages)):
    text = reader.pages[i].extract_text()
    print(f'\n========== 第{i+1}页 ==========')
    print(text)
