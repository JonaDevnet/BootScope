# Seguridad

## Alcance

BootScope es una herramienta **local** de diagnóstico y ajuste para Windows.
Requiere privilegios de administrador para leer SMART, el evento de rendimiento
de arranque y para aplicar cambios en servicios y autoinicios.

## Qué hace con tus datos

- **No envía nada a Internet.** No hay telemetría, ni cuentas, ni actualizador
  automático.
- El reporte JSON se escribe donde vos indiques y, por defecto, **oculta IPs y
  el nombre del equipo**. Revisalo antes de compartirlo: puede contener nombres
  de servicios y rutas de programas instalados.
- Los respaldos (`bootscope-backup-*.txt`) quedan junto al ejecutable y
  contienen solo nombres de servicios/autoinicios y su estado previo.

## Modelo de confianza

- Sin administrador, la app **solo lee** y muestra datos parciales.
- Con administrador, nunca aplica nada sin que marques ítems y confirmes.
- Lista negra: Defender, firewall, RPC/DCOM, `LSM`, `SamSs`, `gpsvc`, `CryptSvc`,
  `EventLog` y los servicios de arranque del sistema no se pueden cambiar.
- Antes de aplicar cambios de riesgo alto se intenta crear un punto de
  restauración y siempre se escribe un respaldo reversible.

## Reportar una vulnerabilidad

Escribí a **jonathan.rinaldi03@gmail.com** con:

- versión (`bootscope.exe --version`),
- sistema operativo,
- descripción del problema y pasos para reproducirlo.

No abras un issue público hasta que se haya publicado una corrección.

## Falsos positivos de antivirus

Un ejecutable que deshabilita servicios suele disparar heurísticas de PUA. Si te
pasa:

- verificá el SHA256 publicado en cada release,
- compilá desde el código fuente,
- reportá el falso positivo al fabricante del antivirus mencionando el repositorio.

Nunca se recomienda agregar exclusiones permanentes sin revisar el código.
