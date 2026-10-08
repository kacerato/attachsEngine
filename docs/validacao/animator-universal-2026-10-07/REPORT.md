# Animator universal — workspace e fontes físicas

Revisão iniciada em 07/10/2026; aceite Android solicitado em 08/10/2026. Repo confirmado: kacerato/attachsEngine, baseline 88dec944f6ce58a9774477549fb1b5c7e6d90ddf. Bloco de workspace e vínculos físicos aceito no host e no aparelho em 08/10/2026. Não é declaração de conclusão de U03/U06/U08.

## Capacidade e fluxo

Modelo v2 → editor/histórico/archive → fontes tipadas manuais ou físicas → avaliador de grafo → mixer e canais de transform existentes. Raiz visual e fonte física independentes, ambas referências remapeáveis em prefab. Sem solver/rig/motor paralelo. V1 conserva parâmetros manuais. Não há tracks arbitrários de propriedades, root motion, IK, retargeting ou controller compartilhado nesta entrega.

Grafo dominante com uma gaveta contextual. Novos oito ícones estão no atlas/vetor real. Autoria inclui duplicação com nova identidade, prioridade de transições, rolagem integral e pinça; Play permite parâmetros manuais sem sobrescrever defaults. Defaults, histórico e posições são dados autorais; valores vivos e navegação são temporários.

## Bugs detectados e corrigidos durante o aceite

- O espelho genérico da Inspeção em Play redirecionava ações do Animator para defaults autorais. Os widgets do grafo agora seguem a autoridade runtime; o diálogo numérico reconhece o contexto do espelho.
- A rolagem era armazenada, mas o desenho usava a extensão ainda vazia do layout novo e voltava ao topo. Desenho usa o offset, medição concluída limita o offset antes da pintura final. O teste verifica o alvo visível do oitavo evento e ausência de alvo dos campos que saíram da tela.
- O primeiro dedo Android usa ID zero, que coincidia com a sentinela interna de ausência de captura. A captura do Animator converte IDs para um domínio interno a partir de 1; a pinça real com dedos 0/1 passou e ganhou regressão integrada no host.
- Clang Android recusou indentação ambígua na reordenação de transições; estrutura de controle foi explicitada, sem reduzir warnings.

## Host e SDK

Nove cenários focados do Animator: mistura 1D/2D, archive/validação, transição/gatilhos/tempo de saída, sampling/pose, máscara/eventos, migração v1/v2 e referências de prefab, corpo real e diagnóstico de ausência, ambos os motores com apoio medido, e EditorSession real com seleção/duplicação/histórico/pinça/rolagem/Play/defaults. Resultado final em host-animator.log.

SDK Astra.Scripting Release: zero warnings e zero erros (sdk-build.log). Parâmetro vinculado recusa setter com WorldStatus.Rejected. Configuração aninhada via SDK permanece no roadmap; Target e MotionSource são referências tipadas reais da fachada.

Capturas host são a UI executável com atlas real, não provas de GPU/aparelho. Conceito anterior com personagem é apenas hipótese de design. O exemplo de porta das capturas de autoria tem clipes não atribuídos; não é prova de pose. A cena física de aceite usa painel glTF sem skin, clipes reais e fonte física independente.

## Android

Build Release e SDK concluídos; Dev 0.2.5-dev.20261007/code 13 instalado com `install -r`, sem desinstalar nem limpar dados. Hash do APK instalado idêntico ao artefato local: 3b9dfaecdc9cf81462f2afa4549474c6da3dd1c553e0b519d365c1634a7df02c. Manifesto inclui hash da biblioteca nativa. Público continua 0.2.3-preview.20261007/code 12. Projetos anteriores preservados; backup do projeto Fox antes do aceite, nova cena de mecanismo isolada.

POCO F7/Android 16: painel glTF sem skin com três clipes reais; fonte física é outro objeto. Runtime mediu velocidade 1.500, mudou a pose, recusou setter vinculado, mediu 0.000 após parar e restaurou a pose. Logs registram Float manual 0.750, Bool manual true e gatilho entrando no estado 5 e retornando ao 2. Comandos ocorreram pela UI real em Inspecionar/Play, sem substituir defaults.

Pinça Android com dedos 0/1, rolagem até o parâmetro 32 e evento 8, duplicar → desfazer → refazer, salvar → encerrar app → abrir novamente passaram. Arquivo salvo foi extraído e inspecionado: três IDs únicos de estado, 32 parâmetros, três clipes/oito eventos iguais na cópia, MotionSource 4, resposta 0.12 e defaults manuais ainda zero. Capturas de reabertura confirmam esses dados na UI; JSON conserva a leitura do archive.

`device-motion.mp4`: 717 frames decodificados/extratos, nenhum pulado. Todas as 24 folhas indexadas foram inspecionadas quadro a quadro na região do mecanismo; frames completos 0/13/222/270/716 conferidos adicionalmente. Abertura contínua nos frames 13–59, pose aberta estável 60–222, retorno contínuo 223–269, repouso estável 270–716. `frames.csv` retém PTS e hashes, `review.json` registra método, intervalos e limites. Avisos de DTS da saída PNG não descartaram frames; contagem igual à do ffprobe. Esta gravação valida a resposta do mecanismo, não foot sliding de todo personagem nem FPS sustentado.

Capturas reais pós-implementação mostram gaveta única, alvos de toque, rolagem e hierarquia de informação; concept art não foi contada como aceite. Resultado visual conserva identidade angular e texto curto. Não foi criada aba de Atualizações no APK.

## Limites e continuidade

Limites de capacidade permanecem 32 parâmetros, 4 camadas, 24 estados/48 transições por camada, 8 movimentos/8 eventos por estado e 4 condições por transição. Fonte física lê o último passo concluído, podendo ter um quadro de atraso. Corpo comum não fornece apoio universal: Grounded exige Character/Motor e diagnostica ausência. Sem comparação de desempenho sustentado nesta revisão.

Roadmap e referências oficiais versionadas: ../../planos/ui-universal/ANIMATOR-UNIVERSAL-2026-10-07.md.
