# Pure reduction of monotonic, timestamped samples; never fills sensor gaps.
function Get-SoakPowerSummary {
    param([object[]]$Samples, [double]$MaximumGapSeconds = 15, [double]$BudgetWatts = 4)
    $continuous = $Samples.Count -ge 2
    $energy = 0.0
    $duration = 0.0
    $peak = 0.0
    $validSamples = 0
    $thermalPeak = 0
    $thermalKnown = $Samples.Count -gt 0
    for ($i = 0; $i -lt $Samples.Count; ++$i) {
        $sample = $Samples[$i]
        if ($null -eq $sample.elapsedSeconds -or [double]::IsNaN([double]$sample.elapsedSeconds) -or
            [double]::IsInfinity([double]$sample.elapsedSeconds) -or $sample.elapsedSeconds -lt 0) {
            throw 'Timestamp monotônico inválido no soak.'
        }
        $valid = $null -ne $sample.powerWatts -and -not [double]::IsNaN([double]$sample.powerWatts) -and
            -not [double]::IsInfinity([double]$sample.powerWatts) -and $sample.powerWatts -ge 0 -and
            $null -ne $sample.externallyPowered -and -not $sample.externallyPowered
        if (-not $valid) { $continuous = $false } else { ++$validSamples; $peak = [Math]::Max($peak, $sample.powerWatts) }
        if ($null -eq $sample.thermalStatus -or $sample.thermalStatus -lt 0 -or $sample.thermalStatus -gt 6) {
            $thermalKnown = $false
        } else { $thermalPeak = [Math]::Max($thermalPeak, $sample.thermalStatus) }
        if ($i -gt 0) {
            $dt = $sample.elapsedSeconds - $Samples[$i - 1].elapsedSeconds
            if ($dt -le 0) { throw 'Amostras do soak fora de ordem.' }
            if ($dt -gt $MaximumGapSeconds) { $continuous = $false }
            $duration += $dt
            if ($continuous) { $energy += 0.5 * ($sample.powerWatts + $Samples[$i - 1].powerWatts) * $dt }
        }
    }
    $average = if ($continuous -and $duration -gt 0) { $energy / $duration } else { $null }
    return [ordered]@{
        source = 'battery ibat*vbat sampled via Android HAL (whole device, not app-only)'
        samples = $Samples.Count
        validPowerSamples = $validSamples
        elapsedSeconds = $duration
        continuous = $continuous
        maximumAllowedGapSeconds = $MaximumGapSeconds
        averageWatts = $average
        peakWatts = if ($validSamples -gt 0) { $peak } else { $null }
        budgetWatts = $BudgetWatts
        withinPowerBudget = $null -ne $average -and $average -lt $BudgetWatts
        thermalStatusKnown = $thermalKnown
        peakThermalStatus = if ($thermalKnown) { $thermalPeak } else { $null }
        noThermalWarningObserved = $thermalKnown -and $thermalPeak -eq 0
        limitation = 'Amostragem não mede picos entre leituras nem prova ausência de DVFS/throttle; carga externa invalida potência.'
    }
}
