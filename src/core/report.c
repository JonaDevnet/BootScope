/*
 * BootScope - reporte JSON (compartible, con redaccion opcional)
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "checks.h"
#include "../sys/util.h"
#include <stdio.h>
#include <string.h>

static void jstr(FILE *f, const char *s)
{
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    fputc('"', f);
    for (; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n", f);  break;
        case '\r': fputs("\\r", f);  break;
        case '\t': fputs("\\t", f);  break;
        default:
            if (*p < 0x20) fprintf(f, "\\u%04x", *p);
            else fputc(*p, f);
        }
    }
    fputc('"', f);
}

static void jkey(FILE *f, const char *k)
{
    jstr(f, k);
    fputc(':', f);
}

/* enmascara el ultimo octeto de una IPv4 */
static const char *mask_ip(const char *ip, char *buf, size_t n)
{
    const char *dot = NULL, *p;
    if (!ip) return "";
    for (p = ip; *p; p++)
        if (*p == '.') dot = p;
    if (!dot) return ip;
    snprintf(buf, n, "%.*s.x", (int)(dot - ip), ip);
    return buf;
}

int report_write(const bs_model *m, const char *ruta, int redactar)
{
    FILE *f;
    char tmp[64];

    if (!ruta || !ruta[0]) return 0;
    f = fopen(ruta, "wb");
    if (!f) return 0;

    fputs("{\n", f);
    jkey(f, "schema"); fputs("1,\n", f);
    jkey(f, "tool"); fputs("\"BootScope\",\n", f);
    jkey(f, "version"); jstr(f, bs_version()); fputs(",\n", f);
    jkey(f, "fecha"); jstr(f, m->escaneo_hora); fputs(",\n", f);
    jkey(f, "redactado"); fputs(redactar ? "true,\n" : "false,\n", f);
    jkey(f, "administrador"); fputs(m->admin ? "true,\n" : "false,\n", f);
    jkey(f, "sistema"); fputs("{\n", f);
    jkey(f, "nombre"); jstr(f, m->os_nombre); fputs(",\n", f);
    jkey(f, "build"); jstr(f, m->os_build); fputs(",\n", f);
    jkey(f, "vbs"); fputs(m->vbs ? "true\n" : "false\n", f);
    fputs("},\n", f);

    jkey(f, "puntaje"); fprintf(f, "%d,\n", m->puntaje);
    jkey(f, "puntaje_categoria"); fputs("{", f);
    for (int c = 0; c < BS_CAT_COUNT; c++)
        fprintf(f, "%s\"%s\":%d", c ? "," : "", bs_cat_nombre((bs_cat)c), m->puntaje_cat[c]);
    fputs("},\n", f);

    jkey(f, "arranque"); fputs("{\n", f);
    jkey(f, "medido"); fputs(m->boot.valido ? "true,\n" : "false,\n", f);
    jkey(f, "firmware_s"); fprintf(f, "%.2f,\n", m->boot.firmware_s);
    jkey(f, "windows_s"); fprintf(f, "%.2f,\n", m->boot.main_path_s);
    jkey(f, "escritorio_s"); fprintf(f, "%.2f,\n", m->boot.kernel_desktop_s);
    jkey(f, "inicio_rapido_ok"); fputs(m->boot.fast_startup_ok ? "true,\n" : "false,\n", f);
    jkey(f, "inicio_rapido_conocido"); fputs(m->boot.fast_startup_known ? "true,\n" : "false,\n", f);
    jkey(f, "inicio_rapido_error"); jstr(f, m->boot.fast_startup_err); fputs(",\n", f);
    jkey(f, "apagado_sucio"); fputs(m->boot.apagado_sucio ? "true\n" : "false\n", f);
    fputs("},\n", f);

    jkey(f, "bios"); fputs("{", f);
    jkey(f, "version"); jstr(f, m->bios.version); fputs(",", f);
    jkey(f, "fecha"); jstr(f, m->bios.fecha); fputs(",", f);
    jkey(f, "placa"); jstr(f, m->bios.placa); fputs("},\n", f);

    jkey(f, "memoria"); fputs("[", f);
    for (int i = 0; i < m->n_dimms; i++) {
        fprintf(f, "%s{", i ? "," : "");
        jkey(f, "slot"); jstr(f, m->dimms[i].slot); fputs(",", f);
        jkey(f, "gb"); fprintf(f, "%d,", m->dimms[i].gb);
        jkey(f, "mhz"); fprintf(f, "%d,", m->dimms[i].mhz);
        jkey(f, "modulo"); jstr(f, m->dimms[i].modulo);
        fputs("}", f);
    }
    fputs("],\n", f);

    jkey(f, "discos"); fputs("[", f);
    for (int i = 0; i < m->n_discos; i++) {
        fprintf(f, "%s{", i ? "," : "");
        jkey(f, "modelo"); jstr(f, m->discos[i].modelo); fputs(",", f);
        jkey(f, "bus"); jstr(f, m->discos[i].bus); fputs(",", f);
        jkey(f, "tipo"); jstr(f, m->discos[i].tipo); fputs(",", f);
        jkey(f, "gb"); fprintf(f, "%llu,", m->discos[i].bytes / (1024ULL * 1024ULL * 1024ULL));
        jkey(f, "estado"); jstr(f, m->discos[i].estado); fputs(",", f);
        jkey(f, "salud"); jstr(f, m->discos[i].salud); fputs(",", f);
        jkey(f, "edad"); jstr(f, m->discos[i].edad); fputs(",", f);
        jkey(f, "sistema"); fputs(m->discos[i].sistema ? "true" : "false", f);
        fputs("}", f);
    }
    fputs("],\n", f);

    jkey(f, "hallazgos"); fputs("[", f);
    for (int i = 0; i < m->n_items; i++) {
        const bs_item *it = &m->items[i];
        fprintf(f, "%s{", i ? "," : "");
        jkey(f, "check"); jstr(f, it->check_id); fputs(",", f);
        jkey(f, "categoria"); jstr(f, bs_cat_nombre(it->cat)); fputs(",", f);
        jkey(f, "riesgo"); jstr(f, bs_riesgo_nombre(it->riesgo)); fputs(",", f);
        jkey(f, "titulo"); jstr(f, it->titulo); fputs(",", f);
        jkey(f, "estado"); jstr(f, it->estado); fputs(",", f);
        jkey(f, "dato"); jstr(f, it->metrica); fputs(",", f);
        jkey(f, "edad"); jstr(f, it->edad); fputs(",", f);
        jkey(f, "detalle"); jstr(f, it->detalle); fputs(",", f);
        jkey(f, "recomendado"); fputs(it->recomendado ? "true" : "false", f);
        fputs("}", f);
    }
    fputs("],\n", f);

    jkey(f, "puertos"); fputs("[", f);
    for (int i = 0; i < m->n_puertos; i++) {
        fprintf(f, "%s{", i ? "," : "");
        jkey(f, "puerto"); fprintf(f, "%d,", m->puertos[i].puerto);
        jkey(f, "proceso"); jstr(f, m->puertos[i].proceso); fputs(",", f);
        jkey(f, "exposicion"); jstr(f, m->puertos[i].exposicion);
        fputs("}", f);
    }
    fputs("],\n", f);

    jkey(f, "conexiones"); fputs("[", f);
    for (int i = 0; i < m->n_conexiones; i++) {
        const char *rem = m->conexiones[i].remoto;
        fprintf(f, "%s{", i ? "," : "");
        jkey(f, "proceso"); jstr(f, m->conexiones[i].proceso); fputs(",", f);
        jkey(f, "remoto");
        jstr(f, redactar ? mask_ip(rem, tmp, sizeof(tmp)) : rem);
        fputs("}", f);
    }
    fputs("]\n", f);

    fputs("}\n", f);
    fclose(f);
    return 1;
}
