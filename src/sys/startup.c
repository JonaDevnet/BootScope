/*
 * BootScope - autoinicios (registro + carpetas Inicio)
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "sys.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlobj.h>

/* devuelve 1 habilitado, 0 deshabilitado, -1 sin dato */
static int approval_state(HKEY root, const wchar_t *sub, const wchar_t *valor,
                          unsigned char *primer_byte)
{
    HKEY k;
    DWORD type = 0, sz = 0;
    unsigned char buf[32];
    int estado = -1;

    if (primer_byte) *primer_byte = 0;
    if (RegOpenKeyExW(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return -1;
    if (RegQueryValueExW(k, valor, NULL, &type, NULL, &sz) == ERROR_SUCCESS &&
        type == REG_BINARY && sz >= 1 && sz <= sizeof(buf)) {
        if (RegQueryValueExW(k, valor, NULL, &type, buf, &sz) == ERROR_SUCCESS) {
            if (primer_byte) *primer_byte = buf[0];
            /* 2 y 6 = habilitado, 3 = deshabilitado por el usuario */
            estado = (buf[0] == 3) ? 0 : 1;
        }
    }
    RegCloseKey(k);
    return estado;
}

static void agregar_autoinicio(bs_model *m, const char *origen, const char *nombre,
                               const char *comando, int estado, unsigned char byte)
{
    bs_item *it = bs_add_item(m);
    if (!it) return;
    it->cat = BS_CAT_INICIO;
    snprintf(it->check_id, sizeof(it->check_id), "inicio.%s", origen);
    snprintf(it->titulo, sizeof(it->titulo), "%s", nombre);
    snprintf(it->detalle, sizeof(it->detalle), "%s", comando);
    snprintf(it->estado, sizeof(it->estado), "%s",
             estado == 0 ? "Deshabilitado" : "Habilitado");
    snprintf(it->metrica, sizeof(it->metrica), "autoinicio");
    snprintf(it->edad, sizeof(it->edad), "%s", origen);
    snprintf(it->prev, sizeof(it->prev), "%02X", byte);
    if (estado == 0) {
        it->riesgo = BS_RIESGO_NULO;
        it->accion = BS_ACT_NINGUNA;
    } else {
        it->riesgo = BS_RIESGO_SEGURO;
        it->accion = BS_ACT_AUTOINICIO_DESHABILITAR;
        if (bs_ci_contains(origen, "HKCU") || bs_ci_contains(origen, "HKLM"))
            it->recomendado = 0;
    }
}

static void scan_run_key(bs_model *m, HKEY root, const wchar_t *runkey,
                         const wchar_t *approval, const char *root_name)
{
    HKEY k;
    wchar_t nombre[256];
    char nombre_u8[256], runkey_u8[300], target[400];
    DWORD idx = 0;

    if (RegOpenKeyExW(root, runkey, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return;
    bs_u8(runkey, runkey_u8, sizeof(runkey_u8));

    for (;;) {
        DWORD nlen = 256, tipo = 0, dsz = 0;
        LONG r = RegEnumValueW(k, idx++, nombre, &nlen, NULL, &tipo, NULL, &dsz);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) break;
        if (tipo != REG_SZ && tipo != REG_EXPAND_SZ) continue;

        {
            wchar_t *val = (wchar_t *)calloc(dsz / sizeof(wchar_t) + 4, sizeof(wchar_t));
            char valor_u8[400];
            unsigned char byte = 2;
            int estado;
            if (!val) break;
            if (RegEnumValueW(k, idx - 1, nombre, &nlen, NULL, &tipo,
                              (LPBYTE)val, &dsz) != ERROR_SUCCESS) {
                free(val);
                continue;
            }
            bs_u8(nombre, nombre_u8, sizeof(nombre_u8));
            if (tipo == REG_EXPAND_SZ) {
                wchar_t exp[600];
                if (ExpandEnvironmentStringsW(val, exp, 600))
                    bs_u8(exp, valor_u8, sizeof(valor_u8));
                else
                    bs_u8(val, valor_u8, sizeof(valor_u8));
            } else {
                bs_u8(val, valor_u8, sizeof(valor_u8));
            }
            free(val);

            estado = approval_state(root, approval, nombre, &byte);
            if (estado < 0) estado = 1;

            snprintf(target, sizeof(target), "%s|%s|%s", root_name, runkey_u8, nombre_u8);
            agregar_autoinicio(m, root_name, nombre_u8, valor_u8, estado, byte);
            /* guardamos el target real en el ultimo item agregado */
            if (m->n_items > 0)
                snprintf(m->items[m->n_items - 1].target, sizeof(m->items[m->n_items - 1].target),
                         "%s", target);
        }
    }
    RegCloseKey(k);
}

static void scan_carpeta(bs_model *m, const wchar_t *carpeta, const wchar_t *approval,
                         const char *origen)
{
    WIN32_FIND_DATAW fd;
    HANDLE h;
    wchar_t patron[MAX_PATH];

    swprintf(patron, MAX_PATH, L"%ls\\*", carpeta);
    h = FindFirstFileW(patron, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        char nombre[260], ruta_u8[MAX_PATH * 2], target[MAX_PATH * 2 + 64];
        unsigned char byte = 2;
        int estado;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        bs_u8(fd.cFileName, nombre, sizeof(nombre));
        if (!bs_ci_contains(nombre, ".lnk") && !bs_ci_contains(nombre, ".url") &&
            !bs_ci_contains(nombre, ".exe"))
            continue;

        {
            wchar_t full[MAX_PATH];
            swprintf(full, MAX_PATH, L"%ls\\%ls", carpeta, fd.cFileName);
            bs_u8(full, ruta_u8, sizeof(ruta_u8));
        }
        estado = approval_state(HKEY_CURRENT_USER, approval, fd.cFileName, &byte);
        if (estado < 0) estado = 1;

        snprintf(target, sizeof(target), "FOLDER|%s|%s", ruta_u8, nombre);        agregar_autoinicio(m, origen, nombre, ruta_u8, estado, byte);
        if (m->n_items > 0) {
            bs_item *it = &m->items[m->n_items - 1];
            snprintf(it->target, sizeof(it->target), "%s", target);
            snprintf(it->edad, sizeof(it->edad), "carpeta Inicio");
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

void startup_collect(bs_model *m)
{
    wchar_t ruta[MAX_PATH];

    scan_run_key(m, HKEY_CURRENT_USER,
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",
                 "HKCU");
    scan_run_key(m, HKEY_CURRENT_USER,
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",
                 "HKCU-RunOnce");
    scan_run_key(m, HKEY_LOCAL_MACHINE,
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",
                 "HKLM");
    scan_run_key(m, HKEY_LOCAL_MACHINE,
                 L"Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Run",
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run32",
                 "HKLM32");
    scan_run_key(m, HKEY_LOCAL_MACHINE,
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",
                 "HKLM-RunOnce");

    if (SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, ruta) == S_OK)
        scan_carpeta(m, ruta,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\StartupFolder",
                     "Carpeta usuario");
    if (SHGetFolderPathW(NULL, CSIDL_COMMON_STARTUP, NULL, 0, ruta) == S_OK)
        scan_carpeta(m, ruta,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\StartupFolder",
                     "Carpeta comun");
}


