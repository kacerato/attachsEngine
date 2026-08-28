Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../../tools/android-frame-profile.ps1')
$count = 0
function Assert-Profile { param([bool]$Value); if (-not $Value) { throw 'Asserção de FrameProfile falhou.' } }
function Test-Profile { param([string]$Name, [scriptblock]$Body); & $Body; ++$script:count; Write-Host "PASS: $Name" }
function New-TestWindow {
    param([int]$Index = 1, [int]$Epoch = 1)
    $window = [ordered]@{ schemaVersion = 1; pid = 7; epoch = $Epoch; window = $Index; build = 'debug';
        instances = 5000; width = 2772; height = 1280; frames = 600; elapsed_ms = 10000.0;
        present_fps = 60.0; warmup_samples = 300; warmup_process_cpu_max_ms = 4.0 }
    foreach ($metric in $FrameProfileMetricNames) {
        $value = if ($metric -eq 'interval_ms') { 10000.0 / 600 } else { 1.0 }
        $window[$metric] = [pscustomobject]@{ mean = $value; p50 = $value; p95 = $value; p99 = $value; max = $value }
    }
    return [pscustomobject]$window
}
function Convert-TestWindow { param($Window); return '[FrameProfile] ' + ($Window | ConvertTo-Json -Depth 5 -Compress) }
function Assert-Rejected {
    param([scriptblock]$Body)
    $rejected = $false
    try { & $Body | Out-Null } catch { $rejected = $true }
    Assert-Profile $rejected
}

Test-Profile 'JSON nativo válido' {
    $windows = @(ConvertFrom-FrameProfileLog (Convert-TestWindow (New-TestWindow)) 7)
    Assert-Profile ($windows.Count -eq 1 -and $windows[0].present_fps -eq 60)
}
Test-Profile 'não mistura outro processo' {
    Assert-Profile (@(ConvertFrom-FrameProfileLog (Convert-TestWindow (New-TestWindow)) 8).Count -eq 0)
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
    Assert-Profile ($null -eq (Get-FrameProfileCapture @((New-TestWindow)) 20))
    Assert-Rejected { Get-FrameProfileCapture @((New-TestWindow 1), (New-TestWindow 3)) 20 }
}
Test-Profile 'reset de lifecycle não soma epochs' {
    Assert-Profile ($null -eq (Get-FrameProfileCapture @((New-TestWindow 1 1), (New-TestWindow 2 2)) 20))
}
Test-Profile 'médias agregadas não fingem percentil global' {
    $a = New-TestWindow 1
    $b = New-TestWindow 2
    $b.process_cpu_ms = [pscustomobject]@{ mean = 2.0; p50 = 2.0; p95 = 3.0; p99 = 4.0; max = 5.0 }
    $capture = Get-FrameProfileCapture @($a, $b) 20
    Assert-Profile ($capture.metrics.process_cpu_ms.mean -eq 1.5)
    Assert-Profile ($capture.metrics.process_cpu_ms.worstWindowP95 -eq 3)
    Assert-Profile (-not $capture.pocA.cpuBudgetPassed -and -not $capture.pocA.accepted)
}
Test-Profile 'limite CPU é estritamente menor que 3 ms' {
    $window = New-TestWindow
    $window.process_cpu_ms.max = 3
    Assert-Profile (-not (Get-FrameProfileCapture @($window) 10).pocA.cpuBudgetPassed)
}
Test-Profile 'CPU verde não comprova FPS exibido' {
    $capture = Get-FrameProfileCapture @((New-TestWindow)) 10
    Assert-Profile ($capture.pocA.cpuBudgetPassed -and -not $capture.pocA.displayedFpsMeasured -and -not $capture.pocA.accepted)
}
Test-Profile 'mudança de configuração no primeiro intervalo falha' {
    $first = New-TestWindow 1
    $first.width = 1280
    Assert-Rejected { Get-FrameProfileCapture @($first, (New-TestWindow 2)) 20 }
}
Test-Profile 'FrameTracker usa segunda coluna e ignora fences pendentes' {
    $text = "16666666`n10 100 20`n11 0 22`n12 9223372036854775807 23`n13 200 24"
    $times = @(ConvertFrom-SurfaceFrameLatency $text)
    Assert-Profile ($times.Count -eq 2 -and $times[0] -eq 100 -and $times[1] -eq 200)
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
Write-Host "$count testes de FrameProfile passaram."
