# Tâches et Améliorations Futures (TODO)

## Moteur (Core & Native)
- **[ ] Résoudre les avertissements de Shadowing (C4456)** : Dans `core/src/memory/hl_reader.cpp`, les variables `vf` (lignes 752 et 785) et `hero_data` (lignes 900 et 917) sont déclarées plusieurs fois dans des portées imbriquées. Renommer les variables locales pour supprimer les warnings à la compilation.
