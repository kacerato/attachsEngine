[CmdletBinding()]
param(
    [string]$BudgetPath = (Join-Path $PSScriptRoot "..\metrics\budgets.v1.json"),
    [string]$MeasurementPath,
    [string]$OutputPath,
    [switch]$RequireAllMetrics
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Read-JsonFile {
    param([Parameter(Mandatory = $true)][string]$Path, [string]$Description)
    $resolved = [IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
        throw "$Description não encontrado em '$resolved'."
    }
    try {
        return Get-Content -LiteralPath $resolved -Raw -Encoding UTF8 | ConvertFrom-Json
    }
    catch {
        throw "$Description não contém JSON válido: $($_.Exception.Message)"
    }
}

function Require-Property {
    param([object]$Object, [string]$Name, [string]$Context)
    if ($null -eq $Object.PSObject.Properties[$Name]) {
        throw "$Context não possui a propriedade obrigatória '$Name'."
    }
    return $Object.$Name
}

function Assert-FiniteNumber {
    param([object]$Value, [string]$Context)
    if ($Value -isnot [byte] -and $Value -isnot [int16] -and $Value -isnot [int32] -and
        $Value -isnot [int64] -and $Value -isnot [single] -and $Value -isnot [double] -and
        $Value -isnot [decimal]) {
        throw "$Context precisa ser numérico."
    }
    $number = [double]$Value
    if ([double]::IsNaN($number) -or [double]::IsInfinity($number)) {
        throw "$Context precisa ser finito."
    }
    return $number
}

$budget = Read-JsonFile -Path $BudgetPath -Description "Registro de orçamentos"
if ((Require-Property $budget "schemaVersion" "Registro de orçamentos") -ne 1) {
    throw "schemaVersion do registro de orçamentos não suportada; esperado 1."
}
$revision = [string](Require-Property $budget "budgetRevision" "Registro de orçamentos")
if ([string]::IsNullOrWhiteSpace($revision)) { throw "budgetRevision não pode ser vazia." }
$requiredCategories = @(Require-Property $budget "requiredCategories" "Registro de orçamentos")
$budgetMetrics = @(Require-Property $budget "metrics" "Registro de orçamentos")
if ($budgetMetrics.Count -eq 0) { throw "O registro precisa conter pelo menos uma métrica." }

$budgetsById = @{}
$categories = @{}
foreach ($metric in $budgetMetrics) {
    $id = [string](Require-Property $metric "id" "Métrica de orçamento")
    if ([string]::IsNullOrWhiteSpace($id)) { throw "ID de métrica não pode ser vazio." }
    if ($budgetsById.ContainsKey($id)) { throw "ID de métrica duplicado: '$id'." }

    $category = [string](Require-Property $metric "category" "Métrica '$id'")
    $unit = [string](Require-Property $metric "unit" "Métrica '$id'")
    $comparison = [string](Require-Property $metric "comparison" "Métrica '$id'")
    $target = Assert-FiniteNumber (Require-Property $metric "target" "Métrica '$id'") "target de '$id'"
    $failureThreshold = Assert-FiniteNumber (Require-Property $metric "failureThreshold" "Métrica '$id'") "failureThreshold de '$id'"
    $null = Require-Property $metric "scope" "Métrica '$id'"
    $null = Require-Property $metric "collector" "Métrica '$id'"
    $enforcement = [string](Require-Property $metric "enforcement" "Métrica '$id'")

    if ([string]::IsNullOrWhiteSpace($category) -or [string]::IsNullOrWhiteSpace($unit)) {
        throw "Categoria e unidade de '$id' não podem ser vazias."
    }
    if ($comparison -notin @("maximum", "minimum")) {
        throw "Comparação de '$id' deve ser 'maximum' ou 'minimum'."
    }
    if ($enforcement -notin @("host-required", "device-required", "advisory")) {
        throw "Enforcement de '$id' não é reconhecido: '$enforcement'."
    }
    if ($comparison -eq "maximum" -and $failureThreshold -lt $target) {
        throw "Limite de falha de '$id' não pode ser menor que o alvo para comparação maximum."
    }
    if ($comparison -eq "minimum" -and $failureThreshold -gt $target) {
        throw "Limite de falha de '$id' não pode ser maior que o alvo para comparação minimum."
    }

    $budgetsById[$id] = $metric
    $categories[$category] = $true
}

foreach ($requiredCategory in $requiredCategories) {
    if (-not $categories.ContainsKey([string]$requiredCategory)) {
        throw "Categoria obrigatória sem métrica versionada: '$requiredCategory'."
    }
}

$results = [System.Collections.Generic.List[object]]::new()
$violations = [System.Collections.Generic.List[string]]::new()
$observedIds = @{}

if ($MeasurementPath) {
    $measurements = Read-JsonFile -Path $MeasurementPath -Description "Arquivo de medições"
    if ((Require-Property $measurements "schemaVersion" "Arquivo de medições") -ne 1) {
        throw "schemaVersion do arquivo de medições não suportada; esperado 1."
    }
    $measurementRevision = [string](Require-Property $measurements "budgetRevision" "Arquivo de medições")
    if ($measurementRevision -ne $revision) {
        throw "Medições usam budgetRevision '$measurementRevision', mas o registro atual é '$revision'."
    }

    foreach ($measurement in @(Require-Property $measurements "metrics" "Arquivo de medições")) {
        $id = [string](Require-Property $measurement "id" "Medição")
        if (-not $budgetsById.ContainsKey($id)) { throw "Medição referencia ID desconhecido: '$id'." }
        if ($observedIds.ContainsKey($id)) { throw "Medição duplicada para '$id'." }
        $observedIds[$id] = $true

        $value = Assert-FiniteNumber (Require-Property $measurement "value" "Medição '$id'") "value de '$id'"
        $definition = $budgetsById[$id]
        $threshold = [double]$definition.failureThreshold
        $failed = if ($definition.comparison -eq "maximum") { $value -gt $threshold } else { $value -lt $threshold }
        $status = if ($failed) { "failed" } elseif (($definition.comparison -eq "maximum" -and $value -gt [double]$definition.target) -or
                                                     ($definition.comparison -eq "minimum" -and $value -lt [double]$definition.target)) { "warning" } else { "passed" }
        $results.Add([ordered]@{
            id = $id
            category = $definition.category
            value = $value
            unit = $definition.unit
            target = [double]$definition.target
            failureThreshold = $threshold
            comparison = $definition.comparison
            enforcement = $definition.enforcement
            status = $status
        })
        if ($failed) {
            $violations.Add("$id=$value $($definition.unit), limite de falha=$threshold ($($definition.comparison))")
        }
    }
}

if ($RequireAllMetrics) {
    foreach ($id in $budgetsById.Keys) {
        if (-not $observedIds.ContainsKey($id)) { $violations.Add("métrica obrigatória ausente: $id") }
    }
}

$report = [ordered]@{
    schemaVersion = 1
    budgetRevision = $revision
    budgetPath = [IO.Path]::GetFullPath($BudgetPath)
    measurementPath = if ($MeasurementPath) { [IO.Path]::GetFullPath($MeasurementPath) } else { $null }
    status = if ($violations.Count -eq 0) { "passed" } else { "failed" }
    definitionsValidated = $budgetMetrics.Count
    measurementsValidated = $observedIds.Count
    results = $results
    violations = $violations
}

$json = $report | ConvertTo-Json -Depth 8
if ($OutputPath) {
    $resolvedOutput = [IO.Path]::GetFullPath($OutputPath)
    $parent = Split-Path -Parent $resolvedOutput
    if ($parent) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
    $json | Set-Content -LiteralPath $resolvedOutput -Encoding UTF8
}
$json

if ($violations.Count -gt 0) {
    throw "Orçamento de métricas violado: $($violations -join '; ')"
}
