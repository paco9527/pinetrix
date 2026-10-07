#ifndef _PTX_OS_NET_H
#define _PTX_OS_NET_H

#include <stdint.h>
#include <stddef.h>

int ptx_os_net_open(void);
int ptx_os_net_connect_ip(int fd, char* addr, uint16_t port);
int ptx_os_net_read(int fd, char* buf, int buf_size);
int ptx_os_net_timed_read(int socket, uint8_t* buf, size_t size, int timeout);
int ptx_os_net_write(int fd, char* buf, int buf_size);
int ptx_os_net_timed_write(int socket, uint8_t* buf, size_t size, int timeout);
int ptx_os_net_close(int fd);

/* 域名解析: 解析 host 的首个 IPv4 到 ip_out (字符串, 需 >= INET_ADDRSTRLEN) */
int ptx_os_net_resolve(const char* host, char* ip_out, size_t ip_out_len);

/* HTTP 响应 */
typedef struct {
    int    status;
    char   reason[64];
    char*  raw;        /* 完整原始响应 (malloc) */
    size_t raw_len;
    char*  head;       /* 头部块(含状态行, 不含结尾空行) (malloc) */
    size_t head_len;
    char*  body;       /* 去分块后的 body (malloc) */
    size_t body_len;
} ptx_os_net_response_t;

/*
 * 一站式 HTTP 请求:
 *   url           : "http://host[:port]/path?query" (https 返回错误)
 *   method        : NULL 视为 "GET"
 *   extra_headers : 追加头, 形如 "K1: V1\r\nK2: V2\r\n" (可 NULL)
 *   body          : 请求体 (可 NULL)
 *   timeout_ms    : connect/read/write 超时
 *   max_body      : body 上限
 * 成功(拿到响应, 任意状态码)返回 0 并填充 out; 传输失败返回 -1 并写 err。
 * out->raw/head/body 由调用者 free。
 */
int ptx_os_net_http_request(const char* url,
                     const char* method,
                     const char* extra_headers,
                     const char* body,
                     int timeout_ms,
                     int max_body,
                     ptx_os_net_response_t* out,
                     char* err, size_t err_len);

#endif
