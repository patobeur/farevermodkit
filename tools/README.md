# Outils locaux

## Générer les ressources de Collection Atlas

Les atlas d’images de Farever ne sont pas distribués sur GitHub. Le joueur les génère depuis sa propre installation du jeu avec Node.js 18 ou plus récent :

```powershell
node tools\gen-atlas.mjs --game "D:\SteamLibrary\steamapps\common\Farever"
```

Sans `--game`, le script cherche l’installation dans les bibliothèques Steam. Il lit localement `res.light.pak`, `res.pak`, `res.map.pak` et `res.levels.pak`, puis produit :

```text
tools/out/atlas/farever-atlas.tsv
tools/out/atlas/farever-atlas-icons.dds
```

Les deux fichiers sont également copiés dans `assets/atlas/`. Le DDS et le PNG éventuel sont ignorés par Git. Le TSV reste versionné car il contient la structure et les métadonnées utilisées par l’interface.

Pour copier aussi les fichiers générés dans l’installation active de FMK :

```powershell
node tools\gen-atlas.mjs --install --game "D:\SteamLibrary\steamapps\common\Farever"
```

Le script ne télécharge aucune ressource du jeu et ne modifie pas ses archives. Il ne fait que les lire.