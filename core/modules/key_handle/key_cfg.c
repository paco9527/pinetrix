/* 板级按键表 -- 换板子就改这个文件 */
#include "ptx_key.h"

/* 示例: 4 个按键 (名字/引脚是占位, 板子画好后按实际接线改)
 *   PREV/NEXT/OK : 普通键(归当前前台应用)
 *   HOME         : 保留键(归启动器/系统)
 */
static const ptx_key_cfg_t g_keys[] = {
    /* id      spec          active_low  long_press_ms  reserved */
    { "PREV",  "BOARD11",    1,          800,           0 },
    { "NEXT",  "BOARD13",    1,          800,           0 },
    { "OK",    "BOARD15",    1,          800,           0 },
    { "HOME",  "BOARD16",    1,          1500,          1 },
};

const ptx_key_cfg_t* ptx_key_cfg(int* count)
{
    if (count)
        *count = (int)(sizeof(g_keys) / sizeof(g_keys[0]));
    return g_keys;
}
