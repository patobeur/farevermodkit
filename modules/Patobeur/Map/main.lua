local zoom = 1.0
local show_chests = true
local show_orbs = true

local function map_asset(world)
    if not world then return nil end
    local w = string.lower(world)
    if string.find(w, "w1_siagarta", 1, true) or string.find(w, "siagarta", 1, true) then
        return "maps/w1_siagarta.png"
    end
    return nil
end

function on_render()
    local data = farever.map_data()
    if not data or not data.player then
        imgui.text("MAP_STATUS|En attente du monde...")
        return
    end

    ui.canvas(470, 396)

    local asset = map_asset(data.world)
    if not asset then
        imgui.text("MAP_STATUS|Monde non pris en charge: " .. tostring(data.world))
        return
    end
    
    local px = data.player.x
    local py = data.player.y

    if px and py then
        -- 1. Calcul des coordonnées du joueur sur la carte 2816x2816
        local map_x = (px + 2304.0) * 0.4444444444444444
        local map_y = (py + 3456.0) * 0.4444444444444444

        -- 2. Calcul de la zone visible (Vue)
        local view_w = 470.0 / zoom
        local view_h = 396.0 / zoom
        
        local src_x = map_x - (view_w * 0.5)
        local src_y = map_y - (view_h * 0.5)

        -- 3. Conversion en UV normalisés [0, 1] pour ui.draw_image
        local u0 = src_x / 2816.0
        local v0 = src_y / 2816.0
        local u1 = (src_x + view_w) / 2816.0
        local v1 = (src_y + view_h) / 2816.0

        -- 4. Dessiner le fond (cadré sur la zone visible grâce aux UV)
        ui.draw_image(asset, 0, 0, 470, 396, 1, 1, 1, 1, u0, v0, u1, v1)

        -- 5. Dessiner les entités projetées
        for _, entity in ipairs(data.entities or {}) do
            local x = entity.x
            local y = entity.y
            if x and y then
                local draw_it = false
                local r, g, b = 1.0, 1.0, 1.0
                
                if entity.isPlayer then 
                    draw_it = true
                    r, g, b = 0.2, 0.6, 1.0 
                elseif entity.isBoss then 
                    draw_it = true
                    r, g, b = 1.0, 0.1, 0.1 
                elseif show_chests and entity.kind == "chest" then
                    draw_it = true
                    r, g, b = 1.0, 0.8, 0.0
                elseif show_orbs and entity.kind == "orb" then
                    draw_it = true
                    r, g, b = 0.6, 0.0, 1.0
                elseif entity.kind == "npc" or entity.kind == "enemy" then
                    draw_it = true
                    r, g, b = 1.0, 0.55, 0.1
                end

                if draw_it then
                    local ent_map_x = (x + 2304.0) * 0.4444444444444444
                    local ent_map_y = (y + 3456.0) * 0.4444444444444444
                    
                    local screen_x = (ent_map_x - src_x) * zoom
                    local screen_y = (ent_map_y - src_y) * zoom

                    if screen_x >= 0 and screen_x <= 470 and screen_y >= 0 and screen_y <= 396 then
                        ui.draw_circle(screen_x, screen_y, 4, r, g, b, 1.0, 1.0, true)
                    end
                end
            end
        end

        -- 6. Dessiner le joueur local (toujours au centre)
        ui.draw_circle(235, 198, 6, 0.2, 1.0, 0.4, 1.0, 1.0, true)
        
        -- 7. Interface et statut
        local world_name = data.world or "Inconnu"
        if world_name == "w1_siagarta" then world_name = "Siagarta" end
        imgui.text("MAP_STATUS|" .. world_name .. "|" .. string.format("%.2f", zoom))
        
        -- Boutons interactifs factices pour l'instant (gérés par on_event de la DLL)
        -- Les icônes de la carte sont envoyées via event click
    end
end

function on_event(name)
    if name == "map_zoom_in" then
        zoom = math.min(4.0, zoom + 0.25)
    elseif name == "map_zoom_out" then
        zoom = math.max(0.5, zoom - 0.25)
    elseif name == "map_chests" then
        show_chests = not show_chests
    elseif name == "map_orbs" then
        show_orbs = not show_orbs
    end
end

function on_settings()
end

function on_shutdown()
end