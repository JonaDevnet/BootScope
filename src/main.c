/*
 * BootScope - entrada Win32
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "nuklear.h"
#include "nuklear_gdi.h"

#include "app.h"
#include "ui.h"

#define BS_WIN_W 1280
#define BS_WIN_H 820

static struct nk_context *g_ctx;
static GdiFont            *g_font;
static bs_model            g_model;
static bs_ui               g_ui;
static HDC                 g_dc;
static int                 g_cli = 0;

/* ------------------------------------------------------------------ */
static void cli_out(const char *texto)
{
    if (!g_cli) return;
    fputs(texto, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

static int revisar_cli(void)
{
    const char *cmd = GetCommandLineA();
    if (!cmd) return 1;

    if (strstr(cmd, "--help") || strstr(cmd, "-h")) {
        g_cli = 1;
        AttachConsole(ATTACH_PARENT_PROCESS);
        freopen("CONOUT$", "w", stdout);
        cli_out("BootScope " BS_VERSION " - analizador de arranque y estado de Windows");
        cli_out("");
        cli_out("Uso: bootscope.exe [opciones]");
        cli_out("  --version       muestra la version y sale");
        cli_out("  --help          esta ayuda");
        cli_out("");
        cli_out("Sin opciones abre la interfaz grafica.");
        return 0;
    }
    if (strstr(cmd, "--version") || strstr(cmd, "-v")) {
        g_cli = 1;
        AttachConsole(ATTACH_PARENT_PROCESS);
        freopen("CONOUT$", "w", stdout);
        cli_out("BootScope " BS_VERSION);
        return 0;
    }
    return 1;
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
        HDC dc = BeginPaint(hwnd, &ps);
        if (g_ctx) {
            ui_draw(g_ctx, &g_model, &g_ui);
            nk_gdi_render(nk_rgb(24, 25, 28));
        }
        EndPaint(hwnd, &ps);
        (void)dc;
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
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE prev, LPSTR cmdline, int show)
{
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    RECT rc;

    (void)prev; (void)cmdline;

    if (!revisar_cli()) return 0;

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
