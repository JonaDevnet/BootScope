# Catálogo de chequeos

Cada fila es un `check_id` que puede aparecer en el reporte JSON y en la
interfaz. `Reversible` indica si el motor de acciones puede deshacerlo.

## Arranque

| check_id | Qué detecta | Riesgo | Acción | Reversible |
|---|---|---|---|---|
| `boot.faststartup` | El Inicio rápido falló (`Kernel-Boot` 29). Windows hace un arranque completo cada vez. | alto | aviso (requiere `powercfg /h off` + apagar) | — |
| `boot.faststartup.ok` | Inicio rápido operativo | informativo | — | — |
| `boot.firmware` | UEFI tarda más de 8 s antes de cargar Windows (`Kernel-Boot` 30) | moderado/alto | aviso (BIOS, RAM, discos) | — |
| `boot.mainpath` | Fase de Windows mayor a 15 s (`Diagnostics-Performance` 100 o tiempo de `explorer.exe`) | moderado/alto | aviso | — |
| `boot.apagado` | Arranque anterior sin cierre limpio (`6008`/`41`) | moderado | aviso | — |
| `boot.drivers` | Controladores de arranque que no cargaron (`7026`) | informativo | — | — |
| `boot.vbs` | VBS/HVCI activo (cuesta algunos segundos) | informativo | — | — |

## Servicios

| check_id | Qué detecta | Riesgo | Acción | Reversible |
|---|---|---|---|---|
| `svc.pesado` | Servicio de terceros en Automático de la lista conocida (SQL Server, WSL, VMs de apps, gaming, actualizadores) | seguro | pasar a Manual | sí |
| `svc.auto` | Cualquier otro servicio de terceros en Automático | seguro | pasar a Manual | sí |

Los servicios de Windows y los protegidos se muestran como *protegido* y no
tienen acción.

## Inicio

| check_id | Qué detecta | Acción | Reversible |
|---|---|---|---|
| `inicio.HKCU` | Valor en `HKCU\...\Run` habilitado | deshabilitar en `StartupApproved` | sí |
| `inicio.HKCU-RunOnce` | `HKCU\...\RunOnce` | ídem | sí |
| `inicio.HKLM` | `HKLM\...\Run` | ídem | sí |
| `inicio.HKLM32` | `HKLM\...\WOW6432Node\...\Run` | ídem (Run32) | sí |
| `inicio.HKLM-RunOnce` | `HKLM\...\RunOnce` | ídem | sí |
| `inicio.Carpeta usuario` | Accesos directos en la carpeta Inicio del usuario | ídem (StartupFolder) | sí |
| `inicio.Carpeta comun` | Carpeta Inicio de todos los usuarios | ídem | sí |

## Red y acceso remoto

| check_id | Qué detecta | Riesgo | Acción | Reversible |
|---|---|---|---|---|
| `remoto.rdp` | `fDenyTSConnections = 0` (RDP aceptando conexiones) | alto | bloquear RDP | sí |
| `remoto.rdp.off` | RDP deshabilitado | informativo | — | — |
| `remoto.canal` | Chrome Remote Desktop, TeamViewer, AnyDesk, RustDesk, VNC, ScreenConnect, Splashtop | alto | deshabilitar el servicio | sí |
| `remoto.asistencia` | Asistencia remota de Windows habilitada | moderado | aviso | — |
| `red.puerto` | Puerto escuchando fuera de loopback por un proceso no-sistema | seguro/moderado | aviso | — |
| `red.conexion` | Conexión TCP establecida (agrupada por proceso y destino) | informativo | — | — |

## Hardware

| check_id | Qué detecta | Riesgo | Acción | Reversible |
|---|---|---|---|---|
| `hw.bios` | Versión y fecha de BIOS por SMBIOS; marca si tiene 4 años o más | moderado | aviso | — |
| `hw.ram` | Módulos instalados; marca si son dispares en tamaño | moderado | aviso | — |
| `hw.disco` | Modelo, bus, tamaño y salud SMART (NVMe); horas y ciclos | informativo/moderado | aviso | — |
| `hw.disco.sistema` | Si Windows arranca desde NVMe o desde un disco más lento | informativo/moderado | aviso | — |

## Puntaje

Cada ítem descuenta puntos de su categoría: alto 15, moderado 7, seguro 2.
El total es el promedio ponderado: arranque 40 %, servicios 25 %, inicio 10 %,
red 15 %, hardware 10 %. Un ítem *protegido* o informativo no descuenta.
