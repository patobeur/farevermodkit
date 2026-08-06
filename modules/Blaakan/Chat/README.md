# Chat

Module **Blaakan** ($id) prévu pour la migration du chat de farever-mods.

## État

Ce module est actuellement un **squelette de migration** : FMK le détecte et l’affiche, mais sa fonctionnalité métier n’est pas encore connectée. Il est désactivé par défaut. L’activer ne doit pas être interprété comme une fonction terminée.

## Fichiers

- `manifest.json` : identité et configuration du module ;
- `main.lua` : callbacks Lua minimaux ;
- `languages/` : textes traduits disponibles ;
- `icon.png` : icône du module lorsqu’elle est fournie.

La future implémentation doit consommer uniquement l’API publique de FareverModKit et ne pas lire directement la mémoire du jeu.
