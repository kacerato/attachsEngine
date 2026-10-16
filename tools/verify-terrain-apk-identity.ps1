[CmdletBinding()]
param(
    [string]$Apk = '',
    [string]$ExpectedVersionName = '',
    [Parameter(Mandatory=$true)][int]$ExpectedVersionCode,
    [string]$ExpectedPackage = 'dev.aether.editor.terrain',
    [string]$ExpectedLabel = 'Astra Terreno',
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')]
    [string]$OutputPrefix = 'terrain'
)
$ErrorActionPreference = 'Stop'
$terrainAuditRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $Apk) {
    $Apk = Join-Path $terrainAuditRoot 'android/app/build/outputs/apk/release/app-release.apk'
}
$terrainApkPath = (Resolve-Path -LiteralPath $Apk).Path
$terrainBuildTools = Join-Path $env:LOCALAPPDATA 'Android/Sdk/build-tools/35.0.0'
$terrainEvidence = Join-Path $terrainAuditRoot 'build/terrain-evidence'
New-Item -ItemType Directory -Force -Path $terrainEvidence | Out-Null

$terrainSignature = & (Join-Path $terrainBuildTools 'apksigner.bat') verify --verbose --print-certs $terrainApkPath 2>&1
if ($LASTEXITCODE -ne 0) { throw 'APK signature verification failed.' }
$terrainSignatureText = $terrainSignature -join "`n"
$terrainCertificateMatch = [regex]::Match($terrainSignatureText, 'Signer #1 certificate SHA-256 digest:\s*([0-9a-fA-F:]+)')
if (-not $terrainCertificateMatch.Success) { throw 'Signer certificate SHA-256 was not reported.' }
$terrainCertificate = $terrainCertificateMatch.Groups[1].Value.Replace(':', '').ToLowerInvariant()
$terrainCode75Certificate = '68a37f8803e5f27155676328e8dc97385b41930bd878ad4168b1c962026599be'
if ($terrainCertificate -ne $terrainCode75Certificate) { throw 'Signer differs from the previously generated code 75 APK.' }
if ($terrainSignatureText -notmatch 'Verified using v2 scheme[^\r\n]*:\s*true') { throw 'APK Signature Scheme v2 was not verified.' }

$terrainBadging = & (Join-Path $terrainBuildTools 'aapt2.exe') dump badging $terrainApkPath 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect APK manifest.' }
$terrainBadgingText = $terrainBadging -join "`n"
$terrainPackageMatch = [regex]::Match($terrainBadgingText, "package: name='([^']+)' versionCode='([^']+)' versionName='([^']+)'")
if (-not $terrainPackageMatch.Success) { throw 'Package identity missing from APK manifest.' }
$terrainPackage = $terrainPackageMatch.Groups[1].Value
$terrainCode = [int]$terrainPackageMatch.Groups[2].Value
$terrainVersion = $terrainPackageMatch.Groups[3].Value
if ($terrainPackage -ne $ExpectedPackage -or $terrainCode -ne $ExpectedVersionCode) {
    throw "Unexpected package/versionCode: $terrainPackage / $terrainCode"
}
if ($ExpectedVersionName -and $terrainVersion -ne $ExpectedVersionName) { throw 'Unexpected APK versionName.' }
if ($terrainBadgingText -notmatch ("application-label:'" + [regex]::Escape($ExpectedLabel) + "'")) { throw 'Expected application label missing.' }
if ($terrainBadgingText -notmatch "(?m)^native-code: 'arm64-v8a'\s*$") { throw 'Expected an ARM64-only APK.' }
if ($terrainBadgingText -match '(?m)^application-debuggable') { throw 'Release APK is debuggable.' }

$terrainAlignment = & (Join-Path $terrainBuildTools 'zipalign.exe') -c -P 16 -v 4 $terrainApkPath 2>&1
if ($LASTEXITCODE -ne 0) { throw 'APK ZIP alignment check failed.' }
$terrainSignatureText | Set-Content -LiteralPath (Join-Path $terrainEvidence "${OutputPrefix}-apksigner.txt") -Encoding utf8
$terrainBadgingText | Set-Content -LiteralPath (Join-Path $terrainEvidence "${OutputPrefix}-aapt-badging.txt") -Encoding utf8
$terrainAlignment | Set-Content -LiteralPath (Join-Path $terrainEvidence "${OutputPrefix}-zipalign.txt") -Encoding utf8
$terrainIdentity = [ordered]@{
    apk = $terrainApkPath
    sha256 = (Get-FileHash -LiteralPath $terrainApkPath -Algorithm SHA256).Hash.ToLowerInvariant()
    package = $terrainPackage
    versionCode = $terrainCode
    versionName = $terrainVersion
    label = $ExpectedLabel
    abi = 'arm64-v8a'
    debuggable = $false
    signature_v2_verified = $true
    certificate_sha256 = $terrainCertificate
    same_certificate_as_generated_code75 = $true
    zipalign_verified = $true
    elf_page_alignment = "See ${OutputPrefix}-apk-audit.json; ZIP alignment alone does not establish ELF 16 KB compatibility."
    runtime_verified = $false
}
$terrainIdentityJson = $terrainIdentity | ConvertTo-Json -Depth 4
$terrainIdentityJson | Set-Content -LiteralPath (Join-Path $terrainEvidence "${OutputPrefix}-apk-identity.json") -Encoding utf8
$terrainIdentityJson
