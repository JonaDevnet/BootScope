/*
 * BootScope - analisis: convierte mediciones en hallazgos
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "../app.h"
#include "../sys/util.h"
#include <stdio.h>
#include <string.h>

static bs_item *hallazgo(bs_model *m, bs_cat cat, bs_riesgo riesgo,
                         const char *check, const char *titulo, const char *detalle,
                         const char *metrica)
{
    bs_item *it = bs_add_item(m);
    if (!it) return NULL;
    it->cat = cat;
    it->riesgo = riesgo;
    it->accion = BS_ACT_NINGUNA;
    snprintf(it->check_id, sizeof(it->check_id), "%s", check);
    snprintf(it->titulo, sizeof(it->titulo), "%s", titulo);
    snprintf(it->detalle, sizeof(it->detalle), "%s", detalle);
    snprintf(it->metrica, sizeof(it->metrica), "%s", metrica ? metrica : "");
    return it;
}

void checks_analyze(bs_model *m)
{
    char buf[512];
    double win_s = m->boot.main_path_s > 0 ? m->boot.main_path_s : m->boot.kernel_desktop_s;

    /* ---------- arranque ---------- */
    if (!m->boot.fast_startup_known) {
        /* sin acceso al evento 29: no inventamos estado */
    } else if (!m->boot.fast_startup_ok) {
        bs_item *it = hallazgo(m, BS_CAT_ARRANQUE, BS_RIESGO_ALTO, "boot.faststartup",
                               "Inicio rapido roto",
                               "Windows no pudo restaurar hiberfil.sys y hace un arranque "
                               "completo en frio cada vez. Se arregla con: powercfg /h off, "
                               "apagar, encender y powercfg /h on.",
                               m->boot.fast_startup_err);
        if (it) {
            snprintf(it->estado, sizeof(it->estado), "Roto");
            snprintf(it->prev, sizeof(it->prev), "%s", m->boot.fast_startup_err);
        }
    } else if (m->boot.valido) {
        bs_item *it = hallazgo(m, BS_CAT_ARRANQUE, BS_RIESGO_NULO, "boot.faststartup.ok",
                               "Inicio rapido", "Operativo: Windows reutiliza la sesion guardada.",
                               "OK");
        if (it) snprintf(it->estado, sizeof(it->estado), "OK");
    }

    if (m->boot.firmware_s > 8.0) {
        snprintf(buf, sizeof(buf),
                 "La UEFI tarda %.1f s antes de cargar Windows (lo normal es 3 a 6 s). "
                 "Causas habituales: BIOS antiguo, modulos de RAM distintos o discos SATA "
                 "que se inicializan de mas. Comparar con la seccion Hardware.",
                 m->boot.firmware_s);
        hallazgo(m, BS_CAT_ARRANQUE,
                 m->boot.firmware_s > 15.0 ? BS_RIESGO_ALTO : BS_RIESGO_MODERADO,
                 "boot.firmware", "Firmware / POST lento", buf, "UEFI");
    }

    if (win_s > 15.0) {
        snprintf(buf, sizeof(buf),
                 "Windows tarda %.1f s desde el kernel hasta el escritorio (lo normal es 8 a 13 s).",
                 win_s);
        hallazgo(m, BS_CAT_ARRANQUE,
                 win_s > 25.0 ? BS_RIESGO_ALTO : BS_RIESGO_MODERADO,
                 "boot.mainpath", "Fase de Windows lenta", buf, "kernel");
    }

    if (m->boot.apagado_sucio) {
        hallazgo(m, BS_CAT_ARRANQUE, BS_RIESGO_MODERADO, "boot.apagado",
                 "Apagado no limpio",
                 "El arranque anterior termino de golpe (corte de luz o apagado forzado). "
                 "Eso invalida el Inicio rapido y puede danar el sistema de archivos.",
                 "6008");
    }

    if (m->boot.drivers_fallidos[0]) {
        snprintf(buf, sizeof(buf),
                 "Windows no cargo estos controladores de arranque: %s. Si son de un "
                 "antivirus o disco virtual desinstalado, es inofensivo.",
                 m->boot.drivers_fallidos);
        hallazgo(m, BS_CAT_ARRANQUE, BS_RIESGO_NULO, "boot.drivers",
                 "Controladores no cargados", buf, "7026");
    }

    /* ---------- sistema ---------- */
    if (m->vbs) {
        hallazgo(m, BS_CAT_ARRANQUE, BS_RIESGO_NULO, "boot.vbs",
                 "VBS / HVCI activo",
                 "La seguridad basada en virtualizacion agrega unos segundos al arranque y "
                 "algunos por ciento de rendimiento. Es una proteccion real: no se toca.",
                 "on");
    }

    /* ---------- hardware ---------- */
    if (m->n_discos > 0) {
        for (int i = 0; i < m->n_discos; i++) {
            bs_disk *d = &m->discos[i];
            if (!d->sistema) continue;
            if (bs_ci_contains(d->bus, "NVMe"))
                hallazgo(m, BS_CAT_HARDWARE, BS_RIESGO_NULO, "hw.disco.sistema",
                         "Windows en NVMe", d->modelo, "OK");
            else
                hallazgo(m, BS_CAT_HARDWARE, BS_RIESGO_MODERADO, "hw.disco.sistema",
                         "Windows no esta en el NVMe",
                         "El sistema arranca desde un disco mas lento que el NVMe disponible.",
                         d->bus);
        }
    }
}
