# N04 pacote C — validação no aparelho (12/10/2026)

Astra Dev code 48, projeto AnimatorFox.

- `busca-inercia.png`: Adicionar componente → busca "Inertia" encontra **Inércia** (Lógica · IK), com ícone novo.
- `inercia-inspector.png`: componente no osso b_Neck_04; inspector em abas Influência/Mola; ícone no viewport.
- `play.png`: Play com Inércia ativa, sem falha no logcat.

Host: runtime de animação 58/58 (10 cenários novos: impacto, recuo, inércia, amplificador,
inclinação, poser, penetração, interação com pegar/soltar e eventos, corpo inteiro, persistência),
editor de clipes 61/61, GUI 126/126, aether_tests 1551/1554 (3 falhas intermitentes de
gravação de arquivo, código 5; não relacionadas).

Não testado no aparelho: interação, recuo e golpe dependem de script/conexão; validados no host.

## Revisão sem pendências (code 49)

- `busca-vrik.png`: busca "VRIK" encontra **Corpo por alvos (VRIK)** com ícone novo.
- `vrik-inspector.png`: componente no Fox com abas Influência/Corpo/Cabeça/Mãos; referências e opção de rotação da cabeça.
- `vrik-play.png`: Play sem falha (componente sem alvos reporta diagnóstico, sem mexer na pose). Componente desfeito depois do teste.

Host: aether_tests 1555/1555, runtime de animação 64/64 (FABRIK em árvore, corpo por alvos, prioridade,
membros longos, carregar corpo físico, golpe de frente, versões antigas), editor de clipes 62/62
(assistente pela estrutura; fixar ponta abaixo de 2 mm), GUI 126/126.
