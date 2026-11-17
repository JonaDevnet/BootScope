# Cómo contribuir

Gracias por querer mejorar BootScope. El proyecto busca ser chico, auditable y
seguro: **preferimos un chequeo menos a un chequeo que rompa una PC**.

## Compilar

```bat
build-mingw.bat
```

o con CMake:

```bat
cmake -S . -B build
cmake --build build --config Release
```

Requisitos: C11 y un compilador de Windows (MSVC o MinGW-w64). No se agregan
dependencias externas: lo que falte se vendoriza en `vendor/`.

## Estilo

- C11, sin extensiones de compilador.
- Indentación de 4 espacios, líneas de hasta 100 columnas.
- Nombres en español para lo que ve el usuario, en inglés para las APIs.
- Nada de `strcpy`/`sprintf`: siempre `snprintf` con tamaño.
- Los textos de la interfaz van en UTF-8 y se dibujan con `bs_u8`.

## Agregar un chequeo

1. Elegí la categoría: `arranque`, `servicios`, `inicio`, `red` o `hardware`.
2. En el colector correspondiente (`src/sys/*.c`), creá el ítem con
   `bs_add_item()` y llená `check_id`, `titulo`, `detalle`, `estado`, `metrica`,
   `edad`, `riesgo` y `accion`.
3. Si el ítem se puede arreglar, implementá la acción en `src/core/actions.c`
   y su reversión.
4. Documentalo en `docs/checks.md`.
5. Si es un hallazgo derivado (no un inventario), agregalo en
   `src/core/checks.c`.

### Reglas de seguridad (obligatorias)

- **Si un chequeo no implementa reversión, no puede tener acción**: se muestra
  como *solo aviso*.
- Nunca agregues como accionable: Defender, firewall, RDP/DCOM, `LSM`, `SamSs`,
  `gpsvc`, servicios de arranque ni nada firmado por el sistema.
- Los identificadores con `recomendado = 1` entran al perfil Balanceado: tienen
  que ser reversibles y de impacto bajo.
- Un ítem de riesgo alto necesita confirmación explícita del usuario y punto de
  restauración.

## Probar

```bat
build\test_actions.exe brave auto
```

Pone un servicio en Manual, verifica y lo revierte (requiere administrador).
Usalo con servicios descartables: `brave`, `edgeupdate`, `GoogleUpdater*`.

## Pull requests

- Un cambio por PR, con descripción de qué mide y cómo se probó.
- Si tocás la interfaz, adjuntá captura (con datos de ejemplo, sin datos
  personales).
- `-Wall -Wextra` sin advertencias nuevas en `src/`.
- Si agregás un chequeo, adjuntá el JSON de `--report` antes y después.

## Reportar un bug

Abrí un issue con el archivo que genera:

```bat
bootscope.exe --report bug.json --scan
```

El reporte ya viene redactado (sin IPs ni nombre del equipo) salvo que uses
`--no-redact`.
