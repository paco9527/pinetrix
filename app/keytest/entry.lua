-- 按键测试应用: 显示当前被按下的按键, 长按显示 "名字 LP"
--   无按键 -> 空
--   短按 HOME -> "HOME"; 长按 HOME -> "HOME LP"; 松开 -> 空
local label
local cur = ""

local function setText(t)
    if t ~= cur then
        cur = t
        label:set({ text = t })
    end
end

local function onKey(id, ev)
    if ev == "down" then
        setText(id)
    elseif ev == "long" or ev == "repeat" then
        setText(id .. " LP")
    elseif ev == "short" or ev == "up" then
        setText("")
    end
    return false
end

function setup()
    -- always: 连保留键(HOME)也要收到
    sys.key.on(onKey, { always = true })

    label = lvgl.Label(nil, {
        x = 0, y = 1,
        text = "",
        text_color = "#fff",
    })

    return { loop = 1000 }
end
