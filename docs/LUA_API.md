# API Lua FareverModKit v1

Chaque callback est facultatif :

```lua
function on_init() end
function on_render() end
function on_event(name, data) end
function on_settings() end
function on_shutdown() end
```

Une erreur est confinée au module concerné. Le runtime retire `os`, `io`, `debug`, `package`, `require`, `dofile`, `loadfile`, `load` et `collectgarbage`, puis applique un budget d’instructions.

## Interface et traduction

```lua
imgui.text(value)
i18n(key)
```

`imgui.text` publie une ligne rendue par l’hôte. Certains modules natifs utilisent des préfixes privés (`LOG|`, `TIMER|`, etc.) ; ils ne constituent pas encore une API UI générique.

## Données en lecture seule

```lua
local status = farever.memory_status()
-- appFound, heroFound, loading, inWorld,
-- buildValidated, probeCount, available

local player = farever.player()
-- name, characterUuid, accountId, class, activeWeapon,
-- level, experience, bankSlots ; nil si indisponible

local counts = farever.inventory_summary()
-- bank, bankEquipment, equipped, bags, bankSlots

local target = farever.boss()
-- valid, present, inCombat, defeated, tracked, isBoss,
-- kind, runtimeClass, health, nowMs
```

Les tables sont des copies contrôlées. `farever.boss()` représente le boss du HUD ou la cible courante reconnue par le cœur ; malgré son nom historique, une cible ordinaire peut donc être retournée avec `isBoss = false`.

## Stockage BossRun

```lua
local stats = farever.bossrun_load(kind)
farever.bossrun_save(kind, runtimeClass, lastMs, bestMs,
                     totalMs, kills, wipes)
```

Cette API spécialisée écrit sous `%LOCALAPPDATA%\farevermodkit`, par compte puis personnage. Elle sera généralisée plus tard en API de stockage propre à chaque module.

## Manifestes

Les champs actuellement utilisés sont `id`, `author`, `name`, `version`, `apiVersion`, `entry`, `defaultLanguage`, `languages`, `enabledByDefault` et `requiresGameWorld`.
## Export Report

`farever.report_generate()` demande au worker mémoire de sauvegarder les données du personnage courant et de régénérer le rapport HTML local. La fonction renvoie `true` lorsque la demande a été acceptée. Lua ne reçoit aucun accès au système de fichiers.

## Carte et display list

`farever.map_data()` renvoie `nil` hors du monde ou un instantané contenant `player`, éventuellement `camera`, et le tableau `entities`. Chaque entité expose uniquement des valeurs copiées : `x`, `y`, `z`, `kind`, `runtimeClass`, `isPlayer` et `isBoss`. Aucun pointeur HashLink n’est transmis à Lua.

Les commandes suivantes sont relatives à la zone de contenu de la fenêtre du module et sont exécutées plus tard par le renderer :

```lua
ui.draw_circle(x, y, radius, r, g, b, a, thickness, filled)
ui.draw_line(x1, y1, x2, y2, r, g, b, a, thickness)
ui.draw_rect(x, y, width, height, r, g, b, a, filled, thickness)
ui.draw_image(asset, x, y, width, height, r, g, b, a, u0, v0, u1, v1)
```

Les couleurs utilisent des composantes de `0` à `1`. Les rayons et épaisseurs sont bornés par le cœur, et chaque runtime est limité à 2 048 commandes par image.
Les coordonnées sont relatives à la zone de contenu de la fenêtre du module. La fonction ui.canvas(width, height) fixe un canevas logique mis à l’échelle lors du redimensionnement. FMK applique un scissor DX12 à la zone de contenu.

ui.draw_image accepte uniquement un chemin PNG relatif au dossier assets/ du module (64 images préchargées au maximum). Lua ne peut pas ouvrir un chemin arbitraire.