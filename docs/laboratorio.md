# Laboratorio descartable (Windows Sandbox)

Entorno para probar **cambios destructivos** de BootScope sin riesgo para el
equipo real: borrar servicios, desactivar autoinicios, tocar RDP, matar procesos
y romper el sistema a propósito.

## Requisitos

- Windows 11 **Pro** (o Enterprise/Education).
- Virtualización habilitada en BIOS (ya la usa WSL2/Hyper-V).
- La característica opcional **Windows Sandbox** habilitada:

```powershell
# como administrador, una sola vez
Enable-WindowsOptionalFeature -Online -FeatureName "Containers-DisposableClientVM" -All
# reiniciar
```

## Uso

1. Compilá en el host: `build-mingw.bat` (genera `build\bootscope.exe`,
   `build\test_lab.exe` y `build\test_actions.exe`).
2. Doble clic en `lab\sandbox.wsb`.
3. Esperá a que corra el script: deja los resultados en `lab\resultados\` y abre
   la app dentro del Sandbox para que la uses a mano.
4. En el host, leé los resultados:

```powershell
powershell -ExecutionPolicy Bypass -File lab\leer-resultados.ps1
```

## Qué prueba el script

| Paso | Qué hace | Criterio |
|---|---|---|
| 1 | Reporte inicial (`--report antes.json`) | JSON válido, admin = true |
| 2 | Servicio de prueba: Auto → Manual → Auto | `sc qc` coincide en cada paso |
| 3 | Autoinicio de prueba: deshabilitar → habilitar | `StartupApproved` 0x03 → 0x02 |
| 4 | RDP: habilitar → bloquear → restaurar | `fDenyTSConnections` 1 → 0 |
| 5 | Respaldo creado y no vacío | archivo `bootscope-backup-*.txt` |
| 6 | Test de interfaz (`--selftest`) | 8/8 |
| 7 | Reporte final (`--report despues.json`) | JSON válido |

## Límites del Sandbox

- **No se puede reiniciar**: no sirve para validar persistencia ni tiempos de
  arranque. Para eso hace falta una VM (Hyper-V) o el equipo real.
- El disco es virtual: la pestaña **Hardware** mostrará "sin datos" y no hay SMART.
- Sin red: la pestaña **Red** aparece vacía (a propósito).
- Todo cambio se descarta al cerrar la ventana. Esa es la gracia.
- No hay canal de comandos remoto: la comunicación es por la carpeta mapeada
  (`C:\lab` → `D:\proyectsVS\BootScope`).

## Seguridad

- `dentro.ps1` aborta si no detecta el usuario `WDAGUtilityAccount`.
- El Sandbox no toca el registro ni los servicios del host.
- La carpeta compartida está mapeada con escritura: el script solo escribe en
  `lab\resultados\` y en `C:\BootScope` (dentro del Sandbox).
