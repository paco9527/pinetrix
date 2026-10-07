#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#include "os_net.h"
#include "log.h"

int ptx_os_net_open(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    return fd;
}

int ptx_os_net_connect_ip(int fd, char* addr, uint16_t port)
{
    ASSERT_RET(addr == NULL, -1);
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, addr, &serveraddr.sin_addr.s_addr);
    serveraddr.sin_port = htons(port);
    return connect(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
}

int ptx_os_net_read(int fd, char* buf, int buf_size)
{
    ASSERT_RET(buf == NULL, -1);
    return read(fd, buf, buf_size);
}

// timeout:ms
int ptx_os_net_timed_read(int socket, uint8_t* buf, size_t size, int timeout)
{
    struct timeval tm;
    fd_set read_fd;
    FD_ZERO(&read_fd);
    FD_SET(socket, &read_fd);
    tm.tv_sec = timeout/1000;
    tm.tv_usec = timeout%1000*1000;
    if(select(socket+1, &read_fd, NULL, NULL, &tm))
    {
        if(FD_ISSET(socket, &read_fd))
        {
            return read(socket, buf, size);
        }
    }
    return -1;
}

int ptx_os_net_write(int fd, char* buf, int buf_size)
{
    ASSERT_RET(buf == NULL, -1);
    return write(fd, buf, buf_size);
}

// timeout:ms
int ptx_os_net_timed_write(int socket, uint8_t* buf, size_t size, int timeout)
{
    struct timeval tm;
    fd_set write_fd;
    FD_ZERO(&write_fd);
    FD_SET(socket, &write_fd);
    tm.tv_sec = timeout/1000;
    tm.tv_usec = timeout%1000*1000;
    if(select(socket+1, NULL, &write_fd, NULL, &tm))
    {
        if(FD_ISSET(socket, &write_fd))
        {
            return write(socket, buf, size);
        }
    }
    return -1;
}

int ptx_os_net_close(int fd)
{
    close(fd);
}

/* ==================== 域名解析 ==================== */

int ptx_os_net_resolve(const char* host, char* ip_out, size_t ip_out_len)
{
    if (!host || !ip_out || ip_out_len < INET_ADDRSTRLEN)
        return -1;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = NULL;
    if (getaddrinfo(host, NULL, &hints, &res) != 0 || res == NULL)
        return -1;

    struct sockaddr_in* sin = (struct sockaddr_in*)res->ai_addr;
    const char* p = inet_ntop(AF_INET, &sin->sin_addr, ip_out, (socklen_t)ip_out_len);
    freeaddrinfo(res);
    return p ? 0 : -1;
}

static int resolve_addr(const char* host, uint16_t port, struct sockaddr_in* out)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char portstr[8];
    snprintf(portstr, sizeof(portstr), "%u", (unsigned)port);

    struct addrinfo* res = NULL;
    if (getaddrinfo(host, portstr, &hints, &res) != 0 || res == NULL)
        return -1;

    memcpy(out, res->ai_addr, sizeof(struct sockaddr_in));
    freeaddrinfo(res);
    return 0;
}

/* ==================== HTTP ==================== */

static void set_err(char* err, size_t err_len, const char* msg)
{
    if (err && err_len)
        snprintf(err, err_len, "%s", msg);
}

static int connect_timed(int fd, const struct sockaddr_in* addr, int timeout_ms)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int r = connect(fd, (const struct sockaddr*)addr, sizeof(*addr));
    if (r == 0)
    {
        fcntl(fd, F_SETFL, flags);
        return 0;
    }
    if (errno != EINPROGRESS)
    {
        fcntl(fd, F_SETFL, flags);
        return -1;
    }

    fd_set wf;
    FD_ZERO(&wf);
    FD_SET(fd, &wf);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    r = select(fd + 1, NULL, &wf, NULL, &tv);
    if (r <= 0)
    {
        fcntl(fd, F_SETFL, flags);
        return -1;
    }

    int soerr = 0;
    socklen_t slen = sizeof(soerr);
    fcntl(fd, F_SETFL, flags);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &slen) < 0 || soerr != 0)
        return -1;
    return 0;
}

static int send_all(int fd, const char* buf, size_t len, int timeout_ms)
{
    size_t sent = 0;
    while (sent < len)
    {
        fd_set wf;
        FD_ZERO(&wf);
        FD_SET(fd, &wf);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (select(fd + 1, NULL, &wf, NULL, &tv) <= 0)
            return -1;
        ssize_t n = write(fd, buf + sent, len - sent);
        if (n <= 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

/* http://host[:port]/path?query -> scheme/host/port/target */
static int parse_url(const char* url,
                     char* scheme, size_t sl,
                     char* host, size_t hl,
                     uint16_t* port,
                     char* target, size_t tl)
{
    const char* p = strstr(url, "://");
    if (!p)
        return -1;

    size_t schemelen = (size_t)(p - url);
    if (schemelen == 0 || schemelen >= sl)
        return -1;
    memcpy(scheme, url, schemelen);
    scheme[schemelen] = '\0';

    const char* a = p + 3;   /* authority */
    const char* ae = a;
    while (*ae && *ae != '/' && *ae != '?' && *ae != '#')
        ae++;

    /* 丢掉 userinfo */
    const char* at = memchr(a, '@', (size_t)(ae - a));
    const char* h = at ? at + 1 : a;

    const char* colon = memchr(h, ':', (size_t)(ae - h));
    size_t hostlen = colon ? (size_t)(colon - h) : (size_t)(ae - h);
    if (hostlen == 0 || hostlen >= hl)
        return -1;
    memcpy(host, h, hostlen);
    host[hostlen] = '\0';

    if (colon && colon + 1 < ae)
        *port = (uint16_t)atoi(colon + 1);
    else
        *port = 0;

    if (*ae == '\0')
    {
        if (tl < 2)
            return -1;
        strcpy(target, "/");
    }
    else
    {
        const char* te = ae;
        while (*te && *te != '#')
            te++;
        size_t tlen = (size_t)(te - ae);
        if (tlen + 1 > tl)
            return -1;
        memcpy(target, ae, tlen);
        target[tlen] = '\0';
    }
    return 0;
}

/* 去分块编码; 返回 malloc 的 body, 长度写入 out_len */
static char* dechunk(const char* in, size_t in_len, size_t* out_len)
{
    char* out = (char*)malloc(in_len + 1);
    if (!out)
        return NULL;

    size_t o = 0, i = 0;
    while (i < in_len)
    {
        size_t sz = 0;
        int digits = 0;
        while (i < in_len)
        {
            char c = in[i];
            int v;
            if (c >= '0' && c <= '9')       v = c - '0';
            else if (c >= 'a' && c <= 'f')  v = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')  v = c - 'A' + 10;
            else break;
            sz = sz * 16 + (size_t)v;
            digits++;
            i++;
        }
        if (!digits)
            break;
        while (i < in_len && in[i] != '\n')   /* 跳到行尾 */
            i++;
        if (i < in_len)
            i++;
        if (sz == 0)
            break;                            /* 末块 */
        if (i + sz > in_len)
            sz = in_len - i;
        memcpy(out + o, in + i, sz);
        o += sz;
        i += sz;
        while (i < in_len && (in[i] == '\r' || in[i] == '\n'))
            i++;
    }
    out[o] = '\0';
    *out_len = o;
    return out;
}

int ptx_os_net_http_request(const char* url,
                     const char* method,
                     const char* extra_headers,
                     const char* body,
                     int timeout_ms,
                     int max_body,
                     ptx_os_net_response_t* out,
                     char* err, size_t err_len)
{
    char scheme[16] = {0};
    char host[256] = {0};
    char target[512] = {0};
    uint16_t port = 0;
    struct sockaddr_in addr;
    int fd = -1;
    char* head = NULL;
    char* buf = NULL;

    if (!out)
    {
        set_err(err, err_len, "bad args");
        return -1;
    }
    memset(out, 0, sizeof(*out));

    if (!url)
    {
        set_err(err, err_len, "url required");
        return -1;
    }
    if (!method)
        method = "GET";
    if (timeout_ms <= 0)
        timeout_ms = 5000;
    if (max_body <= 0)
        max_body = 65536;

    if (parse_url(url, scheme, sizeof(scheme), host, sizeof(host), &port,
                  target, sizeof(target)) != 0)
    {
        set_err(err, err_len, "invalid url");
        return -1;
    }
    if (strcasecmp(scheme, "http") != 0)
    {
        set_err(err, err_len,
                strcasecmp(scheme, "https") == 0 ? "TLS not supported"
                                                 : "unsupported scheme");
        return -1;
    }
    if (port == 0)
        port = 80;

    if (resolve_addr(host, port, &addr) != 0)
    {
        set_err(err, err_len, "dns failed");
        return -1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        set_err(err, err_len, "socket failed");
        return -1;
    }
    if (connect_timed(fd, &addr, timeout_ms) != 0)
    {
        set_err(err, err_len, "connect failed");
        ptx_os_net_close(fd);
        return -1;
    }

    /* 组装请求 */
    {
        size_t body_len = body ? strlen(body) : 0;
        size_t ex_len = extra_headers ? strlen(extra_headers) : 0;
        char hosthdr[300];
        if (port == 80)
            snprintf(hosthdr, sizeof(hosthdr), "%s", host);
        else
            snprintf(hosthdr, sizeof(hosthdr), "%s:%u", host, (unsigned)port);

        size_t headcap = strlen(method) + strlen(target) + strlen(hosthdr)
                         + ex_len + 128;
        head = (char*)malloc(headcap + body_len + 1);
        if (!head)
        {
            set_err(err, err_len, "oom");
            ptx_os_net_close(fd);
            return -1;
        }
        int hn = 0;
        hn += snprintf(head + hn, headcap - hn, "%s %s HTTP/1.1\r\n", method, target);
        hn += snprintf(head + hn, headcap - hn, "Host: %s\r\n", hosthdr);
        hn += snprintf(head + hn, headcap - hn, "Connection: close\r\n");
        hn += snprintf(head + hn, headcap - hn, "User-Agent: pinetrix/1.0\r\n");
        if (ex_len)
            hn += snprintf(head + hn, headcap - hn, "%s", extra_headers);
        if (body_len)
            hn += snprintf(head + hn, headcap - hn, "Content-Length: %zu\r\n", body_len);
        hn += snprintf(head + hn, headcap - hn, "\r\n");
        if (body_len)
        {
            memcpy(head + hn, body, body_len);
            hn += (int)body_len;
        }

        if (send_all(fd, head, (size_t)hn, timeout_ms) != 0)
        {
            set_err(err, err_len, "send failed");
            free(head);
            ptx_os_net_close(fd);
            return -1;
        }
        free(head);
        head = NULL;
    }

    /* 收响应 (Connection: close -> 读到 EOF; 有 Content-Length 可提前结束) */
    {
        size_t cap = (size_t)max_body + 16384;
        buf = (char*)malloc(cap);
        if (!buf)
        {
            set_err(err, err_len, "oom");
            ptx_os_net_close(fd);
            return -1;
        }

        size_t total = 0;
        size_t head_len = 0;
        long content_length = -1;

        while (total < cap)
        {
            fd_set rf;
            FD_ZERO(&rf);
            FD_SET(fd, &rf);
            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            if (select(fd + 1, &rf, NULL, NULL, &tv) <= 0)
                break;
            ssize_t n = read(fd, buf + total, cap - total);
            if (n <= 0)
                break;
            total += (size_t)n;

            if (head_len == 0)
            {
                for (size_t k = 0; k + 3 < total; k++)
                {
                    if (buf[k] == '\r' && buf[k+1] == '\n' &&
                        buf[k+2] == '\r' && buf[k+3] == '\n')
                    {
                        head_len = k + 4;
                        break;
                    }
                }
                if (head_len)
                {
                    for (size_t k = 0; k + 15 < head_len; k++)
                    {
                        if (strncasecmp(buf + k, "Content-Length:", 15) == 0)
                        {
                            content_length = atol(buf + k + 15);
                            break;
                        }
                    }
                }
            }
            if (head_len && content_length >= 0 &&
                total >= head_len + (size_t)content_length)
                break;
        }
        ptx_os_net_close(fd);

        if (total == 0)
        {
            set_err(err, err_len, "no response");
            free(buf);
            return -1;
        }

        /* raw */
        out->raw = (char*)malloc(total + 1);
        if (!out->raw)
        {
            set_err(err, err_len, "oom");
            free(buf);
            return -1;
        }
        memcpy(out->raw, buf, total);
        out->raw[total] = '\0';
        out->raw_len = total;

        /* head (含状态行, 不含结尾空行) */
        if (head_len >= 2)
        {
            size_t hl = head_len - 2;   /* 去掉末尾的 \r\n */
            out->head = (char*)malloc(hl + 1);
            if (out->head)
            {
                memcpy(out->head, buf, hl);
                out->head[hl] = '\0';
                out->head_len = hl;
            }
        }

        /* status line */
        if (total >= 12 && strncasecmp(buf, "HTTP/", 5) == 0)
        {
            const char* sp = strchr(buf, ' ');
            if (sp)
            {
                out->status = atoi(sp + 1);
                const char* rs = strchr(sp + 1, ' ');
                if (rs)
                {
                    rs++;
                    const char* re = strstr(rs, "\r\n");
                    size_t rlen = re ? (size_t)(re - rs) : strlen(rs);
                    if (rlen >= sizeof(out->reason))
                        rlen = sizeof(out->reason) - 1;
                    memcpy(out->reason, rs, rlen);
                    out->reason[rlen] = '\0';
                }
            }
        }

        /* body */
        const char* body_src = buf + head_len;
        size_t body_src_len = (total > head_len) ? (total - head_len) : 0;

        int chunked = 0;
        if (out->head)
        {
            for (size_t k = 0; k + 18 < out->head_len; k++)
            {
                if (strncasecmp(out->head + k, "Transfer-Encoding:", 18) == 0)
                {
                    const char* line = out->head + k + 18;
                    const char* le = strstr(line, "\r\n");
                    size_t ll = le ? (size_t)(le - line) : strlen(line);
                    for (size_t j = 0; j + 7 <= ll; j++)
                    {
                        if (strncasecmp(line + j, "chunked", 7) == 0)
                        {
                            chunked = 1;
                            break;
                        }
                    }
                    break;
                }
            }
        }

        if (chunked)
        {
            out->body = dechunk(body_src, body_src_len, &out->body_len);
        }
        else
        {
            out->body = (char*)malloc(body_src_len + 1);
            if (out->body)
            {
                memcpy(out->body, body_src, body_src_len);
                out->body[body_src_len] = '\0';
                out->body_len = body_src_len;
            }
        }
        free(buf);
    }

    return 0;
}
