# A/B por recurso no POCO F7 (22/10/2026): cada configuração roda em S, intercalada
# com rodadas de controle da base completa, para que a deriva de clock apareça nos
# controles e não nos recursos. Chama measure-g0-baseline.ps1 uma vez por rodada.
#
# Cada item de -Configurations é "nome=argumentos", com os argumentos do coletor do G0
# separados por espaço, por exemplo "sem-gather=-NoShadowGather" ou
# "sem-ao=-PostIsolation 2". "base" (sem argumentos) é o controle.
#
# Cortes de 23/10: -ControlName antes -ControlArguments "-NoQualityCeiling" com
# -Configurations "depois=-ProductDefaults" mede antes × depois no mesmo binário, e
# -Tier C repete o mesmo A/B no nível C (referência 1600x720).
param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [Parameter(Mandatory=$true)][string]$Project,
    [Parameter(Mandatory=$true)][string[]]$Configurations,
    # Um controle a cada N configurações (e no início e no fim).
    [int]$ControlEvery=3,
    [int]$Seconds=40,
    # Cena leve presa no vsync baixa o clock e esconde o custo: alvo acima do quadro real.
    [float]$TargetFps=60,
    [switch]$Edit,
    [float[]]$Pose=@(),
    [ValidateSet('S','C')][string]$Tier='S',
    [string]$ControlName='base',
    [string]$ControlArguments='',
    [double]$MaximumStartBatteryCelsius=40.0,
    [int]$MaximumCoolingMinutes=25
)
$ErrorActionPreference='Stop'
$collector=Join-Path $PSScriptRoot 'measure-g0-baseline.ps1'
$Adb='C:/Users/donod/AppData/Local/Android/Sdk/platform-tools/adb.exe'
$env:ANDROID_SERIAL=@(& $Adb devices | ForEach-Object { if($_ -match "^(adb-53e1eb7[^\t]*)\tdevice$") { $Matches[1] } })[0]
$plan=New-Object System.Collections.Generic.List[string]
$control="$ControlName=$ControlArguments"
$plan.Add($control)
for($index=0;$index -lt $Configurations.Count;++$index) {
    $plan.Add($Configurations[$index])
    if((($index+1) % $ControlEvery) -eq 0 -or $index -eq $Configurations.Count-1) { $plan.Add($control) }
}
$summary=New-Object System.Collections.Generic.List[object]
for($step=0;$step -lt $plan.Count;++$step) {
    $name,$arguments=$plan[$step].Split('=',2)
    $run=Join-Path $OutputDirectory ('{0:00}-{1}' -f ($step+1),$name)
    $extra=@{}
    $tokens=@($arguments.Split(' ',[StringSplitOptions]::RemoveEmptyEntries))
    for($t=0;$t -lt $tokens.Count;++$t) {
        $key=$tokens[$t].TrimStart('-')
        if($t+1 -lt $tokens.Count -and !$tokens[$t+1].StartsWith('-')) { $extra[$key]=[int]$tokens[$t+1];++$t }
        else { $extra[$key]=$true }
    }
    $call=@{OutputDirectory=$run;Projects=@($Project);Order=@($Tier);Seconds=$Seconds;TargetFps=$TargetFps
        MaximumStartBatteryCelsius=$MaximumStartBatteryCelsius;MaximumCoolingMinutes=$MaximumCoolingMinutes}
    # A névoa resolvida por texel é o padrão do APK; o coletor do G0 a desliga se não pedir.
    if($extra.ContainsKey('NoFogResolve')) { $extra.Remove('NoFogResolve') } else { $call.FogResolve=$true }
    if($Edit) { $call.Edit=$true }
    if($Pose.Count) { $call.Pose=$Pose }
    foreach($key in $extra.Keys) { $call[$key]=$extra[$key] }
    # Retomável: passo com rows.json já medido não roda de novo.
    if(!(Test-Path (Join-Path $run 'rows.json'))) {
        # Tela apagada (PIN) faz a superfície sumir no meio da rodada: parar com o motivo.
        $power=(& $Adb shell dumpsys power | Out-String)
        if($power -notmatch 'mWakefulness=Awake') { throw "POCO com a tela apagada antes do passo $($step+1) ($name)." }
        if(Test-Path $run) { Remove-Item -Recurse -Force $run }
        Write-Output "== $Project passo $($step+1)/$($plan.Count): $name $arguments"
        & $collector @call
    }
    $rows=Get-Content (Join-Path $run 'rows.json') -Raw | ConvertFrom-Json
    foreach($row in @($rows)) {
        $row | Add-Member -NotePropertyName configuration -NotePropertyValue $name -Force
        $row | Add-Member -NotePropertyName step -NotePropertyValue ($step+1) -Force
        $summary.Add($row)
    }
    $summary | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $OutputDirectory 'ab-rows.json')
}
