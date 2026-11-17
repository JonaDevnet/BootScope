/*
 * BootScope - puntaje
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "../app.h"
#include <stdio.h>
#include <string.h>

/* pesos por categoria (suman 100) */
static const int PESO[BS_CAT_COUNT] = { 40, 25, 10, 15, 10 };

void score_compute(bs_model *m)
{
    int ded[BS_CAT_COUNT] = { 0 };
    int total_peso = 0;
    double acum = 0.0;
    int criticos = 0, moderados = 0, seguros = 0;

    for (int i = 0; i < BS_CAT_COUNT; i++)
        m->puntaje_cat[i] = 100;

    for (int i = 0; i < m->n_items; i++) {
        bs_item *it = &m->items[i];
        int c = (int)it->cat;
        if (c < 0 || c >= BS_CAT_COUNT) continue;
        switch (it->riesgo) {
        case BS_RIESGO_ALTO:     ded[c] += 15; criticos++;  break;
        case BS_RIESGO_MODERADO: ded[c] += 7;  moderados++; break;
        case BS_RIESGO_SEGURO:   ded[c] += 2;  seguros++;   break;
        default: break;
        }
    }

    for (int c = 0; c < BS_CAT_COUNT; c++) {
        int d = ded[c];
        if (d > 100) d = 100;
        m->puntaje_cat[c] = 100 - d;
        acum += (double)m->puntaje_cat[c] * PESO[c];
        total_peso += PESO[c];
    }

    m->puntaje = total_peso > 0 ? (int)(acum / total_peso + 0.5) : 100;
    if (m->puntaje < 0) m->puntaje = 0;
    if (m->puntaje > 100) m->puntaje = 100;

    snprintf(m->resumen, sizeof(m->resumen),
             "%d criticos, %d a revisar, %d mejorables",
             criticos, moderados, seguros);
}
