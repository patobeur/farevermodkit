# Idées d'amélioration (Map)

Ces fonctionnalités pourraient être implémentées dans une future version du module Map :

1. **Indicateur de direction (Boussole/Rotation) :**
   Le joueur est actuellement un simple cercle. En utilisant `data.player.rotation` on peut dessiner une ligne (`ui.draw_line`) ou un indicateur pour montrer **vers où le personnage regarde**.

2. **Support de la Caméra Libre :**
   Actuellement, la carte se centre sur `data.player.x / y`. Si la caméra est déliée du joueur, utiliser plutôt `data.camera.targetX` et `data.camera.targetY` pour centrer la carte sur la zone observée.

3. **Ciblage et Poursuite (Smooth Tracking) :**
   Ajouter une interpolation pour que le zoom et les déplacements soient fluides au lieu d'un saut brutal.

4. **Différenciation visuelle affinée :**
   - Dessiner les boss (`isBoss = true`) avec un cercle légèrement plus grand et une pulsation (en utilisant un calcul basé sur le temps).
   - Dessiner un contour (`filled = false`) sur certains éléments moins importants pour aérer la carte.
