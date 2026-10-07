#include "os_console.h"

#include <stdio.h>
#include <string.h>

int ptx_os_console_read_line(char* buf, size_t cap)
{
    if (!buf || cap == 0)
        return 0;
    if (!fgets(buf, (int)cap, stdin))
        return 0;
    return (int)strlen(buf);
}

void ptx_os_console_write(const char* s, size_t n)
{
    if (s && n)
        fwrite(s, 1, n, stdout);
}

void ptx_os_console_puts(const char* s)
{
    if (s)
        fputs(s, stdout);
}
