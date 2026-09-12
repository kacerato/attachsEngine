# Mundo de execução, física consultável e ações de entrada

Contrato efetivo em 11/09/2026, branch `codex/gameplay-runtime`. Decisões e
alternativas descartadas em [ADR](adr/ADR-RUNTIME-GAMEPLAY.md); estado por
entrega em [Próximo pacote](PROXIMO-PACOTE-GAMEPLAY.md).

Este documento descreve **o que existe e foi exercitado**, não o plano.

## 1. O que roda no Play

```
EditorDocument (autoral)
        │  cópia única, no start
        ▼
runtime::GameWorld ──► runtime::ScenePhysics ──► Jolt
        │              runtime::ScriptBridge ──► Astra.Scripting (C#)
        │              runtime::InputService
        ▼
EditorMapScene::extract → lotes de desenho
```

O documento autoral **não é escrito** durante o Play. Um teste compara a revisão
do documento antes e depois de trinta passos de simulação
(`runtime_play_scene_runs_on_the_world_and_stop_preserves_authoring`).

`runtime::SceneGraph` não inclui nenhum cabeçalho de `native/editor`. É o que
permite ligar o mesmo par física+scripts a um consumidor sem editor. **Isso ainda
não é exportação de jogo** — não existe empacotador nem player autônomo.

Como o documento autoral não muda durante o Play, o renderer Android publica os
quadros da execução pelo caminho de poses (`queueAuthoredPoses`). Esse caminho
publica também o estado por instância que **não** é pose — material,
visibilidade e sombra —, senão uma propriedade escrita por script mudaria o
componente sem nunca mudar a tela. As camadas de água ficam de fora de propósito:
a simulação de água escreve nos mesmos lotes depois da extração.

## 2. Identidade de objetos

| Campo | Papel |
|---|---|
| `world` | Contador do processo. Handle de uma sessão de Play anterior é recusado |
| `id` | Persistente na cena; nunca reciclado dentro de um mundo |
| `generation` | Zera na destruição — a referência vence ainda dentro do callback |

Em C#, `GameObject` carrega os três. `IsAlive` é falso assim que o objeto é
destruído, e qualquer operação lança `WorldException` com o motivo real
(`StaleHandle`, `ForeignWorld`, `TransformOwnedByPhysics`, …).

## 3. Ponto seguro

Criar é imediato. Destruir, reparentear e remover componente entram na fila e são
aplicados entre passos. A tabela do ADR explica a assimetria; o resumo prático:

```csharp
var caixa = Object.CreateChild("Caixa");   // handle já funciona aqui
caixa.AddComponent(ComponentIds.MeshRenderer);
alvo.Destroy();                            // alvo.IsAlive já é falso
// o armazenamento sai no próximo ponto seguro do mundo
```

## 4. Componentes

O schema (`scene/component_schema.h`) é a única lista de regras. O que ele diz:

| Tipo | Categoria | Exige | Incompatível com | Anexável em Play |
|---|---|---|---|---|
| `astra.physics.body` | Física | — | personagem | não |
| `astra.physics.character` | Física | — | corpo, colisor | não |
| `astra.physics.collider` | Física | — | personagem | não |
| `astra.physics.joint` | Física | corpo | — | não |
| `astra.camera` | Câmera | — | — | sim |
| `astra.camera.look` | Câmera | câmera | — | sim |
| `astra.render.mesh` | Visual | — | — | sim |
| `astra.render.light` | Visual | — | — | sim |
| `astra.script.behavior` | Código | — | — | não |

"Anexável em Play" é sobre estrutura; escrever **propriedades** é permitido em
todos, exceto no comportamento de script (cujos campos são hidratados no start).

Componentes repetíveis (colisor, junta, comportamento) são endereçados por
instância, em C# e na ABI:

```csharp
var primeiro = Object.GetComponent(ComponentIds.Collider);
var segundo  = Object.GetComponent(ComponentIds.Collider, ordinal: 1);
segundo?.SetFloat("half_x", 2.5f);
```

Ler um campo com o tipo errado é recusado: devolver os bits de um float como enum
produziria um valor plausível e errado.

## 5. Autoridade de pose

| Autoridade | Quem publica | Escrita por script |
|---|---|---|
| Livre | ninguém | aceita |
| Corpo físico | Jolt | recusada, inclusive em ancestrais |
| Personagem | CharacterMotor | recusada, inclusive em ancestrais |

Para mover um corpo, use as APIs físicas: `SetBodyVelocity`, `MoveKinematic`,
`AddForce`/`AddImpulse`/`AddTorque`/`AddAngularImpulse`.

## 6. Camadas de gameplay

32 camadas nomeadas, matriz recíproca, persistidas na cena. A camada é do objeto
(campo `layer`, que a cena já guardava) e vale para o **corpo inteiro**.

A matriz vale no solver: um par proibido não gera contato. O teste
`runtime_layer_matrix_prevents_the_contact_in_the_solver` derruba uma bola sobre
um piso duas vezes — com as camadas interagindo ela repousa, sem interagir ela
atravessa.

Limites declarados:

- camada por **corpo**, não por colisor;
- 32 é o teto (uma máscara de 32 bits por linha da matriz);
- matriz assimétrica é recusada na gravação e na leitura.

## 7. Consultas físicas

```csharp
var filtro = QueryFilter.Default;
filtro.Ignore = ObjectId;                     // não acertar a si mesmo
filtro.LayerMask = Physics.LayerMask("Cenário");

if (Physics.RayCast(origem, direcao * alcance, filtro) is { } acerto)
{
    var objeto   = acerto.Object;             // identidade viva, com geração
    var colisor  = acerto.ColliderInstance;   // qual colisor respondeu
    var normal   = acerto.Normal;             // null quando não há normal
}
```

| Consulta | Devolve | Normal |
|---|---|---|
| `RayCast` / `RayCastAll` | objeto, colisor, ponto, distância, fração | sim (segunda consulta ao corpo) |
| `ShapeCast` | idem, fração ao longo do caminho | sim (eixo de separação normalizado) |
| `Overlap` | objeto, colisor, ponto | **não** — sobreposição parada não tem direção |

`RayCastAll` e `Overlap` devolvem a contagem **real**; `truncated` diz quando o
buffer não coube em tudo. Raio de comprimento zero é recusado. Sensores ficam
fora por padrão.

## 8. Contatos sólidos

`CollisionEnter/Stay/Exit` chegam aos **dois** objetos do par, um por passo
físico, agregados por par de corpos. `Collision.Normal` é nulo no Exit: o Jolt
não informa geometria quando o contato termina, e um vetor zero no lugar
pareceria um contato de frente.

Sensores continuam usando `TriggerEnter/Stay/Exit`, dirigidos do sensor ao outro.

## 9. Ações de entrada

```csharp
var mover = Input.Move;                 // papel de movimento do projeto
if (Input.JustPressed("Interagir")) { … }
Input.SetContextEnabled("menu", true);  // silencia sem apagar a configuração
```

O mapa declara ações (Button, Axis1D, Axis2D), bindings por fonte, zona morta,
sensibilidade, inversão e contexto; e diz quais **papéis** (mover, olhar, saltar)
cada ação cumpre. Renomear a ação leva o papel junto.

**Foco.** Enquanto a interface do editor consome o toque — IDE aberta, teclado,
modal, gizmo arrastado, Play pausado — o gameplay lê zero e os botões são
soltos na hora. A pausa e o cancelamento usam o mesmo caminho.

**Dispositivos.** O toque Android alimenta manche, arraste e botões da interface
de jogo. Teclado e gamepad têm fontes de binding implementadas e avaliadas pelo
serviço, exercitadas nos testes de host; **o shell Android ainda não alimenta
nenhuma das duas**, então elas não são apresentadas como validadas no aparelho.

## 10. Modelos de comportamento

Oito modelos editáveis acompanham o editor (`assets/script-templates`), oferecidos
ao criar um script e **nunca semeados em projeto nenhum**:

| Modelo | O que exercita |
|---|---|
| Seguir alvo | Referência resolvida uma vez, transform de mundo |
| Câmera orbital | Ações de entrada, shape cast contra obstáculo |
| Interação | Raycast com máscara de camada, capacidade por contrato |
| Porta | Contrato `IInteragivel`, pose autorada preservada |
| Plataforma móvel | `MoveKinematic` por passo fixo, transporte pelo solver |
| Item coletável | Sensor, destruição no ponto seguro, contrato de coleta |
| Trocar material | Propriedades de componente pela API comum, contato sólido |
| Controle de luz | Propriedades de Luz pela API comum, alvo em outro objeto |

Eles conversam por **interface** (`IInteragivel`, `IColetavel`), resolvida por
capacidade: acrescentar uma alavanca não exige tocar no script que interage. As
interfaces não são componentes e não aparecem no menu "Adicionar".

O header embutido no editor é gerado por `tools/build-script-templates.py` a
partir dos mesmos arquivos que o teste gerenciado compila com o compilador do
projeto do usuário. Um modelo que não compila não chega ao editor.

## 11. Catálogo de código: instância, schema e execução

São três coisas distintas, e confundi-las produziu um defeito observável: editar
o texto apagava a lista de tipos, o componente sumia de "Adicionar componente" e
do inspetor — e continuava anexado e executando no Play.

| Coisa | Quem é dono | O que a apaga |
|---|---|---|
| Instância anexada | o documento de cena | remover o componente, comando explícito |
| Schema conhecido | o catálogo **publicado** | fechar o projeto |
| Versão em execução | a sessão de Play | Stop |

O catálogo publicado **não** é limpo por digitar, desfazer, refazer, criar
arquivo nem por uma compilação que falhou — a mensagem de falha sempre prometeu
preservar a versão anterior, e agora isso é verdade.

A publicação é atômica: `applyBuildReport` apenas **encena** os tipos, e só
`publishBuild` os promove, depois de o hospedeiro confirmar que carregou o
assembly. Recusar a publicação devolve o catálogo anterior, que continua correto
porque o assembly anterior continua em uso. Um relatório de build de uma geração
antiga é recusado e não publica nada.

O estado do catálogo é dito, não escondido:

| Estado | Significado |
|---|---|
| Nenhum código publicado | o projeto ainda não compilou nada |
| Publicado corresponde ao texto | em dia |
| Texto alterado desde a última publicação | há rascunho mais novo; o publicado vale |
| Compilação falhou | o publicado anterior continua em uso |

## 12. ABI de scripts

v5. Campos de v2 nas mesmas posições; v3 acrescentou hierarquia/ciclo de
vida/componentes/propriedades/transform de mundo, v4 consultas e contatos, v5
ações de entrada. O lado gerenciado exige a versão corrente e confere `size`.

## 13. Luzes

`astra.render.light` é um componente como qualquer outro: anexado pelo mesmo
catálogo, editado pelo mesmo inspetor genérico, escrito pela mesma API de
propriedades em C#, salvo pelo mesmo registro de componentes. A pose vem da
Transformação, e **`+Z` local é a direção de emissão** — a mesma convenção de
frente da Câmera.

| Propriedade | Id | Faixa | Significado |
|---|---|---|---|
| Modalidade | `kind` | Direcional / Pontual / Spot | consumidor gráfico distinto por modalidade |
| Acesa | `enabled` | — | apagar não remove o componente |
| Cor | `color.r/g/b` | 0..1 | **RGB linear**, não sRGB |
| Intensidade | `intensity` | 0..10000 | direcional: irradiância na escala do sol; pontual/spot: valor a um metro |
| Alcance | `range` | 0,01..1000 m | janela suave; a luz chega a zero exatamente em `range` |
| Cone interno/externo | `inner_angle`/`outer_angle` | 0..89° | meio-ângulos; interno maior que externo é recusado na autoria |

Pontual e spot entram num buffer de quadro com teto de
`renderer::MaximumPunctualLights` (**8**). A escolha é feita no quadro, com a
câmera dele: maior influência primeiro — intensidade e alcance contra a
distância — e, em empate, menor id de objeto, para que dois quadros com a mesma
cena produzam a mesma lista. **Nada some em silêncio**: `LightBudgetReport` conta
o que não coube, por modalidade, e o shell publica um aviso quando o número muda.

Direcional tem **um** consumidor: o sol do ambiente, com as cascatas de sombra
que já existiam. Uma direcional acesa na cena passa a ser esse sol; a segunda é
declarada excedente pelo mesmo relatório. Sem nenhuma, o sol do recurso de
ambiente continua valendo.

Sombra é capacidade publicada, não promessa. `renderer::lightCapabilities` diz o
que existe em passe e shader:

| Modalidade | Ilumina | Projeta sombra |
|---|---|---|
| Direcional | sim | **sim** (cascatas do sol) |
| Pontual | sim | não |
| Spot | sim | não |

Por isso a Luz **não expõe** interruptor de sombra: enquanto não existir um
cubemap ou um atlas de spot, o botão seria um controle sem shader atrás. Uma luz
pontual atravessa parede, e isso está escrito aqui em vez de disfarçado.

Mudar uma luz durante o Play funciona pelo mesmo caminho de qualquer
propriedade: a coleta do quadro lê o mundo em execução, não o documento autoral.
O modelo `ControleDeLuz` acende, apaga e pulsa uma luz por esse caminho.

## 14. Recursos e importação

Um recurso tem **identidade** (`AssetGuid`, 128 bits), um **caminho** e um
**hash de conteúdo**, e os três são coisas diferentes:

| Ação | Muda o caminho | Muda o hash | Muda a identidade |
|---|---|---|---|
| Renomear ou mover o arquivo | sim | não | **não** |
| Editar o arquivo e reimportar | não | sim | **não** |
| Importar outro arquivo | — | — | sim |

É o motivo de o registro existir. Uma cena que referencia por caminho quebra
quando alguém arrasta o arquivo para outra pasta; uma que referencia por hash
quebra quando alguém corrige uma normal.

O registro (`.astra/assets.astra`, ao lado da cena) guarda por recurso: tipo,
fonte, hash, versão e parâmetros do importador, dependências e derivados.
Dependência para um GUID inexistente é recusada na entrada, apagar recurso com
dependente é recusado com a lista de quem depende, e uma leitura inválida deixa
o registro anterior intacto.

**Malha**: `MeshRenderer` carrega `mesh` (o slot resolvido neste processo) e
`asset` (a identidade). Ao carregar, o slot é recalculado a partir da
identidade; quando ela não está no pacote, o slot vira **zero** — referência
ausente é informação, e desenhar a malha que por acaso ocupa o índice antigo
seria corromper a cena em silêncio. Cena anterior a isto carrega e ganha a
identidade derivada do slot que já trazia.

**A árvore do arquivo vira árvore de objetos.** A lista de desenhos é uma saída
para renderização; ela não é a hierarquia. A importação produz um nó por nó do
arquivo — **inclusive os sem malha**, que são exatamente o que segura a
articulação — com a transformação LOCAL de cada um, e a geometria fica no espaço
do nó em vez de assada em coordenadas de mundo.

Sem isso, importar achata: as partes nascem irmãs na raiz, mover a carroceria
não leva a porta junto, e a pose de cada nó se perde. Uma versão anterior desta
importação fazia exatamente isso.

O **pivô** de uma parte importada é a origem do NÓ, não o centro dos limites.
Uma porta gira na dobradiça porque foi ali que o autor colocou a origem;
recentralizar no volume visível moveria a rotação para o meio da porta. As
primitivas internas do editor mantêm a convenção do pacote — pivô no centro —,
que é o que faz um cubo girar em torno de si.

Um nó cuja matriz tem shear ou reflexão não cabe em translação/rotação/escala.
Ele chega com a pose fixa no desenho e o objeto nasce na identidade, e a
importação **conta** esses casos. Arredondar e chamar de preservado seria mentir
sobre o arquivo. Um nó com várias primitivas ganha um filho por primitiva, com o
nome indicando a parte: slots por submesh pertencem ao material compartilhado,
que não existe.

**Importar um GLB no aparelho.** `Adicionar objeto → Geometria → Importar
modelo` abre o seletor do sistema. O URI é `content://` e **não** vira caminho
POSIX: quem lê os bytes é o `ContentResolver`, que é quem tem a permissão. O
arquivo é copiado para `Fontes/` dentro do projeto antes de qualquer outra
coisa — depois disso o projeto abre sem depender do URI temporário. Ao reabrir,
as fontes do registro são relidas e a geometria republicada; os objetos que já
estavam na cena reatam pelo identificador, sem duplicar.

O que a importação traz e o que não traz:

| Traz | Não traz |
|---|---|
| a árvore inteira, com nós sem malha e transformações locais | texturas (imagens) |
| instâncias: o mesmo mesh em vários nós custa N desenhos e **uma** cópia da geometria | skins e animações |
| primitivas TRIANGLES | pontos, linhas, faixas e leques |
| POSITION, NORMAL, TANGENT, TEXCOORD_0/1, COLOR_0 | câmeras e luzes do arquivo |
| fatores PBR, emissivo, alfa e dupla face | extensões do formato |

Texturas ficam de fora por um motivo concreto: o runtime não decodifica PNG nem
JPEG, e o decodificador do sistema (`AImageDecoder`) só existe a partir do
Android 11 enquanto o mínimo suportado é o 8. O material chega com os fatores,
que é o que o shader consome sem imagem. O que o arquivo trazia e ficou para
trás é **contado** e aparece na barra de status.

Nenhuma conversão de coordenadas: glTF é destro, +Y para cima, e é assim que o
pacote de mapa desta engine já guarda geometria. Só **GLB**: um `.gltf` depende
de arquivos ao lado dele, que o seletor do Android não entrega, e é recusado com
esse motivo escrito. Extensão exigida, acessor esparso, índice fora da faixa,
ciclo na hierarquia e nó com `matrix` e TRS juntos também são recusados, cada um
com diagnóstico próprio.

Reimportar a mesma fonte **substitui** a geometria e não duplica os objetos:
quem quer outra cópia instancia de novo, e isso é um ato diferente. A ligação
sobrevive porque a identidade de cada malha vem dos NOMES do nó e da malha, não
do índice — acrescentar um objeto no editor 3D reindexa tudo o que vem depois.
Renomear no editor 3D quebra a ligação, e isso é dito em vez de adivinhado.

## 15. Grade editorial

A grade do chão é **desenho no mundo**, não interface. Ela é um plano analítico
desenhado dentro da cena, depois da geometria opaca e antes do transparente,
com teste de profundidade: uma caixa opaca esconde as linhas atrás dela.

Antes ela eram 258 segmentos projetados como linhas de interface, depois de tudo
e sem profundidade nenhuma — por construção apareciam por cima de qualquer
objeto. E trocar de década de escala trocava todos os segmentos de uma vez, que
era o piscar ao aproximar e afastar.

> A primeira versão do passe analítico ainda saía inclinada e escorregava sobre
> a geometria ao dar zoom. A causa não era o passe, e sim a reconstrução do raio
> de câmera — está descrita abaixo, porque é o tipo de erro que reaparece em
> qualquer passe de tela inteira que precise do raio por pixel.

A separação é a do plano mestre:

| Tipo | Exemplo | Profundidade |
|---|---|---|
| Desenho no mundo | grade, eixos do piso | testa contra a cena |
| Ferramenta | gizmo de mover/girar | sempre visível, por decisão |
| HUD da vista | botões, rótulos | nunca finge ser geometria |

A política fica no editor (`buildEditorGridPlan`) e a cobertura no fragmento:
o editor escolhe a célula fina, a grossa, o peso da mistura e onde a grade some;
o fragmento intersecta o raio com o plano, desenha as linhas com espessura
constante em pixels (`fwidth`) e escreve `gl_FragDepth` com **a mesma projeção**
do vértice da cena.

Duas células coexistem e a fina perde peso continuamente dentro da década, e
chega a zero antes de a próxima assumir. Um teste de host percorre a distância
da câmera e exige exatamente isso.

### O contrato de câmera vive num lugar só

A mesma projeção aparecia escrita à mão no vértice da cena, no céu, no passe da
grade, na seleção por toque e na oclusão. Foi assim que uma das cópias divergiu
sem que nada acusasse. Agora ela está em `renderer/camera_ray.h`, que é a
referência que as outras espelham:

```
view   = B · (mundo − câmera)                B ortonormal, inversa = transposta
xy     = (view.x/tanH, view.y/tanV)          tanH = tanV · proporção
ndc    = M · (xy.x, −xy.y) / view.z          M = pré-rotação do display
clip.z = (far·view.z − near·far)/(far − near),  clip.w = view.z
```

`screenPointToRay` (a seleção por toque) usa o contrato diretamente, em vez da
cópia que mantinha. Cinco testes de host cobrem o caminho: ida e volta
mundo→pixel→raio, as quatro orientações de tela, e a recusa fechada de um
frustum que não dá para inverter.

### O raio de câmera precisa ser afim em NDC

O vértice do passe reconstrói o raio de cada pixel invertendo a projeção do
vértice da cena. Ele passa o raio **cru** `(x, y, 1)`, que é afim em NDC e por
isso interpola exato entre os três vértices. O versor **não é afim**, e
normalizar no vértice era o defeito de fundo do viewport:

- o triângulo de tela inteira tem vértices em NDC `(-1,-1)`, `(3,-1)`, `(-1,3)`;
- os três raios têm comprimentos bem diferentes (≈1,4, ≈2,8 e ≈2,2 num campo de
  60° com a proporção da vista);
- normalizar e depois interpolar devolve, **no centro da tela**, uma direção
  cerca de 17° fora em X e 5° em Y da que o pixel realmente enxerga.

O efeito era exatamente o relatado: a grade saía inclinada contra objetos retos,
o horizonte dela não coincidia com o da cena, e ao mover a câmera ela escorregava
em relação à geometria — a grade parecia "subir por cima" do objeto enquanto o
objeto encolhia. O céu tinha o mesmo defeito desde sempre e ninguém via, porque
gradiente tolera deformação; a grade, não. O fragmento normaliza quando precisa,
e a interseção com o plano nem precisa — ela é invariante à escala do raio.

O vértice também subtrai o deslocamento temporal (`jitter`) antes de desfazer a
projeção, porque a cena o soma **depois** da divisão por `view.z`. Sem isso a
grade anda meio pixel por quadro contra a geometria.

Dois testes de host trancam isso sem precisar do aparelho: um exige que
interpolar o raio cru entre os três vértices do triângulo reproduza o raio exato
em qualquer ponto da tela, e o outro **mede o erro do jeito errado** — normaliza
nos vértices, interpola, e exige que o desvio no centro passe de 15°. O segundo
está lá para que a regra do primeiro não pareça zelo.

### Moiré por família, não pela grade inteira

Cada família de linhas — as perpendiculares a X e as perpendiculares a Z — mede
o próprio tamanho de célula em pixels e desaparece sozinha abaixo de ~2 px, onde
o `fract` passa a alternar dentro do mesmo pixel e o que aparece são faixas
diagonais em ângulos que não existem na grade. Medir a grade por um número só
obrigava a escolher entre apagar as linhas ainda nítidas ou manter as que já
viraram ruído, porque em ângulo rasante a célula tem centenas de pixels numa
direção e frações na outra.

**Resolvido junto:** o vazamento da grade através de faces em ângulo rasante era
consequência do mesmo erro — a profundidade escrita vinha de um ponto de
interseção calculado com o raio errado. Com o raio correto, o empurrão relativo
de profundidade caiu de 0,5% para 0,05%, e ele existe só para desempatar o
contato coplanar entre o chão e a base de um objeto apoiado nele.

## 16. Escrita de texto no Android

O teclado é do sistema; o **campo é do editor**.

A ponte (`EditorTextInput.java`) não desenha campo nenhum. Ela mantém uma `View`
de um pixel, invisível, só para segurar o foco e a `InputConnection` — é o que
faz o IME abrir e entregar composição, correção e área de transferência. O texto
vai para o lado nativo **a cada tecla**, e o editor desenha o campo com o cursor
na sua própria superfície, apoiado na borda de cima do teclado.

Antes disso a edição inteira acontecia num `AlertDialog` que cobria a tela: o
usuário não via o objeto que estava renomeando nem o valor que estava mudando
enquanto digitava, e a busca só filtrava depois de confirmar.

| Coisa | Quem faz |
|---|---|
| Teclado, composição, correção, área de transferência | IME do Android |
| Foco e `InputConnection` | a `View` de um pixel |
| Campo, cursor, posição na tela | o editor, na superfície dele |
| Rascunho e confirmação | `EditorSession` |

`updateTextDraft` é **rascunho, não comando**: nada entra no documento nem no
histórico. A única coisa que acontece antes de confirmar é a busca filtrar
enquanto se digita, que não toca em nada. Confirmar continua sendo
`completeTextEdit`, e continua sendo um único comando de histórico.

O cursor chega em **bytes UTF-8**, não no índice UTF-16 do Android: é assim que o
lado nativo indexa o texto, e converter no lugar errado poria o traço no meio de
um glifo acentuado. A altura do teclado chega como **fração** da janela, porque o
Android mede em pixels físicos e a superfície do editor é lógica.

Teclas chegam por dois caminhos que existem os dois — o IME pela
`InputConnection`, e teclado físico ou evento injetado direto na `View`. Os dois
passam pela mesma função.

### O código é escrito no próprio editor

Não há mais diálogo em nenhum caminho. Tocar numa linha do editor abre o teclado
e põe o cursor ali; o texto aparece na linha, no editor, enquanto é digitado.

Duas decisões que isso obrigou:

- **Código não tem "aplicar".** O texto digitado **é** o buffer, e entra a cada
  tecla por `EditorCodeWorkspace::type`. Voltar fecha o teclado e o texto fica
  onde está, como em qualquer editor.
- **Uma entrada de desfazer por sessão de digitação, não por tecla.** `replace`
  guarda uma cópia do arquivo inteiro a cada chamada; com uma chamada por tecla,
  desfazer voltaria caractere a caractere e o histórico cresceria com uma cópia
  do arquivo por tecla. `type` empilha o instantâneo na **primeira** tecla da
  sessão e sobrescreve nas seguintes. Qualquer comando que não seja digitar —
  desfazer, refazer, `replace`, fechar o campo — encerra a sessão.

O corpo do editor desconta a altura do teclado, e a rolagem persegue o cursor a
cada quadro: a altura útil só encolhe quando o teclado termina de subir, um ou
dois quadros depois do toque, e é a contagem de linhas visíveis daquele instante
que vale.

**O toque escolhe a linha; o cursor vai para o fim dela.** Coluna exata exigiria
medir o texto, que só acontece na construção das instâncias. Aproximar por
largura média poria o cursor no lugar errado em toda linha com indentação — ou
seja, em toda linha de código. É o que falta do M05.3, junto com seleção e
rolagem horizontal.

**Ainda não verificado no aparelho:** o mapeamento do toque para a linha e uma
inserção de texto que apareceu uma vez durante o teste e que eu não consegui
atribuir. Ver o relatório de validação.

## 17. Gerenciar recursos no painel de arquivos

Renomear, mover e apagar um recurso, com as regras que a identidade separada do
caminho torna possíveis.

**Renomear e mover são a mesma operação**, no painel e no registro: o que muda é
o caminho. E **a cena não é tocada** — os objetos guardam o GUID do recurso, não
o caminho dele. Esse é o retorno concreto do registro com identidade: mover um
arquivo não pode quebrar um objeto, e um teste de host exige exatamente isso
(renomeia a fonte e conta quantos objetos continuam sendo desenhados).

Mover uma pasta reaponta tudo que está dentro dela numa operação só. Aplicada aos
poucos, uma colisão no meio deixaria metade da pasta apontando para o caminho
novo e metade para o velho, e não há como saber qual metade. `source` e
`derived` acompanham: um arquivo que muda de pasta muda para todos, senão o
projeto reabre procurando o arquivo onde ele não está mais.

**Apagar confronta quem usa.** O primeiro toque conta os objetos da cena e os
recursos que dependem do arquivo e recusa, dizendo os números; o segundo, no
mesmo arquivo, confirma. Dois toques e não um diálogo, pela mesma razão de §16.

A contagem passa pelas identidades da fonte, e não pelo recurso da fonte: um GLB
de cinco nós vira cinco identidades, e são elas que os objetos guardam. Contar só
o recurso do arquivo diria "ninguém usa" com a cena inteira montada em cima dele.

Forçando, os objetos que apontavam para o recurso ficam **sem malha,
visivelmente** — a reconciliação já transforma identidade ausente em slot zero,
em vez de apontar para a malha que por acaso ocupar o índice antigo.

### O registro vai ao disco na hora

Renomear e apagar mexem no disco imediatamente. Até o registro acompanhar, ele
aponta para um caminho que não existe mais.

Medido no aparelho: renomear a fonte e reabrir o projeto **antes de salvar**
apagava o veículo da tela com a hierarquia inteira preservada. Por isso a
operação marca o registro para gravação imediata, e não espera o próximo salvar.

### O que ainda não existe

- Mover para outra pasta pelo painel (arrastar ou escolher destino). A operação
  aceita qualquer destino; a interface só oferece renomear no lugar.
- Criar pasta pelo painel, e desfazer de uma operação de arquivo — apagar é
  definitivo, e é por isso que ele pede confirmação.

## 18. Ícones no idioma da marca

Os 37 ícones que o editor usa hoje são desenhados por geometria própria, em
`tools/generate-astra-mark-icons.py`, no vocabulário da marca Astra. **O enum não
muda** — nem uma linha de código de tela foi tocada. O identificador de um ícone
é um índice, o índice vem do nome de catálogo, e só a arte por trás do nome
trocou.

A marca é duas cores e nada mais: lima sobre quase-preto, formas sólidas, sem
contorno e sem gradiente. Quatro traços dela aparecem em todo desenho:

- forma **sólida**, nunca traço fino — a silhueta é o ícone;
- varredura que **afina até a ponta**, como o anel que abraça a estrela;
- lado **côncavo** onde uma ponta nasce, como as pontas da estrela;
- um **círculo cheio** como acento, quando o desenho pede um centro.

### Por que isso não exige mudança na interface

Os ícones saem em lima sobre transparente. Nos painéis escuros eles aparecem como
são; na pastilha acesa a tintura multiplica pelo quase-preto e devolve a relação
da marca invertida — preto sobre lima. É a mesma relação do logotipo nos dois
estados, e é por isso que a tintura existente continua valendo sem exceção.

### A marca funciona grande; o ícone vive a 20 px

Foi o que a primeira versão errou. A ponta que afina é o gesto da marca, mas
abaixo de umas três unidades de espessura ela simplesmente some na tela. Desfazer
e refazer viravam ganchos, girar virava um ponto. A regra que ficou: a ponta fina
só onde ela é **o gesto** — nunca onde ela é o desenho inteiro.

Sem brilho externo, de propósito: a célula do atlas tem 96 px e um halo largo
vira sangramento entre células vizinhas.

Os outros 115 nomes do catálogo continuam na arte anterior. Desenhar um a um sem
consumidor seria trabalho sem leitor; o M06.2 dirá de quais a reconstrução do IDE
precisa.

## 19. O que este documento NÃO afirma

- Não há exportação de jogo nem player autônomo.
- Sensor **por colisor** não existe: o sensor pertence ao corpo inteiro.
- `CharacterVirtual` não participa da broadphase como corpo rígido; sensores e
  consultas **não** acertam o personagem.
- Da entrega B existem o registro com GUID, a identidade de malha e a
  importação de GLB pelo aparelho. **Não** existem: renomear/mover/apagar pelo
  painel de arquivos, cenas reutilizáveis, e texturas na importação.
- Da entrega C, só a Luz anexável e o override de material **por instância**
  existem. MaterialAsset compartilhado, slots por submesh e pré-visualização
  isolada de material **não** existem.
- Sombra de luz pontual e de spot **não** existe: não há passe para elas.
- A grade ainda atravessa faces quase verticais em ângulos rasantes; só a
  oclusão por faces horizontais e vista de cima está verificada.
- Nada aqui foi medido em FPS nem exercitado por sessão prolongada.
