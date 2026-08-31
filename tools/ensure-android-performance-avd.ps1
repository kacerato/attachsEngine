[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string]$ProfilePath = (Join-Path $PSScriptRoot 'android-avd\Aether-C-Synthetic.json'),
    [string]$SdkPath,
    [switch]$Recreate,
    [switch]$Launch,
    [switch]$SoftwareRenderer,
    [switch]$PlanOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-AndroidSdkPath {
    param([string]$ExplicitPath)
    if ($ExplicitPath) { return [IO.Path]::GetFullPath($ExplicitPath) }
    if ($env:ANDROID_HOME) { return [IO.Path]::GetFullPath($env:ANDROID_HOME) }
    if ($env:ANDROID_SDK_ROOT) { return [IO.Path]::GetFullPath($env:ANDROID_SDK_ROOT) }
    return [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'Android\Sdk'))
}

function Resolve-AvdHome {
    if ($env:ANDROID_AVD_HOME) { return [IO.Path]::GetFullPath($env:ANDROID_AVD_HOME) }
    if ($env:ANDROID_USER_HOME) { return [IO.Path]::GetFullPath((Join-Path $env:ANDROID_USER_HOME 'avd')) }
    return [IO.Path]::GetFullPath((Join-Path $env:USERPROFILE '.android\avd'))
}

function Assert-SyntheticProfile {
    param([object]$Profile)
    if ($Profile.schemaVersion -ne 1 -or
        $Profile.profileId -notmatch '^aether-[a-z0-9-]+-v[0-9]+$' -or
        $Profile.name -notmatch '^Aether-[A-Za-z0-9-]+$' -or
        $Profile.systemImage -notmatch '^system-images;android-[0-9]+;[a-z0-9_]+;(?:arm64-v8a|x86_64)$' -or
        $Profile.device -notmatch '^[a-z0-9_]+$' -or
        $Profile.performanceCertification -ne $false -or
        $Profile.purpose -cne 'correctness-fallback-lifecycle-memory-only') {
        throw 'Perfil AVD inválido ou tentando certificar desempenho físico.'
    }
    $required = @('aether.profile.id', 'hw.cpu.ncore', 'hw.ramSize', 'hw.lcd.width',
        'hw.lcd.height', 'hw.lcd.density', 'hw.lcd.vsync', 'hw.gpu.enabled', 'hw.gpu.mode')
    foreach ($key in $required) {
        if ($null -eq $Profile.config.$key -or [string]::IsNullOrWhiteSpace([string]$Profile.config.$key)) {
            throw "Perfil AVD sem configuração obrigatória '$key'."
        }
    }
    if ($Profile.config.'aether.profile.id' -cne $Profile.profileId) {
        throw 'Marcador do config.ini diverge do profileId.'
    }
}

function Set-AvdConfigValues {
    param([string]$Path, [object]$Values)
    $entries = [ordered]@{}
    foreach ($line in [IO.File]::ReadAllLines($Path)) {
        if ($line -match '^([^=]+)=(.*)$') { $entries[$Matches[1]] = $Matches[2] }
    }
    foreach ($property in $Values.PSObject.Properties) { $entries[$property.Name] = [string]$property.Value }
    $lines = @($entries.GetEnumerator() | Sort-Object Key | ForEach-Object { "$($_.Key)=$($_.Value)" })
    [IO.File]::WriteAllLines($Path, $lines, [Text.UTF8Encoding]::new($false))
}

$resolvedProfilePath = [IO.Path]::GetFullPath($ProfilePath)
if (-not (Test-Path -LiteralPath $resolvedProfilePath -PathType Leaf)) {
    throw "Perfil AVD ausente: $resolvedProfilePath"
}
$profile = Get-Content -LiteralPath $resolvedProfilePath -Raw | ConvertFrom-Json
Assert-SyntheticProfile -Profile $profile

$resolvedSdkPath = Resolve-AndroidSdkPath -ExplicitPath $SdkPath
$avdHome = Resolve-AvdHome
$systemImageParts = @($profile.systemImage -split ';')
$systemImageAbi = $systemImageParts[-1]
$hostArchitecture = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
$cpuAccelerationExpected = ($systemImageAbi -eq 'x86_64' -and $hostArchitecture -eq 'X64') -or
                           ($systemImageAbi -eq 'arm64-v8a' -and $hostArchitecture -eq 'Arm64')
$plan = [ordered]@{
    schemaVersion = 1
    profileId = $profile.profileId
    name = $profile.name
    systemImage = $profile.systemImage
    abi = $systemImageAbi
    device = $profile.device
    avdHome = $avdHome
    sdkPath = $resolvedSdkPath
    renderer = if ($SoftwareRenderer) { 'swiftshader_indirect' } else { 'auto' }
    hostArchitecture = $hostArchitecture
    cpuAccelerationExpected = $cpuAccelerationExpected
    purpose = $profile.purpose
    performanceCertification = $false
    config = $profile.config
}
if ($PlanOnly) {
    $plan | ConvertTo-Json -Depth 5
    return
}

$avdManager = Join-Path $resolvedSdkPath 'cmdline-tools\latest\bin\avdmanager.bat'
$emulator = Join-Path $resolvedSdkPath 'emulator\emulator.exe'
foreach ($tool in @($avdManager, $emulator)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "Ferramenta Android ausente: $tool" }
}
$imagePath = Join-Path $resolvedSdkPath ($systemImageParts -join '\')
if (-not (Test-Path -LiteralPath $imagePath -PathType Container)) {
    throw "Imagem AVD ausente: $($profile.systemImage). Instale-a pelo SDK Manager do Android Studio."
}
if (-not $cpuAccelerationExpected) {
    Write-Warning "ABI $systemImageAbi difere do host $hostArchitecture; o AVD pode usar emulação de CPU lenta ou não suportada. O APK atual é ARM64 e não pode ser validado num AVD x86_64."
}

$avdDirectory = Join-Path $avdHome "$($profile.name).avd"
$configPath = Join-Path $avdDirectory 'config.ini'
if (Test-Path -LiteralPath $avdDirectory -PathType Container) {
    if ($Recreate) {
        if ($PSCmdlet.ShouldProcess($profile.name, 'Excluir e recriar AVD sintético')) {
            & $avdManager delete avd --name $profile.name
            if ($LASTEXITCODE -ne 0) { throw "Falha ao excluir AVD '$($profile.name)'." }
        }
    } elseif (-not (Test-Path -LiteralPath $configPath -PathType Leaf) -or
              (Get-Content -LiteralPath $configPath -Raw) -notmatch "(?m)^aether\.profile\.id=$([regex]::Escape($profile.profileId))$") {
        throw "AVD '$($profile.name)' já existe, mas não pertence a este perfil. Use outro nome ou remova-o manualmente."
    }
}

if (-not (Test-Path -LiteralPath $avdDirectory -PathType Container)) {
    if ($PSCmdlet.ShouldProcess($profile.name, 'Criar AVD sintético')) {
        'no' | & $avdManager create avd --name $profile.name --package $profile.systemImage --device $profile.device
        if ($LASTEXITCODE -ne 0) { throw "Falha ao criar AVD '$($profile.name)'." }
    }
}
if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
    throw "config.ini do AVD não foi criado: $configPath"
}
if ($PSCmdlet.ShouldProcess($configPath, 'Aplicar configuração Aether perfil C sintético')) {
    Set-AvdConfigValues -Path $configPath -Values $profile.config
}

$plan | ConvertTo-Json -Depth 5
if ($Launch -and $PSCmdlet.ShouldProcess($profile.name, 'Abrir Android Emulator')) {
    $gpuMode = if ($SoftwareRenderer) { 'swiftshader_indirect' } else { 'auto' }
    Start-Process -FilePath $emulator -ArgumentList @('-avd', $profile.name, '-gpu', $gpuMode, '-no-snapshot-load')
}
