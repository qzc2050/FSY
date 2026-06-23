#!/usr/bin/env python3
"""Escape non-ASCII C string literals as UTF-8 \\xHH sequences for ARM Compiler 5."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def _leading_backslashes(text: str, idx: int) -> int:
    n = 0
    while idx - 1 - n >= 0 and text[idx - 1 - n] == "\\":
        n += 1
    return n


def _escape_utf8_bytes(raw: str) -> str:
  out: list[str] = []
  for ch in raw:
    data = ch.encode("utf-8")
    if len(data) == 1 and 32 <= data[0] < 127:
      out.append(ch)
    else:
      for b in data:
        out.append(f"\\x{b:02x}")
  return "".join(out)


def escape_c_string_literals(content: str) -> str:
  out: list[str] = []
  i = 0
  n = len(content)

  while i < n:
    ch = content[i]

    if ch == '"':
      prefix_start = i
      while prefix_start > 0 and (content[prefix_start - 1].isalnum() or content[prefix_start - 1] == "_"):
        prefix_start -= 1
      out.append(content[prefix_start:i])
      out.append('"')
      i += 1

      raw: list[str] = []
      while i < n:
        ch = content[i]
        if ch == '"' and (_leading_backslashes(content, i) % 2 == 0):
          out.append(_escape_utf8_bytes("".join(raw)))
          out.append('"')
          i += 1
          break
        raw.append(ch)
        i += 1
      continue

    out.append(ch)
    i += 1

  return "".join(out)


def convert_file(path: Path, dry_run: bool = False) -> bool:
  text = path.read_text(encoding="utf-8")
  converted = escape_c_string_literals(text)
  if converted == text:
    return False
  if not dry_run:
    path.write_text(converted, encoding="utf-8")
  return True


def main(argv: list[str]) -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument("files", nargs="+", type=Path)
  parser.add_argument("--dry-run", action="store_true")
  args = parser.parse_args(argv)

  changed = 0
  for file_path in args.files:
    if convert_file(file_path, dry_run=args.dry_run):
      changed += 1
      print(f"converted: {file_path}")
  print(f"done, changed {changed} file(s)")
  return 0


if __name__ == "__main__":
  raise SystemExit(main(sys.argv[1:]))
