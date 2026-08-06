@echo off
setlocal
cd /d "%~dp0"
set "STAGE=%~dp0test-package"
if not exist "%STAGE%" mkdir "%STAGE%"
if exist "%STAGE%\farevermodkit\config" rmdir /s /q "%STAGE%\farevermodkit\config"
if exist "%STAGE%\farevermodkit\modules" rmdir /s /q "%STAGE%\farevermodkit\modules"
if exist "%STAGE%\farevermodkit\third_party" rmdir /s /q "%STAGE%\farevermodkit\third_party"
if exist "%STAGE%\farevermodkit\assets" rmdir /s /q "%STAGE%\farevermodkit\assets"
if not exist "%STAGE%\farevermodkit\third_party\lua\bin" mkdir "%STAGE%\farevermodkit\third_party\lua\bin"
if not exist "%STAGE%\farevermodkit\config" mkdir "%STAGE%\farevermodkit\config"
if not exist "%STAGE%\farevermodkit\logs" mkdir "%STAGE%\farevermodkit\logs"
if not exist "%STAGE%\farevermodkit\assets" mkdir "%STAGE%\farevermodkit\assets"
copy /y build\dxgi.dll "%STAGE%\dxgi.dll" >nul
xcopy /e /i /y ..\modules "%STAGE%\farevermodkit\modules" >nul
copy /y ..\third_party\lua\bin\lua54.dll "%STAGE%\farevermodkit\third_party\lua\bin\lua54.dll" >nul
copy /y ..\config\lua-runtime.json "%STAGE%\farevermodkit\config\lua-runtime.json" >nul
copy /y ..\config\native.json "%STAGE%\farevermodkit\config\native.json" >nul
xcopy /e /i /y ..\assets "%STAGE%\farevermodkit\assets" >nul
echo package ready: %STAGE%