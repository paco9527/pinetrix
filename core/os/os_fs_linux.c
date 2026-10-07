#include "os_fs.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int ptx_os_fs_exists(const char* path)
{
    return (path && access(path, R_OK) == 0) ? 1 : 0;
}

int ptx_os_fs_read_file(const char* path, char* buf, size_t cap, size_t* out_len)
{
    if (!path || !buf || cap == 0)
        return -1;

    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return -1;
    }

    size_t n = (size_t)st.st_size;
    if (n > cap - 1)
        n = cap - 1;

    ssize_t r = read(fd, buf, n);
    close(fd);
    if (r < 0)
        return -1;

    buf[r] = '\0';
    if (out_len)
        *out_len = (size_t)r;
    return 0;
}
