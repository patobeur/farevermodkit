# Report

Module `patobeur.report` créé par **Patobeur** pour générer un rapport HTML local, classé par compte puis par personnage.

Cette adaptation reprend l’idée et la présentation du rapport du projet **farever-mods** de Blaakan. Merci à Blaakan et aux autres contributeurs cités dans les crédits du projet.

## Fonctionnement

- Le module attend qu’un personnage soit réellement détecté dans le monde.
- Il appelle uniquement l’API publique `farever.report_generate()`.
- La demande est traitée sur le worker mémoire existant, jamais dans le rendu DX12.
- Les instantanés JSON sont écrits sous `%LOCALAPPDATA%\farevermodkit\data\accounts\<compte>\characters\<personnage>`.
- Le site est installé sous `%LOCALAPPDATA%\farevermodkit\html`.
- Les données agrégées sont écrites dans `%LOCALAPPDATA%\farevermodkit\farever-report-data.js`.

Visiter chaque personnage au moins une fois permet d’actualiser ses données. Le rapport n’invente aucune donnée absente.

## Sécurité

Lua conserve sa sandbox : `io`, `os`, `loadfile`, `dofile` et `require` restent interdits. Le module ne reçoit aucun accès général au système de fichiers. L’écriture est effectuée par le service Report du cœur dans le seul espace FMK.

## Fichiers générés localement

`farever-atlas-icons.png` n’est pas distribué dans le dépôt public. Le service copie la version générée localement par l’outil Atlas lorsqu’elle existe.