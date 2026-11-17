/*
 * BootScope - modelo de datos compartido
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#ifndef BS_APP_H
#define BS_APP_H

#ifndef BS_VERSION
#  if defined(BS_VER_MAJOR)
#    define BS_STR2(x) #x
#    define BS_STR(x) BS_STR2(x)
#    define BS_VERSION BS_STR(BS_VER_MAJOR) "." BS_STR(BS_VER_MINOR) "." BS_STR(BS_VER_PATCH)
#  else
#    define BS_VERSION "0.0.1"
#  endif
#endif

#define BS_MAX_ITEMS   512
#define BS_MAX_CONN    256
#define BS_MAX_PORTS   128
#define BS_MAX_DISKS    16
#define BS_MAX_DIMMS     8

/* Categorias visibles en la UI */
typedef enum {
    BS_CAT_ARRANQUE = 0,
    BS_CAT_SERVICIOS,
    BS_CAT_INICIO,
    BS_CAT_RED,
    BS_CAT_HARDWARE,
    BS_CAT_COUNT
} bs_cat;

/* Nivel de riesgo de aplicar una accion */
typedef enum {
    BS_RIESGO_NULO = 0,   /* solo informativo, no se aplica */
    BS_RIESGO_SEGURO,     /* reversible y sin impacto visible */
    BS_RIESGO_MODERADO,   /* reversible, requiere explicacion */
    BS_RIESGO_ALTO        /* destructivo o desinstalacion: confirmacion extra */
} bs_riesgo;

/* Accion que el motor sabe ejecutar y deshacer */
typedef enum {
    BS_ACT_NINGUNA = 0,
    BS_ACT_SERVICIO_MANUAL,        /* target = nombre del servicio, prev = tipo inicio */
    BS_ACT_SERVICIO_DESHABILITAR,  /* target = nombre del servicio, prev = tipo inicio */
    BS_ACT_SERVICIO_DETENER,       /* target = nombre del servicio */
    BS_ACT_AUTOINICIO_DESHABILITAR,/* target = "raiz|clave|valor", prev = bytes previos */
    BS_ACT_PROCESO_TERMINAR,       /* target = pid */
    BS_ACT_RDP_DESHABILITAR,       /* target = "rdp", prev = fDenyTSConnections */
    BS_ACT_INFO                    /* no aplicable */
} bs_accion;

typedef struct {
    int         id;
    bs_cat      cat;
    bs_riesgo   riesgo;
    bs_accion   accion;
    char        check_id[48];   /* identificador estable, ej: "svc.mssql.manual" */
    char        titulo[160];
    char        detalle[512];
    char        estado[48];     /* Running / Stopped / Healthy / Activo ... */
    char        metrica[48];    /* impacto medido o dato destacado */
    char        edad[32];       /* antiguedad / vida estimada */
    char        target[300];    /* objeto real sobre el que actua */
    char        prev[80];       /* estado previo para poder deshacer */
    int         seleccionado;
    int         recomendado;    /* preseleccion del perfil Recomendado */
    int         aplicado;       /* 0 pendiente, 1 ok, -1 error */
    char        resultado[160]; /* mensaje post-aplicacion */
} bs_item;

/* Resumen del ultimo arranque medido */
typedef struct {
    int    valido;
    double firmware_s;      /* tiempo de firmware/UEFI (evento Kernel-Boot 30) */
    double main_path_s;     /* fase critica (Diagnostics-Performance 100) */
    double kernel_desktop_s;/* kernel -> escritorio */
    int    fast_startup_ok; /* 0 = roto, 1 = ok */
    int    fast_startup_known; /* 1 si pudimos leer el evento 29 */
    char   fast_startup_err[32];
    int    apagado_sucio;
    char   drivers_fallidos[160];
} bs_boot;

typedef struct {
    int  pid;
    char proceso[64];
    char local[48];
    char remoto[48];
    char estado[24];
    int  escucha;
} bs_conn;

typedef struct {
    int  puerto;
    int  pid;
    char proceso[64];
    char direccion[48];
    char exposicion[24];  /* "LAN", "solo local", "todas" */
} bs_port;

typedef struct {
    char modelo[64];
    char bus[16];        /* NVMe / SATA */
    char tipo[8];        /* HDD / SSD */
    char estado[24];
    char salud[96];      /* temperatura, % usado, errores */
    char edad[48];       /* horas encendido, ciclos */
    unsigned long long bytes;
    int  sistema;        /* 1 si contiene la unidad del sistema */
} bs_disk;

typedef struct {
    char slot[32];
    char modulo[64];
    int  gb;
    int  mhz;
    int  mhz_nominal;
} bs_dimm;

typedef struct {
    char version[32];
    char fecha[16];
    char placa[96];
} bs_bios;

typedef struct {
    int     puntaje;                     /* 0..100 */
    int     puntaje_cat[BS_CAT_COUNT];
    char    resumen[256];
    int     admin;
    int     escaneado;
    char    escaneo_hora[32];
    char    os_nombre[96];
    char    os_build[32];
    int     vbs;                         /* VBS/HVCI activo */

    bs_boot  boot;
    bs_bios  bios;
    bs_dimm  dimms[BS_MAX_DIMMS];
    int      n_dimms;
    bs_disk  discos[BS_MAX_DISKS];
    int      n_discos;
    bs_conn  conexiones[BS_MAX_CONN];
    int      n_conexiones;
    bs_port  puertos[BS_MAX_PORTS];
    int      n_puertos;

    bs_item  items[BS_MAX_ITEMS];
    int      n_items;
} bs_model;

const char *bs_version(void);
const char *bs_cat_nombre(bs_cat c);
const char *bs_riesgo_nombre(bs_riesgo r);
void        bs_model_init(bs_model *m);
bs_item    *bs_add_item(bs_model *m);

#endif /* BS_APP_H */
