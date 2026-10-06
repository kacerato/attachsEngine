param(
    [string]$Adb='C:/Users/donod/AppData/Local/Android/Sdk/platform-tools/adb.exe',
    [string]$Project='Sponza',
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [int]$Seconds=40,
    [ValidateSet('A','B')][string[]]$Order=@('A','B','B','A'),
    [float]$RenderScale=.75,
    [float]$TargetFps=120,
    [ValidateRange(0,5)][int]$GpuIsolation=0,
    [ValidateSet('Composition','OpaqueNoClip','FullDetailSampling','Combined','Lighting','PointLighting')][string]$Experiment='Composition'
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'android-frame-profile.ps1')
if($Seconds -lt 30) { throw 'A coleta requer ao menos 30 segundos.' }
$taskOutput=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$culture=[Globalization.CultureInfo]::InvariantCulture
try {
    for($index=0;$index -lt $Order.Count;++$index) {
        $variant=$Order[$index]
        $run=Join-Path $taskOutput ('{0:00}-{1}' -f ($index+1),$variant)
        New-Item -ItemType Directory -Path $run -Force | Out-Null
        & $Adb shell dumpsys battery | Set-Content (Join-Path $run 'battery-before.txt')
        & $Adb shell dumpsys thermalservice | Set-Content (Join-Path $run 'thermal-before.txt')
        & $Adb logcat -c
        $launch=@('shell','am','start','-S','-n','dev.aether.editor/.shell.AstraShellActivity',
            '--es','astra.open_project',$Project,'--ez','aether.profile_frames','true',
            '--ez','aether.start_play','true',
            '--ez','aether.disable_dynamic_resolution','true',
            '--ez','aether.lock_rendering_quality','true',
            '--ei','aether.gpu_isolation',[string]$GpuIsolation,
            '--ez','aether.play_post_ui_fusion',$(if($Experiment -eq 'Composition' -and $variant -eq 'B'){'true'}else{'false'}),
            '--ez','aether.opaque_no_clip',$(if($Experiment -in @('FullDetailSampling','Lighting','PointLighting') -or ($Experiment -in @('OpaqueNoClip','Combined') -and $variant -eq 'B')){'true'}else{'false'}),
            '--ez','aether.point_lighting_specialization',$(if($Experiment -eq 'PointLighting' -and $variant -eq 'B'){'true'}else{'false'}),
            '--ez','aether.full_detail_sampling',$(if($Experiment -in @('FullDetailSampling','Combined') -and $variant -eq 'B'){'true'}else{'false'}),
            '--ef','aether.target_fps',$TargetFps.ToString($culture),
            '--ef','aether.resolution_scale',$RenderScale.ToString($culture))
        $started=(& $Adb @launch 2>&1 | Out-String)
        $started | Set-Content (Join-Path $run 'launch.txt')
        if($LASTEXITCODE -ne 0 -or $started -match 'Error|Exception') { throw 'Launch rejeitado.' }
        # The engine starts Play after the actual project publication, avoiding
        # coordinate taps during loading and mixed editor/gameplay windows.
        Start-Sleep -Seconds 20
        $layer=Find-FrameProfileSurfaceLayer -Text (& $Adb shell dumpsys SurfaceFlinger --list | Out-String) -Component 'dev.aether.editor/dev.aether.editor.AetherActivity'
        if(!$layer) { throw 'Surface não encontrada.' }
        & $Adb shell dumpsys SurfaceFlinger --latency-clear $layer | Out-Null
        $state=New-SurfaceFrameCaptureState
        $timer=[Diagnostics.Stopwatch]::StartNew()
        while($timer.Elapsed.TotalSeconds -lt $Seconds) {
            $times=@(ConvertFrom-SurfaceFrameLatency (& $Adb shell dumpsys SurfaceFlinger --latency $layer | Out-String))
            if($times.Count) {
                $added=@(Merge-SurfaceFrameSnapshot -State $state -Timestamps $times)
                Write-ProfileEvidence (Join-Path $run 'surface.jsonl') ([ordered]@{elapsedSeconds=$timer.Elapsed.TotalSeconds;newPresentNs=$added;continuous=$state.continuous})
            }
            Start-Sleep -Milliseconds 100
        }
        $summary=Get-SurfaceFrameSummary -Timestamps @($state.timestamps)
        if($summary) {
            # Full timestamps already live in the incremental JSONL evidence.
            $summary.Remove('actualPresentTimestampsNs')
            $summary.continuous=$state.continuous;$summary.polls=$state.polls
            $summary | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $run 'surface-summary.json')
        }
        & $Adb logcat -d -s Aether.Android:I | Set-Content (Join-Path $run 'profile.log')
        $logText=Get-Content (Join-Path $run 'profile.log') -Raw
        $sample=[regex]::Match($logText,'\[FrameProfile\] (\{[^\r\n]*\})')
        if(!$sample.Success) { throw 'Nenhuma janela FrameProfile.' }
        $processId=($sample.Groups[1].Value | ConvertFrom-Json).pid
        $windows=@(ConvertFrom-FrameProfileLog -Text $logText -ExpectedPid $processId -ExpectedInstances $null)
        $contexts=@(ConvertFrom-FrameProfileContextLog -Text $logText -ExpectedPid $processId)
        $capture=Get-FrameProfileCapture -Windows $windows -Contexts $contexts -MinimumSeconds 15 -Scene authored-project-play
        if(!$capture -or $capture.context.post_ui_fused -ne ($Experiment -eq 'Composition' -and $variant -eq 'B')) {
            throw 'O caminho executado não corresponde à variante pedida.'
        }
        if($Experiment -in @('OpaqueNoClip','Combined') -and $capture.context.opaque_no_clip -ne ($variant -eq 'B')) {
            throw 'A família opaca executada não corresponde à variante pedida.'
        }
        if($Experiment -in @('FullDetailSampling','Combined') -and $capture.context.full_detail_sampling -ne ($variant -eq 'B')) {
            throw 'A variante de amostragem de material não foi aplicada.'
        }
        if($Experiment -in @('FullDetailSampling','Lighting','PointLighting') -and !$capture.context.opaque_no_clip) {
            throw 'A bancada de material requer a família opaca ativa nas duas variantes.'
        }
        $expectedIsolation=@('full','no-normal','no-ibl','base-color','no-punctual','no-directional-shadow')[$GpuIsolation]
        if($capture.context.gpu_isolation -ne $expectedIsolation) {
            throw 'A variante de atribuição GPU não foi aplicada.'
        }
        $selected=@($windows | Where-Object epoch -eq $capture.epoch)
        if($Experiment -eq 'PointLighting') {
            if($capture.context.point_lighting_specialization -ne ($variant -eq 'B')) {
                throw 'A especialização de luzes não corresponde à variante pedida.'
            }
            $expectedPointFrames=if($variant -eq 'B'){600}else{0}
            if(@($selected | Where-Object point_lighting_frames -ne $expectedPointFrames).Count) {
                throw 'A janela mistura pipelines de iluminação.'
            }
        }
        if(@($selected | Where-Object scene_reused_frames -ne 0).Count) { throw 'Play está reutilizando frames do editor.' }
        if($Experiment -in @('OpaqueNoClip','Combined') -and
           $selected[0].PSObject.Properties.Name -contains 'opaque_no_clip_frames') {
            $expectedFrames=if($variant -eq 'B'){600}else{0}
            if(@($selected | Where-Object opaque_no_clip_frames -ne $expectedFrames).Count) {
                throw 'A janela mistura famílias opacas; não atribuir o ganho à variante.'
            }
        }
        $selected | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $run 'windows.json')
        $capture.context | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $run 'context.json')
        & $Adb shell screencap -p /sdcard/astra-play-composition.png
        & $Adb pull /sdcard/astra-play-composition.png (Join-Path $run 'screen.png') | Out-Null
        & $Adb shell dumpsys battery | Set-Content (Join-Path $run 'battery-after.txt')
        & $Adb shell dumpsys thermalservice | Set-Content (Join-Path $run 'thermal-after.txt')
        Write-Output ('rodada {0} {1}: {2:F2} FPS apresentados, contínua={3}' -f ($index+1),$variant,$summary.displayedFps,$state.continuous)
    }
} finally {
    # No diagnostic quality lock remains in the user's normal session.
    & $Adb shell am start -S -n dev.aether.editor/.shell.AstraShellActivity --es astra.open_project $Project | Out-Null
}
