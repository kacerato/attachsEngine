param(
  [string]$BuildDirectory = 'build/editor-host',
  [string]$EvidenceDirectory = 'docs/validacao/n01-n07-2026-10-10/host',
  [string]$PluginDirectory = 'C:/Users/donod/Downloads/analise-plugins',
  [string]$PolygonDirectory = 'local-assets/synty-polygon-icons-v1.01'
)

# Runs the focused package once against already-built binaries. No Android/ADB.
# A successful build is a prerequisite, never inferred from an old executable.
$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $taskRoot
$taskNames = @('AE_CLIP_CAPTURE_DIR','ASTRA_TGA_SOURCE_ROOT','ASTRA_TIFF_SOURCE_ROOT',
  'ASTRA_FBX_SOURCE_ROOT','ASTRA_FBX_DEFORMATION_ROOT','ASTRA_TGA_CAPTURE',
  'ASTRA_TIFF_CAPTURE','ASTRA_FBX_CAPTURE','ASTRA_UNITY_LIBRARY_CAPTURE',
  'ASTRA_AUTHORING_SDK_BRIDGE','AETHER_REQUIRE_NATIVE')
$taskEnvironment = @{}
$taskResults = [Collections.Generic.List[object]]::new()
foreach($name in $taskNames) { $taskEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
try {
  $taskBuild = (Resolve-Path $BuildDirectory).Path
  $taskEvidence = [IO.Path]::GetFullPath((Join-Path $taskRoot $EvidenceDirectory))
  $taskCaptures = Join-Path $taskEvidence 'capturas'
  [IO.Directory]::CreateDirectory($taskCaptures) | Out-Null
  $taskBridge = Join-Path $taskBuild 'libaether_animation_authoring_sdk_bridge.dll'
  foreach($file in @('aether_animation_clip_editor_tests.exe','aether_unity_material_tests.exe',
      'aether_gui_core_tests.exe','libaether_animation_authoring_sdk_bridge.dll')) {
    if(!(Test-Path -LiteralPath (Join-Path $taskBuild $file) -PathType Leaf)) {throw "Build artifact absent: $file"}
  }
  $env:AE_CLIP_CAPTURE_DIR = $taskCaptures
  $env:ASTRA_TGA_SOURCE_ROOT = (Resolve-Path $PluginDirectory).Path
  $env:ASTRA_TIFF_SOURCE_ROOT = $env:ASTRA_TGA_SOURCE_ROOT
  $env:ASTRA_FBX_DEFORMATION_ROOT = $env:ASTRA_TGA_SOURCE_ROOT
  $env:ASTRA_FBX_SOURCE_ROOT = (Resolve-Path $PolygonDirectory).Path
  $env:ASTRA_TGA_CAPTURE = Join-Path $taskCaptures 'tga-focal-655'
  $env:ASTRA_TIFF_CAPTURE = Join-Path $taskCaptures 'tiff-focal-655'
  $env:ASTRA_FBX_CAPTURE = Join-Path $taskCaptures 'fbx-focal-655'
  $env:ASTRA_UNITY_LIBRARY_CAPTURE = Join-Path $taskCaptures 'unity-library-655.ppm'
  $env:ASTRA_AUTHORING_SDK_BRIDGE = $taskBridge
  $env:AETHER_REQUIRE_NATIVE = '1'
  function Invoke-PackageAcceptance([string]$Name,[string]$Command,[string[]]$Arguments) {
    $taskLog = Join-Path $taskEvidence ($Name+'-integrated.log')
    $taskStarted = [DateTime]::UtcNow
    & $Command @Arguments 2>&1 | Tee-Object -FilePath $taskLog
    $taskExit = $LASTEXITCODE
    $taskResults.Add([ordered]@{name=$Name;exitCode=$taskExit;startedUtc=$taskStarted.ToString('o');
      endedUtc=[DateTime]::UtcNow.ToString('o');log=$taskLog;
      binarySha256=if(Test-Path -LiteralPath $Command -PathType Leaf){(Get-FileHash -LiteralPath $Command -Algorithm SHA256).Hash}else{$null}})
  }
  Invoke-PackageAcceptance 'editor' (Join-Path $taskBuild 'aether_animation_clip_editor_tests.exe') @()
  Invoke-PackageAcceptance 'library' (Join-Path $taskBuild 'aether_unity_material_tests.exe') @()
  Invoke-PackageAcceptance 'gui' (Join-Path $taskBuild 'aether_gui_core_tests.exe') @()
  Invoke-PackageAcceptance 'sdk' 'dotnet' @('run','--project','tests/Aether.Tests/Aether.Tests.csproj',
    '--no-build','--','AnimationAuthoringSdkTests')
  $taskManifest = [ordered]@{schemaVersion=1;platform='Windows host';androidAccepted=$false;
    scope='Focused authoring/library acceptance; captures are CPU controls, not Android/Vulkan proof';
    sdkBridgeSha256=(Get-FileHash -LiteralPath $taskBridge -Algorithm SHA256).Hash;results=$taskResults.ToArray()}
  $taskManifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $taskEvidence 'authoring-library-integrated.json') -Encoding utf8
  if(@($taskResults | Where-Object exitCode -ne 0).Count) {exit 1}
}
finally {
  foreach($name in $taskNames) { [Environment]::SetEnvironmentVariable($name,$taskEnvironment[$name],'Process') }
  Pop-Location
}
