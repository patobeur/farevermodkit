# BossRun

BossRun (`patobeur.bossrun`) est un module créé par **Patobeur** pour chronométrer les combats contre les boss sans dépendre de la langue affichée par Farever.

## Utilisation

Le module nécessite un personnage présent dans le monde (`requiresGameWorld: true`). Lorsqu’il est actif, sa fenêtre est ouverte par défaut et peut être fermée ou rouverte avec son icône.

La fenêtre affiche :

- le nom interne détecté ;
- un grand chronomètre blanc au repos, orange pendant le combat et jaune après une victoire ;
- le dernier temps et la moyenne lorsqu’une victoire est enregistrée ;
- les nombres de victoires et d’échecs.

Un véritable boss est suivi automatiquement et ne peut pas être désactivé. Une cible ordinaire apparaît en vert avec un interrupteur permettant de choisir si elle doit aussi être chronométrée.

## Détection

BossRun ne compare jamais le nom traduit affiché à l’écran. Le cœur lit les identifiants internes `kind` et la classe HashLink, par exemple `ent.boss.MunsterChuck`.

Pour les boss, le lecteur suit le HUD réel :

```text
GameApp.gui
  -> GameUI.gameRoot
  -> GameUiRoot.hud
  -> Hud.bossesInfo
  -> BossesInfo.bossInfos[]
  -> BossInfo.unit
```

Pour une cible ordinaire, le cœur lit la cible verrouillée ou automatique du héros, puis la résout dans les unités du monde. Les données sont lues en lecture seule et mises en cache à fréquence limitée.

## Chronomètre

Une tentative commence lorsque la cible suivie est présente, en combat et non vaincue. Elle se termine lorsque le cœur confirme sa défaite. Une simple disparition temporaire de la barre ne doit pas suffire, car certains boss changent de phase ou créent des copies.

Si le module est arrêté pendant une tentative, cette tentative est comptée comme un échec dans la session. La validation des autres causes d’échec et des changements de phase reste en cours.

## Sauvegarde

Après une victoire, les statistiques sont écrites dans un JSON propre au compte, au personnage et au `kind` interne :

```text
%LOCALAPPDATA%\farevermodkit\data\accounts\<account-id>\characters\<character-uuid>\bossrun\<boss-kind>.json
```

Le fichier contient l’identité du compte et du personnage, le dernier temps, le meilleur temps, le temps total, les victoires, les échecs et la date de mise à jour. L’écriture passe par un fichier temporaire remplacé atomiquement.

Cette organisation est prévue pour être consommée plus tard par le module Personnages et son affichage HTML, classé d’abord par compte puis par personnage.

## Limites connues

- la distinction instance/monde ouvert n’est pas encore exposée de manière fiable ;
- la sélection persistante des monstres ordinaires n’est pas encore stockée sur disque ;
- les traductions concernent l’interface, jamais la détection.