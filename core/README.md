# Cœur FareverModKit

Le cœur natif découvre et valide les manifestes, charge Lua 5.4, isole les runtimes, applique le budget d’instructions et centralise toutes les lectures mémoire de Farever.

Les modules Lua utilisent uniquement l’API documentée dans [`docs/LUA_API.md`](../docs/LUA_API.md). Ils ne reçoivent ni pointeur, ni offset, ni accès direct à la mémoire du processus.

## Compilation et tests

```powershell
cmd /c core\build.cmd
cmd /c core\build_smoke.cmd
cmd /c core\build_host_smoke.cmd
```

Les sorties sont produites dans `core/build/` et ne sont pas versionnées.