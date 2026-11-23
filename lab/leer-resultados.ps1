# Lee los resultados que dejo el laboratorio (Windows Sandbox)
# Uso:  powershell -ExecutionPolicy Bypass -File lab\leer-resultados.ps1

$ErrorActionPreference = 'Continue'
$res = Join-Path $PSScriptRoot 'resultados'

if (-not (Test-Path $res)) {
    Write-Host "Todavia no hay resultados." -ForegroundColor Yellow
    Write-Host "1) Habilita Windows Sandbox y reinicia (ver docs\laboratorio.md)"
    Write-Host "2) Doble clic en lab\sandbox.wsb"
    Write-Host "3) Volve a correr este script"
    exit 1
}

Write-Host ""
Write-Host "=== Resultados del laboratorio ===" -ForegroundColor Cyan

$resumen = Join-Path $res 'resumen.txt'
if (Test-Path $resumen) {
    Get-Content $resumen | ForEach-Object { Write-Host "  $_" }
}

$motor = Join-Path $res 'motor.txt'
if (Test-Path $motor) {
    Write-Host ""
    Write-Host "--- Motor de acciones ---" -ForegroundColor Cyan
    Get-Content $motor | ForEach-Object { Write-Host "  $_" }
    $fallos = (Get-Content $motor | Select-String '^FALLO').Count
    if ($fallos -eq 0) { Write-Host "  Sin fallos." -ForegroundColor Green }
    else { Write-Host "  $fallos fallos." -ForegroundColor Red }
}

$ui = Join-Path $res 'ui.txt'
if (Test-Path $ui) {
    Write-Host ""
    Write-Host "--- Interfaz ---" -ForegroundColor Cyan
    Get-Content $ui | Select-Object -Last 12 | ForEach-Object { Write-Host "  $_" }
}

foreach ($j in 'antes.json', 'despues.json') {
    $ruta = Join-Path $res $j
    if (Test-Path $ruta) {
        try {
            $d = Get-Content $ruta -Raw | ConvertFrom-Json
            Write-Host ""
            Write-Host "--- $j ---" -ForegroundColor Cyan
            Write-Host ("  puntaje: {0}   componentes: {1}   admin: {2}" -f `
                $d.puntaje, $d.hallazgos.Count, $d.administrador)
        } catch {
            Write-Host "  $j ilegible" -ForegroundColor Yellow
        }
    }
}

Write-Host ""
Write-Host "Carpeta completa: $res" -ForegroundColor DarkGray
