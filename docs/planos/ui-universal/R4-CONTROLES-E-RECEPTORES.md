# Controles autorados e receptores — entrega de 04/10/2026

Trabalho em `https://github.com/kacerato/attachsEngine`, checkout
`C:/Users/donod/Downloads/atchengine`. O roadmap original permanece íntegro.
Esta entrega amplia R4; não conclui o roadmap nem a engine.

## Caminho utilizável

Na Hierarquia, selecione o Canvas, crie Joystick, ActionButton ou LookArea e
configure uma ação existente em Entrada do jogo. No componente Canvas, escolha
Jogador/receptor, câmera e espaço de movimento. A referência aceita Character
ou DynamicBodyMotor com Body dinâmico sólido e Collider. Body sozinho não é
convertido implicitamente em um motor.

O documento guarda os controles; o componente guarda os alvos. Dois Canvas
podem usar a mesma fonte e jogadores diferentes. A cena de aceite é criada por
`aether_gui_preview write-controls DIRETORIO_NOVO`; contém Character, filho
CylinderVisual sem autoridade física própria, chão estático, CameraLook,
CameraFollow e HUD. A entrega inicial foi validada em **UI Authored Controls v2**.
A continuação adiciona a receita no menu e conversão transacional de uma malha
existente, documentadas em [R4-AUTORIA-CHARACTER.md](R4-AUTORIA-CHARACTER.md).
O gerador atual usa essa mesma receita e publica **UI Authored Controls v3**;
`write-conversion` gera **UI Character Conversion v3** para exercitar a conversão
pelo editor, sem atribuir implicitamente o receptor do Canvas.

Joystick oferece origem fixa, flutuante no toque e dinâmica que acompanha o
dedo, eixos, limite circular/quadrado, raio de entrada, zonas mortas interna e
externa, expoente e sensibilidade. Base e puxador têm cores, raios, imagens e
visibilidade independentes. O fundo do retângulo é opcional e começa transparente.
Soltar zera a ação imediatamente; o retorno animado afeta apenas o desenho.

ActionButton gera Button/Press no toque inicial, conserva o estado pressionado
e solta sem depender de um evento Click. Seu clique ao soltar também usa as
ações ordenadas existentes. LookArea acumula deslocamento normalizado pela
área, consumido uma vez por amostra. Joystick e LookArea podem emitir um clique
adicional quando explicitamente configurados.

## Entrada, motor e ciclo de vida

As contribuições têm identidade `(lease do Canvas, GuiId)`. Vetores usam a
maior magnitude, com empate a favor do hardware; botões usam OR. Cancelar uma
fonte não solta outra fonte da mesma ação. O mapa aplica suas próprias curvas,
contextos, enable e filtros de dispositivo depois do processamento do controle.
Consequentemente, zona morta no joystick e no mapa se compõem.

Sem receptor, um Canvas publica no mapa global. Com Character ou motor dinâmico, publica em um
serviço independente que herda o mapa e a política globais. Os papéis do mapa
selecionam mover, olhar e saltar; nomes não são impostos pelo motor. Espaço
local/câmera usa a matriz mundial atual. CameraLook recebe apenas o delta da
câmera atribuída. A física existente continua proprietária da pose; o motor
dinâmico aplica forças ao Body, descrito em [R4-MOTOR-DINAMICO.md](R4-MOTOR-DINAMICO.md).

O salto possui fila limitada a 32 eventos por receptor, identificando Canvas,
node e geração de cancelamento. Um toque curto pode preceder FixedUpdate;
desativar, ocultar, editar o controle, perder foco ou retirar seu proprietário
cancela eventos obsoletos. Cada tentativa é consumida uma vez. A fila não é uma
permissão de salto aéreo: o grounding do motor decide se aceita a tentativa.
Transbordamento e vínculo/receiver/câmera inválidos geram diagnóstico.

## Persistência e API

AEUI 5 acrescenta os dados de controle e lê AEUI 1–4. UiCanvas v2 acrescenta
referências de jogador/câmera e espaço, com defaults para v1. Referências usam
o contrato existente de clonagem/remapeamento da cena. História, Save e abertura
continuam passando pelo mesmo documento e pelo journal do AssetRegistry.

ABI 43 conserva o layout da tabela de funções e amplia o despacho tipado.
SDK e native devem ser distribuídos juntos. `Gui.ForCanvas(owner).Find(name)`
expõe `ControlSettings`, `InputAction`, `BaseImage`, `KnobImage` e o estado bruto
`Input`. Em LookArea, o delta bruto pode já ter sido drenado pelo hospedeiro;
para ler a ação amostrada, use `Gui.ForCanvas(owner).Input.Axis2(action)`.
Esse contexto também lê Pressed, JustPressed, JustReleased e ActionState de
ações próprias, inclusive as que não são papéis do motor. A política é alterada
no Input global; a leitura por Canvas não inventa políticas locais persistentes.

O encaminhamento de ações exige um Canvas de cena. O runtime global legado
continua disponível para UI simples/estado bruto, mas não hospeda uma segunda
cadeia de ações autoradas. Não confundir sua leitura global com a leitura do
receptor obtida por ForCanvas.

## Limites abertos

Há 13 tipos de node, 64 Canvas, 32 receptores Character/motor dinâmico e 32 capturas por
documento. Base/puxador usam o atlas de imagens existente, com seus limites.
Ainda faltam PlayerInput/pareamento de hardware por jogador, receptor genérico,
buffer geral de eventos para
scripts em FixedUpdate, Hold/Tap no botão autorado, knockback/respawn e diagnóstico
de motor/autoridade por alvo no Inspector. O modo Interagir mostra o vetor local;
não substitui inspeção remota completa do Play. O aceite físico de três dedos e
de todos os modos/imagens do joystick continua pendente.

## Referências concretas

- [Unity Input System 1.14.2: On-screen Controls](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.14/manual/OnScreen.html), API e workflow: separar a representação visual da geração de entrada. Aqui, a imagem usa o atlas real e a ação usa o mapa existente; o alvo pertence ao Canvas.
- [Fonte oficial OnScreenStick.cs, tag 1.14.2](https://github.com/Unity-Technologies/InputSystem/blob/1.14.2/Packages/com.unity.inputsystem/InputSystem/Plugins/OnScreen/OnScreenStick.cs): estudar ownership do ponteiro, origem e retorno; não introduzimos troca automática de dispositivo virtual.
- [Godot 4.5: Input](https://docs.godotengine.org/en/4.5/classes/class_input.html): ações e amostragem são distintas do encaminhamento de eventos da UI. A adaptação conserva contribuições identificadas para não liberar outro controle ao cancelar uma fonte.

Ícones novos foram gerados pelas primitivas da marca em
`tools/generate-gui-behavior-icons.py` e integrados pelo atlas real; não são
imagens conceituais usadas como evidência. O relatório de validação registra
host, SDK, pacote e aparelho separadamente.
