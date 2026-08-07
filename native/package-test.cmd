@echo off
setlocal
cd /d "%~dp0"
set "STAGE=%~dp0test-package"
if not exist "%STAGE%" mkdir "%STAGE%"
if exist "%STAGE%\farevermodkit\config" rmdir /s /q "%STAGE%\farevermodkit\config"
if exist "%STAGE%\farevermodkit\modules" rmdir /s /q "%STAGE%\farevermodkit\modules"
if exist "%STAGE%\farevermodkit\third_party" rmdir /s /q "%STAGE%\farevermodkit\third_party"
if exist "%STAGE%\farevermodkit\assets" rmdir /s /q "%STAGE%\farevermodkit\assets"
if exist "%STAGE%\farevermodkit\tools" rmdir /s /q "%STAGE%\farevermodkit\tools"
if not exist "%STAGE%\farevermodkit\third_party\lua\bin" mkdir "%STAGE%\farevermodkit\third_party\lua\bin"
if not exist "%STAGE%\farevermodkit\config" mkdir "%STAGE%\farevermodkit\config"
if not exist "%STAGE%\farevermodkit\logs" mkdir "%STAGE%\farevermodkit\logs"
if not exist "%STAGE%\farevermodkit\assets" mkdir "%STAGE%\farevermodkit\assets"
if not exist "%STAGE%\farevermodkit\tools" mkdir "%STAGE%\farevermodkit\tools"
if not exist "%STAGE%\farevermodkit\tools" mkdir "%STAGE%\farevermodkit\tools"
if not exist "%STAGE%\farevermodkit\logs" mkdir "%STAGE%\farevermodkit\logs"
if not exist "%STAGE%\farevermodkit\assets" mkdir "%STAGE%\farevermodkit\assets"
copy /y build\dxgi.dll "%STAGE%\dxgi.dll" >nul
xcopy /e /i /y ..\modules "%STAGE%\farevermodkit\modules" >nul
copy /y ..\third_party\lua\bin\lua54.dll "%STAGE%\farevermodkit\third_party\lua\bin\lua54.dll" >nul
copy /y ..\config\lua-runtime.json "%STAGE%\farevermodkit\config\lua-runtime.json" >nul
copy /y ..\config\native.json "%STAGE%\farevermodkit\config\native.json" >nul
xcopy /e /i /y ..\assets "%STAGE%\farevermodkit\assets" >nul
xcopy /e /i /y ..\tools "%STAGE%\farevermodkit\tools" >nul

echo @echo off > "%STAGE%\generer-atlas.cmd"
echo echo Generation des ressources de l'Atlas Farever... >> "%STAGE%\generer-atlas.cmd"
echo node farevermodkit\tools\gen-atlas.mjs --install >> "%STAGE%\generer-atlas.cmd"
echo pause >> "%STAGE%\generer-atlas.cmd"

echo # FareverModKit > "%STAGE%\LISEZ-MOI.md"
echo Pour installer, extrayez tout le contenu de l'archive dans le dossier du jeu. >> "%STAGE%\LISEZ-MOI.md"
echo Lancez "generer-atlas.cmd" pour generer les images manquantes de l'Atlas. >> "%STAGE%\LISEZ-MOI.md"

cd "%STAGE%"
powershell -NoProfile -Command "Get-ChildItem -Recurse -File -Exclude 'SHA256SUMS.txt' | ForEach-Object { $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(); $relPath = $_.FullName.Substring((Get-Location).Path.Length + 1).Replace('\', '/'); $hash + '  ' + $relPath } | Out-File -FilePath 'SHA256SUMS.txt' -Encoding ascii"
cd ..

echo package ready: %STAGE%