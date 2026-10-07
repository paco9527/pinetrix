-- counter app: 每 500ms 递增计数
label = "counter: waiting..."
num  = 0

platform.set_timer(500, function()
    num = num + 1
    label = "counter: " .. num
    platform.log(label)
end)
