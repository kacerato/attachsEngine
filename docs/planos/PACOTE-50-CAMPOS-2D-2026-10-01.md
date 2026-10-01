# Terceiro pacote de 50 — campos físicos 2D

## Contagem

**4 componentes + 40 propriedades semânticas + 6 consultas públicas = 50 itens
principais adicionais.** São tipos novos executados por Box2D, além dos campos
3D anteriores. Oito propriedades comuns reaparecem em cada tipo; não se afirma
que sejam 40 conceitos diferentes. Vector2 conta um item, sem recontar X/Y.
Infraestrutura vetorial, payload e contratos auxiliares não inflam a contagem.

| Nº | Categoria | Adição |
|---:|---|---|
| 1 | Componente | `GravityField2D` |
| 2 | Componente | `WindField2D` |
| 3 | Componente | `DragField2D` |
| 4 | Componente | `RadialField2D` |
| 5 | GravityField2D | `Enabled` |
| 6 | GravityField2D | `WakeBodies` |
| 7 | GravityField2D | `Shape` |
| 8 | GravityField2D | `HalfExtents (Vector2)` |
| 9 | GravityField2D | `Radius` |
| 10 | GravityField2D | `Offset (Vector2)` |
| 11 | GravityField2D | `Falloff` |
| 12 | GravityField2D | `AffectedLayer` |
| 13 | GravityField2D | `Vector (Vector2)` |
| 14 | GravityField2D | `ReplaceWorldGravity` |
| 15 | WindField2D | `Enabled` |
| 16 | WindField2D | `WakeBodies` |
| 17 | WindField2D | `Shape` |
| 18 | WindField2D | `HalfExtents (Vector2)` |
| 19 | WindField2D | `Radius` |
| 20 | WindField2D | `Offset (Vector2)` |
| 21 | WindField2D | `Falloff` |
| 22 | WindField2D | `AffectedLayer` |
| 23 | WindField2D | `Vector (Vector2)` |
| 24 | WindField2D | `Coefficient` |
| 25 | DragField2D | `Enabled` |
| 26 | DragField2D | `WakeBodies` |
| 27 | DragField2D | `Shape` |
| 28 | DragField2D | `HalfExtents (Vector2)` |
| 29 | DragField2D | `Radius` |
| 30 | DragField2D | `Offset (Vector2)` |
| 31 | DragField2D | `Falloff` |
| 32 | DragField2D | `AffectedLayer` |
| 33 | DragField2D | `LinearDrag` |
| 34 | DragField2D | `AngularDrag` |
| 35 | RadialField2D | `Enabled` |
| 36 | RadialField2D | `WakeBodies` |
| 37 | RadialField2D | `Shape` |
| 38 | RadialField2D | `HalfExtents (Vector2)` |
| 39 | RadialField2D | `Radius` |
| 40 | RadialField2D | `Offset (Vector2)` |
| 41 | RadialField2D | `Falloff` |
| 42 | RadialField2D | `AffectedLayer` |
| 43 | RadialField2D | `Acceleration` |
| 44 | RadialField2D | `TangentialAcceleration` |
| 45 | API runtime | `GameObject.PhysicsField2D(string typeId)` |
| 46 | API runtime | `PhysicsField2DRuntime.Sample(Vector2, uint)` |
| 47 | API runtime | `TrySample(Vector2, uint, out PhysicsFieldSample2D)` |
| 48 | API runtime | `Contains(Vector2)` |
| 49 | API runtime | `AffectedBodyCount` |
| 50 | API runtime | `AffectedMass` |

Registro resultante: **45 schemas, 44 tipos no Add, 44 fachadas geradas**.
Receitas: **73 → 77**. Atlas: **247 → 251**.
Quatro tipos novos possuem modelo/consumidor/criação/persistência; nenhum novo
tipo placeholder é listado. Aceite físico Android é uma evidência separada.

## Referências e adaptação

Godot **4.5**:
[Area2D](https://docs.godotengine.org/en/4.5/classes/class_area2d.html),
[source do node/Inspector](https://github.com/godotengine/godot/blob/4.5/scene/2d/physics/area_2d.cpp),
[source do solver](https://github.com/godotengine/godot/blob/4.5/modules/godot_physics_2d/godot_area_2d.cpp).
Áreas podem controlar gravidade e amortecimento; gravidade pontual distingue
centro de direção. Extraídos esses princípios de autoria e consumo. Astra usa
metros, XY com Y para cima e política comutativa de combinação, sem copiar
a escala em pixels, prioridades ou sinais de Area2D.

[Workflow oficial usando Area2D, Godot 4.5](https://docs.godotengine.org/en/4.5/tutorials/physics/using_area_2d.html):
volume, parâmetros físicos e combinação ficam configuráveis no Inspector.
Astra reúne área, efeito e alcance na superfície contextual existente, com
contorno no viewport e edição XY. Foram pesquisados vídeos de workflow;
nenhum vídeo foi reproduzido ou usado como evidência de implementação.

Godot **4.5**, [Area3D](https://docs.godotengine.org/en/4.5/classes/class_area3d.html):
vento em área é referência conceitual complementar. O vento oficial é restrito
a SoftBody3D; Area2D não possui esse vento. WindField2D é uma adaptação Astra,
executada sobre massa/velocidade reais de Box2D, não paridade nominal.
Componente tangencial de RadialField2D também é uma adaptação própria.

Backend: **Box2D 3.1.1**, confirmado em `native/third_party/Box2D/src/core.c`.
[API oficial de corpos](https://box2d.org/documentation/group__body.html)
e [source 3.1.1](https://github.com/erincatto/box2d/tree/v3.1.1) fundamentam
massa, centro de massa, velocidade, força e sono. Jolt não executa esses campos.

## Cadeia implementada

`Create/Add → PhysicsField2D/ComponentType → Inspector/reflection → archive →
GameWorld → ScenePhysics2D::applyPhysicsFields → Box2D →
ScriptBridge/SDK/contorno no viewport`.

Autoria reutiliza políticas comuns dos campos; tipos e descritores 2D são
independentes. Só canais XY são serializados e expostos. Reutilização de storage
interno não cria propriedades Z. Tamanho do volume, centro, força e vento são
metros/segundos, como o solver físico 2D existente.

Os efeitos atuam em corpos dinâmicos 2D ativos pelo COM atual do solver.
Collider deslocado pode colocar COM dentro da área enquanto a origem do objeto
está fora; esse cenário é parte do aceite. Collider continua responsável por
contatos. Os campos não criam sensores fictícios, não são interseção de superfícies
e não emitem enter/exit. Estáticos, cinemáticos, Character e corpos Jolt 3D não
recebem efeitos; estatísticas também os excluem.

Retângulo usa matriz XY completa da hierarquia; círculo escalado é elipse.
Campo aceita shear representável na transformação XY, mas rejeita tilt fora
do plano, reflexão XY e transformação singular, com erro no runtime. Profundidade
Z é visual e não participa de pertencimento. Direção do efeito usa eixo X
normalizado e sua perpendicular, sem amplificar força por escala.
Em hierarquias com shear, essa é a convenção de orientação publicada.

Uniforme vale 1 dentro da borda; Linear vale 1-d; Suave aplica smoothstep.
Retângulo usa maior distância normalizada por eixo, círculo usa distância local
radial. Radial combina direção do centro com tangente anti-horária em torno
de +Z. No centro, ambas contribuem zero. Acceleration negativa atrai; positiva
repele. TangentialAcceleration positiva produz rotação anti-horária.

Acelerações locais somam. A maior influência de substituição cancela gravidade
padrão uma única vez, usando GravityScale real do corpo nessa compensação.
Aceleração do campo independe de GravityScale. Vento soma acoplamentos e
velocidade ponderada; a massa real determina resposta. Vento/arrasto linear e
angular usam integração exponencial antes do passo Box2D. Colisões, damping e
locks próprios do corpo permanecem sob controle do solver.

WakeBodies falso preserva corpos adormecidos. Campo com efeito zero permite
sono; WakeBodies verdadeiro acorda apenas quando há alteração efetiva.
Camada é uma das 32 camadas ou todas; não muda a matriz de colisão.

## Vector2 e compatibilidade

`ComponentTriple` existente ganha kind explícito Vector2 e dimensões 2; mantém
seu transporte fixo de três floats. O terceiro float é padding e deve ser zero,
não um canal persistido. Descritor exige terceira identidade vazia.
Validação, editor, teclado e gerador C# respeitam a dimensão declarada.

`Component.SetVector2` publica XY numa única atribuição validada, via transporte
existente. Fachadas oferecem HalfExtents, Offset e Vector como Vector2.
Inspector desenha duas células; teclado recebe dois números, suporta decimal
com vírgula e separador `;`, rejeita terceiro número e gera um único undo.
RGB/Vector3 continuam com três canais; há regressão direcionada para esse caminho.
Presets/reset/multi-edit continuam endereçados pelos canais persistentes reais.

ABI permanece **33**: não houve mudança de layout/callback. FieldQuery despacha
por identidade de tipo para ScenePhysics2D ou ScenePhysics; payload continua
64 bytes, com canais Z de resposta 2D em zero. SDK oferece retorno Vector2.
Consultas validam mundo, geração, componente, ponto finito, camada, tamanho e
reservado. Contains consulta geometria mesmo quando desabilitado; Sample usa
influência zero. Falhas de estado são retornadas por TrySample; argumentos
inválidos são erros de chamada.

Edição de propriedades aparece no próximo passo fixo, sem reconstruir corpos
por causa do campo. Estrutura usa safe point; remoção é válida até flush e
depois rejeita a identidade antiga. Desativar ancestral/componente retira efeito.
Stop descarta candidatos/frames e expira acesso antigo. Estado do solver não
é salvo como configuração de campo.

## Defaults e domínios

| Propriedade | Default | Domínio |
|---|---|---|
| Enabled / WakeBodies | true / true | bool |
| Shape / Falloff / AffectedLayer | retângulo / uniforme / todas | retângulo/círculo; uniforme/linear/suave; 32 camadas ou todas |
| HalfExtents / Radius | (3;3) / 3 | 0,001–10000 m |
| Offset | (0;0) | ±10000 m |
| Gravity.Vector / ReplaceWorldGravity | (0; -9,81) / true | ±10000 m/s² / bool |
| Wind.Vector / Coefficient | (5;0) / 1 | ±10000 m/s / 0–10000 kg/s |
| LinearDrag / AngularDrag | 1 / 1 | 0–1000 s⁻¹ |
| Acceleration / TangentialAcceleration | -9,81 / 0 | ±10000 m/s² |

HalfExtents aparece só para retângulo; Radius só para círculo. Ambos são
persistidos, para troca de forma preservar configuração. Edição inválida é
atômica; não finitos e dimensão zero são rejeitados.

## Editor e evidências visuais

NÃO IREI SER SIMPLISTA NO DESIGN.

A proposta estrutural é autoria no plano XY com contorno selecionado, duas
células vetoriais e grupos Volume / Efeito / Alcance, usando a superfície
contextual e preservando viewport. Add e Create usam Física 2D/Campos.
Quatro ícones novos possuem formas planas e indicação 2D, integrados no pipeline
SVG/PNG → catálogo → enum → atlas. O conceito gerado não prova implementação.

Capturas executáveis, logs, resultados e hashes estão em
`docs/validacao/evidencias/fields2d50-20261001/`.
Rasterização de UI software usa fonte/atlas reais; não é render Vulkan da cena
nem interação física de toque. Cena permanece em paisagem e IDE em retrato
pelo contrato já implementado; este pacote não altera orientação.

## Aceite e custo

Executável dedicado com quatro cenários integrados:
1. Quatro receitas, Vector2 atômico, teclado localizado/rejeição de terceiro
   número, arquivo/reabertura, undo/redo, ícones e contornos; regressão RGB.
2. Hierarquia, rotação, escala não uniforme, círculo/retângulo, falloff, camada,
   disable e rejeição explícita de tilt no runtime.
3. Box2D real: gravidade sobreposta com GravityScale 2, vento/massas diferentes,
   COM de collider deslocado, arrasto forte, repouso/despertar e radial/vórtice.
4. ScriptBridge real, dois solvers separados, consultas e massa/camada,
   Vector2 vivo, padding/layout/mundo/geração inválidos, remoção/Stop.
O runtime gerenciado desse teste nativo é capturado por harness; não se
apresenta como execução C# completa.

Teste gerenciado direcionado protege transporte XY/zero-padding, fachada
gerada, projeção de retorno Vector2 e recusas. Ele usa recorder de protocolo,
não solver simulado. Dois testes existentes de ABI verificam layout preservado.
Os quatro cenários dos campos 3D e os quatro de molas/comandos são regressões
direcionadas; a suíte geral não é executada.

Loop custa O(corpos × campos). Candidatos são indexados por revisão estrutural;
frames preparados uma vez por passo, com capacidade dos vetores reutilizada.
Benchmark dedicado: 96 corpos Box2D, 32 campos sobrepostos, 60 passos medidos e
5 de aquecimento; inclui campos + solver + publicação. Números host Debug não
estimam FPS/custo térmico Android.

Projeto editável de aceite: `build/acceptance/Fields2D50-20261001`, quatro
campos e quatro cubos com Body2D/Collider2D, câmera ortográfica, luz e
`Scripts/Fields2D50Probe.cs`. Export usa recursos reais e verifica reabertura.
C# usa seis consultas, fachada Vector2 e observa velocidade Box2D.

## Resultado da verificação

- Host: **4/4 cenários novos**; **8/8 cenários de regressão** dos dois pacotes
  anteriores. Sem execução da suíte geral.
- Gerenciado: **1/1 protocolo Vector2 + 2/2 layouts ABI**. Script editável
  `Fields2D50Probe.cs` compilado com **zero avisos e zero erros**.
- Projeto exportado: arquivo reaberto, GameWorld carregado e um passo real
  Box2D executado. A observação C# após um segundo ainda depende de execução
  no aparelho; compilação não prova esse passo.
- UI: cinco capturas do executável de preview com fonte/atlas reais,
  conferindo retângulo/círculo, campos XY e grupos de efeito/alcance. Raster
  software de UI não certifica a renderização Vulkan ou toque Android.
- Android: native arm64 e `:app:assembleDebug` concluídos. SDK com os tipos 2D,
  Rendering e atlas dentro do APK conferidos byte a byte com assets gerados;
  bibliotecas nativas presentes. ABI **33** preservada.
- Custo host Debug: **1,269 ms médio, 1,871 ms p95, 2,254 ms máximo** para
  96 corpos × 32 campos, campos + solver + publicação. Esse recorte não é
  medida de FPS Android.

APK SHA-256:
`eb9d465096031a17edee76ed53fedd29ba30be19f1f987088eb579ecf1d8c957`.

Validação física nova no aparelho **pendente**: não houve instalação, execução
C# Android nem captura física deste pacote. A orientação da cena não foi
alterada; permanece em paisagem, com IDE em retrato.
