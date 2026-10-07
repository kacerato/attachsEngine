# U07 — posse, movimento medido, animação e câmera

Bloco aceito em 07/10/2026 no repositório `https://github.com/kacerato/attachsEngine.git`,
branch `codex/gameplay-runtime`, sobre o merge `4081cad92833323c028cc79f9e48578e60684c37`.
Preserva câmera virtual/Cérebro e alterações de desempenho integradas pelo proprietário.
Fecha este contrato de correção; não declara toda a engine ou todo U08 concluído.

## Capacidade entregue

Personagem e Motor dinâmico arbitram UI, teclado/mouse, gamepad, Script e IA.
Fonte exclusiva mantém posse inclusive com intenção zero; Automático escolhe prioridade.
Inspector, arquivos legados, prefab/overrides, histórico, lifecycle, diagnóstico e SDK
permanecem conectados ao passo físico. A autoria/prioridades anterior está preservada em
[REPORT-POSSE-HISTORICAL.md](REPORT-POSSE-HISTORICAL.md); seus hashes/APK são históricos.

`astra.motor-motion` v1 entrega velocidade e apoio medidos pelos motores reais, com
identidade/lifecycle validados. `MotorAnimationDriver` recebe oito clipes autorais distintos
e comprimentos de passada. Repouso continua animando; Walk/Run compartilham fase e são
misturados pela velocidade relativa ao apoio. A fase avança pela distância percorrida,
incluindo aceleração/frenagem. Saltar, subir, ápice, cair e aterrissar usam contato físico
e velocidade vertical. Alterar cadência/fase não destrói o CrossFade.

O laboratório usa o humanoide de teste Godot TPS: 145 juntas, oito clipes reais,
fontes/derivação/atribuição preservadas. Retirou-se o track dedicado de root motion da
derivação, pois o Body desloca por forças. Três ossos perto do polo Euler exigiram
decomposição ortogonal em double, mantendo a checagem estrita de reconstrução que recusa
cisalhamento/reflexão incompatíveis. Fox permanece como segundo rig. A colisão continua
autorada pela geometria do objeto e pertence ao mesmo Body.

Controle aéreo .85 atua no solver; o facing Y acompanha intenção relativa à câmera.
Não teleporta a malha nem troca a colisão por cilindro. Câmera virtual orbital/Cérebro
recebe gesto escopado do Canvas sem um segundo escritor de pose. Evitar obstáculos usa
a varredura de esfera física existente. Raio .25 m, mínimo .1 m, near plane .05 m e
amortecimento .2 s são autoráveis/persistidos. Play aguarda catálogo de scripts Current,
inclusive no primeiro frame/autostart: Empty não é assembly publicado.

## APK e projetos

O APK contém um pacote de exemplo: U07Laboratorio, 21 arquivos com bytes iguais aos
fontes. `.astra` é transportado como metadata e restaurado na instalação transacional.
Os dez defaults são aposentados somente com nome e assinatura histórica de descriptor/
layout coincidentes; projetos autorados ou cópias renomeadas são preservados.

O principal `dev.aether.editor` foi atualizado para 0.2.2-preview.20261007, código 7,
com a chave de distribuição existente. Sem desinstalação/limpeza de dados. A lista real
ficou CameraVirtual-20261007, U07Laboratorio e teste; os arquivos dos dois projetos do
usuário têm hashes idênticos antes/depois. Chaves/senhas não fazem parte da entrega.

| Gate | Resultado atual | Evidência |
|---|---|---|
| Contratos gerados | 56 tipos/10 famílias, 423 declarações/78 tabelas, 846 acessores; API C# verificada | contracts-current.json |
| Nativo direcionado | 12/12: motores/Jolt, lifecycle/posse, apoio/velocidade/ar, piso em 72 ângulos, polos/importação, barreira de publicação | host-motion-scenarios.log |
| Regressão integrada | 121/121 no reteste final | gui-motion-regression-retry.log |
| Managed | 1/1 com ProjectCompiler real e Behavior do laboratório | sdk-motion-compile-final.log |
| ProjectStore | 7/7: migração/reabertura, preservação e gravação atômica | project-store-tests.xml |
| Android | Release construída; principal e validação instalados, hashes iguais aos APKs | android-startup-build.log; android-public-build.log; install-motion.log; install-public-motion.log; installed-*-hash.log |
| Pacote | Um projeto/21 arquivos exatos; lib nativa igual nas duas aplicações | apk-motion-manifest.json; apk-public-motion-manifest.json |
| Movimento POCO F7/Android 16 | 300/300 quadros, 38/38 páginas: acelerar, salto em movimento, redirecionar no ar, soltar, Land/Idle, salto parado | locomotion-accepted.mp4; locomotion-accepted-review.json; locomotion-accepted-contacts/ |
| Piso/câmera no aparelho | 227/227 quadros, 29/29 páginas: retração contra piso e órbita contraída | camera-collision-accepted.mp4; camera-collision-accepted-review.json; camera-collision-accepted-contacts/ |
| Save/reabertura no principal | Stop/Save, encerramento/abertura fria, scripts publicados, Play/Idle; serializer nativo lê 1 motor/6 SkinnedMesh/2 Animation/1 Ambiente | public-saved-edit.png; public-reopened-play.png; android-motion-saved.aescene; archive-motion-saved-inspection.log; public-play-reopen.log |
| Preservação | Hashes antes/depois iguais; dez defaults removidos, um pacote embutido | public-project-list.png; public-install-acceptance.json; user-projects-before.log; user-projects-after.log |

São **527 quadros e 67 páginas**, todos examinados sem amostragem. Manifestos registram
índice/tempo/SHA de cada quadro, hash do vídeo/APK associado e faixa/observação/hash de
cada página efetivamente revista. Decode individual fica no build; páginas preservam
todos os quadros. `verify-evidence.py` confere hashes/contratos/gates sem repetir suítes.

APK validação: `1ab86324c41ed7cb1a632f4bfa52ebba5d2d961b91eab317754cd4ba063236a0`.
APK principal: `61f7d8d85581dc090ad906e865b2e96c4ff974bf14cab4b5202806c0a2075936`.
Lib nativa comum: `ca59be419bd18110bd478e3f00c5eefbbb1233168dccf2bd728d51f32b8a7175`.
Principal: 102.909.341 bytes; artefato local instalado, não uma nova release GitHub/
Download publicada automaticamente.

## Tentativas rejeitadas e limites

Um vídeo expôs Play anterior à publicação; outro recebeu gestos antes de READY. Foram
rejeitados, não usados como aceite completo. A barreira foi corrigida e o helper espera
READY desta abertura antes de enviar touch real com dois ponteiros. Hashes/razões/
revisão parcial estão em rejected-motion-attempts.json. Histórico de inversão de órbita preservado.

A primeira regressão deu 120/121: publicação de dependências de prefab recusada e
transação revertida. Caso isolado passou 1/1, reteste integrado 121/121. Causa ambiental
da recusa transitória não estabelecida; o log falho permanece.

No extremo de pitch desejado contra o piso, a câmera contrai e **recorta o personagem**.
Não houve visão do verso do chão; enquadramento inteiro, fade de oclusores e composição
automática não são anunciados. Proteção requer colliders/filtros válidos; não cobre toda
malha sem colisão/sensor/spawn penetrado/configuração inválida.

Não é IK de pés, retargeting, root motion em runtime, AnimationTree visual universal ou
animação procedural. Cadência calibrada reduz deslizamento; não garante pés presos em
terreno irregular. Character entrega contato/velocidade de suporte, mas ID zero quando
o backend não o fornece. Gamepad validado no host, sem gamepad físico no Android.
FPS sustentado/thermal não medidos neste bloco.

U03/U06, restante U08, UI componível/U10, modelagem visual completa e SDK/rede/custo amplo
continuam com critérios próprios. Roadmap original intacto. Referências versionadas e
contrato no [plano](../../planos/ui-universal/U07-MOVIMENTO-ANIMACAO-CAMERA-2026-10-07.md).
