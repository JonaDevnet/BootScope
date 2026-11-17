/*
 * BootScope - aplica cambios con respaldo y reversion
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "actions.h"
#include "../sys/util.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static char g_ultimo_respaldo[MAX_PATH];

const char *actions_last_backup(void)
{
    return g_ultimo_respaldo;
}

/* ------------------------------------------------------------------ */
static int eq_ci(const char *a, const char *b)
{
    while (*a && *b) {
        char x = *a++, y = *b++;
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
    }
    return *a == 0 && *b == 0;
}

static DWORD inicio_desde_texto(const char *t)
{
    if (!t || !t[0]) return SERVICE_DEMAND_START;
    if (eq_ci(t, "auto") || eq_ci(t, "automatico") || eq_ci(t, "automatic"))
        return SERVICE_AUTO_START;
    if (eq_ci(t, "disabled") || eq_ci(t, "deshabilitado"))
        return SERVICE_DISABLED;
    if (eq_ci(t, "boot") || eq_ci(t, "arranque"))
        return SERVICE_BOOT_START;
    if (eq_ci(t, "system") || eq_ci(t, "sistema"))
        return SERVICE_SYSTEM_START;
    return SERVICE_DEMAND_START;
}

static const char *texto_desde_inicio(DWORD t)
{
    switch (t) {
    case SERVICE_AUTO_START:   return "auto";
    case SERVICE_DEMAND_START: return "manual";
    case SERVICE_DISABLED:     return "disabled";
    case SERVICE_BOOT_START:   return "boot";
    case SERVICE_SYSTEM_START: return "system";
    default:                   return "manual";
    }
}

/* Devuelve 1 ok, 0 error, con mensaje */
static int servicio_fijar_inicio(const char *nombre, DWORD tipo, char *msg, size_t msgsz)
{
    SC_HANDLE scm, h;
    wchar_t wnombre[300];
    int ok = 0;

    bs_w(nombre, wnombre, 300);
    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) {
        snprintf(msg, msgsz, "no se pudo abrir el administrador de servicios (%lu)",
                 GetLastError());
        return 0;
    }
    h = OpenServiceW(scm, wnombre, SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG);
    if (!h) {
        snprintf(msg, msgsz, "acceso denegado (servicio protegido)");
        CloseServiceHandle(scm);
        return 0;
    }
    if (ChangeServiceConfigW(h, SERVICE_NO_CHANGE, tipo, SERVICE_NO_CHANGE,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
        /* verificar releyendo */
        LPQUERY_SERVICE_CONFIGW cfg;
        DWORD need = 0;
        QueryServiceConfigW(h, NULL, 0, &need);
        cfg = (LPQUERY_SERVICE_CONFIGW)calloc(need ? need : 1, 1);
        if (cfg && QueryServiceConfigW(h, cfg, need, &need))
            ok = (cfg->dwStartType == tipo);
        if (cfg) free(cfg);
        if (ok) snprintf(msg, msgsz, "%s", texto_desde_inicio(tipo));
        else snprintf(msg, msgsz, "cambio no confirmado");
    } else {
        snprintf(msg, msgsz, "error %lu al cambiar el servicio", GetLastError());
    }
    CloseServiceHandle(h);
    CloseServiceHandle(scm);
    return ok;
}

/* ------------------------------------------------------------------ */
static int autoinicio_bytes(const char *target, HKEY *raiz, char *clave_u8, size_t clavesz,
                            wchar_t *valor, size_t valorsz)
{
    /* formatos: "HKCU|Software\...\Run|Nombre"  o  "FOLDER|C:\ruta\archivo.lnk|archivo.lnk" */
    char copia[400];
    char *p1, *p2;
    const char *aprob;

    snprintf(copia, sizeof(copia), "%s", target);
    p1 = strchr(copia, '|');
    if (!p1) return 0;
    *p1 = 0;
    p2 = strchr(p1 + 1, '|');
    if (!p2) return 0;
    *p2 = 0;

    if (eq_ci(copia, "FOLDER")) {
        const char *archivo = strrchr(p2 + 1, '\\');
        bs_w(archivo ? archivo + 1 : p2 + 1, valor, valorsz);
        *raiz = HKEY_CURRENT_USER;
        aprob = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\StartupFolder";
    } else {
        bs_w(p2 + 1, valor, valorsz);
        if (eq_ci(copia, "HKCU") || eq_ci(copia, "HKCU-RunOnce")) {
            *raiz = HKEY_CURRENT_USER;
            aprob = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
        } else if (eq_ci(copia, "HKLM") || eq_ci(copia, "HKLM-RunOnce")) {
            *raiz = HKEY_LOCAL_MACHINE;
            aprob = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
        } else {
            *raiz = HKEY_LOCAL_MACHINE;
            aprob = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run32";
        }
    }
    snprintf(clave_u8, clavesz, "%s", aprob);
    return 1;
}

static int autoinicio_fijar(const char *target, int habilitar, char *msg, size_t msgsz)
{
    HKEY raiz, hk;
    char clave_u8[300];
    wchar_t sub[300], valor[260];
    unsigned char datos[12];
    LONG r;

    if (!autoinicio_bytes(target, &raiz, clave_u8, sizeof(clave_u8), valor, 260)) {
        snprintf(msg, msgsz, "objetivo invalido");
        return 0;
    }
    memset(datos, 0, sizeof(datos));
    datos[0] = habilitar ? 0x02 : 0x03;
    bs_w(clave_u8, sub, 300);
    r = RegCreateKeyExW(raiz, sub, 0, NULL, 0, KEY_SET_VALUE, NULL, &hk, NULL);
    if (r != ERROR_SUCCESS) {
        snprintf(msg, msgsz, "no se pudo abrir StartupApproved (%ld)", r);
        return 0;
    }
    r = RegSetValueExW(hk, valor, 0, REG_BINARY, datos, sizeof(datos));
    RegCloseKey(hk);
    if (r != ERROR_SUCCESS) {
        snprintf(msg, msgsz, "no se pudo escribir el estado (%ld)", r);
        return 0;
    }
    snprintf(msg, msgsz, "%s", habilitar ? "habilitado" : "deshabilitado");
    return 1;
}

static int rdp_fijar(int bloquear, char *msg, size_t msgsz)
{
    HKEY k;
    DWORD v = bloquear ? 1 : 0;
    LONG r = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                           L"SYSTEM\\CurrentControlSet\\Control\\Terminal Server",
                           0, KEY_SET_VALUE, &k);
    if (r != ERROR_SUCCESS) {
        snprintf(msg, msgsz, "no se pudo abrir Terminal Server (%ld)", r);
        return 0;
    }
    r = RegSetValueExW(k, L"fDenyTSConnections", 0, REG_DWORD, (const BYTE *)&v, sizeof(v));
    RegCloseKey(k);
    if (r != ERROR_SUCCESS) {
        snprintf(msg, msgsz, "no se pudo escribir fDenyTSConnections (%ld)", r);
        return 0;
    }
    snprintf(msg, msgsz, v ? "RDP bloqueado" : "RDP habilitado");
    return 1;
}

/* ------------------------------------------------------------------ */
static void ruta_respaldo(char *dst, size_t dstsz)
{
    wchar_t exe[MAX_PATH];
    wchar_t *barra;
    char dir[MAX_PATH * 2];
    SYSTEMTIME st;
    char fecha[32];

    if (!GetModuleFileNameW(NULL, exe, MAX_PATH)) {
        snprintf(dst, dstsz, "bootscope-backup.txt");
        return;
    }
    barra = wcsrchr(exe, L'\\');
    if (barra) *barra = 0;
    bs_u8(exe, dir, sizeof(dir));
    GetLocalTime(&st);
    snprintf(fecha, sizeof(fecha), "%04d%02d%02d-%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    snprintf(dst, dstsz, "%s\\bootscope-backup-%s.txt", dir, fecha);
}

/* punto de restauracion best-effort (sfc.dll, sin dependencia de link) */
typedef struct {
    DWORD dwEventType;
    DWORD dwRestorePtType;
    INT64 llSequenceNumber;
    WCHAR szDescription[256];
} BS_RESTOREPOINTINFO;

typedef struct {
    DWORD dwEventType;
    DWORD dwRestorePtType;
    INT64 llSequenceNumber;
} BS_STATEMGR;

static int crear_punto_restauracion(void)
{
    typedef BOOL (WINAPI *PFN)(BS_RESTOREPOINTINFO *, BS_STATEMGR *);
    HMODULE mod = LoadLibraryW(L"sfc.dll");
    PFN fn;
    BS_RESTOREPOINTINFO info;
    BS_STATEMGR estado;

    if (!mod) return 0;
    fn = (PFN)(void *)GetProcAddress(mod, "SRSetRestorePointW");
    if (!fn) { FreeLibrary(mod); return 0; }

    memset(&info, 0, sizeof(info));
    memset(&estado, 0, sizeof(estado));
    info.dwEventType = 100;          /* BEGIN_SYSTEM_CHANGE */
    info.dwRestorePtType = 12;       /* MODIFY_SETTINGS */
    wcscpy(info.szDescription, L"BootScope antes de aplicar cambios");
    if (!fn(&info, &estado)) { FreeLibrary(mod); return 0; }
    info.dwEventType = 101;          /* END_SYSTEM_CHANGE */
    fn(&info, &estado);
    FreeLibrary(mod);
    return 1;
}

/* ------------------------------------------------------------------ */
int actions_apply(bs_model *m, char *msg, size_t msgsz)
{
    FILE *f;
    int n = 0, ok = 0, errores = 0, altos = 0;
    char ruta[MAX_PATH];

    if (!bs_is_admin()) {
        snprintf(msg, msgsz, "Se necesitan privilegios de administrador");
        return -1;
    }
    for (int i = 0; i < m->n_items; i++) {
        bs_item *it = &m->items[i];
        if (it->seleccionado && it->accion != BS_ACT_NINGUNA) {
            n++;
            if (it->riesgo >= BS_RIESGO_ALTO) altos++;
        }
    }
    if (!n) {
        snprintf(msg, msgsz, "No hay nada seleccionado");
        return 0;
    }

    if (altos > 0) {
        char texto[512];
        snprintf(texto, sizeof(texto),
                 "Vas a aplicar %d cambios, de los cuales %d son de riesgo ALTO "
                 "(deshabilitar accesos remotos).\n\n"
                 "Se va a guardar un respaldo para poder deshacerlo.\n\n¿Continuar?",
                 n, altos);
        if (MessageBoxA(NULL, texto, "BootScope - confirmar",
                        MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) {
            snprintf(msg, msgsz, "Cancelado por el usuario");
            return 0;
        }
    }

    ruta_respaldo(ruta, sizeof(ruta));
    f = fopen(ruta, "wb");
    if (!f) {
        snprintf(msg, msgsz, "No se pudo crear el respaldo");
        return 0;
    }
    fprintf(f, "# bootscope backup 1\n");
    fflush(f);

    if (altos > 0)
        crear_punto_restauracion();

    for (int i = 0; i < m->n_items; i++) {
        bs_item *it = &m->items[i];
        char res[160] = "";
        int r = 0;
        if (!it->seleccionado || it->accion == BS_ACT_NINGUNA) continue;

        switch (it->accion) {
        case BS_ACT_SERVICIO_MANUAL:
        case BS_ACT_SERVICIO_DESHABILITAR: {
            DWORD tipo = (it->accion == BS_ACT_SERVICIO_MANUAL)
                       ? SERVICE_DEMAND_START : SERVICE_DISABLED;
            fprintf(f, "svc\t%s\t%s\n", it->target, it->prev[0] ? it->prev : "auto");
            fflush(f);
            r = servicio_fijar_inicio(it->target, tipo, res, sizeof(res));
            break;
        }
        case BS_ACT_AUTOINICIO_DESHABILITAR:
            fprintf(f, "startup\t%s\t%s\n", it->target, it->prev[0] ? it->prev : "02");
            fflush(f);
            r = autoinicio_fijar(it->target, 0, res, sizeof(res));
            break;
        case BS_ACT_RDP_DESHABILITAR:
            fprintf(f, "rdp\trdp\t%s\n", it->prev[0] ? it->prev : "0");
            fflush(f);
            r = rdp_fijar(1, res, sizeof(res));
            break;
        default:
            continue;
        }

        it->aplicado = r ? 1 : -1;
        snprintf(it->resultado, sizeof(it->resultado), "%s", res);
        if (r) ok++; else errores++;
    }
    fclose(f);

    if (ok > 0) snprintf(g_ultimo_respaldo, sizeof(g_ultimo_respaldo), "%s", ruta);
    {
        const char *nombre = strrchr(ruta, '\\');
        snprintf(msg, msgsz, "%d aplicados, %d errores. Respaldo: %s",
                 ok, errores, ok > 0 ? (nombre ? nombre + 1 : ruta) : "-");
    }
    return ok;
}

/* ------------------------------------------------------------------ */
static int revertir_linea(char *linea)
{
    char *t1 = strchr(linea, '\t');
    char *t2;
    char res[160] = "";
    if (!t1) return 0;
    *t1++ = 0;
    t2 = strchr(t1, '\t');
    if (!t2) return 0;
    *t2++ = 0;

    if (!strcmp(linea, "svc"))
        return servicio_fijar_inicio(t1, inicio_desde_texto(t2), res, sizeof(res));
    if (!strcmp(linea, "startup"))
        return autoinicio_fijar(t1, t2[0] != '3', res, sizeof(res));
    if (!strcmp(linea, "rdp"))
        return rdp_fijar(t2[0] == '1', res, sizeof(res));
    return 0;
}

int actions_undo(char *msg, size_t msgsz)
{
    FILE *f;
    char linea[600];
    int ok = 0, total = 0;

    if (!bs_is_admin()) {
        snprintf(msg, msgsz, "Se necesitan privilegios de administrador");
        return -1;
    }
    if (!g_ultimo_respaldo[0]) {
        snprintf(msg, msgsz, "No hay un respaldo reciente en esta sesion");
        return 0;
    }
    f = fopen(g_ultimo_respaldo, "rb");
    if (!f) {
        snprintf(msg, msgsz, "No se pudo abrir el respaldo");
        return 0;
    }
    while (fgets(linea, sizeof(linea), f)) {
        size_t n = strlen(linea);
        while (n && (linea[n-1] == '\n' || linea[n-1] == '\r')) linea[--n] = 0;
        if (!linea[0] || linea[0] == '#') continue;
        total++;
        if (revertir_linea(linea)) ok++;
    }
    fclose(f);
    snprintf(msg, msgsz, "Revertidos %d de %d cambios", ok, total);
    return ok;
}
