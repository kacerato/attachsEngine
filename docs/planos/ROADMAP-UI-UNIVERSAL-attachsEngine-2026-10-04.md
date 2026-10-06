# attachsEngine — UI universal integrada à cena

**Data:** 4 de outubro de 2026  
**Snapshot analisado:** `kacerato/attachsEngine`, commit `d12c2ea2ed4f19a690ed99410ed865407aec024a`.  
**Natureza:** auditoria estática e proposta de expansão. Não foram compilados APKs, executados testes ou alterados arquivos do repositório nesta análise. Resultados de testes mencionados nas fontes são relatos do projeto, não novas verificações.  
**Documento de origem a complementar:** `docs/planos/UI-ROADMAP-COMPLETO-2026-10-03.md`.

## 1. Decisão central

A interface precisa ser um recurso instanciável da engine, integrado à cena, ao Inspector, à API, aos prefabs, aos dados, à entrada e ao exportador. Não basta ampliar a janela de autoria ImGui ou acrescentar tipos ao `GuiKind`.

Manter a separação já existente entre ferramentas Dear ImGui e documento de jogo. Acrescentar a conexão que falta entre o documento de UI e o mundo: instâncias com identidade própria, hospedadas em objetos da cena, com referências persistentes, apresentação configurável e estado de execução isolado.

“Universal” não deve significar um componente com qualquer campo ou uma promessa de antecipar toda interface concebível. Deve significar capacidades ortogonais, composição, extensões tipadas e uma matriz verificável de combinações. Um caso novo dentro das capacidades existentes não deve exigir um novo switch no renderer.

### O primeiro resultado obrigatório

Criar uma imagem na árvore da cena, hospedá-la em um canvas filho de um cilindro dinâmico, editar suas propriedades, reproduzir a cena, tocar na imagem enquanto outro dedo controla o jogador, duplicar o conjunto, salvar e reabrir sem perder referências nem criar duas autoridades físicas.

Este resultado vem **antes** do acabamento de todas as skins. Ele comprova o alicerce sobre o qual as demais famílias serão construídas.

## 2. Auditoria do estado atual

| Área | O que foi observado | Consequência |
|---|---|---|
| Autoria | `GuiDocument` e `GuiNode` já têm IDs, pai, árvore, validação, histórico e arquivo. [S1, S6] | Não refazer uma árvore de UI do zero. Falta integração da árvore com objetos da cena. |
| Controles | Dez tipos: Panel, Text, Button, Toggle, Slider, Progress, Image, HBox, VBox e Grid. [S1] | Há base real, mas quantidade de tipos não mede completude de famílias. |
| Image interativa | Interação opcional, clique, ações ordenadas, animação simples e estados já existem. [S1, S2, S10] | Não planejar clique em Image como se fosse inteiramente ausente. |
| Canvas 3D | Plano com posição/rotação próprias, resolução, escala, projeção e picking; sem referência ao objeto hospedeiro. [S1, S3, S7] | Estar no mundo não equivale a acompanhar uma entidade física. |
| Instâncias | Um documento/canvas ativo no fluxo descrito; Play injeta um `GuiRuntime` em `ScriptBridge`. [S5, S7] | Falta um conjunto de instâncias independentes por mundo. |
| C# | Handles de UI identificam mundo e nó, sem identidade de instância de documento. [S4] | Duas instâncias do mesmo template precisam de endereçamento adicional. |
| Entrada | `pressed_`/`pointer_` únicos; um segundo Down é recusado durante a captura. [S1, S2] | Não fecha joystick + salto + olhar simultâneos. |
| Desenho | Padding de label, trilho e dimensões de handle possuem constantes em `GuiRuntime::draw`. [S2] | Skins independentes não se resolvem apenas expondo uma cor a mais. |
| Transparência | `hit(..., true)` exclui opacidade zero; uma captura já iniciada pode continuar. [S2, S10] | A nova política de hit independente de pintura exige migração explícita. |
| Eventos | Ações nativas têm alvos dentro do documento; C# pode consumir Poll. [S2, S4, S10] | Ações configuráveis de cena exigem ponte tipada; não afirmar ausência de toda ação possível via script. |
| Persistência/API | AEUI4 e ABI41; compatibilidade de leitura de formatos anteriores relatada. [S8, S10] | Corrigir trecho antigo do roadmap que ainda fala em introduzir AEUI3. |
| Texto | Roadmap registra texto de uma linha e ausência de fontes por controle, shaping, seleção e IME de jogo completos. [S8] | Digitar no Inspector ImGui não comprova InputField completo no jogo. |
| Exportação | O pacote documenta que exportação de jogo com esse sistema não está concluída. [S7, S8] | Validar host sem editor desde o início, não somente ao final. |

O roadmap existente já cobre A–N: aparência, imagens, layout, texto, eventos, joystick, motores, controles, coleções, animação, canvases, editor, templates/exportação e diagnóstico. A proposta abaixo preserva esse inventário, adiciona contratos entre as famílias e antecipa partes de K/M atualmente colocadas tarde em P5/P6. [S8]

## 3. Arquitetura proposta: um modelo, várias apresentações

Os nomes novos abaixo são propostas de contrato, não APIs já disponíveis.

### 3.1 Documento, instância e objeto hospedeiro

**Documento de UI:** asset compartilhável contendo árvore autoral, IDs locais estáveis, estilos, comportamento, defaults, interfaces públicas de dados e referências a recursos. Não contém a posição absoluta obrigatória de uma única cena.

**Instância de UI:** associação de um documento a um mundo e objeto hospedeiro. Guarda overrides, contexto de dados, jogador, câmera de entrada e apresentação. Cada instância possui seus próprios foco, captura, animação, seleção e rolagem em execução.

**Canvas na cena:** componente em entidade existente ou entidade própria, configurando o modo de apresentação. Pode ser filho de um objeto físico sem possuir corpo físico próprio. Uma instância não depende de uma aba de editor aberta.

**Runtime:** concilia instâncias com criação, remoção, habilitação, troca de documento e mudanças de cena. Recursos imutáveis e páginas de atlas podem ser compartilhados; estado de interação não.

### 3.2 Hierarquia unificada sem duplicar a fonte da verdade

Exemplo de autoria desejada:

```text
Cena
  Jogador
    Corpo físico + motor
    Visual
    Canvas de nome/vida
      BarraVida
      Nome
  TerminalFisico
    PhysicsBody + Collider + MeshRenderer
    Canvas do terminal
      Fundo
      BotaoAbrir
      TextoEstado
  HUD do jogador 1
    Canvas de tela
      JoystickMover
      AreaOlhar
      BotaoSaltar
  Inventario
    Canvas de tela
      ListaItens
      Preview3D
```

Os descendentes de UI podem ser entradas virtuais na árvore principal, adaptadas da árvore do documento. Não é necessário transformar cada letra, imagem ou handle em uma entidade física da cena. A seleção deve distinguir entidade, componente, instância de UI e nó de UI sem ambiguidade.

Arrastar um elemento dentro da mesma instância modifica sua árvore. Arrastar entre instâncias exige copiar/mover por transação, converter o referencial quando necessário e remapear referências. Arrastar um documento sobre uma entidade oferece adicionar uma instância de canvas; arrastar uma textura oferece Image ou uso visual apropriado ao contexto.

### 3.3 Identidade e referências

Separar identidade persistente de handle de execução:

```text
Referência persistente a UI:
  cena/asset + objeto hospedeiro + instância + ID local do nó

Handle de execução:
  geração do mundo + instância de UI + ID do nó + geração aplicável

Referência a propriedade:
  alvo tipado + componente/capacidade + PropertyId + caminho validado
```

Duplicação de uma instância cria identidade de instância nova. Referências internas acompanham os alvos duplicados; referências externas seguem política explícita. Não resolver vínculos por nome de exibição. Reutilização de IDs após remoção não pode validar handles antigos. Prefabs não devem capturar referências externas silenciosamente; pontos de conexão devem ser parâmetros expostos.

### 3.4 Dados autorais, estado e cache

| Categoria | Exemplos | Persistência proposta |
|---|---|---|
| Autoria | Documento, layout, tema, fontes, bindings, controlador alvo | Asset/cena/prefab versionados |
| Override de instância | Cor local, jogador, câmera, rótulo específico | Cena/prefab, com origem e reset |
| Estado de execução | Pressed, foco, composição IME, drag, animação em andamento | Não salvo automaticamente no asset |
| Estado de aplicação | Inventário, vida, preferências confirmadas | Modelo de jogo/save, não widgets |
| Cache | Retângulos resolvidos, glyphs, geometria, handles GPU | Recriável, nunca fonte de verdade |

A opção de persistir estado de UI deve selecionar explicitamente o que salvar: por exemplo, aba selecionada e rolagem, mas não ponteiro capturado ou senha digitada.

## 4. Matriz espacial e de integração com a cena

| ID | Possibilidade | Implementação/contrato necessário |
|---|---|---|
| SP01 | HUD fixo na tela | Canvas Overlay, safe area, ordem, escala e contexto de jogador. |
| SP02 | UI limitada a uma câmera ou tela dividida | ScreenCamera, viewport e câmera de desenho/entrada explícitos. |
| SP03 | Barra de vida acima de entidade | Âncora projetada mundo→tela; alvo, offset, distância, saída da tela e oclusão. |
| SP04 | Painel 3D parado | Canvas World com matriz da entidade hospedeira e tamanho físico. |
| SP05 | Painel preso a corpo dinâmico | Seguir pose final/interpolada usada pelo renderer; não escrever Transform físico. |
| SP06 | Painel em objeto cinemático | Seguir a autoridade cinemática e sua hierarquia, sem simulação duplicada. |
| SP07 | UI presa a osso/socket | Referência ao esqueleto/socket e pose avaliada; fallback se o osso sumir. |
| SP08 | Billboard | Face-camera total ou limitado por eixo, por câmera, com tamanho em mundo ou pixels. |
| SP09 | UI na superfície de uma malha | Render texture ou adaptador de superfície; picking por UV/triângulo e limites. |
| SP10 | UI enrolada em cilindro | Parametrização curva ou UV; costura, repetição, normal e raio mapeados. Não é o mesmo que SP05. |
| SP11 | Personagem 3D dentro do inventário | EmbeddedViewport, câmera/luzes e cena própria ou visão filtrada da cena existente. |
| SP12 | Minimap, monitor ou câmera de segurança | Câmera→render target→visual de UI; cadência, resolução e dependências de render. |
| SP13 | Imagem decorativa no mundo | SpriteRenderer, material em plano ou decal, conforme intenção; não exigir sistema de botões. |
| SP14 | Botão que cria objeto | Comando tipado de instanciar prefab, parâmetros, parent e posição; ponto seguro de mutação. |
| SP15 | Arrastar item do inventário ao mundo | Raycast da câmera correta, preview, validação do modelo e transação inventário+spawn. |
| SP16 | Arrastar objeto do mundo à UI | Resolver entidade/asset para payload tipado e validar receptor, não mover só textura. |
| SP17 | Várias instâncias do mesmo menu | Asset compartilhado, estados/IDs/dados isolados por instância. |
| SP18 | Dois jogadores | Contextos, dispositivos, canvases, câmeras e foco separados. |
| SP19 | UI persistente entre cenas | Hospedagem em escopo persistente, invalidação de referências e reconexão declarada. |
| SP20 | Popup fora do recorte do pai | Camada visual de popup, mantendo vínculo lógico, foco e destino corretos. |
| SP21 | Conteúdo remoto/assíncrono | Provider controlado, loading/error/retry, cancelamento e geração de destino. |
| SP22 | Preview no editor e jogo exportado | Mesmo documento, contratos e runtime; adaptadores de host distintos. |

Em SP09–SP12, impedir ciclos de render target, como uma câmera mostrar a própria tela recursivamente sem política. Exportar uma capability inexistente deve gerar erro acionável, não resultado preto silencioso.

## 5. Famílias e propriedades universais

Não mostrar todos os campos de todas as famílias para todo nó. Mostrar as capacidades presentes, com valores autorais e resolvidos separados. O contrato de propriedade é compartilhado por Inspector, API, arquivo, animação e binding.

### 5.1 Base, hierarquia e ciclo de vida

ID, nome, tags, pai, ordem, enabled local/efetivo, instância proprietária, origem do template, override local, lock de edição, visibilidade no editor e no jogo. Criação/remoção, duplicação, reparent, substituição de asset e habilitação precisam produzir notificações de ciclo de vida e cancelamento previsíveis.

Separar: participar do layout, produzir desenho, receber hit, permitir interação, participar do foco e aparecer na árvore acessível. Um controle decorativo pode desenhar sem capturar; uma região de gesto pode capturar sem desenhar. Uma caixa oculta pode reservar espaço ou colapsar, conforme configuração explícita.

A migração preserva a semântica antiga de opacidade/hit; projetos novos recebem uma política explícita. Não mudar de repente um fade para deixar botões invisíveis ativos em cenas existentes.

### 5.2 Layout e transformação

Anchors, offsets, pivot, posição local, escala XY, rotação, dimensões em unidades de UI, margin e padding independentes, mínimo/máximo/preferido, conteúdo intrínseco, aspect ratio, flex grow/shrink, alinhamento, baseline e prioridade de ajuste. Definir quais combinações o solver aceita e quem controla cada eixo.

Containers: HBox, VBox, Grid fixo/adaptável, Wrap/Flow, Stack/Overlay, Split e Scroll. Suportar aninhamento por necessidade, com orçamento configurável e detecção de ciclos; não criar limite arbitrário de dois ou três níveis. Invalidação deve alcançar o menor conjunto dependente possível.

Profiles: orientação, tamanho de janela, safe area, DPI, escala de fonte, localização e teclado aberto. Definir precedência de breakpoints, zero-size, overflow e filho fora do fluxo. Auto-size e parent-size não podem formar realimentação indefinida.

### 5.3 Aparência, skin e temas

Partes nomeadas: background, border, content, icon, label, overlay, focus ring, track, fill, handle, checkmark, caret e seleção. Partes opcionais e substituíveis, inclusive `None`. Um slider pode ter uma imagem para trilho, outra para preenchimento e outra para handle; botão pode ser só texto ou só imagem.

Por parte: solid/gradient/sprite, tinta, opacidade, bordas por lado, radius por canto, inset, padding de conteúdo, material suportado, sombra/contorno e clip. Tema de projeto→subárvore→variante→override local, com resolução determinística e origem visível.

Estados não são necessariamente mutuamente exclusivos: um Toggle pode estar Checked e Focused, ou Checked e Disabled. Resolver estados como conjunto de flags/regras com prioridade documentada; transições podem alterar partes específicas, sem pintar o controle inteiro. Normal, Hover, Pressed, Selected, Checked, Focused, Disabled, Error e Loading devem ter fallback explícito.

### 5.4 Imagens, sprites e recursos

Referência de asset estável, sub-recurso/região, tamanho intrínseco, pivot/trim, tint, UV, flip, filtro, repeat, espaço de cor, alpha, mipmaps quando aplicáveis e políticas de escala. Modos: native size, stretch, contain, cover, tiled, nine-slice e preenchimento horizontal/vertical/radial.

Nine-slice exige bordas L/T/R/B, centro opcional, stretch/tile de centro e bordas, política para retângulo menor que a soma das bordas e distinção entre margens visuais e de conteúdo. Sprite por estado e sprite animation são capacidades diferentes.

PNG/JPEG/KTX2 são o caminho já documentado. SVG, vídeo, render targets e fontes externas de textura entram somente com loader, consumidor, lifecycle e teste próprios. Atlas em páginas, quotas, dependências por referência, cancelamento e liberação após uso da GPU. Import settings não são o mesmo que propriedades locais do Image.

### 5.5 Eventos, gestos e ações

PointerDown/Move/Up/Cancel/Enter/Leave, click/double-click/long-press, wheel, drag begin/update/end/drop, value changed, focus/blur, submit/cancel, selection changed e eventos de animação. Definir ordem, propagação, consumo, reentrância e limites de fila.

Hit shape retangular, arredondado, círculo, polígono e máscara alpha opcional. A geometria visual transformada, o clip e o hit precisam usar referenciais compatíveis. Painéis do mundo também consideram oclusão, lado da superfície, câmera e distância.

Ações de cena devem vir de registro tipado: alvo Entity/Component/UiElement, comando e argumentos validados. Exemplos: ativar entidade, instanciar prefab, abrir painel, alterar propriedade autorizada, tocar som e encaminhar uma ação de entrada. Funções C# expostas devem declarar assinatura e lifecycle; um texto qualquer no arquivo não deve executar código arbitrário.

### 5.6 Foco, dispositivos e acessibilidade

Captura por dispositivo e pointerId, roteamento por viewport/jogador, prioridade entre UI/world/editor e gestos concorrentes. Remover, ocultar, desabilitar, abrir modal, mudar app focus ou desconectar dispositivo cancela os donos afetados.

Foco de teclado, foco de gamepad, captura de ponteiro e foco de acessibilidade são conceitos distintos. Tab, direções, navegação explícita/automática, wrap, repeat, submit/cancel e restauração de foco devem funcionar sem toque. Mouse+teclado e touch+gamepad não podem gerar cliques duplicados por tradução de eventos.

Nome, papel semântico, valor/estado, descrição, ações, ordem de leitura e foco visível integram o modelo. Contraste, tamanho de toque, escala de texto, reduced motion, remapeamento, alternativas a gestos e feedback não dependente só de cor são requisitos. No Android, uma árvore virtual acessível requer ponte de plataforma; não surge automaticamente por desenhar os controles. [E6]

### 5.7 Texto e entrada completos

Font asset, fallback, peso/estilo, tamanho, line spacing, letter spacing, alinhamento, rich text restrito, links, imagens inline, wrap, ellipsis, clip, scroll e seleção. Locale/direção não devem ser inferidos sempre da língua do editor. Traduções, plurais, números e troca de idioma precisam invalidar medidas corretamente.

Pipeline de texto com segmentação, bidi, runs de fonte/script/idioma, shaping, quebra de linha e rasterização/cache. HarfBuzz não substitui bidi e toda a edição de texto. [E2, E3, E4]

InputField/TextArea: caret, graphemes, seleção, copiar/colar/cortar, composição IME, placeholder, senha, read-only, limite por política explícita, multiline, teclado apropriado, autocorreção opcional, eventos edit/commit/submit/cancel. Enter pode inserir linha ou submeter; Back pode fechar IME antes de fechar o painel. Undo de digitação tem escopo diferente do Undo estrutural.

O adaptador Android deve sincronizar texto, seleção e faixa de composição e reposicionar o campo quando o teclado reduz a área útil. GameTextInput é candidato para casos de jogo e suporta seleção/composição, mas não fornece por si um editor completo. [E5]

### 5.8 Controles compostos e coleções

Button/IconButton/RepeatButton, Toggle/Radio/Group, Slider/RangeSlider, Progress, ScrollView/Scrollbar, Dropdown/Combo/SearchSelect, Tabs/Accordion, Tooltip/Popover/Dialog/Menu, Numeric/Vector/Color e editores especializados compartilham as capacidades anteriores. Evitar uma classe gigante e também dezenas de variantes que diferem apenas na skin.

List/Grid/Tree/Table precisam de chaves estáveis, seleção simples/múltipla, sort/filter/reorder, expansão de árvore, colunas, paginação e providers. **Listas normais, listas vazias e listas virtualizadas** são casos explícitos. Virtualização instancia a faixa visível e uma margem controlada, não todos os itens invisíveis.

Itens com altura variável, carregamento tardio de imagens, alteração de idioma, foco, captura e IME não podem ser transferidos para outro dado ao reciclar células. Um item focado pode precisar ficar vivo temporariamente ou ter sua edição confirmada/cancelada antes da reciclagem.

### 5.9 Inventário e formulários

Inventário é domínio de jogo com IDs e transações, representado pela UI. Operações: empilhar, dividir, equipar, trocar, transferir, ordenar, filtrar, usar e descartar. Drag visual acompanha uma transação candidata; só o modelo confirma a operação. Fonte removida, destino cheio, peso, compatibilidade e rejeição assíncrona não podem duplicar ou perder itens.

Formulários: campos tipados, dirty/pristine, touched, validação por campo e entre campos, erros, validação assíncrona cancelável, submit pendente/sucesso/falha, reset e restauração. Não gravar senhas em assets, logs de diagnóstico ou exemplos de captura. A UI exibe estados, mas não substitui autorização ou validação do sistema que recebe os dados.

### 5.10 Bindings e animação

Binding one-way, two-way e command com PropertyId estável, tipos, conversão explícita, valor padrão, tratamento de null, batch de alterações e origem. Uso de evento e escrita silenciosa precisa estar documentado para evitar loops; a diferença atual entre Value via API e SetValue por ação não deve desaparecer sem migração. [S10]

Animação: tracks tipadas, keyframes, curvas, sequências, paralelos, loops, reverse/ping-pong, delay, seek, pause/resume, callbacks, sprite frames e timeline autoral. Propriedades discretas não recebem interpolação numérica. Relógio real/escalado e política de pausa ficam explícitos.

Definir autoridade por propriedade: valor base vem de autoria/binding; interação e animação aplicam camadas resolvidas sem sobrescrever o asset a cada frame. Conflitos de múltiplos tracks têm replace/additive/blend onde matematicamente definido. Cancelar uma animação restaura o estado previsto, não um valor antigo que apaga uma mudança recente do modelo.

### 5.11 Renderização e canvas avançado

Sorting/layers/ordem, câmera de render e de input, visibilidade por viewport, depth/occlusion, duas faces, escala física, pixel density e atualização. UI World unlit como base; iluminação somente com material/renderer adequados. Alpha visual e política de oclusão de input são independentes e explicitamente combinados.

Scissor retangular, clips arredondados, stencil e alpha masks, inclusive aninhados e transformados. O hit precisa respeitar a mesma forma relevante. Outline, shadow, blur, backdrop e materiais customizados demandam budgets e passes reais; não criar automaticamente um render target por controle.

## 6. Joystick, jogador e física: contrato fechado

### 6.1 Fluxo de entrada

```text
Touch / teclado / gamepad
  -> roteamento por dispositivo, viewport e player
  -> contribuição identificada para InputAction
  -> agregação configurada por ação
  -> motor do jogador no passo adequado
  -> simulação física
  -> pose final / interpolação de apresentação
  -> câmera e ancoragem da UI
  -> renderização
```

O joystick controla uma ação, não conhece a implementação da física. Cada contribuição registra origem e dono; Cancel/remove/blur zera somente o que deve ser zerado. Definir soma com clamp, maior magnitude, prioridade ou dispositivo ativo conforme ação. Não aplicar uma regra genérica de soma para tudo.

Valores contínuos podem ser amostrados para cada FixedUpdate. Eventos como salto precisam de sequência/buffer e política de consumo para não desaparecer entre frames nem executar duas vezes quando há vários passos físicos no mesmo frame.

### 6.2 Propriedades do joystick

Modo Fixed/Floating/Dynamic; região ativa; base/knob opcionais e substituíveis; raio visual e raio de input; deadzone interna/externa; curva; sensibilidade; eixo livre/H/V; normalização circular/quadrada; retorno visual; referência de câmera e espaço world/local/camera-relative; ação e player/receiver. Mostrar vetor final e pointer dono no debug.

Arrasto de olhar pode ter sensibilidade diferente do movimento. Pinça, rolagem e joystick precisam de arbitragem, não roubar capturas em andamento sem Cancel. Ativar menu modal deve suspender as ações relevantes e restaurar navegação sem manter velocidade residual não desejada.

### 6.3 Dois modelos válidos de jogador cilíndrico

**Character com visual cilíndrico:** controlador de personagem e sua forma própria na raiz; malha cilíndrica no filho visual. Não conservar outro Body escrevendo a mesma pose. O caminho atual do projeto considera Character incompatível com Body/Collider no mesmo objeto. [S8]

**Cilindro fisicamente dinâmico:** conservar corpo/colisor cilíndrico, acrescentar DynamicBodyMotor. Forças, impulsos, aceleração, velocidade máxima, controle no ar, probes de chão, salto, rampas, torque e travas são políticas do motor. A física continua sendo a autoridade; não teleportar Transform por frame.

A conversão deve ser uma transação que preserva material, malha, filhos, referências acordadas e Undo. Mostrar visualmente a autoridade física atual. A proposta não redefine restrições de Jolt sem teste; utiliza os caminhos de integração já adotados pelo projeto.

### 6.4 UI anexada a corpo em movimento

O canvas lê a mesma pose de apresentação utilizada para a malha, combinada com seu offset local. Escala não uniforme, hierarquia, corpo dormindo, teleport, respawn e remoção precisam de testes. Matrizes singulares ou conversões sem suporte devem emitir erro, não NaN.

Durante uma captura, conservar alvo e identidade do gesto, mas converter os movimentos para o referencial correto do painel móvel. Não congelar coordenadas globais como se o painel fosse estático. Ao editar um corpo em Play, usar comando seguro ou política de pausa; o gizmo não pode competir com a simulação.

## 7. Roadmap executável e rastreabilidade

As tarefas abaixo são propostas. “Implementado” só será atribuído depois de consumidor, persistência, API, editor e testes correspondentes. Não colocar tipos futuros em menus apenas para aumentar a contagem.

### R0 — Contratos e compatibilidade (complementa P0; pré-requisito geral)

**Saída:** decisão arquitetural e testes de regressão do que já existe.

- R0.1 Fixar snapshot e matriz atual/parcial/planejado, incluindo limitações e evidências.
- R0.2 Corrigir documentação AEUI3 versus AEUI4; negociar próximo formato/ABI e migrações.
- R0.3 Definir identidade de instância, referências persistentes e PropertyId estável.
- R0.4 Definir e migrar pintura, layout, hit, interação, foco e semântica como dimensões distintas.
- R0.5 Declarar interfaces de runtime sem dependência de editor e pontos de integração.
- R0.6 Definir lifecycle, fonte da verdade e regras de comandos/threads/filas.

**Aceite:** exemplos AEUI existentes abrem, interagem e salvam sem alteração semântica inesperada; incompatibilidades têm diagnóstico. Decisões de tipos/IDs são compartilhadas por todas as frentes.

### R1 — Instâncias e hierarquia de cena (antecipa K/M de P5/P6)

**Dependência:** R0. **Saída:** UI é instanciável na cena.

- R1.1 Hospedar documento em entidade com componentes e referências do sistema existente.
- R1.2 Manter estado runtime independente por instância e catálogo por mundo.
- R1.3 Mostrar composição na hierarquia principal por adaptador, sem segunda árvore mutável.
- R1.4 Seleção contextual, Inspector, rename, reorder, duplicate, remove e Undo integrados.
- R1.5 Anexar WorldCanvas à pose de objeto físico e suportar offset/gizmo local.
- R1.6 Criar/remover instâncias em runtime, invalidar handles e limpar capturas/recursos.
- R1.7 Duplicar/prefab com remapeamento de IDs e parâmetros de referências externas.
- R1.8 Executar um documento simples em host sem editor como prova inicial de entrega.

**Aceite:** duas instâncias do mesmo documento, uma no HUD e outra em corpo dinâmico, sem compartilhar estado. Duplicar corpo+UI, salvar/reabrir e destruir uma instância não afeta a outra.

### R2 — Recursos, partes, skins e temas (P1; A/B)

**Dependência:** R0/R1. **Saída:** aparência livre, não apenas botões recoloridos.

- R2.1 Migrar referências de imagem para assets/sub-recursos estáveis, mantendo caminhos antigos importáveis.
- R2.2 Páginas de atlas, quota, gutters, sampling e lifetime com fences.
- R2.3 Partes visuais opcionais e substituíveis; remover dimensões fixas do desenho dos controles.
- R2.4 Nine-slice completo, tiles, regiões, native size e fills com regras de degeneração.
- R2.5 Tema, variante, subárvore e override, com reset/origem e prevenção de ciclos.
- R2.6 Estados combináveis, sprites por estado, foco separado e transições interrompíveis.
- R2.7 Skin editor com preview de estados e edição de partes isoladas.
- R2.8 Reload/erro/unload sem textura antiga enganosa nem perda de overrides.

**Aceite:** Button só de ícone, Toggle com checkmark próprio, Slider com três sprites e painel nine-slice. Trocar tema mantém overrides; pressionar não inventa fundo; recursos faltantes são identificáveis.

### R3 — Entrada multicanal e foco (P2/E; transversal)

**Dependência:** R0/R1. **Saída:** interação simultânea sem roubo de controles.

- R3.1 Capturas por dispositivo/pointerId e destino por instância/viewport/player.
- R3.2 Eventos, propagação, hit policies e arbitragem explícita de gestos.
- R3.3 Hit transformado, forma/alpha opcional e coordenação de oclusão no mundo.
- R3.4 Foco, tab/direcional, modal scopes, gamepad, submit/cancel e restauração.
- R3.5 Cancelamento por perda de app focus, remoção, disable, troca de documento e dispositivo.
- R3.6 Separar foco de editor, UI e gameplay; não zerar todo gameplay por qualquer toque de HUD.
- R3.7 Registrar comandos tipados de UI→cena/script e validar alvo/argumentos/lifecycle.
- R3.8 Instrumentar donos de captura e caminho de consumo dos eventos.

**Aceite:** três dedos em mover/olhar/atirar, modal abrindo durante gesto, mouse e gamepad alternando. Nenhuma ação presa e nenhum clique duplo acidental por tradução de dispositivo.

### R4 — Joystick e motores (P2/F/G)

**Dependência:** R1/R3. **Saída:** player controlável com ligação visual autoral.

- R4.1 Joystick nos três modos, base/knob livres e parâmetros de curva/deadzone.
- R4.2 OnScreenButton e área de olhar para ações tipadas contínuas e discretas.
- R4.3 Catálogo de actions/players/receivers e picker de câmera/espaço.
- R4.4 Agregação de fontes por ação; cancelamento individual; buffer de eventos para FixedUpdate.
- R4.5 Recipe Character + visual Cylinder com conversão reversível.
- R4.6 Recipe DynamicCylinder + visual + DynamicBodyMotor sem dupla autoridade.
- R4.7 Grounding, salto, rampas, plataforma, knockback, respawn e reconexão de alvo.
- R4.8 Debug do vetor, motor/autoridade e erros de vínculo no Inspector.

**Aceite:** joystick + salto simultâneos, referência salva, câmera correta, corpo reage a contato/impulso e UI anexada acompanha sem trepidação introduzida por amostrar outra pose.

### R5 — Layout responsivo e ferramentas espaciais (P3/C/L)

**Dependência:** R0/R2. **Saída:** interfaces recompõem sem ajustes manuais por aparelho.

- R5.1 Medição/arranjo com min/max/preferred, margem/padding, flex e aspect.
- R5.2 Transformação local completa com pivot, rotação e escala XY consistente com hit/clip.
- R5.3 Flow/wrap, stack, grid adaptável, split e políticas de overflow.
- R5.4 Safe area, DPI, font scale, orientação, teclado e perfis/breakpoints.
- R5.5 Detecção de ciclos, diagnóstico de restrições e indicação de eixo controlado pelo pai.
- R5.6 Multiseleção, align/distribute/match-size, snap/guides e transações por gesto.
- R5.7 Dirty flags específicos e layout incremental sem otimização sem medição.
- R5.8 Preview por resolução/idioma e casos de tamanho zero/extremo.

**Aceite:** HUD/inventário em portrait, landscape e tablet; textos mais longos e teclado aberto sem cortar campo focado ou entrar em recomposição infinita.

### R6 — Texto internacional e edição Android (P3/D)

**Dependência:** R2/R3/R5. **Saída:** texto real de jogo e InputField, não só editor ImGui.

- R6.1 Font assets/fallback e catálogo de cobertura de glyphs.
- R6.2 Bidi, segmentação, shaping, quebra de linha e cache de runs.
- R6.3 Multiline, rich text restrito, links, inline images, ellipsis e seleção.
- R6.4 Caret e seleção por grapheme; operações de edição e Undo local.
- R6.5 Adaptador IME com seleção/composição, paste, keyboard types e resize.
- R6.6 Locale/plurais/formatos, troca em runtime e RTL no layout.
- R6.7 Validação, placeholder, read-only, senha, submit/cancel e proteção de dados sensíveis.
- R6.8 Ponte semântica de texto/foco para acessibilidade e testes de retorno do app.

**Aceite:** acentos compostos, emoji com fallback disponível, árabe misturado a números/latim, seleção touch, texto multilinha e IME; a composição não é perdida por um update de binding.

### R7 — Bindings, coleções e controles compostos (P4/H/I; antecipa fundamentos de P6)

**Dependência:** R1/R2/R3/R5/R6 conforme controle. **Saída:** interfaces orientadas a dados.

- R7.1 Binding tipado, conversões, null/error/default e batch de mudanças.
- R7.2 Semântica de commit e prevenção de loops one-way/two-way/eventos.
- R7.3 ScrollView/Scrollbar com clipping, inércia e transferência de gesto aninhado.
- R7.4 Lista/grid/tree/table normal e virtualizada, com chaves estáveis e provider.
- R7.5 Seleção, sort/filter/reorder, paginação, expansão e células de altura variável.
- R7.6 Recycling seguro para foco/captura/IME e imagens assíncronas.
- R7.7 Empty/loading/error/retry/no-results, incluindo dados que somem durante interação.
- R7.8 Dropdown/search/tabs/menus/dialogs usando popup/foco compartilhados.

**Aceite:** zero, um e dez mil itens; filtrar/reordenar preserva identidade e seleção quando aplicável; quantidade de visuais acompanha janela visível, não tamanho total do modelo.

### R8 — Inventário, formulários e drag-and-drop de domínio (P4/H/I)

**Dependência:** R3/R6/R7. **Saída:** exemplos reutilizáveis com modelos reais.

- R8.1 Payload tipado, início/preview/validação/commit/cancel do drag-and-drop.
- R8.2 Inventário: stack/split/equip/transfer/use/drop e restrições do modelo.
- R8.3 Transação de item UI→mundo/prefab com validação de destino.
- R8.4 Recepção mundo→UI, múltiplos inventários e rejeição sem duplicação.
- R8.5 Form state, campo tipado, erros por campo/cruzados e foco no erro relevante.
- R8.6 Validação/submit assíncronos com geração e cancelamento de respostas obsoletas.
- R8.7 Persistência do modelo independente do documento e política de reset.
- R8.8 Exemplo completo de inventário e formulário, touch/gamepad e sem código de ligação repetido.

**Aceite:** arrastar, dividir e descartar altera save/dados reais; erro ou cancelamento não perde itens. Formulário vazio, inválido, pendente e confirmado usa o mesmo conjunto de controles.

### R9 — Apresentação avançada e composição 3D (P5/K)

**Dependência:** R1/R2/R3/R5. **Saída:** cobrir SP01–SP22 por capabilities entregues.

- R9.1 ScreenCamera, world anchors, billboard e identificação por viewport.
- R9.2 Múltiplos painéis/câmeras/players, sorting e seleção de instância no mundo.
- R9.3 Máscaras retangulares/arredondadas/stencil/alpha com hit coerente.
- R9.4 RenderTexture e EmbeddedViewport com resolução/cadência/orçamento.
- R9.5 Canvas sobre malha/UV e mapeamento curvo como capability específica.
- R9.6 Lado/oclusão/transparência/distância e materiais suportados com fallback declarado.
- R9.7 Render graph sem ciclos; recursos por consumidor, não RT por widget.
- R9.8 Smoke de exportação por modo e diagnóstico de backend ausente.

**Aceite:** HUD, dois painéis em objetos, câmeras diferentes e preview 3D no inventário; clicar usa o referencial da imagem apresentada. Curvatura só é marcada pronta após picking e costura testados.

### R10 — Animação e timeline (P5/J)

**Dependência:** R0/R2/R5 e R9 para tracks específicas. **Saída:** animações compostas, editáveis e previsíveis.

- R10.1 Tracks/keyframes tipados, interpolação apropriada e referência de propriedade estável.
- R10.2 Sequências/paralelos/loops/reverse, seek e callbacks de lifecycle.
- R10.3 Relógio real/escalado, pausa e política de animação de menus durante pausa do jogo.
- R10.4 Resolver script/binding/estado/timeline por propriedade, sem gravar autoria.
- R10.5 Sprite animation, rotação/escala XY e animação de partes.
- R10.6 Timeline/curvas no editor, preview, Undo e origem do valor inspecionável.
- R10.7 Reduced motion e interrupção sem salto indevido ou callback fantasma.
- R10.8 Invalidação granular: mudar cor não deve recalcular todo layout.

**Aceite:** abrir/fechar inventário repetidamente durante transições, interromper por modal, trocar modelo e remover alvo; estado final correto e custos atribuíveis.

### R11 — Reutilização, extensões, entrega e exportação (P6/L/M)

**Dependência:** contratos R0/R1 e famílias utilizadas. **Saída:** autoria escalável e projeto executável fora do editor.

- R11.1 Templates com slots, parâmetros públicos e variantes, sem dependências externas implícitas.
- R11.2 Overrides por instância, apply/revert e atualização do original sem apagar customizações locais.
- R11.3 Pacotes de UI com assets/fontes/temas/bindings e inventário de licenças.
- R11.4 Extensão registrada: schema, propriedade, editor, serializer, runtime, input e lifecycle.
- R11.5 API autoral e operações automatizadas usam os mesmos comandos/transações do editor.
- R11.6 Export resolve dependências e capabilities por plataforma; não embute estado/editor sem necessidade.
- R11.7 Migração de versões, hot reload e recuperação de erro sem substituir dados válidos.
- R11.8 Host de jogo sem EditorPlayScene e testes de equivalência com preview/Play.

**Aceite:** menu/inventário usados em duas cenas, atualizados pelo template, personalizados e exportados; extensão ausente produz diagnóstico, não elemento vazio que parece sucesso.

### R12 — Acessibilidade, qualidade e escala (P7, iniciado em R0)

**Dependência:** contínua; gate final depende dos módulos publicados. **Saída:** evidência verificável no Android.

- R12.1 Árvore semântica e bridge Android, ações acessíveis e IDs estáveis.
- R12.2 TalkBack, gamepad/teclado, fonte ampliada, reduced motion e contraste/estado não só por cor.
- R12.3 Inspectors de layout/skin/input/binding/recursos com razões de invalidação.
- R12.4 CPU/GPU/memória/allocations, p50/p95/p99 e cenas representativas.
- R12.5 Orçamentos de nós/páginas/glyphs/masks/draws/RT e falhas controladas ao exceder.
- R12.6 Surface loss, background/foreground, memória baixa, rotação e desconexão de dispositivos.
- R12.7 Matriz de testes combinatórios, golden captures e rastreabilidade tarefa→evidência.
- R12.8 Relatório de capacidades suportadas/parciais/indisponíveis, sem alias de suporte fictício.

**Aceite:** percursos reais acessíveis, desempenho atribuído e nenhum vazamento/ação presa nos cenários de interrupção. Build aprovado não substitui validação visual e física no aparelho.

## 8. Bibliotecas e fontes: aproveitar sem criar outra engine

| Tecnologia | Uso recomendado | Limite/condição |
|---|---|---|
| Dear ImGui já integrado | Ferramentas de autoria, Inspector, diagnóstico e extensões de editor. | Não fornece sozinho o modelo de jogo universal, internacionalização completa ou acessibilidade. [E1] |
| HarfBuzz + FreeType | Shaping e rasterização/carregamento de fontes. | Fallback, bidi, line breaks, edição e IME ainda precisam de integração. [E2, E3] |
| ICU | Bidi e segmentação de graphemes/palavras/linhas; avaliar dados por locale. | Medir tamanho/memória e escolher estratégia consistente entre plataformas. [E4] |
| GameTextInput/ponte Android existente | Seleção/composição e teclado de casos de jogo. | Respeitar tipo de host; integração com GameActivity e standalone não se acumulam. [E5] |
| Yoga | Candidato para flex/wrap e cálculo de layout. | Um único resolvedor por contrato; Yoga não desenha nem entrega foco, física ou Inspector. [E7] |
| RmlUi | Alternativa de backend amplo com documento, estilos e data bindings. | Fazer experimento comparável antes de trocar modelo/renderer; renderer e interfaces de plataforma continuam necessários. [E8] |
| Componentes de engine open source | Referência para contratos de Control, Canvas, Scroll e Selection. | Não transportar classes isoladas presumindo que não têm dependências. Fixar commit/licença e registrar alterações. |

**Decisão prática sugerida:** aproveitar primeiro o runtime nativo, identidade, histórico e renderer já existentes. Medir um experimento RmlUi apenas se a expansão de layout/texto/skins justificar trocar o backend. Não manter duas árvores editáveis e dois layouts decidindo o mesmo retângulo.

O experimento precisa mostrar: menu com skins, formulário IME, lista realmente virtualizada, duas instâncias em objetos, input simultâneo, persistência e host exportado. Mostrar uma página RML bonita não fecha esses contratos. Repetir elementos via binding não é, por si, prova de virtualização.

## 9. Mapa de integração no repositório

| Ponto observado | Ampliação proposta |
|---|---|
| `native/ui/gui_document.h/.cpp` | Separar capacidades, estado e caches; preservar leitura antiga; identidade local estável. |
| `native/ui/gui_workbench.h/.cpp` | Seleção contextual, propriedades/partes/estados/bindings, árvore de instância e comandos compartilhados. |
| `native/ui/gui_world.h/.cpp` | Receber matriz resolvida e view context; ampliar mapeamento e múltiplos destinos. |
| `native/ui/gui_images.h/.cpp` | Assets/sub-recursos, páginas, settings e ownership; não armazenar handles GPU no documento. |
| `managed/Astra.Scripting/Gui.cs` | API de instância, referências completas, acesso tipado e capabilities negociadas. |
| `native/runtime/script_bridge.*` | Roteamento de instâncias e alvos tipados; ABI versionada e rejeição de handles antigos. |
| `native/editor/editor_play_scene.h` | Substituir ligação exclusiva a uma UI por integração runtime genérica, mantendo editor como host. |
| `native/runtime/input_actions.*` | Contribuições touch por origem/player, cancelamento e agregação, sem acoplar ao visual. |
| Sistema de componentes/contratos do projeto | Registrar Canvas e referências novas no mecanismo existente, não criar Inspector paralelo. |
| `native/ui/ui_draw_list.*` e `ui_instance_builder.*` | Comandos de visual/clip/texture registry; batches com ordem visual preservada. |
| `native/ui/ui_font.*` e `ui_text.*` | Interfaces de texto internacional, shaping/rasterização e cache. |
| Roadmap, exemplos e validação | Estado por tarefa/capability, cenários e rastreabilidade; remover parágrafos contraditórios. |

Novos arquivos como adaptadores de instância, bindings e host de UI podem ser necessários. Os nomes e a divisão final devem seguir as convenções e camadas do repositório; esta tabela não afirma que os novos módulos já existem.

## 10. Matriz de testes de combinações

Não testar só um exemplo isolado por recurso. Modelar os eixos:

**Apresentação:** overlay, câmera, world, projetada, render texture.  
**Alvo:** nenhum, estático, dinâmico, cinemático, osso, entidade removida.  
**Origem:** documento, template, override, criação runtime, versão migrada.  
**Entrada:** touch, multitouch, mouse, teclado, gamepad, acessibilidade.  
**Layout:** livre, container, scroll, virtualizado, aninhado, RTL, zero-size.  
**Estado:** normal, múltiplos estados, loading/error, hidden/disabled, pausa, remoção.  
**Recurso:** válido, ausente, corrupto, reload, quota atingida, GPU recriada.

Cobertura por pares reduz combinações repetidas; cruzamentos de alto risco recebem cenários explícitos com três ou mais eixos. O relatório distingue não testado, passou, falhou e não suportado. Cobertura por pares não é prova matemática de ausência de bugs.

### Cenários de aceite obrigatórios

| ID | Cenário | Critério observável |
|---|---|---|
| T01 | Documento antigo AEUI1/2/3/4 | Leitura/migração preserva semântica e IDs previstos. |
| T02 | Duas instâncias do mesmo documento | Click/estado em uma não altera outra. |
| T03 | Duplicar prefab com UI/bindings | Alvos internos remapeados; externos explicitados. |
| T04 | UI filha de corpo dinâmico | Pose acompanha malha sem dupla autoridade. |
| T05 | Parent móvel com escala/rotação | Transform, desenho e picking concordam ou configuração é rejeitada claramente. |
| T06 | Alvo destruído durante captura | Cancel executado; callback não usa handle inválido. |
| T07 | Três dedos mover/olhar/saltar | Ações independentes e consumo discreto correto. |
| T08 | App perde foco durante joystick | Contribuição zera; retorno não cria salto de input. |
| T09 | Dois jogadores/câmeras | Foco, devices e UI não vazam entre contextos. |
| T10 | Modal abre durante arrasto | Cancel/transferência e retorno de foco conforme contrato. |
| T11 | Botão só de imagem com alpha zero | Pintura/hit seguem política explícita, inclusive migração. |
| T12 | Checked + Focused + Disabled | Skin resolvida determinística por parte. |
| T13 | Nine-slice menor que suas bordas | Política de redução válida; sem UV/geom degenerada. |
| T14 | Recurso removido/recarregado | Erro visível e descarte correto da referência antiga. |
| T15 | Atlas cheio durante interação | Falha controlada e sem corrupção de GPU. |
| T16 | Portrait→landscape com IME | Campo, seleção e composição permanecem coerentes. |
| T17 | Grapheme/emoji/RTL misturado | Caret e seleção não dividem unidades indevidamente. |
| T18 | Fonte fallback ausente | Diagnóstico e substituição previsíveis. |
| T19 | Lista vazia, um e 10.000 itens | Empty e seleção corretos; visuais proporcionais à janela. |
| T20 | Filtrar lista durante edição IME | Estado não migra para outro item reciclado. |
| T21 | Imagem assíncrona chega após reciclagem | Não aparece na célula de outro ID. |
| T22 | Drag entre inventários, destino rejeita | Modelo e UI retornam sem duplicar/perder item. |
| T23 | Drag item→mundo, raycast inválido | Nenhum item consumido nem prefab incorreto criado. |
| T24 | Submit assíncrono após fechar formulário | Resposta obsoleta não reativa painel/alvo removido. |
| T25 | Binding circular/ação recursiva | Ciclo limitado/diagnosticado, sem travar frame. |
| T26 | Animação interrompida por binding | Política de dono preserva o valor correto. |
| T27 | Pausa jogo com menu animado | Relógio escolhido respeitado e input seguro. |
| T28 | Painel móvel sob dedo | Alvo capturado e remapeamento local corretos. |
| T29 | Máscara aninhada em painel rotacionado | Pixel apresentado e área interativa compatíveis. |
| T30 | UI sobre cilindro com costura UV | Picking e continuidade respeitam parametrização. |
| T31 | Preview3D e câmera de segurança | Render targets dimensionados, sem ciclo de render. |
| T32 | Popup de item dentro de scroll | Escapa clip visual, mantém owner/foco/contexto. |
| T33 | Template atualizado com override local | Atualização não elimina customização legítima. |
| T34 | Extensão ou backend ausente | Erro de edição/export acionável, sem sucesso falso. |
| T35 | Background/surface loss/memória baixa | Recursos recriados, sem captura/handle antigo vivo. |
| T36 | Fluxo TalkBack completo | Nome/papel/ação/foco corretos, sem depender de pixels. |
| T37 | Gamepad sem touch | Navegar, editar, confirmar e cancelar todos os fluxos publicados. |
| T38 | Export sem editor | Documento/tema/texto/input e recursos funcionam no host de jogo. |
| T39 | Spawn/Destroy em callback | Comando executa em ponto seguro; iteração permanece válida. |
| T40 | Estresse prolongado de abrir/fechar UI | Memória não cresce continuamente; picos e quotas documentados. |

## 11. Desempenho e medições Android

Os números abaixo são dimensões de ensaio, não resultados obtidos nesta auditoria.

Medir cenas com: menu de aproximadamente 200 controles; lista com 10.000 dados e poucos visuais; HUD multitouch; vários painéis 3D; formulário internacional; carga/recarga de imagens; máscaras e preview 3D. Incluir caso estático e caso com mudanças contínuas.

Separar CPU de input, layout, shaping, bindings, animação, montagem de draw e submissão; GPU de UI/passagens adicionais; memória de docs/instâncias/glyphs/atlas/RT; allocations e uploads. Registrar p50/p95/p99, warmup, duração, resolução, refresh rate, build, cena, versão, temperatura e estado térmico.

A 60 Hz, o frame inteiro tem aproximadamente 16,67 ms; a 120 Hz, 8,33 ms. Não tratar isso como orçamento exclusivo da UI. Definir orçamento da UI a partir da carga de render/física do jogo e dos aparelhos-alvo. Metas sugeridas precisam ser calibradas antes de virarem gates obrigatórios.

A referência de aparelho no histórico do projeto não substitui teste entre classes de GPU e memória. Relatar capacidades ausentes, frames perdidos, limites atingidos e falhas de coleta. Captura host não prova GPU Android, build não prova UX e screenshot não prova ausência de ação presa.

## 12. Como executar com múltiplos agentes sem fragmentar o sistema

Depois de R0, contratos e testes-base são congelados por etapa. Uma frente cuida de instâncias/hierarquia; outra de input/motores; outra de skins/recursos. Texto e dados avançam quando seus contratos dependentes estiverem definidos. Integração acontece por cenário vertical, não apenas por merge de arquivos.

Designar um responsável por modelo/ABI/serialização para evitar três versões incompatíveis de NodeId, PropertyId ou UIInstance. Cada frente entrega consumidor real e atualização da matriz, e não listas de enums futuros. Refatorações de contratos compartilhados entram primeiro; versões de SDK e engine precisam ser distribuídas coerentemente.

Ordem recomendada de demonstrações: UI em corpo dinâmico e duas instâncias → skins livres → multitouch/player → formulário/texto → inventário/dados → apresentações/anim avançadas → templates/export e aceite de qualidade. A11y, compatibilidade, observabilidade e host sem editor recebem trabalho desde o início.

## 13. Definição de pronto por capacidade

Uma capacidade está fechada quando sua cadeia foi comprovada:

```text
criar/importar
  -> armazenar dados e identidade
  -> editar/inspecionar/Undo
  -> salvar/reabrir/migrar
  -> instanciar/duplicar/prefab
  -> executar e consumir por API
  -> interagir com input/foco/pausa
  -> reagir a erro/remoção/reload
  -> exportar com dependências
  -> testar e medir no alvo declarado
```

Exceções precisam ser explícitas e justificadas: um controle puramente decorativo não tem ação de clique obrigatória; um backend opcional não é condição para liberar os backends já comprovados. A meta é fechar a cadeia aplicável, não inventar campos sem sentido.

A prioridade final é **universalidade de composição com contratos e evidência**, não quantidade de funções. Depois desse alicerce, novos menus, painéis em objetos, jogos e ferramentas passam a usar as mesmas peças em vez de exigir uma solução especial para cada tela.

## Fontes e proveniência

Todos os arquivos [S] foram consultados no commit fixado no início. Para localizar um arquivo, use o caminho indicado e esse commit no GitHub. Referências externas [E] foram consultadas em 4 de outubro de 2026; suas funcionalidades não são apresentadas como já integradas à engine.

| ID | Fonte |
|---|---|
| S1 | `native/ui/gui_document.h` |
| S2 | `native/ui/gui_document.cpp`, especialmente `draw`, `hit`, `pointer`, `dispatch` e `poll` |
| S3 | `native/ui/gui_world.cpp` |
| S4 | `managed/Astra.Scripting/Gui.cs` |
| S5 | `native/editor/editor_play_scene.h`, configuração/ownership de UI e foco |
| S6 | `docs/planos/GUI-AUTORIA-E-IMGUI-2026-10-02.md` |
| S7 | `docs/planos/GUI-EXTENSAO-LAYOUT-IMAGEM-MUNDO-2026-10-02.md` |
| S8 | `docs/planos/UI-ROADMAP-COMPLETO-2026-10-03.md` |
| S9 | Commit `d12c2ea2ed4f19a690ed99410ed865407aec024a` |
| S10 | `docs/validacao/UI-ESTADOS-ACOES-2026-10-03.md` |
| E1 | Dear ImGui, README oficial: `https://github.com/ocornut/imgui` |
| E2 | HarfBuzz: `https://harfbuzz.github.io/what-does-harfbuzz-do.html` e `https://harfbuzz.github.io/what-harfbuzz-doesnt-do.html` |
| E3 | Integração FreeType: `https://harfbuzz.github.io/integration-freetype.html` |
| E4 | ICU: `https://unicode-org.github.io/icu/userguide/boundaryanalysis/` e `https://unicode-org.github.io/icu/userguide/transforms/bidi.html` |
| E5 | Android GameTextInput: `https://developer.android.com/games/agdk/add-support-for-text-input` |
| E6 | Android AccessibilityNodeProvider: `https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeProvider` |
| E7 | Yoga: `https://www.yogalayout.dev/docs/about-yoga` |
| E8 | RmlUi: `https://mikke89.github.io/RmlUiDoc/pages/data_bindings.html`, `https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/interfaces.html` e `https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/interfaces/render.html` |
