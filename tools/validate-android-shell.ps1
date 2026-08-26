[CmdletBinding()]
param(
    [string]$AdbPath,
    [string]$DeviceSerial,
    [string]$ApkPath = (Join-Path $PSScriptRoot "..\android\app\build\outputs\apk\debug\app-debug.apk"),
    [ValidateRange(0, 1000)]
    [int]$LifecycleCycles = 3,
    [ValidateRange(5, 300)]
    [int]$StartupTimeoutSeconds = 45,
    [ValidateRange(5, 120)]
    [int]$CycleTimeoutSeconds = 20,
    [ValidateRange(0, 120)]
    [int]$SoakMinutes = 0,
    [switch]$SkipInstall,
    [switch]$PreserveAppData,
    [switch]$ExerciseConfigurationChange,
    [switch]$ExerciseScreenCycle,
    [switch]$AllowScreenshotDifference,
    [switch]$KeepAppRunning,
    [string]$OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$PackageName = "dev.aether.editor"
$ActivityName = "android.app.NativeActivity"
$ComponentName = "$PackageName/$ActivityName"
$LogTag = "Aether.Android"
$FrameMilestonePattern = "Marco de renderização atingido: 1000 frames apresentados\."
$ResumeFramePattern = "Primeiro frame após ativação apresentado:"
$ResourcePatterns = @(
    "Surface Vulkan pronta:",
    "Swapchain pronta:",
    "Pipeline do tri"
)
$StartupPatterns = @(
    "Shell nativo iniciado\.",
    "Surface Vulkan pronta:",
    "Swapchain pronta:",
    "Pipeline do tri",
    "Aplicativo ativo\."
)
$RemoteArtifacts = [System.Collections.Generic.List[string]]::new()
$EvidenceLog = [System.Collections.Generic.List[string]]::new()
$ConfigurationState = $null
$WakeState = $null
$Report = [ordered]@{
    schemaVersion = 1
    status = "running"
    startedAtUtc = [DateTime]::UtcNow.ToString("o")
    finishedAtUtc = $null
    package = $PackageName
    activity = $ActivityName
    apk = $null
    device = [ordered]@{}
    configuration = [ordered]@{
        lifecycleCycles = $LifecycleCycles
        startupTimeoutSeconds = $StartupTimeoutSeconds
        cycleTimeoutSeconds = $CycleTimeoutSeconds
        soakMinutes = $SoakMinutes
        configurationChange = [bool]$ExerciseConfigurationChange
        screenCycle = [bool]$ExerciseScreenCycle
        screenshotEqualityRequired = -not [bool]$AllowScreenshotDifference
    }
    checks = [System.Collections.Generic.List[object]]::new()
    screenshots = [ordered]@{}
    measurements = [ordered]@{
        resourceRecreationsDuringLifecycle = 0
    }
    artifacts = [ordered]@{}
    error = $null
}

function Resolve-AdbExecutable {
    $candidates = [System.Collections.Generic.List[string]]::new()
    if ($AdbPath) { $candidates.Add($AdbPath) }

    $command = Get-Command adb -ErrorAction SilentlyContinue
    if ($command) { $candidates.Add($command.Source) }

    foreach ($sdkRoot in @($env:ANDROID_HOME, $env:ANDROID_SDK_ROOT)) {
        if ($sdkRoot) { $candidates.Add((Join-Path $sdkRoot "platform-tools\adb.exe")) }
    }
    if ($env:LOCALAPPDATA) {
        $candidates.Add((Join-Path $env:LOCALAPPDATA "Android\Sdk\platform-tools\adb.exe"))
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "adb não encontrado. Informe -AdbPath ou configure ANDROID_HOME/ANDROID_SDK_ROOT."
}

function Invoke-AdbBase {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [switch]$AllowFailure
    )

    $output = @(& $script:ResolvedAdb @Arguments 2>&1 | ForEach-Object { $_.ToString() })
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0 -and -not $AllowFailure) {
        throw "adb $($Arguments -join ' ') falhou (código $exitCode): $($output -join [Environment]::NewLine)"
    }
    return $output
}

function Invoke-Adb {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [switch]$AllowFailure
    )

    $deviceArguments = @("-s", $script:ResolvedSerial) + $Arguments
    return Invoke-AdbBase -Arguments $deviceArguments -AllowFailure:$AllowFailure
}

function Get-ConnectedDeviceSerial {
    $devices = Invoke-AdbBase -Arguments @("devices", "-l")
    $connected = @(
        $devices | ForEach-Object {
            if ($_ -match "^(\S+)\s+device(?:\s|$)") { $Matches[1] }
        }
    )

    if ($DeviceSerial) {
        if ($connected -notcontains $DeviceSerial) {
            throw "O aparelho '$DeviceSerial' não está conectado e autorizado. Conectados: $($connected -join ', ')."
        }
        return $DeviceSerial
    }
    if ($connected.Count -eq 0) { throw "Nenhum aparelho ADB autorizado foi encontrado." }
    if ($connected.Count -gt 1) {
        throw "Mais de um aparelho ADB está conectado; use -DeviceSerial. Conectados: $($connected -join ', ')."
    }
    return $connected[0]
}

function Get-AdbValue {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    return ((Invoke-Adb -Arguments $Arguments -AllowFailure) -join "`n").Trim()
}

function Get-AetherLog {
    return ((Invoke-Adb -Arguments @("logcat", "-d", "-v", "brief", "$LogTag`:V", "Aether.Validation:I", "AndroidRuntime:E", "ActivityManager:E", "DEBUG:E", "*:S") -AllowFailure) -join "`n")
}

function Get-CrashLog {
    return ((Invoke-Adb -Arguments @("logcat", "-b", "crash", "-d", "-v", "brief") -AllowFailure) -join "`n")
}

function Get-PatternCount {
    param([string]$Text, [string]$Pattern)
    return [regex]::Matches($Text, $Pattern, [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
}

function Wait-ForPatternCount {
    param(
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][int]$MinimumCount,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][string]$Description
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $log = Get-AetherLog
        if ((Get-PatternCount -Text $log -Pattern $Pattern) -ge $MinimumCount) { return $log }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)

    throw "Timeout aguardando $Description (padrão '$Pattern', mínimo $MinimumCount)."
}

function Write-LogMarker {
    param([Parameter(Mandatory = $true)][string]$Prefix)
    $marker = "$Prefix-$([Guid]::NewGuid().ToString('N'))"
    Invoke-Adb -Arguments @("shell", "log", "-p", "i", "-t", "Aether.Validation", $marker) | Out-Null
    return $marker
}

function Get-LogAfterMarker {
    param([Parameter(Mandatory = $true)][string]$Marker)
    $log = Get-AetherLog
    $markerIndex = $log.LastIndexOf($Marker, [StringComparison]::Ordinal)
    if ($markerIndex -lt 0) { return "" }
    return $log.Substring($markerIndex)
}

function Wait-ForPatternAfterMarker {
    param(
        [Parameter(Mandatory = $true)][string]$Marker,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][string]$Description
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $segment = Get-LogAfterMarker -Marker $Marker
        if ($segment -and $segment -match $Pattern) { return $segment }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)

    throw "Timeout aguardando $Description após o marcador '$Marker' (padrão '$Pattern')."
}

function Wait-ForWakefulness {
    param(
        [Parameter(Mandatory = $true)][ValidateSet("Awake", "Asleep")][string]$Expected,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $powerState = Get-AdbValue -Arguments @("shell", "dumpsys", "power")
        if ($powerState -match "mWakefulness=$Expected") { return }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timeout aguardando mWakefulness=$Expected."
}

function Assert-NoRuntimeFailure {
    $aetherLog = Get-AetherLog
    $crashLog = Get-CrashLog
    $failurePatterns = @(
        "FATAL EXCEPTION",
        "ANR in $([regex]::Escape($PackageName))",
        "Fatal signal",
        "E/$([regex]::Escape($LogTag)).*(Falha|Erro)",
        "$([regex]::Escape($LogTag)).*E.*(Falha|Erro)"
    )
    foreach ($pattern in $failurePatterns) {
        if ($aetherLog -match $pattern -or $crashLog -match $pattern) {
            throw "Falha de runtime detectada no Logcat pelo padrão '$pattern'."
        }
    }
}

function Get-AppPid {
    $pidText = Get-AdbValue -Arguments @("shell", "pidof", $PackageName)
    if (-not $pidText) { return $null }
    return ($pidText -split "\s+")[0]
}

function Assert-AppPid {
    param([string]$ExpectedPid, [string]$Context)
    $actualPid = Get-AppPid
    if (-not $actualPid) { throw "O processo do app não existe após ${Context}." }
    if ($ExpectedPid -and $actualPid -ne $ExpectedPid) {
        throw "O PID mudou após ${Context}: esperado $ExpectedPid, atual $actualPid."
    }
    return $actualPid
}

function Start-AetherActivity {
    $startOutput = Invoke-Adb -Arguments @("shell", "am", "start", "-W", "-n", $ComponentName)
    if (($startOutput -join "`n") -match "Error:") {
        throw "Android recusou iniciar $ComponentName`: $($startOutput -join [Environment]::NewLine)"
    }
}

function Prepare-DeviceWakeState {
    $powerState = Get-AdbValue -Arguments @("shell", "dumpsys", "power")
    $initiallyAsleep = $powerState -match "mWakefulness=Asleep"
    $needsProximityOverride = $initiallyAsleep -or [bool]$ExerciseScreenCycle
    $state = [ordered]@{
        initiallyAsleep = $initiallyAsleep
        proximityOverridden = $needsProximityOverride
        systemProximity = $null
        globalProximity = $null
    }

    if ($needsProximityOverride) {
        $state.systemProximity = Get-AdbValue -Arguments @("shell", "settings", "get", "system", "enable_screen_on_proximity_sensor")
        $state.globalProximity = Get-AdbValue -Arguments @("shell", "settings", "get", "global", "enable_screen_on_proximity_sensor")
        if ($state.systemProximity -ne "null") {
            Invoke-Adb -Arguments @("shell", "settings", "put", "system", "enable_screen_on_proximity_sensor", "0") | Out-Null
        }
        if ($state.globalProximity -ne "null") {
            Invoke-Adb -Arguments @("shell", "settings", "put", "global", "enable_screen_on_proximity_sensor", "0") | Out-Null
        }
    }

    Invoke-Adb -Arguments @("shell", "input", "keyevent", "KEYCODE_WAKEUP") | Out-Null
    Wait-ForWakefulness -Expected "Awake" -TimeoutSeconds $StartupTimeoutSeconds
    Invoke-Adb -Arguments @("shell", "wm", "dismiss-keyguard") -AllowFailure | Out-Null
    Invoke-Adb -Arguments @("shell", "input", "keyevent", "KEYCODE_BACK") -AllowFailure | Out-Null
    Start-Sleep -Milliseconds 500
    return $state
}

function Restore-DeviceWakeState {
    param([Parameter(Mandatory = $true)][object]$State)
    if ($State.proximityOverridden) {
        if ($State.systemProximity -eq "null") {
            Invoke-Adb -Arguments @("shell", "settings", "delete", "system", "enable_screen_on_proximity_sensor") -AllowFailure | Out-Null
        } else {
            Invoke-Adb -Arguments @("shell", "settings", "put", "system", "enable_screen_on_proximity_sensor", $State.systemProximity) -AllowFailure | Out-Null
        }
        if ($State.globalProximity -eq "null") {
            Invoke-Adb -Arguments @("shell", "settings", "delete", "global", "enable_screen_on_proximity_sensor") -AllowFailure | Out-Null
        } else {
            Invoke-Adb -Arguments @("shell", "settings", "put", "global", "enable_screen_on_proximity_sensor", $State.globalProximity) -AllowFailure | Out-Null
        }
    }

    if ($State.initiallyAsleep) {
        $powerState = Get-AdbValue -Arguments @("shell", "dumpsys", "power")
        if ($powerState -match "mWakefulness=Awake") {
            Invoke-Adb -Arguments @("shell", "input", "keyevent", "KEYCODE_POWER") -AllowFailure | Out-Null
            Wait-ForWakefulness -Expected "Asleep" -TimeoutSeconds $CycleTimeoutSeconds
        }
    }
}

function Save-DeviceScreenshot {
    param([Parameter(Mandatory = $true)][string]$Name)
    $remotePath = "/sdcard/Download/aether-validation-$([Guid]::NewGuid().ToString('N')).png"
    $localPath = Join-Path $script:ResolvedOutputDirectory "$Name.png"
    $RemoteArtifacts.Add($remotePath)
    Invoke-Adb -Arguments @("shell", "screencap", "-p", $remotePath) | Out-Null
    Invoke-Adb -Arguments @("pull", $remotePath, $localPath) | Out-Null
    if (-not (Test-Path -LiteralPath $localPath -PathType Leaf)) {
        throw "A captura '$Name' não foi transferida para $localPath."
    }
    return [ordered]@{
        path = (Resolve-Path -LiteralPath $localPath).Path
        sha256 = (Get-FileHash -LiteralPath $localPath -Algorithm SHA256).Hash
        bytes = (Get-Item -LiteralPath $localPath).Length
    }
}

function Add-PassedCheck {
    param([string]$Name, [object]$Details)
    $Report.checks.Add([ordered]@{ name = $Name; status = "passed"; details = $Details })
}

$script:ResolvedAdb = $null
$script:ResolvedSerial = $null
$script:ResolvedOutputDirectory = $null

try {
    $script:ResolvedAdb = Resolve-AdbExecutable
    $script:ResolvedSerial = Get-ConnectedDeviceSerial

    if (-not $OutputDirectory) {
        $OutputDirectory = Join-Path $PSScriptRoot "..\build\android-validation\$([DateTime]::Now.ToString('yyyyMMdd-HHmmss'))"
    }
    $script:ResolvedOutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
    New-Item -ItemType Directory -Path $script:ResolvedOutputDirectory -Force | Out-Null

    $resolvedApk = [IO.Path]::GetFullPath($ApkPath)
    $Report.apk = $resolvedApk
    $Report.artifacts.outputDirectory = $script:ResolvedOutputDirectory
    $Report.artifacts.adb = $script:ResolvedAdb

    if (-not $SkipInstall -and -not (Test-Path -LiteralPath $resolvedApk -PathType Leaf)) {
        throw "APK não encontrado em '$resolvedApk'. Gere-o com :app:assembleDebug ou informe -ApkPath."
    }

    $Report.device.serial = $script:ResolvedSerial
    $Report.device.manufacturer = Get-AdbValue -Arguments @("shell", "getprop", "ro.product.manufacturer")
    $Report.device.model = Get-AdbValue -Arguments @("shell", "getprop", "ro.product.model")
    $Report.device.device = Get-AdbValue -Arguments @("shell", "getprop", "ro.product.device")
    $Report.device.board = Get-AdbValue -Arguments @("shell", "getprop", "ro.product.board")
    $Report.device.socModel = Get-AdbValue -Arguments @("shell", "getprop", "ro.soc.model")
    $Report.device.androidRelease = Get-AdbValue -Arguments @("shell", "getprop", "ro.build.version.release")
    $Report.device.apiLevel = Get-AdbValue -Arguments @("shell", "getprop", "ro.build.version.sdk")
    $Report.device.abi = Get-AdbValue -Arguments @("shell", "getprop", "ro.product.cpu.abi")

    if (-not $SkipInstall) {
        Invoke-Adb -Arguments @("install", "-r", "-t", $resolvedApk) | Out-Null
        Add-PassedCheck -Name "apk-install" -Details "APK instalado com adb install -r -t."
    }
    if (-not $PreserveAppData) {
        Invoke-Adb -Arguments @("shell", "pm", "clear", $PackageName) | Out-Null
        Add-PassedCheck -Name "app-data-clean" -Details "Dados do pacote limpos antes do teste."
    }

    Invoke-Adb -Arguments @("shell", "am", "force-stop", $PackageName) | Out-Null
    Invoke-Adb -Arguments @("logcat", "-c") | Out-Null
    $WakeState = Prepare-DeviceWakeState
    $Report.configuration.initiallyAsleep = $WakeState.initiallyAsleep
    Start-AetherActivity

    foreach ($pattern in $StartupPatterns) {
        Wait-ForPatternCount -Pattern $pattern -MinimumCount 1 -TimeoutSeconds $StartupTimeoutSeconds -Description "inicialização gráfica" | Out-Null
    }
    Wait-ForPatternCount -Pattern $FrameMilestonePattern -MinimumCount 1 -TimeoutSeconds $StartupTimeoutSeconds -Description "1.000 frames apresentados" | Out-Null
    $initialPid = Assert-AppPid -ExpectedPid $null -Context "a inicialização"
    Assert-NoRuntimeFailure
    $EvidenceLog.Add("--- startup ---`n$(Get-AetherLog)")
    Add-PassedCheck -Name "startup" -Details ([ordered]@{ pid = $initialPid; frameMilestone = 1000 })

    $Report.screenshots.before = Save-DeviceScreenshot -Name "before-lifecycle"

    for ($cycle = 1; $cycle -le $LifecycleCycles; ++$cycle) {
        $cycleMarker = Write-LogMarker -Prefix "lifecycle-$cycle"

        Invoke-Adb -Arguments @("shell", "input", "keyevent", "KEYCODE_HOME") | Out-Null
        Wait-ForPatternAfterMarker -Marker $cycleMarker -Pattern "Aplicativo suspenso\." -TimeoutSeconds $CycleTimeoutSeconds -Description "suspensão do ciclo $cycle" | Out-Null
        Assert-AppPid -ExpectedPid $initialPid -Context "enviar o ciclo $cycle ao background" | Out-Null
        Start-AetherActivity

        Wait-ForPatternAfterMarker -Marker $cycleMarker -Pattern "Aplicativo ativo\." -TimeoutSeconds $CycleTimeoutSeconds -Description "retomada do ciclo $cycle" | Out-Null
        $cycleLog = Wait-ForPatternAfterMarker -Marker $cycleMarker -Pattern $ResumeFramePattern -TimeoutSeconds $CycleTimeoutSeconds -Description "primeiro frame após retomada no ciclo $cycle"
        Assert-AppPid -ExpectedPid $initialPid -Context "retomar o ciclo $cycle" | Out-Null
        Assert-NoRuntimeFailure
        $allResourcesRecreated = $true
        foreach ($pattern in $ResourcePatterns) {
            if ($cycleLog -notmatch $pattern) {
                $allResourcesRecreated = $false
            }
        }
        if ($allResourcesRecreated) { ++$Report.measurements.resourceRecreationsDuringLifecycle }
        $EvidenceLog.Add("--- lifecycle $cycle ---`n$cycleLog")
        Write-Progress -Activity "Validando lifecycle Android" -Status "$cycle de $LifecycleCycles ciclos" -PercentComplete (($cycle / [Math]::Max(1, $LifecycleCycles)) * 100)
    }
    Write-Progress -Activity "Validando lifecycle Android" -Completed
    Add-PassedCheck -Name "background-foreground" -Details ([ordered]@{
        cycles = $LifecycleCycles
        stablePid = $initialPid
        renderResumedEveryCycle = $true
        resourceRecreationsObserved = $Report.measurements.resourceRecreationsDuringLifecycle
    })

    if ($SoakMinutes -gt 0) {
        $soak = [Diagnostics.Stopwatch]::StartNew()
        $deadline = [DateTime]::UtcNow.AddMinutes($SoakMinutes)
        $samples = 0
        while ([DateTime]::UtcNow -lt $deadline) {
            Start-Sleep -Seconds 5
            Assert-AppPid -ExpectedPid $initialPid -Context "o soak de estabilidade" | Out-Null
            Assert-NoRuntimeFailure
            ++$samples
            Write-Progress -Activity "Soak de estabilidade Android" -Status "$([Math]::Floor($soak.Elapsed.TotalMinutes)) de $SoakMinutes min" -PercentComplete ([Math]::Min(100, ($soak.Elapsed.TotalMinutes / $SoakMinutes) * 100))
        }
        $soak.Stop()
        Write-Progress -Activity "Soak de estabilidade Android" -Completed
        $Report.measurements.soakElapsedSeconds = [Math]::Round($soak.Elapsed.TotalSeconds, 3)
        $Report.measurements.soakHealthSamples = $samples
        Add-PassedCheck -Name "stability-soak" -Details ([ordered]@{
            requestedMinutes = $SoakMinutes
            elapsedSeconds = $Report.measurements.soakElapsedSeconds
            healthSamples = $samples
            stablePid = $initialPid
            powerMeasured = $false
        })
    }

    if ($ExerciseConfigurationChange) {
        $uiModeOutput = Get-AdbValue -Arguments @("shell", "cmd", "uimode", "night")
        if ($uiModeOutput -notmatch "Night mode:\s+(\S+)") {
            throw "Não foi possível identificar o uiMode atual: '$uiModeOutput'."
        }
        $ConfigurationState = $Matches[1]
        $configMarker = Write-LogMarker -Prefix "configuration-change"
        $targetUiMode = if ($ConfigurationState -eq "yes") { "no" } else { "yes" }
        Invoke-Adb -Arguments @("shell", "cmd", "uimode", "night", $targetUiMode) | Out-Null
        Wait-ForPatternAfterMarker -Marker $configMarker -Pattern "Configuração alterada:" -TimeoutSeconds $CycleTimeoutSeconds -Description "APP_CMD_CONFIG_CHANGED" | Out-Null
        $configLog = Wait-ForPatternAfterMarker -Marker $configMarker -Pattern "Pipeline do tri" -TimeoutSeconds $CycleTimeoutSeconds -Description "pipeline após configuração"
        Assert-AppPid -ExpectedPid $initialPid -Context "a mudança de configuração" | Out-Null
        Assert-NoRuntimeFailure
        $EvidenceLog.Add("--- configuration change ---`n$configLog")
        Add-PassedCheck -Name "configuration-change" -Details ([ordered]@{
            previousUiMode = $ConfigurationState
            targetUiMode = $targetUiMode
            stablePid = $initialPid
        })

        $restoreMarker = Write-LogMarker -Prefix "configuration-restore"
        Invoke-Adb -Arguments @("shell", "cmd", "uimode", "night", $ConfigurationState) | Out-Null
        Wait-ForPatternAfterMarker -Marker $restoreMarker -Pattern "Configuração alterada:" -TimeoutSeconds $CycleTimeoutSeconds -Description "restauração da configuração" | Out-Null
        $restoreLog = Wait-ForPatternAfterMarker -Marker $restoreMarker -Pattern "Pipeline do tri" -TimeoutSeconds $CycleTimeoutSeconds -Description "pipeline após restauração"
        $EvidenceLog.Add("--- configuration restore ---`n$restoreLog")
        $ConfigurationState = $null
    }

    if ($ExerciseScreenCycle) {
        $screenMarker = Write-LogMarker -Prefix "screen-cycle"
        Invoke-Adb -Arguments @("shell", "input", "keyevent", "KEYCODE_POWER") | Out-Null
        Wait-ForPatternAfterMarker -Marker $screenMarker -Pattern "Aplicativo suspenso\." -TimeoutSeconds $CycleTimeoutSeconds -Description "suspensão ao apagar a tela" | Out-Null
        Wait-ForWakefulness -Expected "Asleep" -TimeoutSeconds $CycleTimeoutSeconds
        Invoke-Adb -Arguments @("shell", "input", "keyevent", "KEYCODE_WAKEUP") -AllowFailure | Out-Null
        Wait-ForWakefulness -Expected "Awake" -TimeoutSeconds $CycleTimeoutSeconds
        Invoke-Adb -Arguments @("shell", "wm", "dismiss-keyguard") -AllowFailure | Out-Null
        Start-AetherActivity
        Wait-ForPatternAfterMarker -Marker $screenMarker -Pattern "Aplicativo ativo\." -TimeoutSeconds $CycleTimeoutSeconds -Description "retomada após acender a tela" | Out-Null
        $screenLog = Wait-ForPatternAfterMarker -Marker $screenMarker -Pattern $ResumeFramePattern -TimeoutSeconds $CycleTimeoutSeconds -Description "primeiro frame após o ciclo de tela"
        Assert-AppPid -ExpectedPid $initialPid -Context "o ciclo de tela" | Out-Null
        Assert-NoRuntimeFailure
        $EvidenceLog.Add("--- screen cycle ---`n$screenLog")
        Add-PassedCheck -Name "screen-off-on" -Details ([ordered]@{ stablePid = $initialPid })
    }

    Invoke-Adb -Arguments @("shell", "am", "send-trim-memory", $PackageName, "RUNNING_CRITICAL") | Out-Null
    Start-Sleep -Milliseconds 500
    Assert-AppPid -ExpectedPid $initialPid -Context "trim-memory RUNNING_CRITICAL" | Out-Null
    Assert-NoRuntimeFailure
    Add-PassedCheck -Name "trim-memory-proxy" -Details "Processo sobreviveu; este comando é proxy e não garante APP_CMD_LOW_MEMORY real."

    $Report.screenshots.after = Save-DeviceScreenshot -Name "after-lifecycle"
    $sameScreenshot = $Report.screenshots.before.sha256 -eq $Report.screenshots.after.sha256
    if (-not $sameScreenshot -and -not $AllowScreenshotDifference) {
        $screenshotDeadline = [DateTime]::UtcNow.AddSeconds($CycleTimeoutSeconds)
        do {
            Start-Sleep -Milliseconds 500
            $Report.screenshots.after = Save-DeviceScreenshot -Name "after-lifecycle"
            $sameScreenshot = $Report.screenshots.before.sha256 -eq $Report.screenshots.after.sha256
        } while (-not $sameScreenshot -and [DateTime]::UtcNow -lt $screenshotDeadline)
    }
    if (-not $sameScreenshot -and -not $AllowScreenshotDifference) {
        throw "As capturas antes/depois diferem: $($Report.screenshots.before.sha256) != $($Report.screenshots.after.sha256)."
    }
    Add-PassedCheck -Name "screenshot-stability" -Details ([ordered]@{
        identical = $sameScreenshot
        beforeSha256 = $Report.screenshots.before.sha256
        afterSha256 = $Report.screenshots.after.sha256
    })

    Assert-NoRuntimeFailure
    $Report.status = "passed"
    Write-Host "PASS: shell Android validado em $LifecycleCycles ciclos; PID $initialPid; screenshot idêntica=$sameScreenshot."
}
catch {
    $Report.status = "failed"
    $Report.error = $_.Exception.Message
    Write-Host "FAIL: $($_.Exception.Message)" -ForegroundColor Red
}
finally {
    if ($script:ResolvedAdb -and $script:ResolvedSerial) {
        if ($ConfigurationState) {
            Invoke-Adb -Arguments @("shell", "cmd", "uimode", "night", $ConfigurationState) -AllowFailure | Out-Null
        }
        foreach ($remotePath in $RemoteArtifacts) {
            Invoke-Adb -Arguments @("shell", "rm", "-f", $remotePath) -AllowFailure | Out-Null
        }
        if (-not $KeepAppRunning) {
            Invoke-Adb -Arguments @("shell", "am", "force-stop", $PackageName) -AllowFailure | Out-Null
        }
        if ($WakeState) {
            Restore-DeviceWakeState -State $WakeState
            $WakeState = $null
        }
        if ($script:ResolvedOutputDirectory) {
            $logPath = Join-Path $script:ResolvedOutputDirectory "logcat.txt"
            (($EvidenceLog -join "`n`n") + "`n`n--- final logcat window ---`n" + (Get-AetherLog) + "`n`n--- crash buffer ---`n" + (Get-CrashLog)) | Set-Content -LiteralPath $logPath -Encoding UTF8
            $Report.artifacts.logcat = $logPath
        }
    }
    $Report.finishedAtUtc = [DateTime]::UtcNow.ToString("o")
    if ($script:ResolvedOutputDirectory) {
        $reportPath = Join-Path $script:ResolvedOutputDirectory "report.json"
        $Report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $reportPath -Encoding UTF8
        Write-Host "Relatório: $reportPath"
    }
}

if ($Report.status -ne "passed") { exit 1 }
