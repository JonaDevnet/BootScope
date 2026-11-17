/*
 * BootScope - motor de acciones
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#ifndef BS_ACTIONS_H
#define BS_ACTIONS_H

#include "../app.h"
#include <stddef.h>

/* Aplica los items seleccionados. Devuelve cuantos se aplicaron bien; -1 sin admin. */
int actions_apply(bs_model *m, char *msg, size_t msgsz);

/* Revierte el ultimo respaldo. Devuelve cuantos se revirtieron. */
int actions_undo(char *msg, size_t msgsz);

const char *actions_last_backup(void);

#endif /* BS_ACTIONS_H */
