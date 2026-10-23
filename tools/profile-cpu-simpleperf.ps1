# Perfil de CPU no POCO F7 com simpleperf (cortes de 23/10/2026, item 5 da medição de 22–23/10).
#
# Exige um APK profileable (-PastraProfileable=true): o simpleperf do Android grava o processo
# do app sem root nem debuggable. Abre o projeto em Play com o mesmo lançamento do coletor do G0
# (nível travado, sem resolução dinâmica), espera a cena assentar, grava N segundos com pilha
# por frame pointer e gera no host os relatórios por thread, por biblioteca, por função (próprio)
# e inclusivo (com filhos), usando as .so não removidas do build para os símbolos.
param(
    [string]$Adb='C:/Users/donod/AppData/Local/Android/Sdk/platform-tools/adb.exe',
    [string]$Package='dev.aether.editor.u07',
    [string]$SerialPrefix='adb-53e1eb7',
    [string]$Project='Vila Astra',
    [ValidateSet('S','C')][string]$Tier='S',
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [int]$SettleSeconds=20,
    [int]$Seconds=20,
    [int]$Frequency=2000,
    [ValidateSet('fp','dwarf')][string]$CallGraph='fp',
    [string]$ReferenceResolution='1600x720',
    # Opções extras de lançamento ("--ez chave valor"), por exemplo para o A/B do corte.
    [string[]]$ExtraLaunch=@(),
    [string]$SymbolDirectory='',
    [string]$Simpleperf='C:/Users/donod/AppData/Local/Android/Sdk/ndk/29.0.14206865/simpleperf/bin/windows/x86_64/simpleperf.exe'
)
$ErrorActionPreference='Stop'
$serial=@(& $Adb devices | ForEach-Object { if($_ -match "^($([regex]::Escape($SerialPrefix))[^\t]*)\tdevice$") { $Matches[1] } })[0]
if(!$serial) { throw 'Nenhum aparelho no ADB.' }
$env:ANDROID_SERIAL=$serial
$out=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
New-Item -ItemType Directory -Path $out -Force | Out-Null
$culture=[Globalization.CultureInfo]::InvariantCulture
$dump=(& $Adb shell dumpsys package $Package | Out-String)
if($dump -notmatch 'PROFILEABLE') { Write-Warning 'O pacote não declara PROFILEABLE; o simpleperf pode recusar --app.' }
($dump -split "`n" | Select-String 'versionCode|versionName|flags=') | Set-Content (Join-Path $out 'package-version.txt')
if($Project.Contains("'")) { throw 'Nome de projeto com aspas simples não é suportado.' }
& $Adb shell am force-stop $Package
& $Adb shell dumpsys battery | Set-Content (Join-Path $out 'battery-before.txt')
& $Adb logcat -c
$launch=@('shell','am','start','-S','-n',"$Package/dev.aether.editor.shell.AstraShellActivity",
    '--es','astra.open_project',"'$Project'",'--ez','aether.start_play','true',
    '--ez','aether.disable_dynamic_resolution','true',
    '--es','aether.quality_preset',$Tier.ToLowerInvariant(),'--ef','aether.target_fps','60')
if($Tier -eq 'C') { $launch+=@('--es','aether.reference_resolution',$ReferenceResolution) }
else { $launch+=@('--ef','aether.resolution_scale','1') }
$launch+=$ExtraLaunch
(& $Adb @launch 2>&1 | Out-String) | Set-Content (Join-Path $out 'launch.txt')
Start-Sleep -Seconds $SettleSeconds
$processId=((& $Adb shell pidof $Package) | Out-String).Trim().Split(' ')[0]
if(!$processId) { throw "Processo $Package não encontrado." }
$record="simpleperf record --app $Package -e cpu-clock -f $Frequency --call-graph $CallGraph --duration $Seconds -o /data/local/tmp/astra-perf.data"
Write-Output "Gravando $Seconds s ($record)"
(& $Adb shell $record 2>&1 | Out-String) | Set-Content (Join-Path $out 'record.txt')
& $Adb pull /data/local/tmp/astra-perf.data (Join-Path $out 'perf.data') | Out-Null
& $Adb shell rm -f /data/local/tmp/astra-perf.data
& $Adb shell dumpsys battery | Set-Content (Join-Path $out 'battery-after.txt')
& $Adb logcat -d -s Aether.Android:I | Set-Content (Join-Path $out 'profile.log')
$data=Join-Path $out 'perf.data'
$symfs=@()
if($SymbolDirectory) { $symfs=@('--symfs',$SymbolDirectory) }
$reports=[ordered]@{
    'por-thread.txt'=@('--sort','comm')
    'por-biblioteca.txt'=@('--sort','dso')
    'por-funcao.txt'=@('--sort','dso,symbol','--percent-limit','0.3')
    'inclusivo.txt'=@('--children','--sort','symbol','--percent-limit','1')
    'thread-funcao.txt'=@('--sort','comm,symbol','--percent-limit','0.5')
}
foreach($name in $reports.Keys) {
    & $Simpleperf report -i $data @symfs @($reports[$name]) 2>&1 | Set-Content (Join-Path $out $name)
}
Write-Output "Relatórios em $out"
