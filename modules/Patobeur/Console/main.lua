local lines, sequence = {}, 0
local previous = nil
local previous_target, previous_combat, previous_defeated = "", false, false

local function emit(name, detail)
    sequence = sequence + 1
    local line = string.format("%04d  %s", sequence, name)
    if detail and detail ~= "" then line = line .. "  " .. detail end
    lines[#lines + 1] = line
    if #lines > 250 then table.remove(lines, 1) end
end

local function transition(old, new, field, up, down)
    if old[field] ~= new[field] then emit(new[field] and up or down) end
end

function on_init()
    lines, sequence, previous = {}, 0, nil
    previous_target, previous_combat, previous_defeated = "", false, false
    emit("module.initialized", "patobeur.console")
end

function on_render()
    local current = farever.memory_status()
    if current then
        if previous then
            transition(previous, current, "appFound", "memory.app_found", "memory.app_lost")
            transition(previous, current, "heroFound", "memory.hero_found", "memory.hero_lost")
            transition(previous, current, "loading", "memory.loading_started", "memory.loading_finished")
            transition(previous, current, "inWorld", "memory.world_entered", "memory.world_left")
        else
            emit("memory.status", string.format("app=%s hero=%s loading=%s world=%s",
                 tostring(current.appFound), tostring(current.heroFound),
                 tostring(current.loading), tostring(current.inWorld)))
        end
        previous = current

        if current.inWorld then
            local target = farever.boss()
            local kind = target and target.kind or ""
            local combat = target and target.inCombat or false
            if kind ~= previous_target then
                if kind == "" then emit("target.cleared")
                else emit("target.changed", kind .. " [" .. (target.runtimeClass or "") .. "]") end
                previous_target, previous_defeated = kind, false
            end
            if combat ~= previous_combat then
                emit(combat and "target.combat_started" or "target.combat_ended", kind)
                previous_combat = combat
            end
            local defeated = target and target.defeated or false
            if defeated and not previous_defeated then emit("target.defeated", kind) end
            previous_defeated = defeated
        elseif previous_target ~= "" then
            previous_target, previous_combat, previous_defeated = "", false, false
        end
    end

    for i = 1, #lines do imgui.text("LOG|" .. lines[i]) end
end

function on_shutdown()
    emit("module.shutdown", "patobeur.console")
end