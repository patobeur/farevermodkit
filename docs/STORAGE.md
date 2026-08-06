# Stockage FareverModKit

Les fichiers installés avec FMK restent dans le dossier de Farever. Les réglages et données modifiables sont progressivement centralisés sous :

```text
%LOCALAPPDATA%\farevermodkit\
├── settings\
│   ├── ui-state.json             état, positions et activation des modules
│   ├── farever-modkit.ini        positions Atlas, Navigator et maîtrise
│   └── bossrun-tracked.txt       monstres ordinaires suivis par BossRun
├── data\
│   ├── navigation\
│   │   └── farever-nav-state.txt
│   └── accounts\
│       └── <compte>\
│           ├── farever-collection.json
│           └── characters\<personnage>\
│               ├── farever-inventory-*.json
│               ├── farever-jobs-*.json
│               └── bossrun\*.json
├── html\                       site local installé par Report
├── farever-report-data.js      agrégation multi-compte du rapport
└── logs\                       emplacement commun réservé aux journaux
```

## Migration

Lors du premier démarrage de cette version :

- `config/ui-state.json` est copié vers `settings/ui-state.json` si le nouveau fichier n’existe pas ;
- l’ancien `farever-modkit.ini` situé avec les ressources Atlas est copié vers `settings/` ;
- l’ancien état Navigator est copié vers `data/navigation/`.

Les anciens fichiers ne sont pas supprimés automatiquement. Ils servent de sauvegarde de secours et pourront être retirés plus tard après validation de la migration.

Les ressources statiques (`modules`, Lua, icônes FMK et base Atlas) restent dans le dossier du jeu : elles appartiennent à l’installation et non aux données personnelles.