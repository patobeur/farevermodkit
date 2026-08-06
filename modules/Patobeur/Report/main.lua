local exported_character = ""
local status = "En attente d'un personnage..."

function on_init()
    exported_character = ""
    status = "En attente des donnees du personnage..."
end

function on_render()
    local player = farever.player()
    if player and player.characterUuid and player.characterUuid ~= "" then
        if exported_character ~= player.characterUuid then
            if farever.report_generate() then
                exported_character = player.characterUuid
                status = "Export demande pour " .. (player.name or player.characterUuid)
            else
                status = "Export indisponible"
            end
        end
    end
    imgui.text(status)
    imgui.text("Dossier : %LOCALAPPDATA%\\farevermodkit\\html")
end

function on_settings()
    imgui.text("Le rapport est actualise automatiquement pour chaque personnage detecte.")
end

function on_shutdown() end