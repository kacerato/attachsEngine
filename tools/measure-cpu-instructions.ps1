# G1 (CPU): instruções de usuário por quadro, independentes do clock, em A/B/B/A
# alternando dois APKs do mesmo versionCode (refatorações sem chave de diagnóstico).
# Cada rodada roda measure-g0-baseline.ps1 (FPS interno da janela) e, em paralelo,
# simpleperf stat sobre o processo do app; instruções/quadro = instruções / (FPS x s).
param(
    [string]$Adb='C:/Users/donod/AppData/Local/Android/Sdk/platform-tools/adb.exe',
    [string]$Package='dev.aether.editor.u07',
    [string]$SerialPrefix='adb-53e1eb7',
    [Parameter(Mandatory=$true)][string]$ApkA,
    [Parameter(Mandatory=$true)][string]$ApkB,
    [string]$Project='Vila Astra',
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [ValidateSet('A','B')][string[]]$Order=@('A','B','B','A'),
    [int]$StatSeconds=25
)
$ErrorActionPreference='Stop'
$taskOutput=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$rows=New-Object System.Collections.Generic.List[object]
$index=0
foreach($variant in $Order) {
    ++$index
    $serial=@(& $Adb devices | ForEach-Object { if($_ -match "^($([regex]::Escape($SerialPrefix))[^\t]*)\tdevice$") { $Matches[1] } })[0]
    if(!$serial) { throw 'Nenhum aparelho no ADB.' }
    $apk=if($variant -eq 'A'){$ApkA}else{$ApkB}
    $install=(& $Adb -s $serial install -r $apk 2>&1 | Out-String)
    if($install -notmatch 'Success') { throw "Instalação falhou ($variant): $install" }
    $run=Join-Path $taskOutput ('{0:00}-{1}' -f $index,$variant)
    $job=Start-Job -ScriptBlock {
        param($script,$project,$run)
        & $script -Projects $project -Order 'S' -OutputDirectory $run -MaximumStartBatteryCelsius 99 -FogResolve *>&1 | Out-String
    } -ArgumentList (Join-Path $PSScriptRoot 'measure-g0-baseline.ps1'),$Project,$run
    # O coletor espera 20 s pela abertura do projeto e coleta 40 s; o stat cai dentro da coleta.
    Start-Sleep -Seconds 28
    $stat=(& $Adb -s $serial shell simpleperf stat --app $Package -e 'instructions:u,cpu-cycles:u' --duration $StatSeconds 2>&1 | Out-String)
    $log=Receive-Job -Job $job -Wait -AutoRemoveJob
    New-Item -ItemType Directory -Path $run -Force | Out-Null
    $stat | Set-Content (Join-Path $run 'simpleperf-stat.txt')
    $log | Set-Content (Join-Path $run 'baseline.log')
    $instructions=[regex]::Match($stat,'([\d,]+)\s+instructions:u'); $cycles=[regex]::Match($stat,'([\d,]+)\s+cpu-cycles:u')
    if(!$instructions.Success) { throw "simpleperf stat sem contagem de instruções ($variant): $stat" }
    $row=(Get-Content (Join-Path $run 'rows.json') -Raw | ConvertFrom-Json) | Select-Object -First 1
    $frames=[double]$row.internalFps*$StatSeconds
    $item=[ordered]@{round=$index;variant=$variant;internalFps=$row.internalFps;processCpuMs=$row.process_cpu_ms
        instructions=[double]($instructions.Groups[1].Value -replace ',','')
        cycles=if($cycles.Success){[double]($cycles.Groups[1].Value -replace ',','')}else{$null}}
    $item.instructionsPerFrame=$item.instructions/$frames
    $item.cyclesPerFrame=if($item.cycles){$item.cycles/$frames}else{$null}
    $rows.Add([pscustomobject]$item)
    Write-Output ('{0} {1}: {2:F1} FPS, {3:N0} instruções/quadro, {4:N0} ciclos/quadro' -f $index,$variant,$item.internalFps,$item.instructionsPerFrame,$item.cyclesPerFrame)
}
$rows | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $taskOutput 'instructions.json')
foreach($metric in 'instructionsPerFrame','cyclesPerFrame') {
    $valuesA=@($rows | Where-Object { $_.variant -eq 'A' } | ForEach-Object { [double]$_.$metric })
    $valuesB=@($rows | Where-Object { $_.variant -eq 'B' } | ForEach-Object { [double]$_.$metric })
    if($valuesA.Count -and $valuesB.Count) {
        $a=($valuesA | Measure-Object -Average).Average; $b=($valuesB | Measure-Object -Average).Average
        Write-Output ('{0}: A {1:N0} B {2:N0} ({3:+0.0;-0.0}%)' -f $metric,$a,$b,100*($b/$a-1))
    }
}
