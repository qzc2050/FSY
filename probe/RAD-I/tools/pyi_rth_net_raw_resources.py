# PyInstaller runtime hook（打包 exe 启动时自动执行）
import net_raw_pack_support as _pack

_pack.early_init()
