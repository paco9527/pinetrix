local URL = "http://api.seniverse.com/v3/weather/now.json?" ..
            "key=YOUR_SECRET_KEY&location=ip&language=en&unit=c"

local icon_table = {}

local function refresh()
    local resp = net.request(URL, { timeout = 5000 })
    if not resp or not resp.ok then
        return
    end

    local result = cjson.decode(resp.body)
    if type(result) ~= "table" or type(result.results) ~= "table"
        or not result.results[1] then
        return
    end

    local now = result.results[1].now
    if type(now) ~= "table" then
        return
    end

    local hour = tonumber(os.date("%H"))
    local weather_text = now.text

    label:set({ text = tostring(now.temperature) })

    if weather_text == "Fair"
        or weather_text == "Mostly cloudy"
        or weather_text == "Partly cloudy" then
        if hour > 18 or hour < 6 then
            icon:set({ src = icon_table[weather_text .. "night"] })
        else
            icon:set({ src = icon_table[weather_text] })
        end
    elseif icon_table[weather_text] then
        icon:set({ src = icon_table[weather_text] })
    end
end

function setup()
    label = lvgl.Label(nil, {
        x = 20, y = 1,
        text = "NA",
        text_color = "#133",
    })

    icon_table["Sunny"] = "A:" .. APPROOT .. "/icons/sunny.png"
    icon_table["Clear"] = "A:" .. APPROOT .. "/icons/clear.png"
    icon_table["Fair"] = icon_table["Sunny"]
    icon_table["Fair" .. "night"] = icon_table["Clear"]
    icon_table["Cloudy"] = "A:" .. APPROOT .. "/icons/cloudy.png"
    icon_table["Mostly cloudy"] = "A:" .. APPROOT .. "/icons/mostly-cloudy.png"
    icon_table["Mostly cloudy" .. "night"] = "A:" .. APPROOT .. "/icons/mostly-cloudy-night.png"
    icon_table["Partly cloudy"] = icon_table["Mostly cloudy"]
    icon_table["Partly cloudy" .. "night"] = icon_table["Mostly cloudy" .. "night"]
    icon_table["Overcast"] = "A:" .. APPROOT .. "/icons/overcast.png"
    icon_table["Shower"] = "A:" .. APPROOT .. "/icons/shower.png"
    icon_table["Light rain"] = "A:" .. APPROOT .. "/icons/light-rain.png"
    icon_table["Moderate rain"] = "A:" .. APPROOT .. "/icons/moderate-rain.png"
    icon_table["Heavy rain"] = "A:" .. APPROOT .. "/icons/heavy-rain.png"
    icon_table["Storm"] = "A:" .. APPROOT .. "/icons/storm.png"
    icon_table["Heavy storm"] = icon_table["Storm"]
    icon_table["Severe storm"] = icon_table["Storm"]
    icon_table["Ice rain"] = "A:" .. APPROOT .. "/icons/ice-rain.png"
    icon_table["Sleet"] = "A:" .. APPROOT .. "/icons/sleet.png"
    icon_table["Light snow"] = "A:" .. APPROOT .. "/icons/light-snow.png"
    icon_table["Moderate snow"] = "A:" .. APPROOT .. "/icons/moderate-snow.png"
    icon_table["Heavy snow"] = "A:" .. APPROOT .. "/icons/heavy-snow.png"
    icon_table["Snowstorm"] = "A:" .. APPROOT .. "/icons/snowstorm.png"
    icon_table["Foggy"] = "A:" .. APPROOT .. "/icons/foggy.png"
    icon_table["Haze"] = "A:" .. APPROOT .. "/icons/haze.png"

    icon = lvgl.Image(nil, {
        src = icon_table["Sunny"],
        x = 2,
    })

    refresh()   -- 首次立即拉取

    return { loop = 1800000 }
end

function loop()
    refresh()
end
