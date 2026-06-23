#!/usr/bin/env python3
"""Remove LVGL groups/files and include paths from NeiJi.uvprojx."""
import re
import sys
from pathlib import Path

UVPROJX = Path(__file__).resolve().parents[1] / "MDK-ARM" / "NeiJi.uvprojx"

LVGL_GROUP_PREFIXES = ("LVGL/",)
REMOVE_FILES = {
    "ui_task.c",
    "joystick.c",
}

INCLUDE_DROP = (
    "../LVGL",
    "../LVGL/src",
    "../LVGL/src/porting",
    "../LVGL/app",
)


def strip_include_path(path: str) -> str:
    parts = [p for p in path.split(";") if p]
    out = []
    for p in parts:
        norm = p.strip().lstrip(";")
        if not norm:
            continue
        if any(norm.endswith(d) or norm == d.lstrip("../") for d in INCLUDE_DROP):
            continue
        if "LVGL" in norm.replace("\\", "/"):
            continue
        out.append(norm)
    return ";".join(out)


def main() -> int:
    text = UVPROJX.read_text(encoding="utf-8")

  # drop entire <Group> blocks for LVGL/*
    def drop_lvgl_group(m: re.Match) -> str:
        name = m.group(1)
        if any(name.startswith(p) for p in LVGL_GROUP_PREFIXES):
            return ""
        return m.group(0)

    text = re.sub(
        r"<Group>\s*<GroupName>([^<]+)</GroupName>.*?</Group>",
        drop_lvgl_group,
        text,
        flags=re.DOTALL,
    )

    # remove specific File entries inside remaining groups
    for fname in REMOVE_FILES:
        text = re.sub(
            rf"\s*<File>\s*<FileName>{re.escape(fname)}</FileName>.*?</File>",
            "",
            text,
            flags=re.DOTALL,
        )

    # clean IncludePath tags
    def fix_include(m: re.Match) -> str:
        inner = strip_include_path(m.group(1))
        return f"<IncludePath>{inner}</IncludePath>"

    text = re.sub(r"<IncludePath>([^<]*)</IncludePath>", fix_include, text)

    UVPROJX.write_text(text, encoding="utf-8")
    print(f"Updated {UVPROJX}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
