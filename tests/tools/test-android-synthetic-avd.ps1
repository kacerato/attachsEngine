$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$script = Join-Path $root 'tools\ensure-android-performance-avd.ps1'
$plan = (& $script -PlanOnly | Out-String) | ConvertFrom-Json

function Assert-Equal($actual, $expected, [string]$message) {
    if ($actual -cne $expected) { throw "$message (esperado='$expected', atual='$actual')" }
}

Assert-Equal $plan.schemaVersion 1 'schema do plano AVD'
Assert-Equal $plan.name 'Aether-C-Synthetic' 'nome versionado'
Assert-Equal $plan.systemImage 'system-images;android-35;google_apis;arm64-v8a' 'imagem pinada na ABI do APK'
Assert-Equal $plan.abi 'arm64-v8a' 'ABI do AVD'
Assert-Equal $plan.config.'hw.cpu.ncore' '2' 'núcleos sintéticos'
Assert-Equal $plan.config.'hw.ramSize' '4096' 'RAM sintética'
Assert-Equal $plan.config.'hw.lcd.width' '1080' 'largura sintética'
Assert-Equal $plan.config.'hw.lcd.height' '2400' 'altura sintética'
Assert-Equal $plan.renderer 'auto' 'renderer acelerado padrão'
Assert-Equal $plan.performanceCertification $false 'AVD não certifica FPS físico'
$hostCanAccelerateArm64 = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString() -eq 'Arm64'
Assert-Equal $plan.cpuAccelerationExpected $hostCanAccelerateArm64 'aceleração respeita ABI do host'

$softwarePlan = (& $script -PlanOnly -SoftwareRenderer | Out-String) | ConvertFrom-Json
Assert-Equal $softwarePlan.renderer 'swiftshader_indirect' 'fallback de software explícito'

Write-Output '2/2 planos AVD sintéticos passaram.'
