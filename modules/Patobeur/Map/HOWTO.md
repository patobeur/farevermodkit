# HOWTO — API FMK pour Map

Ce fichier est le contrat entre le cœur C++ de FareverModKit et le module patobeur.map. Il doit être mis à jour à chaque modification de cette API. Le module est responsable de la présentation et de la logique de main.lua.

## Cycle des données

La DLL lit les données sur le worker mémoire, jamais dans le rendu DX12. Les entités sont rafraîchies au maximum quatre fois par seconde (250 ms), dans un rayon natif maximal de 250 mètres. Une copie immuable, sans pointeur, est transmise à Lua.

farever.map_data() renvoie nil hors du monde ou une table :

    {
      available = true,
      world = "w1_siagarta", -- niveau world.World.level
      player = { x, y, z, rotation },
      camera = { x, y, z, targetX, targetY, targetZ }, -- peut être absente
      entities = {
        { x, y, z, kind, runtimeClass, isPlayer, isBoss }
      }
    }

La liste est bornée à 512 entités spatiales validées de type ent.*. Les coordonnées non finies et les objets hors rayon sont rejetés.
Important : les sous-tables player, camera et chaque entité utilisent des champs nommés. Ce ne sont pas des tableaux numériques. Il faut écrire :

    local px = data.player.x
    local py = data.player.y
    local first = data.entities[1]
    local ex = first.x
    local boss = first.isBoss

Il ne faut pas écrire data.player[1], first[1] ou first[7] : ces valeurs seront nil et le module n’enverra alors aucun dessin.

## Canevas et redimensionnement

Déclarer une taille logique à chaque rendu avant de dessiner :

    ui.canvas(470, 396)

Toutes les coordonnées suivantes sont exprimées dans ce canevas. FMK les adapte à la taille réelle de la zone de contenu. Un rectangle de découpe DX12 strict empêche les cercles, traits, rectangles et images de dépasser dans la barre de titre ou hors de la fenêtre.

## Display list

Ces fonctions ajoutent des commandes que la DLL exécutera plus tard :

    ui.draw_circle(x, y, rayon, r, g, b, a, epaisseur, rempli)
    ui.draw_line(x1, y1, x2, y2, r, g, b, a, epaisseur)
    ui.draw_rect(x, y, largeur, hauteur, r, g, b, a, rempli, epaisseur)
    ui.draw_image(asset, x, y, largeur, hauteur, r, g, b, a, u0, v0, u1, v1)

Les couleurs sont comprises entre 0 et 1. La liste est limitée à 2 048 commandes par module et par image.

**⚠️ Précautions critiques sur les vertex et coordonnées :**
1. **Coordonnées `NaN` ou Infinies** : Les variables issues de calculs mathématiques (`math.cos`, division par zéro, etc.) peuvent produire des `NaN`. Si ces valeurs sont passées à `ui.draw_circle`, le moteur natif DX12 peut crasher (Access Violation). Il faut toujours valider les coordonnées.
2. **Limite des sommets (kMaxVerts)** : La géométrie est dessinée en temps réel. Un cercle avec l'argument `filled = true` utilise 144 sommets (48 triangles). Le moteur DirectX est plafonné à `96 * 1024` sommets (environ 98 000). Il faut impérativement filtrer (via les UV ou des conditions) les entités invisibles pour ne pas saturer le buffer géométrique du plugin hôte.
asset est un chemin relatif utilisant / sous le dossier assets/ du module :

    ui.draw_image("maps/w1_siagarta.png", 0, 0, 470, 396, 1, 1, 1, 1)
Exemple minimal de rendu avec fond et position du joueur :

    function on_render()
        local data = farever.map_data()
        if not data or not data.player then return end
        ui.canvas(470, 396)
        ui.draw_image("maps/w1_siagarta.png", 0, 0, 470, 396, 1, 1, 1, 1)
        ui.draw_circle(235, 198, 5, 0.2, 1, 0.4, 1, 1, true)
    end

Le fond n’est pas ajouté automatiquement par FMK : le module doit appeler ui.draw_image lui-même, puis dessiner ses marqueurs par-dessus.
Les quatre paramètres UV u0, v0, u1, v1 sont optionnels. Ils permettent de recadrer la carte sans agrandir l’image logique :

    ui.draw_image("maps/w1_siagarta.png", 0, 0, 470, 396,
                  1, 1, 1, 1, u0, v0, u1, v1)

FMK rend Map dans un canevas physique fixe 470×396 : redimensionner la fenêtre ne change plus l’échelle de ses commandes. Le module doit donc modifier les UV pour centrer la vue sur le joueur ; le redimensionnement de la fenêtre ne doit pas être utilisé comme zoom.

Lua ne reçoit aucun accès au système de fichiers. FMK précharge uniquement les PNG présents sous modules/Patobeur/Map/assets/, avec une limite de 64 images par module.

## Génération correcte des fonds du jeu

Attention : les fichiers UI/Window/Map/*.png de res.pak sont des vignettes de zone, pas la carte du monde. Ils ne doivent pas servir de fond avec une projection de coordonnées.

La vraie carte est composée de tuiles dans res.map.pak :

    Level/World/W1_Siagarta.dat/minimap/<tx>_<ty>_1024.png

Le générateur FMK correct est :

    python tools/gen-real-map-assets.py --game "D:\...\steamapps\common\Farever" --world W1_Siagarta

Il assemble les tuiles dans :

    modules/Patobeur/Map/assets/maps/w1_siagarta.png
    modules/Patobeur/Map/assets/maps/w1_siagarta.json

Le fichier JSON contient la transformation vérifiée :

    image_x = (world_x - origin_x) * px_per_unit
    image_y = (world_y - origin_y) * px_per_unit

Pour W1_Siagarta à l’échelle 0.25 :

    origin_x = -2304
    origin_y = -3456
    px_per_unit = 0.444444...
    y_down = true
    units_per_tile = 576
    tile_px = 256

Le module doit choisir l’asset à partir de data.world. Il ne faut pas choisir une carte au hasard ni utiliser Primevalley, Azuram ou une vignette UI pour représenter tout le monde.

Les cartes générées et leurs JSON sont ignorés par Git : chaque installation doit les produire localement depuis son propre res.map.pak.
## Vérification avant lancement

Le développeur doit lancer :

    node tools/check-map-module.mjs

Le vérificateur refuse le module si le Lua n’appelle pas ui.draw_image, s’il utilise des indices numériques pour player/camera/entity, s’il n’utilise pas map.world ou si le fond local w1_siagarta manque. Il s’agit d’un contrôle statique ; il ne remplace pas un test en jeu.
## Limite volontaire actuelle

FMK fournit les images et les positions réelles, mais ne prétend pas encore connaître les bornes monde→texture de chaque région. Ne pas inventer ces bornes. Le module peut afficher un radar local immédiatement ; l’alignement exact sur un fond doit attendre une transformation validée.

## Historique du contrat

- 2026-08-07 : farever.map_data(), entités worker, display list bornée.
- 2026-08-07 : ajout de ui.canvas, découpage DX12 strict et mise à l’échelle.
- 2026-08-07 : ajout de ui.draw_image limité aux PNG du dossier assets/.
- 2026-08-07 : ajout de tools/gen-real-map-assets.py basé sur res.map.pak et la transformation des tuiles.