[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$NdkRoot,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot "..\build\vulkan-headers")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$resolvedNdk = [IO.Path]::GetFullPath($NdkRoot)
$sourceRoot = Join-Path $resolvedNdk "toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\include"
$vulkanSource = Join-Path $sourceRoot "vulkan"
$videoSource = Join-Path $sourceRoot "vk_video"
if (-not (Test-Path -LiteralPath (Join-Path $vulkanSource "vulkan.h") -PathType Leaf)) {
    throw "O NDK '$resolvedNdk' não contém os headers Vulkan esperados em '$vulkanSource'."
}
if (-not (Test-Path -LiteralPath $videoSource -PathType Container)) {
    throw "O NDK '$resolvedNdk' não contém os headers auxiliares vk_video em '$videoSource'."
}

$resolvedOutput = [IO.Path]::GetFullPath($OutputDirectory)
$vulkanDestination = Join-Path $resolvedOutput "vulkan"
$videoDestination = Join-Path $resolvedOutput "vk_video"
New-Item -ItemType Directory -Path $vulkanDestination -Force | Out-Null
New-Item -ItemType Directory -Path $videoDestination -Force | Out-Null
Copy-Item -Path (Join-Path $vulkanSource "*") -Destination $vulkanDestination -Recurse -Force
Copy-Item -Path (Join-Path $videoSource "*") -Destination $videoDestination -Recurse -Force

if (-not (Test-Path -LiteralPath (Join-Path $vulkanDestination "vulkan.h") -PathType Leaf)) {
    throw "A cópia isolada dos headers Vulkan não foi produzida em '$vulkanDestination'."
}

Write-Output $resolvedOutput
