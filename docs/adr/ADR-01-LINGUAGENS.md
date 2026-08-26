# ADR-01 — C# para engine/editor, C++ para núcleo

- **Estado:** aceita e implementada
- **Data:** 26/08/2026
- **Item do plano:** 0.4.1
- **Decisores:** arquitetura

## Contexto

A engine roda no próprio celular — editor incluso, não só o jogo publicado
(CONVENCOES.md §1). Isso exige reflexão para gerar inspectors, hot reload
durante a edição, segurança de memória para não crashar o editor por bug de
plugin de usuário, e produtividade de time. Ao mesmo tempo, o núcleo precisa
de controle fino sobre memória e cache em ARM big.LITTLE com orçamento
térmico de ~4 W.

## Decisão

C# (.NET, net8.0) para engine/editor/gameplay/ferramentas — ~85% do código.
C++20 restrito a RHI Vulkan, física, áudio, codecs, geometria pesada e
alocadores.

## Alternativas descartadas

1. **100% C++.** Perde reflexão barata para gerar Inspector, hot reload, e
   segurança de memória — um bug de gameplay do usuário poderia crashar o
   processo do editor inteiro.
2. **100% C#.** Overhead de interop com Vulkan por comando de desenho (a
   submissão de draw calls no hot path não pode pagar o custo de P/Invoke por
   chamada) e ausência de bibliotecas maduras de física/codec em C#.
3. **Rust.** Sem ecossistema de reflexão/hot reload equivalente ao runtime
   .NET no momento da decisão; time e bibliotecas do projeto já orientados a
   C#.

## Regras de fronteira (CONVENCOES.md §2)

- A fronteira só transporta tipos **blittable**. Nada de `string` gerenciada
  cruzando diretamente — texto vai como ponteiro UTF-8 + comprimento
  explícito (ver `native/resources/sqlite_bridge.h` para o padrão).
- Chamadas nativas são sempre **em lote**. Orçamento: 200 chamadas nativas
  por frame.

## Evidência

- Código gerenciado: `managed/Aether.Core/`, `managed/Aether.Flow/`,
  `managed/Aether.Scene/` — cerca de 11.000 linhas (`docs/ESTADO.md`,
  resumo).
- Código nativo próprio (sem vendorizado): `native/core/`, `native/rhi/`,
  `native/rendergraph/`, `native/physics/`, `native/platform/android/`,
  `native/resources/` — cerca de 3.400 linhas.
- Fronteira P/Invoke via `[LibraryImport]` (source-generated, sem
  reflection): `managed/Aether.Core/Physics/NativePhysics.cs` (37
  assinaturas), `managed/Aether.Core/Resources/NativeSqlite.cs` (15
  assinaturas), `managed/Aether.Core/ECS/NativeTransformKernel.cs` (chamada
  em lote por frame).
- Integração CoreCLR real em produção: `native/platform/android/dotnet_host.h/.cpp`
  hospeda CoreCLR via `hostfxr`, validado em hardware físico (Xiaomi
  SM8735) — log real de dispositivo confirmando `CoreCLR carregado com
  sucesso via hostfxr` (`docs/ESTADO.md`, item 0.1.4).

## Divergência registrada

O plano original (`docs/PLANO-ENGINE-MOBILE.md` §3.1, regra 4) pede um IDL
próprio (`.aidl`) que **gera** os dois lados da fronteira, para que binding
nunca seja escrito à mão. Isso não existe: não há nenhum arquivo `.aidl` nem
gerador no repositório. `docs/MATRIZ-MARCOS.md` confirma o item 0.4.2 como
"não iniciado" — o P/Invoke atual (`NativePhysics.cs`, `NativeSqlite.cs`,
`NativeTransformKernel.cs`) é escrito à mão, um arquivo por subsistema.

Isso não invalida a decisão de linguagens em si (ADR-01), mas é uma dívida
declarada: cada novo subsistema nativo hoje exige escrever e manter dois
lados de assinatura manualmente, em vez de gerar a partir de uma única fonte.
O item 0.4.2 do plano principal ("Especificação do IDL de fronteira C#↔C++")
continua não iniciado e é o item que fecharia essa lacuna — não recebeu ADR
própria porque ainda é trabalho futuro, não uma decisão já tomada.

## Consequências

- Todo subsistema nativo precisa de um arquivo `Native*.cs` espelhando 1:1 o
  header C ABI correspondente — convenção estabelecida e seguida
  consistentemente (física, transform, SQLite).
- A ausência de gerador de IDL é um risco de divergência silenciosa entre os
  dois lados da fronteira (um campo renomeado só em C++ quebra em runtime,
  não em compilação) — mitigado hoje por testes de integração que exercitam
  a ABI de ponta a ponta (`tests/Aether.Tests/PhysicsTests.cs`,
  `SqliteBridgeTests.cs`).
