/*
 * BootScope - conexiones y puertos (TCP)
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <tlhelp32.h>

#include "sys.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    DWORD pid;
    char  nombre[64];
} proc_entry;

static int mapa_procesos(proc_entry *map, int max)
{
    HANDLE snap;
    PROCESSENTRY32W pe;
    int n = 0;

    snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (n >= max) break;
            map[n].pid = pe.th32ProcessID;
            bs_u8(pe.szExeFile, map[n].nombre, sizeof(map[n].nombre));
            n++;
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return n;
}

static const char *nombre_pid(proc_entry *map, int n, DWORD pid, char *fallback, size_t fsz)
{
    for (int i = 0; i < n; i++)
        if (map[i].pid == pid)
            return map[i].nombre;
    snprintf(fallback, fsz, "pid %lu", (unsigned long)pid);
    return fallback;
}

static void ip_str(DWORD ip, char *out, size_t outsz)
{
    unsigned char *b = (unsigned char *)&ip;
    snprintf(out, outsz, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
}

static int es_loopback(DWORD ip)
{
    unsigned char *b = (unsigned char *)&ip;
    return b[0] == 127;
}

static int puerto_sensible(int puerto)
{
    switch (puerto) {
    case 11434: /* Ollama */
    case 5041:
    case 8080:
    case 3000:
    case 5432:
    case 3306:
    case 6379:
    case 27017:
    case 5000:
    case 8000:
        return 1;
    default:
        return 0;
    }
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

/* procesos que escuchan por diseno del sistema: no son un hallazgo */
static int proceso_sistema(const char *n)
{
    static const char *lista[] = {
        "svchost", "system", "wininit", "services", "spoolsv", "lsass",
        "smss", "csrss", "winlogon", "sihost", "dns", "msdtc", "mqsvc"
    };
    size_t ln = strlen(n);
    for (size_t i = 0; i < sizeof(lista) / sizeof(lista[0]); i++) {
        if (eq_ci(n, lista[i])) return 1;
        /* "svchost.exe" vs "svchost" */
        if (ln == strlen(lista[i]) + 4) {
            char base[32];
            snprintf(base, sizeof(base), "%.*s", (int)strlen(lista[i]), n);
            if (eq_ci(base, lista[i]) && eq_ci(n + strlen(lista[i]), ".exe"))
                return 1;
        }
    }
    return 0;
}

void net_collect(bs_model *m)
{
    proc_entry *map;
    int nmap;
    PMIB_TCPTABLE_OWNER_PID tabla;
    DWORD sz = 0;
    int max = 4096;

    map = (proc_entry *)calloc(1024, sizeof(proc_entry));
    if (!map) return;
    nmap = mapa_procesos(map, 1024);

    if (GetExtendedTcpTable(NULL, &sz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0)
            != ERROR_INSUFFICIENT_BUFFER) {
        free(map);
        return;
    }
    tabla = (PMIB_TCPTABLE_OWNER_PID)calloc(1, sz);
    if (!tabla) { free(map); return; }

    if (GetExtendedTcpTable(tabla, &sz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0)
            != NO_ERROR) {
        free(tabla);
        free(map);
        return;
    }

    for (DWORD i = 0; i < tabla->dwNumEntries; i++) {
        MIB_TCPROW_OWNER_PID *r = &tabla->table[i];
        char proc[64], fb[32], lip[24], rip[24];
        int lport = (int)ntohs((u_short)r->dwLocalPort);
        int rport = (int)ntohs((u_short)r->dwRemotePort);
        const char *pname;

        pname = nombre_pid(map, nmap, r->dwOwningPid, fb, sizeof(fb));
        snprintf(proc, sizeof(proc), "%s", pname);
        ip_str(r->dwLocalAddr, lip, sizeof(lip));
        ip_str(r->dwRemoteAddr, rip, sizeof(rip));

        if (r->dwState == MIB_TCP_STATE_LISTEN) {
            if (m->n_puertos < BS_MAX_PORTS) {
                bs_port *p = &m->puertos[m->n_puertos++];
                memset(p, 0, sizeof(*p));
                p->puerto = lport;
                p->pid = (int)r->dwOwningPid;
                snprintf(p->proceso, sizeof(p->proceso), "%s", proc);
                snprintf(p->direccion, sizeof(p->direccion), "%s", lip);
                if (r->dwLocalAddr == 0 || bs_ci_contains(lip, "0.0.0.0"))
                    snprintf(p->exposicion, sizeof(p->exposicion), "todas");
                else if (es_loopback(r->dwLocalAddr))
                    snprintf(p->exposicion, sizeof(p->exposicion), "solo local");
                else
                    snprintf(p->exposicion, sizeof(p->exposicion), "LAN");
            }
            /* un puerto abierto a toda la red en un proceso no-sistema es un hallazgo */
            if (lport > 0) {
                bs_item *it = bs_add_item(m);
                if (it) {
                    int expuesto = (r->dwLocalAddr == 0) && !proceso_sistema(proc);
                    it->cat = BS_CAT_RED;
                    snprintf(it->check_id, sizeof(it->check_id), "red.puerto");
                    snprintf(it->titulo, sizeof(it->titulo), "%s", proc);
                    snprintf(it->estado, sizeof(it->estado), "Escuchando (%s)",
                             r->dwLocalAddr == 0 ? "todas" : (es_loopback(r->dwLocalAddr) ? "local" : "LAN"));
                    snprintf(it->metrica, sizeof(it->metrica), ":%d", lport);
                    snprintf(it->detalle, sizeof(it->detalle), "%s:%d  (%s)",
                             lip, lport, proc);
                    if (expuesto && puerto_sensible(lport)) {
                        it->riesgo = BS_RIESGO_MODERADO;
                        snprintf(it->resultado, sizeof(it->resultado), "expuesto a la LAN");
                    } else if (expuesto) {
                        it->riesgo = BS_RIESGO_SEGURO;
                        snprintf(it->resultado, sizeof(it->resultado), "escucha en todas las interfaces");
                    } else {
                        it->riesgo = BS_RIESGO_NULO;
                    }
                }
            }
        } else if (r->dwState == MIB_TCP_STATE_ESTAB) {
            int dup = 0;
            if (es_loopback(r->dwRemoteAddr)) continue;
            for (int k = 0; k < m->n_conexiones; k++) {
                if ((int)m->conexiones[k].pid == (int)r->dwOwningPid &&
                    strcmp(m->conexiones[k].remoto, rip) == 0) {
                    char cnt[24];
                    /* reutilizamos estado para el contador */
                    (void)cnt;
                    dup = 1;
                    break;
                }
            }
            if (m->n_conexiones < BS_MAX_CONN) {
                bs_conn *c = &m->conexiones[m->n_conexiones++];
                memset(c, 0, sizeof(*c));
                c->pid = (int)r->dwOwningPid;
                snprintf(c->proceso, sizeof(c->proceso), "%s", proc);
                snprintf(c->local, sizeof(c->local), "%s:%d", lip, lport);
                snprintf(c->remoto, sizeof(c->remoto), "%s", rip);
                snprintf(c->estado, sizeof(c->estado), "%s", "Establecida");
                c->escucha = 0;
            }
            if (!dup) {
                bs_item *it = bs_add_item(m);
                if (it) {
                    it->cat = BS_CAT_RED;
                    snprintf(it->check_id, sizeof(it->check_id), "red.conexion");
                    snprintf(it->titulo, sizeof(it->titulo), "%s", proc);
                    snprintf(it->estado, sizeof(it->estado), "Establecida");
                    snprintf(it->metrica, sizeof(it->metrica), "%s", rip);
                    snprintf(it->detalle, sizeof(it->detalle), "%s:%d -> %s:%d", lip, lport, rip, rport);
                    snprintf(it->edad, sizeof(it->edad), ":%d", rport);
                    it->riesgo = BS_RIESGO_NULO;
                }
            }
        }
    }

    free(tabla);
    free(map);
    (void)max;
}
