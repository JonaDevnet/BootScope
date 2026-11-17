# BootScope

**Analizador de arranque y estado para Windows 11/10, escrito en C.**

BootScope mide dónde se va el tiempo cuando encendés la PC, inventaría servicios,
autoinicios, accesos remotos y hardware, y explica cada hallazgo con datos
medidos. Interfaz nativa Win32 (Nuklear sobre GDI), sin .NET, sin Python y sin
instalador: un solo `.exe`.

> Estado: **v0.2.0 — dashboard e inventario funcionando.** El motor de acciones
> llega en la próxima fase.

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

## Licencia

MIT — ver [LICENSE](LICENSE).

Hecho por [JonaDevnet](https://github.com/JonaDevnet).
