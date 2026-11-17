/*
 * BootScope - colectores del sistema
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#ifndef BS_SYS_H
#define BS_SYS_H

#include "../app.h"

void osinfo_collect(bs_model *m);
void boot_collect(bs_model *m);
void scm_collect(bs_model *m);
void startup_collect(bs_model *m);
void net_collect(bs_model *m);
void remote_collect(bs_model *m);
void hw_collect(bs_model *m);

#endif /* BS_SYS_H */
