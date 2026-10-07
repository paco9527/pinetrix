Pitrix
=====================
这是一个基于树莓派3B + WS2812点阵屏方案实现的电子钟，可以编写Lua脚本获取所需信息并显示到点阵屏上，实现显示时间、联网获取天气等功能，而不需要连接到专门的服务端。

本仓库基于以下开源项目“焊接”而成：

- 点阵屏驱动依赖：
  - [jgarff/rpi_ws281x](https://github.com/jgarff/rpi_ws281x)
- 上层用户程序依赖：
  - [lua/lua](https://github.com/lua/lua)
  - [lvgl/lvgl](https://github.com/lvgl/lvgl)
    - [TomThumb字体](https://robey.lag.net/2010/01/23/tiny-monospace-font.html)
  - [XuNeo/luavgl](https://github.com/XuNeo/luavgl)
  - [mpx/lua-cjson](https://github.com/mpx/lua-cjson)

## 使用方法

### 1. 获取源码
```shell
git clone https://github.com/paco9527/pitrix2.git
git submodule update --init # 目前不需要添加参数--recursive，编译luavgl时会使用extlib/lvgl，不需要下载额外的依赖
```

### 2. 编译
使用 CMake 构建，编译产物为可执行文件`pitrix`，位于对应的 build 目录下（如`build/pitrix`）。

本机编译（例如直接在树莓派上编译）：
```shell
cmake -B build .
cmake --build build -j
```

交叉编译（aarch64，对应树莓派64位系统），需要先安装aarch64工具链：
```shell
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
cmake -B build-aarch64-linux-gnu -DCOMPILER_PREFIX=aarch64-linux-gnu- .
cmake --build build-aarch64-linux-gnu -j
```

指定`COMPILER_PREFIX`后会自动按交叉编译配置（工具链前缀`aarch64-linux-gnu-`对应编译器`aarch64-linux-gnu-gcc`/`g++`）。`core/extlib`下的第三方库（lua、lvgl、luavgl、lua-cjson、rpi_ws281x）作为构建目标会自动编译；本机与交叉编译请使用不同的 build 目录。

#### CMake 参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `COMPILER_PREFIX` | 空 | 交叉编译工具链前缀，如`aarch64-linux-gnu-`；为空时使用本机编译器 |
| `PTX_SCREEN` | `ws2812` | 屏幕后端：`ws2812`（真机点阵屏）或`dummy`（空实现，用于本机调试） |
| `PTX_PLATFORM` | `linux` | 平台后端：`linux`（GPIO chardev 等）或`dummy`（本机测试，GPIO 用空实现） |
| `MATRIX_WIDTH` | `32` | 点阵屏宽度 |
| `MATRIX_HEIGHT` | `8` | 点阵屏高度 |
| `GPIO_PIN` | `18` | WS2812数据引脚 |
| `WS2812_DMA` | `10` | WS2812 DMA通道 |
| `PTX_TICK_MS` | `10` | 主循环tick间隔(ms) |
| `PTX_LOOP_INTERVAL` | `100` | 循环间隔(ms) |
| `SETUP_SCRIPT_BUF_LEN` | `4096` | setup脚本缓冲区大小 |
| `MAX_EXPORT_CONTENT_LEN` | `1024` | 导出日志内容最大长度 |
| `CMAKE_BUILD_TYPE` | `Debug` | 编译类型 |

例如在本机以 dummy 屏幕/平台后端编译用于调试：
```shell
cmake -B build_dummy -DPTX_SCREEN=dummy -DPTX_PLATFORM=dummy .
cmake --build build_dummy -j
```

清理编译文件：
```shell
rm -rf build* core/extlib/output
```

### 3. 在树莓派上执行
编译成果物为可执行文件`pitrix`（位于对应的 build 目录下）；将`app`目录和`pitrix`文件拷贝到树莓派上，`app`目录和`pitrix`可执行文件在同一目录下，然后执行：
```shell
sudo ./pitrix
```

注意，rpi_ws281x库操作硬件需要使用root权限。执行命令后应当能看到`Enter Pitrix REPL.`的提示且没有异常退出，这时可以继续输入命令了。

### 4. 加载脚本
`app`目录下包含一些自测应用。每个应用是一个目录，入口脚本为 **`entry.lua`**，可选导出：

- `setup()`：加载时调用一次，返回表 `{ loop = 毫秒, background = 毫秒 }`，声明前台/后台节拍（字段可缺省，默认取 `PTX_LOOP_INTERVAL` / `PTX_BG_INTERVAL`）
- `loop()`：前台节拍，仅当该应用是当前前台时触发
- `background()`：后台节拍，不受前台影响，始终触发
- `on_show()` / `on_hide()`：切换到前台 / 离开前台时触发

以显示时间的应用为例，其脚本在`app/time`目录下，输入命令：
```shell
sys.load("app/time")
```
加载后，若这是第一个应用会自动成为前台并开始显示；`loop()` 每秒刷新一次时钟。

#### 应用管理

同一时刻只有一个应用显示在屏幕上（每个应用一个独立的 lvgl screen），其它应用保留但不可见。非前台应用的 `loop()` 暂停，`background()` 继续运行。

##### sys
`sys.load(path[, name]) -> id`

从指定目录加载应用，返回应用 id。
- path：应用脚本**所在目录**
- name：应用名，不填写时默认为 `"app"`
- 仅当当前没有前台应用时，新加载的应用才会自动成为前台

`sys.del(id)`

删除应用（同时清理其定时器与屏幕）。

`sys.ls() -> list`

打印并返回已加载应用列表（`{ {id=, name=, fg=}, ... }`），`fg` 标记当前前台：
```shell
sys.ls()
current loaded apps:
id: 0   name: time (fg), node: 0x75c066e0
id: 1   name: label, node: 0x75c14100
```

`sys.show(id | name [, opts])` / `sys.show()`

切换前台到指定应用（接受 id 或名字）；不带参数时恢复上次前台。

`opts`（可选）为切屏过渡参数，缺省不过渡：
- `anim`：`"none"`/`"over_left|right|top|bottom"`/`"move_left|right|top|bottom"`/`"fade_in|on|out"`/`"out_left|right|top|bottom"`
- `time`：动画时长(ms)；`delay`：延迟(ms)
- 仅作用于本次切屏（per-call），不是全局动画速度

`sys.hide()`

隐藏当前前台（黑屏），可用 `sys.show()` 恢复（瞬时，无过渡）。

`sys.current() -> id | nil`

返回当前前台应用 id；隐藏时为 `nil`。

`sys.self() -> id | nil`

返回当前正在执行的应用 id（应用脚本里即自身；REPL 下为 `nil`）。

`sys.next([opts])` / `sys.prev([opts])`

按加载顺序切换前台，返回新的前台 id。`opts` 同 `sys.show`。


`sys.log_lvl(lvl)`

设置C代码内的打印等级，只显示等级 **≥** 设定等级的消息；等级从小到大为 `DEBUG(0)` / `INFO(1)` / `ERROR(2)`，默认 `ERROR(2)`。例如 `sys.log_lvl(1)` 显示 INFO 及以上，`sys.log_lvl(0)` 显示全部（含 DEBUG）。

`sys.set_brightness(brightness)`

设置屏幕亮度，在rpi_ws281x驱动上支持的范围是0~255。

##### sys.key（按键）

按键事件以 `(id, ev)` 两个字符串回调，`id` 是按键逻辑名（见 `core/modules/key_handle/key_cfg.c`），`ev` 取 `"down"`（按下）/`"short"`（短按释放）/`"long"`（长按达到阈值）/`"repeat"`（长按保持）/`"up"`（长按后释放）。

`sys.key.on(cb[, opts]) -> handle`

注册按键回调，`cb(id, ev)` 返回真表示已消费（短路）。`opts.always = true` 表示该条目常驻（即使不是前台也接收，供启动器使用）。返回句柄。

`sys.key.off(handle)`

注销。

`sys.key.pressed(id) -> bool`

只读：该按键当前是否按下。

分发规则（安卓式）：保留键（配置里 `reserved=1`）只给 `always` 条目；普通键先给当前前台应用的条目，未被消费再给 `always` 条目。条目随所属应用 `sys.del` 自动清除。

`sys.key.send(id, ev)`

（测试用）直接注入一个按键事件，走与真实按键相同的分发路径；`ev` 同回调。可在本机 dummy 平台自测启动器。

`sys.key.list() -> {{id=, spec=, reserved=}, ...}`

枚举板级按键表（`core/modules/key_handle/key_cfg.c`）。

##### app（应用脚本内可用）
- `app.include(name)`：引入同目录下的 `<name>.lua`
- `app.log(msg)`：以 `[app:名字]` 前缀打印

##### net（应用脚本内可用）

`net.resolve(host) -> ip, err`

解析域名到 IPv4 字符串（内部 `getaddrinfo`）。

`net.request(url[, opts]) -> resp, err`

对某个 URL 发起请求并返回完整响应，`opts` 可选：
- `method`：默认 `"GET"`
- `headers`：追加请求头表，如 `{ ["X-Token"]="abc" }`
- `body`：请求体字符串
- `timeout`：超时毫秒，默认 5000
- `max_body`：响应体上限，默认 65536

`resp` 字段：`ok`（2xx 为真）、`status`、`reason`、`headers`（键小写的表）、`body`、`raw`（完整原始响应）。失败返回 `nil, err`。

注意：当前仅支持 `http://`（无 TLS），`https://` 会返回 `nil, "TLS not supported"`。

```lua
local r = net.request("http://api.seniverse.com/v3/weather/now.json?key=KEY&location=ip&language=en&unit=c")
if r and r.ok then
    local data = cjson.decode(r.body)
end
```

*自己焊接代码的水平有限，Lua用的也不算太熟，万一这堆代码真有人来看，不足之处欢迎路过的各位少侠指正*

