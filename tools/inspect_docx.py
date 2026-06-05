import re
import zipfile
from pathlib import Path

path = Path(r"e:\androidtest\Fsy\20260529需求整理.docx")
out_dir = Path(r"e:\androidtest\Fsy\docs\screenshots")
out_dir.mkdir(parents=True, exist_ok=True)

with zipfile.ZipFile(path) as z:
    media = [n for n in z.namelist() if n.startswith("word/media/")]
    print(f"media files: {len(media)}")
    for n in media:
        info = z.getinfo(n)
        print(f"  {n}  {info.file_size} bytes")
        data = z.read(n)
        name = Path(n).name
        (out_dir / name).write_bytes(data)
        print(f"    -> extracted to docs/screenshots/{name}")

    rels = z.read("word/_rels/document.xml.rels").decode("utf-8")
    imgs = re.findall(r'Target="(media/[^"]+)"', rels)
    print("linked images:", imgs)
