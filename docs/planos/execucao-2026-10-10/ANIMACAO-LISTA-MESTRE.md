# Animação — lista mestre (UMotion + FinalIK → Astra)

Data: 11/10/2026. Fonte: manual UMotion Pro 1.29 (75 páginas, `analise-plugins/manual-texto`)
e FinalIK 2.2 (167 scripts de runtime). Cada linha é uma capacidade **no jeito Astra**:
genérica (humanoide, quadrúpede, mecanismo), operável pela interface da engine,
persistida, com Undo, consumida em Play e coberta por teste. Um bloco só fecha quando
todas as suas linhas estão `feito` com validação no host **e** no aparelho.

Legenda: `feito` · `parcial` (o que falta está descrito) · `pacote X` (planejado).

## N01 — Autoria de clipe (UMotion: Clip Editor, Pose Mode)
| Capacidade | Origem | Estado |
|---|---|---|
| Dopesheet, curvas, tangentes, seleção em caixa, copiar/colar chaves | Dopesheet, Curves, DopesheetCurves | feito |
| Camadas aditiva/override, peso, mute/solo, referência | Layers | feito |
| Eventos e marcadores (cues), regiões, intervalos | Animation, ClipSettings | feito |
| Modo pose: mover/girar/escala, eixos, espaço local/mundo, travas de canal | PoseMode, Tools, Channels | feito |
| Seleção: pai/filhos/irmãos, conjuntos salvos, isolar | Selection | feito |
| Modos de rotação Quaternion/Euler, ramo de Euler | RotationModes | feito |
| Auto-key, gravar/cancelar pose, política de gravação | PoseEditor, Options | feito |
| Estúdio em painéis (árvore, viewport, timeline, inspector) | — (Astra) | feito |
| Fantasma do quadro anterior/seguinte | — | parcial: passo fixo 0,1 s → pacote B usa chaves vizinhas |
| Exibição de ossos configurável (tamanho, cor por lado, só selecionados) | ConfigDisplay, PoseDisplay | pacote B |
| Editar em Play (gravar pose durante o Play) | EditInPlayMode | pacote B |

## N02 — Propriedades e drivers
| Propriedades tipadas animáveis, curvas, remapeamento | AnimatedPropertiesList, CustomProperty | feito |
| Drivers (DAG) | — | feito |

## N03 — Rig, espelho, poses, transferência
| Rig genérico com limites dobradiça/cone | RigHierarchy | feito |
| Espelho: pares por nome (com sufixo), membro para o outro lado | MirrorMapping | feito |
| Biblioteca de poses aplicada como chave | PoseEditor | feito; miniatura → pacote B |
| Transferência entre rigs de repouso diferente | — | feito: rotações por nome; mapa manual + raiz → pacote B |

## N04 — Constraints e IK (UMotion: IK, Child-Of, Pinning · FinalIK)
| Capacidade | Origem | Estado |
|---|---|---|
| IK de dois ossos (limb/trigonometric), direção da dobra | LimbIK, TrigonometricIK, BendGoal | feito |
| IK de cadeia CCD / FABRIK com iterações e tolerância | CCDIK, FABRIK | feito |
| Apoio do pé por pé (raio físico, rampa, fase aérea) | Grounding, GroundingLeg | feito |
| Constraints de posição/rotação/escala/mira/pai/look-at (cena) | Constraint* | feito (pré-existente) |
| IK no estúdio (arrastar alvo, dobra, limites do rig) | InverseKinematics | feito |
| **Look At IK** (coluna + cabeça + olhos com pesos e limite) | LookAtIK, IKSolverLookAt | pacote A |
| **Aim IK** em cadeia (arma/braço aponta com pesos por osso) | AimIK, IKSolverAim | pacote A |
| **Limites de rotação no runtime** (dobradiça, ângulo, polígono) aplicados pelo IK | RotationLimit* | pacote A |
| **Grounder do corpo**: vários pés + compensação da pelve (bípede e quadrúpede) | Grounder, GrounderQuadruped, GroundingPelvis | pacote A |
| **Dedos**: curvatura por dedo com peso e alvo | FingerRig | pacote A |
| **Offset de efetor** e **relaxar torção** | EffectorOffset, OffsetEffector, TwistRelaxer | pacote A |
| Ordem de execução e prioridade entre IKs | IKExecutionOrder, SolverManager | pacote A |
| **Assistente de IK**: detecta membros pela hierarquia e cria IK/alvos | IKSetupWizard | pacote B |
| **FK ↔ IK**: blend, converter IK em chaves FK num intervalo | FKtoIKConversion | pacote B |
| **Child-Of animável** (troca de pai mantendo a pose, com chave) | ChildOf | pacote B |
| **Pinning** (fixar efetor no mundo enquanto edita o resto) | Constraints | pacote B |
| FABRIK Root (árvore de várias cadeias) e Full Body Biped | FABRIKRoot, FullBodyBipedIK | pacote C |
| Interação (objeto, alvo, gatilho, mãos em prop) | InteractionSystem, InteractionObject | pacote C |
| Reação a impacto, recuo, inércia, amplificador, inclinação | HitReaction, Recoil, Inertia, Amplifier, BodyTilt | pacote C |
| Poser de mão / pose genérica | HandPoser, GenericPoser | pacote C |
| Evitar penetração | PenetrationAvoidance | pacote C |
| VRIK | VRIK | condicional: exige tracking XR real (N14) |

## N05 — Locomoção, root motion (UMotion RootMotion · FinalIK Grounder)
Pacote D: root motion do clipe (extrair/aplicar, loop, eixo), deslocamento por física,
grounder integrado à locomoção, plataformas móveis, quadrúpede.

## N06 — Bake e exportação (UMotion ImportExport · FinalIK Baker)
Pacote E: bake de IK/constraints/física para clipe, exportação FBX/glTF com relatório
de perdas, mesmos solvers do Play.

## Templates para tutoriais (após N06)
Cenas prontas no projeto de exemplo: personagem humanoide com rig e IK, quadrúpede
(Fox) com grounder, braço mecânico com IK de cadeia, cena de transferência entre dois
personagens, cena de interação (pegar objeto). Cada template é criado pela engine e
documentado com passo a passo.
