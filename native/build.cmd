@echo off
setlocal
cd /d "%~dp0"
for /f "usebackq tokens=*" %%V in (`where vswhere.exe 2^>nul`) do set VSWHERE=%%V
if not defined VSWHERE set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
for /f "usebackq tokens=*" %%V in (`"%VSWHERE%" -latest -products * -property installationPath`) do set VSROOT=%%V
if not defined VSROOT exit /b 1
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist build mkdir build
cl /nologo /LD /O2 /MT /W3 /EHa /std:c++17 /D_CRT_SECURE_NO_WARNINGS ^
   /I. /I..\core\src /Fo:build\ /Fe:build\dxgi.dll ^
   dxgi_proxy.cpp dxgi_wrap.cpp overlay_d3d12.cpp atlas_ui.cpp atlas_input.cpp navigator.cpp atlas_stubs.cpp ^
   ..\core\build\plugin_host.obj ..\core\build\plugin_manager.obj ..\core\build\lua_runtime.obj ^
   ..\core\build\memory_log.obj ..\core\build\hl_runtime.obj ..\core\build\hl_scan.obj ^
   ..\core\build\hl_reader.obj ..\core\build\game_memory.obj ^
   /link /DEF:dxgi.def /OUT:build\dxgi.dll kernel32.lib user32.lib gdi32.lib ^
   dxgi.lib d3d12.lib d3dcompiler.lib bcrypt.lib windowscodecs.lib ole32.lib
if errorlevel 1 exit /b 1
echo built: build\dxgi.dll