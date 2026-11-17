# BootScope

**Analizador y optimizador de arranque para Windows 11/10, escrito en C.**

BootScope mide dónde se va el tiempo cuando encendés la PC, inventaria servicios,
autoinicios, accesos remotos y hardware, y permite deshabilitar lo que sobra con
respaldo y reversión. Interfaz nativa Win32 (Nuklear sobre GDI), sin .NET, sin
Python y sin instalador: un solo `.exe`.

> Estado: **v0.0.1 — esqueleto de la interfaz**. Todavía no analiza el sistema.

---

## Por qué existe

Un Windows 11 recién instalado en un NVMe puede tardar 60 segundos en estar
usable. Casi nunca es el disco: es firmware viejo, Inicio rápido roto, servicios
de desarrollo que arrancan solos y accesos remotos que nadie recuerda haber
dejado abiertos. BootScope nació de un caso real documentado con mediciones
(eventos `Kernel-Boot`, `Diagnostics-Performance`, SMART) y convierte ese
diagnóstico manual en una herramienta.

## Estado del proyecto

| Fase | Contenido | Estado |
|------|-----------|--------|
| 0 | Esqueleto Win32 + Nuklear, build, licencia | ✅ |
| 1 | Colectores (arranque, servicios, inicio, red, hardware) + reporte JSON | ⬜ |
| 2 | Dashboard e inventario con datos reales | ⬜ |
| 3 | Motor de acciones: plan, respaldo, aplicar, verificar, deshacer | ⬜ |
| 4 | Recomendador por perfiles, puntaje, historial y documentación | ⬜ |

## Compilar

### MinGW-w64 (lo más rápido)

```bat
build-mingw.bat
```

### CMake (MSVC o MinGW)

```bat
cmake -S . -B build
cmake --build build --config Release
```

Requiere CMake 3.16+ y un compilador C11. No hay dependencias externas: Nuklear
está vendorizado en `vendor/`.

## Uso

```bat
bootscope.exe              :: interfaz grafica
bootscope.exe --version
```

## Seguridad

- Por defecto la herramienta **solo lee**. Nada se modifica hasta que apretás
  *Aplicar* con ítems seleccionados.
- Toda acción debe poder deshacerse; si un chequeo no implementa reversión, se
  muestra como *solo aviso* y nunca se aplica automáticamente.
- Nunca se tocan Defender, el firewall, RPC/DCOM ni los servicios críticos.

## Licencia

MIT — ver [LICENSE](LICENSE).

Hecho por [JonaDevnet](https://github.com/JonaDevnet).
