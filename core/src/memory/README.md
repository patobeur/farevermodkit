# Lecture mémoire en lecture seule

Cette couche, issue de la méthode éprouvée de `farever-mods`, appartient exclusivement au cœur FMK.

- `hl_runtime.*` sécurise les lectures avec `VirtualQuery` et SEH ;
- `hl_scan.cpp` contient les mécanismes de découverte HashLink, désactivés dans la configuration publique ;
- `offsets.gen.h` décrit le build Farever reconnu ;
- `hl_reader.*` produit des snapshots métier ;
- `game_memory.*` expose la façade utilisée par l’hôte ;
- `memory_log.*` route les diagnostics vers le journal natif.

Aucune fonction n’écrit dans la mémoire du jeu. Avant toute lecture, le SHA-256 de `hlboot.dat` doit correspondre au profil compilé. Un build inconnu est refusé.

Le démarrage utilise la découverte légère de `App.inst`, espacée dans le temps. Les lectures de collection ne commencent qu’après détection d’un personnage dans le monde et sont effectuées sur un worker à fréquence limitée afin de ne pas bloquer le rendu.

Après une mise à jour de Farever, les offsets et le hash doivent être régénérés et validés avant réactivation.