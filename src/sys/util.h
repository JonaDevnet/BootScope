/*
 * BootScope - utilidades Win32 compartidas
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#ifndef BS_UTIL_H
#define BS_UTIL_H

#include <windows.h>
#include <stddef.h>

int  bs_is_admin(void);
void bs_u8(const wchar_t *w, char *dst, size_t dstsz);
void bs_w(const char *u8, wchar_t *dst, size_t dstsz);

int  bs_reg_str(HKEY root, const wchar_t *sub, const wchar_t *name,
                char *out, size_t outsz);
int  bs_reg_dword(HKEY root, const wchar_t *sub, const wchar_t *name, DWORD *out);
int  bs_reg_exists(HKEY root, const wchar_t *sub);

void bs_now(char *out, size_t outsz);
void bs_fmt_hours(char *out, size_t outsz, unsigned long long horas);
int  bs_file_exists(const wchar_t *path);
int  bs_dir_exists(const wchar_t *path);
void bs_trim(char *s);
int  bs_ci_contains(const char *hay, const char *needle);

/* tiempo desde el arranque, en segundos */
unsigned long long bs_uptime_s(void);

void bs_debug(const char *nombre, const char *texto);

#endif /* BS_UTIL_H */
