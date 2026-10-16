# Compila o APK público da Astra (pacote dev.aether.editor) com a chave de
# distribuição, sem exibir nem gravar a senha.
#
# Uso (PowerShell, na raiz do repositório):
#   tools/release/build-public-apk.ps1 -VersionCode 29 -VersionName 0.3.1-preview.20261016
#
# A chave vive fora do repositório em ~/.astra/signing (astra-distribution.p12 e
# password.dpapi, criptografada pelo DPAPI do usuário Windows; ver README lá).
# As variáveis de ambiente usadas pelo build.gradle.kts são definidas só neste
# processo e apagadas no fim. Nada aqui imprime a senha.
param(
  [Parameter(Mandatory = $true)][int]$VersionCode,
  [Parameter(Mandatory = $true)][string]$VersionName,
  [string]$SigningDir = (Join-Path $HOME '.astra/signing'),
  [string]$Alias = 'astra-distribution'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$keystore = Join-Path $SigningDir 'astra-distribution.p12'
$passwordFile = Join-Path $SigningDir 'password.dpapi'
if (-not (Test-Path $keystore)) { throw "Keystore ausente: $keystore" }
if (-not (Test-Path $passwordFile)) { throw "Senha DPAPI ausente: $passwordFile" }
$secure = Get-Content -LiteralPath $passwordFile -Raw | ForEach-Object { $_.Trim() } | ConvertTo-SecureString
$bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
try {
  $env:ASTRA_KEYSTORE = $keystore
  $env:ASTRA_KEYSTORE_PASSWORD = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
  $env:ASTRA_KEY_PASSWORD = $env:ASTRA_KEYSTORE_PASSWORD
  $env:ASTRA_KEY_ALIAS = $Alias
  Push-Location (Join-Path $root 'android')
  try {
    & ./gradlew.bat ':app:assembleRelease' '-PastraApplicationId=dev.aether.editor' "-PastraVersionCode=$VersionCode" "-PastraVersionName=$VersionName" '-q'
    if ($LASTEXITCODE -ne 0) { throw "Gradle exit code $LASTEXITCODE" }
  } finally { Pop-Location }
  $apk = Join-Path $root 'android/app/build/outputs/apk/release/app-release.apk'
  $out = Join-Path $root ("build/public-apk/Astra-$VersionName-android-arm64.apk")
  New-Item -ItemType Directory -Force (Split-Path -Parent $out) | Out-Null
  Copy-Item -LiteralPath $apk -Destination $out -Force
  $hash = (Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash.ToLowerInvariant()
  "$hash  $(Split-Path -Leaf $out)" | Set-Content -LiteralPath (Join-Path (Split-Path -Parent $out) 'SHA256SUMS.txt') -Encoding ascii
  Write-Host "APK público: $out"
  Write-Host "SHA-256: $hash"
  Write-Host ("Tamanho: {0} bytes" -f (Get-Item -LiteralPath $out).Length)
} finally {
  [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
  Remove-Item Env:ASTRA_KEYSTORE_PASSWORD -ErrorAction SilentlyContinue
  Remove-Item Env:ASTRA_KEY_PASSWORD -ErrorAction SilentlyContinue
  Remove-Item Env:ASTRA_KEYSTORE -ErrorAction SilentlyContinue
  Remove-Item Env:ASTRA_KEY_ALIAS -ErrorAction SilentlyContinue
}
