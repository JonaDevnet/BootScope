@echo off
cd /d D:\proyectsVS\BootScope
echo #include "app.h" > tv.c
echo const char *pv = BS_VERSION; >> tv.c
gcc -DBS_VER_MAJOR=0 -DBS_VER_MINOR=2 -DBS_VER_PATCH=0 -Isrc -E tv.c > tv.i 2> tv.err
echo EXIT=%errorlevel%
