# Convenções de engenharia do Aether

Contrato obrigatório para todo código deste repositório. Vale para humanos e agentes.

## 1. Contexto do produto

O Aether é uma engine de jogos cujo **editor roda no próprio celular**, em
**orientação landscape, fullscreen**. Alvo: Vulkan 1.3, ARM big.LITTLE, orçamento
térmico de ~4 W, 60 fps. Isso não é uma engine de desktop portada — cada decisão de
código deve considerar cache pequeno, largura de banda de memória escassa e bateria.

Ver `docs/PLANO-ENGINE-MOBILE.md` para o plano completo.

## 2. Linguagens e fronteira

- **C#** (net8.0, `LangVersion` 12): engine, editor, gameplay, ferramentas. ~85% do código.
- **C++20**: apenas RHI Vulkan, física, áudio, codecs, geometria pesada, alocadores.
- A fronteira só transporta tipos **blittable**. Nada de `string`, nada de marshalling.
- Chamadas nativas são sempre **em lote**. Orçamento: 200 chamadas nativas por frame.

## 3. Regras não-negociáveis de desempenho

| Regra | Como se manifesta no código |
|---|---|
| **Zero alocação de GC no caminho de frame** | `struct` em vez de `class`, `Span<T>`, `stackalloc`, pools, arenas. Sem LINQ, sem lambdas capturantes, sem `foreach` sobre interface, sem boxing, sem concatenação de string |
| **Dados contíguos** | Arrays de struct, não arrays de referência. Iteração linear |
| **Sem trabalho por objeto na CPU quente** | Preferir loops sobre spans a chamadas virtuais por entidade |
| **Nada bloqueia a thread principal por > 100 ms** | Trabalho pesado vai para job |

Todo caminho quente novo deve vir acompanhado de um teste `Assert.NoAlloc`.

## 4. Estilo

- Namespace raiz `Aether`; submódulos `Aether.Flow`, `Aether.Scene`, etc.
- Tipos matemáticos em minúsculo por convenção de shader: `float2`, `float3`, `float4`,
  `quaternion`, `float4x4`, `math`. Todo o resto em `PascalCase`.
- `Nullable` habilitado, warnings são erros. Não suprimir warning sem comentário justificando.
- **Comentários e mensagens de erro em português.** Nomes de identificadores em inglês
  (é o padrão da plataforma), exceto nomes de testes, que são em português e descrevem
  o comportamento esperado — o nome do teste é documentação.
- Comentários explicam **por quê**, nunca **o quê**. Um comentário que repete o código é ruído.
- Sem código morto, sem `TODO` órfão, sem `throw new NotImplementedException()` em caminho alcançável.

## 5. Convenções de espaço 3D (fixadas e testadas)

- **Mão-esquerda**: X = direita, Y = cima, **Z = frente**. `Cross(Right, Up) == Forward`.
- Matrizes **column-major**; translação em `C3`. `a * b` aplica `b` primeiro.
- Ângulos em **radianos** em toda API interna; graus só na UI.
- Projeção segue Vulkan: profundidade em **[0, 1]**, **Y invertido** no clip space, `w = +z`.
- Preferir **reverse-Z** com plano distante infinito onde a precisão importar.

## 6. Testes

Runner próprio, sem NuGet (`tests/Aether.Tests`). O repositório **não tem dependências
externas** — o build precisa funcionar offline, e a suíte precisa poder rodar dentro do
dispositivo mais tarde.

```csharp
[Test] public static void NomeDescrevendoOComportamentoEsperado()
{
    Assert.Close(esperado, obtido, what: "por que isso importa");
}
```

- Métodos `public static`, sem estado compartilhado entre testes.
- Asserções disponíveis: `True`, `False`, `Equal`, `NotEqual`, `Close` (float/float3/quaternion),
  `Throws<T>`, **`NoAlloc`**.
- Um teste deve falhar por um motivo só, e a mensagem deve dizer qual.
- Testar **casos degenerados**: zero, vazio, singular, paralelo, capacidade estourada,
  índice reciclado. É lá que os bugs moram.

Rodar: `dotnet run --project tests/Aether.Tests` (aceita um filtro por substring).

## 7. Build

```bash
dotnet build Aether.sln          # camada C#
dotnet run --project tests/Aether.Tests
cmake -S native -B build/native -G Ninja && ninja -C build/native   # núcleo nativo
```

`NuGet.config` limpa todas as fontes de pacote de propósito. **Não adicione `PackageReference`.**

## 8. Barra de qualidade

Cinco coisas que nunca podem ser sacrificadas (do plano, Parte 18.3):

1. Nunca perder trabalho do usuário.
2. Nunca travar a thread principal por mais de 100 ms.
3. Nunca crashar o editor por culpa do conteúdo do usuário.
4. Nunca mostrar um erro que o usuário não entenda.
5. Nunca esquentar o aparelho além do orçamento térmico.
