#ifndef _PTX_OS_CONSOLE_H
#define _PTX_OS_CONSOLE_H

#include <stddef.h>

/* 读取一行(含换行); 返回读到的字符数, 0 = EOF/无数据 */
int  ptx_os_console_read_line(char* buf, size_t cap);

void ptx_os_console_write(const char* s, size_t n);
void ptx_os_console_puts(const char* s);

#endif /* _PTX_OS_CONSOLE_H */
