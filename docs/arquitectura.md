# Arquitectura

```
src/
├─ main.c        ventana Win32, bucle de mensajes y CLI
├─ app.h/.c      modelo en memoria (items, boot, discos, red) y utilidades
├─ ui.h/.c       interfaz Nuklear: header, navegación, tablas y detalle
├─ nk_impl.c     única unidad de compilación de Nuklear + backend GDI
├─ sys/          colectores (solo lectura)
│   ├─ util.c    UTF-8, registro, admin, formatos
│   ├─ osinfo.c  versión de Windows, VBS/HVCI
│   ├─ boot.c    eventos de arranque + fallback con explorer.exe
│   ├─ scm.c     servicios en Automático (SCM)
│   ├─ startup.c Run/RunOnce, carpetas Inicio, StartupApproved
│   ├─ net.c     tablas TCP por PID, puertos en escucha
│   ├─ remote.c  RDP, Chrome Remote Desktop, VNC y similares
│   └─ hw.c      SMBIOS (BIOS, RAM) y discos con SMART NVMe
├─ core/
│   ├─ scan.c    orquesta el escaneo completo
│   ├─ checks.c  convierte mediciones en hallazgos
│   ├─ score.c   puntaje por categoría
│   ├─ report.c  export JSON con redacción
│   └─ actions.c aplicar/verificar/deshacer con respaldo
└─ tests/        prueba del motor de acciones
```

## Flujo

1. `bs_scan(&modelo)` limpia el modelo y llama a cada colector.
2. Cada colector agrega ítems con `bs_add_item()`. Un ítem es **inventario**
   (aunque esté sano) o un **hallazgo** (riesgo mayor a informativo).
3. `checks_analyze()` agrega los hallazgos que dependen de varias fuentes
   (por ejemplo, firmware lento + BIOS viejo).
4. `score_compute()` calcula el puntaje.
5. La UI dibuja el modelo; el reporte lo serializa.

## Decisiones

- **Sin dependencias**: Nuklear está vendorizado y el backend es GDI, así que no
  hace falta Direct3D ni runtime extra.
- **Datos honestos**: si una medición no está disponible (sin admin, driver sin
  soporte), se muestra *sin datos*; nunca un cero.
- **Todo reversible**: las acciones se registran en un respaldo de texto antes
  de ejecutarse y se verifican releyendo el estado.
- **Un hilo**: el escaneo tarda menos de un segundo en un equipo moderno. Si en
  el futuro algún colector se vuelve lento, se moverá a un hilo con cola de
  eventos sin cambiar el modelo.
- **UTF-8 de punta a punta**: las APIs `W` se convierten al entrar al modelo.

## Limitaciones

- SMART SATA depende del driver de la controladora; si rechaza el IOCTL se
  informa como no disponible.
- El tiempo de firmware no incluye el logo de la placa anterior al handoff de
  UEFI.
- El evento 100 de `Diagnostics-Performance` puede estar deshabilitado o el log
  lleno; en ese caso se usa el tiempo real de `explorer.exe`.
