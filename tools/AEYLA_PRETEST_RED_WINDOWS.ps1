param(
  [Parameter(Mandatory=$false)]
  [string]$LocalIPv4 = "2.0.0.20",

  [Parameter(Mandatory=$false)]
  [int]$PrefixLength = 8,

  [Parameter(Mandatory=$false)]
  [string]$TargetIPv4 = "",

  [Parameter(Mandatory=$false)]
  [string]$ReaperPath = "C:\Program Files\REAPER (x64)\reaper.exe"
)

$ErrorActionPreference = "Stop"
$script:FailCount = 0
$script:WarnCount = 0

function Pass([string]$Text) {
  Write-Host "[PASS] $Text" -ForegroundColor Green
}

function Warn([string]$Text) {
  $script:WarnCount++
  Write-Host "[WARN] $Text" -ForegroundColor Yellow
}

function Fail([string]$Text) {
  $script:FailCount++
  Write-Host "[FAIL] $Text" -ForegroundColor Red
}

function Info([string]$Text) {
  Write-Host "[INFO] $Text" -ForegroundColor Cyan
}

Write-Host ""
Write-Host "AEYLA · PRETEST RED ART-NET · WINDOWS" -ForegroundColor White
Write-Host "Lectura solamente: este script NO cambia IP, firewall ni adaptadores." -ForegroundColor DarkGray
Write-Host ""

# 1. Adaptador e IPv4 esperada.
$ipEntries = @(Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
  Where-Object { $_.IPAddress -eq $LocalIPv4 })

if($ipEntries.Count -eq 0) {
  Fail "La IPv4 esperada $LocalIPv4 no existe en ningún adaptador. Configúrala en Windows ANTES de abrir AEYLA."
} elseif($ipEntries.Count -gt 1) {
  Fail "La IPv4 $LocalIPv4 aparece en más de un adaptador. Evita subredes duplicadas."
} else {
  $entry = $ipEntries[0]
  $adapter = Get-NetAdapter -InterfaceIndex $entry.InterfaceIndex -ErrorAction SilentlyContinue
  if($null -eq $adapter) {
    Fail "Windows reporta la IPv4, pero no se pudo resolver su adaptador."
  } else {
    if($adapter.Status -eq "Up") {
      Pass "Adaptador TX/RX activo: $($adapter.Name) · $LocalIPv4/$($entry.PrefixLength)"
    } else {
      Fail "El adaptador $($adapter.Name) no está Up. Estado: $($adapter.Status)"
    }

    if($entry.PrefixLength -eq $PrefixLength) {
      Pass "Prefijo IPv4 coincide con el esperado: /$PrefixLength"
    } else {
      Fail "Prefijo real /$($entry.PrefixLength) != esperado /$PrefixLength. AEYLA no modifica la máscara de Windows."
    }

    if($adapter.InterfaceDescription -match "Wi-Fi|Wireless|802\.11") {
      Warn "La IPv4 esperada está sobre Wi-Fi. Para la prueba oficial usa Ethernet cableado."
    }
  }
}

# 2. Rutas/adaptadores que pueden confundir la red de show.
$upAdapters = @(Get-NetAdapter -ErrorAction SilentlyContinue | Where-Object { $_.Status -eq "Up" })
$virtualHints = @($upAdapters | Where-Object {
  $_.InterfaceDescription -match "Hyper-V|VMware|VirtualBox|TAP|VPN|WireGuard|ZeroTier|Tailscale"
})
if($virtualHints.Count -gt 0) {
  Warn ("Hay adaptadores virtuales/VPN activos: " + (($virtualHints | ForEach-Object {$_.Name}) -join ", ") + ". Desactívalos durante la prueba si no son necesarios.")
} else {
  Pass "No se detectaron adaptadores virtuales/VPN activos conocidos."
}

# 3. Firewall: TX normalmente sale; RX necesita excepción si el perfil bloquea entrada.
$profiles = @(Get-NetFirewallProfile -ErrorAction SilentlyContinue | Where-Object { $_.Enabled })
if($profiles.Count -eq 0) {
  Warn "No se pudo confirmar un perfil de Firewall de Windows activo."
} else {
  foreach($profile in $profiles) {
    if($profile.DefaultOutboundAction -eq "Block") {
      Fail "Firewall $($profile.Name): salida por defecto BLOQUEADA. Art-Net TX puede no salir sin regla explícita."
    } else {
      Pass "Firewall $($profile.Name): salida por defecto no está bloqueada."
    }
  }
}

$reaperExists = Test-Path -LiteralPath $ReaperPath
if(!$reaperExists) {
  Warn "No se encontró REAPER en: $ReaperPath. Si está en otra ruta, vuelve a ejecutar con -ReaperPath."
} else {
  $allowRules = @()
  try {
    $allowRules = @(Get-NetFirewallRule -Enabled True -Direction Inbound -Action Allow -ErrorAction SilentlyContinue |
      Get-NetFirewallApplicationFilter -ErrorAction SilentlyContinue |
      Where-Object { $_.Program -and $_.Program -ieq $ReaperPath })
  } catch {
    $allowRules = @()
  }

  if($allowRules.Count -gt 0) {
    Pass "Existe al menos una excepción de entrada asociada a REAPER."
  } else {
    Warn "No se encontró una excepción de entrada asociada a REAPER. La CAPTURA Art-Net UDP/6454 puede ser bloqueada por Windows."
    Info "Antes de la prueba, crea una regla de entrada restringida a REAPER + UDP 6454 + red de show. Hazlo con privilegios de administrador."
  }
}

# 4. Conflictos locales con UDP 6454.
$udp6454 = @(Get-NetUDPEndpoint -LocalPort 6454 -ErrorAction SilentlyContinue)
if($udp6454.Count -eq 0) {
  Pass "UDP 6454 está libre antes de abrir el host."
} else {
  Warn "Ya existen $($udp6454.Count) endpoint(s) UDP/6454. Cierra otros listeners Art-Net antes de la prueba para evitar ambigüedad."
  foreach($endpoint in $udp6454) {
    $processName = "PID $($endpoint.OwningProcess)"
    try {
      $processName = (Get-Process -Id $endpoint.OwningProcess -ErrorAction Stop).ProcessName + " (PID $($endpoint.OwningProcess))"
    } catch {}
    Info "UDP/6454 · $($endpoint.LocalAddress) · $processName"
  }
}

# 5. Ruta al nodo, si se conoce su IP.
if($TargetIPv4 -ne "") {
  try {
    $tnc = Test-NetConnection -ComputerName $TargetIPv4 -InformationLevel Detailed -WarningAction SilentlyContinue
    if($tnc.SourceAddress -and $tnc.SourceAddress.IPAddress -eq $LocalIPv4) {
      Pass "La ruta hacia $TargetIPv4 sale con la IPv4 esperada $LocalIPv4."
    } elseif($tnc.SourceAddress) {
      Fail "La ruta hacia $TargetIPv4 usa $($tnc.SourceAddress.IPAddress), no $LocalIPv4."
    } else {
      Warn "No se pudo confirmar la IPv4 de origen hacia $TargetIPv4."
    }

    if($tnc.PingSucceeded) {
      Pass "El nodo/objetivo $TargetIPv4 responde ICMP."
    } else {
      Warn "$TargetIPv4 no respondió ping. Esto NO demuestra que Art-Net falle; algunos equipos bloquean ICMP."
    }
  } catch {
    Warn "No se pudo completar Test-NetConnection hacia $TargetIPv4: $($_.Exception.Message)"
  }
}

Write-Host ""
if($script:FailCount -eq 0) {
  Write-Host "RESULTADO: PRECHECK SIN BLOQUEOS DUROS · WARN=$script:WarnCount" -ForegroundColor Green
  Write-Host "Aún falta validar paquetes Art-Net reales con AEYLA + nodo/receptor." -ForegroundColor White
  exit 0
} else {
  Write-Host "RESULTADO: NO IR A PRUEBA TODAVÍA · FAIL=$script:FailCount · WARN=$script:WarnCount" -ForegroundColor Red
  exit 2
}
