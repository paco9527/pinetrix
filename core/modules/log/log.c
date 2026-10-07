#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "log.h"
#include "ptx_config.h"
#include "os_console.h"
#include "os_time.h"

LOG_LEVEL log_level = ERROR;

static export_log g_write_func = NULL;
static char logcontent[MAX_EXPORT_CONTENT_LEN];

int log_enabled(LOG_LEVEL lvl)
{
    return lvl >= log_level;
}

/* 只取文件名, 去掉编译机上的绝对路径 (同时处理 '/' 与 '\') */
static const char* base_name(const char* file)
{
    const char* s1 = file ? strrchr(file, '/') : NULL;
    const char* s2 = file ? strrchr(file, '\\') : NULL;
    const char* s = (s1 > s2) ? s1 : s2;
    return s ? s + 1 : (file ? file : "?");
}

void writelog(const char* file, int line, int level, const char* fmt, ...)
{
    if (!log_enabled((LOG_LEVEL)level))
        return;

    char head[160] = {0};
    int head_len = 0;
    switch (level) {
        case DEBUG:
            head_len = snprintf(head, sizeof(head), "[%s][%d][debug]:", base_name(file), line);
            break;
        case INFO:
            head_len = snprintf(head, sizeof(head), "[%s][%d][info] :", base_name(file), line);
            break;
        default:
            head_len = snprintf(head, sizeof(head), "[%s][%d][error]:", base_name(file), line);
            break;
    }

    struct tm tm;
    if (ptx_os_time_localtime(&tm) == 0) {
        head_len += snprintf(head + head_len, sizeof(head) - head_len,
                             " %02d-%02d %02d:%02d:%02d\t",
                             tm.tm_mon + 1, tm.tm_mday,
                             tm.tm_hour, tm.tm_min, tm.tm_sec);
    }

    strncpy(logcontent, head, sizeof(logcontent) - 1);
    logcontent[sizeof(logcontent) - 1] = '\0';
    size_t used = strlen(logcontent);

    va_list arg;
    va_start(arg, fmt);
    vsnprintf(logcontent + used, sizeof(logcontent) - used, fmt, arg);
    va_end(arg);

    /* 保证每条日志以单个换行结束 (fmt 自带与否都只留一个) */
    size_t len = strlen(logcontent);
    if (len == 0 || logcontent[len - 1] != '\n') {
        if (len + 1 < sizeof(logcontent)) {
            logcontent[len] = '\n';
            logcontent[len + 1] = '\0';
        }
    }

    ptx_os_console_write(logcontent, strlen(logcontent));

    if (g_write_func) {
        size_t total_len = strlen(logcontent);
        if (total_len > MAX_EXPORT_CONTENT_LEN)
            total_len = MAX_EXPORT_CONTENT_LEN;
        g_write_func(logcontent, total_len);
    }
}

void log_set_export_func(export_log write_func)
{
    g_write_func = write_func;
    LOG_DEBUG("log construct complete");
}

void log_set_level(LOG_LEVEL level)
{
    log_level = level;
}
