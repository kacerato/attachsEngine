[CmdletBinding()]
param(
    [ValidatePattern('^[a-z][a-z0-9_]*$')]
    [string]$ShaderName = "instanced",
    [string[]]$Stages = @("vert", "frag"),
    [string]$OutputHeader,
    [string]$NdkPath,
    [switch]$All,
    [switch]$Check
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# A mudança em um include GLSL (por exemplo environment_lighting.glsl) pode
# invalidar várias famílias. O modo agregado impede que o build valide apenas o
# shader default e embarque silenciosamente um SPIR-V antigo em outra pipeline.
if ($All) {
    if ($OutputHeader) { throw '-OutputHeader não pode ser usado com -All.' }
    $embeddedShaders = [ordered]@{
        astc_encode = @('comp')
        dirt_road_coverage_fallback = @('frag')
        dirt_road_coverage = @('frag')
        dirt_road_coverage_shade_fallback = @('frag')
        dirt_road_coverage_shade = @('frag')
        dirt_road_fallback = @('frag')
        dirt_road_sky = @('vert', 'frag')
        dirt_road = @('vert', 'frag')
        draw_cull = @('comp')
        hzb_reduce_first = @('vert', 'frag')
        hzb_reduce = @('vert', 'frag')
        hzb_reduce_compute = @('comp')
        instanced_fallback = @('frag')
        instanced = @('vert', 'frag')
        material_fallback = @('frag')
        material_preview = @('vert', 'frag')
        post_process = @('vert', 'frag')
        post_process_temporal = @('frag')
        runtime_hud = @('vert', 'frag')
        scene_preview = @('vert')
        shadow_depth_masked_fallback = @('frag')
        shadow_depth_masked = @('frag')
        shadow_depth = @('vert')
        triangle = @('vert', 'frag')
    }
    foreach ($entry in $embeddedShaders.GetEnumerator()) {
        $shaderParameters = @{
            ShaderName = [string]$entry.Key
            Stages = [string[]]$entry.Value
        }
        if ($NdkPath) { $shaderParameters.NdkPath = $NdkPath }
        if ($Check) { $shaderParameters.Check = $true }
        & $PSCommandPath @shaderParameters
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    exit 0
}

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$shaderDirectory = Join-Path $repositoryRoot "native\rhi\shaders"
if (-not $OutputHeader) {
    $OutputHeader = Join-Path $shaderDirectory "${ShaderName}_spirv.h"
}

if (-not $NdkPath) {
    $gradle = Get-Content -LiteralPath (Join-Path $repositoryRoot "android\app\build.gradle.kts") -Raw
    if ($gradle -notmatch 'ndkVersion\s*=\s*"([^"]+)"') {
        throw "ndkVersion não encontrado em android/app/build.gradle.kts."
    }
    $sdkRoot = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } elseif ($env:ANDROID_SDK_ROOT) {
        $env:ANDROID_SDK_ROOT
    } else {
        Join-Path $env:LOCALAPPDATA "Android\Sdk"
    }
    $NdkPath = Join-Path $sdkRoot "ndk\$($Matches[1])"
}

$shaderTools = Join-Path $NdkPath "shader-tools\windows-x86_64"
$glslc = Join-Path $shaderTools "glslc.exe"
$spirvVal = Join-Path $shaderTools "spirv-val.exe"
foreach ($tool in @($glslc, $spirvVal)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "Ferramenta ausente: $tool" }
}

$temporaryDirectory = Join-Path ([IO.Path]::GetTempPath()) ("aether-shaders-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null

function Convert-SpirvToArray {
    param([Parameter(Mandatory = $true)][string]$Path)
    $bytes = [IO.File]::ReadAllBytes($Path)
    if (($bytes.Length % 4) -ne 0) { throw "SPIR-V '$Path' não está alinhado em palavras de 32 bits." }
    $words = [System.Collections.Generic.List[string]]::new()
    for ($offset = 0; $offset -lt $bytes.Length; $offset += 4) {
        $word = [BitConverter]::ToUInt32($bytes, $offset)
        $words.Add(('0x{0:x8}' -f $word))
    }
    $lines = [System.Collections.Generic.List[string]]::new()
    for ($index = 0; $index -lt $words.Count; $index += 8) {
        $end = [Math]::Min($index + 7, $words.Count - 1)
        $lines.Add((($words[$index..$end] -join ',') + ','))
    }
    return [ordered]@{ Lines = $lines; ByteCount = $bytes.Length }
}

try {
    $compiled = [ordered]@{}
    foreach ($stage in $Stages) {
        $source = Join-Path $shaderDirectory "$ShaderName.$stage"
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Shader ausente: $source" }
        $binary = Join-Path $temporaryDirectory "$ShaderName.$stage.spv"
        & $glslc -O --target-env=vulkan1.1 $source -o $binary
        if ($LASTEXITCODE -ne 0) { throw "glslc falhou para $source." }
        & $spirvVal --target-env vulkan1.1 $binary
        if ($LASTEXITCODE -ne 0) { throw "spirv-val falhou para $binary." }
        $compiled[$stage] = Convert-SpirvToArray -Path $binary
    }

    $prefix = [Globalization.CultureInfo]::InvariantCulture.TextInfo.ToTitleCase($ShaderName)
    $header = [System.Collections.Generic.List[string]]::new()
    $header.Add("// GERADO por tools/generate-embedded-shaders.ps1 com glslc -O (NDK pinado).")
    $header.Add("// Não editar este arquivo manualmente; altere os GLSL e regenere.")
    $header.Add("#pragma once")
    $header.Add("")
    $header.Add("#include <cstdint>")
    $header.Add("")
    $header.Add("namespace ae::rhi::shaders {")
    $header.Add("")
    foreach ($stage in $Stages) {
        $stageTitle = [Globalization.CultureInfo]::InvariantCulture.TextInfo.ToTitleCase($stage)
        $header.Add("inline constexpr uint32_t k${prefix}${stageTitle}Spirv[] = {")
        foreach ($line in $compiled[$stage].Lines) { $header.Add($line) }
        $header.Add("};")
        $header.Add("inline constexpr uint32_t k${prefix}${stageTitle}SpirvSize = sizeof(k${prefix}${stageTitle}Spirv);")
        $header.Add("")
    }
    $header.Add("} // namespace ae::rhi::shaders")
    if ($Check) {
        $existing = (Get-Content -LiteralPath $OutputHeader -Raw).Replace("`r`n", "`n")
        $generated = ($header -join "`n") + "`n"
        if ($existing -cne $generated) { throw "Header desatualizado: regenere $ShaderName antes do build." }
        Write-Output "SPIR-V validado; header $ShaderName reproduzível."
    } else {
        [IO.File]::WriteAllLines([IO.Path]::GetFullPath($OutputHeader), $header, [Text.UTF8Encoding]::new($false))
        Write-Output "SPIR-V validado e gerado em $([IO.Path]::GetFullPath($OutputHeader))."
    }
}
finally {
    if (Test-Path -LiteralPath $temporaryDirectory) {
        $resolvedTemporary = (Resolve-Path -LiteralPath $temporaryDirectory).Path
        $expectedParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
        if ([IO.Path]::GetDirectoryName($resolvedTemporary) -ne $expectedParent -or
            [IO.Path]::GetFileName($resolvedTemporary) -notmatch '^aether-shaders-[a-f0-9]{32}$') {
            throw "Limpeza recusada fora do diretório temporário esperado: $resolvedTemporary"
        }
        Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
    }
}
