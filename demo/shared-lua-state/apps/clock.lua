-- clock app: 每秒更新一次时间
label = "clock: waiting..."
count = 0

platform.set_timer(1000, function()
    count = count + 1
    label = os.date("clock: %H:%M:%S") .. ("  #%d"):format(count)
    platform.log(label)
end)
