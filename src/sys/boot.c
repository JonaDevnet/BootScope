/*
 * BootScope - medicion del ultimo arranque (registro de eventos)
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "sys.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winevt.h>
#include <tlhelp32.h>

/* ------------------------------------------------------------------ */
static long long parse_num(const char *s)
{
    long long v = 0;
    if (!s) return -1;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    if (*s < '0' || *s > '9') return -1;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    return v;
}

/* busca <Data Name="nombre">valor</Data> y devuelve el numero */
static long long xml_num(const char *xml, const char *nombre)
{
    char pat[96];
    const char *p;

    snprintf(pat, sizeof(pat), "Name='%s'", nombre);
    p = strstr(xml, pat);
    if (!p) {
        snprintf(pat, sizeof(pat), "Name=\"%s\"", nombre);
        p = strstr(xml, pat);
    }
    if (!p) return -1;
    p = strchr(p, '>');
    if (!p) return -1;
    return parse_num(p + 1);
}

/*
 * Devuelve el XML del evento mas reciente que cumpla el XPath.
 * 1 = encontrado, 0 = no hay / sin acceso.
 */
static int evt_first(const wchar_t *canal, const wchar_t *xpath,
                     char *xml, size_t xmlsz, FILETIME *cuando)
{
    EVT_HANDLE q, ev = NULL;
    DWORD leidos = 0;
    int ok = 0;

    if (xml && xmlsz) xml[0] = 0;
    q = EvtQuery(NULL, canal, xpath,
                 EvtQueryChannelPath | EvtQueryReverseDirection);
    if (!q) return 0;

    if (EvtNext(q, 1, &ev, 0, 0, &leidos) && leidos == 1) {
        DWORD need = 0, props = 0;
        EvtRender(NULL, ev, EvtRenderEventXml, 0, NULL, &need, &props);
        if (need > 0) {
            wchar_t *wbuf = (wchar_t *)calloc(need / sizeof(wchar_t) + 2, sizeof(wchar_t));
            if (wbuf) {
                if (EvtRender(NULL, ev, EvtRenderEventXml, need, wbuf, &need, &props)) {
                    bs_u8(wbuf, xml, xmlsz);
                    ok = 1;
                }
                free(wbuf);
            }
        }
        if (ok && cuando) {
            /* EvtGetEventInfo falla en algunas versiones de MinGW: parseamos el XML.
               SystemTime='2026-11-16T23:43:59.9235063Z' (UTC) */
            const char *p = strstr(xml, "SystemTime=");
            int Y = 0, M = 0, D = 0, h = 0, m = 0, s = 0;
            memset(cuando, 0, sizeof(*cuando));
            if (p) {
                char q = p[11];
                if ((q == '\'' || q == '"') &&
                    sscanf(p + 12, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) == 6) {
                    SYSTEMTIME st;
                    memset(&st, 0, sizeof(st));
                    st.wYear = (WORD)Y; st.wMonth = (WORD)M; st.wDay = (WORD)D;
                    st.wHour = (WORD)h; st.wMinute = (WORD)m; st.wSecond = (WORD)s;
                    SystemTimeToFileTime(&st, cuando);
                }
            }
        }
        EvtClose(ev);
    }
    EvtClose(q);
    return ok;
}

static ULONGLONG ft_ull(const FILETIME *ft)
{
    ULARGE_INTEGER u;
    u.LowPart = ft->dwLowDateTime;
    u.HighPart = ft->dwHighDateTime;
    return u.QuadPart;
}

static ULONGLONG ahora_ull(void)
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return ft_ull(&ft);
}

/* Fallback sin admin: tiempo desde el arranque hasta que aparecio explorer.exe */
static double medir_escritorio(void)
{
    HANDLE snap, hp;
    PROCESSENTRY32W pe;
    FILETIME c, e, k, u;
    ULONGLONG creacion = 0, boot;
    double seg;

    snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0.0;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (wcscmp(pe.szExeFile, L"explorer.exe") != 0) continue;
            hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (hp) {
                if (GetProcessTimes(hp, &c, &e, &k, &u)) {
                    ULARGE_INTEGER ci;
                    ci.LowPart = c.dwLowDateTime;
                    ci.HighPart = c.dwHighDateTime;
                    if (creacion == 0 || ci.QuadPart < creacion) creacion = ci.QuadPart;
                }
                CloseHandle(hp);
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    if (!creacion) return 0.0;

    boot = ahora_ull() - bs_uptime_s() * 10000000ULL;
    seg = (double)(creacion - boot) / 10000000.0;
    if (seg <= 0.0 || seg > 900.0) return 0.0;
    return seg;
}

/* ------------------------------------------------------------------ */
void boot_collect(bs_model *m)
{
    char xml[32768];
    FILETIME cuando;
    ULONGLONG boot_inicio;

    memset(&m->boot, 0, sizeof(m->boot));
    m->boot.fast_startup_ok = 1;

    boot_inicio = ahora_ull() - bs_uptime_s() * 10000000ULL;

    /* 1) metricas de firmware: Kernel-Boot 30 */
    if (evt_first(L"System",
                  L"*[System[Provider[@Name='Microsoft-Windows-Kernel-Boot'] and EventID=30]]",
                  xml, sizeof(xml), &cuando)) {
        bs_debug("evt30", xml);
        long long load = xml_num(xml, "LoadOSImageStart");
        long long exit_boot = xml_num(xml, "ExitBootServicesEntry");
        if (load > 0 && ft_ull(&cuando) >= boot_inicio - 3000000000ULL) {
            m->boot.firmware_s = (double)load / 1000.0;
            m->boot.valido = 1;
        }
        (void)exit_boot;
    }

    /* 2) estado del inicio rapido: Kernel-Boot 29 */
    if (evt_first(L"System",
                  L"*[System[Provider[@Name='Microsoft-Windows-Kernel-Boot'] and EventID=29]]",
                  xml, sizeof(xml), &cuando)) {
        m->boot.fast_startup_known = 1;
        if (ft_ull(&cuando) >= boot_inicio - 600000000ULL) {
            long long st = xml_num(xml, "FailureStatus");
            m->boot.fast_startup_ok = 0;
            if (st > 0)
                snprintf(m->boot.fast_startup_err, sizeof(m->boot.fast_startup_err),
                         "0x%08llX", (unsigned long long)st);
            else
                snprintf(m->boot.fast_startup_err, sizeof(m->boot.fast_startup_err),
                         "error");
        }
    }

    /* 3) fase critica: Diagnostics-Performance 100 (solo si es de este arranque) */
    if (evt_first(L"Microsoft-Windows-Diagnostics-Performance/Operational",
                  L"*[System[EventID=100]]", xml, sizeof(xml), &cuando)) {
        long long mainp = xml_num(xml, "MainPathBootTime");
        if (mainp > 0 && ft_ull(&cuando) >= boot_inicio - 900000000ULL) {
            m->boot.main_path_s = (double)mainp / 1000.0;
            m->boot.valido = 1;
        }
    }

    /* 4) apagado sucio de este arranque: 6008 / 41 */
    if (evt_first(L"System", L"*[System[EventID=6008]]", xml, sizeof(xml), &cuando)) {
        if (ft_ull(&cuando) >= boot_inicio - 1200000000ULL)
            m->boot.apagado_sucio = 1;
    }
    if (!m->boot.apagado_sucio &&
        evt_first(L"System", L"*[System[EventID=41]]", xml, sizeof(xml), &cuando)) {
        if (ft_ull(&cuando) >= boot_inicio - 1200000000ULL)
            m->boot.apagado_sucio = 1;
    }

    /* 5) controladores de arranque que no cargaron: 7026 */
    if (evt_first(L"System", L"*[System[EventID=7026]]", xml, sizeof(xml), &cuando)) {
        if (ft_ull(&cuando) >= boot_inicio - 1800000000ULL) {
            const char *p = strstr(xml, "<Data>");
            const char *e;
            if (p) {
                p += 6;
                e = strstr(p, "</Data>");
                if (e) {
                    size_t n = (size_t)(e - p);
                    if (n >= sizeof(m->boot.drivers_fallidos))
                        n = sizeof(m->boot.drivers_fallidos) - 1;
                    memcpy(m->boot.drivers_fallidos, p, n);
                    m->boot.drivers_fallidos[n] = 0;
                    bs_trim(m->boot.drivers_fallidos);
                }
            }
        }
    }

    /* la fase de Windows: evento 100 si es reciente, si no medimos explorer.exe */
    if (m->boot.main_path_s > 0.0)
        m->boot.kernel_desktop_s = m->boot.main_path_s;
    else
        m->boot.kernel_desktop_s = medir_escritorio();
    if (m->boot.kernel_desktop_s > 0.0 || m->boot.firmware_s > 0.0)
        m->boot.valido = 1;
}



