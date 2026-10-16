param(
    [Parameter(Mandatory=$true)][string]$CompilerDirectory,
    [Parameter(Mandatory=$true)][string]$VulkanHeaders
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$testOutput = Join-Path $repoRoot 'build/terrain-buffer-vulkan'
New-Item -ItemType Directory -Force -Path $testOutput | Out-Null
$env:PATH = $CompilerDirectory + ';' + $env:PATH
$loader = Join-Path $env:WINDIR 'System32/vulkan-1.dll'
if (!(Test-Path -LiteralPath $loader)) { throw 'Vulkan loader unavailable; this is a real GPU test, not a simulated pass.' }
$exports = & (Join-Path $CompilerDirectory 'llvm-readobj.exe') --coff-exports $loader
if ($LASTEXITCODE -ne 0) { throw 'Cannot read Vulkan loader exports.' }
$names = @($exports | ForEach-Object { if ($_ -match '^\s+Name:\s+(vk\w+)\s*$') { $Matches[1] } })
if ($names.Count -lt 20) { throw 'Incomplete Vulkan loader exports.' }
$definition = Join-Path $testOutput 'vulkan.def'
@('LIBRARY vulkan-1.dll', 'EXPORTS') + $names | Set-Content -LiteralPath $definition -Encoding ascii
$importLibrary = Join-Path $testOutput 'libvulkan-1.a'
& (Join-Path $CompilerDirectory 'llvm-dlltool.exe') -m i386:x86-64 -d $definition -l $importLibrary
if ($LASTEXITCODE -ne 0) { throw 'Cannot generate Vulkan import library.' }
$sources = @(
    'tests/native/family_base_main.cpp',
    'tests/native/test_buffer_updates.cpp',
    'tests/native/test_buffer_upload_vulkan.cpp',
    'native/rhi/memory_allocator.cpp',
    'native/rhi/memory_budget.cpp',
    'native/rhi/resource_desc.cpp',
    'native/rhi/upload_context.cpp'
) | ForEach-Object { Join-Path $repoRoot $_ }
$executable = Join-Path $testOutput 'buffer-vulkan-tests.exe'
& (Join-Path $CompilerDirectory 'clang++.exe') -std=c++20 -O1 `
    '-I' (Join-Path $repoRoot 'native') `
    '-I' (Join-Path $repoRoot 'tests/native') `
    '-I' (Join-Path $repoRoot 'native/third_party/VulkanMemoryAllocator/include') `
    '-idirafter' $VulkanHeaders @sources $importLibrary -o $executable
if ($LASTEXITCODE -ne 0) { throw 'Vulkan buffer test compilation failed.' }
& $executable
if ($LASTEXITCODE -ne 0) { throw 'Vulkan buffer regression failed.' }
