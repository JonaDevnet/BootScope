/*
 * BootScope - interfaz
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#ifndef BS_UI_H
#define BS_UI_H

#include "nuklear.h"
#include "app.h"

#define BS_TAB_RESUMEN  0
#define BS_TAB_COUNT    (BS_CAT_COUNT + 1)

typedef struct {
    int  tab;
    int  pedir_escaneo;
    int  pedir_aplicar;
    int  pedir_exportar;
    int  pedir_deshacer;
    char mensaje[256];
    int  mensaje_error;
} bs_ui;

void ui_init_style(struct nk_context *ctx);
void ui_draw(struct nk_context *ctx, bs_model *m, bs_ui *ui);

#endif /* BS_UI_H */
