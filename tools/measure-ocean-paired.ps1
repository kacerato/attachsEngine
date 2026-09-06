# Comparação pareada A/B para a cena oceânica.
#
# Existe porque medir A, depois B, e subtrair não funciona neste aparelho. Sem
# root não há acesso a /sys/class/kgsl, então o clock da GPU não pode ser fixado:
# quando a carga cai o governor abaixa a frequência, e o tempo por quadro deixa
# de ser monotônico com o trabalho. Em 2026-09-06, três dos cinco modos de
# WaterCostIsolation "economizaram" tempo negativo justamente por isso — ver
# docs/PLANO-AGUA-AMBIENTE-ASTRA.md §2.4.
#
# O que funciona sem root é intercalar: A B A B ... Cada B é comparado com a
# média dos dois A vizinhos, o que cancela deriva linear (térmica e de clock)
# dentro do par. O desvio entre as réplicas de A é o ruído da bancada, e nenhuma
# diferença menor que ele deve ser reivindicada como ganho.
#
# O pareamento cancela deriva linear, não transiente. Uma série iniciada com o
# aparelho frio mediu desvio de 6,058 ms entre réplicas da mesma configuração,
# contra 0,039 ms numa série já em regime — porque durante o aquecimento o clock
# não varia de forma linear no tempo. Por isso existe o aquecimento antes da
# primeira rodada válida, e por isso a faixa térmica da série entra no relatório
# e no veredito: uma série que aqueceu demais no meio não conclui nada.
#
#   ./tools/measure-ocean-paired.ps1 -Name espectral -IsolationB 2 -Repeats 3
param(
    [Parameter(Mandatory = $true)][string]$Name,
    # Modo de WaterCostIsolation da ponta B; -1 mantém a configuração de produção.
    [Parameter(Mandatory = $true)][int]$IsolationB,
    # Ponta A. O padrão é produção, que é a referência natural.
    [int]$IsolationA = -1,
    [int]$Repeats = 3,
    [int]$DurationSeconds = 50,
    # Rodada descartada que leva o aparelho ao regime antes da primeira medida.
    # Zero desliga, para quem já sabe que a série anterior deixou o aparelho quente.
    [int]$WarmupSeconds = 60,
    # Faixa térmica tolerada dentro da série, em graus. Acima disso o relatório
    # sai marcado como não confiável, mesmo que os números pareçam bons.
    [double]$MaximumThermalRangeCelsius = 1.5,
    [double]$TargetFps = 120.0,
    [switch]$NoSpectralWater,
    # Aplicadas nas duas pontas: é o que mantém o par comparável quando o
    # experimento muda a política e não o modo de isolamento.
    [string[]]$ExtraStrings = @(),
    [string[]]$ExtraStringsB = @(),
    [string]$AdbPath = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe",
    [string]$OutputRoot = (Join-Path $PSScriptRoot '..\build\android-validation')
)

$ErrorActionPreference = 'Stop'
$measure = Join-Path $PSScriptRoot 'measure-ocean.ps1'
if (-not (Test-Path $measure)) { throw "measure-ocean.ps1 não encontrado ao lado deste script" }

function Invoke-Run {
    param([string]$RunName, [int]$Isolation)
    $arguments = @{
        Name            = $RunName
        TargetFps       = $TargetFps
        DurationSeconds = $DurationSeconds
        AdbPath         = $AdbPath
        OutputRoot      = $OutputRoot
        LockCamera      = $true
    }
    if (-not $NoSpectralWater) { $arguments['SpectralWater'] = $true }
    if ($Isolation -ge 0) { $arguments['WaterIsolation'] = $Isolation }
    $strings = @($ExtraStrings)
    if ($RunName -like '*-B*') { $strings += $ExtraStringsB }
    if ($strings.Count -gt 0) { $arguments['ExtraStrings'] = $strings }
    & $measure @arguments *>&1 | Out-Null

    $summaryPath = Join-Path (Join-Path $OutputRoot $RunName) 'summary.json'
    $summary = Get-Content $summaryPath -Raw | ConvertFrom-Json
    # A última janela é a mais estável: as anteriores ainda carregam o aquecimento
    # de cache e de shader do início da cena.
    $window = $summary.windows[-1]
    return [ordered]@{
        run       = $RunName
        isolation = $Isolation
        gpuMs     = [double]$window.gpu_frame_ms[2]
        fps       = [double]$window.present_fps
        celsius   = [double]$summary.thermalAfter.batteryCelsius
    }
}

if ($WarmupSeconds -gt 0) {
    Write-Host "aquecendo por $WarmupSeconds s (rodada descartada)..."
    $warmupName = "$Name-warmup"
    $warmupArguments = @{
        Name            = $warmupName
        TargetFps       = $TargetFps
        DurationSeconds = $WarmupSeconds
        AdbPath         = $AdbPath
        OutputRoot      = $OutputRoot
        LockCamera      = $true
    }
    if (-not $NoSpectralWater) { $warmupArguments['SpectralWater'] = $true }
    if ($IsolationA -ge 0) { $warmupArguments['WaterIsolation'] = $IsolationA }
    if ($ExtraStrings.Count -gt 0) { $warmupArguments['ExtraStrings'] = $ExtraStrings }
    & $measure @warmupArguments *>&1 | Out-Null
}

$runs = @()
for ($i = 1; $i -le $Repeats; $i++) {
    $runs += Invoke-Run -RunName "$Name-A$i" -Isolation $IsolationA
    $runs += Invoke-Run -RunName "$Name-B$i" -Isolation $IsolationB
}
# Um A final fecha o último par: sem ele o B mais recente só teria vizinho de um
# lado, e é exatamente onde a deriva térmica é maior.
$runs += Invoke-Run -RunName "$Name-A$($Repeats + 1)" -Isolation $IsolationA

$aRuns = @($runs | Where-Object { $_.run -like '*-A*' })
$bRuns = @($runs | Where-Object { $_.run -like '*-B*' })

$deltas = @()
for ($i = 0; $i -lt $bRuns.Count; $i++) {
    $neighbourMean = ($aRuns[$i].gpuMs + $aRuns[$i + 1].gpuMs) / 2.0
    $deltas += [double]($bRuns[$i].gpuMs - $neighbourMean)
}

function Get-Stats {
    param([double[]]$Values)
    $mean = ($Values | Measure-Object -Average).Average
    if ($Values.Count -lt 2) { return @{ mean = $mean; sd = [double]::NaN } }
    $variance = ($Values | ForEach-Object { [Math]::Pow($_ - $mean, 2) } | Measure-Object -Sum).Sum / ($Values.Count - 1)
    return @{ mean = $mean; sd = [Math]::Sqrt($variance) }
}

$baseline = Get-Stats -Values (@($aRuns | ForEach-Object { $_.gpuMs }))
$effect = Get-Stats -Values $deltas

$temperatures = @($runs | ForEach-Object { $_.celsius })
$thermalRange = ($temperatures | Measure-Object -Maximum).Maximum - ($temperatures | Measure-Object -Minimum).Minimum
$thermallyStable = ($thermalRange -le $MaximumThermalRangeCelsius)

$report = [ordered]@{
    name            = $Name
    capturedAt      = (Get-Date).ToString('o')
    isolationA      = $IsolationA
    isolationB      = $IsolationB
    repeats         = $Repeats
    durationSeconds = $DurationSeconds
    runs            = $runs
    baselineMeanMs  = [Math]::Round($baseline.mean, 3)
    baselineSdMs    = [Math]::Round($baseline.sd, 3)
    pairedDeltaMs   = [Math]::Round($effect.mean, 3)
    pairedDeltaSdMs = [Math]::Round($effect.sd, 3)
    warmupSeconds   = $WarmupSeconds
    thermalRangeCelsius = [Math]::Round($thermalRange, 1)
    thermallyStable = $thermallyStable
    # Duas condições, e as duas são necessárias: o efeito precisa superar o ruído
    # da própria referência, e a série precisa ter corrido em regime térmico. Um
    # número limpo obtido durante aquecimento é limpo por acaso.
    conclusive      = (([Math]::Abs($effect.mean) -gt (2.0 * $baseline.sd)) -and $thermallyStable)
}

$reportPath = Join-Path $OutputRoot "$Name-paired.json"
$report | ConvertTo-Json -Depth 6 | Set-Content -Path $reportPath -Encoding utf8

$runs | ForEach-Object {
    '{0,-22} iso={1,3}  gpu={2,7:N3} ms  fps={3,6:N2}  {4:N1} C' -f $_.run, $_.isolation, $_.gpuMs, $_.fps, $_.celsius
}
''
'referência A : {0:N3} ms  (desvio entre {1} réplicas: {2:N3} ms)' -f $baseline.mean, $aRuns.Count, $baseline.sd
'efeito B-A   : {0:N3} ms  (desvio entre {1} pares: {2:N3} ms)' -f $effect.mean, $deltas.Count, $effect.sd
'térmico     : {0:N1} C de faixa na série  ({1})' -f $thermalRange, $(if ($thermallyStable) { 'regime' } else { 'INSTÁVEL' })
'conclusivo   : {0}' -f $(if ($report.conclusive) {
    'sim'
} elseif (-not $thermallyStable) {
    'NÃO — a série aqueceu {0:N1} C; refaça com o aparelho já em regime' -f $thermalRange
} else {
    'NÃO — o efeito ({0:N3} ms) não supera o ruído da bancada ({1:N3} ms)' -f $effect.mean, (2.0 * $baseline.sd)
})
'relatório    : {0}' -f $reportPath
