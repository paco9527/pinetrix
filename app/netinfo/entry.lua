get_msg = 'GET /api/v1/data?'..
        'chart=net.eth0&'..
        'points=1&'..
        'after=-2&'..
        'units=kilobits&'..
        'options=seconds HTTP/1.1\r\n'..
        'Connection: keep-alive\r\n'..
        'Host: 192.168.31.247:19999\r\n\r\n'

function setup()
    send_label = lvgl.Label(nil, {
        x = 1, y = 1,
        text = "NA",
        text_color = "#100",
    })
    recv_label = lvgl.Label(nil, {
        x = 18, y = 1,
        text = "NA",
        text_color = "#010",
    })

    return { loop = 2000 }
end

function loop()
    local sock = net.open()
    if not sock then return end
    net.connect_ip(sock, "192.168.31.247", 19999)
    local ret = net.timed_write(sock, get_msg)
    if ret >= 0 then
        local recv = net.timed_read(sock, 1024)
        if type(recv) == "string" then
            local idx = string.find(recv, '\r\n\r\n')
            local json_str = string.sub(recv, idx + 4)
            local decoded = cjson.decode(json_str)
            local send_kps = math.floor(decoded["data"][1][3])
            if send_kps < 0 then send_kps = -send_kps end
            if send_kps < 1000 then
                send_label:set({ text = string.format("%dK", tonumber(string.sub(tostring(send_kps), 1, 2))) })
            else
                send_label:set({ text = string.format("%dM", tonumber(string.sub(tostring(send_kps / 1000), 1, 2))) })
            end
            local recv_kps = math.floor(decoded["data"][1][2])
            if recv_kps < 1000 then
                recv_label:set({ text = string.format("%dK", tonumber(string.sub(tostring(recv_kps), 1, 2))) })
            else
                recv_label:set({ text = string.format("%dM", tonumber(string.sub(tostring(recv_kps / 1000), 1, 2))) })
            end
        end
    end
    net.close(sock)
end
