@echo off
setlocal
cd /d "%~dp0"
for /f "usebackq tokens=*" %%V in (`where vswhere.exe 2^>nul`) do set VSWHERE=%%V
if not defined VSWHERE set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
for /f "usebackq tokens=*" %%V in (`"%VSWHERE%" -latest -products * -property installationPath`) do set VSROOT=%%V
if not defined VSROOT exit /b 1
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++20 /EHsc /MT /W4 /c src\plugin_manager.cpp /Fo:build\plugin_manager.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /MT /W4 /c src\lua_runtime.cpp /Fo:build\lua_runtime.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /MT /W4 /c src\plugin_host.cpp /Fo:build\plugin_host.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /MT /W4 /c src\memory\memory_log.cpp /Fo:build\memory_log.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /MT /W4 /c src\memory\hl_runtime.cpp /Fo:build\hl_runtime.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /MT /W4 /c src\memory\hl_scan.cpp /Fo:build\hl_scan.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /MT /W4 /c src\memory\hl_reader.cpp /Fo:build\hl_reader.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /MT /W4 /c src\memory\game_memory.cpp /Fo:build\game_memory.obj
if errorlevel 1 exit /b 1
echo built: core and memory objects