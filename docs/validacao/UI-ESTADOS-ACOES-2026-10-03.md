# attachsEngine — ações ordenadas e estados visuais

NÃO IREI SER SIMPLISTA NO DESIGN.

Trabalho local em `C:\Users\donod\Downloads\atchengine`, origin
`https://github.com/kacerato/attachsEngine.git`, branch `codex/gameplay-runtime`.
Este pacote não declara completa a engine nem as famílias A/E/J/L.

## Capacidade e cadeia implementada

Um Image, Text, Panel ou container conserva seu tipo, recurso e layout ao receber
interação. A ação principal de clique executa antes de até 16 ações adicionais
ordenadas. Os listeners filtram Click ou ValueChanged e executam as seis ações
nativas existentes. Não há funções de script arbitrárias registráveis no arquivo:
C# recebe o evento real por Poll e pode executar seu próprio código.

Dados em `GuiNode` são validados transacionalmente, serializados em AEUI4 e
consumidos por `GuiRuntime::dispatch`. AEUI1/2/3 seguem legíveis, com lista vazia
e estados opcionais desligados. Duplicar uma subárvore remapeia todos os alvos
internos. Um alvo removido fica identificável; diagnóstico não aborta as ações
seguintes. SetValue por ação emite ValueChanged quando o valor muda; API Value
permanece silenciosa. Despacho usa fila limitada a 256, sem recursão; o Poll
também tem fila limitada. Notify não duplica o evento já enviado ao script.

`GuiTransitions` contém Normal, Pressed e Disabled com pose e tinta locais.
Runtime mantém a aparência corrente e interpola até o novo estado, compondo
com `GuiMotion`, layout, herança de escala/alpha, clipping e desenho. Disabled
usa habilitação efetiva incluindo os ancestrais. Interromper uma transição parte
do valor atual. Captura conserva o retângulo do início do gesto para evitar
oscilação causada pelo próprio encolhimento; sair desse retângulo retorna o visual
ao Normal e soltar fora não emite clique. Cancel e disable liberam captura.
Opacidade zero não recebe novos hits; uma pose pressionada transparente pode
manter o gesto já capturado até a liberação. Estado não escreve a autoria.

Inspector mantém as propriedades no contexto do elemento: sequência ordenada
com editor da ação selecionada e seletor do estado a editar. Criar, remover,
reordenar e editar usa o histórico existente. Grupos recolhíveis evitam expor
todos os controles simultaneamente; Interagir dedica o espaço ao canvas.
Dois ícones próprios usam as formas sólidas já existentes na marca, gerados por
`tools/generate-gui-behavior-icons.py` e integrados por `pack-icon-atlas.py`.
Cada callback de ícone mantém uma lista de desenho distinta até o fim do frame.
As listas também respeitam o clip real do child do Inspector durante a rolagem.

ABI41 acrescenta callbacks tipados de ações (16 bytes) e transições (72 bytes).
SDK e engine devem ser distribuídos juntos; scripts precisam recompilar.
API: Actions/AddAction/ReplaceAction/MoveAction/RemoveAction e Transitions,
preservando Interaction/Animation/OnClick. Contexto, mundo, IDs, enum u32 antes
da conversão, domínio numérico e limites são conferidos na ponte real.

## Referências e adaptação

- [Unity uGUI 2.0, Selectable transitions](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/script-SelectableTransition.html):
  estados e duração do fade orientam propriedades reais; aqui pose e tinta se
  compõem sem exigir Animator nem transformar Image em Button.
- [Godot 4.5 BaseButton](https://docs.godotengine.org/en/4.5/classes/class_basebutton.html)
  e [source 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/scene/gui/base_button.cpp):
  prioridade de Disabled e Pressed condicionado à posição dentro do controle
  orientam captura/estado. Hover/foco da referência não são suporte desta engine.
- [Unity Learn, Working with UI](https://learn.unity.com/tutorial/working-with-ui-in-unity):
  seleção, Inspector e teste em Play orientam o fluxo. A pesquisa também localizou
  o vídeo oficial [UI Guide — All About Buttons](https://www.youtube.com/watch?v=l0QwB7xafl4);
  a listagem dos capítulos foi consultada, sem afirmar observação quadro a quadro.

## Evidências

Arquivos locais em `build/ui-states-actions-20261003/` (não são mocks):

- `native-tests-final.log`: 51/51 cenários. Três cenários novos protegem sequência,
  serialização/histórico/duplicação, ciclos e estados interrompidos, composição,
  gesto com escala reduzida e opacidade zero. O cenário de Play existente valida
  os novos callbacks no ScriptBridge real, incluindo campos com efeito e erro
  transacional de enums inválidos.
- `managed-tests.log`: 5 verificações do SDK, incluindo
  tamanhos/offsets ABI e edição independente. O adapter de teste prova o contrato
  C#; o exemplo executado no Android prova a ponte nativa e runtime.
- `android-build-final.log`: APK debug montado e instalado no Xiaomi 25053PC47G.
  A revisão final foi instalada em 2026-10-03 20:17:11, versão 0.2.0-editor-ui.
  `manifest.json` registra o SHA-256 do APK e das evidências deste pacote.
- `device-play-api.png`: Start do C# adiciona/remove/reordena/substitui ações e
  escreve/lê Transitions na ponte real; resultado `API: 2 acoes adicionais | estados: True`.
- `device-play-pressed.png`: gesto mantido, ícone reduzido e colorido sem fundo.
- `device-play-click-final.png`: um clique em Image abre e anima o painel,
  desabilita outro ícone e chega a C# (`1 cliques | open_image | Image`).
- `device-play-disabled-hit.png` e `device-play-reenabled.png`: estado desabilitado
  bloqueia clique; reabilitação restaura a aparência e o evento seguinte.
- `device-saved-opacity.aeui`, `device-saved-undo.aeui`, `device-saved-redo.aeui`:
  Inspector grava opacidade pressionada 0.49000001, Undo retorna 1, Redo restaura
  0.49000001. O projeto final reabre esse arquivo.
- `device-added-action.aeui`, `device-removed-action.aeui` e
  `device-reorder-confirmed.aeui`: criação aumenta a lista a 3; remoção a reduz a 1;
  reordenação grava ToggleEnabled antes de PlayAnimation. O arquivo final restaura
  a ordem PlayAnimation/ToggleEnabled e mantém a opacidade pressionada editada.
  Gestos sintéticos de 150 ms foram usados para conferir botões ao longo de frames.
- `device-final-ui.png`: dois ícones novos, ícone de Image preservado, grupos
  recolhíveis e labels do exemplo sem sobreposição. Nova captura após a correção
  de clipping fica em `device-final-scroll.png`.

Projeto físico: `Projetos/UIEstadosAcoes-20261003`, com três ícones privados do
acervo Synty já importado. Exemplo publicável usa somente o PNG próprio existente:
`examples/ui/states-actions.aeui` + `GuiStatesActions.cs`; o helper cria um projeto
com cena/script/UI/asset reais usando modo `states`.

## Limites e sequência restante

Três estados, quatro curvas, escala uniforme/XY/alpha e tinta local. Sem Hover,
foco/gamepad, Selected, múltiplos ponteiros, propagação de eventos, máscara por
alpha/forma, sprite swap, temas/skins completos, binding de dados ou timeline.
O custo medido pelo teste é CPU host com 1024 animações, sem GPU, não garantia
de frame time no Android. Faltam também joystick com action/player e autoridade
de personagem/corpo dinâmico, previstos em F/G; esta entrega não os simula.

Critério de fechamento deste pacote: editar/salvar/reabrir ações e estados,
usar a sequência no Preview/Play, receber evento e configurar por C#, manter
fundo opcional e lidar com cancelamento, disable, erro e referências removidas.
O restante do roadmap continua marcado parcial/planejado.

Na conclusão da validação, as alterações ainda estavam locais. A publicação na
`main` foi solicitada posteriormente. O origin foi novamente conferido como
`https://github.com/kacerato/attachsEngine.git`.
