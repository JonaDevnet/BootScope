/*
 * BootScope - orquestador de escaneo
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "scan.h"
#include "checks.h"
#include "../sys/sys.h"
#include "../sys/util.h"
#include <string.h>

void bs_scan(bs_model *m)
{
    bs_model_init(m);

    osinfo_collect(m);
    boot_collect(m);
    scm_collect(m);
    startup_collect(m);
    net_collect(m);
    remote_collect(m);
    hw_collect(m);
    checks_analyze(m);
    score_compute(m);

    m->escaneado = 1;
    bs_now(m->escaneo_hora, sizeof(m->escaneo_hora));
}
