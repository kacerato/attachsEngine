param([switch]$Release)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
python "$PSScriptRoot/prepare-godot-editor.py"
if ($LASTEXITCODE -ne 0) { throw 'Falha ao preparar Godot' }
$configuration = if ($Release) { 'release' } else { 'debug' }
$task = if ($Release) { ':editor:assembleAndroidRelease' } else { ':editor:assembleAndroidDebug' }
Push-Location "$projectRoot/external/godot/platform/android/java"
try {
    & ./gradlew.bat $task
    if ($LASTEXITCODE -ne 0) { throw 'Falha na compilação do editor' }
    $outputDirectory = "$projectRoot/build/godot-editor"
    New-Item -ItemType Directory -Force $outputDirectory | Out-Null
    Copy-Item -LiteralPath "editor/build/outputs/apk/android/$configuration/android_editor-android-$configuration.apk" -Destination "$outputDirectory/Astra-Godot-$configuration.apk"
    Write-Output "APK: $outputDirectory/Astra-Godot-$configuration.apk"
} finally { Pop-Location }
