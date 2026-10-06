# attachsEngine: autoria de UI, ferramentas ImGui e controle de player

NÃO IREI SER SIMPLISTA NO DESIGN.

## Objetivo e definição do universo

Permitir criar um HUD, menu, inventário, formulário, ferramentas, joystick e
interface em mundo 3D sem depender de retângulos obrigatórios, scripts de
conexão repetidos ou alterações no renderer para cada controle. O universo
de referência é Unity uGUI 2.0, Input System 1.11.2 e Godot 4.5, com bibliotecas
especializadas para texto, layout e ferramentas. O plano cobre também as
capacidades adicionais que esses sistemas exigem: recursos, foco, acessibilidade,
animação, temas, binding, diagnóstico e empacotamento.

Nenhum roadmap prova que nunca será necessário acrescentar algo. O objetivo
prático é estabilizar contratos extensíveis e fechar famílias completas, com
exemplos de aceite. Cada pacote abaixo é **planejado**, salvo os itens
explicitamente verificados como atuais. Não publicar nomes futuros no menu.

## Auditoria do que existe

| Área | Evidência no checkout | Situação real |
|---|---|---|
| UI de jogo | `native/ui/gui_document.h/.cpp` | 10 tipos; documento tipado, anchors, clipping, HBox/VBox/Grid, imagem, eventos Click/ValueChanged |
| Ferramentas | `native/ui/immediate_gui.*` | Dear ImGui oficial 1.91.9b-docking, não paridade com docking/multi-viewport |
| Recursos | `gui_images.*`, `EditorSession::refreshGuiImages` | PNG/JPEG/KTX2, um atlas 2048², 64 fontes; uploads por revisão |
| Canvas | `gui_world.*`, renderer Vulkan | um documento/canvas ativo; plano com transform, profundidade e picking geométrico |
| API/arquivo | `Gui.cs`, `script_bridge.cpp` | ABI 41 e AEUI 4; leitura AEUI 1/2/3; Play isolado |
| Aparência | `GuiRuntime::draw` | fundo opcional, transições Normal/Pressed/Disabled com pose e tinta; trilho e padding do desenho ainda fixos |
| Texto | medidas atuais/font atlas | uma linha; sem recursos de fonte por controle, shaping, wrap ou seleção |
| Entrada | `GuiRuntime::pressed_ / pointer_` | apenas uma captura simultânea, sem foco/teclado/gamepad no documento de jogo |
| Personagem | `ScenePhysics::start`, `ToggleCharacter` | cápsula própria; incompatível com Body/Collider no mesmo objeto e ancestral físico móvel |
| Cilindro | `editor_creation_catalog.h` | cilindro visual e cilindros físicos existem como receitas diferentes; o dinâmico físico não traz malha visual |

O problema é estrutural: comportamento e desenho estão unidos no switch do
tipo. Ter dez controles não significa oferecer dez sistemas de skin completos.
Transparência, hit testing e participação em layout precisam ser independentes.

Correção inicial desta rodada: manter a opacidade autorada durante Pressed,
oferecer `Desenhar fundo` e alpha/hex no Inspector. Isso elimina um bloqueio,
mas **não implementa** a arquitetura de temas e skins abaixo.

### Pacote transversal: visual + interação + animação

O caso **criar Image e atribuir clique/animação** é um aceite explícito das
famílias E, J e L, anterior ao restante de P5. A capacidade não depende de
transformar Image em Button: cada node tem dados opcionais de interação e pose.
O mesmo vale para texto, painel e containers, preservando os controles com
entrada própria. Transparência do fundo não interfere na entrada.

Implementação desta continuação: `GuiInteraction` com clique e ação/alvo por ID;
evento normal para `Gui.Poll()` e ações nativas de visibilidade, habilitação,
valor e iniciar/parar animação. `GuiMotion` interpola deslocamento XY, escala
uniforme e opacidade entre duas poses; duração, atraso inicial, quatro curvas,
loop e ida/volta têm consumidores reais. Filhos herdam a pose; clipping e hit
testing usam a geometria animada; opacidade zero deixa de interceptar cliques.
Reprodução reutiliza o layout medido. Preview/Play não alteram o arquivo autoral.

Inspector oferece Interação e Animação contextual, presets que preenchem dados
reais, alvo na árvore, diagnóstico de alvo removido/incompatível e preview.
API C# expõe `Interaction`, `Animation`, `OnClick`, `PlayAnimation`,
`StopAnimation` e `Gui.Diagnostic`. Duplicar subárvore remapeia alvos internos;
alvos externos permanecem explícitos. Remover elemento libera sua reprodução e
descarta eventos pendentes de IDs removidos. Salvar usa AEUI 4; ler AEUI 1/2
mantém imagens antigas decorativas, sem inventar ações ou autoplay.

Continuação aplicada: até 16 ações adicionais por elemento, em ordem, filtradas
por Click/ValueChanged. A ação principal de clique permanece compatível e roda
primeiro. A lista possui criação, edição, remoção e reordenação no Inspector e
na API; duplicação remapeia cada alvo interno. Alteração de valor por ação pode
gerar outro evento; a fila de despacho limita ciclos sem recursão e informa erro.
Alvos removidos/incompatíveis não impedem a execução das ações seguintes.

`GuiTransitions` define Normal, Pressed e Disabled, cada um com deslocamento,
escala uniforme, opacidade e tinta multiplicativa; duração e curva são comuns.
A transição interrompida parte da aparência corrente. Pressed usa captura
estável durante o gesto; sair da área, soltar ou cancelar retorna ao Normal.
Disabled deriva da habilitação efetiva, incluindo ancestrais. A pose de estado
compõe com a animação, sem escrever a autoria ou forçar um fundo. O Inspector
edita um estado ou ação por vez e mantém os grupos recolhíveis. Dois ícones
próprios são gerados e empacotados no atlas real.

**Estado das famílias: parcial.** Ainda faltam captura por alpha/forma,
Hover/Focus/Selected, foco e múltiplos ponteiros, timeline/keyframes,
rotação/escala por eixo, sprite animation, callbacks de término, pausa/retomada,
relógio configurável e redução de movimento. Um tween de pose não equivale a
AnimationPlayer ou ao Animator. Esses itens continuam em E/J, sem menus fictícios.

Aceite: importar PNG → criar Image → ativar Clicável → escolher ação/alvo →
configurar animação → Interagir → clique com geometria transformada → salvar →
reabrir → Play → receber o evento por C#. Verificar fundo zero, herança de pose,
Undo/Redo, target removido, referência duplicada e Stop sem modificar autoria.
Exemplo executável: `examples/ui/behavior.aeui` + `GuiImageActions.cs`.
Evidências e limites: `docs/validacao/UI-IMAGEM-COMPORTAMENTO-2026-10-03.md`.
Novo aceite e limites: `docs/validacao/UI-ESTADOS-ACOES-2026-10-03.md`.
Exemplo AEUI4: `examples/ui/states-actions.aeui` + `GuiStatesActions.cs`.

Referências concretas: [Godot 4.5 TextureButton](https://docs.godotengine.org/en/4.5/classes/class_texturebutton.html)
e [source 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/scene/gui/texture_button.cpp)
separam resposta do botão e apresentação da textura; o hit mask é uma capacidade
adicional, ainda pendente aqui. [Godot 4.5 Tween](https://docs.godotengine.org/en/4.5/classes/class_tween.html)
e [source](https://github.com/godotengine/godot/blob/4.5-stable/scene/animation/tween.cpp)
orientam interpolação numérica e ownership de reprodução. [Unity uGUI 2.0,
Selectable transitions](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/script-SelectableTransition.html)
mostra transições visuais associadas a interação; a adaptação aqui compõe
capacidade sem impor hierarquia ou visual de Unity.

## Referências estudadas e decisões

| Referência versionada | Princípio extraído | Adaptação |
|---|---|---|
| [Godot 4.5 Control](https://docs.godotengine.org/en/4.5/classes/class_control.html) e [source](https://github.com/godotengine/godot/blob/4.5-stable/scene/gui/control.cpp) | layout, foco, input e tema têm contratos distintos | aparência vazia não elimina interação; foco e propagação explícitos |
| [Godot 4.5 StyleBoxEmpty](https://docs.godotengine.org/en/4.5/classes/class_styleboxempty.html) | um estilo pode desenhar nada | `None` é um skin válido em qualquer estado |
| [Unity uGUI 2.0](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/index.html), [Selectable source](https://github.com/Unity-Technologies/uGUI/blob/main/com.unity.ugui/Runtime/UGUI/UI/Core/Selectable.cs) | seleção e transições referenciam o visual | comportamento usa partes nomeadas; não colore o controle inteiro obrigatoriamente |
| [Godot 4.5 NinePatchRect](https://docs.godotengine.org/en/4.5/classes/class_ninepatchrect.html) | preservar cantos e configurar o centro | 9-slice, regiões, bordas e centro opcional em recurso de skin |
| [Unity ScrollRect source/documentação](https://github.com/Unity-Technologies/uGUI/blob/main/com.unity.ugui/Documentation~/script-ScrollRect.md) | viewport, conteúdo, scrollbars e física de rolagem separados | clipping e virtualização antes de listas grandes |
| [Input System 1.11.2 On-screen Controls](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/OnScreen.html) | controle touch produz entrada sem visual predefinido | joystick usa imagens escolhidas e action/receiver tipados |
| [Godot 4.5 CharacterBody3D](https://docs.godotengine.org/en/4.5/classes/class_characterbody3d.html) | controlador de personagem é uma modalidade física própria | distinguir motor Character de motor de corpo dinâmico |

O source Unity em `main` é material de investigação, não dependência fixada.
Se código externo for incorporado, registrar tag/commit, licença e alterações.

### Vídeos e workflows

[Série oficial Unity 6 Input System, publicada em 13/06/2025](https://unity.com/resources/input-system-video-tutorial-series):
Actions editor, personagem, controles mobile, navegação UI, rebinding e multiplayer.
O fluxo adotado será criar controle → escolher ação → escolher receiver → testar
→ salvar. A separação permite trocar joystick por gamepad sem refazer o player.

[Unity Learn Worldspace UI, 2019.4](https://learn.unity.com/tutorial/creating-a-worldspace-ui?version=2019.4):
composição/configuração e verificação na cena. A composição 2D continuará útil,
mas haverá manipulação do plano na cena e acesso direto à sua câmera.

Limite da pesquisa: páginas e descrição dos vídeos consultadas; não declarar
revisão integral frame a frame. Antes de redesenhar o editor, registrar vídeo
ou capturas de criação/edição em aparelho e contar interações no fluxo equivalente.

## Arquitetura mínima que sustenta a expansão

```text
GuiDocument: identidade/hierarquia + dados autorais tipados
 ├─ Layout: medida, anchors, constraints, transform local
 ├─ Appearance: partes e skin por estado; tema/recurso
 ├─ Behavior: controle e propriedades sem visual obrigatório
 └─ Binding: propriedade/evento/action/receiver com tipo e identidade
       ↓ AEUI versionado + migração + validação transacional
GuiRuntime: layout/cache + estado de foco + capturas por ponteiro
 ├─ draw -> UiDrawList -> instâncias/Vulkan
 ├─ input -> hit/foco/captura -> eventos/action contributions
 └─ bindings -> consumidores reais do mundo/recursos

GuiWorkbench + histórico editam os dados autorais
Dear ImGui usa seu contexto próprio para ferramentas de autoria
```

Não criar managers para cada caixa. Expandir estruturas existentes e introduzir
somente recursos que precisam de compartilhamento: Theme, Sprite/Font e Template.
Estado de Pressed/foco/rolagem é runtime; referências e defaults são authoring;
medidas e geometria são caches. Não salvar ponteiro, GPU handle ou ID de janela.

AEUI 4 já persiste as ações ordenadas e transições; AEUI 1/2/3 continuam legíveis
com defaults documentados. A apresentação por entidade usa o componente
`astra.ui.canvas` v1 no arquivo de cena e uma referência GUID a `UiDocument`;
não exige regravar o documento de UI em outro formato. ABI 42 acrescenta o
endereçamento por instância sem deslocar os campos anteriores. Uma próxima
versão de AEUI dependerá do contrato de skins/layout/hit e de migração testada.
Manter IDs, referências remapeáveis e erros por campo. Estruturas novas da ABI
são anexadas com layout/versão verificados.
Extensões não reconhecidas geram erro explícito, nunca controle vazio silencioso.

## A — Aparência livre, temas e partes

Contrato: `None`, Solid, Gradient e Sprite por parte; opacidade independente;
ARGB, border width/colors, radius por canto, padding do conteúdo, alinhamento,
ícone/label opcionais, fontes e escalas. Sprite usa region/trim/pivot/9-slice.
Separar background, border, content, overlay, track, fill, handle e checkmark.

Estados: Normal, Hover, Pressed, Selected, Checked, Focused, Disabled, Error,
Loading; transições None, troca de skin, tinta ou animação explicitamente escolhida.
Um Button com `None` em todos os estados permanece sem retângulo e clicável.
Não inventar overlay quando não há skin de Pressed.

Tema de projeto → tema de subárvore → variante → override local. Inspector
mostra origem de cada valor, reset local, prévia de estados e propriedades
condicionais. Partes compartilhadas usam recursos; edição local pode criar
override sem alterar os outros controles.

Aceite: criar botão só de ícone, botão só de texto e slider com imagens próprias;
pressionar/desabilitar, trocar tema, Undo, salvar/reabrir e Play conservam a skin.
Diagnóstico aponta parte/recurso ausente e herança circular.

## B — Imagem, vetor e recursos visuais

Sprite: região de atlas, trim, pivot, pixels/unidade, filtro, repeat, mipmaps,
cor/sRGB, 9-slice e bordas independentes. Image: Stretch/Contain/Cover, native
size, tile, fill horizontal/vertical/radial, origem/direção e porcentagem.
Decorative Image pode ignorar input; IconButton referencia imagem em um visual filho.

Recursos derivados: SVG tessellado/rasterizado com cache, SDF/MSDF para ícones
quando houver benefício medido, render texture, camera preview e vídeo real.
Não anunciar SVG/vídeo apenas porque há um campo de caminho.

Atlases por páginas, orçamento e revisão; upload/decode fora do hot path;
evicção só quando a página não possui consumidores e fence liberou a textura.
Erro de decode e arquivo removido não mantêm o recurso anterior disfarçado.

Aceite: trocar asset, mudar região/filtro/9-slice, reload, unload, DPI e canvas
3D. Export incluir só as dependências necessárias, não todas as fontes da biblioteca.

## C — Layout e transformações

Adicionar margin, padding independente do skin, min/max/preferred, percentuais,
expansão, baseline, aspect ratio, auto-size com política de overflow, flow/wrap,
grid por colunas/linhas fixas ou células responsivas, overlay/stack e split.
Transform 2D inclui pivot, posição, escala e rotação; anchors não substituem isso.

Resolução de referência, escala DPI, safe area, orientação, teclado aberto,
breakpoints e regras por perfil. Separar limite autoral de resolução do framebuffer.
Texto muda a medida intrínseca; skin também pode possuir margens mínimas.
Proibir ciclos entre fitter/parent e registrar quem determinou a medida final.

Inspector: desenhar margens/anchors no canvas, presets editáveis, alinhar/
distribuir/match-size, multi-seleção, guides/snapping e bloqueio de fluxo explicado.

Aceite: mesmo inventário em telefone portrait/landscape e tablet, com teclado e
texto traduzido; resize não perde foco/captura nem causa recomposição infinita.

## D — Texto completo e edição

Font resource, fallback por script, weight/italic, tamanho, line spacing,
letter spacing, wrap, overflow clip/ellipsis/scroll, align/baseline, rich text,
links, inline images, seleção/cópia e conteúdo localizado. Shaping e bidi precisam
existir antes de afirmar suporte árabe ou combinações complexas de Unicode.

InputField: caret, seleção por grapheme, composição IME, keyboard type, password,
limites, multiline, read-only, placeholder, validação e mensagens de erro;
Undo de texto não conflita com Undo estrutural. Enter pode submeter ou inserir linha.

Editor preview deve usar o mesmo font/shaping do runtime, com recursos de font
empacotados, fallback ausente visível e cache por runs/face/DPI.

Aceite: acentos, emoji disponível no fallback, árabe, seleção touch, composição
Android, paste, abertura do teclado, foco mantido e save/reload da configuração.

## E — Eventos, foco, touch e acessibilidade

Capturas por `(device, pointerId)`; down/up/move/enter/leave/cancel, click/double,
long press, drag/drop, wheel e gestos com arbitragem. Hit shape retângulo/círculo/
polígono e, opcionalmente, alpha por recurso. Opacidade visual não decide hit.
Input policy Block/Pass/Ignore e ordem de propagação documentadas.

Foco e navegação Auto/Explicit, direções, tab order, submit/cancel, repeat,
modal scope, gamepad, mouse e teclado. Remover/ocultar/desabilitar cancela donos;
perda de foco do app zera contribuições e restaura estado previsível ao voltar.

Nome acessível, role, estado/valor, descrição, foco visível, tamanho de toque,
contraste e reduced motion. TalkBack requer integração Android real.
Som/haptic opcionais por evento, recursos e política de intensidade.

Aceite: joystick + botão + scroll simultâneos; modal bloqueia mundo; gamepad
navega sem touch; nenhum Up perdido mantém ação pressionada.

## F — Joystick e conexão com player

Joystick Fixed/Floating/Dynamic: base/knob sem skin obrigatória, raio, alcance,
deadzone, curva/sensitivity, eixo livre/Horizontal/Vertical, normalização,
return speed e tempo real/escalado, origem limitada a região e safe area.
Saída tipada Vector2 com debug do vetor e pointer dono.

Binding: action `Mover`/`Olhar` escolhida no catálogo real, player/receiver por
ID persistente e política de espaço World/CameraRelative/Local. Dois players
não compartilham entrada implicitamente. Contribuições de teclado/gamepad/touch
possuem combinação definida, prioridade e cancelamento por dono.

Consumidores: CharacterMotor, DynamicBodyMotor, Vehicle/Script receiver. Joystick
não escreve Transform de Body dinâmico. Move/Look/Saltar/Interagir também podem
ser alimentados por OnScreenButton; editor oferece picker do alvo, valida tipo,
destaca alvo/câmera e mostra conexão no canvas.

Aceite vertical prioritário: criar joystick → apontar player → arrastar com
um dedo enquanto outro aciona Saltar → soltar/perder foco zera entrada →
salvar/reabrir conserva o alvo. Excluir player produz referência inválida visível.

## G — Player, cilindro e autoridade física

Hoje `ScenePhysics` recusa Character com Body/Collider e limita a 32 personagens;
`ToggleCharacter` também recusa objetos com Body. O cilindro visual criado como
primitiva contém física, e a receita `physics.dynamic_cylinder` é só física.
Esses caminhos não deveriam parecer equivalentes para quem cria um player.

Fluxo proposto: ação contextual **Controlar como personagem**, com duas opções:

1. **Motor Character:** transação preserva visual/material/filhos em um filho
   visual, cria root com cápsula própria, migra referências acordadas, configura
   câmera/ações; colisão visual antiga não permanece escrevendo uma segunda pose.
2. **Motor dinâmico:** conserva Cylinder Body/Collider e usa forças/impulsos,
   controle de velocidade, ground probe, salto, torque/rotational locks e limites.
   O Shape permanece Cylinder. Não o apelidar de CharacterVirtual.

A primeira entrega deve oferecer Character + qualquer malha visual e receita
completa CylinderDynamic + DynamicBodyMotor. Laterais mostram autoridade atual,
motivo de incompatibilidade e ação de conversão, em vez de menu silenciosamente
desabilitado. Undo restaura o conjunto inteiro; Play não altera authoring.

Aceite: cilindro controlável, chão/rampa/parede/plataforma, câmera, joystick,
salto, save/reload, respawn e remoção. Medir motor em 30/60/120 Hz; nenhuma
autoridade grava pose duas vezes. Conversões não descartam dados sem diagnóstico.

## H — Controles básicos com skins completas

| Família | Propriedades necessárias | Consumidor/aceite específico |
|---|---|---|
| Button/IconButton/RepeatButton | partes icon/label, press/hold/repeat, toggle opcional, tooltip, foco | evento só no gesto válido; Repeat cancela no disable/remove |
| Toggle/Radio/ToggleGroup | seleção exclusiva, allow-none, valor, checkmark, grupo por ID | mudança atômica; remover selecionado segue política explícita |
| Slider/RangeSlider/Progress | direção, steps, min/max/value, track/fill/handles, drag por handle, limites cruzados | valor e fill corretos em todos os sentidos e sem fundo |
| Scrollbar/ScrollView | conteúdo/viewport, eixos, posição, inertia, clamped/elastic, scrollbar visibility | rolagem aninhada transfere gesto; bounds após resize |
| InputField/TextArea | contrato D, validator, eventos edit/submit/cancel | IME Android e persistência da configuração |
| Dropdown/Combo/SearchSelect | opções com IDs/ícones, popup, seleção, filtro e empty state | popup fora do clip do pai com modal/foco corretos |
| Tabs/Accordion/Disclosure | selected ID, content references, lazy content e keyboard | conteúdo inativo não recebe input; histórico autoral |
| Tooltip/Popover/Dialog/Menu | anchor target, placement, dismissal, focus trap, stacking | fecha/devolve foco e respeita safe area/teclado |
| ColorPicker/Numeric/Vector/Curve | typed ranges, parsing, clamping, drag transaction, units | API/model/runtime mantêm tipo; edição inválida não altera valor |

Cada linha exige modelo, preview autoral, runtime, histórico, arquivo, API,
diagnóstico e um cenário integrado. Coleção de nomes é pesquisada, não implementada.

## I — Coleções, inventário e dados

List/Grid/Tree/Table com chave estável, seleção simple/multi, reorder, drag/drop,
colunas, sort/filter e virtualização. Reciclar visual não troca identidade de dado,
foco ou captura. Atualização em lote evita remontar toda a árvore por item.

Template de célula com bindings tipados, empty/loading/error, paginação e lazy
resource. Inventário é composição dessas primitives; slot/stack/drag valida o
modelo de inventário real, não apenas move um ícone pela tela.

Bindings one-way/two-way e eventos com enum/number/vector/string/resource/entity
e compilação de caminho tipada; alteração de schema invalida ligação visivelmente.
Referência visual não vira fonte de verdade da vida/munição do player.

Aceite: 10.000 itens com visible range pequeno, atualização parcial, reorder,
scroll/popup, filtragem e load/reload sem perder seleções ou vazar recursos.

## J — Animação e estados

Clips/tracks tipados de cor, opacidade, layout offset, escala, rotação e valor;
easing, duração, delay, loops, reversible, scaled/unscaled clock, cancel policy.
Timeline autoral e curvas; state transitions podem disparar clips existentes.
Não interpolar IDs nem relayout toda a subárvore quando só a tinta mudou.

Modal, fade e loading usam o mesmo sistema. Conflitos entre script, binding e
animação possuem prioridade/dono explícitos. Remoção mata tracks e callbacks.
Reduced motion conserva resultado/semântica sem depender do movimento.

Aceite: abrir/fechar inventário, interromper transição, Stop, reload, Undo e
mudança de orientação preservam o estado coerente sem callback fantasma.

## K — Canvas múltiplos, mundo e renderização

Canvas screen/world e camera-linked, sorting/layer/order, escala local, objeto
alvo, face-camera, clipping/occlusion policies, camera target e visibilidade por
viewport. Mundo permite gizmo na cena, vários painéis e foco/ray por câmera.

Scissor retangular atual → rounded mask → stencil/alpha masks quando necessário.
Custom materials, blend modes e effects precisam de pipelines/recursos reais:
outline/shadow/blur/backdrop, com orçamento por resolução e sem full-screen
render target automático para cada controle.

World UI unlit é default; iluminação opcional não pode ignorar o material.
Mips, antialias de texto e alpha hit têm política por distância. Plane picking
não pode assumir um único canvas ou única captura.

Aceite: HUD + dois painéis 3D, câmeras diferentes, depth, masks, resize e resource
reload. Registrar limite de planos/atlases, custos e aproximações de picking.

## L — Editor de autoria e integração ImGui

NÃO IREI SER SIMPLISTA NO DESIGN.

Proposta estrutural: árvore de **composição**, visualização de **estados** e
**conexões** são modos da mesma superfície, não três painéis permanentes. No
telefone, canvas dominante e inspector em rota/sheet com histórico; desktop
pode usar áreas fixas. Clicar joystick oferece imediatamente Action/Receiver.

Criar por templates funcionais; drag asset para Image/IconButton; multi-select,
group/reparent, copy/paste, align/snap, hierarchy search, breadcrumbs, lock/hide,
undo da composição inteira. Componentes avançados só mostram campos relevantes.
Skin editor isola partes e estados; preview por resolução/idioma/foco.

Dear ImGui é superfície de ferramentas, não formato do jogo. Docking só após
implementar ownership/persistência de layouts; charts/curves/node graphs usam
plugins específicos após avaliação. Texturas ImGui externas precisam de registro,
ownership e backend; o suporte atual ao atlas não basta.

Aceite UX: joystick conectado sem escrever código; skin sem retângulo; inventário
reutilizável; captura real com teclado aberto e erro de recurso. Contar cliques,
testar touch targets e revisar identidade. Conceito gerado não prova implementação.

## M — Templates, persistência, extensões e entrega

Templates reutilizáveis com overrides, instâncias, referências e migração;
IDs estáveis e duplicate remapeando bindings internos. Suportar prefab/project
dependências reais e resource import settings. Detectar ciclos e override órfão.

Extensões tipadas registram schema, propriedade, serializer, editor, runtime
consumer, input e lifecycle. Custom draw usa comandos existentes ou um recurso
de pipeline registrado. Não aceitar `Map<string,Any>` como sistema universal.

Cook/export resolve dependências, exclui editor state/source, valida recursos e
capabilities por plataforma. Ao faltar backend, recusar build com erro acionável.
Automated recipe creation e API authoring devem usar os mesmos comandos/histórico.

Aceite: template compartilhado, override, atualização do original, recurso ausente,
arquivo anterior migrado e projeto exportado com UI idêntica à sessão Play.

## N — Diagnóstico, desempenho e limites

Inspector de layout mostra measured/assigned rect, fonte de cada tamanho,
clip chain, skin resolvido, focus owner, captures e action contributions.
Debug mostra próximo receiver e por que evento foi consumido/recusado.
Resource panel mostra páginas, bytes, references, reload/error e GPU lifetime.

Separar dirty de dados/layout/text/draw/binding; indexar filhos/hit para escala;
cache de texto e templates; budgets de nós, clips, glyphs, sprites e draw calls.
Uploads após fence e liberação após último consumidor. Nada de polling/decode
por controle por frame. Instrumentar CPU/GPU/allocations e thermal no Android.

Metas são propostas a medir, não resultados: menu 200 controles sem alocação
continuada no hot path; lista 10.000 itens sem 10.000 visuals; dois ponteiros
sem ação presa; limites de atlas e glyphs documentados antes de ampliar budgets.

## Bibliotecas/plugins: aproveitar trabalho pronto com critério

| Biblioteca oficial | O que pode acelerar | Decisão e condição |
|---|---|---|
| [RmlUi](https://github.com/mikke89/RmlUi), [render interface](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/interfaces/render.html) | CSS/flex/scroll/text/events | candidato de backend amplo; spike isolado com Vulkan, Android IME, world canvas, serialização e autoria; não substituir toda engine só para obter estilos |
| [Yoga](https://github.com/react/yoga) | layout flex/web | candidato para flow/wrap/constraints; adaptar um único modelo autoral e medir cache/allocations; não manter dois layouts concorrentes |
| [HarfBuzz](https://github.com/harfbuzz/harfbuzz) + [FreeType](https://freetype.org/) | shaping e font glyphs | caminho preferido para D; bidi/line break/fallback ainda precisam de solução e revisão de licenças/version pin |
| [Dear ImGui extensions](https://github.com/ocornut/imgui/wiki/Useful-Extensions) | catálogo de tooling | fonte de candidatos, não garantia de suporte da engine |
| [ImPlot](https://github.com/epezent/implot) | profiling/curvas/charts | só ferramentas; orçamento de triângulos e texturas do backend |
| [imgui-node-editor](https://github.com/thedmd/imgui-node-editor) | editor de conexões | editor visual do grafo tipado; plugin não executa bindings por si |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) | transform canvas e ferramentas | investigar interação touch versus gizmo existente; não duplicar autoridade de transform |
| [meshoptimizer](https://github.com/zeux/meshoptimizer) | otimização dos ícones GLB | offline, quando triangulação/escala justificarem custo; não necessário para o primeiro catálogo |
| Blender 4.5.0, `tools/asset-tools.lock.json` | FBX → GLB e bake transparente | processo offline independente; hash fixado; não embutir Blender no Android |

Para cada candidato: licença, tag/commit, source disponível, release health,
Android build, input/IME, resource ownership, size/memória, desempenho e custo
de manutenção. Escolher RmlUi versus expansão nativa após dois exemplos idênticos
executados: menu com skins/texto e inventário virtualizado. Uma decisão dessa
altera o formato/workflow e deve ser apresentada para aprovação com o spike pronto.

## Ordem executável e gates

| Pacote | Entrega vertical | Dependências | Gate para considerar pronto |
|---|---|---|---|
| P0 | transparência e auditoria + biblioteca POLYGON | renderer/documento atual | zero fundo no Pressed, arquivo preservado, catálogo real |
| P1 | skins/partes/temas + sprite/9-slice | A+B | três controles com visuais próprios em todos os estados |
| P2 | multi-pointer + joystick/action/player | E+F+G mínimo | player cilíndrico controlável, segundo dedo, foco e persistência |
| P3 | layouts responsivos + font/shaping/IME | C+D | portrait/landscape, multiline, idioma e teclado |
| P4 | scroll/list/seleção/popup/input | H+I sobre P1–P3 | inventário e formulário completos, virtualizados |
| P5 | animação/masks/canvas múltiplos | J+K | HUD e dois painéis 3D animados com clipping e input |
| P6 | templates/bindings editor/extensões/export | L+M | criar/reusar/exportar sem repetir código de ligação |
| P7 | escala/acessibilidade/diagnóstico | N+E em todos os anteriores | medidas e erros públicos; TalkBack e carga representativa |

Pesquisa e trabalho de P7 começam antes; gate de release vem depois. Não criar
todos os enums/menus de P1–P7 antes de um cenário funcionar. Cada pacote registra
implementado/parcial/planejado e modifica a contagem real. Prazo deve ser derivado
do primeiro pacote medido; não prometer datas para adapters ainda desconhecidos.

## POLYGON: fonte e uso completo

Pacote local fornecido: `poly.unitypackage`, POLYGON Icons Pack v1.01.
Extração mantém 520 FBX, 520 prefabs, 17 materiais, 16 texturas, uma cena e um
lighting asset, com meta/GUID e hashes. `tools/import-synty-icons.py` cria
catálogo e conserva as prévias originais: elas são opacas e não devem ser
confundidas com sprites transparentes ou conversão de prefab.

Destino local: `local-assets/synty-polygon-icons-v1.01/`, excluído do Git público.
Todos os 520 ícones entram na biblioteca do projeto, com paginação para respeitar
o atlas de 64 fontes. Fonte/material/textura são preservados e devem poder ser
selecionados por uso; colocar 520 desenhos simultâneos em uma tela não é útil.
O bake a partir do FBX produz PNG RGBA e GLB rastreáveis ao prefab/material.
Cena/lighting Unity ficam como fontes; não fingir importação desses formatos.

Nesta rodada: 520 renders transparentes de geometria real e 520 GLBs derivados,
22 páginas AEUI editáveis e comportamento C# de paginação. As 16 paletas têm
derivados 1024² para UI e modelos; fontes 4096² permanecem intactas. A conversão
mantém geometria, UV e a ligação do material do prefab; não traduz um shader
Unity arbitrário. Metallic 0/roughness 0.8 correspondem aos materiais dos ícones
fornecidos (Glossiness 0.2), com textura do `_MainTex` resolvida por GUID.

Projeto pronto: `local-assets/SyntyIconLibrary-20261003/`. Abrir como projeto da
engine; editar `UI/main.aeui`; em Play usar Anterior/Proxima. Para reutilizar,
atribuir `images/synty/<nome>.png` a um Image ou importar
`models/synty/<nome>.glb` pelo importador existente. Não confundir GLB com a
importação de comportamento de um prefab Unity.

O seletor de imagens passa de 256 para 4096 candidatos, com busca e lista
recortada por ImGuiListClipper. Varredura continua limitada a 8192 entradas;
atlas continua com 64 fontes simultâneas. Isso permite escolher os 520 ícones
e as 16 paletas sem anunciar atlas ilimitado. O popup acompanha a superfície
disponível quando o teclado Android abre.

Ferramentas reproduzíveis: `tools/import-synty-icons.py`,
`tools/bake-synty-icons.py` (Blender oficial 4.5.0, pin verificado em
`tools/asset-tools.lock.json`) e `tools/create-synty-ui-gallery.py`.
Recursos/licenças do pacote ficam locais, excluídos do Git público.
Resultado e evidências: [validação desta rodada](../validacao/UI-ROADMAP-SYNTY-2026-10-03.md).

## Cenários finais que fecham o roadmap

1. Menu acessível, traduzido e com skins próprias; gamepad/touch/teclado.
2. HUD com joystick, action buttons e player visual Cylinder; referência salva.
3. Player Cylinder dinâmico com forças, chão/rampas e câmera, sem dupla autoridade.
4. Inventário 10.000 itens: drag, stack, popup e dados persistentes reais.
5. Formulário multiline/IME/validação/undo e tema reutilizável.
6. HUD de tela + painéis em objetos 3D, profundidade, masks, animação e vários dedos.
7. Template e extensão de controle: authoring/API/arquivo/export sem patch de core.
8. Recursos apagados, foco perdido, Stop/reload e frame stalls sem input preso/vazamento.

Só considerar fechado o conjunto de capacidades após esses cenários e os gates
dos pacotes passarem. A quantidade de componentes, ícones ou métodos não substitui
nenhuma dessas provas.
