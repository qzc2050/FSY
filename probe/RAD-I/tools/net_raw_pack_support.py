# -*- coding: utf-8 -*-
"""打包/启动辅助：资源路径、任务栏图标、窗口防闪烁（上位机主程序仅保留两行调用）"""
import os
import sys

_APP_ID = "Raydose.RWD-I.HostTool.1.0"
_tk_patched = False
_gui_patched = False


def resource_dir() -> str:
    if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"):
        return sys._MEIPASS
    return os.path.dirname(os.path.abspath(__file__))


def resource_path(name: str) -> str:
    return os.path.join(resource_dir(), name)


def setup_dll_search_path() -> None:
    """PyInstaller 单文件：libusb / pyusb 可加载 DLL"""
    if not getattr(sys, "frozen", False) or not hasattr(sys, "_MEIPASS"):
        return
    meipass = sys._MEIPASS
    if hasattr(os, "add_dll_directory"):
        os.add_dll_directory(meipass)
    os.environ["PATH"] = meipass + os.pathsep + os.environ.get("PATH", "")


def setup_windows_app_id() -> None:
    """Windows 任务栏独立图标（需在创建 Tk 之前调用）"""
    if sys.platform != "win32":
        return
    try:
        import ctypes
        ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(_APP_ID)
    except Exception:
        pass


def _icon_path() -> str:
    path = resource_path("rw.ico")
    if os.path.isfile(path):
        return os.path.abspath(path)
    return ""


def apply_window_icon(window) -> None:
    """窗口左上角 + 任务栏图标"""
    icon_path = _icon_path()
    if not icon_path:
        return
    try:
        window.iconbitmap(default=icon_path)
    except Exception:
        pass
    try:
        window.iconbitmap(icon_path)
    except Exception:
        pass


def patch_tk_startup() -> None:
    """创建 Tk 时先隐藏，并设置图标，避免启动闪小窗"""
    global _tk_patched
    if _tk_patched:
        return

    import tkinter as tk

    orig_init = tk.Tk.__init__

    def _init_with_startup(self, *args, **kwargs):
        orig_init(self, *args, **kwargs)
        self.withdraw()
        apply_window_icon(self)

    tk.Tk.__init__ = _init_with_startup
    _tk_patched = True


def register_gui_class(gui_class) -> None:
    """GUI 构建完成后再显示主窗口（与 patch_tk_startup 配合）"""
    global _gui_patched
    if _gui_patched:
        return

    orig_init = gui_class.__init__

    def _init_with_deiconify(self, root, *args, **kwargs):
        orig_init(self, root, *args, **kwargs)
        try:
            root.update_idletasks()
            root.deiconify()
        except Exception:
            pass

    gui_class.__init__ = _init_with_deiconify
    _gui_patched = True


def early_init() -> None:
    """启动早期初始化（直接运行 py / 打包 exe 均会执行）"""
    setup_windows_app_id()
    setup_dll_search_path()
    patch_tk_startup()


# 兼容旧调用名
def patch_tk_rw_icon() -> None:
    early_init()
