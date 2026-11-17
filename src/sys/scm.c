/*
 * BootScope - inventario de servicios (SCM)
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "sys.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Servicios que historicamente solo consumen tiempo de arranque.
 * Se marcan como recomendados; el resto queda disponible pero sin preseleccion. */
static int servicio_pesado(const char *n)
{
    static const char *lista[] = {
        "mssql$", "sqltelemetry$", "wslservice", "coworkvmservice",
        "mistockapi", "gamingservices", "gamingservicesnet", "xboxservice",
        "gameinputredistservice", "steam client service", "epicgamesupdater",
        "epicgameslauncher", "sqlwriter", "nvidia frameview", "brave",
        "edgeupdate", "googleupdater", "adobearm", "battleye", "easyanticheat"
    };
    for (size_t i = 0; i < sizeof(lista) / sizeof(lista[0]); i++)
        if (bs_ci_contains(n, lista[i]))
            return 1;
    return 0;
}

static int es_de_windows(const char *path)
{
    if (!path[0]) return 1;
    if (bs_ci_contains(path, "c:\\windows\\")) return 1;
    if (bs_ci_contains(path, "\\system32\\")) return 1;
    return 0;
}

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

/* Nunca se ofrecen para cambiar: seguridad o nucleo de Windows */
static int servicio_protegido(const char *nombre, const char *path)
{
    static const char *nombres[] = {
        "WinDefend", "MDCoreSvc", "WdNisSvc", "Sense", "SecurityHealthService",
        "mpssvc", "EventLog", "RpcSs", "DcomLaunch", "LSM", "SamSs", "gpsvc",
        "CryptSvc", "BFE", "Winmgmt", "Schedule", "ProfSvc", "Themes", "Power",
        "BrokerInfrastructure", "CoreMessagingRegistrar", "StateRepository",
        "SystemEventsBroker", "TrustedInstaller", "wuauserv"
    };
    static const char *rutas[] = {
        "windows defender", "microsoft defender", "securityhealth", "\\windowsapps\\"
    };

    for (size_t i = 0; i < sizeof(nombres) / sizeof(nombres[0]); i++)
        if (eq_ci(nombre, nombres[i])) return 1;
    for (size_t i = 0; i < sizeof(rutas) / sizeof(rutas[0]); i++)
        if (bs_ci_contains(path, rutas[i])) return 1;
    return 0;
}

static const char *tipo_inicio(DWORD t)
{
    switch (t) {
    case SERVICE_AUTO_START:     return "Automatico";
    case SERVICE_DEMAND_START:   return "Manual";
    case SERVICE_DISABLED:       return "Deshabilitado";
    case SERVICE_BOOT_START:     return "Arranque";
    case SERVICE_SYSTEM_START:   return "Sistema";
    default:                     return "?";
    }
}

void scm_collect(bs_model *m)
{
    SC_HANDLE scm;
    DWORD bytes = 0, count = 0, resume = 0;
    LPBYTE buf = NULL;
    int ok = 0;

    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm) return;

    for (;;) {
        if (!EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32,
                                   SERVICE_STATE_ALL, buf, bytes, &bytes, &count,
                                   &resume, NULL)) {
            DWORD err = GetLastError();
            if (err == ERROR_MORE_DATA) {
                if (buf) free(buf);
                buf = (LPBYTE)calloc(bytes + 1024, 1);
                if (!buf) break;
                continue;
            }
            break;
        }
        ok = 1;
        break;
    }

    if (ok) {
        ENUM_SERVICE_STATUS_PROCESSW *ss = (ENUM_SERVICE_STATUS_PROCESSW *)buf;
        for (DWORD i = 0; i < count; i++) {
            SC_HANDLE h;
            LPQUERY_SERVICE_CONFIGW cfg = NULL;
            DWORD need = 0;
            SERVICE_DELAYED_AUTO_START_INFO delayed = { 0 };
            DWORD got = 0;
            char nombre[64], display[160], path[300], tipo[24];
            bs_item *it;

            bs_u8(ss[i].lpServiceName, nombre, sizeof(nombre));
            bs_u8(ss[i].lpDisplayName, display, sizeof(display));

            h = OpenServiceW(scm, ss[i].lpServiceName, SERVICE_QUERY_CONFIG);
            if (!h) continue;

            QueryServiceConfigW(h, NULL, 0, &need);
            if (need) {
                cfg = (LPQUERY_SERVICE_CONFIGW)calloc(need, 1);
                if (cfg && !QueryServiceConfigW(h, cfg, need, &need)) {
                    free(cfg);
                    cfg = NULL;
                }
            }
            if (!QueryServiceConfig2W(h, SERVICE_CONFIG_DELAYED_AUTO_START_INFO,
                                     (LPBYTE)&delayed, sizeof(delayed), &got))
                delayed.fDelayedAutostart = FALSE;
            CloseServiceHandle(h);

            if (!cfg) continue;
            if (cfg->dwStartType != SERVICE_AUTO_START) {
                free(cfg);
                continue;
            }

            bs_u8(cfg->lpBinaryPathName ? cfg->lpBinaryPathName : L"", path, sizeof(path));
            snprintf(tipo, sizeof(tipo), "Auto%s", delayed.fDelayedAutostart ? " (retrasado)" : "");

            it = bs_add_item(m);
            if (!it) { free(cfg); break; }
            it->cat = BS_CAT_SERVICIOS;
            snprintf(it->check_id, sizeof(it->check_id), "svc.auto");
            snprintf(it->titulo, sizeof(it->titulo), "%s", display[0] ? display : nombre);
            snprintf(it->target, sizeof(it->target), "%s", nombre);
            snprintf(it->estado, sizeof(it->estado), "%s",
                     ss[i].ServiceStatusProcess.dwCurrentState == SERVICE_RUNNING ? "Running" :
                     ss[i].ServiceStatusProcess.dwCurrentState == SERVICE_STOPPED ? "Stopped" : "Otro");
            snprintf(it->metrica, sizeof(it->metrica), "%s", tipo);
            snprintf(it->edad, sizeof(it->edad), "-");
            snprintf(it->detalle, sizeof(it->detalle), "pid %lu  %s",
                     (unsigned long)ss[i].ServiceStatusProcess.dwProcessId, path);

            if (es_de_windows(path) || servicio_protegido(nombre, path)) {
                it->riesgo = BS_RIESGO_NULO;
                it->accion = BS_ACT_NINGUNA;
                if (servicio_protegido(nombre, path))
                    snprintf(it->resultado, sizeof(it->resultado), "protegido");
            } else {
                it->riesgo = BS_RIESGO_SEGURO;
                it->accion = BS_ACT_SERVICIO_MANUAL;
                snprintf(it->prev, sizeof(it->prev), "auto");
                if (servicio_pesado(nombre)) {
                    it->recomendado = 1;
                    snprintf(it->check_id, sizeof(it->check_id), "svc.pesado");
                }
            }
            free(cfg);
        }
    }

    if (buf) free(buf);
    CloseServiceHandle(scm);
}

