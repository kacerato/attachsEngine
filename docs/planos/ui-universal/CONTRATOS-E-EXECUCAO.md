# UI universal: contratos e execução

## Especificação e limite de conclusão

O plano de 04/10/2026 foi copiado integralmente, sem alterações, para
`docs/planos/ROADMAP-UI-UNIVERSAL-attachsEngine-2026-10-04.md`.
SHA-256: `61727152c2e8485b5c9f8b59252074d4fda5856d18a5971b865069d8317d6b29`.
São 102 tarefas R, 22 configurações SP e 40 cenários T.
Requisitos sem ID, restrições, propriedades e critérios continuam no documento
original; os 164 IDs não substituem sua leitura. Nenhuma contagem mede paridade.

O primeiro pacote iniciou R0/R1 e a infraestrutura de R3. Inclui composição virtual na
árvore da cena, edição contextual e Undo intercalado com a cena. Não fecha o
primeiro resultado obrigatório: o aceite de gestos simultâneos no aparelho
permanece pendente. A continuação implementa controles autorados e ligação a
Character/câmera, detalhados em `R4-CONTROLES-E-RECEPTORES.md`. R2 e R5–R12,
assim como as partes restantes de R4, continuam pendentes.

## Contrato de dados e identidade

`GuiDocument` permanece a árvore autoral de UI e escreve AEUI 5. Sua leitura
mantém AEUI 1/2/3/4; os exemplos e testes anteriores continuam executáveis.
`UiCanvas` v2 é componente real do grafo de cena: GUID de documento, modo Tela
ou Mundo, enable, offset XYZ, Euler XYZ, resolução, unidades por pixel, oclusão
e ordem inteira, referências de receptor/câmera e espaço do movimento. Lê v1
com os defaults compatíveis. Não armazena outro Transform nem eventos runtime.

O AssetRegistry recebeu `UiDocument` (tipo 12, anexado). Salvar .aeui publica
arquivo e registro pelo journal transacional existente. Um GUID conhecido é
conservado nas revisões; modificações externas são recusadas antes de sobrescrever.
Um recurso ausente é erro, não uma instância do documento global por fallback.

Tela usa o retângulo do viewport; offset/rotação do plano, resolução local,
unidades por pixel e oclusão pertencem a Mundo. Esses campos têm visibilidade
condicional na reflexão e no Inspector. Não apresentar ajustes sem consumidor
em Tela nem declarar suporte a escala por resolução de referência.

Identidade autoral: objeto proprietário + component.instanceId local ao objeto
+ GUID do documento + GuiId local. Duplicar uma entidade conserva o GUID e o
ID local do componente; o novo proprietário impede colisões. A clonagem de
subárvores do documento continua remapeando suas ações internas.

Identidade de execução: worldId + lease de SceneGui + GuiId. Lease é monotônico
no hospedeiro, não é reciclado após Stop/reload e não vai para o arquivo. Trocar
documento retira a lease antiga. Criação/remoção de componente usa GameWorld e
seu ponto seguro; remove/dispose invalida os handles. Referências persistentes
entre documentos, bindings externos e PropertyId de UI ainda exigem contrato e
migração próprios: não estão representados por um ulong de runtime salvo.

ABI 43 mantém a descoberta por Canvas e despacho por lease, sem mover campos da
ABI anterior. O payload aceita somente as estruturas tipadas de propriedades,
canvas, sizing, comportamento, ações, estados, controles/entrada e comando/texto já consumidas.
Tamanho, operação, mundo e lifetime são verificados antes do despacho. SDK e
native devem ser distribuídos juntos. `Gui.ForCanvas(owner, componentId)` entrega
um contexto independente para Find/Create/Poll e para todas as propriedades
existentes. IDs iguais em contextos diferentes não designam o mesmo elemento.

## Runtime, fonte da verdade e threads

SceneGui não inclui nem linka editor: recebe GameWorld, loader de documento,
atlas de imagens, câmera/viewport e teste de oclusão opcional. O executável
`aether_scene_gui_host` exercita esse caminho sem a biblioteca aether_editor.
O documento carregado é compartilhado como fonte imutável; cada lease possui
uma cópia de execução, clock de UI, eventos, animações e capturas próprios.
Play não escreve no recurso autoral. GuiHistory agrupa gestos de edição UI;
na sessão de editor, seu commit registra snapshots no EditorHistory existente.
As operações de UI e da cena passam pelo mesmo cursor de Undo/Redo. O workbench
isolado, sem sessão, conserva seu histórico local. Replay não grava arquivos:
a fonte restaurada fica como rascunho até Salvar UI. Ao tentar desfazer em outra
fonte com um rascunho atual não salvo, a operação é recusada sem mover o cursor.

A apresentação World lê a matriz final de GameWorld.poseGraph usada pela malha,
combinada ao offset e rotação locais. A transformação é affine completa:
escala não uniforme, reflexão e shear são mantidos no desenho e na inversão do
ray. Matrizes/planos singulares ou não finitos são rejeitados com diagnóstico.
Não se atribui ao Canvas autoridade de física, motor ou animação de objeto.

Todas as APIs são síncronas na thread dona do mundo. O despacho por instância
reutiliza validadores existentes e restaura o runtime legado ao terminar;
reentrada é recusada. As ações nativas atuais não chamam código managed durante
o despacho nem alteram estruturalmente a cena. A futura ponte de comandos
UI→cena/script deve usar os pontos seguros; não é considerada implementada.

Captura é por (device, pointerId); uma instância pode ter 32 controles capturados.
Touch e mouse com o mesmo número têm identidades distintas. Um controle de valor
mantém um único dono por gesto. Remover/desabilitar/trocar documento cancela a
captura; a rota retirada continua consumindo o dedo até Up/Cancel. Um Down novo
depois de perda de foco inicia outra interação. Limite global de rotas: 2048.
Modal scopes, navegação, players/viewports separados e propagação são pendentes.

## Editor e diagnóstico

Canvas UI usa o Inspector de componentes existente e o resource picker por tipo.
Salvar uma UI a registra e permite atribuí-la ao componente. "Editar documento
UI" abre o recurso correto no workbench; trocar com edições não salvas é recusado.
O ícone existente `ui/interface-canvas` representa o mesmo conceito, sem inventar
um conceito gráfico novo.

EditorGuiTree projeta Canvas e elementos na hierarquia principal. A identidade
autoral é (objeto, componente, GUID do documento, ID do nó); entradas não são
entidades SceneGraph e não são leases de execução. O componente identifica a
instância autoral. Cada fonte inativa tem um cache somente de leitura; todas as
projeções do recurso ativo leem o único GuiDocument editável do workbench.
Nenhuma edição ocorre na lista projetada. Tokens de widget não são índices de
linha e não são reciclados ao remover/reordenar. Restaurar um snapshot preserva
o maior próximo ID, para uma nova ramificação de Undo não atingir alvos antigos.

Selecionar um elemento mantém o workspace Scene e substitui o conteúdo do
Inspector pelo editor real de propriedades UI. A trilha identifica objeto e
fonte, declara o número de Canvas compartilhando o recurso e oferece retorno ao
componente. Criar, renomear, reorder, duplicate e remove alteram o mesmo documento
usado pelo runtime. O rascunho ativo alimenta novas instâncias de Play sem gravar
no asset. Overrides autorais por instância continuam pendentes; o aviso de fonte
compartilhada não promete edição exclusiva de uma instância.

A prévia autoral mostra o Canvas selecionado no viewport da cena, usando a mesma
apresentação e inversão affine do runtime, sem animar nem disparar ações de jogo.
Tocar um elemento da prévia seleciona o nó. O contorno e a UI são recortados ao
viewport; picking World respeita oclusão. Não existe segundo escritor de pose.
Prévia autoral simultânea de todos os Canvas, gizmos de layout, drag-and-drop
entre documentos e árvore/Inspector de estado remoto em Play ainda são pendentes.

Referências deste fluxo: Unity uGUI 2.0 [Basic Layout](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/UIBasicLayout.html),
Unity Learn 2019.3 [Creating Basic UI](https://learn.unity.com/tutorial/creating-basic-ui-elements-2019-3)
(workflow e passos publicados, sem alegar assistir ao vídeo), Godot 4.5
[EditorSelection](https://docs.godotengine.org/en/4.5/classes/class_editorselection.html),
[EditorUndoRedoManager](https://docs.godotengine.org/en/4.5/classes/class_editorundoredomanager.html) e
[scene_tree_dock.cpp, 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/editor/docks/scene_tree_dock.cpp).
Princípios extraídos: a seleção determina o alvo do Inspector, hierarquia não
substitui ownership de dados e Undo deve conhecer o contexto da operação. Aqui,
um GuiDocument externo permanece recurso compartilhado e o Inspector ocupa a
superfície contextual já existente, sem acrescentar um terceiro painel mobile.

Uma falha de frame informa a etapa de Play e os erros reais de script/física
disponíveis, em vez de esconder a causa atrás da falha de publicação do renderer.
Capturas Android, testes e limitações são registrados separadamente no relatório
de validação. UI conceitual, host e execução física não são provas equivalentes.

## Limites atuais e próximos pacotes

13 tipos de node existentes, 64 leases por mundo, 1024 nodes por documento,
32 capturas por documento. O atlas de imagem continua em uma página 2048² e
até 64 fontes; páginas/fences/nine-slice/skins não foram acrescentados. Há Tela
e plano World; até 32 receptores Character/motor dinâmico e câmeras atribuídas. Pareamento de
hardware por jogador, RenderTexture, UV curvo e osso permanecem pendentes.

Pintura, opacidade e hit ainda preservam a semântica anterior. Fundo transparente
não força fundo pressionado; opacidade efetiva zero rejeita novas capturas.
Separar explicitamente pintura/hit/foco/semântica exige a migração de R0.4.
Não publicar enums ou propriedades decorativas para simular essa separação.

A sequência permanece a do plano: completar drag, gizmos e contextos de R1;
captura e foco de R3; concluir pareamento,
buffer geral e aceite multitouch de R4 após o caminho Character entregue;
skins/assets de R2, layout R5, texto R6, bindings/coleções R7, inventário R8,
apresentações R9, timeline R10, templates/export R11 e acesso/performance R12.
O pacote seguinte não deve pular a cadeia só porque o Inspector já expõe Canvas.

R4.5 agora possui criação pelo menu e conversão com Undo/Redo, geometria real,
persistência e aceite Android, descritos em `R4-AUTORIA-CHARACTER.md`. Undo é de
sessão; não existe restauração da configuração física anterior após fechar o
projeto. Prefab requer desvinculação explícita e geometria animada exige aceite
próprio. O pacote R4.6 acrescenta o motor dinâmico e sua receita por uma cadeia
própria, descrita em `R4-MOTOR-DINAMICO.md`; R4.7 e R4.8 continuam parciais.

## Referências e decisões

- Unity uGUI 2.0, [Canvas](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/class-Canvas.html): hospedagem/presentação separadas dos controles. Adaptamos ao componente e AssetRegistry existentes.
- Godot 4.5, [CanvasLayer](https://docs.godotengine.org/en/4.5/classes/class_canvaslayer.html) e [fonte canvas_layer.cpp](https://github.com/godotengine/godot/blob/4.5-stable/scene/main/canvas_layer.cpp): propriedade do canvas e viewport explícitos. Multiviewport permanece pendente aqui.
- Godot 4.5, [SubViewportContainer](https://docs.godotengine.org/en/4.5/classes/class_subviewportcontainer.html) e [viewport.cpp](https://github.com/godotengine/godot/blob/4.5-stable/scene/main/viewport.cpp): desenho e encaminhamento de entrada precisam usar o mesmo espaço. Não é suporte a EmbeddedViewport nesta entrega.
- Unity 2019.4.10f1, [workflow World Space](https://learn.unity.com/tutorial/creating-a-worldspace-ui), e [vídeo citado para Unity 4.6](https://www.youtube.com/watch?v=Mzt1rEEdeOI): criar/apresentar UI como parte da cena. **O vídeo ainda não teve análise visual quadro a quadro registrada; é uma referência localizada, não evidência observada de interação.** As referências de documentação/código e a revisão executável no Android são evidências separadas. A análise atual de vídeo trata de [Colliders](ANALISE-VIDEO-COLLIDERS-2026-10-05.md), com cobertura explícita, e não encerra a análise de World Space.
