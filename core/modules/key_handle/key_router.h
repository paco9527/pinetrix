#ifndef _PTX_KEY_ROUTER_H
#define _PTX_KEY_ROUTER_H

/*
 * 按键路由层: 注册表 + 安卓式分发。
 *   保留键 -> 只给 always 条目; 普通键 -> 前台条目优先, 未消费再给 always。
 * 分层: 设备层 ptx_key.c(扫描/事件) / 路由层 key_router.c(本文件) / Lua 绑定 key_lua.c
 */

void ptx_key_router_init(void);

/* 注册一个按键回调(已 luaL_ref 的 ref); 返回句柄(>0), 失败 <0 */
int  ptx_key_router_add(int app_id, int ref, int always);
/* 注销句柄; 返回 1 成功, 0 未找到 */
int  ptx_key_router_remove(int handle);
/* 某 app 被删除时清掉它的所有条目 */
void ptx_key_router_unregister_app(int app_id);

/* 注入/转发一个事件到分发路径 */
void ptx_key_router_dispatch(const char* id, int ev);

#endif /* _PTX_KEY_ROUTER_H */
