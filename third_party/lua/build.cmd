@echo off
setlocal
cd /d "%~dp0"
set "SRC=lua-5.4.8\src"
if not exist "%SRC%\lua.h" exit /b 2
if not exist bin mkdir bin
for /f "usebackq tokens=*" %%V in (`where vswhere.exe 2^>nul`) do set VSWHERE=%%V
if not defined VSWHERE set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
for /f "usebackq tokens=*" %%V in (`"%VSWHERE%" -latest -products * -property installationPath`) do set VSROOT=%%V
if not defined VSROOT exit /b 3
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /LD /O2 /W3 /EHsc /D_CRT_SECURE_NO_WARNINGS /DLUA_BUILD_AS_DLL /I"%SRC%" ^
 "%SRC%\lapi.c" "%SRC%\lcode.c" "%SRC%\lctype.c" "%SRC%\ldebug.c" "%SRC%\ldo.c" ^
 "%SRC%\ldump.c" "%SRC%\lfunc.c" "%SRC%\lgc.c" "%SRC%\llex.c" "%SRC%\lmem.c" ^
 "%SRC%\lobject.c" "%SRC%\lopcodes.c" "%SRC%\lparser.c" "%SRC%\lstate.c" ^
 "%SRC%\lstring.c" "%SRC%\ltable.c" "%SRC%\ltm.c" "%SRC%\lundump.c" "%SRC%\lvm.c" "%SRC%\lzio.c" ^
 "%SRC%\lauxlib.c" "%SRC%\lbaselib.c" "%SRC%\lcorolib.c" "%SRC%\ldblib.c" ^
 "%SRC%\liolib.c" "%SRC%\lmathlib.c" "%SRC%\loadlib.c" "%SRC%\loslib.c" ^
 "%SRC%\lstrlib.c" "%SRC%\ltablib.c" "%SRC%\lutf8lib.c" "%SRC%\linit.c" ^
 /link /Brepro /OUT:bin\lua54.dll
if errorlevel 1 exit /b 1
del /q *.obj *.lib *.exp *.pdb *.ilk 2>nul
echo built: bin\lua54.dll