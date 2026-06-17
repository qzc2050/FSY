#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""源码/打包统一启动入口"""
import os
import sys

_tools_dir = os.path.dirname(os.path.abspath(__file__))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)
os.chdir(_tools_dir)

import net_raw_pack_support as _pack

_pack.early_init()

import net_raw_tester

_pack.register_gui_class(net_raw_tester.NetRawTesterGUI)

if __name__ == "__main__":
    net_raw_tester.main()
