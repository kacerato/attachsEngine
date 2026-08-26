# ADR-12 — WAL de edição com recuperação total

- **Estado:** aceita e implementada
- **Data:** 26/08/2026
- **Item do plano:** 0.4.1, 1.4.4
- **Decisores:** arquitetura, editor

## Contexto

O sistema operacional mobile mata processos em background a qualquer
momento, sem aviso — não é uma condição de erro rara, é o comportamento
normal esperado. Salvamento manual ou autosave periódico deixa uma janela de
trabalho perdido entre o último save e o kill do processo. A regra 1 da
barra de qualidade (CONVENCOES.md §8) não admite exceção: "nunca perder
trabalho do usuário".

## Decisão

Cada comando de edição é gravado num write-ahead log (WAL) append-only, com
`fsync` explícito, **antes** de o comando ser efetivamente aplicado. Se o
processo morrer a qualquer momento, reabrir o projeto recupera todos os
comandos gravados no log e os reaplica.

## Alternativas descartadas

1. **Salvamento manual.** Depende do usuário lembrar de salvar — o SO pode
   matar o processo entre duas ações do usuário, sem qualquer aviso.
2. **Autosave periódico.** Reduz a janela de perda, mas não a elimina — um
   crash entre dois autosaves ainda perde trabalho, e um intervalo curto o
   suficiente para eliminar a janela tem custo de I/O constante.

## Evidência

- `managed/Aether.Core/Editing/WriteAheadLog.cs` (201 linhas): log
  append-only com `fsync` explícito (`_stream.Flush(flushToDisk: true)`)
  antes de `Do()` rodar. Formato de registro: `int32 tamanho | uint32
  checksum FNV-1a | payload`. `Recover()` para na primeira falha (cabeçalho
  incompleto, tamanho inválido, checksum não bate, exceção de
  desserialização), preservando tudo que veio antes — cobre explicitamente
  o caso de queda de energia no meio de um append.
- `managed/Aether.Core/Editing/UndoStack.cs`: pilha de undo/redo com
  profundidade limitada, descarta o mais antigo em vez de travar ou vazar
  memória.
- `managed/Aether.Core/Editing/WalCommandRegistry.cs`: registro de
  (de)serialização por tag estável, por instância — evita estado
  compartilhado entre testes/sessões.
- `managed/Aether.Core/Editing/IEditCommand.cs`: contrato de comando
  (`Do`/`Undo`/`Description`).
- **Testes:** `tests/Aether.Tests/EditingTests.cs` — 16 testes, incluindo
  `ExecuteWithWal_GravaNoDiscoAntesDeChamarDoDoComando`,
  `WriteAheadLog_Recover_ApósCrashSemCheckpoint_DevolveComandosNaOrdemCerta`,
  `WriteAheadLog_Recover_ArquivoTruncadoNoMeioDoUltimoRegistro_DevolveOsAnterioresSemLancar`,
  `WriteAheadLog_Recover_ChecksumCorrompido_ParaAntesDoRegistroCorrompido`,
  `WriteAheadLog_Checkpoint_ZeraOLogERecoverNaoTrazNadaDepois`,
  `UndoStack_EstourarProfundidadeMaxima_DescartaOMaisAntigoENaoLanca`.

## Limitação atual

`docs/MATRIZ-MARCOS.md` (item 3.5.4) classifica o "salvamento contínuo" como
"parcial" — o mecanismo de dados (WAL, checksum, recuperação parcial) está
pronto e testado no nível de dados, mas não há fluxo de projeto/UI real
consumindo isso como salvamento contínuo de produto, porque a Fase 3 do
editor ainda é protótipo.

## Consequências

- Toda mutação editorial de produto futura deve passar por
  `UndoStack.ExecuteWithWal`, nunca aplicar uma mudança diretamente sem
  registro no WAL — é o único jeito de manter a garantia desta ADR válida
  para o editor inteiro, não só para os casos já cobertos.
- Quando o formato de projeto (item 7.4, ADR-07) for implementado como
  estrutura de diretório real, o `.aether/wal/` citado no plano deve usar
  exatamente este mecanismo, não um novo sistema de recuperação paralelo.
