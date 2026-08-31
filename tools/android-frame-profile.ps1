# Leitura/estatística dos relatórios nativos. Não depende de ADB.
function Write-ProfileEvidence {
    param([string]$Path, [object]$Value)
    # Append on the host as samples arrive: cancelling the process must not lose
    # an entire soak held in memory. Each complete JSONL line is independently readable.
    [IO.File]::AppendAllText($Path, ($Value | ConvertTo-Json -Depth 10 -Compress) + "`n", [Text.UTF8Encoding]::new($false))
}

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

function New-SurfaceFrameCaptureState {
    return [pscustomobject]@{
        timestamps = [Collections.Generic.SortedSet[long]]::new()
        pendingLines = [Collections.Generic.List[string]]::new()
        lastTimestamp = 0L
        continuous = $true
        polls = 0
    }
}

function Merge-SurfaceFrameSnapshot {
    param(
        [Parameter(Mandatory = $true)][object]$State,
        [Parameter(Mandatory = $true)][long[]]$Timestamps
    )
    $times = @($Timestamps | Sort-Object -Unique)
    if ($times.Count -eq 0) { return @() }
    ++$State.polls

    # FrameTracker guarda uma janela circular curta. O coletor dedicado deve
    # produzir sobreposição; ainda aceitamos o próximo intervalo observável
    # quando o lote girou exatamente na fronteira. Uma lacuna maior que três
    # intervalos típicos permanece inválida porque não é possível distinguir
    # stall real de timestamps sobrescritos.
    if ($State.lastTimestamp -gt 0 -and $times[0] -gt $State.lastTimestamp) {
        $rawIntervals = @(for ($i = 1; $i -lt $times.Count; ++$i) {
            [double]($times[$i] - $times[$i - 1])
        })
        $intervals = @($rawIntervals | Sort-Object)
        $typicalInterval = if ($intervals.Count -gt 0) {
            $intervals[[int][Math]::Floor($intervals.Count / 2)]
        } else { 16666667.0 }
        $maximumAdjacentGap = [Math]::Max(40000000.0, $typicalInterval * 3.25)
        if (($times[0] - $State.lastTimestamp) -gt $maximumAdjacentGap) {
            $State.continuous = $false
        }
    }

    $newTimes = [Collections.Generic.List[long]]::new()
    foreach ($time in $times) {
        if ($State.timestamps.Add($time)) { $newTimes.Add($time) }
    }
    $State.lastTimestamp = $times[-1]
    return $newTimes.ToArray()
}

function Get-SurfaceFrameSummary {
    param([long[]]$Timestamps)
    $sorted = @($Timestamps | Sort-Object -Unique)
    if ($sorted.Count -lt 2) { return $null }
    $intervals = @(for ($i = 1; $i -lt $sorted.Count; ++$i) { ($sorted[$i] - $sorted[$i - 1]) / 1e6 })
    $intervals = @($intervals | Sort-Object)
    $duration = ($sorted[-1] - $sorted[0]) / 1e9
    # Sliding 1 s windows (including stalls), not a mean over the entire soak.
    $minimumRollingFps = $null
    $end = 0
    for ($start = 0; $start -lt $sorted.Count -and $sorted[$start] + 1000000000L -le $sorted[-1]; ++$start) {
        while ($end + 1 -lt $sorted.Count -and $sorted[$end + 1] -le $sorted[$start] + 1000000000L) { ++$end }
        $fps = $end - $start
        if ($null -eq $minimumRollingFps -or $fps -lt $minimumRollingFps) { $minimumRollingFps = $fps }
    }
    return [ordered]@{
        source = 'SurfaceFlinger FrameTracker actualPresentTime'
        interpretation = 'tracked layer frames made visible; capture window is independent from engine profile windows'
        frames = $sorted.Count
        elapsedSeconds = $duration
        displayedFps = ($sorted.Count - 1) / $duration
        minimumRollingSecondFps = $minimumRollingFps
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
    'interop_wall_ms', 'record_submit_wall_ms', 'present_wall_ms', 'gpu_frame_ms',
    'gpu_geometry_ms', 'gpu_background_ms', 'gpu_transparent_ms')

function ConvertFrom-FrameProfileContextLog {
    param([string]$Text, [string]$ExpectedPid)
    $seen = @{}
    foreach ($line in ($Text -split "`n")) {
        if ($line -notmatch '\[FrameProfileContext\] (\{.*)$') { continue }
        $context = $Matches[1] | ConvertFrom-Json
        if ([string]$context.pid -ne $ExpectedPid) { continue }
        if ($context.schemaVersion -ne 1 -or $context.epoch -lt 1 -or
            -not $context.scene -or $context.scene -notmatch '^[a-z0-9-]+$' -or
            $context.content_fingerprint -notmatch '^[0-9a-f]{16}$' -or
            $context.target_fps -lt 1 -or $context.instances -lt 1 -or
            $context.width -lt 1 -or $context.height -lt 1 -or
            $context.draws -lt 0 -or $context.materials -lt 0 -or
            $context.textures -lt 0 -or $context.triangles -lt 0) {
            throw 'FrameProfileContext incompatível ou incompleto.'
        }
        if ($context.PSObject.Properties.Name -notcontains 'gpu_isolation') {
            # Capturas schema v1 anteriores ao isolamento continuam legíveis e
            # representam o caminho de qualidade completo.
            $context | Add-Member -NotePropertyName gpu_isolation -NotePropertyValue 'full'
        }
        if ($context.gpu_isolation -notin @('full', 'no-normal', 'no-ibl', 'base-color')) {
            throw 'FrameProfileContext contém gpu_isolation inválido.'
        }
        $pose = @($context.camera_pose)
        if ($pose.Count -ne 5) { throw 'FrameProfileContext exige camera_pose com 5 valores.' }
        foreach ($value in $pose) {
            if ($null -eq $value -or [double]::IsNaN([double]$value) -or
                [double]::IsInfinity([double]$value)) { throw 'FrameProfileContext contém câmera inválida.' }
        }
        $key = "$($context.pid):$($context.epoch)"
        $wire = $context | ConvertTo-Json -Depth 5 -Compress
        if ($seen.ContainsKey($key)) {
            if ($seen[$key] -cne $wire) { throw "FrameProfileContext conflitante: $key." }
            continue
        }
        $seen[$key] = $wire
        $context
    }
}

function ConvertFrom-FrameProfileLog {
    param([string]$Text, [string]$ExpectedPid, [Nullable[int]]$ExpectedInstances = 5000)
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
        if ($window.schemaVersion -ne 3 -or
            ($null -ne $ExpectedInstances -and $window.instances -ne $ExpectedInstances) -or
            $window.frames -ne 600 -or $window.elapsed_ms -le 0 -or
            $window.width -le 0 -or $window.height -le 0 -or $window.epoch -lt 1 -or $window.window -lt 1) {
            throw 'Relatório FrameProfile incompatível ou incompleto.'
        }
        foreach ($metric in $FrameProfileMetricNames) {
            $compact = @($window.$metric)
            if ($compact.Count -ne 5) { throw "FrameProfile inválido: $metric precisa de 5 valores." }
            $distribution = [pscustomobject]@{
                mean = $compact[0]; p50 = $compact[1]; p95 = $compact[2]; p99 = $compact[3]; max = $compact[4]
            }
            $window.$metric = $distribution
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
            # Distribuições usam 4 casas para permanecer abaixo do limite de
            # 1023 bytes por entrada do logger Android. Em 600 frames, o erro
            # máximo de arredondamento da média é 0,03 ms.
            [Math]::Abs($window.interval_ms.mean * $window.frames - $window.elapsed_ms) -gt 0.05) {
            throw 'Tempo/FPS inconsistente no FrameProfile.'
        }
        $key = "$($window.epoch):$($window.window)"
        if ($seen.ContainsKey($key)) { throw "Janela FrameProfile duplicada: $key." }
        $seen[$key] = $true
        $window
    }
}

function Get-FrameProfileCapture {
    param([object[]]$Windows, [double]$MinimumSeconds,
          [string]$Scene = 'poc-a-5000-textured-cubes',
          [object[]]$Contexts)
    if ($Windows.Count -eq 0) { return $null }
    $ordered = @($Windows | Sort-Object window)
    $latest = $ordered[-1]
    $selected = @($ordered | Where-Object { $_.epoch -eq $latest.epoch })
    $matchingContexts = @($Contexts | Where-Object { $_.pid -eq $latest.pid -and $_.epoch -eq $latest.epoch })
    if ($matchingContexts.Count -ne 1) {
        throw "Captura exige exatamente um FrameProfileContext para PID/epoch; encontrados $($matchingContexts.Count)."
    }
    $context = $matchingContexts[0]
    if ($context.scene -cne $Scene) {
        throw "Cena nativa '$($context.scene)' difere da cena solicitada '$Scene'."
    }
    if ($context.instances -ne $latest.instances -or $context.width -ne $latest.width -or
        $context.height -ne $latest.height) {
        throw 'FrameProfileContext não corresponde às janelas da captura.'
    }
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
        schemaVersion = 2
        scene = $context.scene
        context = $context
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
