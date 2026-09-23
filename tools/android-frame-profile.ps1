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

# Metricas de nivel de frame: viajam na propria linha [FrameProfile].
$FrameProfileFrameMetricNames = @('interval_ms', 'process_cpu_ms', 'thread_cpu_ms', 'acquire_wall_ms',
    'interop_wall_ms', 'record_submit_wall_ms', 'present_wall_ms', 'gpu_frame_ms')
# Regioes de GPU: viajam em [FrameProfilePasses], porque a linha da janela ja
# ocupava 937 dos ~1023 bytes que o Logcat entrega antes de truncar em silencio.
# A ordem espelha ae::GpuPassClass em native/core/gpu_pass_class.h.
$FrameProfilePassMetricNames = @('gpu_camera_preview_ms', 'gpu_water_simulation_ms',
    'gpu_shadow_ms', 'gpu_local_shadow_ms', 'gpu_culling_ms', 'gpu_opaque_ms',
    'gpu_coverage_ms', 'gpu_sky_ms', 'gpu_transparent_ms', 'gpu_auto_exposure_ms',
    'gpu_post_ms', 'gpu_fsr_easu_ms', 'gpu_fsr_rcas_ms', 'gpu_ui_ms', 'gpu_hzb_ms')
$FrameProfileLegacyPassMetricNames = @('gpu_shadow_ms', 'gpu_culling_ms', 'gpu_opaque_ms',
    'gpu_coverage_ms', 'gpu_sky_ms', 'gpu_transparent_ms', 'gpu_ui_ms', 'gpu_post_ms', 'gpu_hzb_ms')
$FrameProfileMetricNames = $FrameProfileFrameMetricNames + $FrameProfilePassMetricNames

# Converte os vetores compactos [mean,p50,p95,p99,max] em objetos e valida
# monotonicidade. Compartilhado pelos dois registros para que uma regiao de GPU
# receba exatamente a mesma checagem que uma metrica de frame.
function ConvertTo-FrameProfileDistributions {
    param([object]$Record, [string[]]$MetricNames, [string]$Label)
    foreach ($metric in $MetricNames) {
        $compact = @($Record.$metric)
        if ($compact.Count -ne 5) { throw "$Label invalido: $metric precisa de 5 valores." }
        $distribution = [pscustomobject]@{
            mean = $compact[0]; p50 = $compact[1]; p95 = $compact[2]; p99 = $compact[3]; max = $compact[4]
        }
        foreach ($field in @('mean', 'p50', 'p95', 'p99', 'max')) {
            $value = [double]$distribution.$field
            if ($null -eq $distribution.$field -or [double]::IsNaN($value) -or
                [double]::IsInfinity($value) -or $value -lt 0) {
                throw "$Label invalido: $metric.$field."
            }
        }
        if ($distribution.p50 -gt $distribution.p95 -or $distribution.p95 -gt $distribution.p99 -or
            $distribution.p99 -gt $distribution.max -or $distribution.mean -gt $distribution.max) {
            throw "Percentis inconsistentes: $metric."
        }
        if ($Record.PSObject.Properties.Name -contains $metric) { $Record.$metric = $distribution }
        else { $Record | Add-Member -NotePropertyName $metric -NotePropertyValue $distribution }
    }
}

# Classificação por budget emitida separadamente para não estourar a entrada do
# Logcat. Existe desde o schema 5; o schema 6 acrescenta ADPF/Game Mode/Thermal
# no mesmo instante da janela. Schema 4 permanece legível e explicitamente sem
# diagnóstico de pressão.
function ConvertFrom-FrameProfilePressureLog {
    param([string]$Text, [string]$ExpectedPid)
    $pressures = @{}
    foreach ($line in ($Text -split "`n")) {
        if ($line -notmatch '\[FrameProfilePressure\] (\{.*)$') { continue }
        $record = $Matches[1] | ConvertFrom-Json
        if ([string]$record.pid -ne $ExpectedPid) { continue }
        if ($record.schemaVersion -notin @(5, 6, 7, 8) -or $record.epoch -lt 1 -or $record.window -lt 1 -or
            $record.classification -notin @('unknown', 'within-budget', 'cpu', 'gpu', 'mixed', 'presentation')) {
            throw 'Registro FrameProfilePressure incompatível ou incompleto.'
        }
        foreach ($field in @('cpu_p95_ratio', 'gpu_p95_ratio', 'interval_p95_ratio',
                              'presentation_wait_p95_ms', 'frame_budget_ms', 'cpu_budget_ms',
                              'gpu_budget_ms', 'render_scale_min', 'render_scale_max',
                              'render_scale_end')) {
            $value = $record.$field
            if ($null -eq $value -or [double]::IsNaN([double]$value) -or
                [double]::IsInfinity([double]$value) -or [double]$value -lt 0) {
                throw "FrameProfilePressure inválido: $field."
            }
        }
        if ($record.frame_budget_ms -le 0 -or $record.cpu_budget_ms -le 0 -or
            $record.gpu_budget_ms -le 0 -or $record.render_scale_min -lt 0.5 -or
            $record.render_scale_max -gt 1.0 -or
            $record.render_scale_min -gt $record.render_scale_max -or
            $record.render_scale_end -lt $record.render_scale_min -or
            $record.render_scale_end -gt $record.render_scale_max) {
            throw 'FrameProfilePressure contém budget ou escala inválida.'
        }
        if ($record.schemaVersion -ge 6) {
            foreach ($field in @('adpf', 'adpf_gpu_work', 'game_mode', 'sustained_supported', 'sustained_enabled',
                                  'thermal_api', 'thermal_status', 'thermal_headroom_valid',
                                  'thermal_headroom', 'thermal_pressure')) {
                if ($record.PSObject.Properties.Name -notcontains $field) {
                    throw "FrameProfilePressure schema 6 sem $field."
                }
            }
            if ($record.game_mode -lt 0 -or $record.game_mode -gt 4 -or
                $record.thermal_status -lt -1 -or $record.thermal_status -gt 6 -or
                $record.thermal_pressure -notin @('none', 'light', 'severe') -or
                [double]::IsNaN([double]$record.thermal_headroom) -or
                [double]::IsInfinity([double]$record.thermal_headroom) -or
                ($record.thermal_headroom_valid -and $record.thermal_headroom -lt 0) -or
                (-not $record.thermal_headroom_valid -and $record.thermal_headroom -ne -1)) {
                throw 'FrameProfilePressure contém estado ADPF/térmico inválido.'
            }
        }
        $key = "$($record.epoch):$($record.window)"
        if ($pressures.ContainsKey($key)) { throw "Registro FrameProfilePressure duplicado: $key." }
        $pressures[$key] = $record
    }
    return $pressures
}

# RAM + memória gráfica do mesmo instante em que a janela temporal fecha.
# O wire usa arrays para permanecer abaixo do limite de uma entrada Logcat;
# aqui eles voltam a nomes explícitos antes de chegar ao relatório JSONL.
function ConvertFrom-FrameProfileMemoryLog {
    param([string]$Text, [string]$ExpectedPid)
    $records = @{}
    foreach ($line in ($Text -split "`n")) {
        if ($line -notmatch '\[FrameProfileMemory\] (\{.*)$') { continue }
        $record = $Matches[1] | ConvertFrom-Json
        if ([string]$record.pid -ne $ExpectedPid) { continue }
        if ($record.schemaVersion -notin @(7, 8) -or $record.epoch -lt 1 -or $record.window -lt 1 -or
            $record.classification -notin @('unknown', 'normal', 'warning', 'critical')) {
            throw 'Registro FrameProfileMemory incompatível ou incompleto.'
        }
        $ram = @($record.ram_bytes)
        $system = @($record.system_ram_bytes)
        $heap = @($record.gpu_heap_bytes)
        $engine = @($record.gpu_engine_bytes)
        $classes = @($record.gpu_classes)
        $ratios = @($record.ratios)
        if ($ram.Count -ne 7 -or $system.Count -ne 2 -or $heap.Count -ne 3 -or
            $engine.Count -ne 2 -or $classes.Count -ne 4 -or $ratios.Count -ne 3) {
            throw 'FrameProfileMemory contém vetores com layout inválido.'
        }
        foreach ($vector in @($ram, $system, $heap, $engine)) {
            foreach ($value in $vector) {
                if ($null -eq $value -or [decimal]$value -lt 0) {
                    throw 'FrameProfileMemory contém bytes inválidos.'
                }
            }
        }
        foreach ($class in $classes) {
            $values = @($class)
            if ($values.Count -ne 3 -or [decimal]$values[0] -lt 0 -or
                [decimal]$values[1] -lt [decimal]$values[0] -or
                [decimal]$values[2] -lt [decimal]$values[0]) {
                throw 'FrameProfileMemory contém classe Vulkan inválida.'
            }
        }
        foreach ($ratio in $ratios) {
            if ($null -eq $ratio -or [double]::IsNaN([double]$ratio) -or
                [double]::IsInfinity([double]$ratio) -or $ratio -lt -1) {
                throw 'FrameProfileMemory contém razão inválida.'
            }
        }
        if (($record.ram_valid -and $ram[1] -le 0) -or
            ($record.system_ram_valid -and ($system[0] -le 0 -or $system[1] -gt $system[0])) -or
            ($record.gpu_budget_supported -and $heap[1] -le 0) -or
            $engine[0] -ne (($classes | ForEach-Object { @($_)[0] } | Measure-Object -Sum).Sum) -or
            $engine[1] -ne (($classes | ForEach-Object { @($_)[1] } | Measure-Object -Sum).Sum)) {
            throw 'FrameProfileMemory contém totais inconsistentes.'
        }
        $names = @('buffer', 'texture', 'render_target', 'staging')
        $record | Add-Member -NotePropertyName ram_virtual_bytes -NotePropertyValue $ram[0]
        $record | Add-Member -NotePropertyName ram_rss_bytes -NotePropertyValue $ram[1]
        $record | Add-Member -NotePropertyName ram_peak_rss_bytes -NotePropertyValue $ram[2]
        $record | Add-Member -NotePropertyName ram_anon_bytes -NotePropertyValue $ram[3]
        $record | Add-Member -NotePropertyName ram_file_bytes -NotePropertyValue $ram[4]
        $record | Add-Member -NotePropertyName ram_shmem_bytes -NotePropertyValue $ram[5]
        $record | Add-Member -NotePropertyName ram_swap_bytes -NotePropertyValue $ram[6]
        $record | Add-Member -NotePropertyName system_ram_total_bytes -NotePropertyValue $system[0]
        $record | Add-Member -NotePropertyName system_ram_available_bytes -NotePropertyValue $system[1]
        $record | Add-Member -NotePropertyName gpu_heap_size_bytes -NotePropertyValue $heap[0]
        $record | Add-Member -NotePropertyName gpu_heap_budget_bytes -NotePropertyValue $heap[1]
        $record | Add-Member -NotePropertyName gpu_driver_usage_bytes -NotePropertyValue $heap[2]
        $record | Add-Member -NotePropertyName gpu_engine_used_bytes -NotePropertyValue $engine[0]
        $record | Add-Member -NotePropertyName gpu_engine_peak_bytes -NotePropertyValue $engine[1]
        for ($i = 0; $i -lt $names.Count; ++$i) {
            $values = @($classes[$i])
            foreach ($slot in 0..2) {
                $suffix = @('used_bytes', 'peak_bytes', 'limit_bytes')[$slot]
                $record | Add-Member -NotePropertyName ("gpu_{0}_{1}" -f $names[$i], $suffix) `
                    -NotePropertyValue $values[$slot]
            }
        }
        $record | Add-Member -NotePropertyName system_ram_available_ratio -NotePropertyValue $ratios[0]
        $record | Add-Member -NotePropertyName gpu_driver_usage_ratio -NotePropertyValue $ratios[1]
        $record | Add-Member -NotePropertyName gpu_engine_class_ratio -NotePropertyValue $ratios[2]
        $key = "$($record.epoch):$($record.window)"
        if ($records.ContainsKey($key)) { throw "Registro FrameProfileMemory duplicado: $key." }
        $records[$key] = $record
    }
    return $records
}

# Regioes de GPU por janela, indexadas por "epoch:window". Emitidas logo apos a
# janela correspondente; ConvertFrom-FrameProfileLog exige o par.
function ConvertFrom-FrameProfilePassLog {
    param([string]$Text, [string]$ExpectedPid)
    $passes = @{}
    $fragments = @{}
    foreach ($line in ($Text -split "`n")) {
        if ($line -notmatch '\[FrameProfilePasses\] (\{.*)$') { continue }
        $record = $Matches[1] | ConvertFrom-Json
        if ([string]$record.pid -ne $ExpectedPid) { continue }
        if ($record.schemaVersion -notin @(4, 5, 6, 7, 8) -or $record.epoch -lt 1 -or $record.window -lt 1) {
            throw 'Registro FrameProfilePasses incompativel ou incompleto.'
        }
        if ($record.PSObject.Properties.Name -notcontains 'attribution') {
            throw 'Registro FrameProfilePasses sem veredito de atribuicao.'
        }
        if ($record.attribution -notin @('resolved', 'tile-deferred')) {
            throw "FrameProfilePasses com atribuicao invalida: $($record.attribution)."
        }
        $key = "$($record.epoch):$($record.window)"
        if ($record.schemaVersion -lt 8) {
            if ($passes.ContainsKey($key) -or $fragments.ContainsKey($key)) {
                throw "Registro FrameProfilePasses duplicado: $key."
            }
            $available = @($FrameProfilePassMetricNames | Where-Object { $record.PSObject.Properties.Name -contains $_ })
            foreach ($required in $FrameProfileLegacyPassMetricNames) {
                if ($available -notcontains $required) { throw "FrameProfilePasses legado sem ${required}: $key." }
            }
            ConvertTo-FrameProfileDistributions -Record $record -MetricNames $available -Label 'FrameProfilePasses'
            $passes[$key] = $record
            continue
        }
        if ($passes.ContainsKey($key) -or $record.parts -ne 3 -or $record.part -notin @(0, 1, 2)) {
            throw "Fragmento FrameProfilePasses inválido: $key."
        }
        $part = [int]$record.part
        $expected = @($FrameProfilePassMetricNames | Select-Object -Skip ($part * 5) -First 5)
        $present = @($FrameProfilePassMetricNames | Where-Object { $record.PSObject.Properties.Name -contains $_ })
        if (@(Compare-Object $expected $present).Count -ne 0) {
            throw "Fragmento FrameProfilePasses com métricas divergentes: $key/$part."
        }
        ConvertTo-FrameProfileDistributions -Record $record -MetricNames $expected -Label 'FrameProfilePasses'
        if (-not $fragments.ContainsKey($key)) { $fragments[$key] = @{} }
        if ($fragments[$key].ContainsKey($part)) { throw "Fragmento FrameProfilePasses duplicado: $key/$part." }
        $fragments[$key][$part] = $record
    }
    foreach ($key in $fragments.Keys) {
        $pieces = $fragments[$key]
        if ($pieces.Count -ne 3) { throw "Fragmentos FrameProfilePasses incompletos: $key." }
        $first = $pieces[0]
        foreach ($part in 1..2) {
            $next = $pieces[$part]
            if ($next.schemaVersion -ne $first.schemaVersion -or $next.pid -ne $first.pid -or
                $next.attribution -ne $first.attribution -or
                $next.collapsed_frames -ne $first.collapsed_frames -or
                $next.attribution_samples -ne $first.attribution_samples) {
                throw "Fragmentos FrameProfilePasses inconsistentes: $key."
            }
            foreach ($metric in @($FrameProfilePassMetricNames | Select-Object -Skip ($part * 5) -First 5)) {
                $first | Add-Member -NotePropertyName $metric -NotePropertyValue $next.$metric
            }
        }
        $passes[$key] = $first
    }
    return $passes
}

function ConvertFrom-FrameProfileContextLog {
    param([string]$Text, [string]$ExpectedPid)
    $seen = @{}
    foreach ($line in ($Text -split "`n")) {
        if ($line -notmatch '\[FrameProfileContext\] (\{.*)$') { continue }
        $context = $Matches[1] | ConvertFrom-Json
        if ([string]$context.pid -ne $ExpectedPid) { continue }
        if ($context.schemaVersion -notin @(2, 3, 4, 5, 6, 7, 8) -or $context.epoch -lt 1 -or
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
        if ($context.PSObject.Properties.Name -notcontains 'camera_mode') {
            # Capturas schema v2 anteriores à rota de câmera continuam legíveis;
            # inferimos o modo a partir do único sinal que já existia.
            $inferredMode = if ($context.camera_locked) { 'locked' } else { 'free' }
            $context | Add-Member -NotePropertyName camera_mode -NotePropertyValue $inferredMode
            $context | Add-Member -NotePropertyName camera_route_fingerprint -NotePropertyValue '0000000000000000'
            $context | Add-Member -NotePropertyName camera_route_frame_ordinal -NotePropertyValue 0
            $context | Add-Member -NotePropertyName camera_route_tick_count -NotePropertyValue 0
        }
        if ($context.camera_mode -notin @('free', 'locked', 'route')) {
            throw 'FrameProfileContext contém camera_mode inválido.'
        }
        if ($context.camera_mode -eq 'route' -and
            ($context.camera_route_fingerprint -notmatch '^[0-9a-f]{16}$' -or
             $context.camera_route_fingerprint -eq '0000000000000000' -or
             $context.camera_route_tick_count -lt 1)) {
            throw 'FrameProfileContext em modo route exige camera_route_fingerprint e tick_count válidos.'
        }
        if ($context.PSObject.Properties.Name -notcontains 'hzb_tested_draws') {
            # Capturas anteriores ao HZB continuam legíveis; zero é o estado
            # real quando a oclusão HZB está desativada, não uma suposição.
            $context | Add-Member -NotePropertyName hzb_tested_draws -NotePropertyValue 0
            $context | Add-Member -NotePropertyName hzb_occluded_draws -NotePropertyValue 0
            $context | Add-Member -NotePropertyName hzb_revived_draws -NotePropertyValue 0
            $context | Add-Member -NotePropertyName hzb_motion_skipped_draws -NotePropertyValue 0
        }
        if ($context.PSObject.Properties.Name -notcontains 'hzb_motion_skipped_draws') {
            $context | Add-Member -NotePropertyName hzb_motion_skipped_draws -NotePropertyValue 0
        }
        if ($context.PSObject.Properties.Name -notcontains 'hzb_budget_skipped_draws') {
            $context | Add-Member -NotePropertyName hzb_budget_skipped_draws -NotePropertyValue 0
        }
        if ($context.PSObject.Properties.Name -notcontains 'hzb_enabled') {
            $context | Add-Member -NotePropertyName hzb_enabled -NotePropertyValue $false
            $context | Add-Member -NotePropertyName lod_enabled -NotePropertyValue $false
            $context | Add-Member -NotePropertyName package_version -NotePropertyValue 0
            $context | Add-Member -NotePropertyName render_draws -NotePropertyValue $context.draws
            $context | Add-Member -NotePropertyName lod_groups -NotePropertyValue 0
        }
        if ($context.schemaVersion -ge 4) {
            foreach ($field in @('lod_error_px', 'coverage_lod_error_px', 'render_scale',
                                  'render_width', 'render_height')) {
                if ($context.PSObject.Properties.Name -notcontains $field) {
                    throw "FrameProfileContext schema 4 sem $field."
                }
            }
            if ([double]::IsNaN([double]$context.lod_error_px) -or
                [double]::IsInfinity([double]$context.lod_error_px) -or
                $context.lod_error_px -le 0 -or
                [double]::IsNaN([double]$context.coverage_lod_error_px) -or
                [double]::IsInfinity([double]$context.coverage_lod_error_px) -or
                $context.coverage_lod_error_px -le 0 -or
                [double]::IsNaN([double]$context.render_scale) -or
                [double]::IsInfinity([double]$context.render_scale) -or
                $context.render_scale -lt 0.5 -or $context.render_scale -gt 1.0 -or
                $context.render_width -lt 1 -or $context.render_height -lt 1) {
                throw 'FrameProfileContext schema 4 contém política de resolução/LOD inválida.'
            }
        } else {
            # Campos introduzidos no schema 4. Mantêm capturas antigas
            # comparáveis sem fingir que conhecemos os budgets que as geraram.
            $context | Add-Member -NotePropertyName lod_error_px -NotePropertyValue 0.0
            $context | Add-Member -NotePropertyName coverage_lod_error_px -NotePropertyValue 0.0
            $context | Add-Member -NotePropertyName render_scale -NotePropertyValue 1.0
            $context | Add-Member -NotePropertyName render_width -NotePropertyValue $context.width
            $context | Add-Member -NotePropertyName render_height -NotePropertyValue $context.height
        }
        if ($context.hzb_tested_draws -lt 0 -or $context.hzb_occluded_draws -lt 0 -or
            $context.hzb_revived_draws -lt 0 -or $context.hzb_motion_skipped_draws -lt 0 -or
            $context.hzb_budget_skipped_draws -lt 0 -or
            $context.hzb_occluded_draws -gt $context.hzb_tested_draws) {
            throw 'FrameProfileContext contém contadores de HZB inconsistentes.'
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
    $passes = ConvertFrom-FrameProfilePassLog -Text $Text -ExpectedPid $ExpectedPid
    $pressures = ConvertFrom-FrameProfilePressureLog -Text $Text -ExpectedPid $ExpectedPid
    $memories = ConvertFrom-FrameProfileMemoryLog -Text $Text -ExpectedPid $ExpectedPid
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
        if ($window.schemaVersion -notin @(4, 5, 6, 7, 8) -or
            ($null -ne $ExpectedInstances -and $window.instances -ne $ExpectedInstances) -or
            $window.frames -ne 600 -or $window.elapsed_ms -le 0 -or
            $window.width -le 0 -or $window.height -le 0 -or $window.epoch -lt 1 -or $window.window -lt 1) {
            throw 'Relatório FrameProfile incompatível ou incompleto.'
        }
        # Schema v3 anterior ao hotspot mapping não carregava o ordinal/volume
        # no fechamento de cada janela. Mantemos leitura retrocompatível.
        if ($window.PSObject.Properties.Name -notcontains 'route_frame') {
            $window | Add-Member -NotePropertyName route_frame -NotePropertyValue 0
            $window | Add-Member -NotePropertyName visible_draws -NotePropertyValue 0
            $window | Add-Member -NotePropertyName visible_triangles -NotePropertyValue 0
        }
        if ($window.route_frame -lt 0 -or $window.visible_draws -lt 0 -or
            $window.visible_triangles -lt 0) {
            throw 'FrameProfile contém contexto de hotspot inválido.'
        }
        ConvertTo-FrameProfileDistributions -Record $window -MetricNames $FrameProfileFrameMetricNames -Label 'FrameProfile'
        # Uma janela sem suas regioes e captura incompleta, nao uma janela cujos
        # passes custaram zero: aceitar o orfao produziria atribuicao silenciosa
        # de 0 ms para opaco/folhagem/ceu e o relatorio pareceria valido.
        $passKey = "$($window.epoch):$($window.window)"
        if (-not $passes.ContainsKey($passKey)) {
            throw "Janela FrameProfile sem regioes de GPU correspondentes: $passKey."
        }
        $passRecord = $passes[$passKey]
        if ($passRecord.schemaVersion -ne $window.schemaVersion) {
            throw "Schemas de FrameProfile e FrameProfilePasses divergem: $passKey."
        }
        foreach ($metric in $FrameProfilePassMetricNames) {
            $value = if ($passRecord.PSObject.Properties.Name -contains $metric) { $passRecord.$metric } else { $null }
            $window | Add-Member -NotePropertyName $metric -NotePropertyValue $value
        }
        $window | Add-Member -NotePropertyName gpuPassMetricsMissing -NotePropertyValue @(
            $FrameProfilePassMetricNames | Where-Object { $null -eq $window.$_ })
        # Numa GPU TBDR os timestamps internos ao render pass podem resolver
        # todos no fim do tile: a primeira regiao absorve o frame e as demais
        # medem ~0. Os numeros continuam no relatorio porque sao o que o
        # hardware devolveu, mas viajam marcados -- consumir uma divisao que o
        # hardware nao fez enviaria o ciclo de otimizacao atras do alvo errado.
        $window | Add-Member -NotePropertyName gpuPassAttribution -NotePropertyValue $passRecord.attribution
        if ($window.schemaVersion -ge 7) {
            if (-not $memories.ContainsKey($passKey)) {
                throw "Janela FrameProfile sem memória correspondente: $passKey."
            }
            $memory = $memories[$passKey]
            if ($memory.schemaVersion -ne $window.schemaVersion) {
                throw "Schemas de FrameProfile e FrameProfileMemory divergem: $passKey."
            }
            foreach ($field in @('classification', 'ram_valid', 'ram_virtual_bytes', 'ram_rss_bytes',
                                  'ram_peak_rss_bytes', 'ram_anon_bytes', 'ram_file_bytes',
                                  'ram_shmem_bytes', 'ram_swap_bytes', 'system_ram_valid',
                                  'system_ram_total_bytes', 'system_ram_available_bytes',
                                  'gpu_budget_supported', 'gpu_unified', 'gpu_heap_size_bytes',
                                  'gpu_heap_budget_bytes', 'gpu_driver_usage_bytes',
                                  'gpu_engine_used_bytes', 'gpu_engine_peak_bytes',
                                  'gpu_buffer_used_bytes', 'gpu_buffer_peak_bytes', 'gpu_buffer_limit_bytes',
                                  'gpu_texture_used_bytes', 'gpu_texture_peak_bytes', 'gpu_texture_limit_bytes',
                                  'gpu_render_target_used_bytes', 'gpu_render_target_peak_bytes',
                                  'gpu_render_target_limit_bytes', 'gpu_staging_used_bytes',
                                  'gpu_staging_peak_bytes', 'gpu_staging_limit_bytes',
                                  'system_ram_available_ratio', 'gpu_driver_usage_ratio',
                                  'gpu_engine_class_ratio')) {
                $targetName = if ($field -eq 'classification') { 'memory_classification' } else { $field }
                $window | Add-Member -NotePropertyName $targetName -NotePropertyValue $memory.$field
            }
        } else {
            $window | Add-Member -NotePropertyName memory_classification -NotePropertyValue 'unknown'
            $window | Add-Member -NotePropertyName ram_valid -NotePropertyValue $false
            $window | Add-Member -NotePropertyName gpu_budget_supported -NotePropertyValue $false
        }
        if ($window.schemaVersion -ge 5) {
            if (-not $pressures.ContainsKey($passKey)) {
                throw "Janela FrameProfile sem pressão correspondente: $passKey."
            }
            $pressure = $pressures[$passKey]
            foreach ($field in @('classification', 'cpu_p95_ratio', 'gpu_p95_ratio',
                                  'interval_p95_ratio', 'presentation_wait_p95_ms',
                                  'frame_budget_ms', 'cpu_budget_ms', 'gpu_budget_ms',
                                  'render_scale_min', 'render_scale_max', 'render_scale_end')) {
                $window | Add-Member -NotePropertyName $field -NotePropertyValue $pressure.$field
            }
            if ($window.schemaVersion -ge 6) {
                foreach ($field in @('adpf', 'adpf_gpu_work', 'game_mode', 'sustained_supported',
                                      'sustained_enabled', 'thermal_api', 'thermal_status',
                                      'thermal_headroom_valid', 'thermal_headroom',
                                      'thermal_pressure')) {
                    $window | Add-Member -NotePropertyName $field -NotePropertyValue $pressure.$field
                }
            } else {
                $window | Add-Member -NotePropertyName adpf -NotePropertyValue $false
                $window | Add-Member -NotePropertyName adpf_gpu_work -NotePropertyValue $false
                $window | Add-Member -NotePropertyName game_mode -NotePropertyValue 0
                $window | Add-Member -NotePropertyName sustained_supported -NotePropertyValue $false
                $window | Add-Member -NotePropertyName sustained_enabled -NotePropertyValue $false
                $window | Add-Member -NotePropertyName thermal_api -NotePropertyValue $false
                $window | Add-Member -NotePropertyName thermal_status -NotePropertyValue -1
                $window | Add-Member -NotePropertyName thermal_headroom_valid -NotePropertyValue $false
                $window | Add-Member -NotePropertyName thermal_headroom -NotePropertyValue -1.0
                $window | Add-Member -NotePropertyName thermal_pressure -NotePropertyValue 'none'
            }
        } else {
            $window | Add-Member -NotePropertyName classification -NotePropertyValue 'unknown'
            foreach ($field in @('cpu_p95_ratio', 'gpu_p95_ratio', 'interval_p95_ratio',
                                  'presentation_wait_p95_ms', 'frame_budget_ms', 'cpu_budget_ms',
                                  'gpu_budget_ms')) {
                $window | Add-Member -NotePropertyName $field -NotePropertyValue 0.0
            }
            $window | Add-Member -NotePropertyName render_scale_min -NotePropertyValue 1.0
            $window | Add-Member -NotePropertyName render_scale_max -NotePropertyValue 1.0
            $window | Add-Member -NotePropertyName render_scale_end -NotePropertyValue 1.0
        }
        # As regioes particionam o frame: somadas nao podem exceder o tempo total
        # de GPU alem da tolerancia de arredondamento de 4 casas por metrica.
        $passSum = 0.0
        foreach ($metric in $FrameProfilePassMetricNames) {
            if ($null -ne $window.$metric) { $passSum += $window.$metric.mean }
        }
        if ($passSum -gt $window.gpu_frame_ms.mean + 0.01) {
            throw "Regioes de GPU somam mais que o frame: $passKey."
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
    # Entradas construídas por consumidores antigos (ou por testes unitários)
    # podem não ter passado pelo parser schema 5. Preservamos a captura, mas a
    # ausência de evidência permanece unknown/zero em vez de ser inferida.
    foreach ($window in $selected) {
        if ($window.PSObject.Properties.Name -notcontains 'classification') {
            $window | Add-Member -NotePropertyName classification -NotePropertyValue 'unknown'
            foreach ($field in @('cpu_p95_ratio', 'gpu_p95_ratio', 'interval_p95_ratio',
                                  'presentation_wait_p95_ms', 'frame_budget_ms', 'cpu_budget_ms',
                                  'gpu_budget_ms')) {
                $window | Add-Member -NotePropertyName $field -NotePropertyValue 0.0
            }
            $window | Add-Member -NotePropertyName render_scale_min -NotePropertyValue 1.0
            $window | Add-Member -NotePropertyName render_scale_max -NotePropertyValue 1.0
            $window | Add-Member -NotePropertyName render_scale_end -NotePropertyValue 1.0
        }
        if ($window.PSObject.Properties.Name -notcontains 'thermal_pressure') {
            $window | Add-Member -NotePropertyName adpf -NotePropertyValue $false
            $window | Add-Member -NotePropertyName adpf_gpu_work -NotePropertyValue $false
            $window | Add-Member -NotePropertyName game_mode -NotePropertyValue 0
            $window | Add-Member -NotePropertyName sustained_supported -NotePropertyValue $false
            $window | Add-Member -NotePropertyName sustained_enabled -NotePropertyValue $false
            $window | Add-Member -NotePropertyName thermal_api -NotePropertyValue $false
            $window | Add-Member -NotePropertyName thermal_status -NotePropertyValue -1
            $window | Add-Member -NotePropertyName thermal_headroom_valid -NotePropertyValue $false
            $window | Add-Member -NotePropertyName thermal_headroom -NotePropertyValue -1.0
            $window | Add-Member -NotePropertyName thermal_pressure -NotePropertyValue 'none'
        }
        if ($window.PSObject.Properties.Name -notcontains 'memory_classification') {
            $window | Add-Member -NotePropertyName memory_classification -NotePropertyValue 'unknown'
            $window | Add-Member -NotePropertyName ram_valid -NotePropertyValue $false
            $window | Add-Member -NotePropertyName gpu_budget_supported -NotePropertyValue $false
        }
    }
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
        $available = @($selected | Where-Object { $null -ne $_.$metric }).Count
        if ($available -ne 0 -and $available -ne $selected.Count) {
            throw "Disponibilidade da métrica $metric muda dentro da captura."
        }
        if ($available -eq 0) { continue }
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
    # Janela sem veredito nunca vira 'resolved': isso afirmaria atribuicao que
    # ninguem mediu. Toda janela vinda de ConvertFrom-FrameProfileLog carrega o
    # campo, entao 'unknown' so aparece para entrada construida a mao.
    $attributions = @($selected | ForEach-Object {
        if ($_.PSObject.Properties.Name -contains 'gpuPassAttribution') { $_.gpuPassAttribution }
        else { 'unknown' } })
    $captureAttribution = if ($attributions -contains 'tile-deferred') { 'tile-deferred' }
                          elseif ($attributions -contains 'unknown') { 'unknown' }
                          else { 'resolved' }
    $pressureCounts = [ordered]@{}
    foreach ($kind in @('within-budget', 'cpu', 'gpu', 'mixed', 'presentation', 'unknown')) {
        $pressureCounts[$kind] = @($selected | Where-Object classification -eq $kind).Count
    }
    $dominantPressure = @($pressureCounts.GetEnumerator() |
        Sort-Object -Property @{ Expression = 'Value'; Descending = $true },
                              @{ Expression = 'Name'; Descending = $false })[0].Name
    $memoryAvailable = $latest.PSObject.Properties.Name -contains 'gpu_engine_used_bytes'
    $memoryPressureCounts = [ordered]@{}
    foreach ($kind in @('normal', 'warning', 'critical', 'unknown')) {
        $memoryPressureCounts[$kind] = @($selected | Where-Object memory_classification -eq $kind).Count
    }
    $memorySummary = if ($memoryAvailable) {
        [ordered]@{
            available = $true
            classifications = $memoryPressureCounts
            ramValid = [bool]$latest.ram_valid
            maximumRssBytes = ($selected | Measure-Object ram_rss_bytes -Maximum).Maximum
            peakRssBytes = ($selected | Measure-Object ram_peak_rss_bytes -Maximum).Maximum
            maximumSwapBytes = ($selected | Measure-Object ram_swap_bytes -Maximum).Maximum
            minimumSystemAvailableBytes = ($selected | Measure-Object system_ram_available_bytes -Minimum).Minimum
            minimumSystemAvailableRatio = ($selected | Measure-Object system_ram_available_ratio -Minimum).Minimum
            gpuUnifiedMemory = [bool]$latest.gpu_unified
            gpuDriverBudgetSupported = [bool]$latest.gpu_budget_supported
            gpuHeapSizeBytes = [decimal]$latest.gpu_heap_size_bytes
            gpuHeapBudgetBytes = [decimal]$latest.gpu_heap_budget_bytes
            maximumGpuDriverUsageBytes = ($selected | Measure-Object gpu_driver_usage_bytes -Maximum).Maximum
            maximumGpuDriverUsageRatio = ($selected | Measure-Object gpu_driver_usage_ratio -Maximum).Maximum
            maximumEngineAllocationBytes = ($selected | Measure-Object gpu_engine_used_bytes -Maximum).Maximum
            peakEngineAllocationBytes = ($selected | Measure-Object gpu_engine_peak_bytes -Maximum).Maximum
            endingClasses = [ordered]@{
                buffer = [ordered]@{ usedBytes = $latest.gpu_buffer_used_bytes; peakBytes = $latest.gpu_buffer_peak_bytes; limitBytes = $latest.gpu_buffer_limit_bytes }
                texture = [ordered]@{ usedBytes = $latest.gpu_texture_used_bytes; peakBytes = $latest.gpu_texture_peak_bytes; limitBytes = $latest.gpu_texture_limit_bytes }
                renderTarget = [ordered]@{ usedBytes = $latest.gpu_render_target_used_bytes; peakBytes = $latest.gpu_render_target_peak_bytes; limitBytes = $latest.gpu_render_target_limit_bytes }
                staging = [ordered]@{ usedBytes = $latest.gpu_staging_used_bytes; peakBytes = $latest.gpu_staging_peak_bytes; limitBytes = $latest.gpu_staging_limit_bytes }
            }
        }
    } else {
        [ordered]@{ available = $false; classifications = $memoryPressureCounts }
    }
    return [ordered]@{
        schemaVersion = 4
        scene = $context.scene
        # 'tile-deferred': o hardware resolveu os timestamps do render pass no
        # fim do tile e as metricas gpu_*_ms abaixo NAO atribuem custo por
        # regiao. Os marcadores de debug continuam validos para captura AGI.
        gpuPassAttribution = $captureAttribution
        gpuPassMetricsMissing = @($FrameProfilePassMetricNames | Where-Object { -not $metrics.Contains($_) })
        context = $context
        pid = $latest.pid
        epoch = $latest.epoch
        build = $latest.build
        frames = $frames
        elapsedSeconds = $elapsedMs / 1000.0
        presentFps = 1000.0 * $frames / $elapsedMs
        minimumWindowPresentFps = ($selected | Measure-Object present_fps -Minimum).Minimum
        pressure = [ordered]@{
            dominant = $dominantPressure
            windows = $pressureCounts
            worstCpuP95BudgetRatio = ($selected | Measure-Object cpu_p95_ratio -Maximum).Maximum
            worstGpuP95BudgetRatio = ($selected | Measure-Object gpu_p95_ratio -Maximum).Maximum
            worstIntervalP95BudgetRatio = ($selected | Measure-Object interval_p95_ratio -Maximum).Maximum
            minimumRenderScale = ($selected | Measure-Object render_scale_min -Minimum).Minimum
            maximumRenderScale = ($selected | Measure-Object render_scale_max -Maximum).Maximum
            endingRenderScale = $latest.render_scale_end
            adpf = [bool]$latest.adpf
            adpfGpuWork = [bool]$latest.adpf_gpu_work
            gameMode = [int]$latest.game_mode
            sustainedPerformanceSupported = [bool]$latest.sustained_supported
            sustainedPerformanceEnabled = [bool]$latest.sustained_enabled
            thermalApi = [bool]$latest.thermal_api
            thermalStatus = [int]$latest.thermal_status
            thermalHeadroomValid = [bool]$latest.thermal_headroom_valid
            thermalHeadroom = [double]$latest.thermal_headroom
            thermalPressure = [string]$latest.thermal_pressure
        }
        memory = $memorySummary
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
