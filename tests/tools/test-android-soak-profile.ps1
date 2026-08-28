Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../../tools/android-soak-profile.ps1')
. (Join-Path $PSScriptRoot '../../tools/android-frame-profile.ps1')
$count = 0
function Assert-Soak { param([bool]$Value); if (-not $Value) { throw 'Asserção do soak falhou.' } }
function Test-Soak { param([string]$Name, [scriptblock]$Body); & $Body; ++$script:count; Write-Host "PASS: $Name" }
function Sample { param([double]$Time, $Power = 2); return [pscustomobject]@{ elapsedSeconds = $Time; powerWatts = $Power; externallyPowered = $false; thermalStatus = 0 } }

Test-Soak 'média ponderada pelo intervalo, não pelo número de amostras' {
    $summary = Get-SoakPowerSummary @((Sample 0 1), (Sample 2 3), (Sample 10 3))
    Assert-Soak ([Math]::Abs($summary.averageWatts - 2.8) -lt 1e-10)
    Assert-Soak ($summary.elapsedSeconds -eq 10 -and $summary.withinPowerBudget)
}
Test-Soak 'sensor ausente invalida toda a média' {
    $summary = Get-SoakPowerSummary @((Sample 0), (Sample 5 $null), (Sample 10))
    Assert-Soak ($null -eq $summary.averageWatts -and -not $summary.withinPowerBudget)
}
Test-Soak 'sem sensor não anuncia pico zero nem cobertura' {
    $summary = Get-SoakPowerSummary @((Sample 0 $null), (Sample 5 $null))
    Assert-Soak ($null -eq $summary.peakWatts -and $summary.validPowerSamples -eq 0 -and -not $summary.continuous)
}
Test-Soak 'lacuna de coleta não vira aprovação' {
    Assert-Soak (-not (Get-SoakPowerSummary @((Sample 0), (Sample 20))).continuous)
}
Test-Soak 'carregamento externo não é consumo do aparelho' {
    $charged = Sample 5; $charged.externallyPowered = $true
    Assert-Soak (-not (Get-SoakPowerSummary @((Sample 0), $charged)).withinPowerBudget)
}
Test-Soak 'alimentação desconhecida não vira bateria' {
    $unknown = Sample 5; $unknown.externallyPowered = $null
    Assert-Soak (-not (Get-SoakPowerSummary @((Sample 0), $unknown)).continuous)
}
Test-Soak 'NaN e infinito não viram zero' {
    foreach ($bad in @([double]::NaN, [double]::PositiveInfinity, -1)) {
        Assert-Soak (-not (Get-SoakPowerSummary @((Sample 0), (Sample 5 $bad))).withinPowerBudget)
    }
}
Test-Soak 'limite de 4 W é exclusivo' {
    Assert-Soak (-not (Get-SoakPowerSummary @((Sample 0 4), (Sample 5 4))).withinPowerBudget)
}
Test-Soak 'aviso térmico intermediário não se perde entre pontas frias' {
    $hot = Sample 5; $hot.thermalStatus = 3
    $summary = Get-SoakPowerSummary @((Sample 0), $hot, (Sample 10))
    Assert-Soak ($summary.peakThermalStatus -eq 3 -and -not $summary.noThermalWarningObserved)
}
Test-Soak 'status térmico ausente não significa sem throttle' {
    $unknown = Sample 5; $unknown.thermalStatus = $null
    Assert-Soak (-not (Get-SoakPowerSummary @((Sample 0), $unknown)).noThermalWarningObserved)
}
Test-Soak 'timestamps fora de ordem são rejeitados' {
    $rejected = $false
    try { Get-SoakPowerSummary @((Sample 5), (Sample 0)) | Out-Null } catch { $rejected = $true }
    Assert-Soak $rejected
}
Test-Soak 'FPS de janela detecta stall escondido pela média' {
    $times = @(for ($i = 0; $i -lt 600; ++$i) { if ($i -lt 240 -or $i -gt 300) { [long](1000000000L + $i * 16666667L) } })
    $summary = Get-SurfaceFrameSummary $times
    Assert-Soak ($summary.minimumRollingSecondFps -lt 55 -and $summary.intervalMs.max -gt 1000)
}
Test-Soak '60 FPS estáveis têm cobertura e janela de um segundo' {
    $times = @(for ($i = 0; $i -lt 180; ++$i) { [long](1000000000L + $i * 16666667L) })
    $summary = Get-SurfaceFrameSummary $times
    Assert-Soak ($summary.minimumRollingSecondFps -ge 59 -and $summary.elapsedSeconds -gt 2)
}
Write-Host "$count testes de soak passaram."
