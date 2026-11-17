/*
 * BootScope - interfaces de analisis
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#ifndef BS_CHECKS_H
#define BS_CHECKS_H

#include "../app.h"

void checks_analyze(bs_model *m);
void score_compute(bs_model *m);
int  report_write(const bs_model *m, const char *ruta, int redactar);

#endif /* BS_CHECKS_H */
