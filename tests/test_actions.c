/*
 * Prueba del motor de acciones: pone un servicio en Manual y lo revierte.
 * Uso: test_actions.exe <nombre_servicio> <tipo_previo>
 * Requiere administrador.
 */
#include "../src/core/actions.h"
#include "../src/sys/util.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    bs_model m;
    bs_item *it;
    char msg[256];
    int r;

    if (argc < 3) {
        printf("uso: test_actions <servicio> <tipo_previo>\n");
        return 2;
    }
    bs_model_init(&m);
    it = bs_add_item(&m);
    if (!it) return 3;
    it->cat = BS_CAT_SERVICIOS;
    it->accion = BS_ACT_SERVICIO_MANUAL;
    it->seleccionado = 1;
    it->riesgo = BS_RIESGO_SEGURO;
    snprintf(it->target, sizeof(it->target), "%s", argv[1]);
    snprintf(it->prev, sizeof(it->prev), "%s", argv[2]);

    printf("admin=%d\n", bs_is_admin());
    r = actions_apply(&m, msg, sizeof(msg));
    printf("apply=%d  msg=[%s]  item=[%s]  aplicado=%d\n", r, msg, it->resultado, it->aplicado);
    printf("respaldo=%s\n", actions_last_backup());
    r = actions_undo(msg, sizeof(msg));
    printf("undo=%d  msg=[%s]\n", r, msg);
    return 0;
}
