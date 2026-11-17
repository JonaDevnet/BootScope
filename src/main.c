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

static void imprimir_resumen(const bs_model *m)
{
    printf("BootScope %s\n", bs_version());
    printf("%s  %s\n", m->os_nombre, m->os_build);
    printf("Privilegios: %s\n\n", m->admin ? "administrador" : "usuario (datos limitados)");

    if (m->boot.valido) {
        double wins = m->boot.main_path_s > 0 ? m->boot.main_path_s : m->boot.kernel_desktop_s;
        printf("Arranque: firmware %.1f s + Windows %.1f s = %.1f s   (inicio rapido: %s)\n",
               m->boot.firmware_s, wins, m->boot.firmware_s + wins,
               !m->boot.fast_startup_known ? "sin dato" :
               (m->boot.fast_startup_ok ? "ok" :
               (m->boot.fast_startup_err[0] ? m->boot.fast_startup_err : "roto")));
    } else {
        printf("Arranque: sin mediciones disponibles\n");
    }

    printf("Puntaje: %d/100  (%s)\n\n", m->puntaje, m->resumen);

    printf("Hallazgos:\n");
    for (int i = 0; i < m->n_items; i++) {
        const bs_item *it = &m->items[i];
        if (it->riesgo < BS_RIESGO_MODERADO) continue;
        printf("  [%-9s] %-42s %s\n", bs_riesgo_nombre(it->riesgo),
               it->titulo, it->metrica);
        if (it->detalle[0]) printf("              %s\n", it->detalle);
    }
    printf("\n");
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
    if (g_ui.pedir_escaneo) {
        g_ui.pedir_escaneo = 0;
        bs_scan(&g_model);
        g_ui.fila_sel = 0;
        snprintf(g_ui.mensaje, sizeof(g_ui.mensaje), "Escaneo listo: %d componentes",
                 g_model.n_items);
        g_ui.mensaje_error = 0;
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
    }
    InvalidateRect(GetActiveWindow(), NULL, FALSE);
}

/* ------------------------------------------------------------------ */
static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        if (g_ctx) {
            ui_draw(g_ctx, &g_model, &g_ui);
            nk_gdi_render(nk_rgb(24, 25, 28));
            procesar_ui();
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

    if (g_ctx && nk_gdi_handle_event(hwnd, msg, wp, lp)) {
        if ((msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) ||
            (msg >= WM_KEYFIRST   && msg <= WM_KEYLAST))
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
        if (scan) imprimir_resumen(&m);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE prev, LPSTR cmdline, int show)
{
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    RECT rc;
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    (void)prev; (void)cmdline;

    if (argv && argc > 1) {
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
    bs_scan(&g_model);
    snprintf(g_ui.mensaje, sizeof(g_ui.mensaje), "%d componentes analizados", g_model.n_items);

    ShowWindow(hwnd, show);
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


