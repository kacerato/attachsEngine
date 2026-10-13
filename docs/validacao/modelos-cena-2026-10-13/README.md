# Modelos de cena, joystick do Play e carregar com as duas mãos — validação (13/10/2026)

Referências: Final IK 2.2, `_DEMOS/FBBIK/Interaction Pick Up 2-Handed.unity` e
`Scripts/PickUp2Handed.cs` (duas mãos na mesma InteractionObject, ponto de carregar);
Unity 6000.0, Avatar › Configure › Mapping › Automap
(https://docs.unity3d.com/6000.0/Documentation/Manual/ConfiguringtheAvatar.html) para o
reconhecimento de ossos pelo nome.

## Host (Windows/MinGW)
aether_tests 1558/1558, runtime de animação 77/77, editor de clipes 65/65 (cenário dos
modelos: manequim, interação, braço, quadrúpede, personagem jogável, nomes de ossos Kyle/
Mixamo/Blender/Astra, carregar com as duas mãos — agacha, mãos nas pegadas, caixa no ponto,
levanta, Desfazer/Refazer; preenchimento universal de IK de corpo inteiro e grounder),
GUI 126/126, bulk 4/4 com API C# regenerada.

`aether_gui_preview write-kyle-carry <RobotKyle.glb> <destino>`: projeto novo só com o Robot
Kyle (UMotion Pro, receita `tools/animation-studio/robot-kyle.recipe.json`), palco com colisão,
luz e a interação pelos componentes universais, sem clipe. Play no host: mão esquerda e direita
nas pegadas e caixa no ponto de carregar com desvio < 1 mm.

## Aparelho (Astra Dev code 57–59)
- `personagem-jogavel-joystick.png`, `personagem-jogavel-passagem.png`: joystick flutuante
  desenhado, Personagem anda até a passagem baixa, câmera seguidora, botão Saltar. Antes o
  toque só movia o Personagem se ele estivesse selecionado.
- `painel-modelos.png`: painel em cartões (versão de 6 modelos; a paginação veio depois).
- `carregar-duas-maos.mp4`, `-folha.png`, `-sequencia.png`: manequim agacha/inclina até a
  caixa (1,6 s), levanta (2,2 s) e a carrega junto ao corpo com os dois antebraços (6,5 s).
- `kyle-carregar-caixa.mp4`, `-sequencia.png`, `kyle-montagem.png`: Kyle no projeto dos
  pacotes. Primeira rodada revelou a caixa saltando na pegada (troca de pai enfileirada pelo
  mundo e pose escrita no mesmo quadro); corrigido no code 59: sem salto. Essa cena ainda
  tinha o clipe do braço do projeto e os Vikings, que interferiam; por isso o projeto novo
  "Kyle – Carregar caixa" (só o Kyle, sem clipe) foi gerado e enviado ao aparelho.

## Mudanças após a revisão do proprietário
- O atalho "Carregar caixa com as duas mãos" do menu do objeto foi removido: montar uma
  situação única não é capacidade universal. Ficou o preenchimento universal de ossos ao
  adicionar Sistema de interação, IK de corpo inteiro ou Grounder do corpo.
- Painel de modelos paginado (6 por página) entre as barras da vista; ícone novo
  `component/interaction-carry` no atlas real.

## Pendências
- Play do projeto "Kyle – Carregar caixa" no aparelho (aparelho em uso pelo proprietário).
- Rotação da mão pela pegada (InteractionTarget), gatilho por proximidade e olhar durante a
  interação: ver `docs/planos/execucao-2026-10-10/ALINHAMENTO-IK-UMOTION-ANIMATOR.md`.
- Clipes Humanoid do UMotion (PickUp.anim) não convertem: curvas de músculo.
