local running, boss_id, boss_class = false, "", ""
local started_ms, elapsed_ms, last_ms, best_ms = 0, 0, 0, 0
local total_ms, kills, wipes = 0, 0, 0
local finished = false
local loaded_kind = ""
local boss_is_boss = false

local function label(b)
    if b.kind and b.kind ~= "" then return b.kind end
    if b.runtimeClass and b.runtimeClass ~= "" then return b.runtimeClass end
    return "aucun"
end

local function clock(ms)
    if not ms or ms < 0 then ms = 0 end
    local total = math.floor(ms / 1000)
    return string.format("%02d:%02d:%02d", math.floor(total / 3600),
                         math.floor((total % 3600) / 60), total % 60)
end

function on_init() running, finished = false, false end

local function render_bossrun()
    local b = farever.boss()
    if not b then
        local detected = (running or finished) and boss_id or "aucun"
        local state = running and "running" or (finished and "finished" or "idle")
        local shown = running and elapsed_ms or (finished and last_ms or 0)
        imgui.text("DETECTED|" .. detected .. "|" .. boss_class .. "|" ..
                   (boss_is_boss and "boss" or "mob"))
        imgui.text("TIMER|" .. state .. "|" .. clock(shown))
        if kills > 0 then
            imgui.text("STATS|" .. clock(last_ms) .. "|" .. clock(total_ms / kills))
        end
        imgui.text("COUNTS|" .. kills .. "|" .. wipes)
        return
    end

    if b.present and b.kind and b.kind ~= "" and loaded_kind ~= b.kind then
        loaded_kind = b.kind
        boss_id, boss_class = b.kind, b.runtimeClass or ""
        boss_is_boss = b.isBoss and true or false
        last_ms, best_ms, total_ms, kills, wipes = 0, 0, 0, 0, 0
        finished = false
        local saved = farever.bossrun_load(b.kind)
        if saved then
            last_ms, best_ms, total_ms = saved.lastMs or 0, saved.bestMs or 0, saved.totalMs or 0
            kills, wipes = saved.kills or 0, saved.wipes or 0
            finished = last_ms > 0
        end
    end
    if running and not b.tracked and not b.defeated then running, elapsed_ms = false, 0 end
    if not running and b.present and b.inCombat and b.tracked and not b.defeated then
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
            if best_ms == 0 or last_ms < best_ms then best_ms = last_ms end
            farever.bossrun_save(boss_id, boss_class, last_ms, best_ms,
                                    total_ms, kills, wipes)
        end
    elseif not finished then
        elapsed_ms = 0
    end

    local detected, detected_class = "aucun", ""
    if b.present or b.defeated then
        detected, detected_class = label(b), b.runtimeClass or ""
    elseif running or finished then
        detected, detected_class = boss_id, boss_class
    end
    imgui.text("DETECTED|" .. detected .. "|" .. detected_class .. "|" ..
               (((b.present and b.isBoss) or ((not b.present) and boss_is_boss)) and
                "boss" or "mob"))

    local timer_state = running and "running" or (finished and "finished" or "idle")
    local shown_ms = running and elapsed_ms or (finished and last_ms or 0)
    imgui.text("TIMER|" .. timer_state .. "|" .. clock(shown_ms))
    if kills > 0 then
        imgui.text("STATS|" .. clock(last_ms) .. "|" .. clock(total_ms / kills))
    end
    imgui.text("COUNTS|" .. kills .. "|" .. wipes)
end

function on_render()
    local ok, err = pcall(render_bossrun)
    if not ok then imgui.text("ERROR|" .. tostring(err)) end
end

function on_shutdown()
    if running then wipes = wipes + 1 end
    running, finished = false, false
end