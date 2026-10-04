# attachsEngine: autoria de UI e Dear ImGui integrados

NÃO IREI SER SIMPLISTA NO DESIGN.

## Objetivo e contrato

Criar uma interface visualmente, ajustar layout/estilo/estado, testar interação, salvar/reabrir e consumir o mesmo documento pela API. Integrar Dear ImGui oficial para ferramentas imediatas por código, com entrada e desenho reais. Universal aqui significa compor interfaces com dados tipados e propriedades que têm consumidor; novos controles entram quando sua cadeia estiver implementada.

## Arquitetura aplicada

`GuiDocument` possui nós com ID estável, pai, kind, nome, texto, anchors/offsets, estilo, estado e domínio de valor. `GuiRuntime` resolve retângulos, desenha na `UiDrawList`, captura ponteiros e publica eventos. `GuiHistory` conserva transações para criação, edição, remoção e Undo/Redo. Arquivo `.aeui` versionado preserva IDs, domínio e apresentação; leitura valida limites, referências, ciclos e dados antes de substituir um documento vivo.

O painel de autoria utiliza Dear ImGui v1.91.9b-docking, commit `4806a1924ff6181180bf5e4b8b79ab4394118875` (MIT), preservado com licença. `ImmediateGui` possui um contexto, recebe ponteiro/teclado, constrói frames, traduz triângulos/cores/UV/clipping para instâncias da UI nativa e fornece seu atlas de fontes ao Vulkan. Não interpreta nomes de widgets nem substitui o draw data por uma tela parecida.

O espaço Interface entra pelo seletor de espaços existente. Em telas largas, árvore, canvas e propriedades coexistem. Em telas compactas, árvore e propriedades alternam na superfície contextual. Preview separa mutação autoral de interação: clicar botão publica evento, arrastar slider altera estado de preview, editar propriedades modifica autoria por transação. O documento é independente de estado de janela ImGui e de handles da GPU.

## Lote funcional

| Entrega | Comportamento exigido |
|---|---|
| Documento | Criar Panel/Text/Button/Toggle/Slider/Progress; IDs estáveis; hierarquia validada; remover subtree; duplicar; configurar geometria e estilo |
| Layout | Anchors normalizadas e offsets em referência; escala de canvas; clipping por ancestral; ordem de desenho consistente |
| Entrada | Captura no Down até Up/Cancel; oculto/inativo não recebe evento; slider respeita domínio; botão exige soltura sobre o alvo |
| Autoria | Criar, selecionar no canvas/árvore, editar, arrastar, remover, Undo/Redo, salvar/carregar arquivo de projeto |
| Runtime/API | Buscar por ID/nome; alterar texto/valor/visibilidade/estado; consumir eventos tipados; execução em preview e Play |
| Dear ImGui | Contexto e lifetime reais; font atlas; triângulos com cores/UV; clipping; ponteiro e texto; coexistência com renderer |
| Prova | Criar interface → alterar layout/estilo → Undo/Redo → salvar/reabrir → interagir → ler evento/valor pela API → captura executável |

## UX

Canvas domina a área central. A árvore representa composição; o Inspector mostra layout, conteúdo e aparência do nó selecionado. Controles de criação estão numa faixa de ferramentas, não repetidos em cards. Ação de Preview muda explicitamente o modo. Uma proposta estrutural é permitir a mesma superfície alternar Árvore/Propriedades em tela compacta, mantendo seleção e canvas; ajustes avançados aparecem no grupo apropriado.

Estados tratados: documento vazio, seleção, preview, pressão, captura/arraste, controle desativado/oculto, erro de arquivo e alteração não salva. Salvar não captura caches de layout nem estado de janela. Mudanças podem ser revertidas pela história; não substituem a UI central do editor.

## Referências

- [Unity uGUI 2.0: anchors, pivot e offsets](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/UIBasicLayout.html): composição relativa ao pai e edição de retângulos. A primeira versão usa anchors/offsets; não afirma paridade integral com RectTransform.
- [Godot 4.5 Control](https://docs.godotengine.org/en/4.5/classes/class_control.html): árvore, propriedades de layout, clipping e entrada. Tipos do documento permanecem próprios.
- [Dear ImGui oficial](https://github.com/ocornut/imgui): fonte e exemplos fundamentam contexto/frame, IO e draw data; versão vendorizada e licença registrados.
- [Godot 4.5-stable, fonte de Control](https://github.com/godotengine/godot/blob/4.5-stable/scene/gui/control.cpp): ownership por árvore e relação entre anchors, offsets e retângulo do pai. A adaptação resolve layout sob revisão do documento, sem usar estado ImGui como estado de jogo.
- [Workflow de anchors, GDQuest](https://school.gdquest.com/courses/learn_2d_gamedev_godot_4/telling_a_story/first_ui_exploration) e [vídeo de containers, GDQuest/Godot 2.1](https://www.youtube.com/watch?v=6G3NP5O9VsQ): pesquisa de criação, composição e reutilização. O vídeo foi localizado pela descrição indexada, não assistido integralmente. Containers são uma dependência distinta e não foram simulados por anchors.

## Validação e acompanhamento

Poucos cenários integrados protegerão hierarquia/serialização/transações, entrada/lifecycle e conversão do draw data. Capturas do código executável orientarão refinamento. APK, execução Android e imagem conceitual terão estados distintos. Ícones novos devem passar pelo atlas real. A entrega registrará quais partes passaram e quais ainda dependem de integração; nenhuma API sem consumidor contará como suporte.

## Estado aplicado em 02/10/2026

O lote desta entrega está conectado: 6 tipos de controle, documento AEUI v1,
GUI de autoria, preview, Play, C# com ABI 38 e extensões C++ em Dear ImGui real.
As propriedades de layout/estilo/estado têm consumidor e são preservadas no
arquivo. Há criação, subtree duplicate/remove, histórico, cancelamento de
ponteiro, rejeição de handles de mundos anteriores e erros de arquivo.
O projeto lembra o recurso ativo em `.astra/gui-resource` e restringe leitura e
escrita ao projeto. O renderer de UI possui triângulos e atlas ImGui reais.

O ícone Interface foi gerado e integrado ao atlas de produção (254 entradas).
A fonte ImGui deriva do mesmo Inter OFL usado pelo editor, com ferramenta de
geração reproduzível. As capturas reais motivaram correções de labels, fonte,
rolagem do campo com teclado e retorno ao topo ao trocar a seleção.

O cenário Android foi executado no modelo 25053PC47G, no projeto separado
GuiAcceptance-20261002: criar Button, editar texto por IME, arrastar, Undo/Redo,
salvar, reiniciar e reabrir; Play executou GuiMenu.cs, alterou texto do botão,
exibiu valor do slider, desabilitou o slider pelo toggle e restaurou a autoria
ao sair. As capturas e arquivos extraídos ficam na validação desta entrega.

## Expansão seguinte e limites

Este lote não conclui todo o inventário de componentes da engine nem toda a API
de Unity uGUI. A extensão tem portas definidas: novos kinds entram junto com
layout, draw, interação, persistência, editor e ABI quando precisarem dela.

| Pacote futuro | Dependências e aceite |
|---|---|
| Layout automático | Medição de conteúdo, min/preferred size e containers; provar recomposição ao redimensionar e reabrir |
| Recursos visuais | Referência tipada a textura, carga/reload/ownership e picker; provar troca e remoção sem atlas inválido |
| Entrada e foco de jogo | Controle Input, seleção, IME, foco/tab/gamepad e validação; provar digitar, cancelar, confirmar e reabrir |
| Estilos reutilizáveis | Recurso de tema com precedência de overrides e dependências; provar propagação sem perder alterações locais |
| UI em 3D/exportação | Binding explícito por cena, projection/picking e consumidor fora do EditorPlayScene; provar projeto exportado |

Docking, multi-viewport e texturas externas do ImGui não estão habilitados.
Não há aliases de suporte para essas capacidades. Documentos têm limite de
1024 nós, histórico 64, eventos 256; a renderização respeita os limites da UI
nativa. Performance de layouts maiores e bateria ainda não foi perfilada.
