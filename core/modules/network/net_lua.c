#include "net_lua.h"
#include "os_net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "log.h"

static int do_net_open(lua_State *L)
{
    int socket = ptx_os_net_open();
    if(socket < 0)
    {
        LOG_ERROR("open socket error");
        return 0;
    }
    else
    {
        lua_pushinteger(L, socket);
    }
    return 1;
}

static int do_net_close(lua_State *L)
{
    int socket = (int)lua_tointeger(L, 1);
    ptx_os_net_close(socket);
    return 0;
}

static int do_net_connect_ip(lua_State *L)
{
    int socket = -1;
    char* input = NULL;
    int ret = 0;
    uint16_t port = 0;

    if(lua_isinteger(L, 1))
    {
        socket = (int)lua_tointeger(L, 1);
    }
    else
    {
        LOG_ERROR("Invalid socket");
        return 0;
    }

    if(lua_isstring(L, 2))
    {
        input = (char*)lua_tostring(L, 2);
    }
    else
    {
        LOG_ERROR("Invalid address");
        return 0;
    }

    if(lua_isinteger(L, 3))
    {
        port = (uint16_t)lua_tointeger(L, 3);
    }
    else
    {
        port = 80;
    }

    ret = ptx_os_net_connect_ip(socket, input, port);
    if(ret)
    {
        LOG_ERROR("connect failed, ret %d, IP %s", ret, input);
        ptx_os_net_close(socket);
    }
    return 0;
}

static int do_net_read(lua_State *L)
{
    int socket = -1;
    int len = 0;
    int ret = 0;
    char* tmp = NULL;

    if(lua_isinteger(L, 1))
    {
        socket = (int)lua_tointeger(L, 1);
    }
    else
    {
        LOG_ERROR("Invalid socket");
        return 0;
    }
    
    if(lua_isinteger(L, 2))
    {
        len = (int)lua_tointeger(L, 2);
    }
    else
    {
        LOG_ERROR("Invalid read length");
        return 0;
    }
    
    tmp = malloc(sizeof(char)*len);
    memset(tmp, 0, sizeof(char)*len);

    ret = ptx_os_net_read(socket, tmp, len);
    if(ret >= 0)
    {
        lua_pushstring(L, tmp);
    }
    else
    {
        LOG_ERROR("read failed, socket %d", socket);
        lua_pushinteger(L, ret);
    }
    free(tmp);
    return 1;
}

static int do_net_timed_read(lua_State *L)
{
    int timeout = 0;
    int socket = (int)lua_tointeger(L, 1);
    int len = (int)lua_tointeger(L, 2);
    int ret = 0;

    if(lua_isinteger(L, 1))
    {
        socket = (int)lua_tointeger(L, 1);
    }
    else
    {
        LOG_ERROR("Invalid socket");
        return 0;
    }
    
    if(lua_isinteger(L, 2))
    {
        len = (int)lua_tointeger(L, 2);
    }
    else
    {
        LOG_ERROR("Invalid read length");
        return 0;
    }
    
    if(lua_isinteger(L, 3))
    {
        timeout = (int)lua_tointeger(L, 3);
    }
    else
    {
        timeout = 500;
    }

    char* tmp = malloc(sizeof(char)*len);
    memset(tmp, 0, sizeof(char)*len);

    ret = ptx_os_net_timed_read(socket, tmp, len, timeout);
    if(ret >= 0)
    {
        lua_pushstring(L, tmp);
    }
    else
    {
        LOG_ERROR("read failed, socket %d", socket);
        lua_pushinteger(L, ret);
    }
    free(tmp);
    return 1;
}

static int do_net_write(lua_State *L)
{
    int socket = -1;
    char* input = NULL;
    int len = 0;
    int ret = 0;

    if(lua_isinteger(L, 1))
    {
        socket = (int)lua_tointeger(L, 1);
    }
    else
    {
        LOG_ERROR("Invalid socket");
        return 0;
    }

    if(lua_isstring(L, 2))
    {
        input = (char*)lua_tostring(L, 2);
    }
    else
    {
        LOG_ERROR("Invalid write content address");
        return 0;
    }

    if(lua_isinteger(L, 3))
    {
        len = (int)lua_tointeger(L, 3);
    }
    else
    {
        len = strlen(input);
    }

    ret = ptx_os_net_write(socket, input, len);
    if(ret < 0)
    {
        LOG_ERROR("write failed, socket %d", socket);
    }
    lua_pushinteger(L, ret);
    return 1;
}

static int do_net_timed_write(lua_State *L)
{
    int len = 0;
    int timeout = 0;
    int socket = -1;
    char* input = NULL;
    int ret = 0;

    if(lua_isinteger(L, 1))
    {
        socket = (int)lua_tointeger(L, 1);
    }
    else
    {
        LOG_ERROR("Invalid socket");
        return 0;
    }

    if(lua_isstring(L, 2))
    {
        input = (char*)lua_tostring(L, 2);
    }
    else
    {
        LOG_ERROR("Invalid write content address");
        return 0;
    }

    if(lua_isinteger(L, 3))
    {
        len = (int)lua_tointeger(L, 3);
    }
    else
    {
        len = strlen(input);
    }

    if(lua_isinteger(L, 4))
    {
        timeout = (int)lua_tointeger(L, 4);
    }
    else
    {
        timeout = 500;
    }

    ret = ptx_os_net_timed_write(socket, input, len, timeout);
    if(ret < 0)
    {
        LOG_ERROR("write failed, socket %d", socket);
    }
    lua_pushinteger(L, ret);
    return 1;
}

static int do_net_resolve(lua_State *L)
{
    const char* host = luaL_checkstring(L, 1);
    char ip[64];
    if (ptx_os_net_resolve(host, ip, sizeof(ip)) != 0)
    {
        lua_pushnil(L);
        lua_pushstring(L, "resolve failed");
        return 2;
    }
    lua_pushstring(L, ip);
    return 1;
}

/* 把响应头部块解析成 Lua 表(键小写), 跳过状态行 */
static void push_headers(lua_State *L, const char* head, size_t head_len)
{
    lua_newtable(L);
    size_t i = 0;
    int first = 1;
    while (i < head_len)
    {
        const char* line = head + i;
        size_t j = i;
        while (j < head_len && head[j] != '\n')
            j++;
        size_t line_len = j - i;
        if (line_len && line[line_len - 1] == '\r')
            line_len--;

        if (first)
        {
            first = 0;              /* 状态行 */
        }
        else if (line_len)
        {
            const char* colon = (const char*)memchr(line, ':', line_len);
            if (colon)
            {
                size_t klen = (size_t)(colon - line);
                const char* v = colon + 1;
                size_t vlen = line_len - klen - 1;
                while (vlen && (*v == ' ' || *v == '\t')) { v++; vlen--; }
                while (vlen && (v[vlen-1] == ' ' || v[vlen-1] == '\t')) vlen--;

                char key[96];
                if (klen >= sizeof(key)) klen = sizeof(key) - 1;
                for (size_t k = 0; k < klen; k++)
                    key[k] = (char)tolower((unsigned char)line[k]);
                key[klen] = '\0';

                lua_pushlstring(L, v, vlen);
                lua_setfield(L, -2, key);
            }
        }

        i = (j < head_len) ? j + 1 : head_len;
    }
}

static int do_net_request(lua_State *L)
{
    const char* url = luaL_checkstring(L, 1);
    const char* method = "GET";
    const char* body = NULL;
    int timeout = 5000;
    int max_body = 65536;
    char* extra = NULL;

    if (lua_istable(L, 2))
    {
        lua_getfield(L, 2, "method");
        if (lua_isstring(L, -1)) method = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 2, "body");
        if (lua_isstring(L, -1)) body = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 2, "timeout");
        if (lua_isinteger(L, -1)) timeout = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 2, "max_body");
        if (lua_isinteger(L, -1)) max_body = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 2, "headers");
        if (lua_istable(L, -1))
        {
            int hidx = lua_gettop(L);
            size_t cap = 256, len = 0;
            extra = (char*)malloc(cap);
            if (extra) extra[0] = '\0';
            lua_pushnil(L);
            while (extra && lua_next(L, hidx))
            {
                const char* k = lua_tostring(L, -2);
                const char* v = lua_tostring(L, -1);
                if (k && v)
                {
                    size_t need = len + strlen(k) + strlen(v) + 8;
                    if (need > cap)
                    {
                        cap = need * 2;
                        extra = (char*)realloc(extra, cap);
                    }
                    if (extra)
                        len += (size_t)snprintf(extra + len, cap - len,
                                                "%s: %s\r\n", k, v);
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }

    ptx_os_net_response_t resp;
    char err[128] = {0};
    int rc = ptx_os_net_http_request(url, method, extra, body, timeout, max_body,
                              &resp, err, sizeof(err));
    if (extra)
        free(extra);

    if (rc != 0)
    {
        lua_pushnil(L);
        lua_pushstring(L, err[0] ? err : "request failed");
        return 2;
    }

    lua_newtable(L);
    lua_pushboolean(L, resp.status >= 200 && resp.status < 300);
    lua_setfield(L, -2, "ok");
    lua_pushinteger(L, resp.status);
    lua_setfield(L, -2, "status");
    lua_pushstring(L, resp.reason);
    lua_setfield(L, -2, "reason");
    if (resp.raw)
    {
        lua_pushlstring(L, resp.raw, resp.raw_len);
        lua_setfield(L, -2, "raw");
    }
    if (resp.body)
    {
        lua_pushlstring(L, resp.body, resp.body_len);
        lua_setfield(L, -2, "body");
    }
    push_headers(L, resp.head ? resp.head : "", resp.head_len);
    lua_setfield(L, -2, "headers");

    free(resp.raw);
    free(resp.head);
    free(resp.body);
    return 1;
}

static const luaL_Reg net[10] = {
    {"open", do_net_open},
    {"close", do_net_close},
    {"connect_ip", do_net_connect_ip},
    {"read", do_net_read},
    {"timed_read", do_net_timed_read},
    {"write", do_net_write},
    {"timed_write", do_net_timed_write},
    {"resolve", do_net_resolve},
    {"request", do_net_request},
    {NULL, NULL},
};

int ptx_net_lua_open(lua_State* state)
{
    luaL_newlib(state, net);
    return 1;
}
