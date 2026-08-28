[CmdletBinding()]
param(
    [string]$AdbPath = "$env:LOCALAPPDATA/Android/Sdk/platform-tools/adb.exe",
    [string]$DeviceSerial,
    [ValidatePattern('^[a-zA-Z0-9_]+(?:\.[a-zA-Z0-9_]+)+$')]
    [string]$PackageName = 'dev.aether.editor',
    [ValidateRange(5, 60)][int]$DurationSeconds = 20,
    [ValidateRange(50, 2000)][int]$Frequency = 500,
    [string]$OutputDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$resolvedAdb = (Resolve-Path -LiteralPath $AdbPath).Path
function Invoke-CpuAdb {
    param([string[]]$Arguments)
    $output = @(& $resolvedAdb -s $DeviceSerial @Arguments 2>&1 | ForEach-Object { $_.ToString() })
    if ($LASTEXITCODE -ne 0) { throw "ADB/simpleperf: $($output -join "`n")" }
    return $output -join "`n"
}
if (-not $DeviceSerial) {
    $devices = @(& $resolvedAdb devices)
    if ($LASTEXITCODE -ne 0) { throw 'ADB indisponível.' }
    $serials = @($devices | ForEach-Object { if ($_ -match '^(\S+)\s+device\s*$') { $Matches[1] } })
    if ($serials.Count -ne 1) { throw 'Informe -DeviceSerial para um único aparelho autorizado.' }
    $DeviceSerial = $serials[0]
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $PSScriptRoot "../build/android-cpu/$(Get-Date -Format yyyyMMdd-HHmmss)"
}
$resolvedOutput = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $resolvedOutput) { throw 'Use um diretório novo para preservar capturas anteriores.' }
New-Item -ItemType Directory -Path $resolvedOutput -Force | Out-Null
$remote = '/data/local/tmp/aether-cpu-' + [guid]::NewGuid().ToString('N') + '.data'
$report = [ordered]@{
    schemaVersion = 1; status = 'running'; package = $PackageName; serial = $DeviceSerial
    event = 'task-clock:u'; frequency = $Frequency; requestedSeconds = $DurationSeconds
    startedAtUtc = [DateTime]::UtcNow.ToString('o'); pid = $null; error = $null
    limitation = 'Amostragem de CPU de usuário, não delta completo de CPU do processo nem atribuição exata de cada spike. JIT .NET pode aparecer como unknown.'
}
try {
    $report.pid = (Invoke-CpuAdb @('shell', 'pidof', $PackageName)).Trim()
    if ($report.pid -notmatch '^\d+$') { throw 'Inicie o app; a captura exige exatamente um PID.' }
    $focus = Invoke-CpuAdb @('shell', 'dumpsys', 'window')
    if ($focus -notmatch ('mCurrentFocus=.*' + [regex]::Escape($PackageName) + '/')) {
        throw 'App fora de foco ou aparelho bloqueado. Abra-o/desbloqueie manualmente antes da captura.'
    }
    $report.simpleperfVersion = (Invoke-CpuAdb @('shell', 'simpleperf', '--version')).Trim()
    $appLog = Invoke-CpuAdb @('logcat', '-d', '-v', 'brief', '--pid', $report.pid, 'Aether.Android:I', '*:S')
    $report.managedBuildId = if ($appLog -match '\[ManagedBuild\] id=([a-f0-9]{64})') { $Matches[1] } else { $null }
    $log = Invoke-CpuAdb @('shell', 'simpleperf', 'record', '--app', $PackageName,
        '-e', 'task-clock:u', '-f', [string]$Frequency, '--duration', [string]$DurationSeconds, '-o', $remote)
    $log | Set-Content -LiteralPath (Join-Path $resolvedOutput 'record.txt') -Encoding utf8
    Invoke-CpuAdb @('pull', $remote, (Join-Path $resolvedOutput 'perf.data')) | Out-Null
    $report.pidAfter = (Invoke-CpuAdb @('shell', 'pidof', $PackageName)).Trim()
    if ($report.pidAfter -ne $report.pid) { throw 'Processo reiniciou durante a captura.' }
    $focusAfter = Invoke-CpuAdb @('shell', 'dumpsys', 'window')
    if ($focusAfter -notmatch ('mCurrentFocus=.*' + [regex]::Escape($PackageName) + '/')) {
        throw 'App perdeu foco; captura não representa trabalho contínuo em primeiro plano.'
    }
    foreach ($spec in @(
        @{name = 'threads'; sort = 'comm,tid'},
        @{name = 'libraries'; sort = 'comm,dso'},
        @{name = 'symbols'; sort = 'comm,dso,symbol'})) {
        Invoke-CpuAdb @('shell', 'simpleperf', 'report', '-i', $remote, '--sort', $spec.sort, '--percent-limit', '0') |
            Set-Content -LiteralPath (Join-Path $resolvedOutput "$($spec.name).txt") -Encoding utf8
    }
    $report.sha256 = (Get-FileHash -LiteralPath (Join-Path $resolvedOutput 'perf.data') -Algorithm SHA256).Hash
    if ($log -notmatch 'Samples recorded:\s*([1-9]\d*)') { throw 'Nenhuma amostra confirmada; consulte record.txt.' }
    $report.samples = [long]$Matches[1]
    $report.status = 'captured'
} catch {
    $report.status = 'failed'; $report.error = $_.Exception.Message
    Write-Host "Falha: $($report.error)"
} finally {
    # Only our UUID-named artifact; no cache, package data or system setting is removed.
    & $resolvedAdb -s $DeviceSerial shell rm -f $remote | Out-Null
    $report.finishedAtUtc = [DateTime]::UtcNow.ToString('o')
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $resolvedOutput 'report.json') -Encoding utf8
}
Write-Host "CPU: $($report.status); $resolvedOutput"
if ($report.status -ne 'captured') { exit 1 }
