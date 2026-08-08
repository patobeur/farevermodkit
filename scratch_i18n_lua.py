import os

path = r'd:\farever-mods\farevermodkit\modules\Patobeur\Report\main.lua'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

new_lua = '''local exported_character = ""
local status = "En attente d'un personnage..."

local locale = "en-US"
local f = io.open("options.ini", "r")
if f then
    local content = f:read("*a")
    f:close()
    if string.find(string.lower(content), '"language"%s*:%s*"fr"') then locale = "fr-FR" end
    if string.find(string.lower(content), '"language"%s*:%s*"es"') then locale = "es-ES" end
end

local function t(fr, en, es)
    if locale == "fr-FR" then return fr end
    if locale == "es-ES" then return es end
    return en
end

function on_init()
    exported_character = ""
    status = t("En attente des donnees du personnage...", "Waiting for character data...", "Esperando datos del personaje...")
end

function on_render()
    local player = farever.player()
    if player and player.characterUuid and player.characterUuid ~= "" then
        if exported_character ~= player.characterUuid then
            if farever.report_generate() then
                exported_character = player.characterUuid
                status = t("Export demande pour ", "Export requested for ", "Exportacion solicitada para ") .. (player.name or player.characterUuid)
            else
                status = t("Export indisponible", "Export unavailable", "Exportacion no disponible")
            end
        end
    else
        status = t("En attente d'un personnage...", "Waiting for a character...", "Esperando un personaje...")
    end
    imgui.text("REPORT_STATUS|" .. status)
end

function on_settings()
    imgui.text(t("Le rapport est actualise automatiquement pour chaque personnage detecte.", "The report is automatically updated for each detected character.", "El reporte se actualiza automaticamente para cada personaje detectado."))
end

function on_shutdown() end
'''

with open(path, 'w', encoding='utf-8') as f:
    f.write(new_lua)
