[CmdletBinding()]
param(
    [string]$AdbPath = "$env:LOCALAPPDATA/Android/Sdk/platform-tools/adb.exe",
    [string]$DeviceSerial,
    [string]$ApkPath = "$PSScriptRoot/../android/app/build/outputs/apk/debug/app-debug.apk",
    [string]$OutputDirectory = "$PSScriptRoot/../build/android-validation/rendering-$(Get-Date -Format yyyyMMdd-HHmmss)",
    [switch]$SkipInstall,
    [ValidateRange(10, 180)][int]$TimeoutSeconds = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'android-shell-lifecycle.ps1')
$component = 'dev.aether.editor/android.app.NativeActivity'
$package = 'dev.aether.editor'
if (-not $DeviceSerial) {
    $devices = @(& $AdbPath devices | Where-Object { $_ -match '^\S+\s+device$' })
    if ($devices.Count -ne 1) { throw 'Informe DeviceSerial: é necessário exatamente um aparelho autorizado.' }
    $DeviceSerial = ($devices[0] -split '\s+')[0]
}
function Invoke-Device([string[]]$DeviceArgs) {
    $output = & $AdbPath -s $DeviceSerial @DeviceArgs 2>&1
    if ($LASTEXITCODE -ne 0) { throw "ADB falhou ($($DeviceArgs -join ' ')): $output" }
    return ($output -join "`n")
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$report = [ordered]@{
    schemaVersion = 1; status = 'running'; device = $DeviceSerial
    apkSha256 = (Get-FileHash -LiteralPath $ApkPath -Algorithm SHA256).Hash
    cases = @(); error = $null
}
$installed = $false
try {
    $policy = Invoke-Device @('shell', 'dumpsys', 'window', 'policy')
    $keyguard = ConvertFrom-AndroidKeyguardPolicy -Policy $policy
    if ($null -eq $keyguard.Showing -or $keyguard.Showing -or -not $keyguard.ScreenOn) {
        throw 'É necessário comprovar tela ligada e aparelho desbloqueado; o runner não altera o keyguard.'
    }
    if (-not $SkipInstall) { Invoke-Device @('install', '-r', $ApkPath) | Write-Output }
    $installed = $true
    foreach ($mode in @('bindless', 'fallback')) {
        $fallback = if ($mode -eq 'fallback') { 'true' } else { 'false' }
        $probe = if ($mode -eq 'bindless') { 'true' } else { 'false' }
        Invoke-Device @('shell', 'am', 'start', '-S', '-n', $component,
            '--ez', 'aether.force_descriptor_fallback', $fallback,
            '--ez', 'aether.astc_probe', $probe) | Write-Output
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $appProcess = ''
        $log = ''
        try {
            do {
                Start-Sleep -Milliseconds 500
                $appProcess = Invoke-Device @('shell', 'pidof', $package)
                if ($appProcess -notmatch '^\d+$') { throw 'PID inválido ou processo ausente.' }
                $log = Invoke-Device @('logcat', '-d', "--pid=$appProcess", '-v', 'threadtime')
                $ready = $log -match 'Primeiro frame após ativação apresentado:'
                $probeReady = $mode -eq 'fallback' -or $log -match '\[AstcProbe\] \{'
            } until (($ready -and $probeReady) -or $timer.Elapsed.TotalSeconds -ge $TimeoutSeconds)
            if (-not $ready -or -not $probeReady) { throw "Timeout no caminho $mode." }
            # Janela curta adicional para detectar falhas de apresentação após o primeiro frame.
            Start-Sleep -Seconds 3
            $log = Invoke-Device @('logcat', '-d', "--pid=$appProcess", '-v', 'threadtime')
            if ($log -notmatch '\[VulkanDiagnostics\] validation=enabled') { throw 'Validação Vulkan não está ativa; use o APK debug.' }
            if ($log -match 'VUID-|Validation Error|Fatal signal|FATAL EXCEPTION') { throw "Erro Vulkan/crash no caminho $mode." }
            if ($log -notmatch "Descritores: caminho=$mode,") { throw "O renderer não selecionou $mode." }
            $case = [ordered]@{ mode = $mode; pid = $appProcess; validation = $true; status = 'passed'; astc = $null }
            if ($mode -eq 'bindless') {
                $line = @($log -split "`n" | Where-Object { $_ -match '\[AstcProbe\] (\{.*\})' })[-1]
                $astc = ($line -replace '^.*\[AstcProbe\] ', '') | ConvertFrom-Json
                if (-not $astc.succeeded) { throw 'A prova ASTC rejeitou a saída ou a medição.' }
                if ($astc.gpu_encode_ms -ge 300) { throw 'A codificação GPU excedeu o orçamento de 300 ms da PoC-E.' }
                $case.astc = $astc
            }
            $report.cases += $case
            Write-Output "Caminho $mode validado com camada Khronos ativa."
        } finally {
            $log | Set-Content -LiteralPath (Join-Path $OutputDirectory "$mode-logcat.txt") -Encoding utf8
        }
    }
    $report.status = 'passed'
} catch {
    $report.status = 'failed'
    $report.error = $_.Exception.Message
    throw
} finally {
    # Sem pm clear, settings globais ou desbloqueio; deixa o app no modo normal.
    if ($installed) {
        try { Invoke-Device @('shell', 'am', 'start', '-S', '-n', $component) | Write-Output }
        catch { Write-Warning "Não foi possível restaurar a sessão normal: $_" }
    }
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'report.json') -Encoding utf8
}
