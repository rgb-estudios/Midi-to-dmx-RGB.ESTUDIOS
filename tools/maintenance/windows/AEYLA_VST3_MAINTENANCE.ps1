[CmdletBinding()]
param(
  [ValidateSet('Audit', 'Clean', 'Install', 'CleanInstall')]
  [string]$Action = 'Audit',

  [string]$SourceVst3 = '',

  [switch]$ResetReaperCache,

  [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProductName = 'AEYLA Visual DMX'
$BundleName = 'AeylaVisualDmx.vst3'
$CacheMatch = '(?i)AeylaVisualDmx|AEYLA Visual DMX'

function Write-Step([string]$Message) {
  Write-Host "[AEYLA] $Message"
}

function Assert-HostsClosed {
  $blocked = @()
  foreach ($process in Get-Process -ErrorAction SilentlyContinue) {
    $name = $process.ProcessName
    if ($name -match '^(reaper|reaper_host32|pluginval|validator)$' -or
        $name -match '^Ableton Live') {
      $blocked += $name
    }
  }

  $blocked = $blocked | Sort-Object -Unique
  if ($blocked.Count -gt 0) {
    throw "Cierra los hosts antes de mantener AEYLA: $($blocked -join ', ')"
  }
}

function Get-InstallTargets {
  $targets = New-Object System.Collections.Generic.List[string]

  if ($env:ProgramFiles) {
    $targets.Add((Join-Path $env:ProgramFiles "Common Files\VST3\$BundleName"))
  }
  if ($env:LOCALAPPDATA) {
    $targets.Add((Join-Path $env:LOCALAPPDATA "Programs\Common\VST3\$BundleName"))
  }

  return $targets | Select-Object -Unique
}

function Remove-ExactBundle([string]$Path) {
  if (-not (Test-Path -LiteralPath $Path)) {
    Write-Step "No instalado: $Path"
    return
  }

  if ($DryRun) {
    Write-Step "DRY-RUN eliminaría: $Path"
    return
  }

  Write-Step "Eliminando bundle AEYLA: $Path"
  Remove-Item -LiteralPath $Path -Recurse -Force
  if (Test-Path -LiteralPath $Path) {
    throw "No se pudo eliminar completamente $Path"
  }
}

function Reset-ReaperPluginCacheEntry {
  if (-not $env:APPDATA) { return }

  $reaperRoot = Join-Path $env:APPDATA 'REAPER'
  $cacheFiles = @(
    (Join-Path $reaperRoot 'reaper-vstplugins64.ini'),
    (Join-Path $reaperRoot 'reaper-vstplugins.ini')
  )

  foreach ($cache in $cacheFiles) {
    if (-not (Test-Path -LiteralPath $cache)) { continue }

    $lines = [System.IO.File]::ReadAllLines($cache)
    $matches = @($lines | Where-Object { $_ -match $CacheMatch })
    if ($matches.Count -eq 0) {
      Write-Step "REAPER cache sin entrada AEYLA: $cache"
      continue
    }

    Write-Step "REAPER cache: $($matches.Count) entrada(s) AEYLA detectada(s) en $cache"
    if ($DryRun) {
      foreach ($line in $matches) { Write-Host "  DRY-RUN: $line" }
      continue
    }

    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $backup = "$cache.aeyla-backup-$stamp"
    Copy-Item -LiteralPath $cache -Destination $backup -Force

    $filtered = @($lines | Where-Object { $_ -notmatch $CacheMatch })
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllLines($cache, $filtered, $utf8NoBom)
    Write-Step "Entrada AEYLA retirada. Backup: $backup"
  }
}

function Audit-Aeyla {
  Write-Step "Auditando instalaciones conocidas de $ProductName"
  foreach ($target in Get-InstallTargets) {
    if (Test-Path -LiteralPath $target) {
      Write-Step "ENCONTRADO: $target"
    } else {
      Write-Step "ausente: $target"
    }
  }

  if ($env:APPDATA) {
    foreach ($cache in @(
      (Join-Path $env:APPDATA 'REAPER\reaper-vstplugins64.ini'),
      (Join-Path $env:APPDATA 'REAPER\reaper-vstplugins.ini')
    )) {
      if (-not (Test-Path -LiteralPath $cache)) { continue }
      $matches = @([System.IO.File]::ReadAllLines($cache) | Where-Object { $_ -match $CacheMatch })
      foreach ($line in $matches) {
        Write-Step "REAPER cache AEYLA: $line"
      }
    }
  }
}

function Clean-Aeyla {
  Assert-HostsClosed
  foreach ($target in Get-InstallTargets) {
    Remove-ExactBundle $target
  }

  if ($ResetReaperCache) {
    Reset-ReaperPluginCacheEntry
  } else {
    Write-Step 'Cache REAPER no modificada. Usa -ResetReaperCache para retirar sólo la entrada AEYLA.'
  }
}

function Install-Aeyla {
  Assert-HostsClosed

  if ([string]::IsNullOrWhiteSpace($SourceVst3)) {
    throw 'Install/CleanInstall requiere -SourceVst3 apuntando al bundle AeylaVisualDmx.vst3.'
  }

  $source = [System.IO.Path]::GetFullPath($SourceVst3)
  if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "No se encontró el bundle fuente: $source"
  }
  if ([System.IO.Path]::GetFileName($source) -ne $BundleName) {
    throw "El bundle fuente debe llamarse exactamente $BundleName"
  }

  $destinationRoot = Join-Path $env:ProgramFiles 'Common Files\VST3'
  $destination = Join-Path $destinationRoot $BundleName

  if ($DryRun) {
    Write-Step "DRY-RUN copiaría $source -> $destination"
    return
  }

  New-Item -ItemType Directory -Path $destinationRoot -Force | Out-Null
  if (Test-Path -LiteralPath $destination) {
    Remove-Item -LiteralPath $destination -Recurse -Force
  }
  Copy-Item -LiteralPath $source -Destination $destination -Recurse -Force

  if (-not (Test-Path -LiteralPath $destination -PathType Container)) {
    throw "La verificación posterior a instalación falló: $destination"
  }

  $binary = Get-ChildItem -LiteralPath $destination -Recurse -File -Filter 'AeylaVisualDmx.vst3' |
    Select-Object -First 1
  if (-not $binary) {
    throw 'Bundle copiado, pero no contiene el binario AeylaVisualDmx.vst3 esperado.'
  }

  $hash = (Get-FileHash -LiteralPath $binary.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
  Write-Step "Instalado: $destination"
  Write-Step "SHA256 binario: $hash"
}

Write-Step "Acción=$Action DryRun=$($DryRun.IsPresent) ResetReaperCache=$($ResetReaperCache.IsPresent)"

switch ($Action) {
  'Audit' {
    Audit-Aeyla
  }
  'Clean' {
    Clean-Aeyla
    Audit-Aeyla
  }
  'Install' {
    Install-Aeyla
    Audit-Aeyla
  }
  'CleanInstall' {
    Clean-Aeyla
    Install-Aeyla
    Audit-Aeyla
  }
}

Write-Step 'Operación terminada. AEYLA nunca elimina archivos .aeylashow ni proyectos del DAW.'
