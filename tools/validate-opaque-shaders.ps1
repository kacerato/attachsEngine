param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$output=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$gradle=Get-Content (Join-Path $root 'android/app/build.gradle.kts') -Raw
if($gradle -notmatch 'ndkVersion\s*=\s*"([^"]+)"') { throw 'NDK pinado ausente.' }
$sdk=if($env:ANDROID_HOME){$env:ANDROID_HOME}elseif($env:ANDROID_SDK_ROOT){$env:ANDROID_SDK_ROOT}else{Join-Path $env:LOCALAPPDATA 'Android/Sdk'}
$bin=Join-Path $sdk "ndk/$($Matches[1])/shader-tools/windows-x86_64"
$results=@()
$lightingResults=@()
foreach($shader in @('dirt_road','dirt_road_fallback')) {
    $source=Join-Path $root "native/rhi/shaders/$shader.frag"
    $generic=Join-Path $output "$shader.spv"
    & "$bin/glslc.exe" -O --target-env=vulkan1.1 $source -o $generic
    if($LASTEXITCODE -ne 0) { throw "Falha de compilação: $shader" }
    foreach($mode in @(0,1)) {
        $binary=Join-Path $output "$shader-clip-$mode.spv"
        $assembly=Join-Path $output "$shader-clip-$mode.spvasm"
        & "$bin/spirv-opt.exe" --set-spec-const-default-value "3:$mode" --freeze-spec-const --fold-spec-const-op-composite -O $generic -o $binary
        if($LASTEXITCODE -ne 0) { throw 'Especialização SPIR-V falhou.' }
        & "$bin/spirv-val.exe" --target-env vulkan1.1 $binary
        if($LASTEXITCODE -ne 0) { throw 'SPIR-V inválido.' }
        & "$bin/spirv-dis.exe" $binary -o $assembly
        if($LASTEXITCODE -ne 0) { throw 'Desassemblagem falhou.' }
        $kills=@(Select-String -LiteralPath $assembly -Pattern '\bOpKill\b').Count
        # A new unguarded discard must not silently enter the solid family;
        # the generic family must retain coverage for alpha and LOD fades.
        if(($mode -eq 1 -and $kills -ne 0) -or ($mode -eq 0 -and $kills -eq 0)) {
            throw "Contrato de recorte violado: $shader, especialização $mode, OpKill=$kills"
        }
        $results += [ordered]@{shader=$shader;opaqueNoClip=($mode -eq 1);kills=$kills;bytes=(Get-Item $binary).Length;spirvValidated=$true}
    }
    $shadowSamples=@()
    foreach($point in @(0,1)) {
        $binary=Join-Path $output "$shader-point-$point.spv"
        $assembly=Join-Path $output "$shader-point-$point.spvasm"
        & "$bin/spirv-opt.exe" --set-spec-const-default-value "3:1 5:$point" --freeze-spec-const --fold-spec-const-op-composite -O $generic -o $binary
        if($LASTEXITCODE -ne 0) { throw 'Especialização pontual falhou.' }
        & "$bin/spirv-val.exe" --target-env vulkan1.1 $binary
        if($LASTEXITCODE -ne 0) { throw 'Especialização pontual inválida.' }
        & "$bin/spirv-dis.exe" $binary -o $assembly
        if($LASTEXITCODE -ne 0) { throw 'Desassemblagem pontual falhou.' }
        $samples=@(Select-String -LiteralPath $assembly -Pattern '\bOpImageSampleDref').Count
        $shadowSamples += $samples
        $lightingResults += [ordered]@{shader=$shader;unshadowedPointLighting=($point -eq 1);shadowSampleInstructions=$samples;bytes=(Get-Item $binary).Length;spirvValidated=$true}
    }
    if($shadowSamples[1] -ge $shadowSamples[0]) {
        throw 'A variante pontual ainda carrega as consultas de sombra local.'
    }
}
$results | ConvertTo-Json | Set-Content (Join-Path $output 'shader-validation.json')
$lightingResults | ConvertTo-Json | Set-Content (Join-Path $output 'lighting-validation.json')
$results | Format-Table | Out-String | Write-Output
$lightingResults | Format-Table | Out-String | Write-Output
