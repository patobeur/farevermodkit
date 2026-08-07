local running, boss_id, boss_class = false, "", ""
local started_ms, elapsed_ms, last_ms, best_ms = 0, 0, 0, 0
local total_ms, kills, wipes = 0, 0, 0
local history = {}
local global_history_loaded = false
local finished = false
local loaded_kind = ""
local boss_is_boss = false

local function parse_history(json_str)
    local h = {}
    if type(json_str) ~= "string" then return h end
    -- {"boss":"Mokshi", "class":"...", "ms":1234, "date":"07/08 23:05", "unix":123456}
    for boss, class, ms, date, unix in string.gmatch(json_str, '{"boss"%s*:%s*"([^"]+)"%s*,%s*"class"%s*:%s*"([^"]*)"%s*,%s*"ms"%s*:%s*(%d+)%s*,%s*"date"%s*:%s*"([^"]+)"%s*,%s*"unix"%s*:%s*(%d+)}') do
        table.insert(h, {boss = boss, class = class, ms = tonumber(ms), date = date, unix = tonumber(unix)})
    end
    -- Sort by unix ascending
    table.sort(h, function(a, b) return a.unix < b.unix end)
    return h
end

local function encode_history(h)
    local s = "[\n"
    for i, v in ipairs(h) do
        if i > 1 then s = s .. ",\n" end
        s = s .. '  {"boss":"' .. v.boss .. '", "class":"' .. v.class .. '", "ms":' .. tostring(v.ms) .. ', "date":"' .. v.date .. '", "unix":' .. tostring(v.unix) .. '}'
    end
    return s .. "\n]"
end

-- limits to top 10 per boss, and sorts overall by unix
local function enforce_limits(h)
    local by_boss = {}
    for _, v in ipairs(h) do
        by_boss[v.boss] = by_boss[v.boss] or {}
        table.insert(by_boss[v.boss], v)
    end
    
    local new_h = {}
    for boss, entries in pairs(by_boss) do
        table.sort(entries, function(a, b) return a.ms < b.ms end)
        for i = 1, math.min(10, #entries) do
            table.insert(new_h, entries[i])
        end
    end
    table.sort(new_h, function(a, b) return a.unix < b.unix end)
    return new_h
end

local function get_best_ms(h, boss)
    local b = 0
    for _, v in ipairs(h) do
        if v.boss == boss then
            if b == 0 or v.ms < b then b = v.ms end
        end
    end
    return b
end

local function label(b)
    if b.kind and b.kind ~= "" then return b.kind end
    if b.runtimeClass and b.runtimeClass ~= "" then return b.runtimeClass end
    return i18n("unknown_boss")
end

local function clock(ms)
    if not ms or ms < 0 then ms = 0 end
    local total = math.floor(ms / 1000)
    return string.format("%02d:%02d:%02d", math.floor(total / 3600),
                         math.floor((total % 3600) / 60), total % 60)
end

function on_init() running, finished = false, false end

local function render_bossrun()
    if not global_history_loaded then
        global_history_loaded = true
        if farever.bossrun_load then
            local json_str = farever.bossrun_load()
            history = parse_history(json_str)
        end
    end
    
    local b = farever.boss()
    
    if running and not b.tracked and not b.defeated then running, elapsed_ms = false, 0 end
    if not running and b and b.present and b.inCombat and b.tracked and not b.defeated then
        running, finished = true, false
        boss_id, boss_class = label(b), b.runtimeClass or ""
        boss_is_boss = b.isBoss and true or false
        started_ms, elapsed_ms = b.nowMs, 0
    end
    
    if running then
        elapsed_ms = b.nowMs - started_ms
        if b.defeated and (boss_class == "" or b.runtimeClass == boss_class) then
            running, finished = false, true
            last_ms, kills = elapsed_ms, kills + 1
            total_ms = total_ms + last_ms
            
            if boss_is_boss then
                local date_str, unix = "??/??", 0
                if farever.date_string then
                    date_str, unix = farever.date_string()
                end
                table.insert(history, {boss = boss_id, class = boss_class, ms = last_ms, date = date_str, unix = unix})
                history = enforce_limits(history)
                if farever.bossrun_save then
                    farever.bossrun_save(encode_history(history))
                end
            end
        end
    elseif not finished then
        elapsed_ms = 0
    end
    
    -- Print current boss data
    if b then
        local detected = (running or finished) and boss_id or i18n("unknown_boss")
        local detected_class = ""
        if b.present or b.defeated then
            detected, detected_class = label(b), b.runtimeClass or ""
        elseif running or finished then
            detected, detected_class = boss_id, boss_class
        end
        imgui.text("DETECTED|" .. detected .. "|" .. detected_class .. "|" .. (((b.present and b.isBoss) or ((not b.present) and boss_is_boss)) and "boss" or "mob"))
        
        local timer_state = running and "running" or (finished and "finished" or "idle")
        local shown_ms = running and elapsed_ms or (finished and last_ms or 0)
        imgui.text("TIMER|" .. timer_state .. "|" .. clock(shown_ms))
        if kills > 0 then
            imgui.text("STATS_TEXT|" .. i18n("last") .. " " .. clock(last_ms) .. "    " .. i18n("average") .. " " .. clock(total_ms / kills))
        end
        imgui.text("COUNTS_TEXT|" .. i18n("kills") .. " " .. kills .. "    " .. i18n("wipes") .. " " .. wipes)
    else
        imgui.text("COUNTS|" .. kills .. "|" .. wipes)
    end
    
    -- Always print the 10 most recent from global history
    if #history > 0 then
        local start_idx = math.max(1, #history - 9)
        for i = start_idx, #history do
            local entry = history[i]
            local boss_best = get_best_ms(history, entry.boss)
            local is_best = (entry.ms == boss_best) and "1" or "0"
            -- Send the date as part of the string (appended to the end)
            imgui.text("HISTORY|" .. entry.boss .. "|" .. entry.class .. "|" .. entry.ms .. "|" .. is_best .. "|" .. entry.date)
        end
    end
end

function on_render()
    local ok, err = pcall(render_bossrun)
    if not ok then imgui.text("ERROR|" .. tostring(err)) end
end

function on_shutdown()
    if running then wipes = wipes + 1 end
    running, finished = false, false
end