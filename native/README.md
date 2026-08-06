# Cœur natif DXGI

Le dossier `native/` contient le proxy `dxgi.dll` x64 et l’interface DirectX 12 de FareverModKit.

## Responsabilités

- transmettre les exports DXGI au véritable `dxgi.dll` de Windows ;
- installer l’overlay D3D12 après le délai de démarrage ;
- afficher l’icône FMK, le gestionnaire et les fenêtres natives ;
- charger les textures communes et celles des modules ;
- transmettre les entrées souris sans réserver les touches de fonction du jeu ;
- relier le `PluginHost` au lecteur mémoire centralisé.

Des gardes empêchent la récursion dans `Present`, `ExecuteCommandLists` et `ResizeBuffers`. Le rendu et la lecture mémoire peuvent être désactivés dans `config/native.json` en cas de diagnostic.

## Compilation

```powershell
cmd /c core\build.cmd
cmd /c native\build.cmd
cmd /c native\package-test.cmd
```

`build/` et `test-package/` sont générés localement et exclus de Git.

## Installation de test

Farever doit être complètement fermé. Copier `native/test-package/dxgi.dll` et `native/test-package/farevermodkit/` à la racine du jeu, puis lancer `steam://rungameid/3672400`.

Ne jamais empiler ce proxy avec un autre `dxgi.dll`. Sauvegarder l’ancien fichier avant remplacement.

## Configuration fonctionnelle

La configuration de référence active le palier 4, attend au moins 20 secondes, active la lecture mémoire et interdit le scan global de secours :

```json
{
  "overlay": { "enabled": true, "stage": 4, "startupDelayMs": 20000 },
  "memoryEnabled": true,
  "memoryAllowScan": false
}
```

Le cœur recherche `App.inst` par la méthode validée issue de farever-mods. Les traitements coûteux commencent après détection d’un personnage dans le monde.