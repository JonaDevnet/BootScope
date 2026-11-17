/*
 * BootScope - estado en memoria
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "app.h"
#include <string.h>
#include <stdio.h>

const char *bs_version(void)
{
    static char v[32];
    if (!v[0])
        snprintf(v, sizeof(v), "%s", BS_VERSION);
    return v;
}

const char *bs_cat_nombre(bs_cat c)
{
    switch (c) {
    case BS_CAT_ARRANQUE:  return "Arranque";
    case BS_CAT_SERVICIOS: return "Servicios";
    case BS_CAT_INICIO:    return "Inicio";
    case BS_CAT_RED:       return "Red y remoto";
    case BS_CAT_HARDWARE:  return "Hardware";
    default:               return "?";
    }
}

const char *bs_riesgo_nombre(bs_riesgo r)
{
    switch (r) {
    case BS_RIESGO_NULO:     return "informativo";
    case BS_RIESGO_SEGURO:   return "seguro";
    case BS_RIESGO_MODERADO: return "moderado";
    case BS_RIESGO_ALTO:     return "alto";
    default:                 return "?";
    }
}

void bs_model_init(bs_model *m)
{
    memset(m, 0, sizeof(*m));
    m->puntaje = -1;
    snprintf(m->resumen, sizeof(m->resumen), "Sin escanear");
    for (int i = 0; i < BS_CAT_COUNT; i++)
        m->puntaje_cat[i] = -1;
}

bs_item *bs_add_item(bs_model *m)
{
    if (m->n_items >= BS_MAX_ITEMS)
        return NULL;
    bs_item *it = &m->items[m->n_items++];
    memset(it, 0, sizeof(*it));
    it->id = m->n_items;
    return it;
}
