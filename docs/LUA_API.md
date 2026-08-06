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
