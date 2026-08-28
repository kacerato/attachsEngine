# Leitura/estatística dos relatórios nativos. Não depende de ADB.
function Find-FrameProfileSurfaceLayer {
    param([string]$Text, [string]$Component)
    $pattern = '(?m)(?:^|RequestedLayerState\{)(' + [regex]::Escape($Component) + '#\d+)(?:\s|$)'
    $matchesFound = [regex]::Matches($Text, $pattern)
    if ($matchesFound.Count -ne 1) { return $null }
    return $matchesFound[0].Groups[1].Value
}

function ConvertFrom-SurfaceFrameLatency {
    param([string]$Text)
    foreach ($line in ($Text -split "`n")) {
        # AOSP FrameTracker: desiredPresentTime, actualPresentTime, frameReadyTime.
        if ($line -match '^\s*\d+\s+(\d+)\s+\d+\s*$') {
            $timestamp = [long]$Matches[1]
            if ($timestamp -gt 0 -and $timestamp -lt [long]::MaxValue) { $timestamp }
        }
    }
}

function Get-SurfaceFrameSummary {
    param([long[]]$Timestamps)
    $sorted = @($Timestamps | Sort-Object -Unique)
    if ($sorted.Count -lt 2) { return $null }
    $intervals = @(for ($i = 1; $i -lt $sorted.Count; ++$i) { ($sorted[$i] - $sorted[$i - 1]) / 1e6 })
    $intervals = @($intervals | Sort-Object)
    $duration = ($sorted[-1] - $sorted[0]) / 1e9
    return [ordered]@{
        source = 'SurfaceFlinger FrameTracker actualPresentTime'
        frames = $sorted.Count
        elapsedSeconds = $duration
        displayedFps = ($sorted.Count - 1) / $duration
        firstPresentNs = $sorted[0]
        lastPresentNs = $sorted[-1]
        intervalMs = [ordered]@{
            p50 = $intervals[[int][Math]::Ceiling($intervals.Count * 0.50) - 1]
            p95 = $intervals[[int][Math]::Ceiling($intervals.Count * 0.95) - 1]
            p99 = $intervals[[int][Math]::Ceiling($intervals.Count * 0.99) - 1]
            max = $intervals[-1]
        }
        actualPresentTimestampsNs = $sorted
    }
}

$FrameProfileMetricNames = @('interval_ms', 'process_cpu_ms', 'thread_cpu_ms', 'acquire_wall_ms',
    'interop_wall_ms', 'record_submit_wall_ms', 'present_wall_ms')

function ConvertFrom-FrameProfileLog {
    param([string]$Text, [string]$ExpectedPid)
    $seen = @{}
    foreach ($line in ($Text -split "`n")) {
        if ($line -notmatch '\[FrameProfile\] (\{.*)$') { continue }
        $window = $Matches[1] | ConvertFrom-Json
        if ([string]$window.pid -ne $ExpectedPid) { continue }
        foreach ($field in @('elapsed_ms', 'present_fps', 'warmup_samples', 'warmup_process_cpu_max_ms')) {
            $value = $window.$field
            if ($null -eq $value -or [double]::IsNaN([double]$value) -or
                [double]::IsInfinity([double]$value) -or $value -lt 0) {
                throw "FrameProfile inválido: $field."
            }
        }
        if ($window.schemaVersion -ne 1 -or $window.instances -ne 5000 -or
            $window.frames -ne 600 -or $window.elapsed_ms -le 0 -or
            $window.width -le 0 -or $window.height -le 0 -or $window.epoch -lt 1 -or $window.window -lt 1) {
            throw 'Relatório FrameProfile incompatível ou incompleto.'
        }
        foreach ($metric in $FrameProfileMetricNames) {
            $distribution = $window.$metric
            foreach ($field in @('mean', 'p50', 'p95', 'p99', 'max')) {
                $value = [double]$distribution.$field
                if ($null -eq $distribution.$field -or [double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -lt 0) {
                    throw "FrameProfile inválido: $metric.$field."
                }
            }
            if ($distribution.p50 -gt $distribution.p95 -or $distribution.p95 -gt $distribution.p99 -or
                $distribution.p99 -gt $distribution.max -or $distribution.mean -gt $distribution.max) {
                throw "Percentis inconsistentes: $metric."
            }
        }
        $derivedFps = 1000.0 * $window.frames / $window.elapsed_ms
        if ([Math]::Abs($derivedFps - $window.present_fps) -gt 0.001 -or
            [Math]::Abs($window.interval_ms.mean * $window.frames - $window.elapsed_ms) -gt 0.001) {
            throw 'Tempo/FPS inconsistente no FrameProfile.'
        }
        $key = "$($window.epoch):$($window.window)"
        if ($seen.ContainsKey($key)) { throw "Janela FrameProfile duplicada: $key." }
        $seen[$key] = $true
        $window
    }
}

function Get-FrameProfileCapture {
    param([object[]]$Windows, [double]$MinimumSeconds)
    if ($Windows.Count -eq 0) { return $null }
    $ordered = @($Windows | Sort-Object window)
    $latest = $ordered[-1]
    $selected = @($ordered | Where-Object { $_.epoch -eq $latest.epoch })
    foreach ($window in $selected) {
        if ($window.width -ne $latest.width -or $window.height -ne $latest.height -or
            $window.build -ne $latest.build -or $window.pid -ne $latest.pid) {
            throw 'Configuração mudou dentro da captura FrameProfile.'
        }
    }
    for ($i = 1; $i -lt $selected.Count; ++$i) {
        if ($selected[$i].window -ne $selected[$i - 1].window + 1) {
            throw 'Há lacunas entre janelas FrameProfile; captura não é contínua.'
        }
    }
    $elapsedMs = ($selected | Measure-Object elapsed_ms -Sum).Sum
    if ($elapsedMs -lt 1000.0 * $MinimumSeconds) { return $null }
    $frames = ($selected | Measure-Object frames -Sum).Sum
    $metrics = [ordered]@{}
    foreach ($metric in $FrameProfileMetricNames) {
        $weightedSum = 0.0
        foreach ($window in $selected) { $weightedSum += $window.$metric.mean * $window.frames }
        $metrics[$metric] = [ordered]@{
            mean = $weightedSum / $frames
            # Não confundir maior p95 de janela com p95 global da captura.
            worstWindowP95 = ($selected | ForEach-Object { $_.$metric.p95 } | Measure-Object -Maximum).Maximum
            worstWindowP99 = ($selected | ForEach-Object { $_.$metric.p99 } | Measure-Object -Maximum).Maximum
            max = ($selected | ForEach-Object { $_.$metric.max } | Measure-Object -Maximum).Maximum
        }
    }
    return [ordered]@{
        schemaVersion = 1
        scene = 'poc-a-5000-textured-cubes'
        pid = $latest.pid
        epoch = $latest.epoch
        build = $latest.build
        frames = $frames
        elapsedSeconds = $elapsedMs / 1000.0
        presentFps = 1000.0 * $frames / $elapsedMs
        minimumWindowPresentFps = ($selected | Measure-Object present_fps -Minimum).Minimum
        metrics = $metrics
        pocA = [ordered]@{
            cpuLimitMsExclusive = 3.0
            cpuStatistic = 'maximum process CPU per present interval, after 5s warm-up'
            cpuBudgetPassed = $metrics.process_cpu_ms.max -lt 3.0
            targetDisplayedFps = 60
            displayedFpsMeasured = $false
            accepted = $false
            limitation = 'Present retornado mede envio/ritmo, não scanout; não fecha PoC-A sozinho.'
        }
        windows = $selected
    }
}
