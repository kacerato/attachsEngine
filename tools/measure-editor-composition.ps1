param(
    [string]$Adb = 'C:/Users/donod/AppData/Local/Android/Sdk/platform-tools/adb.exe',
    [string]$Project = 'Sponza',
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [ValidateSet('A','B')][string[]]$Order = @('A','B','B','A'),
    [int]$Seconds = 75,
    [float]$TargetFps = 120,
    [float]$RenderScale = .75,
    [switch]$HzbComparison,
    [switch]$SpatialComparison,
    [switch]$PowerTrace,
    [switch]$SceneReuseComparison,
    [switch]$LockRenderingQuality,
    [float[]]$Pose = @(-6.101169,5.219586,-8.918053,.6,.45)
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'android-frame-profile.ps1')
function Merge-CompositionSurfaceBatches {
    param($Lines,$State,$EvidencePath,$Layer)
    foreach($item in $Lines) {
        $line=$item.ToString()
        if($line -match '^__AETHER_SURFACE_BATCH__:(\d+(?:\.\d+)?)$') {
            $elapsed=[double]::Parse($Matches[1],[Globalization.CultureInfo]::InvariantCulture)
            $times=@(ConvertFrom-SurfaceFrameLatency ($State.pendingLines -join "`n"))
            $State.pendingLines.Clear()
            if(!$times.Count) {continue}
            $added=@(Merge-SurfaceFrameSnapshot -State $State -Timestamps $times)
            Write-ProfileEvidence $EvidencePath ([ordered]@{elapsedSeconds=$elapsed;layer=$Layer;newPresentNs=$added;continuous=$State.continuous})
        } else { $State.pendingLines.Add($line) }
    }
}
if($Pose.Count -ne 5 -or $Seconds -lt 30) { throw 'Pose exige cinco valores; coleta exige ao menos 30 s.' }
$taskOutput = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$culture = [Globalization.CultureInfo]::InvariantCulture
$runs = @()
$surfaceJob = $null
$powerJob = $null
$logJob = $null
try {
    for($index=0;$index -lt $Order.Count;++$index) {
        $variant = $Order[$index]
        $run = Join-Path $taskOutput ('{0:00}-{1}' -f ($index+1),$variant)
        New-Item -ItemType Directory -Path $run -Force | Out-Null
        & $Adb shell dumpsys battery | Set-Content (Join-Path $run 'battery-before.txt')
        & $Adb shell dumpsys display | Set-Content (Join-Path $run 'display-before.txt')
        $launch = @('shell','am','start','-S','-n','dev.aether.editor/.shell.AstraShellActivity',
            '--es','astra.open_project',$Project,'--ez','aether.profile_frames','true',
            '--ez','aether.lock_camera','true','--ez','aether.disable_dynamic_resolution','true',
            '--ez','aether.lock_rendering_quality',$(if($LockRenderingQuality){'true'}else{'false'}),
            '--ez','aether.disable_post_ui_fusion',$(if(!$HzbComparison -and !$SpatialComparison -and !$SceneReuseComparison -and $variant -eq 'A'){'true'}else{'false'}),
            '--ez','aether.hzb_occlusion',$(if($HzbComparison -and $variant -eq 'B'){'true'}else{'false'}),
            '--ez','aether.disable_spatial_geometry',$(if($SpatialComparison -and $variant -eq 'A'){'true'}else{'false'}),
            '--ez','aether.spatial_geometry',$(if($SpatialComparison -and $variant -eq 'B'){'true'}else{'false'}),
            '--ez','aether.editor_scene_reuse',$(if($SceneReuseComparison -and $variant -eq 'B'){'true'}else{'false'}),
            '--ez','aether.disable_editor_scene_reuse',$(if($SceneReuseComparison -and $variant -eq 'A'){'true'}else{'false'}),
            '--ef','aether.target_fps',$TargetFps.ToString($culture),
            '--ef','aether.resolution_scale',$RenderScale.ToString($culture))
        $names = @('x','y','z','yaw','pitch')
        for($part=0;$part -lt 5;++$part) { $launch += @('--ef',('aether.camera_'+$names[$part]),$Pose[$part].ToString($culture)) }
        $started = (& $Adb @launch 2>&1 | Out-String)
        $started | Set-Content (Join-Path $run 'launch.txt')
        if($LASTEXITCODE -ne 0 -or $started -match 'Error|Exception') { throw 'Launch rejeitado.' }
        Start-Sleep -Seconds 15
        $processId = (& $Adb shell pidof dev.aether.editor | Out-String).Trim()
        if($processId -notmatch '^\d+$') { throw 'Editor sem processo único.' }
        $continuousLog=Join-Path $run 'logcat-continuous.txt'
        $logJob=Start-Job -ScriptBlock {
            param($adbPath,$pidValue)
            & $adbPath logcat -v threadtime --pid=$pidValue 2>&1
        } -ArgumentList $Adb,$processId
        $surfaceText = (& $Adb shell dumpsys SurfaceFlinger --list | Out-String)
        $layer = Find-FrameProfileSurfaceLayer -Text $surfaceText -Component 'dev.aether.editor/dev.aether.editor.AetherActivity'
        if(!$layer) { throw 'Surface do editor não encontrada.' }
        & $Adb shell dumpsys SurfaceFlinger --latency-clear $layer | Out-Null
        $state = New-SurfaceFrameCaptureState
        $surfaceJob = Start-Job -ScriptBlock {
            param($adbPath,$surfaceLayer)
            $timer=[Diagnostics.Stopwatch]::StartNew()
            while($true) {
                & $adbPath shell dumpsys SurfaceFlinger --latency $surfaceLayer 2>$null
                '__AETHER_SURFACE_BATCH__:'+$timer.Elapsed.TotalSeconds.ToString('F6',[Globalization.CultureInfo]::InvariantCulture)
                Start-Sleep -Milliseconds 100
            }
        } -ArgumentList $Adb,$layer
        if($PowerTrace) {
            $powerConfig=Join-Path $run 'power.cfg'
            @"
buffers { size_kb: 2048 fill_policy: RING_BUFFER }
duration_ms: $($Seconds*1000)
data_sources { config { name: "android.power" android_power_config {
  battery_poll_ms: 1000
  battery_counters: BATTERY_COUNTER_CAPACITY_PERCENT
  battery_counters: BATTERY_COUNTER_CHARGE
  battery_counters: BATTERY_COUNTER_CURRENT
  battery_counters: BATTERY_COUNTER_CURRENT_AVG
  battery_counters: BATTERY_COUNTER_VOLTAGE
  collect_power_rails: true
} } }
"@ | Set-Content $powerConfig
            $remotePower='/data/misc/perfetto-traces/astra-editor-power-'+[Guid]::NewGuid().ToString('N')+'.pftrace'
            & $Adb push $powerConfig /data/misc/perfetto-configs/astra-editor-power.cfg | Out-Null
            if($LASTEXITCODE -ne 0) { throw 'Configuração Perfetto não enviada.' }
            $powerJob=Start-Job -ScriptBlock {
                param($adbPath,$tracePath)
                & $adbPath shell perfetto --txt -c /data/misc/perfetto-configs/astra-editor-power.cfg -o $tracePath 2>&1
                if($LASTEXITCODE -ne 0) { throw 'Coleta Perfetto falhou.' }
            } -ArgumentList $Adb,$remotePower
        }
        $watch = [Diagnostics.Stopwatch]::StartNew()
        $sample=0
        while($watch.Elapsed.TotalSeconds -lt $Seconds) {
            Receive-Job $logJob | Add-Content $continuousLog
            if($logJob.State -eq 'Failed') { throw 'Coletor contínuo de log falhou.' }
            $batches=@(Receive-Job $surfaceJob)
            if($batches.Count) {
                Merge-CompositionSurfaceBatches -Lines $batches -State $state -EvidencePath (Join-Path $run 'surface.jsonl') -Layer $layer
            }
            if($surfaceJob.State -eq 'Failed') { throw 'Coletor de apresentação falhou.' }
            if(($sample++ % 10) -eq 0) {
                Write-ProfileEvidence (Join-Path $run 'thermal.jsonl') ([ordered]@{
                    utc=[DateTime]::UtcNow.ToString('o');elapsedSeconds=$watch.Elapsed.TotalSeconds;
                    battery=(& $Adb shell dumpsys battery | Out-String);
                    thermal=(& $Adb shell dumpsys thermalservice | Out-String)
                })
                Write-Host "$variant $([int]$watch.Elapsed.TotalSeconds)/$Seconds s"
            }
            Start-Sleep -Milliseconds 550
        }
        Stop-Job $surfaceJob
        $batches=@(Receive-Job $surfaceJob)
        if($batches.Count) { Merge-CompositionSurfaceBatches -Lines $batches -State $state -EvidencePath (Join-Path $run 'surface.jsonl') -Layer $layer }
        Remove-Job $surfaceJob -Force
        $surfaceJob=$null
        Stop-Job $logJob
        Receive-Job $logJob | Add-Content $continuousLog
        Remove-Job $logJob -Force
        $logJob=$null
        if($powerJob) {
            Wait-Job $powerJob -Timeout 10 | Out-Null
            Receive-Job $powerJob | Set-Content (Join-Path $run 'power-collector.txt')
            if($powerJob.State -ne 'Completed') { throw 'Coleta Perfetto não concluiu.' }
            Remove-Job $powerJob -Force
            $powerJob=$null
            & $Adb pull $remotePower (Join-Path $run 'power.pftrace') | Out-Null
            if($LASTEXITCODE -ne 0) { throw 'Trace Perfetto não recebida.' }
        }
        & $Adb shell dumpsys battery | Set-Content (Join-Path $run 'battery-after.txt')
        & $Adb shell dumpsys display | Set-Content (Join-Path $run 'display-after.txt')
        & $Adb shell screencap -p /sdcard/astra-composition-capture.png
        & $Adb pull /sdcard/astra-composition-capture.png (Join-Path $run 'screen.png') | Out-Null
        $log = Get-Content $continuousLog -Raw
        $log | Set-Content (Join-Path $run 'logcat.txt')
        $contexts = @(foreach($line in ($log -split "`n")) {
            if($line -match '\[FrameProfileContext\] (\{.*)$') { $Matches[1] | ConvertFrom-Json }
        })
        $context = $contexts | Where-Object { $_.scene -eq 'authored-project' -and $_.instances -gt 6 } | Select-Object -Last 1
        $expectedFusion=$HzbComparison -or $SpatialComparison -or $SceneReuseComparison -or $variant -eq 'B'
        $expectedHzb=$HzbComparison -and $variant -eq 'B'
        if(!$context -or $context.post_ui_fused -ne $expectedFusion -or
           $context.hzb_enabled -ne $expectedHzb -or !$context.camera_locked) {
            throw 'Contexto não prova cena/pose/caminho solicitado.'
        }
        if($SpatialComparison -and (($context.spatial_chunks -gt 0) -ne ($variant -eq 'B'))) { throw 'Contexto não prova particionamento solicitado.' }
        if($SceneReuseComparison -and ($context.scene_reuse_enabled -ne ($variant -eq 'B'))) { throw 'Contexto não prova reuso solicitado.' }
        if($LockRenderingQuality -and !$context.rendering_quality_locked) {throw 'Contexto não prova qualidade fixa.'}
        for($part=0;$part -lt 5;++$part) {
            if([Math]::Abs($context.camera_pose[$part]-$Pose[$part]) -gt .00001) { throw 'Pose executada difere da solicitada.' }
        }
        # Window ordinals are process-wide. Discard the first complete window
        # of the loaded epoch even when a surface recreation advanced its id.
        $windows = @(ConvertFrom-FrameProfileLog -Text $log -ExpectedPid $processId -ExpectedInstances $null |
            Where-Object { $_.epoch -eq $context.epoch } | Sort-Object window | Select-Object -Skip 1)
        if($windows.Count -lt 2) { throw 'Janelas estáveis insuficientes.' }
        $windows | ConvertTo-Json -Depth 12 | Set-Content (Join-Path $run 'windows.json')
        $context | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $run 'context.json')
        $summary = Get-SurfaceFrameSummary -Timestamps @($state.timestamps)
        $surface = [ordered]@{continuous=$state.continuous;polls=$state.polls;summary=$summary}
        $surface | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $run 'surface.json')
        $runs += [ordered]@{variant=$variant;pid=$processId;context=$context;windows=$windows;surface=$surface}
        $runs | ConvertTo-Json -Depth 14 | Set-Content (Join-Path $taskOutput 'runs.json')
        & $Adb shell am force-stop dev.aether.editor
        Start-Sleep -Seconds 10
    }
} finally {
    if($surfaceJob) { Stop-Job $surfaceJob; Remove-Job $surfaceJob -Force }
    if($powerJob) { Stop-Job $powerJob; Remove-Job $powerJob -Force }
    if($logJob) { Stop-Job $logJob; Remove-Job $logJob -Force }
    & $Adb shell input keyevent KEYCODE_HOME
    & $Adb shell am force-stop dev.aether.editor
}
