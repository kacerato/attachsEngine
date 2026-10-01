# Captura de vínculos no editor — P06

A captura altera o InputActionMap existente, não cria outro mapa de entrada. Teclado, botão de gamepad e eixo de gamepad usam os códigos e os oito eixos lógicos já consumidos por InputService. O resultado percorre EditorSession → EditorHistory → SceneGraph → arquivo de cena → GameWorld/InputService.

## Referências e adaptação

Unity Input System **1.11.2** documenta [interactive rebinding](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/ActionBindings.html#interactive-rebinding); o [código oficial da operação](https://github.com/Unity-Technologies/InputSystem/blob/1.11.2/Packages/com.unity.inputsystem/InputSystem/Actions/InputActionRebindingExtensions.cs) distingue conclusão, cancelamento e seleção de controle. Astra conserva seu próprio modelo de ações e códigos Android. A operação aqui é autoria do projeto; não promete perfis de rebind do jogador ou compatibilidade com os control paths da Unity.

O [vídeo oficial Unity sobre rebind](https://www.youtube.com/watch?v=JfuqMaOiNPs) foi localizado como referência de workflow. Seus quadros não foram inspecionados nesta entrega; decisões visuais abaixo foram verificadas no editor executável.

## Contrato

Uma captura guarda época/revisão da cena, projeto, mapa anterior, ação e índice do vínculo. Mudança desses dados, foco perdido, suspensão, início de Play ou cancelamento descartam a operação. Não há mutação de documento enquanto a operação aguarda entrada. A conclusão válida produz uma alteração de histórico; Undo/Redo restaura o mapa completo.

Teclado exige nova pressão: release e repetição não são candidatos. Escape/Voltar cancelam; Home, volume e energia ficam com o sistema. Fonte incompatível não modifica o vínculo. Na metade negativa de uma ação de eixo, uma tecla igual à positiva é recusada. Eixos precisam passar pela zona neutra (<0,2) antes de cruzar 0,65; o sinal escolhe inversão, preservando eixo de saída e escala. Isso impede que um stick já inclinado seja aceito imediatamente. Valores não finitos ou fora do domínio físico são ignorados.

## Interface

NÃO IREI SER SIMPLISTA NO DESIGN.

Captura é um estado da superfície de Input, com ação/vínculo visíveis, instrução central, fonte atual, feedback e Cancelar. Não abre outro painel sobre o viewport. A linha Fonte divide edição manual e Capturar; a metade negativa tem sua própria ação. Os novos ícones de teclado e gamepad passam pelo gerador SVG e pelo atlas usado pelo renderer. Nenhum controle novo é apenas conceitual.

O conceito gerado partiu da captura executável anterior e foi usado para revisar hierarquia e foco. A implementação usa desenho nativo, não a imagem. [Evidências, hashes e limites](../validacao/evidencias/input-capture-20260930/README.md).

## Aceite

Abrir Input → vínculo de teclado → Capturar → nova tecla → mapa atualizado → Undo/Redo → salvar/reabrir → InputService aciona a ação correspondente. Para eixo: stick já inclinado não conclui; neutralizar e inclinar conclui com direção correta. Cancelamento e mudança de documento não deixam comandos parciais.

Captura por mouse, perfis persistentes de jogador, rebinding durante gameplay e conexões autoráveis de eventos são capacidades distintas, não implementadas por esta operação. P06 permanece parcial.
