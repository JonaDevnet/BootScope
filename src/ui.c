/*
 * BootScope - interfaz Nuklear sobre Win32/GDI
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "ui.h"
#include "sys/util.h"
#include <stdio.h>
#include <string.h>

#define BS_WIN_W 1280
#define BS_WIN_H 820
#define BS_SIDEBAR 196.0f
#define BS_HEADER  68.0f

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
    while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80) n--;
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
    if (bs_ci_contains(s, "Running") || bs_ci_contains(s, "Healthy") ||
        bs_ci_contains(s, "OK") || bs_ci_contains(s, "Activo") ||
        bs_ci_contains(s, "Habilitad"))
        return C_OK;
    if (bs_ci_contains(s, "Stopped") || bs_ci_contains(s, "Deshabilitad") ||
        bs_ci_contains(s, "solo aviso") || bs_ci_contains(s, "Nada"))
        return C_TEXT_DIM;
    return riesgo_color(r);
}

static struct nk_color puntaje_color(int p)
{
    if (p >= 80) return C_OK;
    if (p >= 55) return C_WARN;
    return C_BAD;
}

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
    t[NK_COLOR_SELECT_ACTIVE]           = nk_rgb(38, 62, 92);
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
    ctx->style.progress.normal         = nk_style_item_color(C_PANEL_2);
    ctx->style.progress.cursor_normal  = nk_style_item_color(C_ACCENT);
    ctx->style.progress.border_color   = C_BORDER;
}

/* ------------------------------------------------------------------ */
static void tarjeta(struct nk_context *ctx, const char *titulo, const char *valor,
                    const char *pie, struct nk_color col)
{
    nk_layout_row_push(ctx, 0.25f);
    nk_group_begin(ctx, titulo, NK_WINDOW_BORDER);
    nk_layout_row_dynamic(ctx, 16, 1);
    nk_label_colored(ctx, titulo, NK_TEXT_LEFT, C_TEXT_DIM);
    nk_layout_row_dynamic(ctx, 30, 1);
    nk_label_colored(ctx, valor, NK_TEXT_LEFT, col);
    nk_layout_row_dynamic(ctx, 32, 1);
    nk_label_wrap(ctx, pie);
    nk_group_end(ctx);
}

static void fase(struct nk_context *ctx, const char *nombre, double seg, double total)
{
    char txt[96];
    nk_size v;
    snprintf(txt, sizeof(txt), "%s", nombre);
    nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 3);
    nk_layout_row_push(ctx, 0.26f);
    nk_label(ctx, txt, NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.54f);
    v = (nk_size)(total > 0.01 ? (seg / total) * 100.0 : 0);
    nk_progress(ctx, &v, 100, nk_false);
    nk_layout_row_push(ctx, 0.20f);
    snprintf(txt, sizeof(txt), "%.1f s", seg);
    nk_label_colored(ctx, txt, NK_TEXT_RIGHT, seg > 10.0 ? C_WARN : C_TEXT);
    nk_layout_row_end(ctx);
}

/* ------------------------------------------------------------------ */
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

static void fila_item(struct nk_context *ctx, bs_model *m, bs_ui *ui, bs_item *it)
{
    char b1[240], b2[96];
    nk_bool sel = (ui->fila_sel == it->id) ? nk_true : nk_false;

    nk_layout_row_begin(ctx, NK_DYNAMIC, 26, 7);
    nk_layout_row_push(ctx, 0.040f);
    {
        nk_bool chk = it->seleccionado ? nk_true : nk_false;
        if (nk_checkbox_label(ctx, "", &chk))
            it->seleccionado = chk ? 1 : 0;
    }
    nk_layout_row_push(ctx, 0.300f);
    if (nk_selectable_label(ctx, clip(b1, sizeof(b1), it->titulo, 44), NK_TEXT_LEFT, &sel))
        ui->fila_sel = (ui->fila_sel == it->id) ? 0 : it->id;
    nk_layout_row_push(ctx, 0.130f);
    nk_label_colored(ctx, clip(b2, sizeof(b2), it->estado, 16), NK_TEXT_LEFT,
                     estado_color(it->estado, it->riesgo));
    nk_layout_row_push(ctx, 0.110f);
    nk_label(ctx, clip(b2, sizeof(b2), it->metrica, 12), NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.110f);
    nk_label(ctx, clip(b2, sizeof(b2), it->edad, 12), NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.120f);
    nk_label_colored(ctx, bs_riesgo_nombre(it->riesgo), NK_TEXT_LEFT,
                     riesgo_color(it->riesgo));
    nk_layout_row_push(ctx, 0.190f);
    if (it->aplicado == 1) {
        nk_label_colored(ctx, it->resultado[0] ? it->resultado : "aplicado",
                         NK_TEXT_LEFT, C_OK);
    } else if (it->aplicado < 0) {
        nk_label_colored(ctx, clip(b2, sizeof(b2), it->resultado, 22), NK_TEXT_LEFT, C_BAD);
    } else if (it->accion == BS_ACT_NINGUNA) {
        nk_label_colored(ctx, bs_ci_contains(it->resultado, "protegido") ? "protegido" : "solo aviso",
                         NK_TEXT_LEFT, C_TEXT_DIM);
    } else if (it->recomendado) {
        nk_label_colored(ctx, "recomendado", NK_TEXT_LEFT, C_ACCENT);
    } else {
        nk_label_colored(ctx, "aplicable", NK_TEXT_LEFT, C_TEXT_DIM);
    }
    nk_layout_row_end(ctx);
    (void)m;
}

static void panel_detalle(struct nk_context *ctx, bs_model *m, bs_ui *ui)
{
    bs_item *it = NULL;
    char b1[420];
    if (ui->fila_sel <= 0) return;
    for (int i = 0; i < m->n_items; i++)
        if (m->items[i].id == ui->fila_sel) { it = &m->items[i]; break; }
    if (!it) return;

    nk_layout_row_dynamic(ctx, 8, 1);
    nk_spacing(ctx, 1);
    nk_layout_row_dynamic(ctx, 128, 1);
    if (nk_group_begin(ctx, "detalle", NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label_colored(ctx, clip(b1, sizeof(b1), it->titulo, 90), NK_TEXT_LEFT, C_TEXT);
        nk_layout_row_dynamic(ctx, 18, 1);
        snprintf(b1, sizeof(b1), "%s  |  %s  |  riesgo %s  |  check %s",
                 bs_cat_nombre(it->cat), it->estado,
                 bs_riesgo_nombre(it->riesgo), it->check_id);
        nk_label_colored(ctx, b1, NK_TEXT_LEFT, C_TEXT_DIM);
        nk_layout_row_dynamic(ctx, 18, 1);
        nk_label_wrap(ctx, it->detalle[0] ? it->detalle : "(sin detalle)");
        if (it->target[0]) {
            nk_layout_row_dynamic(ctx, 18, 1);
            snprintf(b1, sizeof(b1), "objeto: %s", it->target);
            nk_label_colored(ctx, clip(b1, sizeof(b1), b1, 120), NK_TEXT_LEFT, C_TEXT_DIM);
        }
        nk_group_end(ctx);
    }
}

/* ------------------------------------------------------------------ */
static void panel_header(struct nk_context *ctx, bs_model *m, bs_ui *ui)
{
    char buf[160];
    int sel = 0, rec = 0;

    for (int i = 0; i < m->n_items; i++) {
        if (m->items[i].seleccionado && m->items[i].accion != BS_ACT_NINGUNA) sel++;
        if (m->items[i].recomendado && m->items[i].accion != BS_ACT_NINGUNA) rec++;
    }

    if (nk_begin(ctx, "header", nk_rect(0, 0, BS_WIN_W, BS_HEADER),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
        nk_layout_row_begin(ctx, NK_DYNAMIC, 30, 3);
        nk_layout_row_push(ctx, 0.34f);
        nk_label_colored(ctx, "BootScope", NK_TEXT_LEFT, C_TEXT);
        nk_layout_row_push(ctx, 0.38f);
        snprintf(buf, sizeof(buf), "v%s   %s   %s", bs_version(),
                 m->escaneado ? m->escaneo_hora : "sin escanear",
                 m->escaneado && m->puntaje >= 0 ? "" : "");
        nk_label_colored(ctx, buf, NK_TEXT_LEFT, C_TEXT_DIM);
        nk_layout_row_push(ctx, 0.28f);
        nk_label_colored(ctx, m->admin ? "ADMINISTRADOR" : "sin privilegios",
                         NK_TEXT_RIGHT, m->admin ? C_OK : C_WARN);
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_DYNAMIC, 26, 5);
        nk_layout_row_push(ctx, 0.16f);
        if (nk_button_label(ctx, "Escanear")) ui->pedir_escaneo = 1;
        nk_layout_row_push(ctx, 0.16f);
        snprintf(buf, sizeof(buf), "Recomendados (%d)", rec);
        if (nk_button_label(ctx, buf)) ui->pedir_recomendados = 1;
        nk_layout_row_push(ctx, 0.16f);
        snprintf(buf, sizeof(buf), "Aplicar (%d)", sel);
        if (nk_button_label(ctx, buf)) ui->pedir_aplicar = 1;
        nk_layout_row_push(ctx, 0.16f);
        if (nk_button_label(ctx, "Exportar JSON")) ui->pedir_exportar = 1;
        nk_layout_row_push(ctx, 0.36f);
        if (ui->mensaje[0])
            nk_label_colored(ctx, clip(buf, sizeof(buf), ui->mensaje, 60), NK_TEXT_RIGHT,
                             ui->mensaje_error ? C_BAD : C_OK);
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
        nk_layout_row_dynamic(ctx, 12, 1);
        nk_spacing(ctx, 1);
        for (int c = 0; c < BS_CAT_COUNT; c++) {
            static const char *corto[BS_CAT_COUNT] = {
                "Arranque", "Servicios", "Inicio", "Red", "Hardware"
            };
            char buf[64];
            int n = 0, malos = 0;
            for (int i = 0; i < m->n_items; i++) {
                if (m->items[i].cat != (bs_cat)c) continue;
                n++;
                if (m->items[i].riesgo >= BS_RIESGO_MODERADO) malos++;
            }
            if (!n) continue;
            if (malos)
                snprintf(buf, sizeof(buf), "%s: %d (%d)", corto[c], n, malos);
            else
                snprintf(buf, sizeof(buf), "%s: %d", corto[c], n);
            nk_layout_row_dynamic(ctx, 16, 1);
            nk_label_colored(ctx, buf, NK_TEXT_LEFT,
                             malos ? C_WARN : C_TEXT_DIM);
        }
        nk_layout_row_dynamic(ctx, 10, 1);
        nk_spacing(ctx, 1);
        nk_layout_row_dynamic(ctx, 16, 1);
        nk_label_colored(ctx, "MIT License", NK_TEXT_LEFT, C_TEXT_DIM);
        nk_layout_row_dynamic(ctx, 16, 1);
        nk_label_colored(ctx, "github.com/JonaDevnet", NK_TEXT_LEFT, C_TEXT_DIM);
    }
    nk_end(ctx);
}

static void tab_resumen(struct nk_context *ctx, bs_model *m, bs_ui *ui)
{
    char buf[240];
    char fw[32], win[32], tot[32], pie[96];
    double wins = m->boot.main_path_s > 0 ? m->boot.main_path_s : m->boot.kernel_desktop_s;

    nk_layout_row_dynamic(ctx, 26, 1);
    if (m->puntaje >= 0) {
        snprintf(buf, sizeof(buf), "Puntaje: %d / 100    %s", m->puntaje, m->resumen);
        nk_label_colored(ctx, buf, NK_TEXT_LEFT, puntaje_color(m->puntaje));
    } else {
        nk_label_colored(ctx, "Sin datos. Presiona \"Escanear\" para analizar el equipo.",
                         NK_TEXT_LEFT, C_TEXT_DIM);
    }

    if (m->boot.firmware_s > 0) snprintf(fw, sizeof(fw), "%.1f s", m->boot.firmware_s);
    else snprintf(fw, sizeof(fw), "--");
    if (wins > 0) snprintf(win, sizeof(win), "%.1f s", wins);
    else snprintf(win, sizeof(win), "--");
    if (m->boot.firmware_s > 0 || wins > 0)
        snprintf(tot, sizeof(tot), "%.1f s", m->boot.firmware_s + wins);
    else
        snprintf(tot, sizeof(tot), "--");
    snprintf(pie, sizeof(pie), "UEFI / POST");
    nk_layout_row_begin(ctx, NK_DYNAMIC, 112, 4);
    tarjeta(ctx, "Firmware", fw, pie,
            m->boot.firmware_s > 8.0 ? C_BAD : (m->boot.valido ? C_OK : C_TEXT_DIM));
    tarjeta(ctx, "Windows", win, "kernel a escritorio",
            m->boot.main_path_s > 15.0 ? C_BAD : (m->boot.valido ? C_OK : C_TEXT_DIM));
    tarjeta(ctx, "Inicio rapido",
            !m->boot.valido ? "--" : (m->boot.fast_startup_ok ? "OK" : "roto"),
            m->boot.fast_startup_err[0] ? m->boot.fast_startup_err : "sin evento 29",
            !m->boot.valido ? C_TEXT_DIM : (m->boot.fast_startup_ok ? C_OK : C_BAD));
    tarjeta(ctx, "Total medido", tot, "sin contar el logo de la placa",
            m->boot.valido ? puntaje_color(m->puntaje) : C_TEXT_DIM);
    nk_layout_row_end(ctx);

    if (m->boot.valido) {
        nk_layout_row_dynamic(ctx, 10, 1);
        nk_spacing(ctx, 1);
        nk_layout_row_dynamic(ctx, 110, 1);
        if (nk_group_begin(ctx, "timeline", NK_WINDOW_BORDER)) {
            double total = m->boot.firmware_s + wins;
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label_colored(ctx, "Donde se va el tiempo", NK_TEXT_LEFT, C_TEXT);
            fase(ctx, "Firmware / BIOS", m->boot.firmware_s, total);
            fase(ctx, "Windows", wins, total);
            nk_group_end(ctx);
        }
    }

    nk_layout_row_dynamic(ctx, 10, 1);
    nk_spacing(ctx, 1);
    nk_layout_row_dynamic(ctx, 22, 1);
    nk_label_colored(ctx, "Hallazgos principales", NK_TEXT_LEFT, C_TEXT);
    cabecera_tabla(ctx);
    nk_layout_row_dynamic(ctx, 230, 1);
    if (nk_group_begin(ctx, "hallazgos", NK_WINDOW_BORDER)) {
        int mostrados = 0;
        for (int i = 0; i < m->n_items && mostrados < 12; i++) {
            if (m->items[i].riesgo >= BS_RIESGO_MODERADO) {
                fila_item(ctx, m, ui, &m->items[i]);
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
    panel_detalle(ctx, m, ui);
}

static void tab_categoria(struct nk_context *ctx, bs_model *m, int tab, bs_ui *ui)
{
    char buf[128];
    int n = 0, malos = 0;
    bs_cat cat = (bs_cat)(tab - 1);

    for (int i = 0; i < m->n_items; i++) {
        if (m->items[i].cat != cat) continue;
        n++;
        if (m->items[i].riesgo >= BS_RIESGO_MODERADO) malos++;
    }

    snprintf(buf, sizeof(buf), "%s   %d componentes, %d a revisar", TABS[tab], n, malos);
    nk_layout_row_dynamic(ctx, 24, 1);
    nk_label_colored(ctx, buf, NK_TEXT_LEFT, C_TEXT);

    cabecera_tabla(ctx);
    nk_layout_row_dynamic(ctx, 330, 1);
    if (nk_group_begin(ctx, "tabla_cat", NK_WINDOW_BORDER)) {
        for (int i = 0; i < m->n_items; i++) {
            if (m->items[i].cat != cat) continue;
            fila_item(ctx, m, ui, &m->items[i]);
        }
        if (!n) {
            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label_colored(ctx, "Sin elementos en esta categoria.",
                             NK_TEXT_LEFT, C_TEXT_DIM);
        }
        nk_group_end(ctx);
    }

    nk_layout_row_begin(ctx, NK_DYNAMIC, 24, 4);
    nk_layout_row_push(ctx, 0.20f);
    if (nk_button_label(ctx, "Seleccionar todo")) {
        for (int i = 0; i < m->n_items; i++)
            if (m->items[i].cat == cat && m->items[i].accion != BS_ACT_NINGUNA)
                m->items[i].seleccionado = 1;
    }
    nk_layout_row_push(ctx, 0.16f);
    if (nk_button_label(ctx, "Nada")) {
        for (int i = 0; i < m->n_items; i++)
            if (m->items[i].cat == cat)
                m->items[i].seleccionado = 0;
    }
    nk_layout_row_push(ctx, 0.34f);
    nk_label(ctx, "", NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.30f);
    nk_label(ctx, "", NK_TEXT_LEFT);
    nk_layout_row_end(ctx);

    panel_detalle(ctx, m, ui);
}

static void panel_main(struct nk_context *ctx, bs_model *m, bs_ui *ui)
{
    if (nk_begin(ctx, "main", nk_rect(BS_SIDEBAR, BS_HEADER, BS_WIN_W - BS_SIDEBAR,
                                      BS_WIN_H - BS_HEADER),
                 NK_WINDOW_BACKGROUND)) {
        if (ui->tab == BS_TAB_RESUMEN)
            tab_resumen(ctx, m, ui);
        else
            tab_categoria(ctx, m, ui->tab, ui);
    }
    nk_end(ctx);
}

void ui_draw(struct nk_context *ctx, bs_model *m, bs_ui *ui)
{
    panel_header(ctx, m, ui);
    panel_sidebar(ctx, m, ui);
    panel_main(ctx, m, ui);
}





