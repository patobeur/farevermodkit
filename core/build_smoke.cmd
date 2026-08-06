@echo off
setlocal
cd /d "%~dp0"
for /f "usebackq tokens=*" %%V in (`where vswhere.exe 2^>nul`) do set VSWHERE=%%V
if not defined VSWHERE set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
for /f "usebackq tokens=*" %%V in (`"%VSWHERE%" -latest -products * -property installationPath`) do set VSROOT=%%V
if not defined VSROOT exit /b 1
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++20 /EHsc /MT /W4 tests\lua_smoke.cpp build\lua_runtime.obj build\memory_log.obj build\hl_runtime.obj build\hl_scan.obj build\hl_reader.obj build\game_memory.obj /Fo:build\lua_smoke.obj /Fe:build\lua_smoke.exe /link bcrypt.lib
if errorlevel 1 exit /b 1
echo built: build\lua_smoke.exe