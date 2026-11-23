# BootScope

**Analizador de arranque y estado para Windows 11/10, escrito en C.**

BootScope mide dónde se va el tiempo cuando encendés la PC, inventaría servicios,
autoinicios, accesos remotos y hardware, y explica cada hallazgo con datos
medidos. Interfaz nativa Win32 (Nuklear sobre GDI)

> Estado: **v0.3.0 — análisis, inventario y acciones con respaldo y reversión.**

## Fases

| Fase | Contenido | Estado |
|------|-----------|--------|
| 0 | Esqueleto Win32 + Nuklear, build y licencia | ✅ |
| 1 | Colectores (arranque, servicios, inicio, red, hardware) + JSON | ✅ |
| 2 | Dashboard, timeline e inventario con datos reales | ✅ |
| 3 | Motor de acciones: respaldo, aplicar, verificar, deshacer | ✅ |
| 4 | Recomendador, documentación y pulido de release | ⬜ |

---

## Qué mide

| Área | Datos |
|------|-------|
| **Arranque** | Tiempo de firmware/UEFI (`Kernel-Boot` 30), fase de Windows (`Diagnostics-Performance` 100 o, si no hay acceso, el arranque real de `explorer.exe`), estado del Inicio rápido (`Kernel-Boot` 29 y su código de error), apagados sucios (41/6008) y controladores que no cargaron (7026). |
| **Servicios** | Todos los servicios en inicio Automático con estado, tipo, retraso y ruta. Los de terceros se pueden pasar a Manual; los de Windows y los de seguridad quedan bloqueados. |
| **Inicio** | Claves `Run`/`RunOnce` (HKCU/HKLM/32 bits), carpetas Inicio y estado real según `StartupApproved`. |
| **Red y remoto** | Puertos en escucha, conexiones establecidas, RDP, Chrome Remote Desktop, TeamViewer/AnyDesk/RustDesk/VNC/ScreenConnect y Asistencia remota. |
| **Hardware** | BIOS y placa por SMBIOS, módulos de RAM (tamaño, velocidad, disparidad) y discos con tamaño, bus, medio y salud SMART NVMe. |

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

Requiere CMake 3.16+ y un compilador C11. Nuklear está vendorizado en `vendor/`,
sin dependencias externas.

## Uso

```bat
bootscope.exe                      :: interfaz grafica
bootscope.exe --scan               :: resumen por consola
bootscope.exe --report salida.json :: reporte JSON (redactado por defecto)
bootscope.exe --report salida.json --no-redact
bootscope.exe --version
```

El reporte JSON es el formato pensado para adjuntar a un issue: oculta IPs y
datos del equipo salvo que se pida lo contrario.

## Cómo se usa

La app tiene tres momentos: **medir → marcar → aplicar**.

1. **Escanear** (arriba a la izquierda): analiza el equipo y arma el inventario.
2. **Marcar**: hacé clic sobre el **nombre de una fila** o sobre su casilla. Las
   filas con `-` no se pueden cambiar (son de Windows o solo informativas) y la
   columna **"Al aplicar"** dice exactamente qué va a pasar: *pasar a Manual*,
   *deshabilitar*, *quitar del inicio*, *bloquear RDP* o *solo aviso*.
3. **Aplicar (N)**: ejecuta **únicamente lo marcado**. Antes escribe un respaldo
   y después verifica cada cambio releyendo el estado real.
4. **Deshacer** revierte el último respaldo de la sesión.

**Recomendados (N)** no aplica nada: solo **marca** los ítems que la app
considera seguros de cambiar, según el perfil de la barra lateral.

| Perfil | Qué marca |
|---|---|
| Conservador | solo riesgo bajo (pasar servicios a Manual) |
| Balanceado | + accesos remotos (Chrome Remote Desktop, etc.) |
| Agresivo | todo lo accionable |

Después de pulsarlo, revisá la selección y recién ahí **Aplicar**.

| Riesgo | Ejemplo | Protección |
|--------|---------|------------|
| Seguro | servicio Auto → Manual, quitar autoinicio | va al plan |
| Moderado | deshabilitar un servicio de acceso remoto | confirmación |
| Alto | bloquear RDP, deshabilitar Chrome Remote Desktop | confirmación + punto de restauración |

Nunca se ofrecen Defender, el firewall, RPC/DCOM ni los servicios críticos.

## Seguridad

- La herramienta **solo lee** hasta que apretás *Aplicar* con ítems seleccionados.
- Cada acción debe poder deshacerse; si un chequeo no implementa reversión, se
  muestra como *solo aviso* y nunca se aplica solo.
- Nunca se ofrecen Defender, el firewall, RPC/DCOM ni los servicios críticos.

## Limitaciones conocidas

- **SMART en discos SATA** no está disponible en todas las controladoras: si el
  driver rechaza el IOCTL, la app lo informa como *no disponible* en vez de
  inventar valores. El SMART NVMe sí funciona.
- El tiempo de firmware es el que reporta UEFI; no incluye el logo de la placa
  anterior al handoff.
- Sin privilegios de administrador se omiten SMART, el evento 100 de rendimiento
  y algunos servicios protegidos.

## Pruebas

```bat
build\bootscope.exe --selftest
```

Simula clics sobre la interfaz y verifica pestañas, checkbox, panel de detalle,
los tres perfiles, **Escanear**, **Exportar** y **Seleccionar todo**. Deja el
resultado en `%TEMP%\bootscope-selftest.log` y devuelve como código de salida la
cantidad de fallos (0 = todo bien).

Variables de diagnóstico:

- `BS_DEBUG=1` registra los mensajes de entrada y el estado de cada frame en
  `%TEMP%\bootscope-input.log`.
- `BS_TAB=0..5` abre directamente una pestaña.

### Laboratorio descartable

Para probar cambios destructivos (borrar servicios, matar procesos, tocar RDP)
sin riesgo está `lab\sandbox.wsb`: un entorno de **Windows Sandbox** sin red que
corre la batería de pruebas y deja los resultados en `lab\resultados\`.

Ver [docs/laboratorio.md](docs/laboratorio.md).

## Documentación

- [Catálogo de chequeos](docs/checks.md): qué detecta cada `check_id`, su riesgo y si es reversible.
- [Arquitectura](docs/arquitectura.md): módulos y decisiones de diseño.
- [Cómo contribuir](CONTRIBUTING.md).
- [Seguridad](SECURITY.md).

## Licencia

MIT — ver [LICENSE](LICENSE).

Hecho por [JonaDevnet](https://github.com/JonaDevnet).
