# Nova UI: mais capacidades com espaço para trabalhar

## Direção

Manter a estrutura já aprovada: Cena/Arquivos à esquerda, viewport central e Inspector à direita; cinza escuro e acento Astra; painéis recolhíveis/redimensionáveis e densidade de ferramenta. Incorporar a descoberta e as áreas contextuais do Godot, a composição/inspeção da Unity e a gaveta de conteúdo/modos de autoria do Unreal. A implementação permanece no editor nativo atual.

O conceito visual produzido em 23/09 foi recusado por se afastar do editor atual. `conceito-ui.png` permanece apenas como registro histórico; não orienta implementação. A referência de aparência é o layout Astra em uso: superfícies carvão, realces lima, tipografia e densidade existentes. Unity, Godot e Unreal informam organização e fluxos, sem importar sua aparência literalmente.

## Política de expansão das abas e superfícies

Uma função nova ganha **aba** quando resolve uma tarefa própria que precisa permanecer acessível durante a edição e não cabe no comando existente. O `+` da Hierarquia cria objetos; `Add` no Inspector busca e adiciona componentes. Um segundo catálogo em outra aba duplicaria esse fluxo, por isso foi retirado. O Inspector continua reservado aos valores do item selecionado; o viewport continua reservado à cena. Cada nova aba declara onde aparece, quando está disponível, que estado editorial conserva, qual dado real lê e qual comando real executa. Abas sem consumidor não entram na navegação.

Listas usam fonte única do subsistema, item selecionável, paginação/rolagem e estado vazio/indisponível explícitos. Uma linha não executa ação destrutiva escondida: ela abre detalhe ou navega para o contexto. O **popup** concentra confirmação ou explicação curta de uma ação iniciada na aba; mostra requisitos/conflitos e bloqueia toques na cena ao fundo. Propriedades autorais continuam passando por transação e Undo; abrir aba, paginação e popup não alteram o documento.

Em tela larga, a aba de ferramentas reserva uma doca inferior própria e reduz o viewport pela mesma medida. Em tela compacta ou baixa, a aba vira uma folha sobreposta que pode ser fechada; a câmera não recebe seus toques. A seleção do objeto permanece única entre Hierarquia, doca e Inspector. O padrão é fechado para preservar o espaço atual, e o acesso fica na barra superior Astra com ícone PNG registrado no atlas existente.

O primeiro bloco da nova doca é **Problemas / Registros** na Cena. Ambas as abas leem o `EditorConsole` já usado pelo compilador, scripts e importação; a lista abre o detalhe do evento e a origem navegável. Em tela larga a doca reduz o viewport; em tela compacta vira folha sobreposta que bloqueia toques na cena. As ações de composição continuam no `Add` existente. Isso estabelece o contrato para próximas abas sem declarar Recursos, Timeline ou outros painéis como implementados aqui.

Referência de fluxo: o [Console da Unity 6.0](https://docs.unity3d.com/6000.0/Documentation/Manual/Console.html) separa a lista de mensagens de seu detalhe e origem; o [Output do Godot](https://docs.godotengine.org/en/stable/tutorials/scripting/debug/output_panel.html) reúne saída do projeto e do editor. Na Astra, essa tarefa já tem fonte única em `EditorConsole`; a mudança é torná-la acessível durante a edição da Cena.

## Organização do espaço

| Região | Conteúdo permanente | Conteúdo contextual |
|---|---|---|
| Barra global | Projeto/cena, salvar/estado, Undo/Redo, Play/Pause/Stop, busca/ajuda | Exportação em menu, alertas que exigem ação |
| Seletor de espaço | Cena 3D, 2D, Código; demais no seletor conforme suporte | Animação, UI, Material, Áudio, Terreno |
| Esquerda superior | Hierarquia, filtro, criar, ações de seleção | Cena autoral versus mundo remoto de Play claramente rotulados |
| Esquerda inferior | Arquivos e coleções do projeto | Favoritos, importação, recursos ausentes, seleção por tipo |
| Centro | Viewport ou editor de recurso ativo | Ferramentas do modo, gizmos, overlays, previews |
| Direita | Inspector com breadcrumb e pin | Propriedades, referências, dependências, documentação |
| Inferior recolhível | Console/problemas, recursos, histórico | Timeline, mixer, nav bake, profiler, resultados de busca |

Não mostrar as oito áreas como oito botões obrigatórios em celular. O seletor principal tem opções pesquisáveis e recentes. Abas de documentos ficam em uma linha separada de ferramentas, com indicador de alteração não salva.

### Tablet, desktop e celular

Dimensões abaixo são proposta em unidades lógicas, a validar no aparelho; não métricas aprovadas.

| Perfil | Layout | Interação |
|---|---|---|
| Largo, aproximadamente ≥1100 dp | 3 colunas; esquerda 20–24%, centro flexível, Inspector 26–30%; gaveta inferior | Mouse/teclado e toque; resize por divisória, modo foco |
| Intermediário, 760–1099 dp | Centro + um painel fixado; outro como drawer; Inspector 300–380 dp conforme espaço | Alternar painel mantém seleção/scroll; ajuda abre sem fechar edição |
| Compacto, <760 dp ou pouca altura | Viewport/editor ocupa tela; tabs Cena/Arquivos/Inspetor; sheet ou tela dedicada | Um painel por vez; ações primárias acessíveis ao polegar; landscape preferido para cena |

Largura e altura disponíveis, DPI, escala de texto, teclado e safe area determinam o perfil; não apenas resolução física. Tablet pequeno em portrait pode usar compacto. O usuário pode fixar um painel, mas o sistema deve impedir viewport irrecuperável e oferecer restaurar layout.

Alvos de toque principais de aproximadamente 48 dp, mantendo glyphs de 20–24 dp; linhas densas podem ter hit area ampliada sem inflar toda a UI. Arraste numérico tem edição exata por teclado como alternativa. Menus contextuais podem ser abertos por botão visível, sem exigir long press para descobrir uma ação essencial.

## Fluxos detalhados

### Adicionar capacidade

1. “+ Componente” abre busca por nome Astra, sinônimos Unity/Godot, categoria e favoritos. Cada resultado mostra nome, função curta e ícone de família; tipos indisponíveis têm motivo e não parecem executáveis.
2. Selecionar um resultado mostra propriedades iniciais relevantes, requisitos, conflitos e o que será criado. Requisitos já satisfeitos aparecem como tais. Recursos ausentes não são inventados.
3. “Adicionar” aplica a transação pelo resolvedor existente ampliado; um Undo remove exatamente o que a operação acrescentou.
4. A UI foca o novo grupo; links levam aos recursos/objetos requeridos. Se falhar, o estado da cena permanece anterior e o motivo aponta a correção possível.

Fatia implementada no editor nativo: ao escolher um componente do catálogo, o Inspector mostra o objeto de destino, os requisitos já presentes e os que serão anexados. A aba “Valores iniciais” lê os defaults e unidades dos descritores usados pelo Inspector; a busca também aceita termos de Unity e Godot apenas para localizar tipos Astra. Entradas incompatíveis explicam a recusa e não oferecem confirmação. Voltar não altera a cena; confirmar usa `EditorAction::AddComponent` e um Undo para toda a composição. Para comportamentos C#, a prévia fixa o tipo do catálogo publicado, mostra o arquivo e os campos expostos e confirma por `EditorAction::AddScript`; valores iniciais continuam definidos pelo código publicado.

Oferecer modos separados “Componente”, “Composição” e “Recurso”. Exemplo: criar “Personagem 3D” como composição pode criar root/motor, filho visual, câmera e input com preview; adicionar “Personagem” como componente não precisa criar HUD e câmera escondidos.

### Editar propriedade

Cabeçalho com nome/ativo/pin e breadcrumb. Grupos recolhíveis: Transform, Geometria, Materiais, Física etc.; busca por propriedade atravessa todos os grupos. Valores mistos na multisseleção mostram estado misto, nunca valor inventado. Arraste vira um único comando ao fim do gesto; cancelar restaura original. Badge de override abre comparar/reset/apply/revert quando houver prefab.

Ao lado de uma propriedade, menu de copiar valor, colar compatível, restaurar padrão, abrir ajuda, localizar consumidor/recurso e criar track quando animável. Unidades visíveis: m, s, graus, kg, lux/cd/lm conforme contrato. “Avançado” reduz densidade inicial, sem esconder de scripts campo necessário.

Fatia implementada: o componente nativo expandido filtra propriedades pelo nome, identificador e grupo, inclusive em outra aba, respeitando campos condicionais. A aba Material filtra seus controles reais por slot e mantém a escolha entre instância e material compartilhado. Campos refletidos alterados de número, booleano, enum, referência e valor numérico por slot mostram restauração do padrão; triplas restauram seus canais em uma transação. A restauração passa pela validação do componente e pelo histórico de Undo. Busca, limpeza e paginação não alteram a cena. Multisseleção e overrides de prefab continuam pendentes neste fluxo.

### Inspecionar dependências

Vista local no Inspector: “Exige”, “Usa recursos”, “Usado por”, “Conflita”, “Disponível neste alvo”. Relações recebem ícone e texto; cores não carregam o significado sozinhas. Seleção de um alvo pode abrir Inspector fixado ou navegar com voltar. Remover recurso/componente mostra os dependentes afetados; reparação, cancelamento ou operação em cascata explícita quando suportada.

Fatia implementada para componentes nativos e comportamentos C#: “Revisar remoção” abre o painel de impacto sem alterar o documento. Dependências de tipo e referências tipadas impedem a confirmação; “Cancelar remoção” fecha o painel sem Undo. Uma remoção permitida confirma pelo comando existente em um Undo. Se a cena mudar durante a revisão, o autor precisa reabrir o painel. Remoção em cascata continua fora deste fluxo.

### Play e diagnóstico

Mundo autoral e mundo em execução têm faixas/rótulos diferentes. Seleção de um objeto runtime não altera silenciosamente a seleção autoral. Console agrupa repetição e abre fonte/objeto relevante; aviso de capability explica a restrição. Stop devolve o documento preservado. Recompilação/falha de script mantém propriedades e referência de tipo, sem apagar o componente.

### Trabalhar com assets e importação

Arquivos em árvore à esquerda; gaveta mostra thumbnails/lista conforme o tipo e espaço. Preview central reutiliza o painel pertinente: material, mesh/skin, textura, áudio, clip. Inspector edita recurso compartilhado com indicação de usuários; “Fazer único” cria identidade nova e muda só o vínculo escolhido. Importação oferece fonte, perfil, resumo de mudanças, exclusões e relatório; reimportação é transação com cancelamento/publicação segura.

## Novas ferramentas por marco

| Pacote | Painéis/ações que chegam junto | Consumidor exigido |
|---|---|---|
| P01–P04 | Inspector rico, adicionar capacidade, dependências, ajuda contextual | Descriptor, transação, geração de docs |
| P05 | Prefab isolado, diferenças/overrides, dependentes, cenas aditivas | Recurso/instância e reconciliação |
| P06–P07 | Input maps, connections, layers físicas, gizmos de shapes/joints | Input/eventos e física reais |
| P08 | Material parameters, preview, camera/render targets, volumes/probes/bake | Shader/reflexão, render views, job/backend |
| P09 | UI workspace, anchors/layout, foco, tema, preview por resolução/idioma | Runtime UI independente do editor |
| P10 | Mixer, waveform/stream preview, buses | Backend áudio e lifecycle |
| P11 | Clip/timeline, graph de estados, skeleton/rig | Avaliador/pose e alvos tipados |
| P12 | Tile palette, pintura, sprite frames, gizmos 2D | Renderer/física 2D |
| P13 | Nav overlays, região, bake, paths | Navmesh e queries reais |
| P14–P16 | Emitter preview, spline points, brush/foliage | Simulation/geometry/terrain |
| P17–P18 | Extensões, docs/code/graph, build e problemas de projeto | Contratos, compiler/cooker/runtime |
| P19 | Rede/XR/vídeo conforme módulo | Backend disponível; não apenas menus |

A primeira workspace nova desse conjunto é “Agenda de timers”. Ela ocupa a área central com régua temporal e uma linha por instância, alterna horizonte de 2/10/60 segundos e seleciona a instância no Inspector. O Inspector edita intervalo, repetição e ativação; a linha temporal reflete esses valores. A régua descreve o agendamento autoral, não finge ser telemetria da contagem do Play.

“Camadas físicas” é outra workspace própria: escolhe a camada, cria/renomeia nomes e alterna os pares que interagem. A lista contextual substitui a matriz 32×32 impossível de tocar no telefone; cada toque grava a matriz recíproca por um comando de Undo. O mesmo dado salvo é entregue ao filtro de colisão do Jolt no Play. A escolha de camada de cada objeto continua no Inspector.

“Mapa de entrada” navega pelas ações da cena e pelos vínculos de cada ação. A página principal mostra identidade, tipo e papéis de gameplay; a página de propriedades edita contexto e resposta. Para cada vínculo, a página principal escolhe fonte/eixo/inversão, enquanto os códigos e a escala ficam na página de detalhes. Esse corte mantém os alvos tocáveis no telefone em paisagem; campos sem efeito para a fonte escolhida não aparecem. Cada alteração autoral passa pelo histórico e alimenta o mesmo `InputService` usado em Play.

## Ícones entregues e integração planejada

[Galeria](icones/galeria.html) e [prancha](icones/prancha.png): 40 desenhos geométricos originais, SVG de 32 unidades e PNG RGBA 512×512. Traço 1,75 unidades, terminações arredondadas, desenho claro para fundo escuro. A cor de seleção vem do controle, preservando a lógica existente; nenhum ícone de Godot/Unity/Unreal foi copiado. [Gerador](gerar_icones.py) e catálogo de proposta acompanham os arquivos.

| Família | Ícones |
|---|---|
| Composição/recursos | component-stack, dependency, prefab, variant, resource, override, link, broken-link |
| Gameplay | rigidbody, collider, sensor, joint, character, input-action, signal, timer |
| Visual/animação | material, shader, skeleton, animation, timeline, particles, terrain, spline |
| Mundo/UI/áudio | navigation, sprite, tilemap, canvas, text, audio-source, audio-mixer, localization |
| Ferramentas/extensões | network, xr, documentation, search-all, dock, pin, layers, export-game |

Estado: os 40 desenhos deste plano seguem como proposta e **não estão registrados no atlas de runtime**. Para a doca de diagnósticos foi gerado um PNG transparente inspirado na marca Astra (`ui/diagnostics`) e registrado no catálogo nomeado/atlas. Os 23 ícones de `hd-v1` continuam intactos. Próximos ícones devem ganhar ID no catálogo nomeado e passar por `tools/pack-icon-atlas.py`; o controle usa a célula de 96 px do atlas, não carrega o PNG fonte inteiro.

## Arquitetura da implementação

Evoluir `EditorScreenState` e `buildEditorScreen` com layouts por perfil; reutilizar `UiLayoutTree`, input e draw list. Ações seguem `EditorSession::dispatch` e comandos/IDs/versionamento. Estado de painel e preferências ficam na sessão/configuração do editor, separados de cena e mundo Play.

Extrair painéis quando houver responsabilidade concreta (Inspector, catálogo, resource preview, timeline), evitando acrescentar tudo ao arquivo monolítico. A mudança não pede substituir o toolkit ou incorporar engine externa. Editor e runtime podem compartilhar widgets de baixo nível; gameplay não depende de painel de ferramentas.

## Aceite visual e funcional

Inspecionar com toque/teclado nos perfis disponíveis: sem texto cortado, campo oculto pelo IME, alvo minúsculo ou mudança de seleção involuntária. Foco visível e ordem previsível; ações só por cor são insuficientes. Alterar tamanho de fonte/idioma não torna impossível confirmar/cancelar. Controles indisponíveis explicam o motivo. Resize, sheet e modo foco preservam o contexto.

Capturar estado normal, busca, composição com conflito, recurso ausente, multisseleção, prefab override, Play/Stop e teclado. Essas capturas da implementação futura são distintas da prancha conceitual entregue agora. Testes atuais de `test_ui_layout`, `test_ui_input`, `test_editor_screen` e `test_editor_session` são pontos de extensão, não evidência executada nesta tarefa.
