Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../../tools/android-frame-profile.ps1')
$count = 0
function Assert-Profile { param([bool]$Value); if (-not $Value) { throw 'Asserção de FrameProfile falhou.' } }
function Test-Profile { param([string]$Name, [scriptblock]$Body); & $Body; ++$script:count; Write-Host "PASS: $Name" }
function New-TestWindow {
    param([int]$Index = 1, [int]$Epoch = 1)
    $window = [ordered]@{ schemaVersion = 4; pid = 7; epoch = $Epoch; window = $Index; build = 'debug';
        instances = 5000; width = 2772; height = 1280; frames = 600; elapsed_ms = 10000.0;
        present_fps = 60.0; warmup_samples = 300; warmup_process_cpu_max_ms = 4.0 }
    foreach ($metric in $FrameProfileMetricNames) {
        # Regioes de GPU particionam o frame, entao a soma delas precisa caber
        # dentro de gpu_frame_ms; 6 x 0,1 cabe no 1,0 das metricas de frame.
        $value = if ($metric -eq 'interval_ms') { 10000.0 / 600 }
                 elseif ($FrameProfilePassMetricNames -contains $metric) { 0.1 }
                 else { 1.0 }
        $window[$metric] = [pscustomobject]@{ mean = $value; p50 = $value; p95 = $value; p99 = $value; max = $value }
    }
    return [pscustomobject]$window
}
function New-TestContext {
    param([int]$Epoch = 1, [string]$Scene = 'poc-a-5000-textured-cubes', [int]$Instances = 5000)
    return [pscustomobject][ordered]@{
        schemaVersion = 2; pid = 7; epoch = $Epoch; scene = $Scene
        content_fingerprint = '0123456789abcdef'; target_fps = 60; camera_locked = $true
        gpu_isolation = 'full'
        camera_pose = @(1.0, 2.0, 3.0, 0.25, -0.5); draws = $Instances
        materials = 0; textures = 0; triangles = 0; instances = $Instances
        width = 2772; height = 1280
    }
}
function Convert-TestContext {
    param($Context)
    return '[FrameProfileContext] ' + ($Context | ConvertTo-Json -Depth 5 -Compress)
}
function Get-TestCapture {
    param([object[]]$Windows, [double]$MinimumSeconds,
          [string]$Scene = 'poc-a-5000-textured-cubes')
    $latest = @($Windows | Sort-Object window)[-1]
    $context = New-TestContext -Epoch $latest.epoch -Scene $Scene -Instances $latest.instances
    $context.width = $latest.width; $context.height = $latest.height
    return Get-FrameProfileCapture -Windows $Windows -MinimumSeconds $MinimumSeconds `
        -Scene $Scene -Contexts @($context)
}
function Convert-TestWindow {
    param($Window, [switch]$OmitPasses)
    # Espelha o emissor nativo: a janela leva as metricas de frame, e as regioes
    # de GPU saem numa segunda entrada de Logcat pareada por epoch/window.
    $wire = $Window | ConvertTo-Json -Depth 5 | ConvertFrom-Json
    $passes = [ordered]@{ schemaVersion = 4; pid = $wire.pid; epoch = $wire.epoch; window = $wire.window }
    foreach ($metric in $FrameProfilePassMetricNames) {
        $d = $wire.$metric
        $passes[$metric] = @($d.mean, $d.p50, $d.p95, $d.p99, $d.max)
        $wire.PSObject.Properties.Remove($metric)
    }
    foreach ($metric in $FrameProfileFrameMetricNames) {
        $d = $wire.$metric
        $wire.$metric = @($d.mean, $d.p50, $d.p95, $d.p99, $d.max)
    }
    $line = '[FrameProfile] ' + ($wire | ConvertTo-Json -Depth 5 -Compress)
    if ($OmitPasses) { return $line }
    return $line + "`n" + '[FrameProfilePasses] ' + ([pscustomobject]$passes | ConvertTo-Json -Depth 5 -Compress)
}
function Assert-Rejected {
    param([scriptblock]$Body)
    $rejected = $false
    try { & $Body | Out-Null } catch { $rejected = $true }
    Assert-Profile $rejected
}

Test-Profile 'evidência incremental preserva amostras anteriores sem relatório final' {
    $path = [IO.Path]::GetTempFileName()
    try {
        Write-ProfileEvidence -Path $path -Value (New-TestWindow 1)
        Write-ProfileEvidence -Path $path -Value (New-TestWindow 2)
        $rows = @(Get-Content -LiteralPath $path | ForEach-Object { $_ | ConvertFrom-Json })
        Assert-Profile ($rows.Count -eq 2 -and $rows[0].window -eq 1 -and $rows[1].window -eq 2)
        Assert-Profile ($rows[0].frames -eq 600 -and $rows[1].pid -eq 7)
    } finally { Remove-Item -LiteralPath $path }
}

Test-Profile 'JSON nativo válido' {
    $windows = @(ConvertFrom-FrameProfileLog (Convert-TestWindow (New-TestWindow)) 7)
    Assert-Profile ($windows.Count -eq 1 -and $windows[0].present_fps -eq 60)
}
Test-Profile 'janela preserva ordinal e volume do hotspot' {
    $window = New-TestWindow
    $window | Add-Member -NotePropertyName route_frame -NotePropertyValue 599
    $window | Add-Member -NotePropertyName visible_draws -NotePropertyValue 48
    $window | Add-Member -NotePropertyName visible_triangles -NotePropertyValue 318340
    $decoded = @(ConvertFrom-FrameProfileLog (Convert-TestWindow $window) 7)
    Assert-Profile ($decoded.Count -eq 1 -and $decoded[0].route_frame -eq 599 -and
        $decoded[0].visible_draws -eq 48 -and $decoded[0].visible_triangles -eq 318340)
}
Test-Profile 'não mistura outro processo' {
    Assert-Profile (@(ConvertFrom-FrameProfileLog (Convert-TestWindow (New-TestWindow)) 8).Count -eq 0)
}
Test-Profile 'contexto nativo válido e idempotente' {
    $line = Convert-TestContext (New-TestContext)
    $contexts = @(ConvertFrom-FrameProfileContextLog "$line`n$line" 7)
    Assert-Profile ($contexts.Count -eq 1 -and $contexts[0].scene -eq 'poc-a-5000-textured-cubes')
}
Test-Profile 'contexto valida e preserva isolamento GPU' {
    $context = New-TestContext
    $context.gpu_isolation = 'no-ibl'
    $decoded = @(ConvertFrom-FrameProfileContextLog (Convert-TestContext $context) 7)
    Assert-Profile ($decoded.Count -eq 1 -and $decoded[0].gpu_isolation -eq 'no-ibl')
    $context.gpu_isolation = 'inventado'
    Assert-Rejected { ConvertFrom-FrameProfileContextLog (Convert-TestContext $context) 7 }
}
Test-Profile 'contexto schema v2 sem camera_mode infere locked/free' {
    $context = New-TestContext
    $context.schemaVersion = 2
    $context.camera_locked = $true
    $decoded = @(ConvertFrom-FrameProfileContextLog (Convert-TestContext $context) 7)
    Assert-Profile ($decoded.Count -eq 1 -and $decoded[0].camera_mode -eq 'locked' -and
        $decoded[0].camera_route_fingerprint -eq '0000000000000000')
}
Test-Profile 'contexto schema v3 aceita camera_mode route com fingerprint válido' {
    $context = New-TestContext
    $context.schemaVersion = 3
    $context | Add-Member -NotePropertyName camera_mode -NotePropertyValue 'route'
    $context | Add-Member -NotePropertyName camera_route_fingerprint -NotePropertyValue '1122334455667788'
    $context | Add-Member -NotePropertyName camera_route_frame_ordinal -NotePropertyValue 42
    $context | Add-Member -NotePropertyName camera_route_tick_count -NotePropertyValue 3600
    $decoded = @(ConvertFrom-FrameProfileContextLog (Convert-TestContext $context) 7)
    Assert-Profile ($decoded.Count -eq 1 -and $decoded[0].camera_mode -eq 'route' -and
        $decoded[0].camera_route_frame_ordinal -eq 42)
}
Test-Profile 'contexto route sem fingerprint válido é rejeitado' {
    $context = New-TestContext
    $context.schemaVersion = 3
    $context | Add-Member -NotePropertyName camera_mode -NotePropertyValue 'route'
    $context | Add-Member -NotePropertyName camera_route_fingerprint -NotePropertyValue '0000000000000000'
    $context | Add-Member -NotePropertyName camera_route_frame_ordinal -NotePropertyValue 0
    $context | Add-Member -NotePropertyName camera_route_tick_count -NotePropertyValue 0
    Assert-Rejected { ConvertFrom-FrameProfileContextLog (Convert-TestContext $context) 7 }
}
Test-Profile 'contexto sem contadores de HZB assume zero (desativado)' {
    $context = New-TestContext
    $decoded = @(ConvertFrom-FrameProfileContextLog (Convert-TestContext $context) 7)
    Assert-Profile ($decoded.Count -eq 1 -and $decoded[0].hzb_tested_draws -eq 0 -and
        $decoded[0].hzb_occluded_draws -eq 0 -and $decoded[0].hzb_revived_draws -eq 0)
}
Test-Profile 'contexto aceita contadores de HZB consistentes' {
    $context = New-TestContext
    $context | Add-Member -NotePropertyName hzb_tested_draws -NotePropertyValue 40
    $context | Add-Member -NotePropertyName hzb_occluded_draws -NotePropertyValue 12
    $context | Add-Member -NotePropertyName hzb_revived_draws -NotePropertyValue 3
    $context | Add-Member -NotePropertyName hzb_budget_skipped_draws -NotePropertyValue 0
    $decoded = @(ConvertFrom-FrameProfileContextLog (Convert-TestContext $context) 7)
    Assert-Profile ($decoded.Count -eq 1 -and $decoded[0].hzb_tested_draws -eq 40 -and
        $decoded[0].hzb_occluded_draws -eq 12)
}
Test-Profile 'contexto aceita skip de HZB por orçamento global' {
    $context = New-TestContext
    $context | Add-Member -NotePropertyName hzb_budget_skipped_draws -NotePropertyValue 54
    $decoded = @(ConvertFrom-FrameProfileContextLog (Convert-TestContext $context) 7)
    Assert-Profile ($decoded.Count -eq 1 -and $decoded[0].hzb_budget_skipped_draws -eq 54 -and
        $decoded[0].hzb_tested_draws -eq 0)
}
Test-Profile 'contexto com hzb_occluded_draws maior que testados é rejeitado' {
    $context = New-TestContext
    $context | Add-Member -NotePropertyName hzb_tested_draws -NotePropertyValue 5
    $context | Add-Member -NotePropertyName hzb_occluded_draws -NotePropertyValue 9
    $context | Add-Member -NotePropertyName hzb_revived_draws -NotePropertyValue 0
    Assert-Rejected { ConvertFrom-FrameProfileContextLog (Convert-TestContext $context) 7 }
}
Test-Profile 'contexto camera_mode inválido é rejeitado' {
    $context = New-TestContext
    $context.schemaVersion = 3
    $context | Add-Member -NotePropertyName camera_mode -NotePropertyValue 'inventado'
    Assert-Rejected { ConvertFrom-FrameProfileContextLog (Convert-TestContext $context) 7 }
}
Test-Profile 'contexto conflitante no mesmo epoch falha' {
    $a = New-TestContext
    $b = New-TestContext
    $b.content_fingerprint = 'fedcba9876543210'
    Assert-Rejected { ConvertFrom-FrameProfileContextLog "$(Convert-TestContext $a)`n$(Convert-TestContext $b)" 7 }
}
Test-Profile 'aceita contagem explícita da cena real' {
    $window = New-TestWindow
    $window.instances = 27
    $windows = @(ConvertFrom-FrameProfileLog (Convert-TestWindow $window) 7 27)
    $capture = Get-TestCapture $windows 10 'dirt-road'
    Assert-Profile ($capture.scene -eq 'dirt-road' -and $capture.windows[0].instances -eq 27)
}
Test-Profile 'duplicatas não aumentam duração' {
    $line = Convert-TestWindow (New-TestWindow)
    Assert-Rejected { ConvertFrom-FrameProfileLog "$line`n$line" 7 }
}
Test-Profile 'JSON truncado falha' {
    Assert-Rejected { ConvertFrom-FrameProfileLog '[FrameProfile] {"pid":7' 7 }
}
Test-Profile 'FPS inconsistente falha' {
    $window = New-TestWindow
    $window.present_fps = 120
    Assert-Rejected { ConvertFrom-FrameProfileLog (Convert-TestWindow $window) 7 }
}
Test-Profile 'percentis inconsistentes falham' {
    $window = New-TestWindow
    $window.process_cpu_ms.p95 = 10
    Assert-Rejected { ConvertFrom-FrameProfileLog (Convert-TestWindow $window) 7 }
}
Test-Profile 'métrica nula não vira zero' {
    $window = New-TestWindow
    $window.process_cpu_ms.mean = $null
    Assert-Rejected { ConvertFrom-FrameProfileLog (Convert-TestWindow $window) 7 }
}
Test-Profile 'captura exige duração e continuidade' {
    Assert-Profile ($null -eq (Get-TestCapture @((New-TestWindow)) 20))
    Assert-Rejected { Get-TestCapture @((New-TestWindow 1), (New-TestWindow 3)) 20 }
}
Test-Profile 'reset de lifecycle não soma epochs' {
    Assert-Profile ($null -eq (Get-TestCapture @((New-TestWindow 1 1), (New-TestWindow 2 2)) 20))
}
Test-Profile 'médias agregadas não fingem percentil global' {
    $a = New-TestWindow 1
    $b = New-TestWindow 2
    $b.process_cpu_ms = [pscustomobject]@{ mean = 2.0; p50 = 2.0; p95 = 3.0; p99 = 4.0; max = 5.0 }
    $capture = Get-TestCapture @($a, $b) 20
    Assert-Profile ($capture.metrics.process_cpu_ms.mean -eq 1.5)
    Assert-Profile ($capture.metrics.process_cpu_ms.worstWindowP95 -eq 3)
    Assert-Profile (-not $capture.pocA.cpuBudgetPassed -and -not $capture.pocA.accepted)
}
Test-Profile 'limite CPU é estritamente menor que 3 ms' {
    $window = New-TestWindow
    $window.process_cpu_ms.max = 3
    Assert-Profile (-not (Get-TestCapture @($window) 10).pocA.cpuBudgetPassed)
}
Test-Profile 'CPU verde não comprova FPS exibido' {
    $capture = Get-TestCapture @((New-TestWindow)) 10
    Assert-Profile ($capture.pocA.cpuBudgetPassed -and -not $capture.pocA.displayedFpsMeasured -and -not $capture.pocA.accepted)
}
Test-Profile 'mudança de configuração no primeiro intervalo falha' {
    $first = New-TestWindow 1
    $first.width = 1280
    Assert-Rejected { Get-TestCapture @($first, (New-TestWindow 2)) 20 }
}
Test-Profile 'captura rejeita rótulo de cena divergente do runtime' {
    $window = New-TestWindow
    $context = New-TestContext -Scene 'dirt-road'
    Assert-Rejected { Get-FrameProfileCapture @($window) 10 'poc-a-5000-textured-cubes' @($context) }
}
Test-Profile 'captura exige contexto nativo correspondente' {
    Assert-Rejected { Get-FrameProfileCapture @((New-TestWindow)) 10 'poc-a-5000-textured-cubes' @() }
}
Test-Profile 'FrameTracker usa segunda coluna e ignora fences pendentes' {
    $text = "16666666`n10 100 20`n11 0 22`n12 9223372036854775807 23`n13 200 24"
    $times = @(ConvertFrom-SurfaceFrameLatency $text)
    Assert-Profile ($times.Count -eq 2 -and $times[0] -eq 100 -and $times[1] -eq 200)
}
Test-Profile 'snapshots SurfaceFlinger sobrepostos preservam continuidade' {
    $state = New-SurfaceFrameCaptureState
    $first = @(10000000L, 20000000L, 30000000L)
    $second = @(20000000L, 30000000L, 40000000L)
    Assert-Profile (@(Merge-SurfaceFrameSnapshot $state $first).Count -eq 3)
    Assert-Profile (@(Merge-SurfaceFrameSnapshot $state $second).Count -eq 1)
    Assert-Profile ($state.continuous -and $state.timestamps.Count -eq 4 -and $state.polls -eq 2)
}
Test-Profile 'fronteira adjacente é aceita mas perda do buffer é rejeitada' {
    $adjacent = New-SurfaceFrameCaptureState
    Merge-SurfaceFrameSnapshot $adjacent @(10000000L, 20000000L, 30000000L) | Out-Null
    Merge-SurfaceFrameSnapshot $adjacent @(40000000L, 50000000L, 60000000L) | Out-Null
    Assert-Profile $adjacent.continuous

    $lost = New-SurfaceFrameCaptureState
    Merge-SurfaceFrameSnapshot $lost @(10000000L, 20000000L, 30000000L) | Out-Null
    Merge-SurfaceFrameSnapshot $lost @(130000000L, 140000000L, 150000000L) | Out-Null
    Assert-Profile (-not $lost.continuous)
}
Test-Profile 'FPS exibido usa intervalos únicos ordenados' {
    $summary = Get-SurfaceFrameSummary @(30000000, 10000000, 20000000, 20000000)
    Assert-Profile ($summary.frames -eq 3 -and $summary.displayedFps -eq 100)
    Assert-Profile ($summary.intervalMs.p95 -eq 10)
}
Test-Profile 'localiza somente a layer de buffer da Activity' {
    $text = "RequestedLayerState{abc dev.aether.editor/android.app.NativeActivity#10 parentId=1}`nRequestedLayerState{dev.aether.editor/android.app.NativeActivity#11 parentId=10}"
    Assert-Profile ((Find-FrameProfileSurfaceLayer $text 'dev.aether.editor/android.app.NativeActivity') -eq 'dev.aether.editor/android.app.NativeActivity#11')
}
Test-Profile 'surface ambígua não produz medição inventada' {
    $text = "dev.aether.editor/android.app.NativeActivity#10`ndev.aether.editor/android.app.NativeActivity#11"
    Assert-Profile ($null -eq (Find-FrameProfileSurfaceLayer $text 'dev.aether.editor/android.app.NativeActivity'))
}
Test-Profile 'regiões de GPU chegam pareadas à janela e preservam a partição do frame' {
    $window = New-TestWindow
    $parsed = @(ConvertFrom-FrameProfileLog (Convert-TestWindow $window) 7)
    Assert-Profile ($parsed.Count -eq 1)
    foreach ($metric in $FrameProfilePassMetricNames) {
        Assert-Profile ($parsed[0].$metric.mean -eq 0.1)
    }
    $regionSum = 0.0
    foreach ($metric in $FrameProfilePassMetricNames) { $regionSum += $parsed[0].$metric.mean }
    Assert-Profile ($regionSum -le $parsed[0].gpu_frame_ms.mean)
}
Test-Profile 'janela sem registro de regiões é recusada em vez de virar zero' {
    # O modo de falha que este teste tranca: aceitar a janela órfã produziria
    # 0 ms para opaco/folhagem/céu e um relatório aparentemente válido.
    Assert-Rejected { ConvertFrom-FrameProfileLog (Convert-TestWindow (New-TestWindow) -OmitPasses) 7 }
}
Test-Profile 'regiões que somam mais que o frame são recusadas' {
    $window = New-TestWindow
    $window.gpu_opaque_ms = [pscustomobject]@{ mean = 9.0; p50 = 9.0; p95 = 9.0; p99 = 9.0; max = 9.0 }
    Assert-Rejected { ConvertFrom-FrameProfileLog (Convert-TestWindow $window) 7 }
}
Test-Profile 'nomes das regiões acompanham ae::GpuPassClass do nativo' {
    $header = Get-Content (Join-Path $PSScriptRoot '../../native/core/gpu_pass_class.h') -Raw
    if ($header -notmatch 'GpuPassClassMetricNames\[\] = \{([^}]*)\}') {
        throw 'Tabela canônica de nomes não encontrada em gpu_pass_class.h.'
    }
    $native = @([regex]::Matches($Matches[1], '"([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
    # Compare-Object devolve $null quando não há diferença; @() normaliza para
    # que StrictMode não confunda "iguais" com erro de propriedade ausente.
    $differences = @(Compare-Object $native $FrameProfilePassMetricNames -SyncWindow 0)
    Assert-Profile ($differences.Count -eq 0)
}
Write-Host "$count testes de FrameProfile passaram."
