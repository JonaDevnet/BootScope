@echo off
rem Build rapido con MinGW-w64 (sin CMake)
setlocal
set VERSION=0.0.1
set OUT=build
if not exist %OUT% mkdir %OUT%
where gcc >nul 2>nul
if errorlevel 1 (
  echo [ERROR] gcc no esta en PATH. Instala MinGW-w64 o usa CMake con MSVC.
  exit /b 1
)
echo Compilando BootScope %VERSION%...
gcc -std=c11 -O2 -Wall -Wextra ^
  -DBS_VERSION=\"%VERSION%\" ^
  -Isrc -Ivendor ^
  src\main.c src\app.c src\ui.c src\nk_impl.c ^
  -o %OUT%\bootscope.exe ^
  -mwindows -luser32 -lgdi32 -lmsimg32 -lshell32 -ladvapi32
if errorlevel 1 (
  echo [ERROR] fallo la compilacion.
  exit /b 1
)
echo Listo: %OUT%\bootscope.exe
endlocal
