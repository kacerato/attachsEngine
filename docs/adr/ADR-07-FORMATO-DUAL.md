# ADR-07 — Formato dual texto+binário

- **Estado:** aceita e implementada
- **Data:** 26/08/2026
- **Item do plano:** 0.4.1, 1.4.2, 7.4
- **Decisores:** arquitetura, serialização

## Contexto

O formato de projeto precisa de duas propriedades que um único formato não
entrega bem: diff/merge viável em controle de versão (exige texto legível,
determinístico linha a linha) e carga rápida em runtime (exige binário
compacto, sem parsing textual no caminho quente).

## Decisão

Todo dado persistido (cena, componente, junta física) tem duas serializações
geradas do mesmo modelo: texto determinístico (a verdade, versionável,
mergeável) e binário bit-exato derivado (descartável, para carga rápida).

## Alternativas descartadas

1. **Só binário.** Carga rápida, mas nenhum diff/merge legível em controle
   de versão — dois colaboradores editando a mesma cena produzem um
   conflito binário opaco.
2. **Só texto.** Diff/merge funciona, mas parsing textual no caminho de
   carga é custo desnecessário toda vez que o binário poderia ser
   reconstruído uma vez e cacheado.

## Evidência

- `managed/Aether.Core/Serialization/TextSerializer.cs`: formato texto v2,
  determinístico — o mesmo `World` sempre produz o mesmo texto byte a byte;
  `ComponentField.Id` persistente e aliases de componente/campo permitem
  rename sem perder dado (fecha `GAP-SER-01`).
- `managed/Aether.Core/Serialization/BinarySerializer.cs` (1091 linhas):
  serializador binário bit-exato, mesma disciplina de migração por cadeia
  (v1→v2→v3).
- `managed/Aether.Core/Serialization/ComponentMetadata.cs`,
  `WorldSnapshot.cs`, `ComponentRegistryBootstrap.cs`: metadados runtime que
  alimentam os dois serializadores a partir da mesma fonte.
- **Testes:** `tests/Aether.Tests/SerializationTests.cs` — 22 testes,
  cobrindo fixtures de pelo menos duas versões anteriores, rename de campo,
  campo novo/removido, arquivo de versão incompatível rejeitado com
  contexto.
- O mesmo padrão de formato dual é reutilizado em: juntas físicas (round-trip
  binário/texto de `Joint` com remapeamento de entidades — item 4.1.3) e no
  formato `.aflow` do AetherFlow (mesma disciplina textual — ADR-06).

## Consequências

- Qualquer novo tipo de componente/recurso persistido precisa registrar
  metadados compatíveis com os dois serializadores desde o início — não é
  aceitável um tipo que só serializa em binário ou só em texto.
- Toda mudança de schema exige um migrador explícito na cadeia
  (`v(N)→v(N+1)`), nunca reinterpretação silenciosa de um formato antigo.
- O item 7.4 do plano principal (estrutura completa de projeto —
  `project.aether`, `.ascene`/`.ascene.bin`, `index.db` via SQLite) ainda
  não foi implementado como estrutura de diretório de produto; o que existe
  hoje são os dois serializadores em si, prontos para essa estrutura os
  consumir quando a Fase 7 (pipeline de import) começar.
