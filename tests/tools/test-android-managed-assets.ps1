[CmdletBinding()]
param([string]$BuildDirectory = (Join-Path $PSScriptRoot '../../android/app/build'))
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $BuildDirectory).Path
$assets = Join-Path $root 'generated/aetherAssets'
$dotnetRoot = Join-Path $assets 'dotnet'
$publishRoot = Join-Path $root 'managed/rendering-net8.0-8.0.27'
$paths = @(Get-Content -LiteralPath (Join-Path $assets 'dotnet_manifest.txt'))
$actual = [string[]]@(Get-ChildItem -LiteralPath $dotnetRoot -File -Recurse | ForEach-Object {
    [IO.Path]::GetRelativePath($dotnetRoot, $_.FullName).Replace('\', '/')
})
[Array]::Sort($actual, [StringComparer]::Ordinal)
if (($paths -join "`n") -cne ($actual -join "`n")) { throw 'Manifesto não representa todos os assets, em ordem e sem duplicação.' }
Write-Host 'PASS: manifesto completo e determinístico'
$digest = [Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA256)
try {
    foreach ($path in $paths) {
        if ($path -match '(^|/)\.\.(/|$)|^[/\\]' -or $path.Contains(':')) { throw 'Caminho inválido no manifesto.' }
        $digest.AppendData([Text.Encoding]::UTF8.GetBytes($path))
        $digest.AppendData([byte[]]@(0))
        $stream = [IO.File]::OpenRead((Join-Path $dotnetRoot $path))
        try {
            $buffer = [byte[]]::new(65536)
            while (($read = $stream.Read($buffer, 0, $buffer.Length)) -gt 0) { $digest.AppendData($buffer, 0, $read) }
        } finally { $stream.Dispose() }
    }
    $id = [BitConverter]::ToString($digest.GetHashAndReset()).Replace('-', '').ToLowerInvariant()
    if ($id -cne [IO.File]::ReadAllText((Join-Path $assets 'dotnet_build_id.txt'))) { throw 'SHA-256 dos assets difere do build ID.' }
} finally { $digest.Dispose() }
Write-Host 'PASS: build ID identifica o conteúdo completo'
foreach ($name in @('Aether.Core.dll', 'Aether.Scene.dll', 'Aether.Rendering.dll', 'Aether.Rendering.deps.json', 'Aether.Rendering.runtimeconfig.json')) {
    $generated = (Get-FileHash -LiteralPath (Join-Path $dotnetRoot $name)).Hash
    $published = (Get-FileHash -LiteralPath (Join-Path $publishRoot $name)).Hash
    if ($generated -ne $published) { throw "Asset stale: $name." }
}
Write-Host 'PASS: assembly e manifestos vêm do publish corrente'
$config = Get-Content -LiteralPath (Join-Path $dotnetRoot 'Aether.Rendering.runtimeconfig.json') -Raw | ConvertFrom-Json
if ($config.runtimeOptions.tfm -ne 'net8.0' -or
    $config.runtimeOptions.framework.name -ne 'Microsoft.NETCore.App' -or
    $config.runtimeOptions.framework.version -ne '8.0.27' -or
    $null -ne $config.runtimeOptions.PSObject.Properties['includedFrameworks']) { throw 'Publicação precisa ser framework-dependent.' }
$coreLib = Join-Path $dotnetRoot 'shared/Microsoft.NETCore.App/8.0.27/System.Private.CoreLib.dll'
if (-not (Test-Path -LiteralPath $coreLib -PathType Leaf)) {
    throw 'System.Private.CoreLib.dll do runtime Android 8.0.27 não foi empacotado.'
}
$deps = Get-Content -LiteralPath (Join-Path $dotnetRoot 'Aether.Rendering.deps.json') -Raw | ConvertFrom-Json
if ($deps.runtimeTarget.name -notmatch '/linux-bionic-arm64$') { throw 'RID gerenciado incorreto.' }
Write-Host 'PASS: net8.0 framework-dependent, CoreLib 8.0.27 e RID Android ARM64'
Write-Host '4 verificações dos assets Android passaram.'
