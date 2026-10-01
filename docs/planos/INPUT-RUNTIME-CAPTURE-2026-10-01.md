# Captura de rebind em gameplay — ABI27

Referência: Unity Input System **1.11.2**, [Interactive rebinding](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/ActionBindings.html#interactive-rebinding). O princípio extraído é selecionar um vínculo existente, interromper sua execução enquanto escolhe o controle e aplicar um override separado da autoria. A Astra usa fontes tipadas existentes, não paths genéricos da Unity.

## Caminho real

Android → estado de dispositivo → InputService.capture → mapa efetivo → perfil do jogador / consumidores de gameplay. A captura acontece no serviço de entrada da sessão, sem usar o histórico ou estado de UI do editor.

C# `Input.BeginBindingCapture(action,index,source,negative,cancelKey)`, `BindingCaptureStatus` e `CancelBindingCapture` atravessam callback obrigatório da ABI27. Estados: Idle, Waiting, Completed, Cancelled. A API recusa ação/índice ausente, fonte não capturável, segunda captura concorrente, falta de foco e tecla negativa em ação Button. Fontes capturáveis: Key, GamepadButton, GamepadAxis e MouseButton. Touch e MouseAxis não são capturados por esta API.

O primeiro estado recebido é a referência inicial; controles já mantidos não são escolhidos. Tecla/botão exige nova pressão. Eixo deve passar por neutralidade <0,2 e depois ultrapassar 0,65; direção negativa modifica a inversão. Tecla negativa deve diferir da positiva e requer vínculo Key existente. Saída, escala e demais valores existentes permanecem preservados.

Enquanto aguarda, todas as ações de gameplay leem zero. No quadro que completa, o evento também é bloqueado. O controle aceito precisa ser solto/neutralizado antes de voltar a produzir ação, impedindo que escolher uma tecla cause salto. Cancelamento explícito ou nova pressão da tecla configurada (Escape Android111 por padrão; zero desabilita) não altera o vínculo. Teclas reservadas pelo Android permanecem com o sistema. O menu de jogo pode chamar CancelBindingCapture por seu callback; esta fatia não apresenta uma UI de jogo inexistente como implementada.

Perda de foco, pausa do aplicativo, pausa de Play e Stop cancelam a espera. Substituir o mapa limpa captura e contextos transitórios da sessão. Importar perfil ou alterar diretamente um vínculo enquanto aguarda é recusado; restaurar todos os defaults cancela antes. Contextos e foco são preservados quando se conclui/importa um override compatível.

O produtor Android agora preserva down/up de teclado e gamepad ocorrido entre quadros: um quadro pressionado e soltura no seguinte, assim como mouse. Repetição de tecla não cria novos pulsos. Desconexão/troca de proprietário/foco limpam também eventos preservados. Múltiplos cliques no mesmo quadro são coalescidos; não há promessa de fila completa de eventos ou múltiplos jogadores.

## Aceite e limites

Começar captura → pressionar uma tecla nova → consultar vínculo → salvar perfil → soltar → pressionar novamente → ação chegar ao consumidor. Escape/cancel/foco/suspensão não devem alterar o vínculo. Capturar eixo já inclinado exige neutralidade. Cenário C# independente: RuntimeCapture-20261001, com logs RUNTIME CAPTURE READY / COMMITTED / PASS e perfil separado.

[Evidências](../validacao/evidencias/input-runtime-capture-abi27-20261001/README.md): captura4/4, entrada48/48, lifecycle nativo, regressões de consumidores, C#54/54 e sete fixtures compiláveis. SDK e APK devem usar ABI27 juntos. Nenhum schema/fachada/ícone de componente foi adicionado: 32/31/226, cena16, Timer3, prefab3.

Captura interativa da API está implementada e verificada em host; menu autorável de gameplay e qualificação física Android continuam pendentes. Isso não conclui P06 nem os demais pacotes do atlas.
