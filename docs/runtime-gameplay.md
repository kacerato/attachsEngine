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

## 16. O que este documento NÃO afirma

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
