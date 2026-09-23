# Captura reproduzível da cena oceânica.
#
# Existe porque uma medição de água só vale com o estado declarado ao lado dela:
# escala de render efetiva, alvo de apresentação, modo de jogo e estado térmico.
# O script grava esse contexto junto das janelas, para que duas rodadas possam
# ser comparadas sem depender de memória de quem executou.
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [double]$ResolutionScale = 1.0,
    [double]$TargetFps = 120.0,
    [switch]$DynamicResolution,
    [switch]$SpectralWater,
    [switch]$LockCamera,
    # Opções booleanas extras, aplicadas como `--ez <nome> true`. Existe para que
    # um A/B caiba num APK só: reinstalar entre as pontas mede o aparelho.
    [string[]]$ExtraBooleans = @(),
    # Opções de texto extras, no formato `nome=valor`, aplicadas como
    # `--es <nome> <valor>`. É o caminho para os overrides de política de
    # qualidade (aether.quality_ambient, aether.quality_preset, ...), que não
    # são booleanos e por isso não cabiam em ExtraBooleans.
    [string[]]$ExtraStrings = @(),
    # Modo de WaterCostIsolation; -1 não envia a opção.
    [int]$WaterIsolation = -1,
    [int]$DurationSeconds = 90,
    [string]$AdbPath = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe",
    [string]$OutputRoot = (Join-Path $PSScriptRoot '..\build\android-validation')
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path $AdbPath)) { throw "adb não encontrado em $AdbPath" }
$outputDirectory = Join-Path $OutputRoot $Name
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

function Invoke-Adb { param([string[]]$Arguments) & $AdbPath @Arguments 2>&1 }

# O estado térmico antes e depois delimita a rodada: uma janela medida durante
# aquecimento não é comparável com uma medida a frio, mesmo no mesmo aparelho.
function Get-ThermalSnapshot {
    $dump = Invoke-Adb @('shell', 'dumpsys', 'thermalservice')
    $status = ($dump | Select-String -Pattern 'Thermal Status:\s*(\d+)' | Select-Object -First 1)
    # A API térmica do Android reportou status 0 durante toda uma sessão em que o
    # mesmo trabalho variou de 10 a 18 ms e a bateria subiu de 32 para 42 graus.
    # Portanto o status sozinho não qualifica uma rodada: a temperatura entra.
    $battery = Invoke-Adb @('shell', 'dumpsys', 'battery')
    $celsius = ($battery | Select-String -Pattern '^\s*temperature:\s*(\d+)' | Select-Object -First 1)
    $level = ($battery | Select-String -Pattern '^\s*level:\s*(\d+)' | Select-Object -First 1)
    return [ordered]@{
        thermalStatus     = if ($status) { [int]$status.Matches[0].Groups[1].Value } else { $null }
        batteryCelsius    = if ($celsius) { [double]$celsius.Matches[0].Groups[1].Value / 10.0 } else { $null }
        batteryLevel      = if ($level) { [int]$level.Matches[0].Groups[1].Value } else { $null }
    }
}

$arguments = @(
    'shell', 'am', 'start', '-S', '-n', 'dev.aether.editor/.AetherActivity',
    '--ez', 'aether.ocean_preview', 'true',
    '--ez', 'aether.profile_frames', 'true',
    '--ef', 'aether.resolution_scale', $ResolutionScale.ToString([Globalization.CultureInfo]::InvariantCulture),
    '--ef', 'aether.target_fps', $TargetFps.ToString([Globalization.CultureInfo]::InvariantCulture)
)
# Ausência de opção não é o mesmo que desligado: a política resolve o padrão.
# Só enviamos o override quando ele foi pedido, e ele é registrado no contexto.
if ($DynamicResolution) { $arguments += @('--ez', 'aether.dynamic_resolution', 'true') }
else { $arguments += @('--ez', 'aether.disable_dynamic_resolution', 'true') }
if ($SpectralWater) { $arguments += @('--ez', 'aether.water_fft', 'true') }
if ($LockCamera) { $arguments += @('--ez', 'aether.lock_camera', 'true') }
foreach ($option in $ExtraBooleans) { $arguments += @('--ez', $option, 'true') }
foreach ($option in $ExtraStrings) {
    $separator = $option.IndexOf('=')
    if ($separator -lt 1) { throw "ExtraStrings espera 'nome=valor', recebi '$option'" }
    $arguments += @('--es', $option.Substring(0, $separator), $option.Substring($separator + 1))
}
if ($WaterIsolation -ge 0) { $arguments += @('--ei', 'aether.water_isolation', $WaterIsolation) }

$thermalBefore = Get-ThermalSnapshot
Invoke-Adb @('logcat', '-c') | Out-Null
Invoke-Adb @('shell', 'am', 'force-stop', 'dev.aether.editor') | Out-Null
$startOutput = Invoke-Adb $arguments
Start-Sleep -Seconds $DurationSeconds
$logPath = Join-Path $outputDirectory 'logcat.txt'
Invoke-Adb @('logcat', '-d') | Set-Content -Path $logPath -Encoding utf8
$thermalAfter = Get-ThermalSnapshot

$lines = Get-Content $logPath
function Select-Records { param([string]$Tag)
    $lines | Select-String -Pattern "\[$Tag\] \{" | ForEach-Object {
        $json = $_.Line.Substring($_.Line.IndexOf('{'))
        try { $json | ConvertFrom-Json } catch { $null }
    } | Where-Object { $_ -ne $null }
}

$windows = @(Select-Records 'FrameProfile')
$passes = @()
if ($windows.Count -gt 0) {
    . (Join-Path $PSScriptRoot 'android-frame-profile.ps1')
    $passRecords = ConvertFrom-FrameProfilePassLog -Text ($lines -join "`n") -ExpectedPid ([string]$windows[-1].pid)
    foreach ($window in $windows) {
        $key = "$($window.epoch):$($window.window)"
        if (-not $passRecords.ContainsKey($key)) { throw "FrameProfilePasses ausente: $key." }
        $passes += $passRecords[$key]
    }
}
$pressure = @(Select-Records 'FrameProfilePressure')

# Uma opção rejeitada pelo Android (tipo errado no extra) é silenciosa no app e
# invalida a rodada inteira. Falhar aqui é mais barato do que publicar o número.
$rejected = @($lines | Select-String -Pattern 'Key aether\.\S+ expected')
if ($rejected.Count -gt 0) {
    $rejected | ForEach-Object { Write-Warning $_.Line.Trim() }
    throw 'Opção de lançamento rejeitada pelo Android: a rodada não representa a configuração pedida.'
}

$summary = [ordered]@{
    name             = $Name
    capturedAt       = (Get-Date).ToString('o')
    requested        = [ordered]@{
        resolutionScale   = $ResolutionScale
        targetFps         = $TargetFps
        dynamicResolution = [bool]$DynamicResolution
        spectralWater     = [bool]$SpectralWater
        lockCamera        = [bool]$LockCamera
        extraBooleans     = $ExtraBooleans
        waterIsolation    = $WaterIsolation
        durationSeconds   = $DurationSeconds
    }
    thermalBefore    = $thermalBefore
    thermalAfter     = $thermalAfter
    waterIsolationLog = @($lines | Select-String -Pattern '\[WaterIsolation\]' |
        Select-Object -First 1 | ForEach-Object { $_.Line.Trim() })
    runtimeControls  = @($lines | Select-String -Pattern '\[RuntimeControls\]' |
        Select-Object -Last 1 | ForEach-Object { $_.Line.Trim() })
    renderPolicy     = @($lines | Select-String -Pattern '\[RenderPolicy\] preset=' |
        Select-Object -First 1 | ForEach-Object { $_.Line.Trim() })
    windows          = $windows
    passes           = $passes
    pressure         = $pressure
}
$summaryPath = Join-Path $outputDirectory 'summary.json'
$summary | ConvertTo-Json -Depth 12 | Set-Content -Path $summaryPath -Encoding utf8

if ($windows.Count -eq 0) { throw "Nenhuma janela [FrameProfile] em $logPath." }
$last = $windows[-1]
$lastPressure = if ($pressure.Count -gt 0) { $pressure[-1] } else { $null }
[pscustomobject]@{
    janela        = $last.window
    fps           = [math]::Round($last.present_fps, 2)
    gpuMediaMs    = [math]::Round($last.gpu_frame_ms[2], 3)
    gpuP95Ms      = [math]::Round($last.gpu_frame_ms[3], 3)
    cpuP95Ms      = [math]::Round($last.process_cpu_ms[3], 3)
    escalaRender  = if ($lastPressure) { $lastPressure.render_scale_end } else { $null }
    gameMode      = if ($lastPressure) { $lastPressure.game_mode } else { $null }
    termico       = if ($lastPressure) { $lastPressure.thermal_pressure } else { $null }
    bateriaC      = "$($thermalBefore.batteryCelsius) -> $($thermalAfter.batteryCelsius)"
    resumo        = $summaryPath
} | Format-List
