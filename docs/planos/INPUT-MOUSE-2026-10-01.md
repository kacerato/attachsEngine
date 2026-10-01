# Mouse — entrada autorável, cena 16

Esta fatia amplia P06; não representa paridade completa com Input System.

## Referências e decisão

Unity Input System **1.11.2**: [Mouse](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/api/UnityEngine.InputSystem.Mouse.html), [Pointer](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/api/UnityEngine.InputSystem.Pointer.html) e [fonte Mouse.cs](https://github.com/Unity-Technologies/InputSystem/blob/1.11.2/Packages/com.unity.inputsystem/InputSystem/Devices/Mouse.cs). A separação entre botões, deslocamento e scroll e o acúmulo/reset por atualização orientam a implementação. A Astra usa frações do viewport para consumir o CameraLook existente; não transporta as unidades em pixels da Unity.

Godot **4.5**: [InputEventMouseMotion](https://docs.godotengine.org/en/4.5/classes/class_inputeventmousemotion.html) distingue movimento de posição e mantém semântica de tela. Android: [MotionEvent](https://developer.android.com/reference/android/view/MotionEvent#getButtonState()) e [NDK Input](https://developer.android.com/ndk/reference/group/input). A captura usa diferença de buttonState, disponível no mínimo Android 26, sem depender de getActionButton do Android 33.

## Cadeia funcional

MotionEvent → AndroidGameInputState → InputDeviceState → mapa de ações → InputService → EditorPlayScene → C# InputAccess / CameraLook.

Editor → vínculo nomeado/captura → uma alteração de histórico → arquivo de cena v16 → mapa no Play. Mouse e toque possuem identidades distintas; mouse no viewport não produz o controle virtual de toque. Controles do editor, foco, IME e lifecycle têm precedência sobre gameplay.

Fontes novas: MouseButton=7 e MouseAxis=8. Botões: principal, secundário, meio, voltar, avançar. Canais: X para direita, Y para baixo, scroll horizontal e vertical. Movimento acumula em frações do viewport; canais transitórios zeram após cada quadro, botões mantidos persistem. Clique down/up entre quadros produz um quadro pressionado e soltura no seguinte; múltiplos cliques no mesmo quadro são coalescidos. Scroll e deslocamento são limitados a [-1,1] antes de sensibilidade/escala. Zona morta continua propriedade real da ação; para movimento fino, use zero como na fixture.

Um mouse é proprietário do estado. Troca de proprietário reinicia a posição, desconexão libera somente seu estado, perda de foco limpa entrada. Não existem nesta fatia pointer capture relativo ilimitado, dupla contagem de cliques, API de posição absoluta de UI de jogo ou roteamento de vários jogadores.

## Editor e compatibilidade

O mapa usa a superfície contextual existente. Botão/Canal alterna opções com nomes; Capturar recebe botões, e clicar em Cancelar cancela sem gravar o clique. Eixos são escolhidos explicitamente, sem captura de movimento acidental. Undo/Redo e arquivo preservam fonte/canal/escala/inversão. O ícone input/mouse entra no SVG e atlas reais (226 ícones).

Cena v16 admite novas fontes. Versões anteriores continuam legíveis, mas não aceitam identificadores novos disfarçados de contrato antigo. ABI25 permanece igual; 32 schemas e 31 fachadas, nenhum componente novo.

## Aceite e evidência

Criar vínculo → escolher botão nomeado → capturar outro → cancelar sem alterar → Undo/Redo → salvar/reabrir → Play. Movimento deve atingir CameraLook uma vez; perder foco deve bloquear; Stop deve restaurar autoria. [Evidências executáveis e resultados](../validacao/evidencias/input-mouse-scene16-20261001/README.md).

APK instalado e assets comparados. Telefone permaneceu com keyguard seguro: mouse físico, captura Android e MOUSE PASS não foram qualificados. Capturas host são renderizações executáveis da UI, não prova de uso no aparelho.
