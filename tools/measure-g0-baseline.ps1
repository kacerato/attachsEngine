# G0 (DESEMPENHO-GRAFICOS-VIVOS-2026-10-17): linha de base por passe no POCO F7
# em nível S nativo e em nível C forçado na área de um celular 720p.
#
# O nível C forçado mede o custo RELATIVO das variantes do nível C; não prova o
# FPS de um aparelho C (banda e ALU são do POCO). O FPS real do nível C vem da
# telemetria dos testadores (bloco GT).
param(
    [string]$Adb='C:/Users/donod/AppData/Local/Android/Sdk/platform-tools/adb.exe',
    [string]$Package='dev.aether.editor.u07',
    [string]$SerialPrefix='adb-53e1eb7',
    [string[]]$Projects=@('Vila Astra','Sponza'),
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [int]$Seconds=40,
    [ValidateSet('S','C')][string[]]$Order=@('S','C','C','S'),
    [string]$ReferenceResolution='1600x720',
    [float]$TargetFps=60,
    [switch]$Edit,
    # Pose fixa x,y,z,yaw,pitch (modo Edit). Sponza de 05/10: -6.101169,5.219586,-8.918053,.6,.45
    [float[]]$Pose=@(),
    # Diagnóstico do G1: bits do passe de pós desligados (1 neblina, 2 AO, 4 bloom,
    # 8 FXAA, 16 nitidez). Aplica-se a todas as rodadas desta chamada.
    [ValidateRange(0,31)][int]$PostIsolation=0,
    # Candidata do G1: névoa resolvida por texel antes do pós (aether.post_fog_resolve).
    [switch]$FogResolve,
    # Bateria acima disso antes de uma rodada: esperar esfriar (décimos de °C não;
    # graus). Clocks caem sem a API térmica acusar; ver baseline de 17/10/2026.
    [double]$MaximumStartBatteryCelsius=38.0,
    [int]$MaximumCoolingMinutes=20
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'android-frame-profile.ps1')
if($Seconds -lt 30) { throw 'A coleta requer ao menos 30 segundos.' }
if($Pose.Count -notin @(0,5)) { throw 'Pose exige cinco valores: x,y,z,yaw,pitch.' }
if($ReferenceResolution -notmatch '^\d+[xX]\d+$') { throw 'Use LARGURAxALTURA, por exemplo 1600x720.' }
# O serial do ADB sem fio muda entre reconexões e pode aparecer duplicado
# ("... (2)._adb-tls-connect._tcp"): resolver por prefixo e fixar via ANDROID_SERIAL.
$serial=@(& $Adb devices | ForEach-Object { if($_ -match "^($([regex]::Escape($SerialPrefix))[^\t]*)\tdevice$") { $Matches[1] } })[0]
if(!$serial) { throw 'Nenhum aparelho no ADB. Conecte o POCO por USB ou depuração sem fio.' }
$env:ANDROID_SERIAL=$serial
$shellActivity="$Package/dev.aether.editor.shell.AstraShellActivity"
$renderActivity="$Package/dev.aether.editor.AetherActivity"
$taskOutput=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$culture=[Globalization.CultureInfo]::InvariantCulture
& $Adb shell getprop ro.product.model | Set-Content (Join-Path $taskOutput 'device-model.txt')
& $Adb shell dumpsys package $Package | Select-String 'versionCode|versionName' |
    Set-Content (Join-Path $taskOutput 'package-version.txt')
$rows=New-Object System.Collections.Generic.List[object]
try {
    foreach($project in $Projects) {
        # adb shell junta os argumentos numa linha de shell: nomes com espaço vão entre aspas simples.
        if($project.Contains("'")) { throw "Nome de projeto com aspas simples não é suportado: $project" }
        $slug=($project -replace '[^A-Za-z0-9]+','-').Trim('-').ToLowerInvariant()
        for($index=0;$index -lt $Order.Count;++$index) {
            $tier=$Order[$index]
            $run=Join-Path $taskOutput ('{0}/{1:00}-{2}' -f $slug,($index+1),$tier)
            New-Item -ItemType Directory -Path $run -Force | Out-Null
            # Parar o app e esperar a bateria esfriar: sem isso as rodadas finais
            # medem clock reduzido e não o nível pedido.
            & $Adb shell am force-stop $Package
            $cooling=[Diagnostics.Stopwatch]::StartNew()
            while($true) {
                $celsius=[double](([regex]::Match((& $Adb shell dumpsys battery | Out-String),'temperature: (\d+)')).Groups[1].Value)/10.0
                if($celsius -le $MaximumStartBatteryCelsius) { break }
                if($cooling.Elapsed.TotalMinutes -ge $MaximumCoolingMinutes) { throw "Bateria a $celsius °C após $MaximumCoolingMinutes min; rodada não iniciada." }
                Start-Sleep -Seconds 20
            }
            Write-Output ('{0} rodada {1}: início com bateria a {2:F1} °C após {3:F1} min de resfriamento' -f $project,($index+1),$celsius,$cooling.Elapsed.TotalMinutes)
            & $Adb shell dumpsys battery | Set-Content (Join-Path $run 'battery-before.txt')
            & $Adb shell dumpsys thermalservice | Set-Content (Join-Path $run 'thermal-before.txt')
            & $Adb logcat -c
            $launch=@('shell','am','start','-S','-n',$shellActivity,
                '--es','astra.open_project',"'$project'",'--ez','aether.profile_frames','true',
                '--ez','aether.disable_dynamic_resolution','true',
                '--ez','aether.lock_rendering_quality','true',
                '--es','aether.quality_preset',$tier.ToLowerInvariant(),
                '--ef','aether.target_fps',$TargetFps.ToString($culture))
            if(!$Edit) { $launch+=@('--ez','aether.start_play','true') }
            else {
                # O reuso estático do editor (M7) tornaria quadros parados gratuitos.
                $launch+=@('--ez','aether.disable_editor_scene_reuse','true')
                if($Pose.Count -eq 5) {
                    $launch+=@('--ez','aether.lock_camera','true')
                    $names=@('x','y','z','yaw','pitch')
                    for($part=0;$part -lt 5;++$part) { $launch+=@('--ef',('aether.camera_'+$names[$part]),$Pose[$part].ToString($culture)) }
                }
            }
            if($PostIsolation) { $launch+=@('--ei','aether.post_isolation',[string]$PostIsolation) }
            $launch+=@('--ez','aether.post_fog_resolve',$(if($FogResolve){'true'}else{'false'}))
            if($tier -eq 'C') { $launch+=@('--es','aether.reference_resolution',$ReferenceResolution) }
            else { $launch+=@('--ef','aether.resolution_scale','1') }
            $started=(& $Adb @launch 2>&1 | Out-String)
            $started | Set-Content (Join-Path $run 'launch.txt')
            if($LASTEXITCODE -ne 0 -or $started -match 'Error|Exception') { throw "Launch rejeitado: $project/$tier." }
            Start-Sleep -Seconds 20
            $layer=Find-FrameProfileSurfaceLayer -Text (& $Adb shell dumpsys SurfaceFlinger --list | Out-String) -Component $renderActivity
            if(!$layer) { throw "Surface não encontrada: $project/$tier." }
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
                $summary.Remove('actualPresentTimestampsNs')
                $summary.continuous=$state.continuous;$summary.polls=$state.polls
                $summary | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $run 'surface-summary.json')
            }
            & $Adb logcat -d -s Aether.Android:I | Set-Content (Join-Path $run 'profile.log')
            $logText=Get-Content (Join-Path $run 'profile.log') -Raw
            $policy=[regex]::Matches($logText,'\[(RenderPolicy|ReferenceResolution|RuntimeControls|PostIsolation|PostFogResolve)\][^\r\n]*') | ForEach-Object Value
            $policy | Set-Content (Join-Path $run 'policy.txt')
            if($PostIsolation -and !($policy -match "\[PostIsolation\] mask=$PostIsolation ")) {
                throw "A máscara de pós $PostIsolation não foi aplicada ($project/$tier)."
            }
            if(!($policy -match "\[RenderPolicy\] preset=$($tier.ToLowerInvariant()) ")) {
                throw "O nível pedido não foi aplicado ($project/$tier); veja policy.txt."
            }
            if($tier -eq 'C' -and !($policy -match '\[ReferenceResolution\] superficie=')) {
                throw "O APK instalado não aplicou aether.reference_resolution ($project/C): instale um build com o G0."
            }
            $sample=[regex]::Match($logText,'\[FrameProfile\] (\{[^\r\n]*\})')
            if(!$sample.Success) { throw "Nenhuma janela FrameProfile: $project/$tier." }
            $processId=($sample.Groups[1].Value | ConvertFrom-Json).pid
            $windows=@(ConvertFrom-FrameProfileLog -Text $logText -ExpectedPid $processId -ExpectedInstances $null)
            $contexts=@(ConvertFrom-FrameProfileContextLog -Text $logText -ExpectedPid $processId)
            $scene=if($Edit){'authored-project'}else{'authored-project-play'}
            $capture=Get-FrameProfileCapture -Windows $windows -Contexts $contexts -MinimumSeconds 15 -Scene $scene
            if(!$capture) { throw "Captura curta demais: $project/$tier." }
            $capture | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $run 'capture.json')
            & $Adb shell screencap -p /sdcard/astra-g0.png
            & $Adb pull /sdcard/astra-g0.png (Join-Path $run 'screen.png') | Out-Null
            & $Adb shell dumpsys battery | Set-Content (Join-Path $run 'battery-after.txt')
            & $Adb shell dumpsys thermalservice | Set-Content (Join-Path $run 'thermal-after.txt')
            $fogResolved=[bool]($policy -match '\[PostFogResolve\] .*por texel')
            if($fogResolved -ne [bool]$FogResolve) { throw "Névoa resolvida=$fogResolved difere do pedido ($project/$tier)." }
            $row=[ordered]@{project=$project;round=$index+1;tier=$tier;postIsolation=$PostIsolation;fogResolve=$fogResolved
                displayedFps=if($summary){$summary.displayedFps}else{$null}
                continuous=$state.continuous;internalFps=$capture.presentFps
                width=$capture.context.width;height=$capture.context.height
                attribution=$capture.gpuPassAttribution;thermal=$capture.pressure.thermalPressure
                dominantPressure=$capture.pressure.dominant}
            foreach($metric in $FrameProfileMetricNames) {
                if($capture.metrics -and $capture.metrics.Contains($metric)) { $row[$metric]=$capture.metrics[$metric].mean }
            }
            $rows.Add([pscustomobject]$row)
            Write-Output ('{0} rodada {1} nível {2}: {3:F2} FPS apresentados, interno {4:F2}, {5}x{6}, atribuição {7}' -f
                $project,($index+1),$tier,$row.displayedFps,$row.internalFps,$row.width,$row.height,$row.attribution)
        }
    }
} finally {
    # Nenhum override de diagnóstico permanece na sessão normal do usuário.
    & $Adb shell am start -S -n $shellActivity | Out-Null
    $rows | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $taskOutput 'rows.json')
}

# Tabela por projeto e nível: média das rodadas, para decidir os alvos do G1.
$columns=@('displayedFps','internalFps') + @($FrameProfileMetricNames | Where-Object {
    $name=$_; @($rows | Where-Object { $null -ne $_.$name }).Count -gt 0 })
$lines=New-Object System.Collections.Generic.List[string]
$lines.Add('# G0: linha de base por passe')
$lines.Add('')
$lines.Add("Aparelho: $((Get-Content (Join-Path $taskOutput 'device-model.txt')) -join ' '). Referência do nível C: $ReferenceResolution. Ordem: $($Order -join '/').")
$lines.Add('')
$lines.Add('| Métrica | ' + (($Projects | ForEach-Object { "$_ S | $_ C" }) -join ' | ') + ' |')
$lines.Add('|---|' + (($Projects | ForEach-Object { '---|---' }) -join '|') + '|')
foreach($column in $columns) {
    $cells=foreach($project in $Projects) { foreach($tier in @('S','C')) {
        $values=@($rows | Where-Object { $_.project -eq $project -and $_.tier -eq $tier -and $null -ne $_.$column } | ForEach-Object { [double]$_.$column })
        if($values.Count) { ($values | Measure-Object -Average).Average.ToString('F2',$culture) } else { '—' }
    } }
    $lines.Add("| $column | $($cells -join ' | ') |")
}
$lines | Set-Content (Join-Path $taskOutput 'baseline.md') -Encoding utf8
Write-Output "Tabela: $(Join-Path $taskOutput 'baseline.md')"
