#ifndef _PTX_OS_FS_H
#define _PTX_OS_FS_H

#include <stddef.h>

/* 读取整个文件到 buf (至多 cap-1 字节, 结尾补 '\0'); 成功返回 0 */
int ptx_os_fs_read_file(const char* path, char* buf, size_t cap, size_t* out_len);

/* 文件是否存在且可读 */
int ptx_os_fs_exists(const char* path);

#endif /* _PTX_OS_FS_H */
