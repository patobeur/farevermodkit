# FareverModKit

FareverModKit est un socle de mods pour Farever. Il doit fonctionner sur une installation vierge du jeu et fournir une base commune aux plugins créés par différents auteurs.

## Décision principale

FareverModKit est **Lua-first** :

```text
dxgi.dll = cœur natif obligatoire
Lua      = plugins standards
DLL      = modules natifs avancés et optionnels
```

La DLL principale est nécessaire pour être chargée par Farever, intercepter DirectX, afficher l’overlay, lire la mémoire du jeu et fournir une API aux plugins. Les plugins standards ne doivent pas compiler de DLL.

## Structure cible

```text
farevermodkit\
├── core\
├── modules\
│   └── Auteur\
│       └── NomDuMod\
│           ├── manifest.json
│           ├── main.lua
│           └── languages\
│               ├── en-US.json
│               ├── fr-FR.json
│               ├── es-ES.json
│               └── ...
├── updates\
├── backups\
├── languages\
├── logs\
├── config\
└── README.md
```

Le dossier du projet ne contient pas la version dans son nom. Les versions sont gérées par `manifest.json`, Git et GitHub Releases.

## Premier objectif

Créer un cœur minimal qui :

1. fonctionne sur un jeu vierge de mod ;
2. détecte les plugins dans `modules` ;
3. charge un moteur Lua sandboxé ;
4. affiche une interface de gestion ;
5. charge `Patobeur.Bravo` ;
6. affiche « Bravo ! » dans le jeu ;
7. gère les langues `en-US`, `fr-FR` et `es-ES` ;
8. journalise les chargements et les erreurs.

## Manifest d’un plugin

```json
{
  "id": "patobeur.bravo",
  "author": "Blaakan",
  "name": "Bravo",
  "version": "1.0.0",
  "entry": "main.lua",
  "defaultLanguage": "en-US",
  "languages": ["en-US", "fr-FR", "es-ES"],
  "github": "Blaakan/farever-bravo"
}
```

## API Lua envisagée

```lua
function on_init() end
function on_render() end
function on_event(name, data) end
function on_settings() end
function on_shutdown() end
```

Aucune fonction n’est obligatoire. Le premier plugin de test pourra être :

```lua
function on_render()
    imgui.text("Bravo !")
end
```

## Sécurité

Un plugin Lua ne doit pas pouvoir :

- lire ou écrire n’importe quel fichier ;
- lancer un programme ;
- accéder au réseau ;
- inspecter un autre processus ;
- écrire dans la mémoire du jeu ;
- sortir de son dossier de stockage ;
- bloquer durablement le thread du jeu.

Les callbacks doivent être protégés par `pcall`. Une erreur de plugin doit être journalisée, signalée dans le gestionnaire et isolée des autres plugins. Les mises à jour seront téléchargées en HTTPS, vérifiées par hash, installées atomiquement et conservées avec une sauvegarde précédente.

Les DLL natives de modules sont considérées comme non sandboxables dans le processus du jeu. Elles seront optionnelles, explicitement confirmées et traitées comme du code de confiance.

## Compatibilité avec les mises à jour de Farever

Les offsets mémoire appartiennent exclusivement au cœur natif. Ils seront versionnés par hash du jeu et générés par les outils du projet. Les plugins Lua utiliseront uniquement l’API publique et ne connaîtront aucun offset.

Si le build du jeu n’est pas reconnu, FareverModKit doit refuser les lectures mémoire plutôt que de deviner.

## Identité compte/personnage

`st.Player.uid` est l’identifiant stable du compte. `st.Player.__uid` est un identifiant de session et ne doit pas être utilisé comme identifiant persistant de personnage. Tant qu’aucun UUID persistant de personnage n’est exposé par le jeu, l’identité de sauvegarde repose sur `UUID du compte + nom du personnage`.

## Journal de développement

### 2026-08-05 — décisions initiales

- Création du projet séparé `D:\farever-mods\farevermodkit`.
- Choix d’une architecture Lua-first.
- Le cœur natif centralisera la lecture mémoire et l’API.
- Les plugins standards seront des dossiers avec manifeste, Lua et langues.
- Les erreurs de modules et de compatibilité du jeu devront être documentées ici lorsqu’elles sont pertinentes pour la sécurité ou l’architecture.

## Règle de documentation

Toute nouvelle fonction importante, décision d’architecture, erreur de sécurité ou incompatibilité découverte pendant le développement doit être ajoutée à ce README au moment où elle est traitée.
## État du prototype

### Étape 1 — contrat Lua et premier plugin

- Arborescence du projet créée.
- Manifeste FareverModKit créé.
- Manifeste `Patobeur.Bravo` créé.
- Plugin Lua de démonstration créé avec les cinq callbacks prévus.
- Langues `en-US`, `fr-FR` et `es-ES` créées.
- Contrat initial documenté dans `docs/LUA_API.md`.
- Le moteur natif et l’interface de gestion restent à implémenter.
### Étape 2 — découverte sûre des manifestes

Le premier composant natif est `core/src/plugin_manager.*`. Il ne charge encore aucun code Lua : il découvre uniquement `modules/<Auteur>/<Mod>/manifest.json`, valide les champs, refuse les chemins dangereux et vérifie que le fichier d’entrée reste dans le dossier du module.

Cette séparation est volontaire : la découverte et la validation doivent être testables avant d’autoriser l’exécution de code.
### Erreur rencontrée pendant l’étape 2

Le premier build du gestionnaire de plugins a échoué car un chemin Windows (`std::filesystem::path`) utilisait un préfixe texte étroit `".."` avec `starts_with`. Le correctif utilise le préfixe large `L".."`. Le composant compile maintenant avec MSVC.

Cette erreur rappelle que les chemins Windows doivent rester en Unicode de bout en bout.
### Étape 3 — abstraction du moteur Lua

`core/src/lua_runtime.*` a d’abord chargé explicitement une DLL Lua et refusé l’exécution tant que le sandbox n’était pas installé. Cette étape historique est maintenant complétée par l’étape 4 ; le rendu ImGui et les catalogues restent à brancher.

Aucune DLL Lua n’a été trouvée localement dans les projets existants. Le moteur Lua devra donc être fourni comme dépendance versionnée, avec son hash et sa licence documentés. Il ne faut pas télécharger ou exécuter une DLL inconnue automatiquement.
### Erreur rencontrée pendant l’étape 3

MSVC refuse un seul `/Fo` lorsqu’une commande compile plusieurs fichiers source. Le script `core/build.cmd` compile maintenant chaque fichier séparément afin de produire `plugin_manager.obj` et `lua_runtime.obj` sans ambiguïté.
### Étape 4 — premier sandbox Lua

`core/src/lua_runtime.*` charge uniquement la DLL configurée, crée un état Lua privé, puis ouvre les bibliothèques standard avant de supprimer `os`, `io`, `debug`, `package`, `require`, `dofile`, `loadfile`, `load` et `collectgarbage`. Aucun script de module n’est exécuté avant cette étape.

Le runtime expose provisoirement deux fonctions sans pointeurs natifs : `imgui.text(...)` (encore sans rendu, en attente de la couche DXGI) et `i18n(key)` (retourne la clé tant que les catalogues ne sont pas branchés). Le fichier Lua est compilé puis appelé avec `lua_pcallk`, afin qu’une erreur reste confinée au module.

Le sandbox possède maintenant un hook de limite d’instructions, mais il n’est pas encore considéré comme complet pour une installation publique : le seuil doit être mesuré, et la DLL Lua devra être fournie sous `third_party` avec version, architecture, hash SHA-256 et licence vérifiés. Ne télécharge pas une DLL arbitraire et ne copie pas encore FareverModKit dans le jeu.

### Erreur rencontrée pendant l’étape 4

Les fonctions de l’API Lua chargées par `GetProcAddress` doivent garder leur signature exacte (`lua_tolstring` possède un troisième paramètre de longueur et `lua_newtable` correspond à `lua_createtable`). Une signature approximative peut compiler mais provoquer une corruption mémoire à l’exécution ; les typedefs ont donc été corrigés avant d’autoriser l’exécution.
Le runtime installe également un hook `LUA_MASKCOUNT` : le script est interrompu après environ 10 millions d’instructions VM. Ce seuil est une protection de prototype, pas une garantie temps réel ; il devra être mesuré et configuré avant la sortie publique.
### Étape 5 — intégrité de la DLL Lua

`LuaRuntime::load_engine` exige maintenant un SHA-256 de 64 caractères hexadécimaux. Le fichier est haché avant `LoadLibraryW`; une DLL absente, modifiée ou non documentée est refusée. La valeur attendue devra venir d’une configuration signée ou d’un manifeste de version, jamais d’une saisie récupérée sur le réseau au démarrage.
### Étape 6 — dépendance Lua testable

Les sources officielles Lua 5.4.8 sont conservées dans `third_party/lua/lua-5.4.8`. L’archive `lua-5.4.8.tar.gz` correspond au SHA-256 officiel `4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae`. Le script `third_party/lua/build.cmd` produit une DLL x64 reproductible dans `third_party/lua/bin/lua54.dll`, dont le hash enregistré est `cf364b9cd60e3fa342c441f8552ae287f23832a47ac324ec3ffdc24b6f00a380`.

La configuration active est `config/lua-runtime.json`. Le test hors jeu se compile et se lance ainsi :

```powershell
cd D:\farever-mods\farevermodkit
cmd /c core\build.cmd
cmd /c core\build_smoke.cmd
$cfg = Get-Content -Raw config\lua-runtime.json | ConvertFrom-Json
$dll = Join-Path (Get-Location) $cfg.dll
.\core\build\lua_smoke.exe $dll $cfg.sha256 (Join-Path (Get-Location) 'core\tests\lua_sandbox.lua')
```

Le test de boucle `core/tests/lua_budget.lua` doit échouer avec `Lua plugin instruction budget exceeded` ; cet échec est attendu et prouve que la protection fonctionne. Cette étape ne copie toujours rien dans Farever.
## Feuille de route actualisée

### Terminé

- [x] Arborescence FareverModKit et règles de modules `Auteur/Mod`.
- [x] Manifeste de FareverModKit et manifeste `Patobeur.Bravo`.
- [x] Fichiers de langue `en-US`, `fr-FR`, `es-ES`.
- [x] Découverte et validation sûre des manifestes.
- [x] Sources officielles Lua 5.4.8 vérifiées et DLL x64 reconstruite localement.
- [x] Vérification SHA-256 avant chargement de Lua.
- [x] Sandbox : retrait des accès système, appel protégé et budget d’instructions.
- [x] API minimale `imgui.text`, `i18n`, callback `on_render` et chargement de catalogue JSON.
- [x] Smoke tests hors jeu : sandbox, hash incorrect, boucle infinie et callback du module Bravo.
- [x] Documentation du projet, du plan et des erreurs rencontrées.

### En cours immédiat

- [x] Recompiler le smoke test après le chargement des langues et vérifier que `Bravo !` vient de `en-US.json`.
- [x] Ajouter un hôte de plugins qui relie `PluginManager` et `LuaRuntime` : `on_init`, `on_render` et `on_shutdown` sont opérationnels.
- [ ] Ajouter l’état activé/désactivé et la journalisation isolée par module.

### Avant le premier lancement dans Farever

- [x] Créer un cœur natif `dxgi.dll` de test sans remplacer l’installation existante.
- [x] Relier un renderer D3D12 de test au texte produit par `imgui.text`.
- [ ] Afficher la fenêtre gestionnaire : modules détectés, activation, désactivation, options et erreurs.
- [ ] Brancher les langues et le choix de locale dans cette interface.
- [ ] Ajouter la lecture mémoire publique du cœur avec profils d’offsets par hash du jeu.
- [ ] Refuser proprement un build Farever inconnu.
- [ ] Tester dans une copie propre du jeu avec sauvegarde et désinstallation réversible.

### Après le premier lancement

- [ ] Manifeste complet des modules : permissions, version d’API, dépendances et compatibilité.
- [ ] Mise à jour GitHub en HTTPS avec hash, sauvegarde, installation atomique et retour arrière.
- [ ] Configuration persistante, journaux lisibles et export de diagnostic.
- [ ] Confirmation avant suppression et restauration d’un module.
- [ ] Politique séparée pour les DLL natives optionnelles, non sandboxables.
- [ ] Tests CI, guide créateur de module, licence et procédure de release.
### Étape 7 — PluginHost et cycle de vie

`core/src/plugin_host.*` orchestre les modules découverts. Chaque module reçoit son propre `LuaRuntime`, son catalogue de langue et son état isolé. Le host appelle `on_init` au chargement, `on_render` pour le rendu, et `on_shutdown` à la désactivation ou à l’arrêt. Une erreur de rendu désactive le module concerné sans arrêter les autres.

Le smoke test `core/build_host_smoke.cmd` confirme le chargement de `Patobeur.Bravo`, la traduction `en-US`, le texte rendu, la désactivation/réactivation et l’arrêt du module.
### Étape 8 — événements Lua

`PluginHost::dispatch_event` diffuse maintenant un nom d’événement aux modules actifs. Le runtime appelle `on_event(name, nil)` sous protection et désactive le module en cas d’erreur. La table `data` structurée sera ajoutée après définition de l’API publique Farever.
### Étape 9 — proxy DXGI de test

`native/dxgi_proxy.cpp` transmet les exports DXGI au système, installe le renderer D3D12 et relie le `PluginHost`. `native/package-test.cmd` construit un paquet autonome avec Lua et `Patobeur.Bravo`. Le proxy affiche le texte du callback Lua dans le renderer.

Cette DLL n’est pas encore installée dans Farever. Elle doit être testée dans une copie propre, car l’installation existante de `farever-mods` possède déjà son propre `dxgi.dll`.Impossible de remplacer la variable Error, car elle est constante ou en lecture seule.
### Erreur rencontrée pendant l’étape 9

Le premier lien de la DLL native mélangeait `/MD` et `/MT` entre le proxy et les objets du PluginHost, ce qui provoquait `LNK2038 RuntimeLibrary` et des doublons de la bibliothèque standard. Tous les scripts utilisent maintenant explicitement `/MT`.
## Début de migration de farever-mods

Les dossiers Blaakan.Inventory, Blaakan.Progression, Blaakan.Map, Blaakan.Loot, Blaakan.Chat, Blaakan.Players et Blaakan.Report sont maintenant détectés mais désactivés par défaut (enabledByDefault: false). Ce sont des squelettes de migration ; ils n’accèdent pas encore à la mémoire. hl_reader, les offsets, les hooks et l’API de données doivent d’abord être centralisés dans le cœur FareverModKit.

## Migration du lecteur mémoire (2026-08-05)

La lecture mémoire de l’ancien `farever-mods` est maintenant portée dans
`core/src/memory`. Le cœur possède une façade `GameMemory` et centralise les
lectures HashLink de l’inventaire, de la progression, de la carte, du chat, du
roster, de la caméra et de la pose du héros.

La migration est volontairement en lecture seule : les fonctions qui écrivaient
directement des fichiers JSON n’ont pas été copiées dans cette couche. Les
plugins Lua ne reçoivent pas encore ces données ; l’API Lua sera ajoutée après
validation du contrat et des profils d’offsets.

Le scan n’est pas lancé pendant la découverte des modules. `GameMemory::probe`
doit être appelé après le délai de démarrage (20 secondes minimum pour les
hooks), et le cœur doit refuser les offsets dont le hash de build n’est pas
reconnu. Le test `core/build/memory_smoke.exe` valide les lectures protégées
hors jeu sans effectuer de scan.
## Étape suivante — API mémoire Lua

`LuaRuntime` expose désormais uniquement des copies contrôlées via :

- `farever.memory_status()` ;
- `farever.player()` ;
- `farever.inventory_summary()`.

Le module `Blaakan.Inventory` est le premier consommateur de cette API. Les
catalogues de langue sont isolés par runtime : un module ne peut plus écraser
la traduction d’un autre.

La lecture en jeu reste opt-in (`memoryEnabled: false` par défaut). Lorsque
l’option est activée, le host attend le délai de démarrage, calcule le hash réel
de `hlboot.dat`, le compare à `offsets.gen.h`, puis seulement autorise le
probe. Un build inconnu ne déclenche aucune lecture.