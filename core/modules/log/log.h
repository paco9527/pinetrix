#ifndef __LOG_
#define __LOG_

#include <stddef.h>

/* 等级递增 = 更严重; 显示条件: msg_level >= log_level */
typedef enum
{
    DEBUG = 0,
    INFO  = 1,
    ERROR = 2,
} LOG_LEVEL;

extern LOG_LEVEL log_level;

typedef int(*export_log)(char*, size_t);

void log_set_export_func(export_log write_func);
void log_set_level(LOG_LEVEL level);

/* 唯一格式化输出入口; 等级判断集中于此 */
void writelog(const char* file, int line, int level, const char* fmt, ...);

/* 该等级当前是否输出 */
int log_enabled(LOG_LEVEL lvl);

#define LOG_DEBUG(fmt, ...) writelog(__FILE__, __LINE__, DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  writelog(__FILE__, __LINE__, INFO,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) writelog(__FILE__, __LINE__, ERROR, fmt, ##__VA_ARGS__)

#define ASSERT_RET(c, exec) {if(c){LOG_ERROR("assert failed");exec;}}

#endif
