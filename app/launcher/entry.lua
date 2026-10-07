-- 启动器 / 全局按键路由 + 自动轮播
-- 无应用时显示 "NO APP" 占位; 有真实应用后由本应用主动让位(前台交给它),
-- 因此启动器永远不是前台页 —— 它只做全局按键路由 + 自动轮播。
-- 切换由启动器自己按 sys.ls() 列表完成(跳过自己), 不依赖 sys.next/prev。
-- 4 键: PREV / NEXT / OK / HOME
local BG_MS = 500
local AUTOSCROLL_MS = 5000
local NEXT_OPT = { anim = "move_top",    time = 300 }
local PREV_OPT = { anim = "move_bottom", time = 300 }

local self_id
local label
local auto = false
local ticks = 0

-- 除自己外的应用 id 列表(按加载顺序)
local function realApps()
    local ids = {}
    for _, k in ipairs(sys.ls()) do
        if k.id ~= self_id then ids[#ids + 1] = k.id end
    end
    return ids
end

local function showFirstApp()
    local ids = realApps()
    if #ids > 0 then
        sys.show(ids[1], NEXT_OPT)
    end
end

-- 相对当前前台, 切到上/下一个真实应用(环绕)
local function switchApp(dir)
    local ids = realApps()
    local n = #ids
    if n == 0 then return end

    local cur = sys.current()
    local idx = nil
    for i, id in ipairs(ids) do
        if id == cur then idx = i; break end
    end

    local ni
    if idx == nil then
        ni = (dir >= 0) and 1 or n
    else
        ni = (dir >= 0) and (idx % n) + 1 or ((idx - 2) % n) + 1
    end
    sys.show(ids[ni], dir >= 0 and NEXT_OPT or PREV_OPT)
end

local function on_key(id, ev)
    if id == "HOME" then
        if ev == "short" then
            auto = false
            showFirstApp()               -- 回第一个真实应用(无应用则留在占位)
            return true
        elseif ev == "long" then
            auto = false
            if sys.current() == nil then
                sys.show()               -- 亮屏(恢复上次前台)
            else
                sys.hide()               -- 息屏
            end
            return true
        end
    elseif id == "PREV" and ev == "short" then
        auto = false
        switchApp(-1)
        return true
    elseif id == "NEXT" and ev == "short" then
        auto = false
        switchApp(1)
        return true
    elseif id == "OK" then
        if ev == "long" then
            auto = not auto
            ticks = 0
            return true
        elseif ev == "short" then
            auto = false
            switchApp(1)
            return true
        end
    end
    return false
end

function setup()
    self_id = sys.self()
    sys.key.on(on_key, { always = true })

    label = lvgl.Label(nil, {
        x = 0, y = 1,
        text = "NO APP",
        text_color = "#fff",
    })

    return { loop = 200, background = BG_MS }
end

-- 仅在自己是前台时运行: 一旦有真实应用, 让位(瞬时)
function loop()
    if sys.current() ~= self_id then return end
    local ids = realApps()
    if #ids > 0 then
        sys.show(ids[1])
    end
end

function background()
    if auto then
        ticks = ticks + 1
        if ticks * BG_MS >= AUTOSCROLL_MS then
            ticks = 0
            switchApp(1)
        end
    end
end
