/*
 * BootScope - prueba del motor de acciones para el laboratorio
 *
 * Crea objetos de prueba, aplica un cambio con el mismo motor que usa la app,
 * verifica el resultado y lo deshace. Pensado para correr dentro de Windows
 * Sandbox o una VM: no debe usarse en un equipo de produccion.
 *
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "../src/core/actions.h"
#include "../src/sys/util.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SVC_NOMBRE L"BootScopeLabSvc"
#define RUN_VALOR  L"BootScopeLabRun"
#define RUN_CLAVE  L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define APROB_CLAVE L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run"

static int pruebas, fallos;

static void ok(const char *nombre)
{
    pruebas++;
    printf("OK    %s\n", nombre);
}

static void fallo(const char *nombre, const char *detalle)
{
    pruebas++;
    fallos++;
    printf("FALLO %s   (%s)\n", nombre, detalle);
}

/* ---------------- servicio ---------------- */
static DWORD servicio_inicio(void)
{
    SC_HANDLE scm, h;
    LPQUERY_SERVICE_CONFIGW cfg;
    DWORD need = 0, tipo = 0;

    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return 0xFFFFFFFF;
    h = OpenServiceW(scm, SVC_NOMBRE, SERVICE_QUERY_CONFIG);
    if (!h) { CloseServiceHandle(scm); return 0xFFFFFFFF; }
    QueryServiceConfigW(h, NULL, 0, &need);
    cfg = (LPQUERY_SERVICE_CONFIGW)calloc(need ? need : 1, 1);
    if (cfg && QueryServiceConfigW(h, cfg, need, &need))
        tipo = cfg->dwStartType;
    free(cfg);
    CloseServiceHandle(h);
    CloseServiceHandle(scm);
    return tipo;
}

static int servicio_crear(void)
{
    SC_HANDLE scm, h;
    int okk = 0;

    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scm) return 0;
    h = CreateServiceW(scm, SVC_NOMBRE, SVC_NOMBRE, SERVICE_ALL_ACCESS,
                       SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                       SERVICE_ERROR_IGNORE,
                       L"C:\\Windows\\System32\\cmd.exe /c exit",
                       NULL, NULL, NULL, NULL, NULL);
    if (h) {
        okk = 1;
        CloseServiceHandle(h);
    }
    CloseServiceHandle(scm);
    return okk;
}

static void servicio_borrar(void)
{
    SC_HANDLE scm, h;
    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return;
    h = OpenServiceW(scm, SVC_NOMBRE, DELETE);
    if (h) {
        DeleteService(h);
        CloseServiceHandle(h);
    }
    CloseServiceHandle(scm);
}

/* ---------------- autoinicio ---------------- */
static int run_crear(void)
{
    HKEY k;
    LONG r = RegCreateKeyExW(HKEY_CURRENT_USER, RUN_CLAVE, 0, NULL, 0,
                             KEY_SET_VALUE, NULL, &k, NULL);
    if (r != ERROR_SUCCESS) return 0;
    r = RegSetValueExW(k, RUN_VALOR, 0, REG_SZ,
                       (const BYTE *)L"cmd.exe /c exit", (DWORD)((wcslen(L"cmd.exe /c exit") + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

static void run_borrar(void)
{
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_CLAVE, 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) {
        RegDeleteValueW(k, RUN_VALOR);
        RegCloseKey(k);
    }
    if (RegOpenKeyExW(HKEY_CURRENT_USER, APROB_CLAVE, 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) {
        RegDeleteValueW(k, RUN_VALOR);
        RegCloseKey(k);
    }
}

static int aprob_primer_byte(void)
{
    HKEY k;
    DWORD tipo = 0, sz = 16;
    unsigned char buf[16];
    int v = -1;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, APROB_CLAVE, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return -1;
    if (RegQueryValueExW(k, RUN_VALOR, NULL, &tipo, buf, &sz) == ERROR_SUCCESS &&
        tipo == REG_BINARY && sz >= 1)
        v = buf[0];
    RegCloseKey(k);
    return v;
}

/* ---------------- RDP ---------------- */
static DWORD rdp_leer(void)
{
    DWORD v = 1;
    bs_reg_dword(HKEY_LOCAL_MACHINE,
                 L"SYSTEM\\CurrentControlSet\\Control\\Terminal Server",
                 L"fDenyTSConnections", &v);
    return v;
}

static void rdp_escribir(DWORD v)
{
    HKEY k;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SYSTEM\\CurrentControlSet\\Control\\Terminal Server",
                      0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) {
        RegSetValueExW(k, L"fDenyTSConnections", 0, REG_DWORD, (const BYTE *)&v, sizeof(v));
        RegCloseKey(k);
    }
}

/* ---------------- items ---------------- */
static bs_item *item_nuevo(bs_model *m, bs_accion accion, bs_riesgo riesgo,
                           const char *target, const char *prev)
{
    bs_item *it = bs_add_item(m);
    if (!it) return NULL;
    it->cat = BS_CAT_SERVICIOS;
    it->accion = accion;
    it->riesgo = riesgo;
    it->seleccionado = 1;
    snprintf(it->target, sizeof(it->target), "%s", target);
    snprintf(it->prev, sizeof(it->prev), "%s", prev);
    return it;
}

/* ---------------- pruebas ---------------- */
static void probar_servicio(void)
{
    bs_model m;
    bs_item *it;
    char msg[256];

    printf("\n[1/4] Servicio: Auto -> Manual -> Auto\n");
    if (!servicio_crear()) {
        fallo("crear servicio de prueba", "CreateService fallo (hace falta administrador)");
        return;
    }
    if (servicio_inicio() == SERVICE_AUTO_START) ok("servicio creado en Automatico");
    else fallo("servicio creado en Automatico", "estado inesperado");

    bs_model_init(&m);
    it = item_nuevo(&m, BS_ACT_SERVICIO_MANUAL, BS_RIESGO_SEGURO, "BootScopeLabSvc", "auto");
    if (!it) { fallo("item de servicio", "sin memoria"); servicio_borrar(); return; }

    if (actions_apply(&m, msg, sizeof(msg)) == 1) ok("aplicar servicio a Manual");
    else fallo("aplicar servicio a Manual", msg);

    if (servicio_inicio() == SERVICE_DEMAND_START) ok("verificado: Manual");
    else fallo("verificado: Manual", "sigue en Automatico");

    if (actions_undo(msg, sizeof(msg)) == 1) ok("deshacer servicio");
    else fallo("deshacer servicio", msg);

    if (servicio_inicio() == SERVICE_AUTO_START) ok("verificado: volvio a Automatico");
    else fallo("verificado: volvio a Automatico", "no revirtio");

    servicio_borrar();
}

static void probar_autoinicio(void)
{
    bs_model m;
    char target[400];
    char msg[256];
    int byte;

    printf("\n[2/4] Autoinicio: deshabilitar -> habilitar\n");
    if (!run_crear()) {
        fallo("crear valor Run de prueba", "no se pudo escribir HKCU");
        return;
    }
    ok("valor Run creado");

    bs_model_init(&m);
    snprintf(target, sizeof(target), "HKCU|%s|%s",
             "Software\\Microsoft\\Windows\\CurrentVersion\\Run", "BootScopeLabRun");
    if (!item_nuevo(&m, BS_ACT_AUTOINICIO_DESHABILITAR, BS_RIESGO_SEGURO, target, "02")) {
        fallo("item de autoinicio", "sin memoria");
        run_borrar();
        return;
    }

    if (actions_apply(&m, msg, sizeof(msg)) == 1) ok("aplicar deshabilitar autoinicio");
    else fallo("aplicar deshabilitar autoinicio", msg);

    byte = aprob_primer_byte();
    if (byte == 0x03) ok("verificado: StartupApproved = 0x03");
    else fallo("verificado: StartupApproved = 0x03", "byte distinto");

    if (actions_undo(msg, sizeof(msg)) == 1) ok("deshacer autoinicio");
    else fallo("deshacer autoinicio", msg);

    byte = aprob_primer_byte();
    if (byte == 0x02) ok("verificado: StartupApproved = 0x02");
    else fallo("verificado: StartupApproved = 0x02", "byte distinto");

    run_borrar();
}

static void probar_rdp(void)
{
    bs_model m;
    DWORD original = rdp_leer();
    char prev[8], msg[256];

    printf("\n[3/4] RDP: habilitado -> bloqueado -> restaurado\n");
    rdp_escribir(0);
    if (rdp_leer() == 0) ok("RDP habilitado como base");
    else fallo("RDP habilitado como base", "no se pudo poner en 0");

    snprintf(prev, sizeof(prev), "%lu", (unsigned long)original);
    bs_model_init(&m);
    /* riesgo moderado: evita el dialogo de confirmacion en modo headless */
    if (!item_nuevo(&m, BS_ACT_RDP_DESHABILITAR, BS_RIESGO_MODERADO, "rdp", prev)) {
        fallo("item RDP", "sin memoria");
        rdp_escribir(original);
        return;
    }

    if (actions_apply(&m, msg, sizeof(msg)) == 1) ok("aplicar bloqueo de RDP");
    else fallo("aplicar bloqueo de RDP", msg);

    if (rdp_leer() == 1) ok("verificado: fDenyTSConnections = 1");
    else fallo("verificado: fDenyTSConnections = 1", "sigue en 0");

    if (actions_undo(msg, sizeof(msg)) == 1) ok("deshacer bloqueo de RDP");
    else fallo("deshacer bloqueo de RDP", msg);

    if (rdp_leer() == original) ok("verificado: valor original restaurado");
    else fallo("verificado: valor original restaurado", "no coincide");

    rdp_escribir(original);
}

static void probar_respaldo(void)
{
    const char *ruta = actions_last_backup();
    FILE *f;
    long tam;

    printf("\n[4/4] Respaldo\n");
    if (!ruta || !ruta[0]) {
        fallo("respaldo creado", "no hay ruta");
        return;
    }
    f = fopen(ruta, "rb");
    if (!f) { fallo("respaldo legible", ruta); return; }
    fseek(f, 0, SEEK_END);
    tam = ftell(f);
    fclose(f);
    if (tam > 0) {
        ok("respaldo creado y no vacio");
        printf("      archivo: %s (%ld bytes)\n", ruta, tam);
    } else {
        fallo("respaldo creado y no vacio", "0 bytes");
    }
}

int main(void)
{
    const char *lab = getenv("BS_LAB");
    const char *usuario = getenv("USERNAME");

    printf("BootScope lab - motor de acciones\n");

    /* doble candado: solo corre en Windows Sandbox o con BS_LAB=1 explicito */
    if ((!lab || strcmp(lab, "1") != 0) &&
        (!usuario || _stricmp(usuario, "WDAGUtilityAccount") != 0)) {
        printf("\n  Este binario crea y borra servicios, toca autoinicios y RDP.\n");
        printf("  Solo debe correr dentro de Windows Sandbox o una VM.\n");
        printf("  Si es a proposito: defini la variable BS_LAB=1\n\n");
        return 3;
    }

    printf("admin=%d\n", bs_is_admin());
    if (!bs_is_admin()) {
        printf("RESULTADO: FALLO (se necesita administrador)\n");
        return 2;
    }

    probar_servicio();
    probar_autoinicio();
    probar_rdp();
    probar_respaldo();

    printf("\nRESULTADO: %s (%d pruebas, %d fallos)\n",
           fallos ? "FALLO" : "PASS", pruebas, fallos);
    return fallos;
}
