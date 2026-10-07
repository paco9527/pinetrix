# 单 lua_State + _ENV 隔离 + Timer 事件驱动

Pitrix 重构方向 demo：所有 app 共用 1 个 lua_State，通过 _ENV 隔离变量，通过 timer 回调驱动更新。

## 文件

| 文件 | 说明 |
|------|------|
| `main.c` | 入口：单 lua_State + _ENV 隔离 + timer 事件循环 |
| `Makefile` | 构建（linux / mingw / arm cross） |
| `apps/clock.lua` | app A：每秒更新时间 |
| `apps/counter.lua` | app B：每 500ms 递增计数 |

## 关键代码位置

- 单 lua_State 创建: `main.c:154`
- _ENV 隔离流程: `main.c:119-140`
- Timer 注册 `platform.set_timer()`: `main.c:70-83`
- 事件循环（替代 script_runner_thread）: `main.c:162-204`
- `platform.dump()` 验证隔离: `main.c:97-118`

## 运行

```bash
make
./demo
```

Enter → dump 查看两 app 的 label 值不同
q → 退出

## 交叉编译（参考）

```bash
make CROSS=1 ARM_LUA_DIR=/path/to/arm/lua
```
