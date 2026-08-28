[CmdletBinding()]
param(
    [string]$AdbPath = "$env:LOCALAPPDATA/Android/Sdk/platform-tools/adb.exe",
    [string]$DeviceSerial,
    [string]$ApkPath = "$PSScriptRoot/../android/app/build/outputs/apk/debug/app-debug.apk",
    [string]$OutputDirectory = "$PSScriptRoot/../build/android-validation/scene-$(Get-Date -Format yyyyMMdd-HHmmss)",
    [switch]$SkipInstall,
    [switch]$RequireIdenticalScreenshot,
    [switch]$MaterialPreview,
    [ValidateRange(15, 180)][int]$TimeoutSeconds = 60
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'android-shell-lifecycle.ps1')
$component = 'dev.aether.editor/android.app.NativeActivity'
$package = 'dev.aether.editor'
$sceneOption = if ($MaterialPreview) { 'aether.material_preview' } else { 'aether.scene_preview' }
if (-not $DeviceSerial) {
    $devices = @(& $AdbPath devices | Where-Object { $_ -match '^\S+\s+device$' })
    if ($devices.Count -ne 1) { throw 'Informe DeviceSerial: é necessário um aparelho autorizado.' }
    $DeviceSerial = ($devices[0] -split '\s+')[0]
}
function Invoke-Device([string[]]$DeviceArgs) {
    $output = & $AdbPath -s $DeviceSerial @DeviceArgs 2>&1
    if ($LASTEXITCODE -ne 0) { throw "ADB falhou ($($DeviceArgs -join ' ')): $output" }
    return ($output -join "`n")
}
function Get-AppLog([string]$AppProcess) {
    return Invoke-Device @('logcat', '-d', "--pid=$AppProcess", '-v', 'threadtime')
}
function Save-Screenshot([string]$Name) {
    # Unique diagnostic file only; no shell glob or user directory cleanup.
    $remote = "/data/local/tmp/aether-scene-$([Guid]::NewGuid().ToString('N')).png"
    $local = Join-Path $OutputDirectory "$Name.png"
    try {
        Invoke-Device @('shell', 'screencap', '-p', $remote) | Out-Null
        Invoke-Device @('pull', $remote, $local) | Out-Null
    } finally { Invoke-Device @('shell', 'rm', $remote) | Out-Null }
    return (Get-FileHash -LiteralPath $local).Hash
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$report = [ordered]@{
    schemaVersion = 1; status = 'running'; device = $DeviceSerial
    apkSha256 = (Get-FileHash -LiteralPath $ApkPath).Hash
    cases = @(); error = $null
}
$launched = $false
try {
    $keyguard = ConvertFrom-AndroidKeyguardPolicy -Policy (Invoke-Device @('shell', 'dumpsys', 'window', 'policy'))
    if ($null -eq $keyguard.Showing -or $keyguard.Showing -or -not $keyguard.ScreenOn) {
        throw 'Desbloqueie manualmente o aparelho e mantenha a tela ligada.'
    }
    if (-not $SkipInstall) { Invoke-Device @('install', '-r', $ApkPath) | Write-Output }
    $modes = if ($MaterialPreview) { @('bindless', 'fallback', 'texture-fallback') } else { @('bindless', 'fallback') }
    foreach ($mode in $modes) {
        $fallback = if ($mode -eq 'fallback') { 'true' } else { 'false' }
        $textureFallback = if ($mode -eq 'texture-fallback') { 'true' } else { 'false' }
        $descriptorMode = if ($mode -eq 'fallback') { 'fallback' } else { 'bindless' }
        Invoke-Device @('shell', 'am', 'start', '-S', '-n', $component,
            '--ez', $sceneOption, 'true', '--ez', 'aether.scene_validation', 'true',
            '--ez', 'aether.force_texture_fallback', $textureFallback,
            '--ez', 'aether.profile_frames', 'true',
            '--ez', 'aether.force_descriptor_fallback', $fallback) | Write-Output
        $launched = $true
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $appProcess = ''
        $log = ''
        try {
            do {
                Start-Sleep -Milliseconds 400
                $appProcess = Invoke-Device @('shell', 'pidof', $package)
                if ($appProcess -notmatch '^\d+$') { throw 'Processo ausente ou PID inválido.' }
                $log = Get-AppLog $appProcess
            } until ($log -match '\[ScenePreview\] presented step=3 ' -or $timer.Elapsed.TotalSeconds -ge $TimeoutSeconds)
            $steps = if ($MaterialPreview) { @(1,1,0,1) } else { @(2,2,1,2) }
            for ($stepIndex=0; $stepIndex -lt 4; ++$stepIndex) {
                $expected = "step=$stepIndex instances=$($steps[$stepIndex])"
                if ($log -notmatch "\[ScenePreview\] presented $expected abi=1 stride=88") { throw "Etapa não apresentada: $expected." }
            }
            if ($log -notmatch 'CoreCLR hospedado no shell: Ping\(2,3\)=5') { throw 'Core não carregou no contexto do módulo Rendering.' }
            if ($log -notmatch '\[VulkanDiagnostics\] validation=enabled') { throw 'Use APK debug com Khronos validation ativa.' }
            if ($log -notmatch "Descritores: caminho=$descriptorMode,") { throw "Caminho incorreto: $mode." }
            if ($MaterialPreview) {
                if ($log -notmatch '\[MaterialPreview\] ready triangles=36480 ') { throw 'Malha/material não ficaram prontos.' }
                if ($mode -eq 'texture-fallback' -and $log -notmatch 'textures=RGBA8-fallback') { throw 'Fallback de textura não foi exercitado.' }
                if ($mode -ne 'texture-fallback' -and $log -notmatch 'textures=ASTC6x6') { throw 'Caminho ASTC indisponível neste aparelho.' }
            }
            $before = Save-Screenshot "$mode-before-resume"
            $snapshotPattern = '\[ScenePreview\] snapshot (instances=\d+ hash=\d+ yaw=\S+ pitch=\S+)'
            $snapshots = [regex]::Matches($log, $snapshotPattern)
            if ($snapshots.Count -lt 4) { throw 'Snapshots das mutações ausentes.' }
            $beforeSnapshot = $snapshots[-1].Groups[1].Value
            $activations = [regex]::Matches($log, 'Primeiro frame após ativação apresentado:').Count
            Invoke-Device @('shell', 'input', 'keyevent', 'KEYCODE_HOME') | Out-Null
            Start-Sleep -Seconds 1
            # Same process and same intent options: do not restart the diagnostic world.
            Invoke-Device @('shell', 'am', 'start', '-n', $component) | Out-Null
            $timer.Restart()
            do {
                Start-Sleep -Milliseconds 400
                if ((Invoke-Device @('shell', 'pidof', $package)) -ne $appProcess) { throw 'PID mudou durante a retomada.' }
                $log = Get-AppLog $appProcess
                $resumed = [regex]::Matches($log, 'Primeiro frame após ativação apresentado:').Count -gt $activations
            } until ($resumed -or $timer.Elapsed.TotalSeconds -ge $TimeoutSeconds)
            if (-not $resumed) { throw 'Nenhum frame apresentado após retomada.' }
            $snapshots = [regex]::Matches($log, $snapshotPattern)
            if ($snapshots.Count -lt 5 -or $snapshots[-1].Groups[1].Value -cne $beforeSnapshot) {
                throw 'Matrizes, entidades, cores ou câmera mudaram durante a retomada.'
            }
            # Full display captures include OEM overlays outside app ownership.
            # Preserve both images and expose inequality, never crop/mask it.
            $timer.Restart()
            do {
                Start-Sleep -Seconds 1
                $after = Save-Screenshot "$mode-after-resume"
            } until ($before -eq $after -or -not $RequireIdenticalScreenshot -or $timer.Elapsed.TotalSeconds -ge $TimeoutSeconds)
            $sameImage = $before -eq $after
            if (-not $sameImage) {
                if ($RequireIdenticalScreenshot) { throw 'Capturas completas diferem após retomada; examine cena, gestos e overlays do sistema.' }
                Write-Warning 'Screenshot completa difere; revisão visual necessária. Snapshot da cena e câmera foi preservado.'
            }
            $log = Get-AppLog $appProcess
            if ($log -match 'VUID-|Validation Error|Fatal signal|FATAL EXCEPTION|extraction_status=-|initialization=failed|\[(ScenePreview|MaterialPreview)\] Falha') {
                throw 'Erro Vulkan, extração ou runtime; examine o log.'
            }
            $report.cases += [ordered]@{ mode = $mode; pid = $appProcess; status = 'passed'; steps = $steps; snapshot = $beforeSnapshot; resumeStateIdentical = $true; resumeImageIdentical = $sameImage; visualReviewRequired = -not $sameImage; screenshotSha256 = $after; materialLogs = @($log -split "`n" | Where-Object { $_ -match '\[MaterialPreview\]' }) }
            Write-Output "PASS técnico: cena $mode, etapas $($steps -join '/'), retomada com PID e snapshot preservados; screenshot idêntica=$sameImage."
        } finally { $log | Set-Content -LiteralPath (Join-Path $OutputDirectory "$mode-logcat.txt") -Encoding utf8 }
    }
    $report.status = 'passed'
} catch {
    $report.status = 'failed'; $report.error = $_.Exception.Message
    throw
} finally {
    # Leave a stable interactive preview, not the automatic mutations or forced fallback.
    if ($launched) {
        try { Invoke-Device @('shell', 'am', 'start', '-S', '-n', $component, '--ez', $sceneOption, 'true') | Out-Null }
        catch { Write-Warning "Não foi possível abrir a cena normal: $_" }
    }
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'report.json') -Encoding utf8
}
