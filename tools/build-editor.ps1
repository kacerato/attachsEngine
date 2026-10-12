[CmdletBinding()]
param(
    [ValidateSet('Editor', 'Preview', 'Tests', 'AllChecks')]
    [string]$Target = 'Editor',
    [string]$BuildDirectory = 'build/editor-host',
    [ValidateRange(1, 64)]
    [int]$Jobs = 4,
    [switch]$RepairDependencies
)

$ErrorActionPreference = 'Stop'
$repository = Split-Path $PSScriptRoot -Parent
if (-not [IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory = Join-Path $repository $BuildDirectory
}
$buildPath = [IO.Path]::GetFullPath($BuildDirectory)
$cachePath = Join-Path $buildPath 'CMakeCache.txt'
if (-not (Test-Path -LiteralPath $cachePath)) {
    throw "Configure primeiro o build host em $buildPath; este comando preserva o compilador e as opcoes do cache existente."
}
# Catch the common concurrent invocation before touching Ninja's object files.
# A held file handle also serializes concurrent invocations of this script.
$buildLock = $null
try {
    $buildLock = [IO.File]::Open((Join-Path $buildPath '.astra-build.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
    $normalizedBuild = $buildPath.Replace('\', '/')
    $active = Get-CimInstance Win32_Process | Where-Object {
        $_.Name -match '^(ninja|cmake|mingw32-make)\.exe$' -and $_.CommandLine -and
        ($_.CommandLine.Replace('\', '/').Contains($normalizedBuild) -or
         ($normalizedBuild -eq (Join-Path $repository 'build/editor-host').Replace('\', '/') -and
          $_.CommandLine.Replace('\', '/').Contains('build/editor-host')))
    }
    if ($active) { throw "Ja existe um build neste diretorio (PID $($active.ProcessId -join ', ')). Aguarde terminar." }
    if ($RepairDependencies) {
        $cache = Get-Content -LiteralPath $cachePath
        if (-not ($cache -contains 'CMAKE_GENERATOR:INTERNAL=Ninja')) {
            throw 'A recuperacao de dependencias exige um build Ninja.'
        }
        $ninjaLine = $cache | Where-Object { $_ -like 'CMAKE_MAKE_PROGRAM:FILEPATH=*' } | Select-Object -First 1
        if (-not $ninjaLine) { throw 'Ninja nao encontrado no cache do CMake.' }
        $ninja = $ninjaLine.Substring($ninjaLine.IndexOf('=') + 1)
        $depsPath = Join-Path $buildPath '.ninja_deps'
        if (Test-Path -LiteralPath $depsPath) {
            $backupPath = "$depsPath.backup-$([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))"
            Copy-Item -LiteralPath $depsPath -Destination $backupPath
            Write-Host "Backup das dependencias: $backupPath"
        }
        # Ninja 1.13.2 can retain a malformed path record during ordinary
        # recovery (#2764). Recompaction rewrites the validated prefix once.
        # Invalid/missing dependencies will be rebuilt normally, never trusted.
        & $ninja -C $buildPath -t recompact
        if ($LASTEXITCODE -ne 0) { throw 'Falha ao recuperar as dependencias; backup preservado.' }
    }
    $targets = @{Editor='aether_editor'; Preview='aether_ui_preview'; Tests='aether_tests'; AllChecks='aether_host_checks'}
    $cmakeLine = Get-Content -LiteralPath $cachePath | Where-Object { $_ -like 'CMAKE_COMMAND:INTERNAL=*' } | Select-Object -First 1
    $cmake = if ($cmakeLine) { $cmakeLine.Substring($cmakeLine.IndexOf('=') + 1) } else { 'cmake' }
    Write-Host "Compilando $($targets[$Target]) com $Jobs tarefas; nenhum teste sera executado."
    & $cmake --build $buildPath --target $targets[$Target] --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw "Build falhou com codigo $LASTEXITCODE." }
} finally {
    if ($buildLock) { $buildLock.Dispose() }
}
