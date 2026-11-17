# 0 "tv.c"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "tv.c"
# 1 "src/app.h" 1
# 26 "src/app.h"
typedef enum {
    BS_CAT_ARRANQUE = 0,
    BS_CAT_SERVICIOS,
    BS_CAT_INICIO,
    BS_CAT_RED,
    BS_CAT_HARDWARE,
    BS_CAT_COUNT
} bs_cat;


typedef enum {
    BS_RIESGO_NULO = 0,
    BS_RIESGO_SEGURO,
    BS_RIESGO_MODERADO,
    BS_RIESGO_ALTO
} bs_riesgo;


typedef enum {
    BS_ACT_NINGUNA = 0,
    BS_ACT_SERVICIO_MANUAL,
    BS_ACT_SERVICIO_DESHABILITAR,
    BS_ACT_SERVICIO_DETENER,
    BS_ACT_AUTOINICIO_DESHABILITAR,
    BS_ACT_PROCESO_TERMINAR,
    BS_ACT_RDP_DESHABILITAR,
    BS_ACT_INFO
} bs_accion;

typedef struct {
    int id;
    bs_cat cat;
    bs_riesgo riesgo;
    bs_accion accion;
    char check_id[48];
    char titulo[160];
    char detalle[512];
    char estado[48];
    char metrica[48];
    char edad[32];
    char target[300];
    char prev[80];
    int seleccionado;
    int recomendado;
    int aplicado;
    char resultado[160];
} bs_item;


typedef struct {
    int valido;
    double firmware_s;
    double main_path_s;
    double kernel_desktop_s;
    int fast_startup_ok;
    int fast_startup_known;
    char fast_startup_err[32];
    int apagado_sucio;
    char drivers_fallidos[160];
} bs_boot;

typedef struct {
    int pid;
    char proceso[64];
    char local[48];
    char remoto[48];
    char estado[24];
    int escucha;
} bs_conn;

typedef struct {
    int puerto;
    int pid;
    char proceso[64];
    char direccion[48];
    char exposicion[24];
} bs_port;

typedef struct {
    char modelo[64];
    char bus[16];
    char tipo[8];
    char estado[24];
    char salud[96];
    char edad[48];
    unsigned long long bytes;
    int sistema;
} bs_disk;

typedef struct {
    char slot[32];
    char modulo[64];
    int gb;
    int mhz;
    int mhz_nominal;
} bs_dimm;

typedef struct {
    char version[32];
    char fecha[16];
    char placa[96];
} bs_bios;

typedef struct {
    int puntaje;
    int puntaje_cat[BS_CAT_COUNT];
    char resumen[256];
    int admin;
    int escaneado;
    char escaneo_hora[32];
    char os_nombre[96];
    char os_build[32];
    int vbs;

    bs_boot boot;
    bs_bios bios;
    bs_dimm dimms[8];
    int n_dimms;
    bs_disk discos[16];
    int n_discos;
    bs_conn conexiones[256];
    int n_conexiones;
    bs_port puertos[128];
    int n_puertos;

    bs_item items[512];
    int n_items;
} bs_model;

const char *bs_version(void);
const char *bs_cat_nombre(bs_cat c);
const char *bs_riesgo_nombre(bs_riesgo r);
void bs_model_init(bs_model *m);
bs_item *bs_add_item(bs_model *m);
# 2 "tv.c" 2
const char *pv = "0" "." "2" "." "0";
