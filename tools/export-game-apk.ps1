[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$TemplateApk,
    [Parameter(Mandatory)][string]$ProjectDirectory,
    [Parameter(Mandatory)][string]$OutputApk,
    [Parameter(Mandatory)][string]$ApplicationId,
    [Parameter(Mandatory)][string]$GameName,
    [Parameter(Mandatory)][string]$KeyStore,
    [string]$Alias = 'game',
    [ValidateSet('PKCS12','JKS')][string]$StoreType = 'PKCS12',
    [ValidateRange(1,2147483647)][int]$VersionCode = 1,
    [string]$VersionName = '1.0'
)
$ErrorActionPreference = 'Stop'
if (-not $env:ASTRA_STORE_PASSWORD -or -not $env:ASTRA_KEY_PASSWORD) {
    throw 'Defina ASTRA_STORE_PASSWORD e ASTRA_KEY_PASSWORD no ambiente; senhas nao entram nos argumentos nem no arquivo de configuracao.'
}
$exportSettings = [ordered]@{
    template=(Resolve-Path -LiteralPath $TemplateApk).Path
    project=(Resolve-Path -LiteralPath $ProjectDirectory).Path
    output=[IO.Path]::GetFullPath($OutputApk)
    applicationId=$ApplicationId; label=$GameName
    versionCode=$VersionCode; versionName=$VersionName
    keystore=(Resolve-Path -LiteralPath $KeyStore).Path
    storeType=$StoreType; alias=$Alias
}
$exportConfig = Join-Path ([IO.Path]::GetTempPath()) ('astra-export-'+[guid]::NewGuid().ToString('N')+'.properties')
try {
    $lines = foreach ($entry in $exportSettings.GetEnumerator()) {
        $value = ([string]$entry.Value).Replace('\','\\').Replace("`r",'\r').Replace("`n",'\n')
        $entry.Key+'='+$value
    }
    [IO.File]::WriteAllLines($exportConfig,$lines,[Text.UTF8Encoding]::new($false))
    Push-Location (Join-Path $PSScriptRoot '../android')
    try {
        & ./gradlew.bat :export-core:exportGame "-PastraExportConfig=$exportConfig" --console=plain --max-workers=2
        if ($LASTEXITCODE -ne 0) {throw "Exportacao recusada (codigo $LASTEXITCODE)."}
    } finally {Pop-Location}
    Get-FileHash -LiteralPath $exportSettings.output -Algorithm SHA256
} finally {
    if (Test-Path -LiteralPath $exportConfig) {Remove-Item -LiteralPath $exportConfig}
}
