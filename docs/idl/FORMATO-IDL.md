# Formato do IDL de fronteira C#↔C++ (item 0.4.2)

Este documento especifica o formato do IDL que descreve a fronteira P/Invoke
entre `managed/Aether.Core/**/Native*.cs` e os headers C ABI correspondentes
em `native/*/*.h`. Ver `docs/adr/ADR-01-LINGUAGENS.md` para o contexto da
decisão e a divergência que este item fecha: o plano original pedia um IDL
que **gera** os dois lados; o que existe aqui é um IDL que **descreve e
valida** os dois lados já escritos à mão, sem gerar código ainda.

## Por que descrever em vez de gerar

Os três bindings existentes (`NativePhysics.cs`, `NativeSqlite.cs`,
`NativeTransformKernel.cs`) já estão escritos, testados e — no caso da
física — validados extensivamente em hardware. Substituí-los por código
gerado agora reintroduziria risco de regressão sem necessidade: nenhum bug
de divergência de ABI foi observado neste repositório até hoje. O risco real
documentado pela ADR-01 é **divergência silenciosa futura** — um campo
renomeado ou reordenado só de um lado, que só se manifesta como corrupção de
memória em runtime, não como erro de compilação.

Um IDL descritivo, comparado por um validador automatizado contra os tipos
`Native*` reais via reflection, resolve exatamente esse risco: qualquer
divergência entre o que o arquivo declara e o que o C# efetivamente expõe
quebra a suíte de testes. Gerar código a partir do IDL continua sendo um
passo futuro possível, mas não é pré-requisito para fechar esta lacuna.

## Por que não YAML/JSON

O repositório mantém política de zero dependência externa
(`NuGet.config` limpa todas as fontes; CONVENCOES.md §7: "não adicione
`PackageReference`") — não há parser YAML disponível sem adicionar uma
biblioteca de terceiros, e escrever um parser YAML completo à mão para um
caso de uso tão restrito seria complexidade desnecessária. `System.Text.Json`
está no BCL e seria uma opção sem dependência nova, mas o projeto já tem
precedente de formato texto próprio determinístico para casos assim
(`Aether.Configuration.ConfigurationStore.Serialize`,
`Aether.Serialization.TextSerializer`) — o IDL segue a mesma linha: um
formato de seções com uma entrada por linha, sem aninhamento, parseável
linearmente.

## Estrutura de arquivo

Um arquivo `.idl` por biblioteca nativa, em `docs/idl/*.idl`. Cada arquivo
corresponde 1:1 a uma classe `internal static partial class Native*` e à
`LibraryName` que ela declara. Codificação UTF-8, `\n` como separador de
linha, linhas em branco e linhas começando com `#` fora de um cabeçalho de
seção são ignoradas (comentário).

```text
library: aether_physics
managed_type: Aether.Physics.NativePhysics
native_header: native/physics/jolt_bridge.h

[enums]
NativeMotionType | uint32 | flags=false | Static, Kinematic, Dynamic
NativeAllowedDOFs | uint32 | flags=true | All, TranslationX, TranslationY, TranslationZ, RotationX, RotationY, RotationZ, Plane2D

[structs]
NativeBodyDesc | Shape, Position, Rotation, MotionType, Friction, Restitution, AllowedDOFs

[functions]
AetherPhysics_CreateWorld | gravity: float3, maxBodies: uint | returns: nint
AetherPhysics_DestroyWorld | world: nint | returns: void
```

### Regras de sintaxe

- As três linhas de cabeçalho (`library:`, `managed_type:`, `native_header:`)
  vêm primeiro, uma por linha, nessa ordem, cada uma com exatamente um `:`
  separando chave e valor (o valor pode conter `:` depois do primeiro, ex.
  um path Windows — só o primeiro `:` é o separador).
- Cada seção começa com um cabeçalho `[nome]` numa linha própria:
  `[enums]`, `[structs]`, `[functions]`, nessa ordem quando presentes (uma
  biblioteca sem enums, por exemplo, simplesmente omite a seção).
- Dentro de uma seção, cada linha não vazia é uma entrada, campos separados
  por ` | ` (pipe com espaço de cada lado, para legibilidade — o parser
  aceita variação de espaço ao redor do `|`).
- **Enum**: `Nome | tipoBase | flags=true|false | Membro1, Membro2, ...`
  — membros na ordem declarada no C#, separados por vírgula e espaço.
- **Struct**: `Nome | Campo1, Campo2, ...` — nomes de campo público, na
  ordem declarada, sem tipo (o validador confere blittable-ness do tipo
  real via reflection, não duplica o layout aqui).
- **Function**: `Nome | param1: tipo1, param2: tipo2, ... | returns: tipoRetorno`
  — sem parâmetros: `Nome | | returns: tipoRetorno`. Modificador de
  passagem (`in`/`ref`/`out`) e ponteiro (`*`) fazem parte do texto do
  tipo, ex. `desc: in NativeBodyDesc`, `outBody: uint*`.

## Tipos reconhecidos pelo validador

| Categoria | Exemplos | Regra |
|---|---|---|
| Primitivos blittable | `int`, `uint`, `long`, `ulong`, `float`, `double`, `byte`, `nint`, `void` | Comparados por nome exato do tipo C# |
| Matemáticos do Core | `float3`, `float4`, `quaternion` | Já blittable por contrato (CONVENCOES.md §2) — aceitos sem checagem adicional de layout |
| Ponteiro | `T*` (ex.: `uint*`, `PhysicsBodyHandle*`) | Só válido em contexto `unsafe`; o validador confere que o parâmetro C# real é de fato um ponteiro do mesmo tipo apontado |
| `in`/`ref`/`out` | prefixo no tipo do parâmetro (ex.: `desc: in NativeBodyDesc`) | O validador confere o modificador de passagem exato — `in` vs `ref` vs valor simples não são intercambiáveis silenciosamente |
| Enum/struct do próprio módulo | qualquer nome declarado em `[enums]`/`[structs]` deste arquivo | Deve corresponder a um tipo com `[StructLayout(LayoutKind.Sequential)]` (struct) ou tipo enum com o tipo base declarado |
| Enum/struct de outro namespace do Core | ex.: `PhysicsBodyHandle`, `PhysicsBodyDescription` | Aceito por nome; o validador só confere que o tipo existe e é um `struct`/`enum` (blittable é responsabilidade de quem o declarou, fora do escopo deste módulo) |

## O que o validador NÃO faz

- Não confere layout de memória byte a byte contra o header C++ (`sizeof`,
  offset de campo) — isso exigiria parsear C++, fora de escopo. A defesa
  contra esse tipo de divergência continua sendo os testes de integração
  que exercitam a ABI de ponta a ponta (`PhysicsTests.cs`,
  `SqliteBridgeTests.cs`, `tests/native/test_*_bridge.cpp`).
- Não gera código C# nem C++ a partir do arquivo `.idl` — ver "Por que
  descrever em vez de gerar" acima.
- Não impede um novo `[LibraryImport]` de ser escrito sem entrada
  correspondente no `.idl` — o validador falha *depois* que isso acontece
  (na próxima execução da suíte), não impede a escrita. Detecção, não
  prevenção — mesma disciplina do resto da suíte de testes do projeto.

## Como rodar

```bash
dotnet run --project tests/Aether.Tests -- IdlValidation
```

O validador está em `tests/Aether.Tests/IdlValidationTests.cs`, como parte
da suíte regular — roda em todo `dotnet run --project tests/Aether.Tests`
sem passo separado.

## Como adicionar uma função nativa nova

1. Escrever o `[LibraryImport]` em `Native*.cs`, como já se faz hoje.
2. Adicionar a entrada correspondente em `docs/idl/*.idl`.
3. Rodar a suíte — `IdlValidationTests` falha explicitamente se qualquer um
   dos dois lados ficar sem o outro, com o nome da função ausente na
   mensagem.

Esta ordem não é imposta pelo validador (ele só confere consistência final,
não ordem de escrita) — é convenção de processo, registrada aqui para quem
for adicionar a próxima função.
