/*
 * BootScope - interfaz Nuklear sobre Win32/GDI
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "ui.h"
#include <stdio.h>
#include <string.h>

#define BS_WIN_W 1280
#define BS_WIN_H 820
#define BS_SIDEBAR 196.0f
#define BS_HEADER  68.0f

/* ------------------------------------------------------------------ */
/* paleta                                                              */
/* ------------------------------------------------------------------ */
static const struct nk_color C_BG       = { 24, 25, 28, 255 };
static const struct nk_color C_PANEL    = { 32, 34, 38, 255 };
static const struct nk_color C_PANEL_2  = { 39, 42, 47, 255 };
static const struct nk_color C_BORDER   = { 58, 62, 70, 255 };
static const struct nk_color C_TEXT     = { 226, 229, 234, 255 };
static const struct nk_color C_TEXT_DIM = { 150, 156, 166, 255 };
static const struct nk_color C_ACCENT   = { 88, 166, 255, 255 };
static const struct nk_color C_OK       = { 87, 199, 133, 255 };
static const struct nk_color C_WARN     = { 233, 178, 74, 255 };
static const struct nk_color C_BAD      = { 232, 96, 96, 255 };

static const char *TABS[BS_TAB_COUNT] = {
    "Resumen", "Arranque", "Servicios", "Inicio", "Red y remoto", "Hardware"
};

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

/* copia hasta max bytes respetando limites UTF-8 y agrega "..." si corta */
static const char *clip(char *dst, size_t dstsz, const char *src, int maxchars)
{
    size_t n;
    if (!src) src = "";
    n = strlen(src);
    if ((int)n <= maxchars) {
        snprintf(dst, dstsz, "%s", src);
        return dst;
    }
    n = (size_t)maxchars;
    while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80)
        n--;
    if (n + 4 > dstsz) n = dstsz - 4;
    memcpy(dst, src, n);
    memcpy(dst + n, "...", 4);
    return dst;
}

static struct nk_color riesgo_color(bs_riesgo r)
{
    switch (r) {
    case BS_RIESGO_ALTO:     return C_BAD;
    case BS_RIESGO_MODERADO: return C_WARN;
    case BS_RIESGO_SEGURO:   return C_OK;
    default:                 return C_TEXT_DIM;
    }
}

static struct nk_color estado_color(const char *s, bs_riesgo r)
{
    if (!s || !s[0]) return C_TEXT_DIM;
    if (strstr(s, "Running") || strstr(s, "Healthy") || strstr(s, "Activo") ||
        strstr(s, "OK") || strstr(s, "Habilitad"))
        return C_OK;
    if (strstr(s, "Stopped") || strstr(s, "Deshabilitad") || strstr(s, "Sin "))
        return C_TEXT_DIM;
    return riesgo_color(r);
}

/* ------------------------------------------------------------------ */
/* estilo                                                              */
/* ------------------------------------------------------------------ */
void ui_init_style(struct nk_context *ctx)
{
    struct nk_color t[NK_COLOR_COUNT];
    t[NK_COLOR_TEXT]                    = C_TEXT;
    t[NK_COLOR_WINDOW]                  = C_BG;
    t[NK_COLOR_HEADER]                  = C_PANEL;
    t[NK_COLOR_BORDER]                  = C_BORDER;
    t[NK_COLOR_BUTTON]                  = C_PANEL_2;
    t[NK_COLOR_BUTTON_HOVER]            = nk_rgb(52, 56, 63);
    t[NK_COLOR_BUTTON_ACTIVE]           = nk_rgb(64, 70, 80);
    t[NK_COLOR_TOGGLE]                  = C_PANEL_2;
    t[NK_COLOR_TOGGLE_HOVER]            = nk_rgb(52, 56, 63);
    t[NK_COLOR_TOGGLE_CURSOR]           = C_ACCENT;
    t[NK_COLOR_SELECT]                  = nk_rgb(44, 49, 58);
    t[NK_COLOR_SELECT_ACTIVE]           = C_ACCENT;
    t[NK_COLOR_SLIDER]                  = C_PANEL_2;
    t[NK_COLOR_SLIDER_CURSOR]           = C_ACCENT;
    t[NK_COLOR_SLIDER_CURSOR_HOVER]     = nk_rgb(120, 186, 255);
    t[NK_COLOR_SLIDER_CURSOR_ACTIVE]    = nk_rgb(150, 200, 255);
    t[NK_COLOR_PROPERTY]                = C_PANEL_2;
    t[NK_COLOR_EDIT]                    = C_PANEL_2;
    t[NK_COLOR_EDIT_CURSOR]             = C_TEXT;
    t[NK_COLOR_COMBO]                   = C_PANEL_2;
    t[NK_COLOR_CHART]                   = C_PANEL_2;
    t[NK_COLOR_CHART_COLOR]             = C_ACCENT;
    t[NK_COLOR_CHART_COLOR_HIGHLIGHT]   = C_ACCENT;
    t[NK_COLOR_SCROLLBAR]               = C_PANEL;
    t[NK_COLOR_SCROLLBAR_CURSOR]        = nk_rgb(70, 76, 86);
    t[NK_COLOR_SCROLLBAR_CURSOR_HOVER]  = nk_rgb(86, 94, 106);
    t[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = C_ACCENT;
    t[NK_COLOR_TAB_HEADER]              = C_PANEL;
    nk_style_from_table(ctx, t);

    ctx->style.window.padding          = nk_vec2(14, 12);
    ctx->style.window.group_padding    = nk_vec2(10, 8);
    ctx->style.window.spacing          = nk_vec2(8, 6);
    ctx->style.window.scrollbar_size   = nk_vec2(12, 12);
    ctx->style.window.border_color     = C_BORDER;
    ctx->style.window.border           = 1;
    ctx->style.window.rounding         = 6;
    ctx->style.button.rounding         = 5;
    ctx->style.button.border           = 1.0f;
    ctx->style.button.border_color     = C_BORDER;
    ctx->style.button.text_normal      = C_TEXT;
    ctx->style.button.text_hover       = C_TEXT;
    ctx->style.button.text_active      = C_TEXT;
    ctx->style.checkbox.border_color   = C_BORDER;
    ctx->style.checkbox.cursor_normal  = nk_style_item_color(C_PANEL_2);
    ctx->style.checkbox.cursor_hover   = nk_style_item_color(nk_rgb(52, 56, 63));
    ctx->style.checkbox.text_normal    = C_TEXT;
}

/* ------------------------------------------------------------------ */
/* barras y tarjetas                                                   */
/* ------------------------------------------------------------------ */
static void barra(struct nk_context *ctx, const char *etiqueta, double seg,
                  double total, struct nk_color col)
{
    float frac = 0.0f;
    char txt[96];
    if (total > 0.001) frac = (float)(seg / total);
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    snprintf(txt, sizeof(txt), "%s  %.1f s", etiqueta, seg);
    nk_layout_row_begin(ctx, NK_DYNAMIC, 18, 2);
    nk_layout_row_push(ctx, 0.42f);
    nk_label(ctx, txt, NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.58f);
    {
        nk_size v = (nk_size)(frac * 100.0f);
        nk_progress(ctx, &v, 100, nk_false);
        (void)col;
    }
    nk_layout_row_end(ctx);
}

static void tarjeta(struct nk_context *ctx, const char *titulo, const char *valor,
                    const char *pie, struct nk_color col)
{
    nk_layout_row_push(ctx, 0.25f);
    nk_group_begin(ctx, titulo, NK_WINDOW_BORDER);
    nk_layout_row_dynamic(ctx, 16, 1);
    nk_label_colored(ctx, titulo, NK_TEXT_LEFT, C_TEXT_DIM);
    nk_layout_row_dynamic(ctx, 30, 1);
    nk_label_colored(ctx, valor, NK_TEXT_LEFT, col);
    nk_layout_row_dynamic(ctx, 16, 1);
    nk_label_colored(ctx, pie, NK_TEXT_LEFT, C_TEXT_DIM);
    nk_group_end(ctx);
}

/* ------------------------------------------------------------------ */
/* fila de item                                                        */
/* ------------------------------------------------------------------ */
static int fila_item(struct nk_context *ctx, bs_model *m, bs_item *it, int idx)
{
    char b1[220], b2[220], b3[80], b4[80], b5[96];
    int toggled = 0;

    nk_layout_row_begin(ctx, NK_DYNAMIC, 26, 7);
    nk_layout_row_push(ctx, 0.040f);
    {
        nk_bool sel = it->seleccionado ? nk_true : nk_false;
        if (nk_checkbox_label(ctx, "", &sel)) {
            it->seleccionado = sel ? 1 : 0;
            toggled = 1;
        }
    }
    nk_layout_row_push(ctx, 0.300f);
    nk_label(ctx, clip(b1, sizeof(b1), it->titulo, 44), NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.130f);
    nk_label(ctx, clip(b2, sizeof(b2), it->estado, 16), NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.110f);
    nk_label(ctx, clip(b3, sizeof(b3), it->metrica, 12), NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.110f);
    nk_label(ctx, clip(b4, sizeof(b4), it->edad, 12), NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.120f);
    nk_label_colored(ctx, bs_riesgo_nombre(it->riesgo), NK_TEXT_LEFT,
                     riesgo_color(it->riesgo));
    nk_layout_row_push(ctx, 0.190f);
    if (it->aplicado == 1) {
        nk_label_colored(ctx, it->resultado[0] ? it->resultado : "aplicado",
                         NK_TEXT_LEFT, C_OK);
    } else if (it->aplicado < 0) {
        nk_label_colored(ctx, clip(b5, sizeof(b5), it->resultado, 22),
                         NK_TEXT_LEFT, C_BAD);
    } else if (it->accion == BS_ACT_NINGUNA) {
        nk_label_colored(ctx, "solo aviso", NK_TEXT_LEFT, C_TEXT_DIM);
    } else {
        nk_label_colored(ctx, "aplicable", NK_TEXT_LEFT, C_ACCENT);
    }
    nk_layout_row_end(ctx);
    (void)m; (void)idx;
    return toggled;
}

static void cabecera_tabla(struct nk_context *ctx)
{
    nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 7);
    nk_layout_row_push(ctx, 0.040f); nk_label(ctx, "", NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.300f); nk_label(ctx, "Componente", NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.130f); nk_label(ctx, "Estado", NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.110f); nk_label(ctx, "Dato", NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.110f); nk_label(ctx, "Edad", NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.120f); nk_label(ctx, "Riesgo", NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.190f); nk_label(ctx, "Accion", NK_TEXT_LEFT);
    nk_layout_row_end(ctx);
}

/* ------------------------------------------------------------------ */
/* paneles                                                             */
/* ------------------------------------------------------------------ */
static void panel_header(struct nk_context *ctx, bs_model *m, bs_ui *ui)
{
    char buf[160];
    if (nk_begin(ctx, "header", nk_rect(0, 0, BS_WIN_W, BS_HEADER),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
        nk_layout_row_begin(ctx, NK_DYNAMIC, 32, 3);
        nk_layout_row_push(ctx, 0.42f);
        nk_label_colored(ctx, "BootScope", NK_TEXT_LEFT, C_TEXT);
        nk_layout_row_push(ctx, 0.30f);
        snprintf(buf, sizeof(buf), "v%s   %s", bs_version(),
                 m->escaneado ? m->escaneo_hora : "sin escanear");
        nk_label_colored(ctx, buf, NK_TEXT_LEFT, C_TEXT_DIM);
        nk_layout_row_push(ctx, 0.28f);
        snprintf(buf, sizeof(buf), "%s", m->admin ? "ADMINISTRADOR" : "sin privilegios");
        nk_label_colored(ctx, buf, NK_TEXT_RIGHT, m->admin ? C_OK : C_WARN);
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_DYNAMIC, 26, 3);
        nk_layout_row_push(ctx, 0.18f);
        if (nk_button_label(ctx, "Escanear")) ui->pedir_escaneo = 1;
        nk_layout_row_push(ctx, 0.18f);
        {
            int n = 0;
            for (int i = 0; i < m->n_items; i++)
                if (m->items[i].seleccionado && m->items[i].accion != BS_ACT_NINGUNA) n++;
            snprintf(buf, sizeof(buf), "Aplicar (%d)", n);
            if (nk_button_label(ctx, buf)) ui->pedir_aplicar = 1;
        }
        nk_layout_row_push(ctx, 0.18f);
        if (nk_button_label(ctx, "Exportar JSON")) ui->pedir_exportar = 1;
        nk_layout_row_end(ctx);
    }
    nk_end(ctx);
}

static void panel_sidebar(struct nk_context *ctx, bs_model *m, bs_ui *ui)
{
    if (nk_begin(ctx, "nav", nk_rect(0, BS_HEADER, BS_SIDEBAR, BS_WIN_H - BS_HEADER),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
        nk_layout_row_dynamic(ctx, 6, 1);
        nk_spacing(ctx, 1);
        for (int i = 0; i < BS_TAB_COUNT; i++) {
            nk_bool sel = (ui->tab == i) ? nk_true : nk_false;
            nk_layout_row_dynamic(ctx, 32, 1);
            if (nk_selectable_label(ctx, TABS[i], NK_TEXT_LEFT, &sel))
                ui->tab = i;
        }
        /* contadores por categoria */
        nk_layout_row_dynamic(ctx, 14, 1);
        nk_spacing(ctx, 1);
        for (int c = 0; c < BS_CAT_COUNT; c++) {
            char buf[64];
            int n = 0;
            for (int i = 0; i < m->n_items; i++)
                if (m->items[i].cat == (bs_cat)c) n++;
            if (!n) continue;
            snprintf(buf, sizeof(buf), "%s: %d", bs_cat_nombre((bs_cat)c), n);
            nk_layout_row_dynamic(ctx, 16, 1);
            nk_label_colored(ctx, buf, NK_TEXT_LEFT, C_TEXT_DIM);
        }
        /* pie */
        nk_layout_row_dynamic(ctx, 10, 1);
        nk_spacing(ctx, 1);
        nk_layout_row_dynamic(ctx, 16, 1);
        nk_label_colored(ctx, "MIT License", NK_TEXT_LEFT, C_TEXT_DIM);
        nk_layout_row_dynamic(ctx, 16, 1);
        nk_label_colored(ctx, "github.com/JonaDevnet", NK_TEXT_LEFT, C_TEXT_DIM);
    }
    nk_end(ctx);
}

static void tab_resumen(struct nk_context *ctx, bs_model *m)
{
    char buf[200];
    struct nk_color col = C_TEXT_DIM;

    nk_layout_row_dynamic(ctx, 26, 1);
    if (m->puntaje >= 0) {
        col = m->puntaje >= 80 ? C_OK : (m->puntaje >= 55 ? C_WARN : C_BAD);
        snprintf(buf, sizeof(buf), "Puntaje de arranque: %d / 100   %s",
                 m->puntaje, m->resumen);
    } else {
        snprintf(buf, sizeof(buf), "Sin datos. Presiona \"Escanear\" para analizar el equipo.");
    }
    nk_label_colored(ctx, buf, NK_TEXT_LEFT, col);

    nk_layout_row_begin(ctx, NK_DYNAMIC, 112, 4);
    tarjeta(ctx, "Firmware",
            (m->boot.valido && m->boot.firmware_s > 0) ? "medido" : "--",
            "UEFI / POST", C_WARN);
    tarjeta(ctx, "Windows",
            (m->boot.valido && m->boot.main_path_s > 0) ? "medido" : "--",
            "kernel a escritorio", C_WARN);
    tarjeta(ctx, "Inicio rapido",
            !m->boot.valido ? "--" : (m->boot.fast_startup_ok ? "OK" : "roto"),
            m->boot.fast_startup_err[0] ? m->boot.fast_startup_err : "-",
            !m->boot.valido ? C_TEXT_DIM : (m->boot.fast_startup_ok ? C_OK : C_BAD));
    tarjeta(ctx, "Total", "--", "estimado", C_TEXT_DIM);
    nk_layout_row_end(ctx);

    nk_layout_row_dynamic(ctx, 10, 1);
    nk_spacing(ctx, 1);

    nk_layout_row_dynamic(ctx, 22, 1);
    nk_label_colored(ctx, "Hallazgos principales", NK_TEXT_LEFT, C_TEXT);
    cabecera_tabla(ctx);
    if (nk_group_begin(ctx, "hallazgos", NK_WINDOW_BORDER)) {
        int mostrados = 0;
        for (int i = 0; i < m->n_items && mostrados < 12; i++) {
            bs_item *it = &m->items[i];
            if (it->riesgo >= BS_RIESGO_MODERADO) {
                fila_item(ctx, m, it, i);
                mostrados++;
            }
        }
        if (!mostrados) {
            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label_colored(ctx, "Nada critico detectado (o sin escanear).",
                             NK_TEXT_LEFT, C_TEXT_DIM);
        }
        nk_group_end(ctx);
    }
}

static void tab_categoria(struct nk_context *ctx, bs_model *m, int cat, bs_ui *ui)
{
    char buf[128];
    int n = 0;
    for (int i = 0; i < m->n_items; i++)
        if (m->items[i].cat == (bs_cat)(cat - 1)) n++;

    snprintf(buf, sizeof(buf), "%s  (%d componentes)", TABS[cat], n);
    nk_layout_row_dynamic(ctx, 24, 1);
    nk_label_colored(ctx, buf, NK_TEXT_LEFT, C_TEXT);

    cabecera_tabla(ctx);
    if (nk_group_begin(ctx, "tabla_cat", NK_WINDOW_BORDER)) {
        for (int i = 0; i < m->n_items; i++) {
            if (m->items[i].cat != (bs_cat)(cat - 1)) continue;
            fila_item(ctx, m, &m->items[i], i);
        }
        if (!n) {
            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label_colored(ctx, "Sin elementos en esta categoria.",
                             NK_TEXT_LEFT, C_TEXT_DIM);
        }
        nk_group_end(ctx);
    }

    if (ui->tab == cat && n > 0) {
        nk_layout_row_dynamic(ctx, 8, 1);
        nk_spacing(ctx, 1);
        nk_layout_row_begin(ctx, NK_DYNAMIC, 24, 3);
        nk_layout_row_push(ctx, 0.22f);
        if (nk_button_label(ctx, "Seleccionar todo")) {
            for (int i = 0; i < m->n_items; i++)
                if (m->items[i].cat == (bs_cat)(cat - 1) &&
                    m->items[i].accion != BS_ACT_NINGUNA)
                    m->items[i].seleccionado = 1;
        }
        nk_layout_row_push(ctx, 0.22f);
        if (nk_button_label(ctx, "Nada")) {
            for (int i = 0; i < m->n_items; i++)
                if (m->items[i].cat == (bs_cat)(cat - 1))
                    m->items[i].seleccionado = 0;
        }
        nk_layout_row_push(ctx, 0.56f);
        nk_label(ctx, "", NK_TEXT_LEFT);
        nk_layout_row_end(ctx);
    }
}

static void panel_main(struct nk_context *ctx, bs_model *m, bs_ui *ui)
{
    if (nk_begin(ctx, "main", nk_rect(BS_SIDEBAR, BS_HEADER, BS_WIN_W - BS_SIDEBAR,
                                      BS_WIN_H - BS_HEADER),
                 NK_WINDOW_BACKGROUND)) {
        if (ui->tab == BS_TAB_RESUMEN)
            tab_resumen(ctx, m);
        else
            tab_categoria(ctx, m, ui->tab, ui);

        if (ui->mensaje[0]) {
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label_colored(ctx, ui->mensaje, NK_TEXT_LEFT,
                             ui->mensaje_error ? C_BAD : C_OK);
        }
    }
    nk_end(ctx);
}

void ui_draw(struct nk_context *ctx, bs_model *m, bs_ui *ui)
{
    panel_header(ctx, m, ui);
    panel_sidebar(ctx, m, ui);
    panel_main(ctx, m, ui);
}

