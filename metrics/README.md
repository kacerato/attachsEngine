# Orçamentos de métricas

`budgets.v1.json` é a fonte versionada dos limites de performance da engine. O
campo `budgetRevision` identifica a revisão exata usada por cada relatório; uma
mudança de limite deve alterar essa revisão e ser revisada como mudança de
produto/performance, não como atualização incidental de CI.

Os seis domínios obrigatórios são `interop`, `allocations`, `cpu`, `gpu`,
`memory` e `energy`. Cada métrica declara unidade, comparação, alvo, limite de
falha, enforcement e coletor. `device-required` significa que o gate exige uma
medição proveniente do laboratório Android; fixtures e runners hospedados apenas
validam o contrato e nunca são apresentados como evidência de hardware.

Uma medição tem este formato mínimo:

```json
{
  "schemaVersion": 1,
  "budgetRevision": "2026-08-26.1",
  "measurements": [
    { "id": "interop.native_calls_per_frame", "value": 180 }
  ]
}
```

Validação do registro, de um relatório completo e do comportamento negativo:

```powershell
pwsh -File tools/validate-metrics-budget.ps1
pwsh -File tools/validate-metrics-budget.ps1 `
  -MeasurementPath metrics/fixtures/measurements-pass.v1.json `
  -RequireAllMetrics
pwsh -File tools/validate-metrics-budget.ps1 `
  -MeasurementPath metrics/fixtures/measurements-fail.v1.json `
  -RequireAllMetrics
```

O último comando deve terminar com código diferente de zero. Resultados reais
devem registrar também dispositivo, build, temperatura/estado térmico, duração e
distribuição p50/p95/p99 quando aplicável, conforme os gates M0/M1.
