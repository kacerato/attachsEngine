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

## 11. ABI de scripts

v5. Campos de v2 nas mesmas posições; v3 acrescentou hierarquia/ciclo de
vida/componentes/propriedades/transform de mundo, v4 consultas e contatos, v5
ações de entrada. O lado gerenciado exige a versão corrente e confere `size`.

## 12. Luzes

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

## 13. O que este documento NÃO afirma

- Não há exportação de jogo nem player autônomo.
- Sensor **por colisor** não existe: o sensor pertence ao corpo inteiro.
- `CharacterVirtual` não participa da broadphase como corpo rígido; sensores e
  consultas **não** acertam o personagem.
- Recursos com GUID e importação GLB pelo Android são a entrega B, **não
  implementada**.
- Da entrega C, só a Luz anexável e o override de material **por instância**
  existem. MaterialAsset compartilhado, slots por submesh e pré-visualização
  isolada de material **não** existem.
- Sombra de luz pontual e de spot **não** existe: não há passe para elas.
- Nada aqui foi medido em FPS nem exercitado por sessão prolongada.
