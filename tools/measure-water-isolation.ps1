# Atribuição do custo da água, por diferença de mínimos emparelhados.
#
# Um GPU tile-deferred colapsa timestamps por subpasse, então o custo da água não
# aparece separado no relatório. A leitura honesta é remover um termo por vez e
# comparar. Duas armadilhas foram medidas neste aparelho e definem o método:
#
#  1. O mesmo binário, na mesma pose, varia de 8 a 18 ms conforme o estado de
#     clock, com `thermal_status=0` o tempo todo. Uma comparação entre rodadas
#     separadas mede DVFS, não a mudança. A primeira varredura assim produziu
#     deltas negativos — trabalho removido "custando mais".
#  2. A média entre janelas herda essa contaminação. O mínimo é a amostra menos
#     disputada e é o que se repete; por isso o agregado aqui é o mínimo, e cada
#     configuração é medida várias vezes, alternando com a referência completa.
param(
    [int[]]$Modes = @(5, 6, 7),
    [int]$Repeats = 3,
    [int]$DurationSeconds = 30,
    [switch]$SpectralWater,
    [double]$ResolutionScale = 1.0,
    [double]$TargetFps = 120.0,
    [string]$Prefix = 'iso'
)

$ErrorActionPreference = 'Stop'
$measure = Join-Path $PSScriptRoot 'measure-ocean.ps1'
$root = Join-Path $PSScriptRoot '..\build\android-validation'

function Invoke-Run {
    param([string]$Name, [int]$Isolation)
    $parameters = @{
        Name = $Name; ResolutionScale = $ResolutionScale; TargetFps = $TargetFps
        LockCamera = $true; DurationSeconds = $DurationSeconds; WaterIsolation = $Isolation
    }
    if ($SpectralWater) { $parameters.SpectralWater = $true }
    & $measure @parameters | Out-Null
    $summary = Get-Content (Join-Path $root "$Name\summary.json") -Raw | ConvertFrom-Json
    if ($summary.windows.Count -lt 2) { throw "$Name produziu menos de duas janelas." }
    # A primeira janela carrega aquecimento de pipeline; descartá-la é a
    # diferença entre medir a cena e medir o start-up.
    $windows = $summary.windows | Select-Object -Skip 1
    $passes = $summary.passes | Select-Object -Skip 1
    [pscustomobject]@{
        gpu = ($windows | ForEach-Object { $_.gpu_frame_ms[2] } | Measure-Object -Minimum).Minimum
        opaque = ($passes | ForEach-Object { $_.gpu_opaque_ms[2] } | Measure-Object -Minimum).Minimum
        fps = ($windows | ForEach-Object { $_.present_fps } | Measure-Object -Maximum).Maximum
    }
}

$results = @()
foreach ($mode in $Modes) {
    $full = @()
    $isolated = @()
    foreach ($repeat in 1..$Repeats) {
        $full += Invoke-Run -Name "$Prefix-full-$mode-$repeat" -Isolation 0
        $isolated += Invoke-Run -Name "$Prefix-mode$mode-$repeat" -Isolation $mode
    }
    $fullGpu = ($full | ForEach-Object { $_.gpu } | Measure-Object -Minimum).Minimum
    $modeGpu = ($isolated | ForEach-Object { $_.gpu } | Measure-Object -Minimum).Minimum
    $fullOpaque = ($full | ForEach-Object { $_.opaque } | Measure-Object -Minimum).Minimum
    $modeOpaque = ($isolated | ForEach-Object { $_.opaque } | Measure-Object -Minimum).Minimum
    $results += [pscustomobject]@{
        mode = $mode
        fullGpuMin = [math]::Round($fullGpu, 3)
        modeGpuMin = [math]::Round($modeGpu, 3)
        deltaGpu = [math]::Round($fullGpu - $modeGpu, 3)
        fullOpaqueMin = [math]::Round($fullOpaque, 3)
        modeOpaqueMin = [math]::Round($modeOpaque, 3)
        deltaOpaque = [math]::Round($fullOpaque - $modeOpaque, 3)
        fullFpsMax = [math]::Round((($full | ForEach-Object { $_.fps } | Measure-Object -Maximum).Maximum), 2)
        modeFpsMax = [math]::Round((($isolated | ForEach-Object { $_.fps } | Measure-Object -Maximum).Maximum), 2)
        # A dispersão entre repetições é publicada junto: um delta menor que ela
        # não é um resultado, é ruído com um nome.
        fullSpread = [math]::Round((($full | ForEach-Object { $_.gpu } | Measure-Object -Maximum).Maximum - $fullGpu), 3)
    }
    $results | Format-Table -AutoSize | Out-String | Write-Host
}
$results | ConvertTo-Json -Depth 5 | Set-Content -Path (Join-Path $root "$Prefix-summary.json") -Encoding utf8
$results | Format-Table -AutoSize
