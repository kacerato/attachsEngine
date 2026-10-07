# U07 — fontes, posse de controle e terceira pessoa

**Registro histórico anterior à correção de movimento/animação/câmera.** Fontes,
contagens, rig e APK abaixo pertencem àquela revisão. O estado atual está em
[REPORT.md](REPORT.md); este texto não comprova o hash atual de código.

**Bloco U07 fechado em 07/10/2026.** Repositório confirmado:
`https://github.com/kacerato/attachsEngine.git`, branch `codex/gameplay-runtime`,
base física `09bd7349`, merge atual `4081cad9` preservado. Não declara a engine
inteira concluída. Os requisitos e o contrato estão no
[plano U07](../../planos/ui-universal/U07-POSSE-CONTROLE-2026-10-07.md).

## O que funciona

Personagem e Motor dinâmico recebem UI, teclado/mouse, gamepad, Script e IA por
origem. Automático escolhe prioridade; fonte fixa é exclusiva inclusive com zero.
Origem vencedora, candidatos, foco e intenção vêm do passo consumido. Trocar
política invalida entrada sem reconstruir o corpo ou descartar momentum. Foco,
pausa, cancelamento, desconexão, retarget, desativação, remoção e substituição
limpam os canais correspondentes. Saltos perdedores não reaparecem depois.

Fonte/prioridades são autoráveis nos dois motores, consumidas pelo runtime e
persistidas em cena/prefab/overrides/histórico. Character v5, DynamicBodyMotor v3,
CameraFollow v2 leem os formatos anteriores com padrões explícitos. A leitura de
PhysicsBody v4–7 aceita os dois layouts históricos bounded, sem mudar o escritor
v8. A fixture antiga de collider usa payload histórico real, não corta o escritor
atual. SDK `astra.motor-control` v1 é extensão opcional; acesso valida mundo,
identidade, geração, atividade e argumentos antes de enviar ou observar.

CameraFollow possui somente posição; CameraLook mantém orientação. Orbitar alvo
gira o deslocamento por orientação da câmera, com pivô autorável. O laboratório
usa movimento UI relativo à câmera e controla a velocidade angular Y do Body
no FixedUpdate; não altera seu transform à força ou separa malha da colisão.

O laboratório [U07Laboratorio](../../../examples/ui/U07Laboratorio/README.md)
possui humano CesiumMan (19 juntas), Fox, clipes reais importados, colisão medida
na malha, Body de 17 kg, piso PBR Poly Haven e atmosfera física. Sources, arquivos
de cena, Canvas, texturas e Behavior permanecem editáveis. É uma amostra da cadeia
existente de skin/animação; U08 universal continua separado.

## Gates e critérios

| Gate | Resultado | Evidência |
|---|---|---|
| Contratos e SDK gerado | 56 tipos/10 famílias, 423 declarações/78 tabelas, 846 acessores atuais; API C# atual | contracts-current.json/log |
| Host integrado ao merge atual | Build nativo concluído | host-build-current-merge.log |
| Cenários direcionados | 8/8; dois atuadores reais/Jolt, prioridades/momentum, arquivos legados/prefab/histórico, rebind/desconexão/foco, retarget/remover/pausa, SDK identidade/estado, órbita/facing físico | host-orbit-poles.log; test_motor_control.cpp; regressão atual inclui estes cenários |
| Regressão do editor | 117/117 | gui-regression-current-merge.log |
| Managed | U07Example_CompilesWithRealProjectCompiler passou; novo Behavior compilado pelo compilador de projeto real | sdk-compile-current-merge.log |
| Android | Release: BUILD SUCCESSFUL, instalação Success, SHA do APK igual ao base.apk instalado | android-build-orbit-poles.log; install-orbit-poles.log; apk-orbit-poles-hash.log; installed-orbit-poles-hash.log |
| Autoria Android | Fonte UI salva; Undo restaura Automático; Redo restaura UI; encerramento do processo/reabertura mantém UI; estado final salvo Automático | author-ui-saved.png; author-ui-cold-reopen.png; play-ui-cold-reopen.png; quatro android-*.aescene; archive-inspection.log |
| Play Android | UI exclusiva, Automático/IA5, Script20, IA50, release Script→IA5, pausa/retomada, soltar joystick→IA; diagnóstico medido do Jolt | runtime-*.png; runtime-solver-diagnostic.png |
| Câmera e corpo Android | 200/200 quadros, 25/25 páginas examinados: órbita cruza 90° sem inverter; Body vira até aproximadamente -108° e desloca na direção da câmera, depois repousa | camera-accepted.mp4; accepted-camera-review.json; accepted-camera-contacts/ |
| Propriedades reais da câmera | Alvo PlayerDynamic, offset 0/0/-5, órbita ligada/pivô1,1 m; arquivo salvo e terceira pessoa após reabertura | author-camera-orbit-properties.png; author-camera-orbit-values.png |

Os oito critérios do plano têm aceites: política nos dois motores; origens reais;
API tipada; alteração ao vivo sem rebuild; cancelamento/lifecycle; persistência e
histórico; diagnóstico; cenário host/aparelho. Hardware gamepad físico **não**
conectado ao Android: sua origem, vínculos, desconexão e coexistência foram
conferidos no host. Arquivos retirados do Android foram interpretados pelo
serializer nativo no host; não atribuímos essa inspeção à execução no aparelho.

## Aparelho e integridade

POCO F7 / 25053PC47G, Android 16, landscape. Pacote separado
`dev.aether.editor.u07`, versão `0.2.0-u07-validation`, código 6.
SHA-256 local e instalado:
`d689f2095633082223b3a5341ab38c49d8c824c78b180f465654bbd39a368e0d`.
A aplicação principal e seus projetos não foram substituídos. Os gestos foram
guardados pela confirmação de foreground do pacote de validação; nenhum toque
foi enviado quando outro aplicativo estava ativo.

Estado final: Play parado, laboratório salvo em Automático com prioridades
10/10/10/20/5 e câmera configurada. [Tela final](editor-ready-final.png).
`acceptance.json` vincula hashes de código, amostra e evidências; o script
`verify-evidence.py` confere esses vínculos sem repetir a suíte.

## Problemas encontrados e resolvidos

O primeiro vídeo **foi rejeitado**. Todos os seus 353 quadros/45 páginas foram
examinados; no quadro 58 (1,020 s) o TRS completo canonicalizou yaw ao cruzar 90°,
inverteu pitch/roll e CameraLook limitou a câmera para cima. A correção publica
somente posição local e preserva os ângulos. O teste percorre 120 passos de 3°
sem mudança de pitch/roll/raio. A nova gravação foi inteiramente examinada e aceita.
[Registro rejeitado](rejected-camera-review.json),
[contato do quadro 58](rejected-camera-frame58-contact.jpg).

Outra execução de regressão usou binário antigo com atlas recém-regenerado de
277 ícones e falhou em 20 verificações. O build foi refeito após o merge 4081cad9;
os 117 cenários passaram. Não foi enfraquecida a verificação do atlas. O log
`gui-regression-orbit-poles.log` registra a tentativa rejeitada; o gate vigente é
`gui-regression-current-merge.log`.

## Limites e continuidade

Não medimos FPS/thermal sustentado e a cadência do vídeo não é benchmark. O clipe
único do humano pode parar numa pose de caminhada; esta amostra não possui grafo
universal idle/walk/run, retargeting ou root motion. A colisão é medida na pose
autoral, não em cada osso por quadro. CameraFollow sozinho não varre paredes;
Braço de mola e câmera virtual/Cérebro existem separadamente e não são usados
nesta cena aberta. IA é origem de intenção, não navegação/decisão ou multiplayer.

U03/U06/U08, UI componível, modelagem visual completa, SDK/rede e custo amplo
continuam nos blocos próprios. O roadmap UI original permanece intacto.

O [guia público](https://astraengine.com.br/pt-br/snapshot-2026-10-07/sistemas/posse-de-controle/)
possui snapshot próprio. A prévia pública 0.2.1 de 07/10/2026, baseada em 09bd7349,
permanece distinta do pacote U07; esta entrega não publica outro APK.

Referências: Godot 4.5 [Input](https://docs.godotengine.org/en/4.5/classes/class_input.html),
[InputMap](https://docs.godotengine.org/en/4.5/classes/class_inputmap.html),
[código de Input](https://github.com/godotengine/godot/blob/4.5-stable/core/input/input.cpp)
e [terceira pessoa](https://docs.godotengine.org/en/4.5/tutorials/3d/spring_arm.html);
Unity Input System 1.4.3 [PlayerInput](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.4/api/UnityEngine.InputSystem.PlayerInput.html).
Princípios extraídos: ações e origens explícitas, liberação por lifecycle,
destinatário independente da aparência, órbita independente de facing.
