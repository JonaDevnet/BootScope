/*
 * BootScope - deteccion de accesos remotos
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "sys.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int  existe;
    int  corriendo;
    char inicio[24];
    char display[160];
} svc_info;

static void consultar_servicio(const wchar_t *nombre, svc_info *out)
{
    SC_HANDLE scm, h;
    SERVICE_STATUS_PROCESS st;
    DWORD got = 0;
    LPQUERY_SERVICE_CONFIGW cfg;
    DWORD need = 0;

    memset(out, 0, sizeof(*out));
    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return;
    h = OpenServiceW(scm, nombre, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!h) { CloseServiceHandle(scm); return; }

    out->existe = 1;
    if (QueryServiceStatusEx(h, SC_STATUS_PROCESS_INFO, (LPBYTE)&st, sizeof(st), &got))
        out->corriendo = (st.dwCurrentState == SERVICE_RUNNING);

    QueryServiceConfigW(h, NULL, 0, &need);
    if (need) {
        cfg = (LPQUERY_SERVICE_CONFIGW)calloc(need, 1);
        if (cfg && QueryServiceConfigW(h, cfg, need, &need)) {
            const char *t = "?";
            switch (cfg->dwStartType) {
            case SERVICE_AUTO_START:   t = "Auto"; break;
            case SERVICE_DEMAND_START: t = "Manual"; break;
            case SERVICE_DISABLED:     t = "Deshabilitado"; break;
            case SERVICE_BOOT_START:   t = "Arranque"; break;
            case SERVICE_SYSTEM_START: t = "Sistema"; break;
            }
            snprintf(out->inicio, sizeof(out->inicio), "%s", t);
            bs_u8(cfg->lpDisplayName ? cfg->lpDisplayName : L"", out->display, sizeof(out->display));
        }
        if (cfg) free(cfg);
    }
    CloseServiceHandle(h);
    CloseServiceHandle(scm);
}

static void fecha_archivo(const wchar_t *ruta, char *out, size_t outsz)
{
    WIN32_FILE_ATTRIBUTE_DATA fa;
    SYSTEMTIME st;
    if (out && outsz) out[0] = 0;
    if (!GetFileAttributesExW(ruta, GetFileExInfoStandard, &fa)) return;
    if (!FileTimeToSystemTime(&fa.ftLastWriteTime, &st)) return;
    snprintf(out, outsz, "%02d/%02d/%04d", st.wDay, st.wMonth, st.wYear);
}

static int startswith_ci(const char *s, const char *pre);

static void agregar_remoto(bs_model *m, const char *titulo, const char *estado,
                           const char *detalle, const char *target, const char *inicio,
                           bs_riesgo riesgo, int recomendado)
{
    bs_item *it = bs_add_item(m);
    if (!it) return;
    it->cat = BS_CAT_RED;
    snprintf(it->check_id, sizeof(it->check_id), "remoto.canal");
    snprintf(it->titulo, sizeof(it->titulo), "%s", titulo);
    snprintf(it->estado, sizeof(it->estado), "%s", estado);
    snprintf(it->detalle, sizeof(it->detalle), "%s", detalle);
    snprintf(it->target, sizeof(it->target), "%s", target);
    snprintf(it->prev, sizeof(it->prev), "%s", inicio);
    snprintf(it->metrica, sizeof(it->metrica), "acceso remoto");
    it->riesgo = riesgo;
    it->recomendado = recomendado;
    if (bs_ci_contains(estado, "Activo") || bs_ci_contains(estado, "Habilitado"))
        it->accion = BS_ACT_SERVICIO_DESHABILITAR;
}

static int startswith_ci(const char *s, const char *pre)
{
    while (*pre) {
        char a = *s++, b = *pre++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (!a || a != b) return 0;
    }
    return 1;
}

void remote_collect(bs_model *m)
{
    DWORD deny = 1, help = 0;
    svc_info sv;

    /* --- Escritorio remoto (RDP) --- */
    if (bs_reg_dword(HKEY_LOCAL_MACHINE,
                     L"SYSTEM\\CurrentControlSet\\Control\\Terminal Server",
                     L"fDenyTSConnections", &deny) && deny == 0) {
        bs_item *it = bs_add_item(m);
        if (it) {
            it->cat = BS_CAT_RED;
            snprintf(it->check_id, sizeof(it->check_id), "remoto.rdp");
            snprintf(it->titulo, sizeof(it->titulo), "Escritorio remoto (RDP)");
            snprintf(it->estado, sizeof(it->estado), "Habilitado");
            snprintf(it->metrica, sizeof(it->metrica), "3389");
            snprintf(it->detalle, sizeof(it->detalle),
                     "Acepta conexiones RDP desde la red. Deshabilitado por defecto en Windows 11.");
            snprintf(it->target, sizeof(it->target), "rdp");
            snprintf(it->prev, sizeof(it->prev), "0");
            it->riesgo = BS_RIESGO_ALTO;
            it->accion = BS_ACT_RDP_DESHABILITAR;
            it->recomendado = 1;
            snprintf(it->resultado, sizeof(it->resultado), "expuesto");
        }
    } else {
        bs_item *it = bs_add_item(m);
        if (it) {
            it->cat = BS_CAT_RED;
            snprintf(it->check_id, sizeof(it->check_id), "remoto.rdp.off");
            snprintf(it->titulo, sizeof(it->titulo), "Escritorio remoto (RDP)");
            snprintf(it->estado, sizeof(it->estado), "Deshabilitado");
            snprintf(it->metrica, sizeof(it->metrica), "3389");
            snprintf(it->detalle, sizeof(it->detalle), "Sin acceso RDP entrante. Correcto.");
            it->riesgo = BS_RIESGO_NULO;
        }
    }

    /* --- Chrome Remote Desktop --- */
    consultar_servicio(L"chromoting", &sv);
    if (sv.existe) {
        char edad[32], det[400];
        fecha_archivo(L"C:\\ProgramData\\Google\\Chrome Remote Desktop\\host.json",
                      edad, sizeof(edad));
        snprintf(det, sizeof(det),
                 "Equipo registrado en Chrome Remote Desktop desde el %s. Permite entrar a esta PC desde cualquier lugar con una cuenta de Google.",
                 edad[0] ? edad : "(fecha desconocida)");
        agregar_remoto(m, "Chrome Remote Desktop", sv.corriendo ? "Activo" : "Instalado (detenido)",
                       det, "chromoting", sv.inicio, BS_RIESGO_ALTO, sv.corriendo);
        snprintf(m->items[m->n_items - 1].edad, sizeof(m->items[m->n_items - 1].edad),
                 "%s", edad[0] ? edad : "-");
    }

    /* --- otros escritorios remotos conocidos --- */
    {
        struct { const wchar_t *svc; const char *nombre; } otros[] = {
            { L"TeamViewer",          "TeamViewer" },
            { L"TeamViewer_Service",  "TeamViewer" },
            { L"AnyDesk",             "AnyDesk" },
            { L"RustDesk",            "RustDesk" },
            { L"tvnserver",           "TightVNC" },
            { L"uvnc_service",        "UltraVNC" },
            { L"ScreenConnect Client", "ScreenConnect" },
            { L"SplashtopRemoteService", "Splashtop" }
        };
        for (size_t i = 0; i < sizeof(otros) / sizeof(otros[0]); i++) {
            consultar_servicio(otros[i].svc, &sv);
            if (!sv.existe) continue;
            agregar_remoto(m, otros[i].nombre,
                           sv.corriendo ? "Activo" : "Instalado (detenido)",
                           "Servicio de acceso remoto de terceros instalado.",
                           otros[i].nombre, sv.inicio, BS_RIESGO_ALTO, sv.corriendo);
        }
    }

    /* --- Asistencia remota de Windows --- */
    if (bs_reg_dword(HKEY_LOCAL_MACHINE,
                     L"SYSTEM\\CurrentControlSet\\Control\\Remote Assistance",
                     L"fAllowToGetHelp", &help) && help) {
        bs_item *it = bs_add_item(m);
        if (it) {
            it->cat = BS_CAT_RED;
            snprintf(it->check_id, sizeof(it->check_id), "remoto.asistencia");
            snprintf(it->titulo, sizeof(it->titulo), "Asistencia remota de Windows");
            snprintf(it->estado, sizeof(it->estado), "Habilitada");
            snprintf(it->metrica, sizeof(it->metrica), "invitacion");
            snprintf(it->detalle, sizeof(it->detalle),
                     "Permite que alguien se conecte enviando una invitacion. No requiere contrasena previa.");
            it->riesgo = BS_RIESGO_MODERADO;
            snprintf(it->resultado, sizeof(it->resultado), "revisar");
        }
    }
}
