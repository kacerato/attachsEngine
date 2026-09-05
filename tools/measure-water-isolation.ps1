# Atribuição do custo do fragmento da água, por diferença emparelhada.
#
# Um GPU tile-deferred colapsa timestamps por subpasse, então o custo da água
# não aparece separado no relatório. A leitura honesta é remover um termo por
# vez e comparar. Como o clock deste aparelho move o mesmo trabalho em mais de
# 20% entre rodadas, cada modo é medido **imediatamente ao lado** de uma rodada
# completa, e só a diferença dentro do par é publicada.
param(
    [int[]]$Modes = @(1, 2, 3, 4, 5, 6),
    [int]$Rounds = 2,
    [int]$DurationSeconds = 40,
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
    # A primeira janela de cada lançamento carrega aquecimento de pipeline e
    # alocação; descartá-la é a diferença entre medir a cena e medir o start-up.
    $windows = $summary.windows | Select-Object -Skip 1
    $passes = $summary.passes | Select-Object -Skip 1
    [pscustomobject]@{
        name = $Name
        fps = ($windows | Measure-Object -Property present_fps -Average).Average
        gpu = ($windows | ForEach-Object { $_.gpu_frame_ms[2] } | Measure-Object -Average).Average
        gpuP95 = ($windows | ForEach-Object { $_.gpu_frame_ms[3] } | Measure-Object -Average).Average
        opaque = ($passes | ForEach-Object { $_.gpu_opaque_ms[2] } | Measure-Object -Average).Average
        post = ($passes | ForEach-Object { $_.gpu_post_ms[2] } | Measure-Object -Average).Average
        windows = $windows.Count
    }
}

$results = @()
foreach ($round in 1..$Rounds) {
    foreach ($mode in $Modes) {
        $baseline = Invoke-Run -Name "$Prefix-full-$mode-$round" -Isolation 0
        $isolated = Invoke-Run -Name "$Prefix-mode$mode-$round" -Isolation $mode
        $results += [pscustomobject]@{
            round = $round
            mode = $mode
            fullGpu = [math]::Round($baseline.gpu, 3)
            modeGpu = [math]::Round($isolated.gpu, 3)
            deltaGpu = [math]::Round($baseline.gpu - $isolated.gpu, 3)
            fullOpaque = [math]::Round($baseline.opaque, 3)
            modeOpaque = [math]::Round($isolated.opaque, 3)
            deltaOpaque = [math]::Round($baseline.opaque - $isolated.opaque, 3)
            fullFps = [math]::Round($baseline.fps, 2)
            modeFps = [math]::Round($isolated.fps, 2)
        }
        $results | Format-Table -AutoSize | Out-String | Write-Host
    }
}
$results | ConvertTo-Json -Depth 5 | Set-Content -Path (Join-Path $root "$Prefix-summary.json") -Encoding utf8
$results | Format-Table -AutoSize
