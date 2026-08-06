# Crédits et remerciements

FareverModKit est un projet communautaire qui s’appuie sur le travail et les recherches de plusieurs créateurs de mods Farever.

## Blaakan — farever-mods

Merci à **Blaakan** pour [`farever-mods`](https://github.com/Blaakan/farever-mods), qui constitue la base technique principale de plusieurs parties de FMK :

- méthode de découverte et de lecture mémoire HashLink en lecture seule ;
- Collection Atlas et son interface ;
- extraction des données et génération locale de l’Atlas ;
- recherches sur l’inventaire, les collections, la progression, la carte et les joueurs.

Les modules placés sous `modules/Blaakan/` restent attribués à Blaakan. Le générateur `tools/gen-atlas.mjs` et ses lecteurs sont adaptés de `farever-mods`.

## ramisotti13-eng — farever-minimap

Merci à **ramisotti13-eng** pour [`farever-minimap`](https://github.com/ramisotti13-eng/farever-minimap), notamment pour son travail sur l’overlay, l’API de plugins et le chronomètre de combats de boss. Le principe fonctionnel de BossRun a été étudié à partir de ce projet, puis adapté à l’architecture FMK.

## Brudr — Farever+ / FareverMeter

Merci à **Brudr**, auteur de Farever+ / FareverMeter, pour ses recherches documentées sur la mémoire de Farever, le HUD des boss, les états de combat et les changements de version du jeu. Ces observations ont servi de référence lors de la validation de BossRun et du lecteur centralisé.

## Lua

FareverModKit embarque les sources de **Lua 5.4.8**, développé par Lua.org à PUC-Rio. Sa licence est reproduite dans [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## Farever

Farever, ses noms, ses données et ses ressources appartiennent à **Shiro Games**. FareverModKit est un projet non officiel sans affiliation ni approbation de Shiro Games ou de Valve. Les atlas d’images du jeu ne sont pas redistribués dans ce dépôt : ils sont générés localement depuis l’installation du joueur.

Merci également aux joueurs et créateurs de modules qui testent FMK, signalent les erreurs et partagent leurs découvertes.