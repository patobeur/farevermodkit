# Console

Console est un module Patobeur qui affiche uniquement les transitions déjà observées par FareverModKit. Il ne fabrique pas de nom d'écran, de zone ou d'instance à partir d'une supposition.

## Événements actuellement disponibles

- `module.initialized` et `module.shutdown` : callbacks réels du cycle de vie du module ;
- `memory.status` : premier état brut observé ;
- `memory.app_found` / `memory.app_lost` : apparition ou disparition du `GameApp` ;
- `memory.hero_found` / `memory.hero_lost` : apparition ou disparition du héros local ;
- `memory.loading_started` / `memory.loading_finished` : transition réelle de `GameApp.loadingState` ;
- `memory.world_entered` / `memory.world_left` : transition du statut FMK `inWorld` ;
- `target.changed` / `target.cleared` : changement de l'identifiant interne de cible ;
- `target.combat_started` / `target.combat_ended` : changement de l'état de combat de la cible ;
- `target.defeated` : indicateur de mort observé sur la cible.

FMK ne distingue pas encore avec certitude l'écran titre de l'écran de sélection des personnages. Il ne publie donc pas d'événement nommé `character_selection`. De même, une fin de chargement n'est pas automatiquement appelée « entrée dans une instance » tant que le type de monde ou d'activité n'a pas été lu et validé.

## Fenêtre

La fenêtre est déplaçable. Elle conserve au maximum 250 lignes et en montre 17 à la fois. La molette fait défiler l'historique lorsque le pointeur se trouve au-dessus de la zone noire. Le compteur du bandeau indique la plage visible et le nombre total d'événements.

Les événements sont gardés en mémoire pendant la session. Une éventuelle exportation sur disque sera ajoutée séparément, sous `%LOCALAPPDATA%\farevermodkit`, afin de ne pas confondre une console de diagnostic avec les journaux natifs permanents.