# FareverModKit

FareverModKit (FMK) est un socle de mods Lua pour **Farever** sous Windows. Son cœur natif fournit l’overlay DirectX 12, une lecture mémoire centralisée et validée par version du jeu, ainsi qu’un hôte Lua isolant les modules les uns des autres.

> Projet communautaire non officiel, sans affiliation avec les développeurs de Farever ou Valve.

## État actuel

Le prototype est fonctionnel sur le build Farever pris en charge. Il propose :

- une icône FMK toujours visible et une fenêtre de gestion déplaçable ;
- activation et désactivation persistantes des modules ;
- icônes et fenêtres de modules déplaçables ;
- Collection Atlas de Blaakan ;
- BossRun et Console de Patobeur ;
- sandbox Lua avec budget d’instructions ;
- lecture mémoire en worker, sans scan global de secours ;
- refus des lectures si le hash du build n’est pas reconnu.

Le projet reste en développement. Plusieurs modules Blaakan visibles dans la liste sont uniquement des squelettes de migration et sont clairement signalés dans leur README.

## Structure

```text
assets/                 ressources communes de FMK et de l’Atlas
config/                 configuration native et runtime Lua
core/                   hôte Lua et lecture mémoire
modules/<Auteur>/<Mod>/ modules Lua et ressources propres au module
native/                 proxy DXGI et interface D3D12
third_party/lua/        sources et build reproductible de Lua 5.4.8
docs/                   API, historique et archives techniques
```

Un module contient au minimum `manifest.json` et `main.lua`. Il peut aussi fournir `icon.png`, `README.md`, `languages/` et `assets/`.

## Compilation

Prérequis : Windows x64 et Visual Studio avec les outils C++ MSVC.

```powershell
cd D:\farever-mods\farevermodkit
cmd /c third_party\lua\build.cmd
node tools\gen-atlas.mjs --game "D:\SteamLibrary\steamapps\common\Farever"
cmd /c core\build.cmd
cmd /c native\build.cmd
cmd /c native\package-test.cmd
```

Le paquet installable est généré dans `native/test-package/`. Ce dossier, comme les dossiers `build/`, est généré localement et n’est pas versionné.

## Installation manuelle

1. Fermer complètement Farever.
2. Construire `native/test-package`.
3. Copier son `dxgi.dll` et son dossier `farevermodkit` à la racine du jeu.
4. Lancer exclusivement le jeu avec Steam (`steam://rungameid/3672400`).

Ne lancez pas directement `Farever.exe`. Sauvegardez tout ancien `dxgi.dll` avant installation : un seul proxy DXGI peut être actif.

## Données et configuration

Le code et les ressources sont installés dans le dossier du jeu. Les données persistantes de modules, notamment BossRun, sont enregistrées sous :

```text
%LOCALAPPDATA%\farevermodkit\data\accounts\...
```

L’état de l’interface FMK est encore conservé par l’installation actuelle du jeu. Sa migration complète vers `%LOCALAPPDATA%\farevermodkit` reste prévue.

## Sécurité

Les modules Lua n’ont pas accès aux bibliothèques système dangereuses (`os`, `io`, `debug`, `package`, etc.). Ils reçoivent des copies de données et aucun pointeur mémoire. Une erreur Lua désactive le module concerné sans arrêter les autres.

Les offsets mémoire appartiennent au cœur. Un build inconnu doit être refusé plutôt que scanné ou deviné.

## Documentation

- [API Lua](docs/LUA_API.md)
- [Cœur natif](native/README.md)
- [Lecture mémoire](core/src/memory/README.md)
- [Historique de développement](docs/DEVELOPMENT_HISTORY.md)
- [Crédits et remerciements](CREDITS.md)
- chaque module possède son propre `README.md`.

## Licence et publication

FareverModKit est distribué sous licence MIT. Lua 5.4.8 conserve sa propre licence, reproduite dans `THIRD_PARTY_NOTICES.md`. Les atlas d’images extraits de Farever ne sont pas distribués dans ce dépôt et doivent être générés localement depuis une installation légitime du jeu.