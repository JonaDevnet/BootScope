/*
 * BootScope - entrada Win32 y CLI
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdarg.h>

#include "nuklear.h"
#include "nuklear_gdi.h"

#include "app.h"
#include "ui.h"
#include "core/scan.h"
#include "core/checks.h"
#include "core/actions.h"
#include "sys/util.h"

#define BS_WIN_W 1280
#define BS_WIN_H 820

static struct nk_context *g_ctx;
static GdiFont            *g_font;
static bs_model            g_model;
static bs_ui               g_ui;
static HDC                 g_dc;
static HWND                g_hwnd;

static int contar_seleccionados(void);

/* Cola de entrada: los mensajes se acumulan y se procesan dentro del frame,
 * entre nk_input_begin y nk_input_end (si se aplican fuera, Nuklear los pierde). */
typedef struct {
    UINT   msg;
    WPARAM wp;
    LPARAM lp;
} bs_event;

#define BS_MAX_EVENTOS 512
static bs_event g_eventos[BS_MAX_EVENTOS];
static int      g_n_eventos;

static int es_mensaje_entrada(UINT msg)
{
    switch (msg) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR:
        return 1;
    default:
        return 0;
    }
}

/* traza opcional de entrada: BS_DEBUG=1 escribe en %TEMP%\bootscope-input.log */
static void dbg_log(const char *fmt, ...)
{
    static FILE *f;
    static int intentado;
    va_list ap;

    if (!getenv("BS_DEBUG")) return;
    if (!intentado) {
        char tmp[MAX_PATH], ruta[MAX_PATH];
        intentado = 1;
        GetTempPathA(MAX_PATH, tmp);
        snprintf(ruta, sizeof(ruta), "%sbootscope-input.log", tmp);
        f = fopen(ruta, "wb");
    }
    if (!f) return;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fflush(f);
}

/* ------------------------------------------------------------------ */
static void ruta_reporte(char *dst, size_t dstsz)
{
    wchar_t exe[MAX_PATH];
    wchar_t *barra;
    char dir[MAX_PATH * 2];

    if (!GetModuleFileNameW(NULL, exe, MAX_PATH)) {
        snprintf(dst, dstsz, "bootscope-report.json");
        return;
    }
    barra = wcsrchr(exe, L'\\');
    if (barra) *barra = 0;
    bs_u8(exe, dir, sizeof(dir));
    snprintf(dst, dstsz, "%s\\bootscope-report.json", dir);
}

static void consola(void)
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    SetConsoleOutputCP(CP_UTF8);
}

static void imprimir_resumen(FILE *out, const bs_model *m)
{
    fprintf(out, "BootScope %s\n", bs_version());
    fprintf(out, "%s  %s\n", m->os_nombre, m->os_build);
    fprintf(out, "Privilegios: %s\n\n", m->admin ? "administrador" : "usuario (datos limitados)");

    if (m->boot.valido) {
        double wins = m->boot.main_path_s > 0 ? m->boot.main_path_s : m->boot.kernel_desktop_s;
        fprintf(out, "Arranque: firmware %.1f s + Windows %.1f s = %.1f s   (inicio rapido: %s)\n",
                m->boot.firmware_s, wins, m->boot.firmware_s + wins,
                !m->boot.fast_startup_known ? "sin dato" :
                (m->boot.fast_startup_ok ? "ok" :
                (m->boot.fast_startup_err[0] ? m->boot.fast_startup_err : "roto")));
    } else {
        fprintf(out, "Arranque: sin mediciones disponibles\n");
    }

    fprintf(out, "Puntaje: %d/100  (%s)\n\n", m->puntaje, m->resumen);

    fprintf(out, "Hallazgos:\n");
    for (int i = 0; i < m->n_items; i++) {
        const bs_item *it = &m->items[i];
        if (it->riesgo < BS_RIESGO_MODERADO) continue;
        fprintf(out, "  [%-9s] %-42s %s\n", bs_riesgo_nombre(it->riesgo),
                it->titulo, it->metrica);
        if (it->detalle[0]) fprintf(out, "              %s\n", it->detalle);
    }
    fprintf(out, "\n");
}

/* ------------------------------------------------------------------ */
static void relanzar_admin(void)
{
    wchar_t exe[MAX_PATH];
    if (!GetModuleFileNameW(NULL, exe, MAX_PATH)) return;
    ShellExecuteW(NULL, L"runas", exe, NULL, NULL, SW_SHOWNORMAL);
}

static void procesar_ui(void)
{
    int cambios = 0;
    if (g_ui.pedir_escaneo) {
        g_ui.pedir_escaneo = 0;
        bs_scan(&g_model);
        g_ui.fila_sel = 0;
        snprintf(g_ui.mensaje, sizeof(g_ui.mensaje), "Escaneo listo: %d componentes",
                 g_model.n_items);
        g_ui.mensaje_error = 0;
        cambios = 1;
    }
    if (g_ui.pedir_recomendados) {
        int n = 0;
        int cap = g_ui.perfil == 0 ? BS_RIESGO_SEGURO :
                  (g_ui.perfil == 1 ? BS_RIESGO_MODERADO : BS_RIESGO_ALTO);
        g_ui.pedir_recomendados = 0;
        for (int i = 0; i < g_model.n_items; i++) {
            bs_item *it = &g_model.items[i];
            if (it->recomendado && it->accion != BS_ACT_NINGUNA && it->riesgo <= cap) {
                it->seleccionado = 1;
                n++;
            }
        }
        snprintf(g_ui.mensaje, sizeof(g_ui.mensaje), "%d recomendados seleccionados (perfil %s)",
                 n, g_ui.perfil == 0 ? "conservador" :
                    (g_ui.perfil == 1 ? "balanceado" : "agresivo"));
        g_ui.mensaje_error = 0;
        cambios = 1;
    }
    if (g_ui.pedir_exportar) {
        char ruta[MAX_PATH * 2];
        g_ui.pedir_exportar = 0;
        ruta_reporte(ruta, sizeof(ruta));
        if (report_write(&g_model, ruta, 1))
            snprintf(g_ui.mensaje, sizeof(g_ui.mensaje), "Reporte guardado junto al .exe");
        else
            snprintf(g_ui.mensaje, sizeof(g_ui.mensaje), "No se pudo escribir el reporte");
        g_ui.mensaje_error = 0;
        cambios = 1;
    }
    if (g_ui.pedir_aplicar) {
        int n = 0;
        g_ui.pedir_aplicar = 0;
        for (int i = 0; i < g_model.n_items; i++)
            if (g_model.items[i].seleccionado && g_model.items[i].accion != BS_ACT_NINGUNA) n++;
        if (!n) {
            snprintf(g_ui.mensaje, sizeof(g_ui.mensaje), "No hay nada seleccionado");
            g_ui.mensaje_error = 1;
        } else if (!g_model.admin) {
            snprintf(g_ui.mensaje, sizeof(g_ui.mensaje),
                     "Se necesita administrador: relanzando...");
            g_ui.mensaje_error = 0;
            relanzar_admin();
            PostQuitMessage(0);
            return;
        } else {
            char msg[256];
            int r = actions_apply(&g_model, msg, sizeof(msg));
            bs_scan(&g_model);
            g_ui.fila_sel = 0;
            snprintf(g_ui.mensaje, sizeof(g_ui.mensaje), "%s", msg);
            g_ui.mensaje_error = (r < 0);
            cambios = 1;
        }
    }
    if (g_ui.pedir_deshacer) {
        char msg[256];
        g_ui.pedir_deshacer = 0;
        if (!g_model.admin) {
            snprintf(g_ui.mensaje, sizeof(g_ui.mensaje),
                     "Se necesita administrador: relanzando...");
            g_ui.mensaje_error = 0;
            relanzar_admin();
            PostQuitMessage(0);
            return;
        }
        actions_undo(msg, sizeof(msg));
        bs_scan(&g_model);
        g_ui.fila_sel = 0;
        snprintf(g_ui.mensaje, sizeof(g_ui.mensaje), "%s", msg);
        g_ui.mensaje_error = 0;
        cambios = 1;
    }
    if (cambios)
        InvalidateRect(g_hwnd, NULL, FALSE);
}

/* ------------------------------------------------------------------ */
static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    dbg_log("WND msg=%u\n", msg);
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        nk_gdi_set_dc(ps.hdc);
        dbg_log("PAINT eventos=%d\n", g_n_eventos);        if (g_ctx) {
            int consumidos = 0;
            nk_input_begin(g_ctx);
            while (consumidos < g_n_eventos) {
                bs_event *e = &g_eventos[consumidos];
                nk_gdi_handle_event(hwnd, e->msg, e->wp, e->lp);
                consumidos++;
                /* un "down" necesita su propio frame para que Nuklear lo vea
                   como clic; el resto de la cola se procesa en el siguiente. */
                if (e->msg == WM_LBUTTONDOWN || e->msg == WM_RBUTTONDOWN ||
                    e->msg == WM_MBUTTONDOWN || e->msg == WM_KEYDOWN)
                    break;
            }
            if (consumidos > 0) {
                memmove(g_eventos, g_eventos + consumidos,
                        (size_t)(g_n_eventos - consumidos) * sizeof(bs_event));
                g_n_eventos -= consumidos;
            }
            nk_input_end(g_ctx);

            ui_draw(g_ctx, &g_model, &g_ui);
            nk_gdi_render(nk_rgb(24, 25, 28));
            dbg_log("FRAME tab=%d fila=%d eventos=%d sel=%d\n", g_ui.tab, g_ui.fila_sel,
                    g_n_eventos, contar_seleccionados());
            procesar_ui();

            if (g_n_eventos > 0)
                InvalidateRect(hwnd, NULL, FALSE);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        if (g_ctx) InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    if (g_ctx && es_mensaje_entrada(msg)) {
        dbg_log("MSG %u wp=%u lp=%ld\n", msg, (unsigned)wp, (long)lp);
        if (g_n_eventos < BS_MAX_EVENTOS) {
            g_eventos[g_n_eventos].msg = msg;
            g_eventos[g_n_eventos].wp = wp;
            g_eventos[g_n_eventos].lp = lp;
            g_n_eventos++;
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ------------------------------------------------------------------ */
static int modo_consola(int argc, wchar_t **argv)
{
    int i;
    const wchar_t *report = NULL;
    int scan = 0, salir = 0, redact = 1;

    for (i = 1; i < argc; i++) {
        if (!wcscmp(argv[i], L"--help") || !wcscmp(argv[i], L"-h")) {
            consola();
            printf("BootScope %s - analizador de arranque y estado de Windows\n\n", bs_version());
            printf("Uso: bootscope.exe [opciones]\n");
            printf("  --scan            analiza e imprime un resumen\n");
            printf("  --report [ruta]   analiza y escribe un JSON (por defecto junto al .exe)\n");
            printf("  --no-redact       no oculta IPs ni datos del equipo en el JSON\n");
            printf("  --version         version\n");
            printf("  --help            esta ayuda\n\n");
            printf("Sin opciones abre la interfaz grafica.\n");
            return 1;
        }
        if (!wcscmp(argv[i], L"--version") || !wcscmp(argv[i], L"-v")) {
            consola();
            printf("BootScope %s\n", bs_version());
            return 1;
        }
        if (!wcscmp(argv[i], L"--scan")) { scan = 1; salir = 1; }
        if (!wcscmp(argv[i], L"--report")) {
            salir = 1;
            if (i + 1 < argc && argv[i + 1][0] != L'-') report = argv[++i];
        }
        if (!wcscmp(argv[i], L"--no-redact")) redact = 0;
    }

    if (salir) {
        bs_model m;
        char ruta[MAX_PATH * 2];
        consola();
        bs_scan(&m);
        if (report) {
            bs_u8(report, ruta, sizeof(ruta));
        } else {
            ruta_reporte(ruta, sizeof(ruta));
        }
        if (report_write(&m, ruta, redact))
            printf("Reporte JSON: %s\n", ruta);
        else
            printf("ERROR: no se pudo escribir %s\n", ruta);
        if (scan) {
            char base[MAX_PATH * 2], resumen[MAX_PATH * 2];
            FILE *f;
            imprimir_resumen(stdout, &m);
            /* el .exe es subsistema GUI: cmd no espera, dejamos el resumen en disco */
            snprintf(base, sizeof(base), "%s", ruta);
            {
                char *punto = strrchr(base, '.');
                if (punto) *punto = 0;
            }
            snprintf(resumen, sizeof(resumen), "%s-resumen.txt", base);
            f = fopen(resumen, "wb");
            if (f) {
                imprimir_resumen(f, &m);
                fclose(f);
                printf("Resumen: %s\n", resumen);
            }
        }
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Test de interfaz: simula clics y verifica que los widgets respondan. */

static FILE *g_st_log;

static void st_log(const char *fmt, ...)
{
    va_list ap;
    if (g_st_log) {
        va_start(ap, fmt);
        vfprintf(g_st_log, fmt, ap);
        va_end(ap);
        fflush(g_st_log);
    }
}

static void frame_clic(int x, int y)
{
    /* press: Nuklear registra el clic mientras el boton esta presionado */
    nk_input_begin(g_ctx);
    nk_input_motion(g_ctx, x, y);
    nk_input_button(g_ctx, NK_BUTTON_LEFT, x, y, 1);
    nk_input_end(g_ctx);
    ui_draw(g_ctx, &g_model, &g_ui);
    nk_clear(g_ctx);

    /* release: cierra el clic */
    nk_input_begin(g_ctx);
    nk_input_motion(g_ctx, x, y);
    nk_input_button(g_ctx, NK_BUTTON_LEFT, x, y, 0);
    nk_input_end(g_ctx);
    ui_draw(g_ctx, &g_model, &g_ui);
    nk_clear(g_ctx);
}

static int contar_seleccionados(void)
{
    int n = 0;
    for (int i = 0; i < g_model.n_items; i++)
        if (g_model.items[i].seleccionado) n++;
    return n;
}

static int selftest(void)
{
    char ruta[MAX_PATH], tmp[MAX_PATH];
    int fallos = 0, vistos[BS_TAB_COUNT];
    int x, y;

    GetTempPathA(MAX_PATH, tmp);
    snprintf(ruta, sizeof(ruta), "%sbootscope-selftest.log", tmp);
    g_st_log = fopen(ruta, "wb");
    st_log("BootScope %s - test de interfaz\n", bs_version());

    bs_scan(&g_model);
    g_ui.tab = BS_TAB_RESUMEN;
    g_ui.fila_sel = 0;
    st_log("componentes: %d\n", g_model.n_items);

    /* 1) las 6 pestanas deben poder seleccionarse con un clic */
    memset(vistos, 0, sizeof(vistos));
    for (y = 80; y <= 380; y += 2) {
        frame_clic(60, y);
        if (g_ui.tab >= 0 && g_ui.tab < BS_TAB_COUNT) vistos[g_ui.tab] = 1;
    }
    {
        int alcanzadas = 0;
        for (int i = 0; i < BS_TAB_COUNT; i++) {
            if (vistos[i]) alcanzadas++;
            else { st_log("FAIL: pestana %d no alcanzable\n", i); fallos++; }
        }
        st_log("pestanas alcanzables: %d/%d\n", alcanzadas, BS_TAB_COUNT);
    }

    /* 2) checkbox de la tabla */
    g_ui.tab = 2;              /* Servicios */
    g_ui.fila_sel = 0;
    for (int i = 0; i < g_model.n_items; i++) g_model.items[i].seleccionado = 0;
    frame_clic(60, 0);
    {
        int encontrado = 0;
        for (y = 140; y <= 180 && !encontrado; y += 2) {
            for (x = 200; x <= 260 && !encontrado; x += 2) {
                frame_clic(x, y);
                if (contar_seleccionados() > 0) encontrado = 1;
            }
        }
        if (encontrado) st_log("OK: checkbox selecciona\n");
        else { st_log("FAIL: checkbox no selecciona\n"); fallos++; }
    }

    /* 3) clic en el titulo abre el panel de detalle */
    {
        int abierto = 0;
        g_ui.fila_sel = 0;
        for (y = 140; y <= 175 && !abierto; y += 2) {
            frame_clic(420, y);
            if (g_ui.fila_sel != 0) abierto = 1;
        }
        if (abierto) st_log("OK: detalle se abre\n");
        else { st_log("FAIL: detalle no se abre\n"); fallos++; }
    }

    /* 4) boton Recomendados */
    g_ui.tab = BS_TAB_RESUMEN;
    g_model.items[0].seleccionado = 0;
    for (int i = 0; i < g_model.n_items; i++) g_model.items[i].seleccionado = 0;
    g_ui.pedir_recomendados = 0;
    frame_clic(280, 55);
    if (g_ui.pedir_recomendados) {
        procesar_ui();
        if (contar_seleccionados() > 0)
            st_log("OK: Recomendados selecciona %d\n", contar_seleccionados());
        else { st_log("FAIL: Recomendados no selecciono nada\n"); fallos++; }
    } else {
        st_log("FAIL: el clic no llego al boton Recomendados\n");
        fallos++;
    }

    /* 5) selector de perfil */
    {
        int vistos_p[3] = { 0, 0, 0 };
        for (y = 540; y <= 700; y += 4) {
            frame_clic(60, y);
            if (g_ui.perfil >= 0 && g_ui.perfil < 3) vistos_p[g_ui.perfil] = 1;
        }
        if (vistos_p[0] && vistos_p[1] && vistos_p[2])
            st_log("OK: los 3 perfiles son seleccionables\n");
        else {
            st_log("FAIL: perfiles conservador=%d balanceado=%d agresivo=%d\n",
                   vistos_p[0], vistos_p[1], vistos_p[2]);
            fallos++;
        }
        g_ui.perfil = 1;
    }

    /* 6) boton Escanear */
    g_ui.pedir_escaneo = 0;
    frame_clic(100, 55);
    if (g_ui.pedir_escaneo) {
        procesar_ui();
        st_log("OK: Escanear (%s)\n", g_ui.mensaje);
    } else { st_log("FAIL: el clic no llego a Escanear\n"); fallos++; }

    /* 7) boton Exportar */
    g_ui.pedir_exportar = 0;
    frame_clic(850, 55);
    if (g_ui.pedir_exportar) {
        procesar_ui();
        st_log("OK: Exportar (%s)\n", g_ui.mensaje);
    } else { st_log("FAIL: el clic no llego a Exportar\n"); fallos++; }

    /* 8) boton Seleccionar todo en Servicios */
    {
        int total_cat = 0, ok = 0;
        g_ui.tab = 2;
        for (int i = 0; i < g_model.n_items; i++) g_model.items[i].seleccionado = 0;
        for (int i = 0; i < g_model.n_items; i++)
            if (g_model.items[i].cat == BS_CAT_SERVICIOS &&
                g_model.items[i].accion != BS_ACT_NINGUNA) total_cat++;
        frame_clic(60, 0);
        for (y = 460; y <= 570 && !ok; y += 4) {
            frame_clic(330, y);
            if (contar_seleccionados() == total_cat && total_cat > 0) ok = 1;
        }
        if (ok) st_log("OK: Seleccionar todo (%d servicios)\n", total_cat);
        else { st_log("FAIL: Seleccionar todo no selecciono los %d\n", total_cat); fallos++; }
    }

    st_log("RESULTADO: %s (%d fallos)\n", fallos ? "FALLO" : "PASS", fallos);    if (g_st_log) { fclose(g_st_log); g_st_log = NULL; }
    return fallos;
}

/* ------------------------------------------------------------------ */
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE prev, LPSTR cmdline, int show)
{
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    RECT rc;
    int argc = 0;
    int selftest_flag = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    (void)prev; (void)cmdline;

    if (argv) {
        for (int i = 1; i < argc; i++)
            if (!wcscmp(argv[i], L"--selftest")) selftest_flag = 1;
    }

    if (argv && argc > 1 && !selftest_flag) {
        int salida = modo_consola(argc, argv);
        LocalFree(argv);
        if (salida) return 0;
    } else if (argv) {
        LocalFree(argv);
    }

    SetProcessDPIAware();

    memset(&wc, 0, sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = wndproc;
    wc.hInstance     = hinst;
    wc.hCursor       = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "BootScopeWnd";
    if (!RegisterClassA(&wc)) return 1;

    rc.left = 0; rc.top = 0; rc.right = BS_WIN_W; rc.bottom = BS_WIN_H;
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);

    hwnd = CreateWindowA("BootScopeWnd", "BootScope - analizador de arranque",
                         WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         rc.right - rc.left, rc.bottom - rc.top,
                         NULL, NULL, hinst, NULL);
    if (!hwnd) return 1;
    g_hwnd = hwnd;
    dbg_log("HWND=%p\n", (void *)hwnd);

    g_dc = GetDC(hwnd);
    g_font = nk_gdifont_create("Segoe UI", 15);
    if (!g_font) {
        MessageBoxA(hwnd, "No se pudo crear la fuente de la interfaz.",
                    "BootScope", MB_ICONERROR);
        return 1;
    }
    g_ctx = nk_gdi_init(g_font, g_dc, BS_WIN_W, BS_WIN_H);
    ui_init_style(g_ctx);

    bs_model_init(&g_model);
    memset(&g_ui, 0, sizeof(g_ui));
    g_ui.tab = BS_TAB_RESUMEN;
    g_ui.perfil = 1;   /* balanceado por defecto */
    {
        /* utilidad de pruebas: BS_TAB=0..5 abre directamente una pestana */
        const char *t = getenv("BS_TAB");
        if (t) {
            int v = atoi(t);
            if (v >= 0 && v < BS_TAB_COUNT) g_ui.tab = v;
        }
    }
    if (selftest_flag) {
        int fallos;
        ShowWindow(hwnd, SW_HIDE);
        fallos = selftest();
        nk_gdi_shutdown();
        nk_gdifont_del(g_font);
        ReleaseDC(hwnd, g_dc);
        DestroyWindow(hwnd);
        return fallos;
    }
    bs_scan(&g_model);
    snprintf(g_ui.mensaje, sizeof(g_ui.mensaje), "%d componentes analizados", g_model.n_items);

    ShowWindow(hwnd, show);
    SetForegroundWindow(hwnd);
    UpdateWindow(hwnd);

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    nk_gdi_shutdown();
    nk_gdifont_del(g_font);
    ReleaseDC(hwnd, g_dc);
    return 0;
}



