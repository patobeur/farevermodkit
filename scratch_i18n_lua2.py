import os

path = r'd:\farever-mods\farevermodkit\modules\Patobeur\Report\main.lua'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

new_lua = '''local exported_character = ""
local status = "btn_wait_char"

function on_init()
    exported_character = ""
    status = "btn_wait_data"
end

function on_render()
    local player = farever.player()
    local can_save = "0"
    local displayed_status = i18n(status)
    if player and player.characterUuid and player.characterUuid ~= "" then
        if exported_character ~= player.characterUuid then
            if farever.report_generate() then
                exported_character = player.characterUuid
                displayed_status = i18n("btn_export_req") .. " " .. (player.name or player.characterUuid)
                can_save = "1"
            else
                displayed_status = i18n("btn_export_unav")
            end
        end
    else
        displayed_status = i18n("btn_wait_char")
    end
    imgui.text("REPORT_STATUS|" .. displayed_status .. "|" .. i18n("btn_save") .. "|" .. i18n("btn_open") .. "|" .. i18n("btn_last_none") .. "|" .. i18n("btn_last_now") .. "|" .. i18n("btn_last_ago") .. "|" .. i18n("btn_sec") .. "|" .. can_save)
end

function on_settings()
    imgui.text(i18n("settings_desc"))
end

function on_shutdown() end
'''

with open(path, 'w', encoding='utf-8') as f:
    f.write(new_lua)
