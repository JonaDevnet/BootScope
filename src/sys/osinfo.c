/*
 * BootScope - informacion del sistema operativo
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "sys.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void osinfo_collect(bs_model *m)
{
    char prod[128] = "";
    char disp[64] = "";
    char build[32] = "";
    DWORD vbs = 0, hvci = 0;

    m->admin = bs_is_admin();

    bs_reg_str(HKEY_LOCAL_MACHINE,
               L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
               L"ProductName", prod, sizeof(prod));
    bs_reg_str(HKEY_LOCAL_MACHINE,
               L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
               L"DisplayVersion", disp, sizeof(disp));
    bs_reg_str(HKEY_LOCAL_MACHINE,
               L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
               L"CurrentBuildNumber", build, sizeof(build));

    {
        int b = atoi(build);
        const char *edition = prod;
        if (b >= 22000) {
            /* Windows 11 sigue reportando "Windows 10 Pro" en ProductName */
            const char *p = strstr(prod, "Windows 10 ");
            edition = p ? p + 11 : "Pro";
            snprintf(m->os_nombre, sizeof(m->os_nombre), "Windows 11 %s", edition);
        } else {
            snprintf(m->os_nombre, sizeof(m->os_nombre), "%s", prod[0] ? prod : "Windows");
        }
        snprintf(m->os_build, sizeof(m->os_build), "%s (%s)",
                 disp[0] ? disp : "?", build[0] ? build : "?");
    }

    if (bs_reg_dword(HKEY_LOCAL_MACHINE,
                     L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
                     L"EnableVirtualizationBasedSecurity", &vbs) && vbs)
        m->vbs = 1;
    if (bs_reg_dword(HKEY_LOCAL_MACHINE,
                     L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity",
                     L"Enabled", &hvci) && hvci)
        m->vbs = 1;
}
