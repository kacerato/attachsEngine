# Alinhamento Final IK 2.2, UMotion Pro 1.29 e Animator → Astra (13/10/2026)

Fonte: `C:\Users\donod\Downloads\analise-plugins\analise-plugins` (Final IK 2.2: 167 scripts
de runtime e 75 cenas de demo; UMotion Pro 1.29p04: 75 páginas do manual, Robot Kyle,
4 `.anim`, 4 controllers). Confrontado com o código da Astra nesta data, não com a lista
de 11/10 (`ANIMACAO-LISTA-MESTRE.md`, que ficou desatualizada em vários itens).

Regra: capacidade universal (humanoide, quadrúpede, mecanismo), operável pela interface,
persistida, com Undo, consumida no Play e testada. Nome de componente Unity não entra
no menu sem runtime. Atalhos que montam uma única situação **não** contam como
capacidade (pedido do proprietário em 13/10): o modelo de cena é só exemplo, e a
capacidade é o componente.

Legenda: **feito** · **parcial** (falta descrita) · **falta** · **condicional** (depende de hardware/serviço externo).

## 1. Final IK — componentes de runtime

| Final IK | Astra | Estado |
|---|---|---|
| LimbIK, TrigonometricIK, ArmIK, LegIK, BendGoal | `astra.ik.two_bone` (dobra, polo, rotação do alvo) | feito |
| CCDIK, FABRIK | `astra.ik.chain` (CCD/FABRIK, iterações, tolerância) | feito |
| FABRIKRoot | `astra.ik.fabrik_root` | feito |
| AimIK, AimPoser, AimController | `astra.ik.aim` | parcial: falta poser por direção (AimPoser) e troca suave de alvo (AimController) |
| LookAtIK, LookAtController | `astra.ik.look_at`, `astra.constraint.look_at` | parcial: falta troca suave de alvo com velocidade/limite (LookAtController) |
| FullBodyBipedIK (efetores, pull, pin, bend, mapping, head effector) | `astra.ik.full_body` | parcial: pelve puxada + peito + 4 membros; faltam pesos de pull/pin por efetor, efetor de cabeça, restrições de dobra por membro |
| BipedIK | two_bone + look_at + aim + grounder | parcial: não há montagem única; os componentes cobrem as peças |
| VRIK (+ calibrador, LOD, locomoção procedural) | `astra.ik.body_by_targets` | condicional para XR; locomoção procedural de passos (IKSolverVRLocomotion) falta |
| Grounder, GrounderIK, GrounderFBBIK, GrounderQuadruped, GroundingLeg/Pelvis | `astra.ik.grounder`, `astra.ik.foot_grounding` (trava de pé) | feito; GrounderVRIK condicional |
| RotationLimitAngle, RotationLimitHinge | `astra.ik.rotation_limit` (cone e torção, dobradiça) | feito |
| RotationLimitPolygonal, RotationLimitSpline | — | **falta** |
| FingerRig, Finger | `astra.ik.finger_curl` | parcial: curvatura por dedo; falta alvo por dedo (FingerRig com target) |
| HandPoser, GenericPoser | `astra.ik.hand_poser` | parcial: GenericPoser (qualquer hierarquia) usa o mesmo caminho, sem cartão próprio |
| InteractionSystem, InteractionObject | `astra.ik.interaction_system`, `astra.ik.interaction_object` | parcial: duas mãos, ponto de carregar, agachar/inclinar, eventos (13/10). Faltam: **rotação da mão pela pegada** (InteractionTarget), pausa/retomada por efetor, efetores de ombro/coxa/pé, curvas de peso por interação |
| InteractionTarget | — | **falta** (orientação e torção da mão na pegada) |
| InteractionTrigger | — | **falta** (iniciar por proximidade/ângulo, sem script) |
| InteractionLookAt | — | **falta** (olhar o objeto durante a interação) |
| Amplifier, BodyTilt, Inertia, HitReaction, Recoil, PenetrationAvoidance | `astra.ik.amplifier/body_tilt/inertia/hit_reaction/recoil/penetration_avoidance` | feito |
| OffsetModifier, OffsetPose, EffectorOffset | `astra.ik.offset` | parcial: falta pose de offset gravada (OffsetPose) |
| TwistRelaxer, TwistSolver | `astra.ik.twist_relaxer` | feito |
| ShoulderRotator | — | **falta** |
| CCDBendGoal | — | **falta** |
| Constraint Position/Rotation/Offset | `astra.constraint.*` (posição, rotação, escala, mira, pai, olhar) | feito |
| RagdollUtility (ragdoll ↔ animação com blend) | — | **falta** (N12) |
| IKExecutionOrder / SolverManager | ordem documentada + Prioridade por componente | feito |
| Baker (Humanoid/Generic) | Bake da pose final (N06) | feito; humanoide de músculos não |
| Encaixe (prop em osso) | `astra.ik.socket` | feito (Astra) |

## 2. Final IK — cenas de demo como modelos/tutoriais

Cada demo vira um cenário que usa só componentes universais. Feitos (modelos de cena):
Manequim com IK e apoio, Quadrúpede com grounder, Braço mecânico (FABRIK), Interação
(uma mão), Carregar com as duas mãos (Pick Up 2-Handed/Carry Box), Personagem jogável.
Faltam cenários para: Aim Weapon/Second Hand On Gun/Two Handed Prop, Holding Hands,
Interaction Trigger, Interaction Walls, Driving, Boxing/Hit Reaction/Recoil (há componentes,
falta cena), Soccer Kick, Mech Spider, Turret, Penetration Avoidance, Finger Rig, Rotation
Limits, Shoulder Rotator, Ragdoll Utility, Motion Absorb, Kissing Rig, Effector Offset.
Com Kyle e Viking reais: a cena **Kyle – Carregar caixa** (projeto próprio, só o Kyle).

## 3. UMotion Pro — editor de animação

| UMotion | Astra | Estado |
|---|---|---|
| Clip Editor: dopesheet, curvas, tangentes, seleção em caixa, copiar/colar | Estúdio de animação (Chave/Curvas) | feito |
| Layers (aditiva/override, peso, mute/solo) | Camadas do clipe | feito |
| Eventos, marcadores, intervalos, regiões | cues/regiões | feito |
| Pose Mode, ferramentas, eixos, espaço local/mundo, travas | modo pose | feito |
| Seleção (pai/filhos/irmãos, conjuntos, isolar) | feito | feito |
| Rotation Modes (Quaternion/Euler) | feito | feito |
| Auto-key, gravar/cancelar pose | feito | feito |
| Animated Properties, Custom Property | N02 (catálogo tipado, drivers DAG) | parcial: aceite Android pendente |
| Rig Hierarchy, limites | N03 | feito |
| Mirror Mapping | N03 | feito |
| Pose Editor (biblioteca de poses) | N03 | feito; miniatura falta |
| IK Setup Wizard | Assistente de IK do estúdio | feito |
| Inverse Kinematics no estúdio, FK↔IK, Pinning | aba IK, Fixar ponta, Seguir | parcial: blend FK↔IK animável (peso como canal) falta |
| Child-Of animável | Seguir objeto | parcial: troca de pai com chave e compensação falta |
| Root Motion | N05 | feito |
| Import/Export (FBX/glTF) | Exportar GLB com reimportação (N06) | parcial: FBX não |
| Config Display / Pose Display (ossos configuráveis) | — | **falta** |
| Edit In Play Mode | — | **falta** |
| Unity Timeline Integration | N09 Sequências | **falta** |
| UMotion API | SDK ABI 12 (N08) | parcial |
| Clips **Humanoid** (curvas de músculo: PickUp, Idle, WalkRM, IKPullups) | importador aceita só Transform/blend shape | **falta**: conversão de músculos para rotação de ossos |
| Projetos `.asset` do UMotion | — | falta (formato proprietário; avaliar só se houver especificação) |

## 4. Animator (controle de estados)

| Unity Animator | Astra (`astra.animation.animator`, AEANIMATOR 3) | Estado |
|---|---|---|
| Parâmetros Float/Int/Bool/Trigger | feito, mais fontes automáticas (velocidade plana/vertical, no chão) | feito |
| Estados, transições, condições, interrupção, sub-máquinas, eventos de estado | feito | feito |
| Blend Tree 1D e 2D | feito | parcial: 2D Freeform Directional/Cartesian e Direct faltam |
| Camadas Override/Aditiva com máscara | feito | feito; Sync layers falta |
| Override Controller | feito | feito |
| Importar `.controller` Unity | conversor N07 | parcial: variantes não equivalentes recusadas |
| IK pass por camada (OnAnimatorIK) | IK roda depois do Animator para todos | parcial: liga/desliga por camada falta |
| Humanoid (Avatar, músculos, retarget por músculo) | transferência por nome entre rigs (N03) | **falta** |
| State Machine Behaviours | scripts C# por evento de estado | parcial |
| Mirror, Cycle Offset, Speed por parâmetro por estado | — | **falta** |
| Root motion pelo Animator | N05 | feito |

## 5. Assets e materiais dos pacotes

| Pacote | Estado |
|---|---|
| Robot Kyle (FBX, TGA, material) | importado (GLB por receita) e usado em cena; **clipes Humanoid não** |
| Viking + Idle/Walk/Run (Final IK) | importados, clipes extraídos editáveis |
| Demais FBX Final IK/UMotion/BoZo | 151 decodificados, 150 aceitos (1 dual quaternion recusado) |
| POLYGON Icons (520 FBX/prefabs, 17 materiais, 16 paletas) | 520 FBX decodificados; galeria/catálogo na engine pendente |
| BoZo Modular Characters (corpo, roupas, acessórios, shapes, cores, criador) | **falta** o sistema modular nativo (N05/N07/N10/N11) |
| Materiais Standard/URP | conversão Standard metálico; shadergraphs com diagnóstico |
| Cenas Unity, prefabs variantes | lacuna declarada |

## 6. Ordem proposta para fechar animação/IK (antes do APK público)

1. **Clips Humanoid** (músculos → ossos) para PickUp/Idle/WalkRM/IKPullups no Kyle.
2. **InteractionTarget** (rotação da mão), **InteractionTrigger**, **InteractionLookAt**, pausa/retomada.
3. RotationLimit Polygonal/Spline, ShoulderRotator, CCDBendGoal, AimController/LookAtController, AimPoser, OffsetPose, FingerRig com alvo.
4. FBBIK completo (pull/pin por efetor, cabeça), Ragdoll Utility (com N12).
5. Animator: Blend 2D Freeform/Direct, Sync layers, Mirror/Cycle Offset/Speed por estado, IK por camada.
6. UMotion: exibição de ossos, editar em Play, Child-Of animável, blend FK↔IK.
7. Cenários das demos restantes (seção 2) com Kyle/Viking, cada um com captura no aparelho.
