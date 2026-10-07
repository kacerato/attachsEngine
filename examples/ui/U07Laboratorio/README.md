# Laboratório de controle, movimento e câmera

Único projeto predefinido do APK. Abra e entre em Play. A fonte inicial é UI:
soltar o joystick para o jogador, sem a IA retomar o movimento automaticamente.

- Joystick: movimento relativo à câmera, corpo girando com a intenção.
- Arraste na metade direita: órbita da câmera virtual; esfera .25 m evita o piso.
- Saltar: impulso, subida, ápice, queda e aterrissagem com clipes reais; a
  caminhada só ocorre no chão. Controle aéreo .85, configurável no Motor.
- Fonte: alterna Automático/UI/teclado/gamepad/Script/IA. Em Automático, os
  comandos IA e Script demonstram arbitragem, não navegação autônoma.
- Script: alterna a origem Script; PrioridadeIA alterna 5/50.

Scripts/U07Controller.cs é editável. Usa MotorAnimationDriver com nomes de
 oito clipes e passadas calibradas na fonte: Walk 1.75 m, Run 2.666667 m.
O controlador usa apoio e velocidade do solver, descontando velocidade do
suporte. Repouso é um clipe independente; não congela uma pose de caminhada.

Humanoide de teste Godot TPS Demo: 145 juntas, fontes e atribuição em Sources.
Track dedicado de root motion removido na derivação: o Body controla o
 deslocamento. Fox é um segundo rig visível. Piso PBR de concreto e atmosfera
física são recursos editáveis da cena, não um fundo de demonstração.

No Inspector, configure Motor dinâmico, Body/colisores e Câmera virtual. O
Canvas referencia jogador e câmera real/Cérebro. Propriedades de Play não
persistem automaticamente; para autoria, saia de Play, altere e salve.

Não implementa IK de pés, retargeting, root motion em runtime, editor visual
universal de blend trees ou todo U08. A câmera precisa de colliders e filtros
adequados; a cena usa chão físico e raio superior à distância do plano próximo.

package-files.txt define os arquivos embutidos e impede enviar caches/fontes
antigos não referenciados. Projeto independente: cena, registro, fontes,
texturas, caches necessários, Canvas e script acompanham o APK.
