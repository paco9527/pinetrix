#ifndef _PTX_OS_TIME_H
#define _PTX_OS_TIME_H

#include <stdint.h>
#include <time.h>

/* 单调毫秒时间戳 */
int64_t ptx_os_time_now_ms(void);

/* 睡眠 ms 毫秒 */
void ptx_os_sleep_ms(int ms);

/* 取本地时间; 成功 0, 失败 -1 (struct tm 为标准 C 类型) */
int ptx_os_time_localtime(struct tm* out);

#endif /* _PTX_OS_TIME_H */
