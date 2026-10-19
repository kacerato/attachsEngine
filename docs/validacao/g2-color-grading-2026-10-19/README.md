# G2 · gradação de cor por LUT 32³ e perfis prontos (19/10/2026)

Plano: `docs/planos/DESEMPENHO-GRAFICOS-VIVOS-2026-10-17.md`, bloco G2, itens 2 e 7.
Aparelho: POCO F7 (Adreno 825), Astra Dev `dev.aether.editor.u07` vc 104 e 105, instalados
com `install -r` (projetos preservados). Branch `claude/wizardly-driscoll-540d21` sobre cddcd4a9.

## Referências

- Unity URP 17: Color Lookup, White Balance, Lift Gamma Gain e Shadows Midtones Highlights
  (<https://docs.unity3d.com/Packages/com.unity.render-pipelines.universal@17.0/manual/post-processing-color-lookup.html>).
  O modelo de trackball (cor + deslocamento), o balanço de branco em LMS (CAT02 em torno de
  D65) e as escalas 0,2/0,8 do lift/gama/ganho seguem o `ColorUtils` da URP.
- Godot 4.5, Environment › Adjustments/Color Correction
  (<https://docs.godotengine.org/en/4.5/tutorials/3d/environment_and_post_processing.html>):
  gradação aplicada à imagem já mapeada para o monitor, que é a ordem usada aqui.

## O que foi implementado

| Parte | Onde |
|---|---|
| Contrato `renderer::ColorGrading` (temperatura, matiz, lift/gama/ganho, sombras/médios/realces com faixas, vibração), `valid()`, padrão neutro, mistura de volumes no grupo Pós | `native/renderer/scene_environment.{h,cpp}` |
| Montagem da LUT 32³ na CPU (função pura) e leitura trilinear de referência | `native/renderer/color_grading.{h,cpp}` |
| Imagem 3D RGBA16F, 2 vagas (vista e prévia da Camera), cópia só quando a aparência muda | `native/platform/android/instanced_color_grading.inl`, `ImageDesc::depth` |
| Shader: binding 10 (`sampler3D`), uma leitura por pixel depois do tonemap e da resolução temporal (bit 65536); com a LUT, saturação, contraste e vinheta saem dela na ordem antiga | `native/rhi/shaders/post_process_common.glsl` (+ SPIR-V) |
| Ambiente v13 (31 números `grading_*`, 6 cores) e perfil de ambiente v9; v12/v8 abrem neutros | `native/scene/environment.h`, `native/resources/environment_profile.cpp` |
| 7 perfis prontos (Neutro, Cinemático, Dia quente, Entardecer, Noite azul, Tempestade, Fantasia) como `EnvironmentProfile` embutidos com GUID estável; aplicar troca só o grupo Pós | `native/resources/environment_looks.{h,cpp}` |
| API C# gerada: `Environment.GradingTemperature`, `GradingTint`, `GradingVibrance`, `GradingLift/Gamma/Gain` (+`Color`), `GradingShadows/Midtones/Highlights` (+`Color`) e as quatro faixas | `managed/Astra.Scripting/Generated/Components.g.cs` |
| Inspector: aba **Cor** do Ambiente com cartões (miniatura graduada pela mesma função da LUT, marca do perfil atual, cartão Personalizado) e **Ajuste fino** recolhido, com contador de controles alterados | `native/editor/editor_screen.cpp`, `editor_screen_input.cpp` |
| Ícones novos no atlas real: `lighting/color-grading` (trackball + deslocamento) e `lighting/look-presets` | `tools/generate-component-icons.py`, `pack-icon-atlas.py` |
| Capacidade `render.post.color_grading` | `native/core/engine_capability.h` |

Fora deste recorte (o plano lista, não foi feito): mixer de canais, curvas e importação de `.cube`.
Os perfis embutidos são escolhidos pelos cartões; não aparecem no seletor de perfis do projeto.

## Host

`tests/native/test_color_grading.cpp`, compilado só com os fontes envolvidos e o harness
(`family_base_main.cpp`): **10/10**. Também registrado em `aether_tests` e
`aether_environment_authoring_tests`.

- Neutro: células = coordenada² (erro ≤ 1e-6); pela LUT em meia precisão e leitura trilinear,
  256 cinzas + grade de cores 8 bits: **0,853/255** no pior caso.
- LUT × função, por perfil (grade 16³ de cores 8 bits): Neutro 0,52; Cinemático 1,40;
  Dia quente 1,53; Entardecer 1,15; Noite azul 2,87; Tempestade 3,21; Fantasia 1,80 (/255).
  O pior caso é cor saturada com um canal quase zero (erro linear ~0,001 ampliado pela curva
  sRGB perto do preto); o limite do teste é 4/255.
- Monotonia da luminância no cinza e em cada primária, para os 7 perfis.
- Limites: extremos válidos geram LUT finita; NaN, cor > 1, faixa < 0 e temperatura fora são
  recusados; entrada fora de [0,1] fica na borda.
- Migração: Ambiente v12 e perfil v8 leem neutros e mantêm o resto; v13/v9 fazem ida e volta.
- Mistura de volumes: peso 1 dá o alvo bit a bit; meio peso mistura; volume só de neblina não
  traz gradação.
- Suíte gerenciada (`Aether.Tests`): 523 passaram, 0 falharam.
- `tools/generate-embedded-shaders.ps1 -All -Check` (pwsh): todos reproduzíveis.

## Aparelho

### Custo (A/B/B/A, Vila em Play, nível S, 2772×1280, névoa resolvida ligada)

B = `-GradingLut` (`aether.post_grading_force`): LUT ligada mesmo no Neutro, o caso de custo
máximo (uma leitura 3D por pixel). Cada rodada confirmou o caminho no log `[ColorGrading]`.

| Rodada | LUT | FPS | GPU quadro (ms) | Opaco | Pós | pós/opaco |
|---|---|---|---|---|---|---|
| 01-A | não | 36,44 | 24,60 | 11,86 | 8,23 | 0,694 |
| 02-B | sim | 31,90 | 27,84 | 13,28 | 8,85 | 0,666 |
| 03-B | sim | 31,48 | 28,71 | 13,70 | 9,02 | 0,658 |
| 04-A | não | 32,31 | 27,89 | 13,46 | 8,76 | 0,651 |

- pós/opaco: A 0,694 e 0,651, B 0,666 e 0,658 — a diferença fica dentro do ruído entre
  rodadas A. No mesmo estado de clock (02-B × 04-A, opaco 13,3–13,5 ms) o pós foi
  8,85 × 8,76 ms: **+0,09 ms (~1%)**. A 1ª rodada A teve clock mais alto (opaco 11,9 ms).
- Conclusão: a leitura 3D por pixel custa perto de zero no POCO; o nível C precisa de dados
  de campo (telemetria) antes de qualquer afirmação.

### Imagem (Neutro sem LUT × Neutro pela LUT)

Pixels estáticos (iguais nas duas A e nas duas B): 97,7% da tela.
|B−A| ≤ 3/255 em **99,997%** deles; média 0,21; p99 e p99,9 = 2. 0,0004% acima de 10
(contornos de 1 px de personagens). Mapa: `diff-A01-B02.png`. Aceite (≤ 3/255): atendido.

### Capturas dos 7 perfis (`aparelho/`)

Vila Astra no editor, objeto "Atmosfera" › Cor, um toque por cartão (vc 104).
`perfis-vila-prancha.jpg` reúne as 7 vistas e a original; `p1`…`p7` são as telas inteiras;
`inspector-cor-vc104.png` e `inspector-ajuste-fino-vc104.png`, o Inspector.

- Os cartões marcam o perfil aplicado e o contador do Ajuste fino acompanha (6 a 9 controles).
- A Vila abre como **Personalizado** (contraste e vinheta próprios) e, migrada de v12, com a
  mesma aparência.
- Julgamento das capturas: o Ajuste fino cortava os rótulos das cores ("Cor das sombra") e o
  céu das miniaturas mostrava listras. Corrigido no vc 105 (cor com o nome da faixa, deslocamento
  "Brilho das sombras"…, degradê com faixas de ≤ 2 px); a captura do vc 105 ficou pendente
  porque o aparelho voltou a ser usado.
- A tela não foi julgada superficial (cartões com prévia, estado e ícones), então não foi
  gerada imagem conceitual; o Ajuste fino continua em linhas do Inspector genérico.

### Ocorrências durante a validação

- Dois toques meus caíram em campos numéricos porque o layout mudou entre a captura e o
  toque; nenhum valor foi confirmado. A Vila foi salva pelo editor com perfis aplicados; foi
  restaurada (Desfazer até o fim + Salvar) e conferida pelo texto do componente.
- Às 08:53 o editor gravou sozinho um estado de dois Desfazer atrás, **depois** de um
  Salvar manual com o estado correto. Corrigi o arquivo trocando só o texto do Ambiente
  (app parado) e conferi. Parece salvamento automático com retrato atrasado; fica como
  investigação à parte.
