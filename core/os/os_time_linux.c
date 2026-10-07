#include "os_time.h"

#include <time.h>
#include <unistd.h>

int64_t ptx_os_time_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void ptx_os_sleep_ms(int ms)
{
    if (ms > 0)
        usleep((useconds_t)ms * 1000);
}

int ptx_os_time_localtime(struct tm* out)
{
    if (!out)
        return -1;
    time_t t = time(NULL);
    return localtime_r(&t, out) ? 0 : -1;
}
