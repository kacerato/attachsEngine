[CmdletBinding()]
param(
    [ValidateSet("Host", "Android")]
    [string]$Platform = "Host",
    [string]$BuildDirectory = "build/native",
    [string]$OutputDirectory = "build/benchmarks/physics2d",
    [ValidateRange(1, 20)]
    [int]$Trials = 3,
    [switch]$Quick,
    [switch]$SkipBuild,
    [string]$NdkRoot,
    [string]$AndroidAbi = "arm64-v8a",
    [string]$DeviceProfile = "unclassified",
    [ValidateRange(26, 99)]
    [int]$AndroidApi = 26
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$resolvedBuild = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDirectory))
$resolvedOutput = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
New-Item -ItemType Directory -Force -Path $resolvedOutput | Out-Null

function Resolve-CommandPath([string]$Name) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -ne $command) { return $command.Source }
    return $null
}

function Invoke-Checked([string]$Executable, [string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Comando falhou (exit $LASTEXITCODE): $Executable $($Arguments -join ' ')"
    }
}

$cmake = Resolve-CommandPath "cmake"
if (-not $SkipBuild -and -not $cmake) {
    $sdkCmakeRoot = Join-Path $env:LOCALAPPDATA "Android/Sdk/cmake"
    $cmakeCandidate = Get-ChildItem $sdkCmakeRoot -Filter cmake.exe -Recurse -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if ($cmakeCandidate) { $cmake = $cmakeCandidate.FullName }
}
if (-not $SkipBuild -and -not $cmake) {
    throw "cmake não encontrado. Adicione-o ao PATH ou use -SkipBuild com binários já compilados."
}
if (-not $SkipBuild) {
    # O pacote CMake do Android SDK traz o Ninja no mesmo diretório, mas nem
    # sempre registra ambos no PATH do runner self-hosted.
    $cmakeDirectory = Split-Path $cmake -Parent
    if (Test-Path (Join-Path $cmakeDirectory "ninja.exe")) {
        $env:PATH = "$cmakeDirectory;$env:PATH"
    }
}

$targets = @("benchmark_physics2d", "benchmark_physics2d_jolt", "benchmark_physics2d_box2d")
$adb = $null
$remoteDirectory = $null
$temperatureStartC = $null
$temperatureEndC = $null

function Get-AndroidBatteryTemperatureC([string]$AdbPath) {
    $line = (& $AdbPath shell dumpsys battery 2>$null | Select-String -Pattern '^\s*temperature:\s*(\d+)' |
        Select-Object -First 1)
    if (-not $line) { return $null }
    $raw = [int]$line.Matches[0].Groups[1].Value
    return [math]::Round($raw / 10.0, 1)
}

if (-not $SkipBuild) {
    if ($Platform -eq "Host") {
        Invoke-Checked $cmake @("-S", (Join-Path $repoRoot "native"), "-B", $resolvedBuild,
            "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release")
    } else {
        if (-not $NdkRoot) {
            if ($env:ANDROID_HOME) { $NdkRoot = Join-Path $env:ANDROID_HOME "ndk/27.1.12297006" }
        }
        if (-not $NdkRoot -or -not (Test-Path $NdkRoot)) {
            throw "NDK não encontrado. Informe -NdkRoot (versão pinada: 27.1.12297006)."
        }
        $toolchain = Join-Path $NdkRoot "build/cmake/android.toolchain.cmake"
        Invoke-Checked $cmake @("-S", (Join-Path $repoRoot "native"), "-B", $resolvedBuild,
            "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
            "-DANDROID_ABI=$AndroidAbi", "-DANDROID_PLATFORM=android-$AndroidApi")
    }
    Invoke-Checked $cmake (@("--build", $resolvedBuild, "--target") + $targets + @("-j", "4"))
}

$benchmarkArgs = @("--trials=$Trials")
if ($Quick) { $benchmarkArgs += "--quick" }
$outputs = [ordered]@{}
$binarySizes = [ordered]@{}

try {
    if ($Platform -eq "Host") {
        foreach ($target in $targets) {
            $candidate = Join-Path $resolvedBuild "$target.exe"
            if (-not (Test-Path $candidate)) { $candidate = Join-Path $resolvedBuild $target }
            if (-not (Test-Path $candidate)) { throw "Binário não encontrado: $candidate" }
            $report = Join-Path $resolvedOutput "$target.txt"
            & $candidate @benchmarkArgs 2>&1 | Tee-Object -FilePath $report
            if ($LASTEXITCODE -ne 0) { throw "$target reportou instabilidade (exit $LASTEXITCODE)." }
            $outputs[$target] = $report
            $binarySizes[$target] = (Get-Item $candidate).Length
        }
    } else {
        $adb = Resolve-CommandPath "adb"
        if (-not $adb) {
            $sdkRoot = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { Join-Path $env:LOCALAPPDATA "Android/Sdk" }
            $adbCandidate = Join-Path $sdkRoot "platform-tools/adb.exe"
            if (Test-Path $adbCandidate) { $adb = $adbCandidate }
        }
        if (-not $adb) { throw "adb não encontrado no PATH." }
        Invoke-Checked $adb @("get-state")
        $temperatureStartC = Get-AndroidBatteryTemperatureC $adb
        $remoteDirectory = "/data/local/tmp/aether-physics2d-ab-$([DateTimeOffset]::UtcNow.ToUnixTimeSeconds())"
        Invoke-Checked $adb @("shell", "mkdir", "-p", $remoteDirectory)
        foreach ($target in $targets) {
            $candidate = Join-Path $resolvedBuild $target
            if (-not (Test-Path $candidate)) { throw "Binário Android não encontrado: $candidate" }
            Invoke-Checked $adb @("push", $candidate, "$remoteDirectory/$target")
            Invoke-Checked $adb @("shell", "chmod", "700", "$remoteDirectory/$target")
            $report = Join-Path $resolvedOutput "$target.txt"
            & $adb shell "$remoteDirectory/$target" @benchmarkArgs 2>&1 | Tee-Object -FilePath $report
            if ($LASTEXITCODE -ne 0) { throw "$target reportou instabilidade no aparelho (exit $LASTEXITCODE)." }
            $outputs[$target] = $report
            $binarySizes[$target] = (Get-Item $candidate).Length
        }
        $temperatureEndC = Get-AndroidBatteryTemperatureC $adb
    }
} finally {
    if ($remoteDirectory -and $adb) {
        & $adb shell rm -rf $remoteDirectory | Out-Null
    }
}

$git = Resolve-CommandPath "git"
$commit = if ($git) { (& $git -C $repoRoot rev-parse HEAD 2>$null) } else { "unknown" }
$workingTreeDirty = if ($git) { [bool](& $git -C $repoRoot status --porcelain 2>$null) } else { $null }
$device = $null
$hostInfo = $null
if ($Platform -eq "Android") {
    $device = [ordered]@{
        manufacturer = (& $adb shell getprop ro.product.manufacturer).Trim()
        model = (& $adb shell getprop ro.product.model).Trim()
        sdk = (& $adb shell getprop ro.build.version.sdk).Trim()
        abi = (& $adb shell getprop ro.product.cpu.abi).Trim()
        hardware = (& $adb shell getprop ro.hardware).Trim()
        profile = $DeviceProfile
        batteryTemperatureStartC = $temperatureStartC
        batteryTemperatureEndC = $temperatureEndC
    }
} else {
    $hostInfo = [ordered]@{
        os = [System.Runtime.InteropServices.RuntimeInformation]::OSDescription
        architecture = [System.Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
        processor = $env:PROCESSOR_IDENTIFIER
        logicalProcessors = [Environment]::ProcessorCount
    }
}
$metadata = [ordered]@{
    schemaVersion = 1
    timestampUtc = [DateTime]::UtcNow.ToString("o")
    commit = "$commit".Trim()
    workingTreeDirty = $workingTreeDirty
    platform = $Platform
    scenarioVersion = 1
    bodyCounts = @(50, 100, 200, 500, 1000, 5000)
    joltCollisionSteps = 4
    box2dSubSteps = 4
    trials = $Trials
    quick = [bool]$Quick
    isolatedRunners = $true
    binarySizeBytes = $binarySizes
    host = $hostInfo
    device = $device
    reports = $outputs
}
$metadata | ConvertTo-Json -Depth 6 | Set-Content -Encoding utf8 (Join-Path $resolvedOutput "metadata.json")
Write-Host "Relatórios A/B gravados em $resolvedOutput"
