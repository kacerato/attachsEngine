# A/B da reconstrução temporal no aparelho: TAA nativo, Arm ASR e AMD FSR 2.
#
# Um número de GPU só vale com a condição ao lado dele (memória
# aether-bancada-de-medicao): mesmo APK, mesma cena, mesma câmera, rodadas
# INTERCALADAS (ABC, BCA, CAB...) para que aquecimento e estado de clock pesem
# igual nos três modos, e o estado térmico antes e depois de cada caso.
#
# Por caso o script grava: logcat, captura de tela no fim da janela, as janelas
# do FrameProfile com as regiões de GPU (gpu_temporal_upscale_ms, gpu_post_ms,
# gpu_motion_ms, gpu_skinning_ms, gpu_frame_ms) e a linha que prova qual
# algoritmo rodou. Um modo pedido que o aparelho recusa entra no relatório com
# o motivo — nunca como se tivesse rodado. Erro da camada de validação ou crash
# invalida o caso.
#
# No fim, `tools/temporal_ab_images.py` compara cada captura com a do TAA
# nativo na escala 1.0 da mesma rodada (PSNR e erro absoluto médio num recorte),
# o que separa diferença de reconstrução de diferença de conteúdo.
[CmdletBinding()]
param(
    [ValidateRange(1, 10)][int]$Rounds = 3,
    [string[]]$Modes = @('taa', 'arm-asr', 'fsr2'),
    [double[]]$Scales = @(1.0, 0.67),
    [ValidateRange(10, 600)][int]$DurationSeconds = 45,
    # Projeto aberto pelo shell (pelo nome) e posto em Play sem toque: a
    # medição percorre a mesma rota de quem usa o editor. Um projeto com
    # personagens animados exercita skin, blend shapes e vetores de movimento.
    [Parameter(Mandatory)][string]$Project,
    [string[]]$SceneBooleans = @('aether.start_play'),
    [string]$TemporalQuality = 'quality',
    [string]$AdbPath = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe",
    [string]$DeviceSerial,
    [string]$OutputRoot = (Join-Path $PSScriptRoot "..\build\android-validation\temporal-ab-$(Get-Date -Format yyyyMMdd-HHmmss)")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'android-shell-lifecycle.ps1')
. (Join-Path $PSScriptRoot 'android-frame-profile.ps1')
if (-not (Test-Path $AdbPath)) { throw "adb não encontrado em $AdbPath" }
if (-not $DeviceSerial) {
    # O serial do ADB sem fio tem espaços ("adb-… (2)._adb-tls-connect._tcp"): vai até a tabulação.
    $devices = @(& $AdbPath devices | Where-Object { $_ -match "^[^\t]+\tdevice$" })
    if ($devices.Count -ne 1) { throw 'Informe DeviceSerial: é necessário exatamente um aparelho autorizado.' }
    $DeviceSerial = ($devices[0] -split "\t")[0]
}
function Invoke-Device([string[]]$DeviceArgs) {
    $output = & $AdbPath -s $DeviceSerial @DeviceArgs 2>&1
    if ($LASTEXITCODE -ne 0) { throw "ADB falhou ($($DeviceArgs -join ' ')): $output" }
    return ($output -join "`n")
}
function Get-Thermal {
    $thermal = Invoke-Device @('shell', 'dumpsys', 'thermalservice')
    $battery = Invoke-Device @('shell', 'dumpsys', 'battery')
    $status = [regex]::Match($thermal, 'Thermal Status:\s*(\d+)')
    $celsius = [regex]::Match($battery, '(?m)^\s*temperature:\s*(\d+)')
    return [ordered]@{
        thermalStatus  = if ($status.Success) { [int]$status.Groups[1].Value } else { $null }
        batteryCelsius = if ($celsius.Success) { [double]$celsius.Groups[1].Value / 10.0 } else { $null }
    }
}
function Get-Median([double[]]$Values) {
    $sorted = @($Values | Where-Object { $null -ne $_ } | Sort-Object)
    if ($sorted.Count -eq 0) { return $null }
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2) { return $sorted[$middle] }
    return ($sorted[$middle - 1] + $sorted[$middle]) / 2.0
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$policy = Invoke-Device @('shell', 'dumpsys', 'window', 'policy')
$keyguard = ConvertFrom-AndroidKeyguardPolicy -Policy $policy
if ($null -eq $keyguard.Showing -or $keyguard.Showing -or -not $keyguard.ScreenOn) {
    throw 'É necessário tela ligada e aparelho desbloqueado; o script não altera o keyguard.'
}

$cases = [System.Collections.Generic.List[object]]::new()
$combinations = @(foreach ($scale in $Scales) { foreach ($mode in $Modes) { [pscustomobject]@{ Mode = $mode; Scale = $scale } } })
for ($round = 0; $round -lt $Rounds; ++$round) {
    # Rotação por rodada: cada combinação ocupa uma posição diferente na ordem.
    $ordered = @(for ($i = 0; $i -lt $combinations.Count; ++$i) { $combinations[($i + $round) % $combinations.Count] })
    foreach ($combination in $ordered) {
        $mode = $combination.Mode
        $scale = $combination.Scale
        $name = "r$($round + 1)-$mode-$($scale.ToString('0.00', [Globalization.CultureInfo]::InvariantCulture))"
        $directory = Join-Path $OutputRoot $name
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
        # O renderer é privado: só o shell o abre, repassando a lista fechada de opções de medição.
        $arguments = @('shell', 'am', 'start', '-S', '-n', 'dev.aether.editor/.shell.AstraShellActivity',
            '--es', 'astra.open_project', $Project,
            '--ez', 'aether.profile_frames', 'true', '--ez', 'aether.disable_dynamic_resolution', 'true',
            '--ef', 'aether.resolution_scale', $scale.ToString([Globalization.CultureInfo]::InvariantCulture),
            '--es', 'aether.anti_aliasing', 'taa', '--es', 'aether.temporal_quality', $TemporalQuality)
        foreach ($option in $SceneBooleans) { $arguments += @('--ez', $option, 'true') }
        # TAA nativo amplia com Catmull-Rom: explícito, para o ajuste salvo no projeto não decidir o caso.
        $arguments += @('--es', 'aether.upscaling', $(if ($mode -eq 'taa') { 'catmull-rom' } else { $mode }))
        $before = Get-Thermal
        Invoke-Device @('logcat', '-c') | Out-Null
        Invoke-Device @('shell', 'am', 'force-stop', 'dev.aether.editor') | Out-Null
        Invoke-Device $arguments | Out-Null
        Start-Sleep -Seconds $DurationSeconds
        $capture = Join-Path $directory 'frame.png'
        & $AdbPath -s $DeviceSerial exec-out screencap -p > $capture
        $log = Invoke-Device @('logcat', '-d', '-v', 'threadtime')
        $log | Set-Content -LiteralPath (Join-Path $directory 'logcat.txt') -Encoding utf8
        $after = Get-Thermal

        $case = [ordered]@{
            name = $name; round = $round + 1; mode = $mode; scale = $scale
            thermalBefore = $before; thermalAfter = $after
            executed = $null; refusal = $null; valid = $true; problems = @()
            gpu = [ordered]@{}; capture = $capture
        }
        if ($log -match 'Key aether\.\S+ expected') { $case.valid = $false; $case.problems += 'opção de lançamento rejeitada' }
        if ($log -match 'VUID-|Validation Error') { $case.valid = $false; $case.problems += 'erro da camada de validação Vulkan' }
        if ($log -match 'Fatal signal|FATAL EXCEPTION') { $case.valid = $false; $case.problems += 'crash' }
        if ($SceneBooleans -contains 'aether.start_play' -and $log -notmatch '\[Editor\] Play iniciado') {
            $case.valid = $false; $case.problems += 'Play não iniciou'
        }
        # O que RODOU, não o que foi pedido: a linha do contexto criado ou a recusa com motivo.
        $ready = [regex]::Matches($log, '\[TemporalUpscaler\] (arm-asr|fsr2) pronto: render (\d+)x(\d+) -> (\d+)x(\d+)')
        if ($ready.Count -gt 0) {
            $last = $ready[$ready.Count - 1]
            $case.executed = [ordered]@{ backend = $last.Groups[1].Value; render = "$($last.Groups[2].Value)x$($last.Groups[3].Value)";
                                         display = "$($last.Groups[4].Value)x$($last.Groups[5].Value)" }
        } elseif ($mode -eq 'taa') {
            $case.executed = [ordered]@{ backend = 'taa' }
        }
        $refused = [regex]::Match($log, '\[TemporalUpscaler\][^\r\n]*(recus|indispon)[^\r\n]*')
        if ($mode -ne 'taa' -and -not $ready.Count) {
            $case.refusal = if ($refused.Success) { $refused.Value.Trim() } else { 'sem linha de contexto criado' }
            $case.valid = $false
        }
        $profilePid = [regex]::Match($log, '\[FrameProfile\] \{"schemaVersion":\d+,"pid":(\d+)')
        if ($profilePid.Success) {
            $windows = @(ConvertFrom-FrameProfileLog -Text $log -ExpectedPid $profilePid.Groups[1].Value -ExpectedInstances $null)
            foreach ($metric in @('gpu_frame_ms', 'gpu_temporal_upscale_ms', 'gpu_post_ms', 'gpu_motion_ms', 'gpu_skinning_ms', 'interval_ms')) {
                $values = @(foreach ($window in $windows) {
                    if ($window.PSObject.Properties.Name -contains $metric -and $null -ne $window.$metric) { [double]$window.$metric.p50 }
                })
                $case.gpu[$metric] = Get-Median $values
            }
            $case.gpu.windows = $windows.Count
        } else {
            $case.valid = $false; $case.problems += 'sem janela FrameProfile'
        }
        $cases.Add([pscustomobject]$case)
        Write-Output ("{0}: executado={1} gpu_frame={2} upscale={3} válido={4}" -f $name,
            ($(if ($case.executed) { $case.executed.backend } else { '-' })), $case.gpu['gpu_frame_ms'],
            $case.gpu['gpu_temporal_upscale_ms'], $case.valid)
    }
}

# Mediana por (modo, escala) só com casos válidos.
$aggregate = @(foreach ($combination in $combinations) {
    $valid = @($cases | Where-Object { $_.mode -eq $combination.Mode -and $_.scale -eq $combination.Scale -and $_.valid })
    [ordered]@{
        mode = $combination.Mode; scale = $combination.Scale; validCases = $valid.Count
        gpuFrameMs = Get-Median @($valid | ForEach-Object { $_.gpu['gpu_frame_ms'] })
        temporalUpscaleMs = Get-Median @($valid | ForEach-Object { $_.gpu['gpu_temporal_upscale_ms'] })
        postMs = Get-Median @($valid | ForEach-Object { $_.gpu['gpu_post_ms'] })
        motionMs = Get-Median @($valid | ForEach-Object { $_.gpu['gpu_motion_ms'] })
    }
})
$summary = [ordered]@{
    schemaVersion = 1; capturedAt = (Get-Date).ToString('o'); device = $DeviceSerial
    project = $Project; rounds = $Rounds; durationSeconds = $DurationSeconds; sceneBooleans = $SceneBooleans; temporalQuality = $TemporalQuality
    cases = $cases; aggregate = $aggregate
}
$summaryPath = Join-Path $OutputRoot 'summary.json'
$summary | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $summaryPath -Encoding utf8
& python (Join-Path $PSScriptRoot 'temporal_ab_images.py') $summaryPath
if ($LASTEXITCODE -ne 0) { throw 'A comparação de imagens falhou; o relatório de GPU ficou em summary.json.' }
Write-Output "Relatório: $summaryPath"
