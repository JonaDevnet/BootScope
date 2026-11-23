@echo off
rem Build rapido con MinGW-w64 (sin CMake)
setlocal
set OUT=build
if not exist %OUT% mkdir %OUT%

set MAJOR=0
set MINOR=0
set PATCH=0
for /f "tokens=1,2,3 delims=." %%a in (VERSION) do (
  set MAJOR=%%a
  set MINOR=%%b
  set PATCH=%%c
)
if "%MINOR%"=="" set MINOR=0
if "%PATCH%"=="" set PATCH=0

where gcc >nul 2>nul
if errorlevel 1 (
  echo [ERROR] gcc no esta en PATH. Instala MinGW-w64 o usa CMake con MSVC.
  exit /b 1
)

echo Compilando BootScope %MAJOR%.%MINOR%.%PATCH%...
gcc -std=c11 -O2 -Wall -Wextra ^
  -DBS_VER_MAJOR=%MAJOR% -DBS_VER_MINOR=%MINOR% -DBS_VER_PATCH=%PATCH% ^
  -Isrc -Ivendor ^
  src\main.c src\app.c src\ui.c src\nk_impl.c ^
  src\sys\util.c src\sys\osinfo.c src\sys\boot.c src\sys\scm.c ^
  src\sys\startup.c src\sys\net.c src\sys\remote.c src\sys\hw.c ^
  src\core\scan.c src\core\checks.c src\core\score.c src\core\report.c src\core\actions.c ^
  -o %OUT%\bootscope.exe ^
  -mwindows -luser32 -lgdi32 -lmsimg32 -lshell32 -ladvapi32 -lwevtapi -liphlpapi -lws2_32 -lole32

if errorlevel 1 (
  echo [ERROR] fallo la compilacion de bootscope.exe.
  exit /b 1
)
echo Listo: %OUT%\bootscope.exe

rem --- pruebas (para el laboratorio y CI) ---
echo Compilando pruebas...
gcc -std=c11 -O2 -w -Isrc -Ivendor ^
  tests\test_actions.c src\core\actions.c src\sys\util.c src\app.c ^
  -o %OUT%\test_actions.exe -luser32 -ladvapi32

gcc -std=c11 -O2 -w -Isrc -Ivendor ^
  tests\test_lab.c src\core\actions.c src\sys\util.c src\app.c ^
  -o %OUT%\test_lab.exe -luser32 -ladvapi32

if errorlevel 1 (
  echo [AVISO] no se pudieron compilar todas las pruebas.
) else (
  echo Listo: %OUT%\test_actions.exe y %OUT%\test_lab.exe
)

endlocal
