function setup()
    label = lvgl.Label(nil, {
        x = 0, y = 1,
        text = "init",
        text_color = "#133",
    })
    return { loop = 1000 }
end

function loop()
    label:set({ text = os.date("%H:%M:%S") })
end
