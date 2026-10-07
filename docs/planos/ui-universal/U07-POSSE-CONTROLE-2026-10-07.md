# U07 — fontes e posse de controle

Bloco selecionado para fechamento integral nesta sessão. Repositório attachsEngine;
base física 09bd7349, integrada à revisão 4081cad9 de câmera virtual/Cérebro,
preservando o merge e a autoria física anterior. U03/U06/U08
continuam distintos; este bloco fecha U07, sem anunciar seus modos/animações.

Intenção → InputActions por origem → arbitragem por motor → força/Character →
diagnóstico. A posse de entrada não muda a autoridade física nem a malha.

Contrato: cinco canais (UI, teclado/mouse, gamepad, script, IA) por destinatário.
Automático escolhe a maior prioridade; empate tem ordem estável IA > Script >
Gamepad > Teclado > UI. Fonte fixa é posse exclusiva, inclusive no repouso.
Script/IA enviam comandos a cada Update/FixedUpdate; zero é uma intenção de parar.
UI/hardware neutros liberam sua contribuição. Salto acompanha a fonte vencedora;
pulsos perdedores são descartados, sem salto atrasado após troca de posse.
Cada canal recebe o último comando no quadro, na ordem real de execução.
Esta política não é gerenciamento de usuários/dispositivos individuais de rede.

Critérios obrigatórios: (1) política autorável nos dois motores reais; (2) separar
UI/teclado/gamepad sem perder ações, rebinds ou foco; (3) comandos tipados Script/IA,
release e estado; (4) troca em Play sem rebuild de corpo; (5) cancelar em perda de
foco, pausa, desativação, remoção, desconexão e troca de receptor; (6) arquivo,
prefab e Undo/Redo; (7) diagnóstico da origem efetiva; (8) cenário real host e Android.

NÃO IREI SER SIMPLISTA NO DESIGN. A posse fica no Inspector do motor existente,
com fonte exclusiva ou prioridades em Automático. O diagnóstico contextual
mostra a origem vencedora e candidatos. Nenhum painel permanente no viewport.

Referências: Godot 4.5 [Input](https://docs.godotengine.org/en/4.5/classes/class_input.html)
e [InputMap](https://docs.godotengine.org/en/4.5/classes/class_inputmap.html), ações,
release e deadzone; [source 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/core/input/input.cpp),
estado por origem. Unity Input System 1.4.3
[PlayerInput](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.4/api/UnityEngine.InputSystem.PlayerInput.html),
destinatário e troca de esquema. Adaptamos ao InputService/SceneGui existentes,
sem presumir paridade de multiplayer ou criar outro controlador físico.

Estado: **U07 fechado e aceito em 07/10/2026**, no contrato das cinco fontes e
arbitragem por motor. Os oito critérios acima têm consumidores e evidência no
[relatório](../../validacao/u07-2026-10-07/REPORT.md) e no
[registro](../../validacao/u07-2026-10-07/acceptance.json): 8/8 direcionados,
117/117 regressões, Behavior compilado pelo ProjectCompiler real, build/instalação
Android com SHA igual e autoria/Play/salvar/desfazer/refazer/reabertura fria no
POCO F7. Gamepad físico Android não conectado; origem, rebind e desconexão
verificados no host. Não representa pareamento multiplayer ou rede.


Correção de terceira pessoa solicitada durante o aceite: CameraFollow v2 conserva
v1 com deslocamento em mundo. Orbitar alvo gira o offset pela orientação da câmera;
Altura do pivô adiciona o ponto observado em Y do mundo. CameraLook mantém rotação,
CameraFollow possui posição após física/LateUpdate. O laboratório usa distância
5 m, pivô 1,1 m e sem atraso de posição para manter o alvo enquadrado ao arrastar.
O Body do humano bloqueia X/Z, permite Y; FixedUpdate envia somente velocidade
angular Y proporcional ao erro circular entre facing e intenção vencedora, com
limite de 8 rad/s. Preserva posição e velocidade linear do solver e a colisão gira
junto do corpo, sem reconstruir ou girar uma malha apartada de seu collider.
Referência: Godot 4.5, terceira pessoa com pivô/órbita:
https://docs.godotengine.org/en/4.5/tutorials/3d/spring_arm.html
Princípio extraído: órbita independente do facing e intenção relativa à câmera.
CameraFollow não faz varredura de paredes. O componente Braço de mola existe
separadamente; o laboratório aberto não o configura nem declara desoclusão
proveniente de CameraFollow. A câmera virtual/Cérebro incorporada pelo proprietário
permanece um sistema distinto, preservado nesta entrega.

O aceite quadro a quadro encontrou uma inversão no primeiro vídeo: publicar o
TRS completo canonicalizava yaw ao cruzar 90° e invertia pitch/roll. CameraFollow
agora publica somente a posição local, preservando os ângulos de CameraLook.
Uma volta completa de 120 passos está protegida no cenário nativo; os 200 quadros
da nova gravação Android foram todos examinados e aprovados, sem amostragem.
Os 353 quadros do vídeo rejeitado e a causa estão registrados separadamente.

Laboratório entregue em `examples/ui/U07Laboratorio`, com fontes dos dois rigs,
script editável, cena, Canvas, textura PBR e atmosfera. A reprodução de um clipe
por velocidade medida demonstra o consumidor existente; não fecha U08, root
motion, retargeting ou grafo universal de animação. U03/U06, UI componível,
modelagem visual completa e SDK/rede/custo amplo continuam nos blocos próprios.

## Correção integrada: movimento, animação e câmera — 07/10/2026

A revisão posterior substitui a amostra de um clipe/CesiumMan por oito clipes
Godot TPS e movimento medido, controle aéreo real, câmera virtual/Cérebro com
varredura contra piso e catálogo Current antes de Play. APK embute só o laboratório.
12/12 direcionados, 121/121 regressões no reteste, ProjectCompiler 1/1, ProjectStore
7/7; 527 quadros examinados, principal atualizado e projetos do usuário preservados.
Os parágrafos acima ficam históricos. [Contrato e limites](U07-MOVIMENTO-ANIMACAO-CAMERA-2026-10-07.md)
e [evidência atual](../../validacao/u07-2026-10-07/REPORT.md). U08 amplo e os outros
blocos não são declarados completos por esta correção.
