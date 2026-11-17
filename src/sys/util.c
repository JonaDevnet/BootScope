/*
 * BootScope - utilidades Win32 compartidas
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int bs_is_admin(void)
{
    HANDLE tok = NULL;
    TOKEN_ELEVATION el;
    DWORD sz = sizeof(el);
    int admin = 0;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok))
        return 0;
    if (GetTokenInformation(tok, TokenElevation, &el, sz, &sz))
        admin = el.TokenIsElevated ? 1 : 0;
    CloseHandle(tok);
    return admin;
}

void bs_u8(const wchar_t *w, char *dst, size_t dstsz)
{
    int n;
    if (!dst || dstsz == 0) return;
    dst[0] = 0;
    if (!w) return;
    n = WideCharToMultiByte(CP_UTF8, 0, w, -1, dst, (int)dstsz - 1, NULL, NULL);
    if (n <= 0) dst[0] = 0;
    else dst[dstsz - 1] = 0;
}

void bs_w(const char *u8, wchar_t *dst, size_t dstsz)
{
    int n;
    if (!dst || dstsz == 0) return;
    dst[0] = 0;
    if (!u8) return;
    n = MultiByteToWideChar(CP_UTF8, 0, u8, -1, dst, (int)dstsz - 1);
    if (n <= 0) dst[0] = 0;
    else dst[dstsz - 1] = 0;
}

int bs_reg_str(HKEY root, const wchar_t *sub, const wchar_t *name,
               char *out, size_t outsz)
{
    HKEY k;
    DWORD type = 0, sz = 0;
    wchar_t *buf;
    LONG r;

    if (out && outsz) out[0] = 0;
    if (!out || outsz == 0) return 0;
    if (RegOpenKeyExW(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return 0;
    r = RegQueryValueExW(k, name, NULL, &type, NULL, &sz);
    if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || sz < 2) {
        RegCloseKey(k);
        return 0;
    }
    buf = (wchar_t *)calloc(sz + sizeof(wchar_t), 1);
    if (!buf) { RegCloseKey(k); return 0; }
    r = RegQueryValueExW(k, name, NULL, &type, (LPBYTE)buf, &sz);
    RegCloseKey(k);
    if (r != ERROR_SUCCESS) { free(buf); return 0; }

    if (type == REG_EXPAND_SZ) {
        wchar_t *exp = NULL;
        DWORD need = ExpandEnvironmentStringsW(buf, NULL, 0);
        if (need > 0) {
            exp = (wchar_t *)calloc(need + 1, sizeof(wchar_t));
            if (exp) {
                ExpandEnvironmentStringsW(buf, exp, need);
                bs_u8(exp, out, outsz);
                free(exp);
                free(buf);
                return 1;
            }
        }
    }
    bs_u8(buf, out, outsz);
    free(buf);
    return 1;
}

int bs_reg_dword(HKEY root, const wchar_t *sub, const wchar_t *name, DWORD *out)
{
    HKEY k;
    DWORD type = 0, sz = sizeof(DWORD), val = 0;
    LONG r;

    if (!out) return 0;
    if (RegOpenKeyExW(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return 0;
    r = RegQueryValueExW(k, name, NULL, &type, (LPBYTE)&val, &sz);
    RegCloseKey(k);
    if (r != ERROR_SUCCESS || type != REG_DWORD) return 0;
    *out = val;
    return 1;
}

int bs_reg_exists(HKEY root, const wchar_t *sub)
{
    HKEY k;
    int ok = 0;
    if (RegOpenKeyExW(root, sub, 0, KEY_READ, &k) == ERROR_SUCCESS) {
        ok = 1;
        RegCloseKey(k);
    }
    return ok;
}

void bs_now(char *out, size_t outsz)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(out, outsz, "%02d/%02d/%04d %02d:%02d:%02d",
             st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);
}

void bs_fmt_hours(char *out, size_t outsz, unsigned long long horas)
{
    if (horas > 0)
        snprintf(out, outsz, "%llu h (%.1f anos)", horas, (double)horas / 8760.0);
    else
        snprintf(out, outsz, "-");
}

int bs_file_exists(const wchar_t *path)
{
    DWORD a = GetFileAttributesW(path);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
}

int bs_dir_exists(const wchar_t *path)
{
    DWORD a = GetFileAttributesW(path);
    return (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
}

void bs_trim(char *s)
{
    size_t n;
    char *p = s;
    if (!s) return;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n'))
        s[--n] = 0;
}

int bs_ci_contains(const char *hay, const char *needle)
{
    size_t nh, nn, i, j;
    if (!hay || !needle) return 0;
    nh = strlen(hay); nn = strlen(needle);
    if (nn == 0) return 1;
    if (nh < nn) return 0;
    for (i = 0; i + nn <= nh; i++) {
        for (j = 0; j < nn; j++) {
            char a = hay[i + j], b = needle[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
        }
        if (j == nn) return 1;
    }
    return 0;
}

unsigned long long bs_uptime_s(void)
{
    return (unsigned long long)(GetTickCount64() / 1000ULL);
}

void bs_debug(const char *nombre, const char *texto)
{
    char ruta[MAX_PATH];
    char tmp[MAX_PATH];
    FILE *f;
    DWORD n;

    if (!getenv("BS_DEBUG") || !nombre || !texto) return;
    n = GetTempPathA(MAX_PATH, tmp);
    if (!n) return;
    snprintf(ruta, sizeof(ruta), "%sbootscope-%s.log", tmp, nombre);
    f = fopen(ruta, "wb");
    if (!f) return;
    fwrite(texto, 1, strlen(texto), f);
    fclose(f);
}
