# BootScope

**Analizador de arranque y estado para Windows 11/10, escrito en C.**

BootScope mide dónde se va el tiempo cuando encendés la PC, inventaría servicios,
autoinicios, accesos remotos y hardware, y explica cada hallazgo con datos
medidos. Interfaz nativa Win32 (Nuklear sobre GDI), sin .NET, sin Python y sin
instalador: un solo `.exe`.

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

## Acciones

1. Marcá con el checkbox los ítems de la tabla (o usá **Recomendados**).
2. El botón **Recomendados** respeta el perfil elegido en la barra lateral:
   *Conservador* (solo seguro), *Balanceado* (seguro + moderado) o *Agresivo*
   (incluye alto).
3. **Aplicar** ejecuta solo lo seleccionado y escribe antes un respaldo
   `bootscope-backup-<fecha>.txt` junto al `.exe`.
4. Cada acción se **verifica** releyendo el estado real: si el cambio no se
   confirma, el ítem queda marcado en rojo.
5. **Deshacer** revierte el último respaldo de esa sesión.

| Riesgo | Ejemplo | Protección |
|--------|---------|------------|
| Seguro | servicio Auto → Manual, deshabilitar autoinicio | va al plan |
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

## Documentación

- [Catálogo de chequeos](docs/checks.md): qué detecta cada `check_id`, su riesgo y si es reversible.
- [Arquitectura](docs/arquitectura.md): módulos y decisiones de diseño.
- [Cómo contribuir](CONTRIBUTING.md).
- [Seguridad](SECURITY.md).

## Licencia

MIT — ver [LICENSE](LICENSE).

Hecho por [JonaDevnet](https://github.com/JonaDevnet).
