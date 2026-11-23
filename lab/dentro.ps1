# BootScope - pruebas dentro de Windows Sandbox
#
# Corre automaticamente al abrir sandbox.wsb. NO debe ejecutarse en el equipo
# real: modifica servicios, autoinicios y RDP a proposito.

$ErrorActionPreference = 'Continue'

$marca = "=== BootScope lab $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') ==="

if ($env:USERNAME -ne 'WDAGUtilityAccount') {
    Write-Host ""
    Write-Host "  ESTE SCRIPT ES SOLO PARA WINDOWS SANDBOX." -ForegroundColor Red
    Write-Host "  Detectado usuario: $env:USERNAME"
    Write-Host "  Abortado sin tocar nada."
    Write-Host ""
    exit 1
}

$raiz    = 'C:\BootScope'
$res     = 'C:\lab\lab\resultados'
$log     = Join-Path $res 'sesion.txt'

New-Item -ItemType Directory -Force -Path $raiz, $res | Out-Null
Set-Content -Path $log -Value $marca -Encoding UTF8

function Log($texto) {
    Write-Host $texto
    Add-Content -Path $log -Value $texto -Encoding UTF8
}

Log "Windows Sandbox detectado. Preparando entorno..."
$env:BS_LAB = '1'

# 1) copiar binarios al disco local del sandbox
$exes = @('bootscope.exe', 'test_lab.exe', 'test_actions.exe')
foreach ($e in $exes) {
    $origen = Join-Path 'C:\lab\build' $e
    if (Test-Path $origen) {
        Copy-Item $origen (Join-Path $raiz $e) -Force
        Log "  copiado $e"
    } else {
        Log "  AVISO: falta $origen (compilar con build-mingw.bat en el host)"
    }
}

# 2) estado inicial
Log ""
Log "[1/5] Reporte inicial"
if (Test-Path (Join-Path $raiz 'bootscope.exe')) {
    Start-Process -FilePath (Join-Path $raiz 'bootscope.exe') `
        -ArgumentList "--report `"$res\antes.json`"" -Wait -WindowStyle Hidden
    Log "  escrito antes.json"
}

# 3) motor de acciones (servicio, autoinicio, RDP, respaldo)
Log ""
Log "[2/5] Motor de acciones"
$motor = Join-Path $raiz 'test_lab.exe'
if (Test-Path $motor) {
    $salida = Join-Path $res 'motor.txt'
    $p = Start-Process -FilePath $motor -Wait -PassThru -NoNewWindow `
        -RedirectStandardOutput $salida
    $codigo = $p.ExitCode
    Log "  exit code = $codigo"
    if (Test-Path $salida) { Get-Content $salida | ForEach-Object { Log "  $_" } }
} else {
    Log "  AVISO: falta test_lab.exe"
    $codigo = -1
}

# 4) test de interfaz
Log ""
Log "[3/5] Test de interfaz"
$ui = Join-Path $raiz 'bootscope.exe'
if (Test-Path $ui) {
    Start-Process -FilePath $ui -ArgumentList '--selftest' -Wait -WindowStyle Hidden
    $uiLog = Join-Path $env:TEMP 'bootscope-selftest.log'
    if (Test-Path $uiLog) {
        Copy-Item $uiLog (Join-Path $res 'ui.txt') -Force
        Get-Content $uiLog | ForEach-Object { Log "  $_" }
    }
}

# 5) estado final
Log ""
Log "[4/5] Reporte final"
if (Test-Path (Join-Path $raiz 'bootscope.exe')) {
    Start-Process -FilePath (Join-Path $raiz 'bootscope.exe') `
        -ArgumentList "--report `"$res\despues.json`"" -Wait -WindowStyle Hidden
    Log "  escrito despues.json"
}

# 6) resumen
$resumen = Join-Path $res 'resumen.txt'
$lineas = @(
    $marca,
    "equipo: $env:COMPUTERNAME",
    "usuario: $env:USERNAME",
    "motor: exit code = $codigo",
    "reportes: antes.json / despues.json",
    "log completo: sesion.txt"
)
Set-Content -Path $resumen -Value $lineas -Encoding UTF8
Log ""
Log "[5/5] Listo. Resultados en C:\lab\lab\resultados"
Log "  (esa carpeta esta mapeada al host: D:\proyectsVS\BootScope\lab\resultados)"

Write-Host ""
Write-Host "  Abriendo BootScope dentro del Sandbox..." -ForegroundColor Cyan
Start-Process -FilePath (Join-Path $raiz 'bootscope.exe')
Write-Host "  Deja esta ventana abierta si queres leer el log." -ForegroundColor DarkGray
