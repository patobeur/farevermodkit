# Dépendance Lua

FareverModKit utilise **Lua 5.4.8 x64**.

- source officielle : `https://www.lua.org/ftp/lua-5.4.8.tar.gz` ;
- SHA-256 de l’archive : `4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae` ;
- build : `build.cmd`, MSVC x64, `/MT` et `/Brepro` ;
- sortie locale : `bin/lua54.dll` ;
- SHA-256 attendu de la DLL : `cf364b9cd60e3fa342c441f8552ae287f23832a47ac324ec3ffdc24b6f00a380`.

Les sources et l’archive officielle sont conservées pour permettre une reconstruction vérifiable. La DLL générée dans `bin/` est exclue de Git et doit être reconstruite avant de produire un paquet.

Le runtime calcule son SHA-256 avant `LoadLibraryW` et refuse tout fichier absent ou différent. La licence de Lua est reproduite dans `THIRD_PARTY_NOTICES.md` à la racine du dépôt et dans la documentation officielle vendue avec les sources.