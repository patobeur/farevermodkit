# Report

Module `patobeur.report` créé par **Patobeur** pour générer un rapport HTML local, classé par compte puis par personnage.

Cette adaptation reprend l’idée et la présentation du rapport du projet **farever-mods** de Blaakan. Merci à Blaakan et aux autres contributeurs cités dans les crédits du projet.

## Fonctionnement

- Le module est désactivé par défaut et doit être activé depuis la fenêtre FMK.
- Le module attend qu’un personnage soit réellement détecté dans le monde.
- Il appelle uniquement l’API publique `farever.report_generate()`.
- La demande est traitée sur le worker mémoire existant, jamais dans le rendu DX12.
- Les instantanés JSON sont écrits sous `%LOCALAPPDATA%\farevermodkit\data\accounts\<compte>\characters\<personnage>`.
- Le site est installé sous `%LOCALAPPDATA%\farevermodkit\html`.
- Les données agrégées sont écrites dans `%LOCALAPPDATA%\farevermodkit\farever-report-data.js`.

Visiter chaque personnage au moins une fois permet d’actualiser ses données. Le rapport n’invente aucune donnée absente.

Le rapport se consulte en ouvrant :

```text
%LOCALAPPDATA%\farevermodkit\html\farever-report.html
```

Les personnages sont regroupés par identifiant de compte. La collection commune est associée au compte correspondant, tandis que l’inventaire, les métiers, les runes et la maîtrise restent associés au personnage.

## Sécurité

Lua conserve sa sandbox : `io`, `os`, `loadfile`, `dofile` et `require` restent interdits. Le module ne reçoit aucun accès général au système de fichiers. L’écriture est effectuée par le service Report du cœur dans le seul espace FMK.

## Fichiers générés localement

`farever-atlas-icons.png` n’est pas distribué dans le dépôt public. Il est généré depuis une installation locale légitime de Farever par `node tools/gen-atlas.mjs --game <dossier-du-jeu>`, puis copié dans le site par le service Report lorsqu’il existe. Sans ce fichier, les données restent consultables mais les vignettes Atlas ne peuvent pas être affichées.
