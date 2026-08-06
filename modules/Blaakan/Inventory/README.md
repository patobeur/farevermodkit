# Collection Atlas

Collection Atlas est le module **Blaakan** `blaakan.inventory`. Il reprend l’interface de collection de `farever-mods` dans une fenêtre native distincte de FareverModKit.

## Fonctionnement

Le module devient disponible lorsqu’un personnage est détecté dans le monde. Son icône `INV` ouvre ou ferme la fenêtre sans désactiver le module. La fenêtre est déplaçable, fermable et conserve les menus, sous-menus, filtres, icônes, états de possession et fiches d’information des objets.

Les données sont lues par le cœur FMK sur un worker à fréquence limitée. Le module ne lance pas de scan mémoire complet et ne charge pas les données de collection avant la détection du personnage.

## Ressources

Les grandes ressources partagées de l’Atlas sont dans `assets/atlas/` à la racine de FareverModKit. `icon.png` reste propre au module.

## Limites

Les fonctions Players, Loot, Report/HTML, Routes et certaines fonctions de progression doivent rester dans des modules séparés. Elles ne doivent pas être réintroduites dans Collection Atlas.