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
    [ValidateRange(0, 600)]
    [int]$ProfileSeconds = 0,
    [ValidateRange(0.1, 100.0)]
    [double]$PowerBudgetWatts = 4.0,
    [switch]$RequirePowerBudget,
    [switch]$RequireSoakBudget,
    [ValidateRange(1, 240)]
    [double]$MinimumSoakFps = 55,
    [switch]$SkipInstall,
    [switch]$PreserveAppData,
    [switch]$ExerciseConfigurationChange,
    [switch]$ExerciseScreenCycle,
    [ValidateRange(1, 100)]
    [int]$ScreenCycles = 1,
    [switch]$AllowScreenshotDifference,
    [switch]$KeepAppRunning,
    [string]$OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "android-shell-lifecycle.ps1")
. (Join-Path $PSScriptRoot "android-frame-profile.ps1")
. (Join-Path $PSScriptRoot "android-soak-profile.ps1")
if ($RequireSoakBudget -and $SoakMinutes -lt 30) { throw '-RequireSoakBudget exige pelo menos 30 minutos.' }
if ($RequirePowerBudget -and $SoakMinutes -eq 0) { throw '-RequirePowerBudget exige -SoakMinutes para coletar potência contínua.' }
$CaptureSeconds = [Math]::Max($ProfileSeconds, $SoakMinutes * 60)

$PackageName = "dev.aether.editor"
$ActivityName = "android.app.NativeActivity"
$ComponentName = "$PackageName/$ActivityName"
$LogTag = "Aether.Android"
$FrameMilestonePattern = "Marco de renderização atingido: 1000 frames apresentados\."
$ResumeFramePattern = "Primeiro frame após ativação apresentado:"
$ResourcePatterns = @(
    "Surface Vulkan pronta:",
    "Swapchain pronta:",
    "InstancedRenderer pronto:"
)
$StartupPatterns = @(
    "Shell nativo iniciado\.",
    "Surface Vulkan pronta:",
    "Swapchain pronta:",
    "InstancedRenderer pronto:",
    "Aplicativo ativo\."
)
$RemoteArtifacts = [System.Collections.Generic.List[string]]::new()
$EvidenceLog = [System.Collections.Generic.List[string]]::new()
$ConfigurationState = $null
$WakeState = $null
$Report = [ordered]@{
    schemaVersion = 4
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
        profileSeconds = $ProfileSeconds
        powerBudgetWatts = $PowerBudgetWatts
        requirePowerBudget = [bool]$RequirePowerBudget
        requireSoakBudget = [bool]$RequireSoakBudget
        minimumSoakFps = $MinimumSoakFps
        configurationChange = [bool]$ExerciseConfigurationChange
        screenCycle = [bool]$ExerciseScreenCycle
        screenCycles = if ($ExerciseScreenCycle) { $ScreenCycles } else { 0 }
        screenshotEqualityRequired = -not [bool]$AllowScreenshotDifference
    }
    checks = [System.Collections.Generic.List[object]]::new()
    screenshots = [ordered]@{}
    measurements = [ordered]@{
        resourceRecreationsDuringLifecycle = 0
        powerSamples = [System.Collections.Generic.List[object]]::new()
        keyguardWaits = [System.Collections.Generic.List[object]]::new()
        keyguardDismissRequests = [System.Collections.Generic.List[object]]::new()
    }
    artifacts = [ordered]@{}
    error = $null
    failureKind = $null
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

function Get-PowerSample {
    $thermal = Get-AdbValue -Arguments @("shell", "dumpsys", "thermalservice")
    $battery = Get-AdbValue -Arguments @("shell", "dumpsys", "battery")

    $thermalStatus = $null
    if ($thermal -match "(?m)^Thermal Status:\s*(-?\d+)\s*$") {
        $thermalStatus = [int]$Matches[1]
    }

    # Usa a seção atual do HAL, não o cache histórico mostrado antes dela.
    $currentHal = $thermal
    $halMarker = "Current temperatures from HAL:"
    $halIndex = $thermal.IndexOf($halMarker, [StringComparison]::Ordinal)
    if ($halIndex -ge 0) { $currentHal = $thermal.Substring($halIndex) }

    $currentAmps = $null
    $voltageVolts = $null
    if ($currentHal -match "Temperature\{mValue=([-+]?\d+(?:\.\d+)?),\s*mType=7,\s*mName=ibat") {
        $currentAmps = [double]::Parse($Matches[1], [Globalization.CultureInfo]::InvariantCulture)
    }
    if ($currentHal -match "Temperature\{mValue=([-+]?\d+(?:\.\d+)?),\s*mType=6,\s*mName=vbat") {
        $voltageVolts = [double]::Parse($Matches[1], [Globalization.CultureInfo]::InvariantCulture)
    }

    $powerWatts = $null
    if ($null -ne $currentAmps -and $null -ne $voltageVolts -and
        [Math]::Abs($currentAmps) -le 30.0 -and $voltageVolts -ge 2.0 -and $voltageVolts -le 20.0) {
        $powerWatts = [Math]::Round([Math]::Abs($currentAmps * $voltageVolts), 4)
    }

    $batteryLevel = $null
    $batteryTemperatureCelsius = $null
    if ($battery -match "(?m)^\s*level:\s*(\d+)\s*$") { $batteryLevel = [int]$Matches[1] }
    if ($battery -match "(?m)^\s*temperature:\s*(-?\d+)\s*$") {
        $batteryTemperatureCelsius = [Math]::Round(([int]$Matches[1]) / 10.0, 1)
    }

    $poweredStates = [regex]::Matches($battery, '(?m)^\s*(?:AC|USB|Wireless|Dock) powered:\s*(true|false)\s*$')
    $externallyPowered = if ($poweredStates.Count -ge 3) {
        @($poweredStates | Where-Object { $_.Groups[1].Value -eq 'true' }).Count -gt 0
    } else { $null }
    return [ordered]@{
        capturedAtUtc = [DateTime]::UtcNow.ToString("o")
        thermalStatus = $thermalStatus
        currentAmps = $currentAmps
        voltageVolts = $voltageVolts
        powerWatts = $powerWatts
        externallyPowered = $externallyPowered
        batteryLevelPercent = $batteryLevel
        batteryTemperatureCelsius = $batteryTemperatureCelsius
    }
}

function Get-AetherLog {
    return ((Invoke-Adb -Arguments @("logcat", "-d", "-v", "brief", "$LogTag`:V", "Aether.Validation:I", "AndroidRuntime:E", "ActivityManager:E", "DEBUG:E", "*:S") -AllowFailure) -join "`n")
}

function Get-CrashLog {
    return ((Invoke-Adb -Arguments @("logcat", "-b", "crash", "-d", "-v", "brief") -AllowFailure) -join "`n")
}

function Save-LifecycleDiagnostics {
    param([Parameter(Mandatory = $true)][string]$Reason)
    # Captura antes de qualquer cleanup: force-stop/restauração ocultariam o estado que falhou.
    $snapshot = [ordered]@{
        capturedAtUtc = [DateTime]::UtcNow.ToString("o")
        reason = $Reason
        pid = Get-AppPid
        power = @((Invoke-Adb -Arguments @("shell", "dumpsys", "power") -AllowFailure) |
            Where-Object { $_ -match "mWakefulness=|mInteractive=|Display Power:|mWakefulnessChanging=" })
        keyguard = @((Invoke-Adb -Arguments @("shell", "dumpsys", "window", "policy") -AllowFailure) |
            Where-Object { $_ -match "(?i)keyguard|showing=|occluded=|inputRestricted=|interactiveState=|screenState=|secure=|trusted=" })
        focus = @((Invoke-Adb -Arguments @("shell", "dumpsys", "window") -AllowFailure) |
            Where-Object { $_ -match "mCurrentFocus=|mFocusedApp=" })
        resumedActivity = @((Invoke-Adb -Arguments @("shell", "dumpsys", "activity", "activities") -AllowFailure) |
            Where-Object { $_ -match "topResumedActivity=|mResumedActivity=" })
    }
    $path = Join-Path $script:ResolvedOutputDirectory "lifecycle-diagnostics.json"
    $snapshot | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $path -Encoding UTF8
    $Report.artifacts.lifecycleDiagnostics = $path
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
        "(?m)^E/$([regex]::Escape($LogTag))"
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
    $arguments = @("shell", "am", "start", "-W", "-n", $ComponentName)
    if ($CaptureSeconds -gt 0) { $arguments += @("--ez", "aether.profile_frames", "true") }
    $startOutput = Invoke-Adb -Arguments $arguments
    if (($startOutput -join "`n") -match "Error:") {
        throw "Android recusou iniciar $ComponentName`: $($startOutput -join [Environment]::NewLine)"
    }
}

function Wait-ForDeviceUnlock {
    param([string]$Context, [int]$TimeoutSeconds)
    Write-Host "Aguardando tela ligada e desbloqueada ($Context); se houver PIN/biometria, desbloqueie manualmente."
    $result = Wait-AndroidKeyguardDismissed -TimeoutSeconds $TimeoutSeconds -ReadPolicy {
        (Invoke-Adb -Arguments @("shell", "dumpsys", "window", "policy")) -join "`n"
    } -RequestDismiss {
        $output = Invoke-Adb -Arguments @("shell", "wm", "dismiss-keyguard")
        $Report.measurements.keyguardDismissRequests.Add([ordered]@{
            context = $Context
            requestedAtUtc = [DateTime]::UtcNow.ToString("o")
            output = $output -join "`n"
        })
    }
    $result.context = $Context
    $Report.measurements.keyguardWaits.Add($result)
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
        # Registrar antes de alterar o aparelho: falha no desbloqueio também exige restore.
        $script:WakeState = $state
        if ($state.systemProximity -ne "null") {
            Invoke-Adb -Arguments @("shell", "settings", "put", "system", "enable_screen_on_proximity_sensor", "0") | Out-Null
        }
        if ($state.globalProximity -ne "null") {
            Invoke-Adb -Arguments @("shell", "settings", "put", "global", "enable_screen_on_proximity_sensor", "0") | Out-Null
        }
    }

    $script:WakeState = $state
    Invoke-Adb -Arguments @("shell", "input", "keyevent", "KEYCODE_WAKEUP") | Out-Null
    Wait-ForWakefulness -Expected "Awake" -TimeoutSeconds $StartupTimeoutSeconds
    Wait-ForDeviceUnlock -Context "startup" -TimeoutSeconds $StartupTimeoutSeconds
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
    if (Test-Path -LiteralPath $resolvedApk -PathType Leaf) {
        $Report.artifacts.apkSha256 = (Get-FileHash -LiteralPath $resolvedApk -Algorithm SHA256).Hash
    }
    $Report.artifacts.gitRevision = ((& git -C (Join-Path $PSScriptRoot '..') rev-parse HEAD 2>$null) -join '').Trim()
    $Report.artifacts.gitDirty = @(& git -C (Join-Path $PSScriptRoot '..') status --porcelain).Count -gt 0
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
    if ($CaptureSeconds -gt 0) { $Report.measurements.profileEnvironmentStart = Get-PowerSample }
    Start-AetherActivity

    foreach ($pattern in $StartupPatterns) {
        Wait-ForPatternCount -Pattern $pattern -MinimumCount 1 -TimeoutSeconds $StartupTimeoutSeconds -Description "inicialização gráfica" | Out-Null
    }
    Wait-ForPatternCount -Pattern $FrameMilestonePattern -MinimumCount 1 -TimeoutSeconds $StartupTimeoutSeconds -Description "1.000 frames apresentados" | Out-Null
    $initialPid = Assert-AppPid -ExpectedPid $null -Context "a inicialização"
    Assert-NoRuntimeFailure
    $managedLog = Get-AetherLog
    if ($managedLog -match '\[ManagedBuild\] id=([a-f0-9]{64}) reused=([01])') {
        $Report.measurements.managedBuild = [ordered]@{ observedId = $Matches[1]; reusedExtraction = $Matches[2] -eq '1'; apkId = $null; coreSha256 = $null }
        if (-not $SkipInstall) {
            $archive = [IO.Compression.ZipFile]::OpenRead($resolvedApk)
            try {
                $idEntry = $archive.GetEntry('assets/dotnet_build_id.txt')
                $dllEntry = $archive.GetEntry('assets/dotnet/Aether.Core.dll')
                if ($null -eq $idEntry -or $null -eq $dllEntry) { throw 'APK sem identidade/assembly gerenciado.' }
                $reader = [IO.StreamReader]::new($idEntry.Open())
                try { $Report.measurements.managedBuild.apkId = $reader.ReadToEnd() } finally { $reader.Dispose() }
                $stream = $dllEntry.Open()
                $hasher = [Security.Cryptography.SHA256]::Create()
                try { $Report.measurements.managedBuild.coreSha256 = [BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-', '') }
                finally { $stream.Dispose(); $hasher.Dispose() }
                if ($Report.measurements.managedBuild.apkId -cne $Report.measurements.managedBuild.observedId) {
                    throw 'Build .NET extraído não corresponde ao APK instalado.'
                }
            } finally { $archive.Dispose() }
        }
    } else { throw 'Inicialização sem identidade verificável do build .NET; recompile o APK.' }
    $EvidenceLog.Add("--- startup ---`n$(Get-AetherLog)")
    Add-PassedCheck -Name "startup" -Details ([ordered]@{ pid = $initialPid; frameMilestone = 1000 })

    if ($CaptureSeconds -gt 0) {
        $contextPath = Join-Path $script:ResolvedOutputDirectory 'capture-context.json'
        $windowPath = Join-Path $script:ResolvedOutputDirectory 'frame-windows.jsonl'
        $powerPath = Join-Path $script:ResolvedOutputDirectory 'power-samples.jsonl'
        $surfacePath = Join-Path $script:ResolvedOutputDirectory 'surface-batches.jsonl'
        foreach ($path in @($contextPath, $windowPath, $powerPath, $surfacePath)) {
            if (Test-Path -LiteralPath $path) { throw 'Diretório já contém coleta; use um novo para não misturar evidência.' }
        }
        $Report.artifacts.captureContext = $contextPath
        $Report.artifacts.frameWindows = $windowPath
        $Report.artifacts.powerSamples = $powerPath
        $Report.artifacts.surfaceBatches = $surfacePath
        $Report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $contextPath -Encoding utf8
        $profileDeadline = [DateTime]::UtcNow.AddSeconds($CaptureSeconds + $StartupTimeoutSeconds + $CycleTimeoutSeconds)
        $captureClock = [Diagnostics.Stopwatch]::StartNew()
        $nextPowerSample = 0.0
        $nextCpuSample = 0.0
        $capturedWindows = @{}
        $capture = $null
        $layerText = Get-AdbValue -Arguments @('shell', 'dumpsys', 'SurfaceFlinger', '--list')
        $surfaceLayer = Find-FrameProfileSurfaceLayer -Text $layerText -Component $ComponentName
        $displayTimes = [Collections.Generic.SortedSet[long]]::new()
        $displaySummary = $null
        $displayContinuous = $true
        $displayAvailable = $null -ne $surfaceLayer
        $displayLast = 0L
        $displayPolls = 0
        do {
            Assert-AppPid -ExpectedPid $initialPid -Context "a coleta CPU/frame" | Out-Null
            if ($SoakMinutes -gt 0 -and $captureClock.Elapsed.TotalSeconds -ge $nextPowerSample) {
                Assert-NoRuntimeFailure
                $sample = Get-PowerSample
                $sample.elapsedSeconds = $captureClock.Elapsed.TotalSeconds
                $Report.measurements.powerSamples.Add($sample)
                Write-ProfileEvidence -Path $powerPath -Value $sample
                $nextPowerSample = $sample.elapsedSeconds + 5
                Write-Progress -Activity 'CPU, apresentação e térmica Android' -Status "$([int]$sample.elapsedSeconds) / $CaptureSeconds s" -PercentComplete ([Math]::Min(100, $sample.elapsedSeconds / $CaptureSeconds * 100))
            }
            if ($displayAvailable) {
                # O nome reconhecido contém somente pacote/classe/#id, sem shell metacharacters.
                if ($surfaceLayer -notmatch '^[a-zA-Z0-9._/#]+$') { throw 'Nome de surface não reconhecido.' }
                $latency = Get-AdbValue -Arguments @('shell', 'dumpsys', 'SurfaceFlinger', '--latency', $surfaceLayer)
                $times = @(ConvertFrom-SurfaceFrameLatency $latency | Sort-Object -Unique)
                ++$displayPolls
                if ($times.Count -eq 0) {
                    $displayAvailable = $false
                } else {
                    if ($displayLast -gt 0 -and $times[0] -gt $displayLast) { $displayContinuous = $false }
                    $newTimes = [Collections.Generic.List[long]]::new()
                    foreach ($time in $times) { if ($displayTimes.Add($time)) { $newTimes.Add($time) } }
                    if ($newTimes.Count -gt 0) {
                        Write-ProfileEvidence -Path $surfacePath -Value ([ordered]@{
                            elapsedSeconds = $captureClock.Elapsed.TotalSeconds; layer = $surfaceLayer
                            continuous = $displayContinuous; timestamps = $newTimes.ToArray()
                        })
                    }
                    $displayLast = $times[-1]
                }
            }
            if ($captureClock.Elapsed.TotalSeconds -ge $nextCpuSample) {
                foreach ($window in @(ConvertFrom-FrameProfileLog -Text (Get-AetherLog) -ExpectedPid $initialPid)) {
                    if (-not $capturedWindows.ContainsKey("$($window.epoch):$($window.window)")) {
                        Write-ProfileEvidence -Path $windowPath -Value $window
                    }
                    $capturedWindows["$($window.epoch):$($window.window)"] = $window
                }
                $capture = Get-FrameProfileCapture -Windows @($capturedWindows.Values) -MinimumSeconds $CaptureSeconds
                $nextCpuSample = $captureClock.Elapsed.TotalSeconds + 2
            }
            if ($null -ne $capture -and $captureClock.Elapsed.TotalSeconds -ge $CaptureSeconds) {
                $displaySummary = Get-SurfaceFrameSummary -Timestamps @($displayTimes)
                if (-not $displayAvailable -or -not $displayContinuous -or
                    ($null -ne $displaySummary -and $displaySummary.elapsedSeconds -ge $CaptureSeconds)) { break }
            }
            Start-Sleep -Milliseconds 250
        } while ([DateTime]::UtcNow -lt $profileDeadline)
        if ($null -eq $capture) { throw "FrameProfile não produziu $CaptureSeconds s contínuos; sem evidência suficiente." }
        Assert-NoRuntimeFailure
        $Report.measurements.profileEnvironmentEnd = Get-PowerSample
        $Report.measurements.frameProfile = $capture
        $Report.measurements.displayFrameProfile = [ordered]@{
            layer = $surfaceLayer
            available = $displayAvailable
            continuous = $displayContinuous
            polls = $displayPolls
            summary = $displaySummary
        }
        $displayValidated = $displayAvailable -and $displayContinuous -and $null -ne $displaySummary -and
            $displaySummary.elapsedSeconds -ge $CaptureSeconds
        $Report.measurements.displayFrameProfile.valid = $displayValidated
        if (-not $displayValidated -and $null -ne $displaySummary) { $displaySummary.displayedFps = $null }
        $capture.pocA.displayedFpsMeasured = $displayValidated
        $capture.pocA.limitation = 'CPU e apresentação têm janelas próprias sobrepostas; um aparelho não fecha a matriz PoC-A.'
        if ($displayValidated) {
            $capture.pocA.observedDisplayedFps = $displaySummary.displayedFps
            Write-Host "SurfaceFlinger: $($displaySummary.displayedFps) frames exibidos/s em $($displaySummary.elapsedSeconds) s contínuos."
        }
        Add-PassedCheck -Name "frame-profile-capture" -Details ([ordered]@{
            frames = $capture.frames
            elapsedSeconds = $capture.elapsedSeconds
            cpuBudgetPassed = $capture.pocA.cpuBudgetPassed
            pocAAccepted = $false
        })
        Write-Host "FrameProfile: $($capture.presentFps) presents/s; CPU processo média=$($capture.metrics.process_cpu_ms.mean) ms; máximo=$($capture.metrics.process_cpu_ms.max) ms."
        # Sucesso da coleta não significa aprovação do orçamento nem FPS exibido comprovado.
        if ($SoakMinutes -gt 0) {
            $sample = Get-PowerSample
            $sample.elapsedSeconds = $captureClock.Elapsed.TotalSeconds
            $Report.measurements.powerSamples.Add($sample)
            Write-ProfileEvidence -Path $powerPath -Value $sample
            $power = Get-SoakPowerSummary -Samples @($Report.measurements.powerSamples) -BudgetWatts $PowerBudgetWatts
            $Report.measurements.soak = [ordered]@{
                requestedMinutes = $SoakMinutes
                elapsedSeconds = $captureClock.Elapsed.TotalSeconds
                power = $power
                displayedFpsMeasured = $displayValidated
                minimumRollingSecondFps = if ($displayValidated) { $displaySummary.minimumRollingSecondFps } else { $null }
                minimumRequiredFps = $MinimumSoakFps
                deviceCriteriaPassed = $false
                accepted = $false
                limitation = 'Um aparelho; HAL amostrado não comprova ausência de throttle entre amostras; CPU/scanout em intervalos sobrepostos.'
            }
            $soak = $Report.measurements.soak
            $soak.deviceCriteriaPassed = $displayValidated -and $soak.minimumRollingSecondFps -ge $MinimumSoakFps -and
                $power.elapsedSeconds -ge 1800 -and $power.withinPowerBudget -and $power.noThermalWarningObserved
            Write-Host "Soak: $($power.elapsedSeconds) s; potência média=$($power.averageWatts) W; mínimo em janela de 1 s=$($soak.minimumRollingSecondFps) FPS; critérios do aparelho=$($soak.deviceCriteriaPassed)."
            if (($RequirePowerBudget -or $RequireSoakBudget) -and -not $power.withinPowerBudget) {
                throw 'Potência ausente, descontínua, sob carga externa ou acima do orçamento; consulte measurements.soak.'
            }
            if ($RequireSoakBudget -and -not $soak.deviceCriteriaPassed) {
                throw 'Critérios térmicos/FPS/duração não atendidos no aparelho; consulte measurements.soak.'
            }
            Add-PassedCheck -Name 'continuous-soak-capture' -Details $soak
        }
        $captureClock.Stop()
        Write-Progress -Activity 'CPU, apresentação e térmica Android' -Completed
    }

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
        $configLog = Wait-ForPatternAfterMarker -Marker $configMarker -Pattern "InstancedRenderer pronto:" -TimeoutSeconds $CycleTimeoutSeconds -Description "pipeline após configuração"
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
        $restoreLog = Wait-ForPatternAfterMarker -Marker $restoreMarker -Pattern "InstancedRenderer pronto:" -TimeoutSeconds $CycleTimeoutSeconds -Description "pipeline após restauração"
        $EvidenceLog.Add("--- configuration restore ---`n$restoreLog")
        $ConfigurationState = $null
    }

    for ($screenCycle = 1; $ExerciseScreenCycle -and $screenCycle -le $ScreenCycles; ++$screenCycle) {
        $screenMarker = Write-LogMarker -Prefix "screen-cycle-$screenCycle"
        Invoke-Adb -Arguments @("shell", "input", "keyevent", "KEYCODE_POWER") | Out-Null
        Wait-ForPatternAfterMarker -Marker $screenMarker -Pattern "Aplicativo suspenso\." -TimeoutSeconds $CycleTimeoutSeconds -Description "suspensão ao apagar a tela" | Out-Null
        Wait-ForWakefulness -Expected "Asleep" -TimeoutSeconds $CycleTimeoutSeconds
        # Não aceitar uma ativação transitória anterior ao estado Asleep como retomada.
        $screenMarker = Write-LogMarker -Prefix "screen-wake-$screenCycle"
        Invoke-Adb -Arguments @("shell", "input", "keyevent", "KEYCODE_WAKEUP") -AllowFailure | Out-Null
        Wait-ForWakefulness -Expected "Awake" -TimeoutSeconds $CycleTimeoutSeconds
        Wait-ForDeviceUnlock -Context "screen-cycle-$screenCycle" -TimeoutSeconds $CycleTimeoutSeconds
        Start-AetherActivity
        Wait-ForPatternAfterMarker -Marker $screenMarker -Pattern "Aplicativo ativo\." -TimeoutSeconds $CycleTimeoutSeconds -Description "retomada após acender a tela" | Out-Null
        $screenLog = Wait-ForPatternAfterMarker -Marker $screenMarker -Pattern $ResumeFramePattern -TimeoutSeconds $CycleTimeoutSeconds -Description "primeiro frame após o ciclo de tela"
        Assert-AppPid -ExpectedPid $initialPid -Context "o ciclo de tela" | Out-Null
        Assert-NoRuntimeFailure
        $EvidenceLog.Add("--- screen cycle ---`n$screenLog")
        Add-PassedCheck -Name "screen-off-on" -Details ([ordered]@{ cycle = $screenCycle; stablePid = $initialPid })
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
    if ($_.Exception.Data['AetherFailureKind'] -eq 'device-keyguard-blocked') {
        $Report.status = "blocked"
        $Report.failureKind = 'device-keyguard-blocked'
    }
    Write-Host "$($Report.status.ToUpperInvariant()): $($Report.error)" -ForegroundColor Red
    if ($script:ResolvedAdb -and $script:ResolvedSerial -and $script:ResolvedOutputDirectory) {
        try { Save-LifecycleDiagnostics -Reason $Report.error }
        catch { $Report.artifacts.lifecycleDiagnosticsError = $_.Exception.Message }
    }
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
            $timelinePath = Join-Path $script:ResolvedOutputDirectory "lifecycle-timeline.txt"
            Invoke-Adb -Arguments @("logcat", "-d", "-v", "threadtime", "$LogTag`:V", "Aether.Validation:I", "*:S") -AllowFailure |
                Set-Content -LiteralPath $timelinePath -Encoding UTF8
            $Report.artifacts.lifecycleTimeline = $timelinePath
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
