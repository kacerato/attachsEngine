# Matriz de propriedades dos componentes

**Gerado por `scene::componentMatrixMarkdown()`** a partir dos descritores de componente.
Não edite à mão: acrescente a propriedade no descritor e regenere.
Uma linha só existe aqui quando tem identidade persistente, consumidor declarado e
capacidade do motor disponível — as três condições que `auditComponentContracts()` exige.

**Registro atual:** 51 schemas; 49 tipos no Add; 49 fachadas geradas.
Esses números descrevem o registro do checkout, não certificam paridade ou aceite no aparelho.

| Tipo | Família | API C# | Criação |
|---|---|---|---|
| `astra.spring.position` | Lógica | `Astra.Components.SpringPositionConstraint` | Add Component |
| `astra.spring.rotation` | Lógica | `Astra.Components.SpringRotationConstraint` | Add Component |
| `astra.spring.scale` | Lógica | `Astra.Components.SpringScaleConstraint` | Add Component |
| `astra.constraint.position` | Lógica | `Astra.Components.PositionConstraint` | Add Component |
| `astra.constraint.rotation` | Lógica | `Astra.Components.RotationConstraint` | Add Component |
| `astra.constraint.scale` | Lógica | `Astra.Components.ScaleConstraint` | Add Component |
| `astra.constraint.aim` | Lógica | `Astra.Components.AimConstraint` | Add Component |
| `astra.constraint.parent` | Lógica | `Astra.Components.ParentConstraint` | Add Component |
| `astra.constraint.look_at` | Lógica | `Astra.Components.LookAtConstraint` | Add Component |
| `astra.tween.transform` | Lógica | `Astra.Components.TransformTween` | Add Component |
| `astra.tween.property` | Lógica | `Astra.Components.PropertyTween` | Add Component |
| `astra.tween.sequence` | Lógica | `Astra.Components.TweenSequence` | Add Component |
| `astra.time.timer` | Lógica | `Astra.Components.GameTimer` | Add Component |
| `astra.logic.event_connection` | Lógica | `Astra.Components.EventConnection` | Add Component |
| `astra.script.behavior` | Lógica | API própria | Fluxo próprio |
| `astra.ui.canvas` | Renderização | `Astra.Components.UiCanvas` | Add Component |
| `astra.render.mesh` | Renderização | `Astra.Components.MeshRenderer` | Add Component |
| `astra.render.skinned_mesh` | Renderização | `Astra.Components.SkinnedMesh` | Add Component |
| `astra.render.lod_group` | Renderização | `Astra.Components.LodGroup` | Add Component |
| `astra.render.environment` | Renderização | `Astra.Components.EnvironmentVolume` | Add Component |
| `astra.render.light` | Luz | `Astra.Components.Light` | Add Component |
| `astra.camera` | Câmera | `Astra.Components.Camera` | Add Component |
| `astra.camera.look` | Câmera | `Astra.Components.CameraLook` | Add Component |
| `astra.camera.follow` | Câmera | `Astra.Components.CameraFollow` | Add Component |
| `astra.physics.collision_recipe` | Física 3D | API própria | Fluxo próprio |
| `astra.physics.field.gravity` | Física 3D | `Astra.Components.GravityField` | Add Component |
| `astra.physics.field.wind` | Física 3D | `Astra.Components.WindField` | Add Component |
| `astra.physics.field.drag` | Física 3D | `Astra.Components.DragField` | Add Component |
| `astra.physics.field.radial` | Física 3D | `Astra.Components.RadialField` | Add Component |
| `astra.physics.event_connection` | Física 3D | `Astra.Components.PhysicsEventConnection3D` | Add Component |
| `astra.physics.constant_force` | Física 3D | `Astra.Components.ConstantForce` | Add Component |
| `astra.physics.body` | Física 3D | `Astra.Components.PhysicsBody` | Add Component |
| `astra.physics.character` | Física 3D | `Astra.Components.Character` | Add Component |
| `astra.physics.collider` | Física 3D | `Astra.Components.Collider` | Add Component |
| `astra.physics.joint` | Física 3D | `Astra.Components.Joint` | Add Component |
| `astra.physics.dynamic_motor` | Física 3D | `Astra.Components.DynamicBodyMotor` | Add Component |
| `astra.animation` | Animação | `Astra.Components.Animation` | Add Component |
| `astra.physics2d.field.gravity` | Física 2D | `Astra.Components.GravityField2D` | Add Component |
| `astra.physics2d.field.wind` | Física 2D | `Astra.Components.WindField2D` | Add Component |
| `astra.physics2d.field.drag` | Física 2D | `Astra.Components.DragField2D` | Add Component |
| `astra.physics2d.field.radial` | Física 2D | `Astra.Components.RadialField2D` | Add Component |
| `astra.physics2d.event_connection` | Física 2D | `Astra.Components.PhysicsEventConnection2D` | Add Component |
| `astra.physics2d.constant-force` | Física 2D | `Astra.Components.ConstantForce2D` | Add Component |
| `astra.physics2d.joint` | Física 2D | `Astra.Components.Joint2D` | Add Component |
| `astra.physics2d.body` | Física 2D | `Astra.Components.Body2D` | Add Component |
| `astra.physics2d.collider` | Física 2D | `Astra.Components.Collider2D` | Add Component |
| `astra.audio.source` | Áudio | `Astra.Components.AudioSource` | Add Component |
| `astra.audio.listener` | Áudio | `Astra.Components.AudioListener` | Add Component |
| `astra.audio.bus` | Áudio | `Astra.Components.AudioBus` | Add Component |
| `astra.path` | Lógica | `Astra.Components.PathComponent` | Add Component |
| `astra.path.follow` | Lógica | `Astra.Components.PathFollow` | Add Component |

## Mola de posição · `astra.spring.position` v1

Segue fonte com velocidade e amortecimento persistentes no runtime. **Consumidor:** runtime/scene_constraints.h. **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-PositionConstraint.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Incompatível | `astra.constraint.position` | Escolha restrição direta ou mola para posição |
| Incompatível | `astra.constraint.parent` | Parent Constraint já escreve posição |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `weight` | Influência | número | Fonte | 1 | 0 … 1 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `frequency` | Frequência | número | Resposta | 3 | 0.01 … 60 | Hz | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `damping_ratio` | Razão de amortecimento | número | Resposta | 1 | 0 … 10 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `max_speed` | Velocidade máxima | número | Resposta | 1000 | 0.001 … 100000 | m/s | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_x` | Offset X | número | Ajustes | 0 | -10000 … 10000 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_y` | Offset Y | número | Ajustes | 0 | -10000 … 10000 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_z` | Offset Z | número | Ajustes | 0 | -10000 … 10000 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `enabled` | Ativo | booleano | Fonte | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `target` | Fonte | referência | Fonte | Escolher fonte | qualquer objeto · fora da subárvore |  | runtime/scene_constraints.h | pose e bounds | não | não | não |

## Mola de rotação · `astra.spring.rotation` v1

Segue orientação por quaternion com amortecimento e caminho curto. **Consumidor:** runtime/scene_constraints.h. **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-RotationConstraint.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Incompatível | `astra.constraint.rotation` | Escolha restrição direta ou mola para rotação |
| Incompatível | `astra.constraint.aim` | Aim já escreve rotação |
| Incompatível | `astra.constraint.parent` | Parent Constraint já escreve rotação |
| Incompatível | `astra.constraint.look_at` | Look At já escreve rotação |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `weight` | Influência | número | Fonte | 1 | 0 … 1 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `frequency` | Frequência | número | Resposta | 3 | 0.01 … 60 | Hz | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `damping_ratio` | Razão de amortecimento | número | Resposta | 1 | 0 … 10 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `max_speed` | Velocidade máxima | número | Resposta | 1000 | 0.001 … 100000 | graus/s | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_x` | Offset X | número | Ajustes | 0 | -10000 … 10000 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_y` | Offset Y | número | Ajustes | 0 | -10000 … 10000 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_z` | Offset Z | número | Ajustes | 0 | -10000 … 10000 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `enabled` | Ativo | booleano | Fonte | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `target` | Fonte | referência | Fonte | Escolher fonte | qualquer objeto · fora da subárvore |  | runtime/scene_constraints.h | pose e bounds | não | não | não |

## Mola de escala · `astra.spring.scale` v1

Segue escala positiva sem publicar escala singular. **Consumidor:** runtime/scene_constraints.h. **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ScaleConstraint.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Incompatível | `astra.constraint.scale` | Escolha restrição direta ou mola para escala |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `weight` | Influência | número | Fonte | 1 | 0 … 1 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `frequency` | Frequência | número | Resposta | 3 | 0.01 … 60 | Hz | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `damping_ratio` | Razão de amortecimento | número | Resposta | 1 | 0 … 10 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `max_speed` | Velocidade máxima | número | Resposta | 1000 | 0.001 … 100000 | x/s | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_x` | Offset X | número | Ajustes | 1 | 0.0001 … 10000 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_y` | Offset Y | número | Ajustes | 1 | 0.0001 … 10000 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_z` | Offset Z | número | Ajustes | 1 | 0.0001 … 10000 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `enabled` | Ativo | booleano | Fonte | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `target` | Fonte | referência | Fonte | Escolher fonte | qualquer objeto · fora da subárvore |  | runtime/scene_constraints.h | pose e bounds | não | não | não |

## Position Constraint · `astra.constraint.position` v1

Restrição de mundo com uma fonte. **Consumidor:** runtime/scene_constraints.h. **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-PositionConstraint.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `weight` | Peso | número | Influência | 1 | 0 … 1 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_x` | Deslocamento X | número | Ajustes | 0 | -10000 … 10000 | u | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_y` | Deslocamento Y | número | Ajustes | 0 | -10000 … 10000 | u | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_z` | Deslocamento Z | número | Ajustes | 0 | -10000 … 10000 | u | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `enabled` | Ativo | booleano | Influência | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_x` | Aplicar X | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_y` | Aplicar Y | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_z` | Aplicar Z | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `target` | Fonte | referência | Influência | Escolher fonte | qualquer objeto · fora da subárvore |  | runtime/scene_constraints.h | pose e bounds | não | não | não |

## Rotation Constraint · `astra.constraint.rotation` v1

Restrição de mundo com uma fonte. **Consumidor:** runtime/scene_constraints.h. **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-RotationConstraint.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `weight` | Peso | número | Influência | 1 | 0 … 1 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_x` | Deslocamento X | número | Ajustes | 0 | -10000 … 10000 | graus | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_y` | Deslocamento Y | número | Ajustes | 0 | -10000 … 10000 | graus | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_z` | Deslocamento Z | número | Ajustes | 0 | -10000 … 10000 | graus | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `enabled` | Ativo | booleano | Influência | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_x` | Aplicar X | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_y` | Aplicar Y | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_z` | Aplicar Z | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `target` | Fonte | referência | Influência | Escolher fonte | qualquer objeto · fora da subárvore |  | runtime/scene_constraints.h | pose e bounds | não | não | não |

## Scale Constraint · `astra.constraint.scale` v1

Restrição de mundo com uma fonte. **Consumidor:** runtime/scene_constraints.h. **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ScaleConstraint.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `weight` | Peso | número | Influência | 1 | 0 … 1 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_x` | Fator X | número | Ajustes | 1 | 0 … 10000 | x | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_y` | Fator Y | número | Ajustes | 1 | 0 … 10000 | x | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_z` | Fator Z | número | Ajustes | 1 | 0 … 10000 | x | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `enabled` | Ativo | booleano | Influência | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_x` | Aplicar X | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_y` | Aplicar Y | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_z` | Aplicar Z | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `target` | Fonte | referência | Influência | Escolher fonte | qualquer objeto · fora da subárvore |  | runtime/scene_constraints.h | pose e bounds | não | não | não |

## Aim Constraint · `astra.constraint.aim` v1

Restrição de mundo com uma fonte. **Consumidor:** runtime/scene_constraints.h. **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-AimConstraint.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `weight` | Peso | número | Influência | 1 | 0 … 1 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_x` | Deslocamento X | número | Ajustes | 0 | -10000 … 10000 | graus | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_y` | Deslocamento Y | número | Ajustes | 0 | -10000 … 10000 | graus | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_z` | Deslocamento Z | número | Ajustes | 0 | -10000 … 10000 | graus | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `enabled` | Ativo | booleano | Influência | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_x` | Aplicar X | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_y` | Aplicar Y | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_z` | Aplicar Z | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `aim_axis` | Eixo de mira | enumeração | Ajustes | +Z | +X \| +Y \| +Z \| -X \| -Y \| -Z |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `up_axis` | Vertical de mundo | enumeração | Ajustes | Y | Y \| Z \| X |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `target` | Fonte | referência | Influência | Escolher fonte | qualquer objeto · fora da subárvore |  | runtime/scene_constraints.h | pose e bounds | não | não | não |

## Parent Constraint · `astra.constraint.parent` v1

Segue posição e rotação da fonte sem herdar escala. **Consumidor:** runtime/scene_constraints.h. **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ParentConstraint.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `weight` | Peso | número | Influência | 1 | 0 … 1 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_x` | Deslocamento X | número | Ajustes | 0 | -10000 … 10000 | u | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_y` | Deslocamento Y | número | Ajustes | 0 | -10000 … 10000 | u | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `offset_z` | Deslocamento Z | número | Ajustes | 0 | -10000 … 10000 | u | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `rotation_offset_x` | Rotação offset X | número | Rotação | 0 | -10000 … 10000 | graus | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `rotation_offset_y` | Rotação offset Y | número | Rotação | 0 | -10000 … 10000 | graus | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `rotation_offset_z` | Rotação offset Z | número | Rotação | 0 | -10000 … 10000 | graus | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `enabled` | Ativo | booleano | Influência | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_x` | Aplicar X | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_y` | Aplicar Y | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_z` | Aplicar Z | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `rotation_x` | Rotação X | booleano | Rotação | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `rotation_y` | Rotação Y | booleano | Rotação | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `rotation_z` | Rotação Z | booleano | Rotação | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `target` | Fonte | referência | Influência | Escolher fonte | qualquer objeto · fora da subárvore |  | runtime/scene_constraints.h | pose e bounds | não | não | não |

## Look At Constraint · `astra.constraint.look_at` v1

Orienta +Z para a fonte com vertical e roll. **Consumidor:** runtime/scene_constraints.h. **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-LookAtConstraint.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `weight` | Peso | número | Influência | 1 | 0 … 1 |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `roll` | Roll | número | Mira | 0 | -360 … 360 | graus | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `enabled` | Ativo | booleano | Influência | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_x` | Aplicar X | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_y` | Aplicar Y | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `axis_z` | Aplicar Z | booleano | Ajustes | verdadeiro | verdadeiro \| falso |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `up_axis` | Vertical de mundo | enumeração | Mira | Y | Y \| Z \| X |  | runtime/scene_constraints.h | pose e bounds | não | não | não |
| `target` | Fonte | referência | Influência | Escolher fonte | qualquer objeto · fora da subárvore |  | runtime/scene_constraints.h | pose e bounds | não | não | não |

## Transform Tween · `astra.tween.transform` v3

Interpola canais locais com espera, curva e repetição. **Consumidor:** runtime/scene_tweens.h. **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_tween.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `duration` | Duração | número | Tempo | 1 | 0.001 … 36000 | s | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `delay` | Espera | número | Tempo | 0 | 0 … 36000 | s | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `position_x` | Destino X | número | Destino | 0 | -100000 … 100000 | u | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `position_y` | Destino Y | número | Destino | 0 | -100000 … 100000 | u | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `position_z` | Destino Z | número | Destino | 0 | -100000 … 100000 | u | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `rotation_x` | Rotação X | número | Destino | 0 | -100000 … 100000 | graus | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `rotation_y` | Rotação Y | número | Destino | 0 | -100000 … 100000 | graus | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `rotation_z` | Rotação Z | número | Destino | 0 | -100000 … 100000 | graus | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `scale_x` | Escala X | número | Destino | 1 | -100000 … 100000 | x | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `scale_y` | Escala Y | número | Destino | 1 | -100000 … 100000 | x | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `scale_z` | Escala Z | número | Destino | 1 | -100000 … 100000 | x | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `enabled` | Ativo | booleano | Tempo | verdadeiro | verdadeiro \| falso |  | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `autoplay` | Iniciar no Play | booleano | Tempo | verdadeiro | verdadeiro \| falso |  | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `pingpong` | Ida e volta | booleano | Repetição | falso | verdadeiro \| falso |  | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `relative` | Destino relativo | booleano | Destino | falso | verdadeiro \| falso |  | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `position` | Mover | booleano | Destino | verdadeiro | verdadeiro \| falso |  | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `rotation` | Girar | booleano | Destino | falso | verdadeiro \| falso |  | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `scale` | Escalar | booleano | Destino | falso | verdadeiro \| falso |  | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `ignore_time_scale` | Ignorar escala de tempo | booleano | Tempo | falso | verdadeiro \| falso |  | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `easing` | Curva | enumeração | Tempo | Linear | Linear \| Smoothstep \| Quadrático entrada \| Quadrático saída |  | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `loops` | Ciclos | enumeração | Repetição | Uma vez | Infinito \| Uma vez \| Duas vezes \| Três vezes \| Dez vezes |  | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `finished_action` | Ao concluir | enumeração | Conexão | Desconectado | Desconectado \| Ativar objeto \| Desativar objeto \| Alternar objeto |  | runtime/scene_tweens.h | pose e bounds | não | não | não |
| `finished_target` | Receptor | referência | Conexão | Escolher objeto | qualquer objeto |  | runtime/scene_tweens.h | pose e bounds | sim | não | não |

**Métodos em Play**

| Método | Rótulo | Argumentos | Retorno | Efeito |
|---|---|---|---|---|
| `restart` | Reiniciar | — | nada | Recomeça da pose atual, conservando a pausa |
| `cancel` | Cancelar | — | nada | Interrompe sem voltar à pose inicial |
| `pause` | Pausar | — | nada | Congela o progresso |
| `resume` | Retomar | — | nada | Continua o progresso pausado |
| `elapsed` | Decorrido | — | número | Segundos desde o início, incluindo o atraso |

**Eventos em Play**

| Evento | Rótulo | Payload | Quando |
|---|---|---|---|
| `completed` | Concluiu | — | Emitido uma vez quando as repetições finitas terminam, depois da pose final |

## Tween de propriedade · `astra.tween.property` v1

Interpola uma propriedade numérica interpolável de um componente. **Consumidor:** runtime/scene_tweens.h → GameWorld::setTweenNumber. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_tween.html#class-tween-method-tween-property).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `destination` | Destino | número | Propriedade | 0 | -100000 … 100000 |  | runtime/scene_tweens.h → GameWorld::setTweenNumber | nada | não | não | não |
| `duration` | Duração | número | Tempo | 1 | 0.001 … 36000 | s | runtime/scene_tweens.h → GameWorld::setTweenNumber | nada | não | não | não |
| `delay` | Espera | número | Tempo | 0 | 0 … 36000 | s | runtime/scene_tweens.h → GameWorld::setTweenNumber | nada | não | não | não |
| `enabled` | Ativo | booleano | Tempo | verdadeiro | verdadeiro \| falso |  | runtime/scene_tweens.h → GameWorld::setTweenNumber | nada | não | não | não |
| `autoplay` | Iniciar no Play | booleano | Tempo | verdadeiro | verdadeiro \| falso |  | runtime/scene_tweens.h → GameWorld::setTweenNumber | nada | não | não | não |
| `pingpong` | Ida e volta | booleano | Repetição | falso | verdadeiro \| falso |  | runtime/scene_tweens.h → GameWorld::setTweenNumber | nada | não | não | não |
| `relative` | Destino relativo | booleano | Propriedade | falso | verdadeiro \| falso |  | runtime/scene_tweens.h → GameWorld::setTweenNumber | nada | não | não | não |
| `ignore_time_scale` | Ignorar escala de tempo | booleano | Tempo | falso | verdadeiro \| falso |  | runtime/scene_tweens.h → GameWorld::setTweenNumber | nada | não | não | não |
| `easing` | Curva | enumeração | Tempo | Linear | Linear \| Smoothstep \| Quadrático entrada \| Quadrático saída |  | runtime/scene_tweens.h → GameWorld::setTweenNumber | nada | não | não | não |
| `loops` | Repetição | enumeração | Repetição | Uma vez | Infinito \| Uma vez \| Duas vezes \| Três vezes \| Dez vezes |  | runtime/scene_tweens.h → GameWorld::setTweenNumber | nada | não | não | não |
| `target` | Alvo | referência | Propriedade | Este objeto | qualquer objeto |  | runtime/scene_tweens.h → GameWorld::setTweenNumber | nada | não | não | não |

**Métodos em Play**

| Método | Rótulo | Argumentos | Retorno | Efeito |
|---|---|---|---|---|
| `restart` | Reiniciar | — | nada | Recomeça do valor atual da propriedade, conservando a pausa |
| `cancel` | Cancelar | — | nada | Interrompe sem voltar ao valor inicial |
| `pause` | Pausar | — | nada | Congela o progresso |
| `resume` | Retomar | — | nada | Continua o progresso pausado |
| `elapsed` | Decorrido | — | número | Segundos desde o início, incluindo a espera |

**Eventos em Play**

| Evento | Rótulo | Payload | Quando |
|---|---|---|---|
| `completed` | Concluiu | — | Emitido uma vez quando as repetições finitas terminam, depois do valor final |

## Sequência de tweens · `astra.tween.sequence` v1

Encadeia Transform Tweens em etapas sequenciais ou paralelas. **Consumidor:** runtime/scene_tween_sequences.h. **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_tween.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `interval_0` | Intervalo da etapa 1 | número | Etapas | 0 | 0 … 3600 | s | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `interval_1` | Intervalo da etapa 2 | número | Etapas | 0 | 0 … 3600 | s | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `interval_2` | Intervalo da etapa 3 | número | Etapas | 0 | 0 … 3600 | s | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `interval_3` | Intervalo da etapa 4 | número | Etapas | 0 | 0 … 3600 | s | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `interval_4` | Intervalo da etapa 5 | número | Etapas | 0 | 0 … 3600 | s | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `interval_5` | Intervalo da etapa 6 | número | Etapas | 0 | 0 … 3600 | s | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `interval_6` | Intervalo da etapa 7 | número | Etapas | 0 | 0 … 3600 | s | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `interval_7` | Intervalo da etapa 8 | número | Etapas | 0 | 0 … 3600 | s | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `enabled` | Ativa | booleano | Execução | verdadeiro | verdadeiro \| falso |  | runtime/scene_tween_sequences.h | pose e bounds | não | não | não |
| `autoplay` | Iniciar no Play | booleano | Execução | verdadeiro | verdadeiro \| falso |  | runtime/scene_tween_sequences.h | pose e bounds | não | não | não |
| `ignore_time_scale` | Ignorar escala de tempo | booleano | Execução | falso | verdadeiro \| falso |  | runtime/scene_tween_sequences.h | pose e bounds | não | não | não |
| `join_1` | Etapa 2 junto da anterior | booleano | Etapas | falso | verdadeiro \| falso |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `join_2` | Etapa 3 junto da anterior | booleano | Etapas | falso | verdadeiro \| falso |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `join_3` | Etapa 4 junto da anterior | booleano | Etapas | falso | verdadeiro \| falso |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `join_4` | Etapa 5 junto da anterior | booleano | Etapas | falso | verdadeiro \| falso |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `join_5` | Etapa 6 junto da anterior | booleano | Etapas | falso | verdadeiro \| falso |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `join_6` | Etapa 7 junto da anterior | booleano | Etapas | falso | verdadeiro \| falso |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `join_7` | Etapa 8 junto da anterior | booleano | Etapas | falso | verdadeiro \| falso |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `loops` | Repetição | enumeração | Execução | Uma vez | Infinito \| Uma vez \| Duas vezes \| Três vezes \| Dez vezes |  | runtime/scene_tween_sequences.h | pose e bounds | não | não | não |
| `step_0` | Etapa 1 | referência | Etapas | Nenhum | objeto compatível com o receptor |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `step_1` | Etapa 2 | referência | Etapas | Nenhum | objeto compatível com o receptor |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `step_2` | Etapa 3 | referência | Etapas | Nenhum | objeto compatível com o receptor |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `step_3` | Etapa 4 | referência | Etapas | Nenhum | objeto compatível com o receptor |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `step_4` | Etapa 5 | referência | Etapas | Nenhum | objeto compatível com o receptor |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `step_5` | Etapa 6 | referência | Etapas | Nenhum | objeto compatível com o receptor |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `step_6` | Etapa 7 | referência | Etapas | Nenhum | objeto compatível com o receptor |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |
| `step_7` | Etapa 8 | referência | Etapas | Nenhum | objeto compatível com o receptor |  | runtime/scene_tween_sequences.h | pose e bounds | sim | não | não |

**Métodos em Play**

| Método | Rótulo | Argumentos | Retorno | Efeito |
|---|---|---|---|---|
| `play` | Tocar | — | nada | Recomeça da etapa 1, cancelando os tweens das etapas em andamento |
| `cancel` | Cancelar | — | nada | Interrompe a sequência e os tweens da etapa em andamento, sem voltar poses |
| `pause` | Pausar | — | nada | Congela intervalo e tweens da etapa em andamento |
| `resume` | Retomar | — | nada | Continua de onde pausou |
| `step` | Etapa atual | — | inteiro | Número da etapa em andamento (1 a 8); zero parada |

**Eventos em Play**

| Evento | Rótulo | Payload | Quando |
|---|---|---|---|
| `step_started` | Etapa começou | step: inteiro | Emitido quando cada etapa reinicia o seu tween; carrega o número da etapa |
| `completed` | Concluiu | — | Emitido uma vez quando as repetições finitas terminam |

## Timer · `astra.time.timer` v4

Dispara eventos temporizados e ações persistentes de ativação de objetos. **Consumidor:** runtime/scene_timers.h → ScriptBridge → Behavior.TimerElapsed. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_timer.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `interval_seconds` | Intervalo | número | Disparo | 1 | 0.05 … 3600 | s | runtime/scene_timers.h → ScriptBridge → Behavior.TimerElapsed | nada | não | não | não |
| `auto_start` | Iniciar automaticamente | booleano | Disparo | verdadeiro | verdadeiro \| falso |  | runtime/scene_timers.h → ScriptBridge → Behavior.TimerElapsed | nada | não | não | não |
| `repeat` | Repetir | booleano | Disparo | verdadeiro | verdadeiro \| falso |  | runtime/scene_timers.h → ScriptBridge → Behavior.TimerElapsed | nada | não | não | não |
| `enabled` | Ativo | booleano | Disparo | verdadeiro | verdadeiro \| falso |  | runtime/scene_timers.h → ScriptBridge → Behavior.TimerElapsed | nada | não | não | não |
| `ignore_time_scale` | Ignorar escala de tempo | booleano | Disparo | falso | verdadeiro \| falso |  | runtime/scene_timers.h → ScriptBridge → Behavior.TimerElapsed | nada | não | não | não |
| `elapsed_action` | Ao disparar | enumeração | Conexão | Desconectado | Desconectado \| Ativar objeto \| Desativar objeto \| Alternar objeto |  | runtime/scene_timers.h -> GameWorld::setActive | nada | não | não | não |
| `elapsed_target` | Receptor | referência | Conexão | Escolher objeto | qualquer objeto |  | runtime/scene_timers.h -> GameWorld::setActive | nada | sim | não | não |

**Métodos em Play**

| Método | Rótulo | Argumentos | Retorno | Efeito |
|---|---|---|---|---|
| `start` | Iniciar | interval: número (s) | nada | Reinicia a contagem; intervalo zero usa o autorado |
| `stop` | Parar | — | nada | Interrompe e zera a contagem |
| `pause` | Pausar | — | nada | Congela a contagem sem perder o restante |
| `resume` | Retomar | — | nada | Continua a contagem pausada |
| `remaining` | Restante | — | número | Segundos até o próximo disparo; zero quando parado |
| `running` | Em execução | — | booleano | Verdadeiro enquanto conta, inclusive pausado |

**Eventos em Play**

| Evento | Rótulo | Payload | Quando |
|---|---|---|---|
| `elapsed` | Disparou | count: inteiro | Um evento por quadro; Disparos conta intervalos vencidos no quadro |

## Conexão de evento · `astra.logic.event_connection` v1

Evento deste objeto aciona objetos ou métodos, sem script. **Consumidor:** runtime/scene_event_connections.h. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Events.UnityEvent.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `argument` | Valor | número | Então | 0 | 0 … 3600 | s | runtime/scene_event_connections.h | nada | sim | não | não |
| `enabled` | Ativa | booleano | Conexão | verdadeiro | verdadeiro \| falso |  | runtime/scene_event_connections.h | nada | não | não | não |
| `once` | Uma vez | booleano | Conexão | falso | verdadeiro \| falso |  | runtime/scene_event_connections.h | nada | sim | não | não |
| `event` | Evento | enumeração | Quando | Nenhum | Nenhum \| Timer disparou \| Tween concluiu \| Sensor 3D: entrou \| Sensor 3D: saiu \| Colisão 3D: começou \| Colisão 3D: terminou \| Sensor 2D: entrou \| Sensor 2D: saiu \| Colisão 2D: começou \| Colisão 2D: terminou \| Sequência: etapa começou \| Sequência concluiu \| Tween de propriedade concluiu |  | runtime/scene_event_connections.h | nada | não | não | não |
| `action` | Ação | enumeração | Então | Desconectado | Desconectado \| Ativar objeto \| Desativar objeto \| Alternar objeto \| Chamar método |  | runtime/scene_event_connections.h | nada | não | não | não |
| `method` | Método | enumeração | Então | Nenhum | Nenhum \| Áudio: tocar \| Áudio: parar \| Áudio: pausar \| Áudio: retomar \| Áudio: posicionar \| Timer: iniciar \| Timer: parar \| Timer: pausar \| Timer: retomar \| Tween: reiniciar \| Tween: cancelar \| Tween: pausar \| Tween: retomar \| Percurso: reiniciar \| Percurso: parar \| Sequência: tocar \| Sequência: cancelar \| Sequência: pausar \| Sequência: retomar \| Tween de propriedade: reiniciar \| Tween de propriedade: cancelar \| Tween de propriedade: pausar \| Tween de propriedade: retomar |  | runtime/scene_event_connections.h | nada | sim | não | não |
| `receiver` | Receptor | referência | Então | Este objeto | qualquer objeto |  | runtime/scene_event_connections.h | nada | sim | não | não |
| `other_filter` | Outro objeto | referência | Quando | Qualquer objeto | qualquer objeto |  | runtime/scene_event_connections.h | nada | sim | não | não |

## Comportamento · `astra.script.behavior` v1

Código C# do projeto. **Consumidor:** runtime/script_bridge.cpp → runtime .NET. **Invalida:** comportamento.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/MonoBehaviour.html).

**Durante Play:** estrutura não alterável; propriedades não alteráveis.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `enabled` | Ativo | booleano | Execução | verdadeiro | verdadeiro \| falso |  | runtime/script_bridge.cpp → runtime .NET | comportamento | não | não | não |

## Canvas UI · `astra.ui.canvas` v2

Instância independente de documento de UI, na tela ou no objeto. **Consumidor:** runtime/scene_gui.cpp → instâncias, layout, desenho e entrada. **Invalida:** desenho, entrada.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/class-Canvas.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

**Recursos endereçados**

| Binding | Rótulo | Tipo de recurso | Herda | Ausência declarada | ID por elemento |
|---|---|---|---|---|---|
| `document` | Documento UI | ui_document | não | não | não |

**Propriedades**

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `offset_x` | Posição X | número | Apresentação | 0 | -100000 … 100000 | m | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | sim | não | não |
| `offset_y` | Posição Y | número | Apresentação | 0 | -100000 … 100000 | m | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | sim | não | não |
| `offset_z` | Posição Z | número | Apresentação | 0 | -100000 … 100000 | m | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | sim | não | não |
| `rotation_x` | Rotação X | número | Apresentação | 0 | -36000 … 36000 | ° | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | sim | não | não |
| `rotation_y` | Rotação Y | número | Apresentação | 0 | -36000 … 36000 | ° | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | sim | não | não |
| `rotation_z` | Rotação Z | número | Apresentação | 0 | -36000 … 36000 | ° | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | sim | não | não |
| `width` | Largura | número | Canvas | 800 | 1 … 16384 | px | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | sim | não | não |
| `height` | Altura | número | Canvas | 600 | 1 … 16384 | px | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | sim | não | não |
| `units_per_pixel` | Unidades por pixel | número | Canvas | 0.005 | 0.00001 … 10 | m/px | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | sim | não | não |
| `order` | Ordem | número | Canvas | 0 | -10000 … 10000 |  | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | não | não | não |
| `enabled` | Habilitado | booleano | Canvas | verdadeiro | verdadeiro \| falso |  | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | não | não | não |
| `occlusion` | Oclusão no mundo | booleano | Canvas | verdadeiro | verdadeiro \| falso |  | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | sim | não | não |
| `mode` | Apresentação | enumeração | Canvas | Tela | Tela \| Mundo / objeto |  | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | não | não | não |
| `movement_space` | Espaço do movimento | enumeração | Entrada | Mundo | Mundo \| Local do jogador \| Camera de entrada |  | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | não | não | não |
| `input_receiver` | Jogador / receptor | referência | Entrada | Acoes globais | objeto compatível com o receptor |  | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | não | não | não |
| `input_camera` | Camera de entrada | referência | Entrada | Nenhuma | astra.camera |  | runtime/scene_gui.cpp → instâncias, layout, desenho e entrada | desenho, entrada | não | não | não |

## Malha · `astra.render.mesh` v9

Geometria e material. **Consumidor:** renderer/map_draw_update.h → instância e material efetivo. **Capacidade:** `render.material.pbr` (implementada). **Invalida:** desenho, material.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

**Recursos endereçados**

| Binding | Rótulo | Tipo de recurso | Herda | Ausência declarada | ID por elemento |
|---|---|---|---|---|---|
| `mesh` | Malha | mesh | não | não | não |
| `material` | Material | material | sim | sim | não |
| `texture.base_color` | Cor base | texture | sim | sim | não |
| `texture.normal` | Normal | texture | sim | sim | não |
| `texture.metallic_roughness` | Metal / rugosidade | texture | sim | sim | não |
| `texture.emissive` | Emissão | texture | sim | sim | não |
| `texture.lightmap` | Lightmap indireto (RGB linear / UV1) | texture | não | não | não |
| `texture.occlusion` | Oclusão | texture | sim | sim | não |

**Propriedades**

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `base_color.r` | Cor R | número |  | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não | não |
| `base_color.g` | Cor G | número |  | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não | não |
| `base_color.b` | Cor B | número |  | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não | não |
| `roughness` | Rugosidade | número |  | 0.5 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não | não |
| `metallic` | Metálico | número |  | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não | não |
| `normal_scale` | Intensidade da normal | número |  | 1 | 0 … 16 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não | não |
| `specular` | Especular | número |  | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não | não |
| `emission.r` | Emissão R | número |  | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não | não |
| `emission.g` | Emissão G | número |  | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não | não |
| `emission.b` | Emissão B | número |  | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não | não |
| `emission_strength` | Potência de emissão | número |  | 1 | 0 … 10000 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não | não |
| `enabled` | Renderizar | booleano |  | verdadeiro | verdadeiro \| falso |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não | não |
| `lightmap.scale_u` | Escala U | número | Lightmap | 1 | 0.00001 … 1 |  | editor/editor_map_scene.h -> material extension -> dirt_road_shading.glsl | desenho, material, residência de textura | não | sim | não |
| `lightmap.scale_v` | Escala V | número | Lightmap | 1 | 0.00001 … 1 |  | editor/editor_map_scene.h -> material extension -> dirt_road_shading.glsl | desenho, material, residência de textura | não | sim | não |
| `lightmap.offset_u` | Deslocamento U | número | Lightmap | 0 | 0 … 1 |  | editor/editor_map_scene.h -> material extension -> dirt_road_shading.glsl | desenho, material, residência de textura | não | sim | não |
| `lightmap.offset_v` | Deslocamento V | número | Lightmap | 0 | 0 … 1 |  | editor/editor_map_scene.h -> material extension -> dirt_road_shading.glsl | desenho, material, residência de textura | não | sim | não |
| `lightmap.intensity` | Intensidade | número | Lightmap | 1 | 0 … 10000 |  | editor/editor_map_scene.h -> material extension -> dirt_road_shading.glsl | desenho, material, residência de textura | não | sim | não |
| `surface.alpha_cutoff` | Corte do alfa | número | Superfície | 0.5 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `channels.occlusion_strength` | Força da oclusão | número | Canais | -1 | -1 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.offset_u` | Deslocamento U | número | Amostragem | 0 | -100 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.offset_v` | Deslocamento V | número | Amostragem | 0 | -100 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.scale_u` | Escala U | número | Amostragem | 1 | 0.01 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.scale_v` | Escala V | número | Amostragem | 1 | 0.01 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.rotation` | Rotação da UV | número | Amostragem | 0 | -360 … 360 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.base_color.offset_u` | Cor base / Deslocamento U | número | Amostragem | 0 | -100 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.base_color.offset_v` | Cor base / Deslocamento V | número | Amostragem | 0 | -100 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.base_color.scale_u` | Cor base / Escala U | número | Amostragem | 1 | 0.01 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.base_color.scale_v` | Cor base / Escala V | número | Amostragem | 1 | 0.01 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.base_color.rotation` | Cor base / Rotação | número | Amostragem | 0 | -360 … 360 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.normal.offset_u` | Normal / Deslocamento U | número | Amostragem | 0 | -100 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.normal.offset_v` | Normal / Deslocamento V | número | Amostragem | 0 | -100 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.normal.scale_u` | Normal / Escala U | número | Amostragem | 1 | 0.01 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.normal.scale_v` | Normal / Escala V | número | Amostragem | 1 | 0.01 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.normal.rotation` | Normal / Rotação | número | Amostragem | 0 | -360 … 360 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.metallic_roughness.offset_u` | Metal / rugosidade / Deslocamento U | número | Amostragem | 0 | -100 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.metallic_roughness.offset_v` | Metal / rugosidade / Deslocamento V | número | Amostragem | 0 | -100 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.metallic_roughness.scale_u` | Metal / rugosidade / Escala U | número | Amostragem | 1 | 0.01 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.metallic_roughness.scale_v` | Metal / rugosidade / Escala V | número | Amostragem | 1 | 0.01 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.metallic_roughness.rotation` | Metal / rugosidade / Rotação | número | Amostragem | 0 | -360 … 360 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.emissive.offset_u` | Emissão / Deslocamento U | número | Amostragem | 0 | -100 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.emissive.offset_v` | Emissão / Deslocamento V | número | Amostragem | 0 | -100 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.emissive.scale_u` | Emissão / Escala U | número | Amostragem | 1 | 0.01 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.emissive.scale_v` | Emissão / Escala V | número | Amostragem | 1 | 0.01 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.emissive.rotation` | Emissão / Rotação | número | Amostragem | 0 | -360 … 360 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `material.base_color.r` | Cor R | número | Cor | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `material.base_color.g` | Cor G | número | Cor | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `material.base_color.b` | Cor B | número | Cor | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `material.roughness` | Rugosidade | número | Superfície | 0.5 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `material.metallic` | Metálico | número | Superfície | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `material.normal_scale` | Intensidade da normal | número | Superfície | 1 | 0 … 16 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `material.specular` | Especular | número | Superfície | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `material.emission.r` | Emissão R | número | Emissão | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `material.emission.g` | Emissão G | número | Emissão | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `material.emission.b` | Emissão B | número | Emissão | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `material.emission_strength` | Potência de emissão | número | Emissão | 1 | 0 … 10000 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `material.override` | Material | enumeração | Cor | Herdado da fonte | Herdado da fonte \| Substituído nesta instância |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `lightmap.enabled` | Receber lightmap indireto | enumeração | Lightmap | Desativado | Desativado \| Lightmap indireto externo |  | editor/editor_map_scene.h -> material extension -> dirt_road_shading.glsl | desenho, material, residência de textura | não | sim | não |
| `surface.alpha_mode` | Tipo de superfície | enumeração | Superfície | Herdar | Herdar \| Opaco \| Recorte \| Mistura |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `surface.sides` | Faces | enumeração | Superfície | Herdar | Herdar \| Face única \| Face dupla |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `channels.roughness` | Canal da rugosidade | enumeração | Canais | Herdar | Herdar \| R \| G \| B \| A |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `channels.metallic` | Canal do metálico | enumeração | Canais | Herdar | Herdar \| R \| G \| B \| A |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `channels.occlusion` | Canal da oclusão | enumeração | Canais | Herdar | Herdar \| R \| G \| B \| A |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `channels.occlusion_source` | Origem da oclusão | enumeração | Canais | Herdar | Herdar \| Sem oclusão \| No mapa metal/rugosidade \| Textura própria |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `channels.normal_flip_y` | Inverter Y da normal | enumeração | Canais | Herdar | Herdar \| Não \| Sim |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `channels.alpha_source` | Origem do alfa | enumeração | Canais | Herdar | Herdar \| Alfa da cor base \| Sempre opaco \| Luminância da cor base |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.uv_set` | Conjunto de UV | enumeração | Amostragem | Herdar | Herdar \| UV 0 \| UV 1 \| Mundo (triplanar) |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.wrap` | Repetição | enumeração | Amostragem | Herdar | Herdar \| Repetir \| Fixar na borda \| Espelhar |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.filter` | Filtro | enumeração | Amostragem | Herdar | Herdar \| Linear \| Vizinho mais próximo |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.base_color.uv_set` | Cor base / UV | enumeração | Amostragem | Herdar | Herdar \| UV 0 \| UV 1 \| Mundo (triplanar) |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.base_color.wrap` | Cor base / Repetição | enumeração | Amostragem | Herdar | Herdar \| Repetir \| Fixar na borda \| Espelhar |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.base_color.filter` | Cor base / Filtro | enumeração | Amostragem | Herdar | Herdar \| Linear \| Vizinho mais próximo |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.normal.uv_set` | Normal / UV | enumeração | Amostragem | Herdar | Herdar \| UV 0 \| UV 1 \| Mundo (triplanar) |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.normal.wrap` | Normal / Repetição | enumeração | Amostragem | Herdar | Herdar \| Repetir \| Fixar na borda \| Espelhar |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.normal.filter` | Normal / Filtro | enumeração | Amostragem | Herdar | Herdar \| Linear \| Vizinho mais próximo |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.metallic_roughness.uv_set` | Metal / rugosidade / UV | enumeração | Amostragem | Herdar | Herdar \| UV 0 \| UV 1 \| Mundo (triplanar) |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.metallic_roughness.wrap` | Metal / rugosidade / Repetição | enumeração | Amostragem | Herdar | Herdar \| Repetir \| Fixar na borda \| Espelhar |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.metallic_roughness.filter` | Metal / rugosidade / Filtro | enumeração | Amostragem | Herdar | Herdar \| Linear \| Vizinho mais próximo |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.emissive.uv_set` | Emissão / UV | enumeração | Amostragem | Herdar | Herdar \| UV 0 \| UV 1 \| Mundo (triplanar) |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.emissive.wrap` | Emissão / Repetição | enumeração | Amostragem | Herdar | Herdar \| Repetir \| Fixar na borda \| Espelhar |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |
| `sampling.emissive.filter` | Emissão / Filtro | enumeração | Amostragem | Herdar | Herdar \| Linear \| Vizinho mais próximo |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim | não |

## Malha deformável · `astra.render.skinned_mesh` v2

Esqueleto e blend shapes da Malha. **Consumidor:** editor/editor_map_scene.cpp → paleta; platform/android/instanced_skinning.inl → compute. **Capacidade:** `render.skinning` (implementada). **Invalida:** desenho, mapa de sombra.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-SkinnedMeshRenderer.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Requer | `astra.render.mesh` | Adicione Malha a este objeto |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `skinned_motion_vectors` | Vetor de movimento da deformação | booleano | Skin | verdadeiro | verdadeiro \| falso |  | platform/android/instanced_motion.inl → passe de movimento com pose anterior | desenho, mapa de sombra | não | não | não |
| `quality` | Qualidade | enumeração | Skin | Automática | Automática \| 1 osso \| 2 ossos \| 4 ossos |  | editor/editor_map_scene.cpp → paleta; platform/android/instanced_skinning.inl → compute | desenho, mapa de sombra | não | não | não |
| `blend_shape_weight` | Peso do blend shape | número | Blend shapes |  | -1000 … 1000 | % | editor/editor_map_scene.cpp → pesos; platform/android/instanced_skinning.inl → compute | desenho, mapa de sombra | não | sim | não |

## LOD Group · `astra.render.lod_group` v3

Nível de detalhe pela altura na tela. **Consumidor:** runtime/lod_groups.h → visibilidade do desenho por vista. **Capacidade:** `render.lod.group` (implementada). **Invalida:** desenho.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-LODGroup.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `transition_0` | Transição LOD 0 | número | Níveis | 60 | 0.1 … 100 | % | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | não | não | não |
| `transition_1` | Transição LOD 1 | número | Níveis | 30 | 0.1 … 100 | % | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não | não |
| `transition_2` | Transição LOD 2 | número | Níveis | 10 | 0.1 … 100 | % | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não | não |
| `transition_3` | Transição LOD 3 | número | Níveis | 5 | 0.1 … 100 | % | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não | não |
| `size` | Tamanho | número | Limites | 1 | 0.001 … 1000000 |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | não | não | não |
| `fade_width_0` | Largura do fade LOD 0 | número | Fade | 0.2 | 0 … 1 |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não | não |
| `fade_width_1` | Largura do fade LOD 1 | número | Fade | 0.2 | 0 … 1 |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não | não |
| `fade_width_2` | Largura do fade LOD 2 | número | Fade | 0.2 | 0 … 1 |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não | não |
| `fade_width_3` | Largura do fade LOD 3 | número | Fade | 0.2 | 0 … 1 |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não | não |
| `enabled` | Ativo | booleano | Níveis | verdadeiro | verdadeiro \| falso |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | não | não | não |
| `animate_cross_fading` | Animate Cross-fading | booleano | Fade | falso | verdadeiro \| falso |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não | não |
| `level_count` | Níveis | enumeração | Níveis | 3 | 1 \| 2 \| 3 \| 4 |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | não | não | não |
| `fade_mode` | Fade Mode | enumeração | Fade | Nenhum | Nenhum \| Cross Fade |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | não | não | não |
| `force_level` | Forçar nível | enumeração | Execução | Automático | Automático \| LOD 0 \| LOD 1 \| LOD 2 \| LOD 3 |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não | não |
| `level_0` | Objetos LOD 0 | referência | Níveis | Nenhum | qualquer objeto · abaixo deste objeto |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | não | não | não |
| `level_1` | Objetos LOD 1 | referência | Níveis | Nenhum | qualquer objeto · abaixo deste objeto |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não | não |
| `level_2` | Objetos LOD 2 | referência | Níveis | Nenhum | qualquer objeto · abaixo deste objeto |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não | não |
| `level_3` | Objetos LOD 3 | referência | Níveis | Nenhum | qualquer objeto · abaixo deste objeto |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não | não |

## Ambiente · `astra.render.environment` v12

Céu, atmosfera, neblina e pós globais ou por volume. **Consumidor:** runtime/scene_environment.cpp → renderer e pós. **Invalida:** desenho, política resolvida.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/Packages/com.unity.render-pipelines.universal@17.0/manual/Volumes.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

**Recursos endereçados**

| Binding | Rótulo | Tipo de recurso | Herda | Ausência declarada | ID por elemento |
|---|---|---|---|---|---|
| `profile` | Perfil | environment_profile | sim | não | não |
| `environment_map` | Mapa HDRI | environment_map | sim | não | não |

**Propriedades**

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `priority` | Prioridade | número | Geral | 0 | -1000 … 1000 |  | runtime/scene_environment.cpp → seleção | desenho, política resolvida | não | não | não |
| `sky_zenith.r` | Zênite R | número | Atmosfera | 0.025 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `sky_zenith.g` | Zênite G | número | Atmosfera | 0.1 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `sky_zenith.b` | Zênite B | número | Atmosfera | 0.32 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `sky_horizon.r` | Horizonte R | número | Atmosfera | 0.28 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `sky_horizon.g` | Horizonte G | número | Atmosfera | 0.42 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `sky_horizon.b` | Horizonte B | número | Atmosfera | 0.62 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `ground.r` | Chão R | número | Atmosfera | 0.11 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `ground.g` | Chão G | número | Atmosfera | 0.12 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `ground.b` | Chão B | número | Atmosfera | 0.14 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `atmosphere` | Força atmosférica | número | Atmosfera | 1 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `sun_disk_degrees` | Diâmetro do sol | número | Atmosfera | 0.53 | 0.05 … 10 | ° | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `sun_disk_intensity` | Brilho do disco solar | número | Atmosfera | 8 | 0 … 100 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `fog_color.r` | Neblina R | número | Neblina | 0.58 | 0 … 1 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `fog_color.g` | Neblina G | número | Neblina | 0.67 | 0 … 1 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `fog_color.b` | Neblina B | número | Neblina | 0.76 | 0 … 1 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `fog_light_energy` | Energia da neblina | número | Neblina | 1 | 0 … 65504 | × | platform/android/instanced_renderer.cpp | desenho, política resolvida | sim | não | não |
| `fog_density` | Densidade | número | Neblina | 0.008 | 0 … 1 | 1/m | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `fog_start` | Início | número | Neblina | 8 | 0 … 10000 | m | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `fog_base_height` | Altura base | número | Neblina | 0 | -100000 … 100000 | m | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `fog_height_falloff` | Decaimento por altura | número | Neblina | 0 | 0 … 10 | 1/m | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `exposure_ev` | Compensação | número | Exposição | 0 | -16 … 16 | EV | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `bloom_threshold` | Limiar do bloom | número | Pós | 1 | 0 … 64 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `bloom_intensity` | Intensidade do bloom | número | Pós | 0.1 | 0 … 2 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `contrast` | Contraste | número | Pós | 1 | 0.5 … 2 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `saturation` | Saturação | número | Pós | 1 | 0 … 2 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `vignette_intensity` | Intensidade da vinheta | número | Pós | 0.18 | 0 … 1 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `film_grain_intensity` | Intensidade do grão | número | Pós | 0.05 | 0 … 1 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `ambient_occlusion_radius` | Raio | número | Oclusão ambiente | 1 | 0.05 … 10 | m | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `ambient_occlusion_intensity` | Intensidade | número | Oclusão ambiente | 1 | 0 … 4 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `ambient_occlusion_power` | Potência | número | Oclusão ambiente | 1.5 | 0.1 … 4 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `ambient_occlusion_bias` | Viés | número | Oclusão ambiente | 0.02 | 0 … 1 | m | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `weight` | Peso | número | Volume | 1 | 0 … 1 |  | renderer/scene_environment.cpp | desenho, política resolvida | não | não | não |
| `blend_distance` | Distância de mistura | número | Volume | 0 | 0 … 100000 | m | renderer/scene_environment.cpp | desenho, política resolvida | sim | não | não |
| `box_size.x` | Tamanho X | número | Volume | 10 | 0.01 … 100000 | m | renderer/scene_environment.cpp | desenho, política resolvida | sim | não | não |
| `box_size.y` | Tamanho Y | número | Volume | 10 | 0.01 … 100000 | m | renderer/scene_environment.cpp | desenho, política resolvida | sim | não | não |
| `box_size.z` | Tamanho Z | número | Volume | 10 | 0.01 … 100000 | m | renderer/scene_environment.cpp | desenho, política resolvida | sim | não | não |
| `sphere_radius` | Raio | número | Volume | 5 | 0.01 … 100000 | m | renderer/scene_environment.cpp | desenho, política resolvida | sim | não | não |
| `indirect_diffuse` | Difuso indireto | número | Luz indireta | 1 | 0 … 4 | × | rhi/shaders/dirt_road_shading.glsl | desenho, política resolvida | não | não | não |
| `indirect_specular` | Reflexo indireto | número | Luz indireta | 1 | 0 … 4 | × | rhi/shaders/environment_lighting.glsl | desenho, política resolvida | não | não | não |
| `physical_sky_intensity` | Intensidade | número | Atmosfera física | 1 | 0 … 16 | × | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `air_density` | Densidade do ar | número | Atmosfera física | 1 | 0 … 8 | × | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `aerosol_density` | Densidade de aerossóis | número | Atmosfera física | 1 | 0 … 8 | × | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `aerosol_anisotropy` | Anisotropia dos aerossóis | número | Atmosfera física | 0.76 | 0 … 0.95 | g | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `planet_radius_km` | Raio do planeta | número | Atmosfera física | 6371 | 1 … 100000 | km | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `observer_height_km` | Altura do observador | número | Atmosfera física | 0.002 | 0 … 1000 | km | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `rayleigh_scale_height_km` | Escala Rayleigh | número | Atmosfera física | 8 | 0.1 … 100 | km | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `aerosol_scale_height_km` | Escala de aerossóis | número | Atmosfera física | 1.2 | 0.05 … 50 | km | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `atmosphere_height_km` | Altura da atmosfera | número | Atmosfera física | 100 | 1 … 1000 | km | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `ground_albedo` | Albedo médio do solo | número | Atmosfera física | 0.1 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `auto_exposure_min_ev` | EV mínimo | número | Exposição | -8 | -16 … 16 | EV | platform/android/instanced_auto_exposure.inl | desenho, política resolvida | sim | não | não |
| `auto_exposure_max_ev` | EV máximo | número | Exposição | 8 | -16 … 16 | EV | platform/android/instanced_auto_exposure.inl | desenho, política resolvida | sim | não | não |
| `auto_exposure_low_percent` | Corte baixo | número | Exposição | 0.05 | 0 … 1 |  | platform/android/instanced_auto_exposure.inl | desenho, política resolvida | sim | não | não |
| `auto_exposure_high_percent` | Corte alto | número | Exposição | 0.95 | 0 … 1 |  | platform/android/instanced_auto_exposure.inl | desenho, política resolvida | sim | não | não |
| `auto_exposure_target_grey` | Cinza alvo | número | Exposição | 0.18 | 0.01 … 1 |  | platform/android/instanced_auto_exposure.inl | desenho, política resolvida | sim | não | não |
| `auto_exposure_speed_up` | Velocidade ao escurecer | número | Exposição | 2 | 0.01 … 20 | EV/s | platform/android/instanced_auto_exposure.inl | desenho, política resolvida | sim | não | não |
| `auto_exposure_speed_down` | Velocidade ao clarear | número | Exposição | 3 | 0.01 … 20 | EV/s | platform/android/instanced_auto_exposure.inl | desenho, política resolvida | sim | não | não |
| `hdri_rotation_degrees` | Rotação HDRI | número | HDRI | 0 | -360 … 360 | ° | rhi/shaders/environment_lighting.glsl | desenho, política resolvida | sim | não | não |
| `hdri_exposure_ev` | Exposição HDRI | número | HDRI | 0 | -16 … 16 | EV | rhi/shaders/environment_lighting.glsl | desenho, política resolvida | sim | não | não |
| `enabled` | Ativo | booleano | Geral | verdadeiro | verdadeiro \| falso |  | runtime/scene_environment.cpp → renderer e pós | desenho, política resolvida | não | não | não |
| `fog` | Neblina | booleano | Neblina | falso | verdadeiro \| falso |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | não | não | não |
| `post` | Pós-processamento | booleano | Pós | verdadeiro | verdadeiro \| falso |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | não | não | não |
| `auto_exposure` | Exposição automática | booleano | Exposição | falso | verdadeiro \| falso |  | platform/android/instanced_auto_exposure.inl | desenho, política resolvida | sim | não | não |
| `auto_exposure_center_weighted` | Peso central | booleano | Exposição | falso | verdadeiro \| falso |  | platform/android/instanced_auto_exposure.inl | desenho, política resolvida | sim | não | não |
| `bloom` | Bloom | booleano | Pós | verdadeiro | verdadeiro \| falso |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `vignette` | Vinheta | booleano | Pós | falso | verdadeiro \| falso |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `film_grain` | Grão de filme | booleano | Pós | falso | verdadeiro \| falso |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `ambient_occlusion` | Oclusão ambiente | booleano | Oclusão ambiente | falso | verdadeiro \| falso |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `override_sky` | Sobrescrever céu | booleano | Volume | verdadeiro | verdadeiro \| falso |  | renderer/scene_environment.cpp | desenho, política resolvida | não | não | não |
| `override_fog` | Sobrescrever neblina | booleano | Volume | verdadeiro | verdadeiro \| falso |  | renderer/scene_environment.cpp | desenho, política resolvida | não | não | não |
| `override_post` | Sobrescrever pós | booleano | Volume | verdadeiro | verdadeiro \| falso |  | renderer/scene_environment.cpp | desenho, política resolvida | não | não | não |
| `override_indirect` | Sobrescrever luz indireta | booleano | Volume | verdadeiro | verdadeiro \| falso |  | renderer/scene_environment.cpp | desenho, política resolvida | não | não | não |
| `physical_atmosphere_high_quality` | Alta qualidade | booleano | Atmosfera física | falso | verdadeiro \| falso |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não | não |
| `sky` | Céu | enumeração | Atmosfera | Atmosfera | HDRI \| Atmosfera \| Atmosfera física |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | não | não | não |
| `tone_mapper` | Tonemapping | enumeração | Pós | ACES | Reinhard \| ACES \| AgX |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não | não |
| `volume_shape` | Modo | enumeração | Volume | Global | Global \| Caixa \| Esfera |  | runtime/scene_environment.cpp | desenho, política resolvida | não | não | não |
| `volume_layer` | Camada | enumeração | Volume | Ambiente 0 | Ambiente 0 \| Ambiente 1 \| Ambiente 2 \| Ambiente 3 \| Ambiente 4 \| Ambiente 5 \| Ambiente 6 \| Ambiente 7 |  | renderer/scene_environment.cpp | desenho, política resolvida | não | não | não |

## Luz · `astra.render.light` v3

Direcional, pontual ou spot. **Consumidor:** runtime/scene_lights.cpp → renderer/punctual_lights.h. **Invalida:** seleção de luzes.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Light.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `color.r` | Cor R | número | Emissão | 1 | 0 … 1 |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não | não | sim |
| `color.g` | Cor G | número | Emissão | 1 | 0 … 1 |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não | não | sim |
| `color.b` | Cor B | número | Emissão | 1 | 0 … 1 |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não | não | sim |
| `color_temperature` | Temperatura | número | Emissão | 6500 | 1667 … 25000 | K | scene/light_units.h → RGB linear | seleção de luzes | sim | não | não |
| `intensity` | Intensidade | número | Emissão | 1000 | 0 … 1000000 |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não | não | sim |
| `range` | Alcance | número | Volume | 10 | 0.01 … 1000 | m | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | sim | não | não |
| `inner_angle` | Meio-cone interno | número | Volume | 20 | 0 … 89 | ° | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | sim | não | não |
| `outer_angle` | Meio-cone externo | número | Volume | 35 | 0 … 89 | ° | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | sim | não | não |
| `shadow_strength` | Força da sombra | número | Sombra | 1 | 0 … 1 |  | renderer/shadow_atlas.h → atlas local | seleção de luzes | sim | não | não |
| `shadow_bias` | Desvio | número | Sombra | 0.05 | 0 … 2 | texel | renderer/shadow_atlas.h → atlas local | seleção de luzes | sim | não | não |
| `shadow_normal_bias` | Desvio na normal | número | Sombra | 0.4 | 0 … 2 | texel | renderer/shadow_atlas.h → atlas local | seleção de luzes | sim | não | não |
| `shadow_near_plane` | Plano próximo da sombra | número | Sombra | 0.2 | 0.01 … 10 | m | renderer/shadow_atlas.h → atlas local | seleção de luzes | sim | não | não |
| `enabled` | Acesa | booleano | Geral | verdadeiro | verdadeiro \| falso |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não | não | não |
| `use_color_temperature` | Filtro por temperatura | booleano | Emissão | falso | verdadeiro \| falso |  | scene/light_units.h → RGB linear | seleção de luzes | não | não | não |
| `kind` | Modalidade | enumeração | Geral | Pontual | Direcional \| Pontual \| Spot |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não | não | não |
| `unit` | Unidade | enumeração | Emissão | Lux / lúmen | Interna (legada) \| Lux / candela \| Lux / lúmen |  | scene/light_units.h → irradiância linear | seleção de luzes | não | não | não |
| `shadow_mode` | Sombra | enumeração | Sombra | Nenhuma | Nenhuma \| Dura \| Suave |  | renderer/shadow_atlas.h → atlas local | seleção de luzes | sim | não | não |
| `shadow_resolution` | Resolução da sombra | enumeração | Sombra | Automática | Automática \| Baixa \| Média \| Alta \| Muito alta |  | renderer/shadow_atlas.h → atlas local | seleção de luzes | sim | não | não |

## Câmera · `astra.camera` v3

Projeção e enquadramento. **Consumidor:** renderer/render_view.h → matriz de projeção e culling. **Invalida:** desenho.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Camera.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `vertical_fov` | Campo vertical | número | Lente | 60 | 1 … 170 | ° | renderer/render_view.h → matriz de projeção e culling | desenho | sim | não | sim |
| `near_plane` | Próximo | número | Lente | 0.1 | 0.001 … 10000 | m | renderer/render_view.h → matriz de projeção e culling | desenho | não | não | não |
| `far_plane` | Distante | número | Lente | 2000 | 0.01 … 1000000 | m | renderer/render_view.h → matriz de projeção e culling | desenho | não | não | não |
| `priority` | Prioridade | número | Saída | 0 | -10000 … 10000 |  | renderer/render_view.h → matriz de projeção e culling | desenho | não | não | não |
| `orthographic_half_height` | Meia altura | número | Lente | 5 | 0.001 … 100000 | m | renderer/render_view.h → matriz de projeção e culling | desenho | sim | não | sim |
| `enabled` | Usar no Play | booleano | Saída | verdadeiro | verdadeiro \| falso |  | renderer/render_view.h → matriz de projeção e culling | desenho | não | não | não |
| `projection` | Projeção | enumeração | Lente | Perspectiva | Perspectiva \| Ortográfica |  | renderer/render_view.h → matriz de projeção e culling | desenho | não | não | não |
| `environment_mask` | Ambientes | enumeração | Saída | Todos os ambientes | Todos os ambientes \| Ambiente 0 \| Ambiente 1 \| Ambiente 2 \| Ambiente 3 \| Ambiente 4 \| Ambiente 5 \| Ambiente 6 \| Ambiente 7 |  | renderer/scene_environment.cpp | desenho | não | não | não |

## Olhar · `astra.camera.look` v2

Rotação local da câmera por entrada ou script. **Consumidor:** runtime/game_world.cpp → pose da câmera. **Invalida:** entrada.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachinePanTilt.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Requer | `astra.camera` | Adicione Câmera a este objeto |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `yaw_sensitivity` | Sensibilidade horizontal graus/tela | número | Sensibilidade | 300 | 0 … 720 | ° | runtime/game_world.cpp → pose da câmera | entrada | não | não | não |
| `pitch_sensitivity` | Sensibilidade vertical graus/tela | número | Sensibilidade | 195 | 0 … 720 | ° | runtime/game_world.cpp → pose da câmera | entrada | não | não | não |
| `pitch_limit` | Limite vertical graus | número | Limites | 83 | 1 … 89 | ° | runtime/game_world.cpp → pose da câmera | entrada | não | não | não |
| `enabled` | Ativo | booleano | Controle | verdadeiro | verdadeiro \| falso |  | runtime/game_world.cpp → pose da câmera | entrada | não | não | não |

## Acompanhar alvo · `astra.camera.follow` v1

Posiciona a câmera após física e animação. **Consumidor:** runtime/scene_camera_follow.h → pose de Play da câmera. **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineFollow.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Requer | `astra.camera` | Adicione Câmera a este objeto |
| Incompatível | `astra.physics.body` | A câmera seguidora não pode receber pose do corpo físico |
| Incompatível | `astra.physics.character` | A câmera seguidora não pode receber pose do personagem |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `offset_x` | Deslocamento X | número | Posição | 0 | -10000 … 10000 | m | runtime/scene_camera_follow.h → pose de Play da câmera | pose e bounds | não | não | não |
| `offset_y` | Deslocamento Y | número | Posição | 2 | -10000 … 10000 | m | runtime/scene_camera_follow.h → pose de Play da câmera | pose e bounds | não | não | não |
| `offset_z` | Deslocamento Z | número | Posição | -5 | -10000 … 10000 | m | runtime/scene_camera_follow.h → pose de Play da câmera | pose e bounds | não | não | não |
| `damping_seconds` | Amortecimento | número | Resposta | 0.2 | 0 … 30 | s | runtime/scene_camera_follow.h → pose de Play da câmera | pose e bounds | não | não | não |
| `enabled` | Ativo | booleano | Resposta | verdadeiro | verdadeiro \| falso |  | runtime/scene_camera_follow.h → pose de Play da câmera | pose e bounds | não | não | não |
| `target` | Alvo | referência | Posição | Escolher objeto | qualquer objeto · fora da subárvore |  | runtime/scene_camera_follow.h → pose de Play da câmera | pose e bounds | não | não | não |

## Receita de colisão · `astra.physics.collision_recipe` v1

Fontes e base das partes para regeneração explícita preservando edições. **Consumidor:** editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/tutorials/assets_pipeline/import_process.html).

**Durante Play:** estrutura não alterável; propriedades não alteráveis.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Requer | `astra.physics.body` | Adicione Corpo físico a este objeto |

**Recursos endereçados**

| Binding | Rótulo | Tipo de recurso | Herda | Ausência declarada | ID por elemento |
|---|---|---|---|---|---|
| `bake_source` | Revisão gerada | mesh | não | não | não |
| `baseline_mesh` | Malha base da parte | mesh | não | não | não |

**Propriedades**

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `source_0` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_1` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_2` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_3` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_4` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_5` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_6` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_7` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_8` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_9` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_10` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_11` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_12` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_13` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_14` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_15` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_16` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_17` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_18` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_19` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_20` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_21` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_22` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_23` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_24` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_25` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_26` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_27` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_28` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_29` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_30` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_31` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_32` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_33` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_34` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_35` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_36` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_37` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_38` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_39` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_40` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_41` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_42` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_43` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_44` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_45` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_46` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_47` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_48` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_49` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_50` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_51` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_52` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_53` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_54` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_55` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_56` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_57` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_58` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_59` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_60` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_61` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_62` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_63` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_64` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_65` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_66` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_67` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_68` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_69` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_70` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_71` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_72` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_73` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_74` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_75` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_76` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_77` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_78` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_79` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_80` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_81` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_82` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_83` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_84` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_85` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_86` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_87` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_88` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_89` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_90` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_91` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_92` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_93` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_94` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_95` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_96` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_97` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_98` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_99` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_100` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_101` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_102` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_103` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_104` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_105` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_106` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_107` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_108` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_109` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_110` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_111` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_112` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_113` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_114` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_115` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_116` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_117` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_118` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_119` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_120` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_121` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_122` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_123` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_124` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_125` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_126` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |
| `source_127` | Fonte da colisão | referência | Fontes | Fonte ausente | qualquer objeto |  | editor/editor_collision_regeneration.cpp → Colliders persistidos → runtime/scene_physics.cpp | nada | sim | não | não |

## Campo de gravidade · `astra.physics.field.gravity` v1

Gravidade local em volume sobre corpos dinâmicos. **Consumidor:** runtime/scene_physics_fields.inl → Jolt. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_area3d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `half_x` | Meia extensão X | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `half_y` | Meia extensão Y | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `half_z` | Meia extensão Z | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `radius` | Raio | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `offset_x` | Centro X | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `offset_y` | Centro Y | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `offset_z` | Centro Z | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `vector_x` | Vetor X | número | Efeito | 0 | -10000 … 10000 | m/s² | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `vector_y` | Vetor Y | número | Efeito | -9.81 | -10000 … 10000 | m/s² | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `vector_z` | Vetor Z | número | Efeito | 0 | -10000 … 10000 | m/s² | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `enabled` | Ativo | booleano | Efeito | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `wake_bodies` | Acordar corpos | booleano | Alcance | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `replace_world_gravity` | Substituir gravidade do mundo | booleano | Efeito | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `shape` | Forma | enumeração | Volume | Caixa | Caixa \| Esfera |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `falloff` | Queda de influência | enumeração | Alcance | Uniforme | Uniforme \| Linear \| Suave |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `affected_layer` | Camada afetada | enumeração | Alcance | Todas | Todas \| Camada 0 \| Camada 1 \| Camada 2 \| Camada 3 \| Camada 4 \| Camada 5 \| Camada 6 \| Camada 7 \| Camada 8 \| Camada 9 \| Camada 10 \| Camada 11 \| Camada 12 \| Camada 13 \| Camada 14 \| Camada 15 \| Camada 16 \| Camada 17 \| Camada 18 \| Camada 19 \| Camada 20 \| Camada 21 \| Camada 22 \| Camada 23 \| Camada 24 \| Camada 25 \| Camada 26 \| Camada 27 \| Camada 28 \| Camada 29 \| Camada 30 \| Camada 31 |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |

## Campo de vento · `astra.physics.field.wind` v1

Arrasto para velocidade local do ar, dependente da massa. **Consumidor:** runtime/scene_physics_fields.inl → Jolt. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_area3d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `half_x` | Meia extensão X | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `half_y` | Meia extensão Y | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `half_z` | Meia extensão Z | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `radius` | Raio | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `offset_x` | Centro X | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `offset_y` | Centro Y | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `offset_z` | Centro Z | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `vector_x` | Vetor X | número | Efeito | 5 | -10000 … 10000 | m/s | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `vector_y` | Vetor Y | número | Efeito | 0 | -10000 … 10000 | m/s | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `vector_z` | Vetor Z | número | Efeito | 0 | -10000 … 10000 | m/s | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `coefficient` | Acoplamento | número | Efeito | 1 | 0 … 10000 | kg/s | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `enabled` | Ativo | booleano | Efeito | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `wake_bodies` | Acordar corpos | booleano | Alcance | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `shape` | Forma | enumeração | Volume | Caixa | Caixa \| Esfera |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `falloff` | Queda de influência | enumeração | Alcance | Uniforme | Uniforme \| Linear \| Suave |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `affected_layer` | Camada afetada | enumeração | Alcance | Todas | Todas \| Camada 0 \| Camada 1 \| Camada 2 \| Camada 3 \| Camada 4 \| Camada 5 \| Camada 6 \| Camada 7 \| Camada 8 \| Camada 9 \| Camada 10 \| Camada 11 \| Camada 12 \| Camada 13 \| Camada 14 \| Camada 15 \| Camada 16 \| Camada 17 \| Camada 18 \| Camada 19 \| Camada 20 \| Camada 21 \| Camada 22 \| Camada 23 \| Camada 24 \| Camada 25 \| Camada 26 \| Camada 27 \| Camada 28 \| Camada 29 \| Camada 30 \| Camada 31 |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |

## Campo de arrasto · `astra.physics.field.drag` v1

Amortecimento linear e angular local. **Consumidor:** runtime/scene_physics_fields.inl → Jolt. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_area3d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `half_x` | Meia extensão X | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `half_y` | Meia extensão Y | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `half_z` | Meia extensão Z | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `radius` | Raio | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `offset_x` | Centro X | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `offset_y` | Centro Y | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `offset_z` | Centro Z | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `linear_drag` | Arrasto linear | número | Efeito | 1 | 0 … 1000 | 1/s | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `angular_drag` | Arrasto angular | número | Efeito | 1 | 0 … 1000 | 1/s | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `enabled` | Ativo | booleano | Efeito | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `wake_bodies` | Acordar corpos | booleano | Alcance | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `shape` | Forma | enumeração | Volume | Caixa | Caixa \| Esfera |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `falloff` | Queda de influência | enumeração | Alcance | Uniforme | Uniforme \| Linear \| Suave |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `affected_layer` | Camada afetada | enumeração | Alcance | Todas | Todas \| Camada 0 \| Camada 1 \| Camada 2 \| Camada 3 \| Camada 4 \| Camada 5 \| Camada 6 \| Camada 7 \| Camada 8 \| Camada 9 \| Camada 10 \| Camada 11 \| Camada 12 \| Camada 13 \| Camada 14 \| Camada 15 \| Camada 16 \| Camada 17 \| Camada 18 \| Camada 19 \| Camada 20 \| Camada 21 \| Camada 22 \| Camada 23 \| Camada 24 \| Camada 25 \| Camada 26 \| Camada 27 \| Camada 28 \| Camada 29 \| Camada 30 \| Camada 31 |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |

## Campo radial · `astra.physics.field.radial` v1

Atração, repulsão e vórtice ao redor do centro. **Consumidor:** runtime/scene_physics_fields.inl → Jolt. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_area3d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `half_x` | Meia extensão X | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `half_y` | Meia extensão Y | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `half_z` | Meia extensão Z | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `radius` | Raio | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | sim | não | não |
| `offset_x` | Centro X | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `offset_y` | Centro Y | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `offset_z` | Centro Z | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `acceleration` | Aceleração radial | número | Efeito | -9.81 | -10000 … 10000 | m/s² | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `tangential_acceleration` | Aceleração tangencial | número | Efeito | 0 | -10000 … 10000 | m/s² | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `enabled` | Ativo | booleano | Efeito | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `wake_bodies` | Acordar corpos | booleano | Alcance | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `shape` | Forma | enumeração | Volume | Caixa | Caixa \| Esfera |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `falloff` | Queda de influência | enumeração | Alcance | Uniforme | Uniforme \| Linear \| Suave |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |
| `affected_layer` | Camada afetada | enumeração | Alcance | Todas | Todas \| Camada 0 \| Camada 1 \| Camada 2 \| Camada 3 \| Camada 4 \| Camada 5 \| Camada 6 \| Camada 7 \| Camada 8 \| Camada 9 \| Camada 10 \| Camada 11 \| Camada 12 \| Camada 13 \| Camada 14 \| Camada 15 \| Camada 16 \| Camada 17 \| Camada 18 \| Camada 19 \| Camada 20 \| Camada 21 \| Camada 22 \| Camada 23 \| Camada 24 \| Camada 25 \| Camada 26 \| Camada 27 \| Camada 28 \| Camada 29 \| Camada 30 \| Camada 31 |  | runtime/scene_physics_fields.inl → Jolt | nada | não | não | não |

## Conexão física 3D · `astra.physics.event_connection` v1

Evento de sensor/contato altera a ativação de um receptor. **Consumidor:** runtime/scene_physics_connections.h → GameWorld::setActive. **Invalida:** corpo físico.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_area3d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Requer | `astra.physics.body` | Adicione Corpo físico ao emissor da conexão |
| Requer | `astra.physics.collider` | Adicione Colisor 3D ao emissor da conexão |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `enabled` | Ativa | booleano | Conexão | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics_connections.h → GameWorld::setActive | corpo físico | não | não | não |
| `event` | Evento | enumeração | Conexão | Entrada sensor | Entrada sensor \| Perm. sensor \| Saída sensor \| Entrada contato \| Perm. contato \| Saída contato |  | runtime/scene_physics_connections.h → GameWorld::setActive | corpo físico | não | não | não |
| `action` | Ação | enumeração | Conexão | Desconectado | Desconectado \| Ativar objeto \| Desativar objeto \| Alternar objeto |  | runtime/scene_physics_connections.h → GameWorld::setActive | corpo físico | não | não | não |
| `receiver` | Receptor | referência | Conexão | Escolher objeto | qualquer objeto |  | runtime/scene_physics_connections.h → GameWorld::setActive | corpo físico | sim | não | não |
| `other_filter` | Outro objeto | referência | Filtro | Qualquer objeto | qualquer objeto |  | runtime/scene_physics_connections.h → GameWorld::setActive | corpo físico | sim | não | não |

## Força constante · `astra.physics.constant_force` v1

Força e torque contínuos sobre corpo dinâmico. **Consumidor:** runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ConstantForce.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Requer | `astra.physics.body` | Adicione Corpo físico a este objeto |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `force_x` | Força X | número | Força mundo | 0 | -10000000 … 10000000 | N | runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt | nada | não | não | não |
| `force_y` | Força Y | número | Força mundo | 0 | -10000000 … 10000000 | N | runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt | nada | não | não | não |
| `force_z` | Força Z | número | Força mundo | 0 | -10000000 … 10000000 | N | runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt | nada | não | não | não |
| `relative_force_x` | Força local X | número | Força local | 0 | -10000000 … 10000000 | N | runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt | nada | não | não | não |
| `relative_force_y` | Força local Y | número | Força local | 0 | -10000000 … 10000000 | N | runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt | nada | não | não | não |
| `relative_force_z` | Força local Z | número | Força local | 0 | -10000000 … 10000000 | N | runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt | nada | não | não | não |
| `torque_x` | Torque X | número | Torque mundo | 0 | -10000000 … 10000000 | N·m | runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt | nada | não | não | não |
| `torque_y` | Torque Y | número | Torque mundo | 0 | -10000000 … 10000000 | N·m | runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt | nada | não | não | não |
| `torque_z` | Torque Z | número | Torque mundo | 0 | -10000000 … 10000000 | N·m | runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt | nada | não | não | não |
| `relative_torque_x` | Torque local X | número | Torque local | 0 | -10000000 … 10000000 | N·m | runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt | nada | não | não | não |
| `relative_torque_y` | Torque local Y | número | Torque local | 0 | -10000000 … 10000000 | N·m | runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt | nada | não | não | não |
| `relative_torque_z` | Torque local Z | número | Torque local | 0 | -10000000 … 10000000 | N·m | runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt | nada | não | não | não |
| `enabled` | Ativo | booleano | Força mundo | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt | nada | não | não | não |

## Corpo físico · `astra.physics.body` v5

Massa e resposta física. **Consumidor:** runtime/scene_physics.cpp → Jolt. **Invalida:** corpo físico.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Rigidbody.html).

**Durante Play:** estrutura não alterável; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Incompatível | `astra.physics.character` | Incompatível com personagem cápsula |

**Recursos endereçados**

| Binding | Rótulo | Tipo de recurso | Herda | Ausência declarada | ID por elemento |
|---|---|---|---|---|---|
| `material` | Material físico | physics_material | não | não | não |

**Propriedades**

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `mass` | Massa kg | número | Corpo | 1 | 0.01 … 1000000 | kg | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `friction` | Atrito | número | Material | 0.5 | 0 … 1 |  | runtime/scene_physics.cpp → Jolt | corpo físico | não | não | não |
| `restitution` | Restituição | número | Material | 0 | 0 … 1 |  | runtime/scene_physics.cpp → Jolt | corpo físico | não | não | não |
| `velocity_x` | Velocidade inicial X | número | Início | 0 | -1000 … 1000 | m/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `velocity_y` | Velocidade inicial Y | número | Início | 0 | -1000 … 1000 | m/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `velocity_z` | Velocidade inicial Z | número | Início | 0 | -1000 … 1000 | m/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `angular_x` | Giro inicial X · rad/s | número | Início | 0 | -1000 … 1000 | rad/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `angular_y` | Giro inicial Y · rad/s | número | Início | 0 | -1000 … 1000 | rad/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `angular_z` | Giro inicial Z · rad/s | número | Início | 0 | -1000 … 1000 | rad/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `linear_damping` | Amortecimento linear | número | Amortecimento | 0.05 | 0 … 10 |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `angular_damping` | Amortecimento angular | número | Amortecimento | 0.05 | 0 … 10 |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `gravity_factor` | Multiplicador da gravidade | número | Amortecimento | 1 | -100 … 100 |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `max_linear_velocity` | Limite linear | número | Simulação | 500 | 0.001 … 100000 | m/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `max_angular_velocity` | Limite angular | número | Simulação | 47.12389 | 0.001 … 100000 | rad/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `solver_velocity_steps` | Iterações de velocidade | número | Simulação | 0 | 0 … 255 |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `sensor` | Sensor sem resposta | booleano | Corpo | falso | verdadeiro \| falso |  | runtime/scene_physics.cpp → Jolt | corpo físico | não | não | não |
| `allow_sleep` | Permitir repouso | booleano | Corpo | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `freeze_position_x` | Travar posição X | booleano | Restrições | falso | verdadeiro \| falso |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `freeze_position_y` | Travar posição Y | booleano | Restrições | falso | verdadeiro \| falso |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `freeze_position_z` | Travar posição Z | booleano | Restrições | falso | verdadeiro \| falso |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `freeze_rotation_x` | Travar rotação X | booleano | Restrições | falso | verdadeiro \| falso |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `freeze_rotation_y` | Travar rotação Y | booleano | Restrições | falso | verdadeiro \| falso |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `freeze_rotation_z` | Travar rotação Z | booleano | Restrições | falso | verdadeiro \| falso |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `continuous_collision` | Colisão contínua | booleano | Simulação | falso | verdadeiro \| falso |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não | não |
| `motion` | Movimento | enumeração | Corpo | Estático | Estático \| Cinemático \| Dinâmico |  | runtime/scene_physics.cpp → Jolt | corpo físico | não | não | não |
| `friction_combine` | Combinar atrito | enumeração | Material | Padrão do motor | Padrão do motor \| Média \| Mínimo \| Multiplicar \| Máximo |  | runtime/scene_physics.cpp → Jolt | corpo físico | não | não | não |
| `restitution_combine` | Combinar restituição | enumeração | Material | Padrão do motor | Padrão do motor \| Média \| Mínimo \| Multiplicar \| Máximo |  | runtime/scene_physics.cpp → Jolt | corpo físico | não | não | não |

## Personagem · `astra.physics.character` v4

Locomoção com cápsula. **Consumidor:** runtime/scene_physics.cpp → CharacterVirtual. **Invalida:** forma física, corpo físico.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-CharacterController.html).

**Durante Play:** estrutura não alterável; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Incompatível | `astra.physics.body` | Incompatível com corpo físico |
| Incompatível | `astra.physics.collider` | O personagem traz a própria cápsula; remova o Colisor 3D |
| Incompatível | `astra.physics.dynamic_motor` | Escolha Character ou motor de corpo dinâmico |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `radius` | Raio m | número | Cápsula | 0.45 | 0.01 … 10 | m | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não | não |
| `half_height` | Meia altura do cilindro m | número | Cápsula | 0.55 | 0.01 … 10 | m | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não | não |
| `eye_height` | Altura dos olhos m | número | Cápsula | 1.65 | 0.02 … 20 | m | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não | não |
| `speed` | Velocidade m/s | número | Locomoção | 8 | 0.01 … 100 | m/s | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não | não |
| `slope_degrees` | Inclinação máxima graus | número | Locomoção | 45 | 1 … 89 | ° | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não | não |
| `jump_speed` | Velocidade do salto m/s | número | Locomoção | 5 | 0 … 100 | m/s | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não | não |
| `step_height` | Altura do degrau | número | Chão | 0.4 | 0 … 10 | m | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não | não |
| `floor_snap_length` | Aderência ao chão | número | Chão | 0.5 | 0 … 10 | m | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não | não |
| `gravity` | Gravidade | número | Locomoção | 9.81 | 0 … 1000 | m/s² | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não | não |
| `inherit_platform_horizontal` | Impulso ao sair | booleano | Chão | falso | verdadeiro \| falso |  | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não | não |

## Colisor 3D · `astra.physics.collider` v8

Volume de contato. **Consumidor:** runtime/scene_physics.cpp → forma do Jolt. **Invalida:** forma física.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-BoxCollider.html).

**Durante Play:** estrutura não alterável; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Incompatível | `astra.physics.character` | O personagem já possui cápsula própria |

**Recursos endereçados**

| Binding | Rótulo | Tipo de recurso | Herda | Ausência declarada | ID por elemento |
|---|---|---|---|---|---|
| `collision_mesh` | Malha de colisão | mesh | sim | não | não |

**Propriedades**

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `half_x` | Meia extensão X | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `half_y` | Meia extensão Y | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `half_z` | Meia extensão Z | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `radius` | Raio | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `half_height` | Meia altura cilíndrica | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `center_x` | Centro X | número | Pose | 0 | -10000000 … 10000000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `center_y` | Centro Y | número | Pose | 0 | -10000000 … 10000000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `center_z` | Centro Z | número | Pose | 0 | -10000000 … 10000000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `rotation_x` | Rotação local X | número | Pose | 0 | -10000000 … 10000000 | ° | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `rotation_y` | Rotação local Y | número | Pose | 0 | -10000000 … 10000000 | ° | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `rotation_z` | Rotação local Z | número | Pose | 0 | -10000000 … 10000000 | ° | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `hull_tolerance` | Tolerância do casco | número | Cozimento | 0.001 | 0.00001 … 1 | u | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `active_edge_angle` | Ângulo de aresta ativa | número | Cozimento | 5 | 0 … 90 | ° | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `enabled` | Ativo | booleano |  | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics.cpp → forma do Jolt | forma física | não | não | não |
| `convex` | Convexo | booleano | Forma | falso | verdadeiro \| falso |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `weld_vertices` | Soldar vértices iguais | booleano | Cozimento | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `optimize_cooking` | Otimizar para o jogo | booleano | Cozimento | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `mesh_local_pose` | Pose local da malha | booleano | Forma | falso | verdadeiro \| falso |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não | não |
| `shape` | Forma | enumeração | Forma | Caixa | Caixa \| Esfera \| Cápsula \| Malha \| Cilindro |  | runtime/scene_physics.cpp → forma do Jolt | forma física | não | não | não |
| `owner` | Corpo proprietário | referência | Vínculo | Neste objeto | astra.physics.body · neste objeto ou ancestral |  | runtime/scene_physics.cpp → forma do Jolt | forma física | não | não | não |

**Eventos em Play**

| Evento | Rótulo | Payload | Quando |
|---|---|---|---|
| `trigger_enter` | Sensor: entrou | other: objeto | Outro corpo começou a sobrepor este sensor |
| `trigger_exit` | Sensor: saiu | other: objeto | Outro corpo deixou de sobrepor este sensor |
| `collision_enter` | Colisão: começou | other: objeto | Contato sólido começou; entregue aos dois objetos |
| `collision_exit` | Colisão: terminou | other: objeto | Contato sólido terminou; entregue aos dois objetos |

## Junta · `astra.physics.joint` v2

Nove mecanismos Jolt com limites, referenciais e motores. **Consumidor:** runtime/scene_physics.cpp → constraint do Jolt. **Invalida:** corpo físico.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_generic6dofjoint3d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Requer | `astra.physics.body` | Adicione Corpo físico a este objeto |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `anchor_a_x` | Âncora A · X | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não | não |
| `anchor_a_y` | Âncora A · Y | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não | não |
| `anchor_a_z` | Âncora A · Z | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não | não |
| `anchor_b_x` | Âncora B · X | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não | não |
| `anchor_b_y` | Âncora B · Y | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não | não |
| `anchor_b_z` | Âncora B · Z | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não | não |
| `axis_a_x` | Eixo A · X | número | Eixos | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `axis_a_y` | Eixo A · Y | número | Eixos | 1 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `axis_a_z` | Eixo A · Z | número | Eixos | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `axis_b_x` | Eixo B · X | número | Eixos | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `axis_b_y` | Eixo B · Y | número | Eixos | 1 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `axis_b_z` | Eixo B · Z | número | Eixos | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `limit_min` | Limite mínimo | número | Movimento | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `limit_max` | Limite máximo | número | Movimento | 1 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `motor_velocity` | Velocidade do motor | número | Motor | 0 | -1000 … 1000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `motor_position` | Alvo do motor | número | Motor | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `motor_force` | Força / torque máximo | número | Motor | 100 | 0 … 1000000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `spring_frequency` | Frequência · Hz | número | Motor | 2 | 0.001 … 1000 | Hz | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `spring_damping` | Amortecimento da mola | número | Motor | 1 | 0 … 10 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `normal_a_x` | Plano A · X | número | Eixos | 1 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `normal_a_y` | Plano A · Y | número | Eixos | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `normal_a_z` | Plano A · Z | número | Eixos | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `normal_b_x` | Plano B · X | número | Eixos | 1 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `normal_b_y` | Plano B · Y | número | Eixos | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `normal_b_z` | Plano B · Z | número | Eixos | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `swing_y` | Cone · semiângulo Y | número | Rotação | 45 | 0 … 180 | ° | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `swing_z` | Cone · semiângulo Z | número | Rotação | 45 | 0 … 180 | ° | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `twist_min` | Torção mínima | número | Rotação | -45 | -180 … 180 | ° | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `twist_max` | Torção máxima | número | Rotação | 45 | -180 … 180 | ° | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_x_minimum` | Limite mínimo | número | Translação X | -1 | -100000 … 100000 | u | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_x_maximum` | Limite máximo | número | Translação X | 1 | -100000 … 100000 | u | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_x_friction` | Atrito máximo | número | Translação X | 0 | 0 … 1000000 | N | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_x_velocity` | Velocidade alvo | número | Translação X | 0 | -1000 … 1000 | u/s | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_x_position` | Posição alvo | número | Translação X | 0 | -100000 … 100000 | u | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_x_force` | Força máxima | número | Translação X | 100 | 0 … 1000000 | N | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_x_frequency` | Frequência | número | Translação X | 2 | 0.001 … 1000 | Hz | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_x_damping` | Amortecimento | número | Translação X | 1 | 0 … 10 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_y_minimum` | Limite mínimo | número | Translação Y | -1 | -100000 … 100000 | u | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_y_maximum` | Limite máximo | número | Translação Y | 1 | -100000 … 100000 | u | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_y_friction` | Atrito máximo | número | Translação Y | 0 | 0 … 1000000 | N | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_y_velocity` | Velocidade alvo | número | Translação Y | 0 | -1000 … 1000 | u/s | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_y_position` | Posição alvo | número | Translação Y | 0 | -100000 … 100000 | u | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_y_force` | Força máxima | número | Translação Y | 100 | 0 … 1000000 | N | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_y_frequency` | Frequência | número | Translação Y | 2 | 0.001 … 1000 | Hz | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_y_damping` | Amortecimento | número | Translação Y | 1 | 0 … 10 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_z_minimum` | Limite mínimo | número | Translação Z | -1 | -100000 … 100000 | u | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_z_maximum` | Limite máximo | número | Translação Z | 1 | -100000 … 100000 | u | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_z_friction` | Atrito máximo | número | Translação Z | 0 | 0 … 1000000 | N | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_z_velocity` | Velocidade alvo | número | Translação Z | 0 | -1000 … 1000 | u/s | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_z_position` | Posição alvo | número | Translação Z | 0 | -100000 … 100000 | u | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_z_force` | Força máxima | número | Translação Z | 100 | 0 … 1000000 | N | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_z_frequency` | Frequência | número | Translação Z | 2 | 0.001 … 1000 | Hz | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_z_damping` | Amortecimento | número | Translação Z | 1 | 0 … 10 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_x_minimum` | Limite mínimo | número | Rotação X | -1 | -180 … 180 | ° | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_x_maximum` | Limite máximo | número | Rotação X | 1 | -180 … 180 | ° | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_x_friction` | Atrito máximo | número | Rotação X | 0 | 0 … 1000000 | Nm | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_x_velocity` | Velocidade alvo | número | Rotação X | 0 | -1000 … 1000 | °/s | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_x_position` | Posição alvo | número | Rotação X | 0 | -180 … 180 | ° | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_x_force` | Força máxima | número | Rotação X | 100 | 0 … 1000000 | Nm | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_x_frequency` | Frequência | número | Rotação X | 2 | 0.001 … 1000 | Hz | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_x_damping` | Amortecimento | número | Rotação X | 1 | 0 … 10 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_y_minimum` | Limite mínimo | número | Rotação Y | -1 | -180 … 180 | ° | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_y_maximum` | Limite máximo | número | Rotação Y | 1 | -180 … 180 | ° | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_y_friction` | Atrito máximo | número | Rotação Y | 0 | 0 … 1000000 | Nm | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_y_velocity` | Velocidade alvo | número | Rotação Y | 0 | -1000 … 1000 | °/s | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_y_position` | Posição alvo | número | Rotação Y | 0 | -180 … 180 | ° | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_y_force` | Força máxima | número | Rotação Y | 100 | 0 … 1000000 | Nm | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_y_frequency` | Frequência | número | Rotação Y | 2 | 0.001 … 1000 | Hz | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_y_damping` | Amortecimento | número | Rotação Y | 1 | 0 … 10 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_z_minimum` | Limite mínimo | número | Rotação Z | -1 | -180 … 180 | ° | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_z_maximum` | Limite máximo | número | Rotação Z | 1 | -180 … 180 | ° | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_z_friction` | Atrito máximo | número | Rotação Z | 0 | 0 … 1000000 | Nm | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_z_velocity` | Velocidade alvo | número | Rotação Z | 0 | -1000 … 1000 | °/s | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_z_position` | Posição alvo | número | Rotação Z | 0 | -180 … 180 | ° | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_z_force` | Força máxima | número | Rotação Z | 100 | 0 … 1000000 | Nm | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_z_frequency` | Frequência | número | Rotação Z | 2 | 0.001 … 1000 | Hz | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_z_damping` | Amortecimento | número | Rotação Z | 1 | 0 … 10 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `enabled` | Ativa | booleano |  | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não | não |
| `kind` | Tipo | enumeração |  | Distância | Ponto \| Dobradiça \| Deslizante \| Distância \| Fixa \| Cone \| Swing / Twist \| Configurável 6DOF \| Mola |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não | não |
| `motor` | Motor | enumeração | Motor | Desligado | Desligado \| Velocidade \| Posição \| Posição e velocidade |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_x_motion` | Movimento | enumeração | Translação X | Travado | Travado \| Limitado \| Livre |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_x_motor` | Motor | enumeração | Translação X | Desligado | Desligado \| Velocidade \| Posição \| Posição e velocidade |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_y_motion` | Movimento | enumeração | Translação Y | Travado | Travado \| Limitado \| Livre |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_y_motor` | Motor | enumeração | Translação Y | Desligado | Desligado \| Velocidade \| Posição \| Posição e velocidade |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_z_motion` | Movimento | enumeração | Translação Z | Travado | Travado \| Limitado \| Livre |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `linear_z_motor` | Motor | enumeração | Translação Z | Desligado | Desligado \| Velocidade \| Posição \| Posição e velocidade |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_x_motion` | Movimento | enumeração | Rotação X | Travado | Travado \| Limitado \| Livre |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_x_motor` | Motor | enumeração | Rotação X | Desligado | Desligado \| Velocidade \| Posição \| Posição e velocidade |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_y_motion` | Movimento | enumeração | Rotação Y | Travado | Travado \| Limitado \| Livre |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_y_motor` | Motor | enumeração | Rotação Y | Desligado | Desligado \| Velocidade \| Posição \| Posição e velocidade |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_z_motion` | Movimento | enumeração | Rotação Z | Travado | Travado \| Limitado \| Livre |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `angular_z_motor` | Motor | enumeração | Rotação Z | Desligado | Desligado \| Velocidade \| Posição \| Posição e velocidade |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não | não |
| `connected_body` | Conectar corpo | referência | Âncoras | Escolher corpo | astra.physics.body · outro objeto |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não | não |

## Motor dinâmico · `astra.physics.dynamic_motor` v2

Locomoção por força e salto com apoio sobre Corpo físico dinâmico. **Consumidor:** runtime/scene_dynamic_motor.cpp → forças/impulsos antes de cada passo Jolt. **Invalida:** corpo físico.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_rigidbody3d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Requer | `astra.physics.body` | Adicione Corpo físico dinâmico ao receptor |
| Incompatível | `astra.physics.character` | Incompatível com personagem cápsula |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `speed` | Velocidade | número | Locomoção | 6 | 0 … 100 | m/s | runtime/scene_dynamic_motor.cpp | corpo físico | não | não | não |
| `acceleration` | Aceleração | número | Locomoção | 24 | 0 … 1000 | m/s² | runtime/scene_dynamic_motor.cpp | corpo físico | não | não | não |
| `braking` | Frenagem | número | Locomoção | 36 | 0 … 1000 | m/s² | runtime/scene_dynamic_motor.cpp | corpo físico | não | não | não |
| `air_control` | Controle no ar | número | Locomoção | 0.25 | 0 … 1 |  | runtime/scene_dynamic_motor.cpp | corpo físico | não | não | não |
| `jump_speed` | Velocidade do salto | número | Locomoção | 6 | 0 … 100 | m/s | runtime/scene_dynamic_motor.cpp | corpo físico | não | não | não |
| `probe_height` | Centro até os pés | número | Chão | 1 | 0.01 … 100 | m | runtime/scene_dynamic_motor.cpp | corpo físico | sim | não | não |
| `probe_distance` | Alcance abaixo dos pés | número | Chão | 0.15 | 0.001 … 2 | m | runtime/scene_dynamic_motor.cpp | corpo físico | não | não | não |
| `support_radius` | Raio de sondagem | número | Chão | 0.3 | 0 … 10 | m | runtime/scene_dynamic_motor.cpp | corpo físico | sim | não | não |
| `max_slope` | Rampa máxima | número | Chão | 50 | 0 … 89 | ° | runtime/scene_dynamic_motor.cpp | corpo físico | não | não | não |
| `enabled` | Habilitado | booleano | Locomoção | verdadeiro | verdadeiro \| falso |  | runtime/scene_dynamic_motor.cpp | corpo físico | não | não | não |
| `inherit_platform_velocity` | Acompanhar plataforma | booleano | Chão | verdadeiro | verdadeiro \| falso |  | runtime/scene_dynamic_motor.cpp | corpo físico | não | não | não |
| `automatic_support` | Apoio pela colisão | booleano | Chão | verdadeiro | verdadeiro \| falso |  | runtime/scene_dynamic_motor.cpp | corpo físico | não | não | não |

## Animação · `astra.animation` v4

Clipes tocados e misturados no Play. **Consumidor:** runtime/scene_animation.cpp → pose local dos nós da instância. **Capacidade:** `animation.clip` (implementada). **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Animation.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

**Coleções com identidade persistente**

| Coleção | Elementos iniciais | Próximo ID |
|---|---|---|
| `clips` | 0 | 1 |

Apply seletivo exige identidade, ordem e fronteira de alocação compatíveis.

**Recursos endereçados**

| Binding | Rótulo | Tipo de recurso | Herda | Ausência declarada | ID por elemento |
|---|---|---|---|---|---|
| `clip` | Clipe padrão | animation_clip | não | não | não |
| `clips` | Clipe | animation_clip | não | não | sim |

**Propriedades**

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `speed` | Velocidade | número | Reprodução | 1 | -10 … 10 | x | runtime/scene_animation.cpp → pose local dos nós da instância | pose e bounds | não | não | não |
| `enabled` | Ativa | booleano | Reprodução | verdadeiro | verdadeiro \| falso |  | runtime/scene_animation.cpp → pose local dos nós da instância | pose e bounds | não | não | não |
| `play_automatically` | Tocar ao iniciar | booleano | Reprodução | verdadeiro | verdadeiro \| falso |  | runtime/scene_animation.cpp → pose local dos nós da instância | pose e bounds | não | não | não |
| `wrap_mode` | Repetição | enumeração | Reprodução | Repetir | Uma vez \| Repetir \| Vai e volta \| Segurar no fim |  | runtime/scene_animation.cpp → pose local dos nós da instância | pose e bounds | não | não | não |
| `clip_count` | Quantidade de clipes | enumeração | Clipes | 0 | 0 \| 1 \| 2 \| 3 \| 4 \| 5 \| 6 \| 7 \| 8 \| 9 \| 10 \| 11 \| 12 \| 13 \| 14 \| 15 \| 16 \| 17 \| 18 \| 19 \| 20 \| 21 \| 22 \| 23 \| 24 \| 25 \| 26 \| 27 \| 28 \| 29 \| 30 \| 31 \| 32 |  | runtime/scene_animation.cpp → pose local dos nós da instância | pose e bounds | não | não | não |

## Campo de gravidade 2D · `astra.physics2d.field.gravity` v1

Gravidade XY em área sobre corpos dinâmicos 2D. **Consumidor:** runtime/scene_physics2d_fields.inl → Box2D. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_area2d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `half_x` | Meia extensão X | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | sim | não | não |
| `half_y` | Meia extensão Y | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | sim | não | não |
| `radius` | Raio | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | sim | não | não |
| `offset_x` | Centro X | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `offset_y` | Centro Y | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `vector_x` | Vetor X | número | Efeito | 0 | -10000 … 10000 | m/s² | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `vector_y` | Vetor Y | número | Efeito | -9.81 | -10000 … 10000 | m/s² | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `enabled` | Ativo | booleano | Efeito | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `wake_bodies` | Acordar corpos | booleano | Alcance | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `replace_world_gravity` | Substituir gravidade do mundo | booleano | Efeito | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `shape` | Forma | enumeração | Volume | Retângulo | Retângulo \| Círculo |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `falloff` | Queda de influência | enumeração | Alcance | Uniforme | Uniforme \| Linear \| Suave |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `affected_layer` | Camada afetada | enumeração | Alcance | Todas | Todas \| Camada 0 \| Camada 1 \| Camada 2 \| Camada 3 \| Camada 4 \| Camada 5 \| Camada 6 \| Camada 7 \| Camada 8 \| Camada 9 \| Camada 10 \| Camada 11 \| Camada 12 \| Camada 13 \| Camada 14 \| Camada 15 \| Camada 16 \| Camada 17 \| Camada 18 \| Camada 19 \| Camada 20 \| Camada 21 \| Camada 22 \| Camada 23 \| Camada 24 \| Camada 25 \| Camada 26 \| Camada 27 \| Camada 28 \| Camada 29 \| Camada 30 \| Camada 31 |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |

## Campo de vento 2D · `astra.physics2d.field.wind` v1

Acoplamento XY à velocidade do ar com massa real. **Consumidor:** runtime/scene_physics2d_fields.inl → Box2D. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_area2d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `half_x` | Meia extensão X | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | sim | não | não |
| `half_y` | Meia extensão Y | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | sim | não | não |
| `radius` | Raio | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | sim | não | não |
| `offset_x` | Centro X | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `offset_y` | Centro Y | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `vector_x` | Vetor X | número | Efeito | 5 | -10000 … 10000 | m/s | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `vector_y` | Vetor Y | número | Efeito | 0 | -10000 … 10000 | m/s | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `coefficient` | Acoplamento | número | Efeito | 1 | 0 … 10000 | kg/s | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `enabled` | Ativo | booleano | Efeito | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `wake_bodies` | Acordar corpos | booleano | Alcance | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `shape` | Forma | enumeração | Volume | Retângulo | Retângulo \| Círculo |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `falloff` | Queda de influência | enumeração | Alcance | Uniforme | Uniforme \| Linear \| Suave |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `affected_layer` | Camada afetada | enumeração | Alcance | Todas | Todas \| Camada 0 \| Camada 1 \| Camada 2 \| Camada 3 \| Camada 4 \| Camada 5 \| Camada 6 \| Camada 7 \| Camada 8 \| Camada 9 \| Camada 10 \| Camada 11 \| Camada 12 \| Camada 13 \| Camada 14 \| Camada 15 \| Camada 16 \| Camada 17 \| Camada 18 \| Camada 19 \| Camada 20 \| Camada 21 \| Camada 22 \| Camada 23 \| Camada 24 \| Camada 25 \| Camada 26 \| Camada 27 \| Camada 28 \| Camada 29 \| Camada 30 \| Camada 31 |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |

## Campo de arrasto 2D · `astra.physics2d.field.drag` v1

Amortecimento linear XY e angular de Body2D. **Consumidor:** runtime/scene_physics2d_fields.inl → Box2D. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_area2d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `half_x` | Meia extensão X | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | sim | não | não |
| `half_y` | Meia extensão Y | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | sim | não | não |
| `radius` | Raio | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | sim | não | não |
| `offset_x` | Centro X | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `offset_y` | Centro Y | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `linear_drag` | Arrasto linear | número | Efeito | 1 | 0 … 1000 | 1/s | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `angular_drag` | Arrasto angular | número | Efeito | 1 | 0 … 1000 | 1/s | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `enabled` | Ativo | booleano | Efeito | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `wake_bodies` | Acordar corpos | booleano | Alcance | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `shape` | Forma | enumeração | Volume | Retângulo | Retângulo \| Círculo |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `falloff` | Queda de influência | enumeração | Alcance | Uniforme | Uniforme \| Linear \| Suave |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `affected_layer` | Camada afetada | enumeração | Alcance | Todas | Todas \| Camada 0 \| Camada 1 \| Camada 2 \| Camada 3 \| Camada 4 \| Camada 5 \| Camada 6 \| Camada 7 \| Camada 8 \| Camada 9 \| Camada 10 \| Camada 11 \| Camada 12 \| Camada 13 \| Camada 14 \| Camada 15 \| Camada 16 \| Camada 17 \| Camada 18 \| Camada 19 \| Camada 20 \| Camada 21 \| Camada 22 \| Camada 23 \| Camada 24 \| Camada 25 \| Camada 26 \| Camada 27 \| Camada 28 \| Camada 29 \| Camada 30 \| Camada 31 |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |

## Campo radial 2D · `astra.physics2d.field.radial` v1

Atração, repulsão e vórtice no plano XY. **Consumidor:** runtime/scene_physics2d_fields.inl → Box2D. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_area2d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `half_x` | Meia extensão X | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | sim | não | não |
| `half_y` | Meia extensão Y | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | sim | não | não |
| `radius` | Raio | número | Volume | 3 | 0.001 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | sim | não | não |
| `offset_x` | Centro X | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `offset_y` | Centro Y | número | Volume | 0 | -10000 … 10000 | m | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `acceleration` | Aceleração radial | número | Efeito | -9.81 | -10000 … 10000 | m/s² | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `tangential_acceleration` | Aceleração tangencial | número | Efeito | 0 | -10000 … 10000 | m/s² | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `enabled` | Ativo | booleano | Efeito | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `wake_bodies` | Acordar corpos | booleano | Alcance | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `shape` | Forma | enumeração | Volume | Retângulo | Retângulo \| Círculo |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `falloff` | Queda de influência | enumeração | Alcance | Uniforme | Uniforme \| Linear \| Suave |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |
| `affected_layer` | Camada afetada | enumeração | Alcance | Todas | Todas \| Camada 0 \| Camada 1 \| Camada 2 \| Camada 3 \| Camada 4 \| Camada 5 \| Camada 6 \| Camada 7 \| Camada 8 \| Camada 9 \| Camada 10 \| Camada 11 \| Camada 12 \| Camada 13 \| Camada 14 \| Camada 15 \| Camada 16 \| Camada 17 \| Camada 18 \| Camada 19 \| Camada 20 \| Camada 21 \| Camada 22 \| Camada 23 \| Camada 24 \| Camada 25 \| Camada 26 \| Camada 27 \| Camada 28 \| Camada 29 \| Camada 30 \| Camada 31 |  | runtime/scene_physics2d_fields.inl → Box2D | nada | não | não | não |

## Conexão física 2D · `astra.physics2d.event_connection` v1

Evento real de sensor/contato 2D altera ativação do receptor. **Consumidor:** runtime/scene_physics_connections.h → GameWorld::setActive antes do callback. **Invalida:** corpo físico.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_area2d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Requer | `astra.physics2d.body` | Adicione Body2D ao objeto |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `enabled` | Ativa | booleano | Conexão | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics_connections.h → GameWorld::setActive antes do callback | corpo físico | não | não | não |
| `event` | Evento | enumeração | Conexão | Entrada sensor | Entrada sensor \| Perm. sensor \| Saída sensor \| Entrada contato \| Perm. contato \| Saída contato |  | runtime/scene_physics_connections.h → GameWorld::setActive antes do callback | corpo físico | não | não | não |
| `action` | Ação | enumeração | Conexão | Desconectado | Desconectado \| Ativar objeto \| Desativar objeto \| Alternar objeto |  | runtime/scene_physics_connections.h → GameWorld::setActive antes do callback | corpo físico | não | não | não |
| `receiver` | Receptor | referência | Conexão | Escolher objeto | qualquer objeto |  | runtime/scene_physics_connections.h → GameWorld::setActive antes do callback | corpo físico | sim | não | não |
| `other_filter` | Outro objeto | referência | Filtro | Qualquer objeto | qualquer objeto |  | runtime/scene_physics_connections.h → GameWorld::setActive antes do callback | corpo físico | sim | não | não |

## Força constante 2D · `astra.physics2d.constant-force` v1

Força XY e torque contínuos sobre Body2D dinâmico. **Consumidor:** runtime/scene_physics2d.cpp → ApplyForce/ApplyTorque por passo. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/ConstantForce2D.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Requer | `astra.physics2d.body` | Adicione Body2D ao objeto |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `force_x` | Força X | número | Propulsão | 0 | -1000000 … 1000000 | N | runtime/scene_physics2d.cpp → ApplyForce/ApplyTorque por passo | nada | não | não | não |
| `force_y` | Força Y | número | Propulsão | 0 | -1000000 … 1000000 | N | runtime/scene_physics2d.cpp → ApplyForce/ApplyTorque por passo | nada | não | não | não |
| `relative_force_x` | Força local X | número | Propulsão | 0 | -1000000 … 1000000 | N | runtime/scene_physics2d.cpp → ApplyForce/ApplyTorque por passo | nada | não | não | não |
| `relative_force_y` | Força local Y | número | Propulsão | 0 | -1000000 … 1000000 | N | runtime/scene_physics2d.cpp → ApplyForce/ApplyTorque por passo | nada | não | não | não |
| `torque` | Torque | número | Propulsão | 0 | -1000000 … 1000000 | N m | runtime/scene_physics2d.cpp → ApplyForce/ApplyTorque por passo | nada | não | não | não |
| `enabled` | Ativo | booleano | Propulsão | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics2d.cpp → ApplyForce/ApplyTorque por passo | nada | não | não | não |

## Junta 2D · `astra.physics2d.joint` v1

Fixed/Weld, Revolute, Prismatic ou Distance reais. **Consumidor:** runtime/scene_physics2d.cpp → Box2D joints. **Invalida:** corpo físico.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Joint2D.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Requer | `astra.physics2d.body` | Adicione Body2D ao objeto |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `anchor_a_x` | Anchor A X | número | Anchors | 0 | -1000 … 1000 | m | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `anchor_a_y` | Anchor A Y | número | Anchors | 0 | -1000 … 1000 | m | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `anchor_b_x` | Anchor B X | número | Anchors | 0 | -1000 … 1000 | m | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `anchor_b_y` | Anchor B Y | número | Anchors | 0 | -1000 … 1000 | m | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `reference_angle_degrees` | Ângulo referência | número | Ajustes | 0 | -180 … 180 | graus | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `axis_angle_degrees` | Eixo local A | número | Ajustes | 0 | -180 … 180 | graus | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `length` | Comprimento repouso | número | Ajustes | 1 | 0.005 … 1000 | m | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `min_length` | Comprimento mínimo | número | Ajustes | 0.005 | 0.005 … 1000 | m | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `max_length` | Comprimento máximo | número | Ajustes | 100 | 0.005 … 1000 | m | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `lower_limit` | Limite inferior | número | Ajustes | -1 | -178 … 178 |  | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `upper_limit` | Limite superior | número | Ajustes | 1 | -178 … 178 |  | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `motor_speed` | Velocidade motor | número | Ajustes | 1 | -1000 … 1000 | m/s | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `motor_angular_speed_degrees` | Motor angular | número | Ajustes | 90 | -36000 … 36000 | graus/s | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `max_motor_force` | Força máxima motor | número | Ajustes | 100 | 0 … 1000000 | N | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `max_motor_torque` | Torque máximo motor | número | Ajustes | 100 | 0 … 1000000 | N m | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `spring_hertz` | Frequência mola | número | Ajustes | 5 | 0 … 120 | Hz | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `spring_damping` | Razão amortecimento | número | Ajustes | 0.7 | 0 … 10 |  | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `spring_target_translation` | Translação alvo | número | Ajustes | 0 | -1000 … 1000 | m | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `spring_target_angle_degrees` | Ângulo alvo | número | Ajustes | 0 | -180 … 180 | graus | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `linear_hertz` | Mola linear weld | número | Ajustes | 0 | 0 … 120 | Hz | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `angular_hertz` | Mola angular weld | número | Ajustes | 0 | 0 … 120 | Hz | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `linear_damping_ratio` | Amortecimento linear weld | número | Ajustes | 1 | 0 … 10 |  | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `angular_damping_ratio` | Amortecimento angular weld | número | Ajustes | 1 | 0 … 10 |  | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `enabled` | Ativo | booleano | Conexão | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `world_anchor` | Conectar ao mundo | booleano | Conexão | falso | verdadeiro \| falso |  | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `collide_connected` | Colidir conectados | booleano | Conexão | falso | verdadeiro \| falso |  | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `limit_enabled` | Limites | booleano | Ajustes | falso | verdadeiro \| falso |  | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `motor_enabled` | Motor | booleano | Ajustes | falso | verdadeiro \| falso |  | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `spring_enabled` | Mola | booleano | Ajustes | falso | verdadeiro \| falso |  | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |
| `kind` | Modo | enumeração | Conexão | Fixed / Weld | Fixed / Weld \| Revolute \| Prismatic \| Distance |  | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | não | não | não |
| `target` | Corpo conectado | referência | Conexão | Escolher Body2D | astra.physics2d.body · outro objeto |  | runtime/scene_physics2d.cpp → Box2D joints | corpo físico | sim | não | não |

## Corpo 2D · `astra.physics2d.body` v1

Corpo XY independente de física 3D. **Consumidor:** runtime/scene_physics2d.cpp → Box2D 3.1.1. **Invalida:** corpo físico.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_rigidbody2d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Requer | `astra.physics2d.collider` | Adicione uma forma 2D ao corpo |
| Incompatível | `astra.physics.body` | Corpos 2D e 3D não compartilham o mesmo objeto |
| Incompatível | `astra.physics.character` | Personagem 3D possui a pose deste objeto |
| Incompatível | `astra.physics.collider` | Colisores 2D e 3D usam mundos separados |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `mass` | Massa | número | Corpo | 1 | 0.001 … 100000 | kg | runtime/scene_physics2d.cpp → Box2D 3.1.1 | corpo físico | sim | não | não |
| `gravity_scale` | Escala gravidade | número | Corpo | 1 | -100 … 100 |  | runtime/scene_physics2d.cpp → Box2D 3.1.1 | corpo físico | sim | não | não |
| `velocity_x` | Velocidade X | número | Movimento | 0 | -10000 … 10000 | m/s | runtime/scene_physics2d.cpp → Box2D 3.1.1 | corpo físico | sim | não | não |
| `velocity_y` | Velocidade Y | número | Movimento | 0 | -10000 … 10000 | m/s | runtime/scene_physics2d.cpp → Box2D 3.1.1 | corpo físico | sim | não | não |
| `angular_velocity_degrees` | Velocidade angular | número | Movimento | 0 | -36000 … 36000 | graus/s | runtime/scene_physics2d.cpp → Box2D 3.1.1 | corpo físico | sim | não | não |
| `linear_damping` | Arrasto linear | número | Movimento | 0.05 | 0 … 100 |  | runtime/scene_physics2d.cpp → Box2D 3.1.1 | corpo físico | sim | não | não |
| `angular_damping` | Arrasto angular | número | Movimento | 0.05 | 0 … 100 |  | runtime/scene_physics2d.cpp → Box2D 3.1.1 | corpo físico | sim | não | não |
| `fixed_rotation` | Fixar rotação | booleano | Movimento | falso | verdadeiro \| falso |  | runtime/scene_physics2d.cpp → Box2D 3.1.1 | corpo físico | sim | não | não |
| `allow_sleep` | Permitir repouso | booleano | Movimento | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics2d.cpp → Box2D 3.1.1 | corpo físico | sim | não | não |
| `motion` | Movimento | enumeração | Corpo | Dinâmico | Estático \| Cinemático \| Dinâmico |  | runtime/scene_physics2d.cpp → Box2D 3.1.1 | corpo físico | não | não | não |

## Colisor 2D · `astra.physics2d.collider` v1

Caixa, círculo ou cápsula; sensor real. **Consumidor:** runtime/scene_physics2d.cpp → Box2D 3.1.1. **Invalida:** forma física.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_collisionshape2d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| Relação no objeto | Tipo | Diagnóstico |
|---|---|---|
| Incompatível | `astra.physics.body` | Corpos 2D e 3D não compartilham o mesmo objeto |
| Incompatível | `astra.physics.character` | Personagem 3D possui a pose deste objeto |
| Incompatível | `astra.physics.collider` | Colisores 2D e 3D usam mundos separados |

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `half_x` | Meia largura | número | Forma | 0.5 | 0.005 … 500 | m | runtime/scene_physics2d.cpp → Box2D 3.1.1 | forma física | sim | não | não |
| `half_y` | Meia altura | número | Forma | 0.5 | 0.005 … 500 | m | runtime/scene_physics2d.cpp → Box2D 3.1.1 | forma física | sim | não | não |
| `radius` | Raio | número | Forma | 0.5 | 0.005 … 500 | m | runtime/scene_physics2d.cpp → Box2D 3.1.1 | forma física | sim | não | não |
| `capsule_half_length` | Meia distância centros | número | Forma | 0.5 | 0.005 … 500 | m | runtime/scene_physics2d.cpp → Box2D 3.1.1 | forma física | sim | não | não |
| `offset_x` | Centro X | número | Forma | 0 | -1000 … 1000 | m | runtime/scene_physics2d.cpp → Box2D 3.1.1 | forma física | não | não | não |
| `offset_y` | Centro Y | número | Forma | 0 | -1000 … 1000 | m | runtime/scene_physics2d.cpp → Box2D 3.1.1 | forma física | não | não | não |
| `friction` | Atrito | número | Contato | 0.5 | 0 … 1 |  | runtime/scene_physics2d.cpp → Box2D 3.1.1 | forma física | sim | não | não |
| `restitution` | Restituição | número | Contato | 0 | 0 … 1 |  | runtime/scene_physics2d.cpp → Box2D 3.1.1 | forma física | sim | não | não |
| `sensor` | Sensor sem resposta | booleano | Contato | falso | verdadeiro \| falso |  | runtime/scene_physics2d.cpp → Box2D 3.1.1 | forma física | não | não | não |
| `shape` | Forma | enumeração | Forma | Caixa | Caixa \| Círculo \| Cápsula Y |  | runtime/scene_physics2d.cpp → Box2D 3.1.1 | forma física | não | não | não |

**Eventos em Play**

| Evento | Rótulo | Payload | Quando |
|---|---|---|---|
| `trigger_enter` | Sensor: entrou | other: objeto | Outro corpo começou a sobrepor este sensor |
| `trigger_exit` | Sensor: saiu | other: objeto | Outro corpo deixou de sobrepor este sensor |
| `collision_enter` | Colisão: começou | other: objeto | Contato sólido começou; entregue aos dois colisores |
| `collision_exit` | Colisão: terminou | other: objeto | Contato sólido terminou; entregue aos dois colisores |

## Audio Source · `astra.audio.source` v1

Clipe de projeto com reprodução e espaço acústico. **Consumidor:** runtime/scene_audio.cpp → miniaudio engine/device. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-AudioSource.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

**Recursos endereçados**

| Binding | Rótulo | Tipo de recurso | Herda | Ausência declarada | ID por elemento |
|---|---|---|---|---|---|
| `clip` | Clipe WAV | audio_clip | não | não | não |

**Propriedades**

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `volume` | Volume | número | Som | 1 | 0 … 1 |  | runtime/scene_audio.cpp → miniaudio engine/device | nada | não | não | sim |
| `pitch` | Velocidade / pitch | número | Reprodução | 1 | 0.1 … 4 | × | runtime/scene_audio.cpp → miniaudio engine/device | nada | não | não | sim |
| `pan` | Pan estéreo | número | Espaço | 0 | -1 … 1 |  | runtime/scene_audio.cpp → miniaudio engine/device | nada | sim | não | sim |
| `min_distance` | Distância mínima | número | Espaço | 1 | 0.01 … 100000 | m | runtime/scene_audio.cpp → miniaudio engine/device | nada | sim | não | não |
| `max_distance` | Distância máxima | número | Espaço | 100 | 0.02 … 100001 | m | runtime/scene_audio.cpp → miniaudio engine/device | nada | sim | não | não |
| `rolloff_factor` | Decaimento | número | Espaço | 1 | 0 … 10 |  | runtime/scene_audio.cpp → miniaudio engine/device | nada | sim | não | não |
| `cone_inner` | Cone interno | número | Emissão | 360 | 0 … 360 | ° | runtime/scene_audio.cpp → miniaudio engine/device | nada | sim | não | não |
| `cone_outer` | Cone externo | número | Emissão | 360 | 0 … 360 | ° | runtime/scene_audio.cpp → miniaudio engine/device | nada | sim | não | não |
| `cone_gain` | Ganho fora do cone | número | Emissão | 0 | 0 … 1 |  | runtime/scene_audio.cpp → miniaudio engine/device | nada | sim | não | não |
| `doppler` | Doppler | número | Emissão | 1 | 0 … 4 | × | runtime/scene_audio.cpp → miniaudio engine/device | nada | sim | não | não |
| `enabled` | Ativo | booleano | Som | verdadeiro | verdadeiro \| falso |  | runtime/scene_audio.cpp → miniaudio engine/device | nada | não | não | não |
| `mute` | Silenciar | booleano | Som | falso | verdadeiro \| falso |  | runtime/scene_audio.cpp → miniaudio engine/device | nada | não | não | não |
| `loop` | Repetir | booleano | Reprodução | falso | verdadeiro \| falso |  | runtime/scene_audio.cpp → miniaudio engine/device | nada | não | não | não |
| `playback` | Pedido | enumeração | Reprodução | Tocar | Parar \| Tocar \| Pausar |  | runtime/scene_audio.cpp → miniaudio engine/device | nada | não | não | não |
| `dimension` | Dimensão | enumeração | Espaço | 2D / estéreo | 2D / estéreo \| 3D / espacial |  | runtime/scene_audio.cpp → miniaudio engine/device | nada | não | não | não |
| `rolloff` | Atenuação | enumeração | Espaço | Inverso | Linear \| Inverso \| Exponencial |  | runtime/scene_audio.cpp → miniaudio engine/device | nada | sim | não | não |
| `bus` | Bus | referência | Som | Master | astra.audio.bus |  | runtime/scene_audio.cpp → miniaudio engine/device | nada | não | não | não |

**Métodos em Play**

| Método | Rótulo | Argumentos | Retorno | Efeito |
|---|---|---|---|---|
| `play` | Tocar | — | nada | Recomeça do início |
| `pause` | Pausar | — | nada | Congela o cursor |
| `resume` | Retomar | — | nada | Continua do cursor pausado |
| `stop` | Parar | — | nada | Interrompe e libera a voz |
| `seek` | Posicionar | seconds: número (s) | nada | Move o cursor; aplicado pelo mixer |

## Audio Listener · `astra.audio.listener` v1

Pose e volume de escuta escolhidos por prioridade. **Consumidor:** runtime/scene_audio.cpp → miniaudio listener. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-AudioListener.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `volume` | Volume global | número | Escuta | 1 | 0 … 1 |  | runtime/scene_audio.cpp → miniaudio listener | nada | não | não | sim |
| `priority` | Prioridade | número | Escuta | 0 | 0 … 255 |  | runtime/scene_audio.cpp → miniaudio listener | nada | não | não | não |
| `enabled` | Ativo | booleano | Escuta | verdadeiro | verdadeiro \| falso |  | runtime/scene_audio.cpp → miniaudio listener | nada | não | não | não |

## Audio Bus · `astra.audio.bus` v1

Roteamento de ganho, mute e solo até Master. **Consumidor:** runtime/scene_audio.cpp → voice gain routing. **Invalida:** nada.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/tutorials/audio/audio_buses.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `volume` | Ganho | número | Mixer | 1 | 0 … 1 |  | runtime/scene_audio.cpp → voice gain routing | nada | não | não | sim |
| `enabled` | Ativo | booleano | Mixer | verdadeiro | verdadeiro \| falso |  | runtime/scene_audio.cpp → voice gain routing | nada | não | não | não |
| `mute` | Silenciar | booleano | Mixer | falso | verdadeiro \| falso |  | runtime/scene_audio.cpp → voice gain routing | nada | não | não | não |
| `solo` | Solo | booleano | Mixer | falso | verdadeiro \| falso |  | runtime/scene_audio.cpp → voice gain routing | nada | não | não | não |
| `output` | Saída | referência | Mixer | Master | astra.audio.bus · outro objeto |  | runtime/scene_audio.cpp → voice gain routing | nada | não | não | não |

## Path · `astra.path` v2

Curva Bézier local com pontos persistentes. **Consumidor:** runtime/scene_paths.cpp. **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_path3d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

**Coleções com identidade persistente**

| Coleção | Elementos iniciais | Próximo ID |
|---|---|---|
| `points` | 0 | 1 |

Apply seletivo exige identidade, ordem e fronteira de alocação compatíveis.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `up_x` | Up X | número | Orientação | 0 | -100000 … 100000 |  | runtime/scene_paths.cpp | pose e bounds | não | não | não |
| `up_y` | Up Y | número | Orientação | 1 | -100000 … 100000 |  | runtime/scene_paths.cpp | pose e bounds | não | não | não |
| `up_z` | Up Z | número | Orientação | 0 | -100000 … 100000 |  | runtime/scene_paths.cpp | pose e bounds | não | não | não |
| `closed` | Fechado | booleano | Caminho | falso | verdadeiro \| falso |  | runtime/scene_paths.cpp | pose e bounds | não | não | não |
| `point_position_x` | Posição X | número | Posição |  | -100000 … 100000 | u | runtime/scene_paths.cpp | pose e bounds | não | sim | não |
| `point_position_y` | Posição Y | número | Posição |  | -100000 … 100000 | u | runtime/scene_paths.cpp | pose e bounds | não | sim | não |
| `point_position_z` | Posição Z | número | Posição |  | -100000 … 100000 | u | runtime/scene_paths.cpp | pose e bounds | não | sim | não |
| `point_in_x` | Entrada X | número | Tangentes |  | -100000 … 100000 | u | runtime/scene_paths.cpp | pose e bounds | não | sim | não |
| `point_in_y` | Entrada Y | número | Tangentes |  | -100000 … 100000 | u | runtime/scene_paths.cpp | pose e bounds | não | sim | não |
| `point_in_z` | Entrada Z | número | Tangentes |  | -100000 … 100000 | u | runtime/scene_paths.cpp | pose e bounds | não | sim | não |
| `point_out_x` | Saída X | número | Tangentes |  | -100000 … 100000 | u | runtime/scene_paths.cpp | pose e bounds | não | sim | não |
| `point_out_y` | Saída Y | número | Tangentes |  | -100000 … 100000 | u | runtime/scene_paths.cpp | pose e bounds | não | sim | não |
| `point_out_z` | Saída Z | número | Tangentes |  | -100000 … 100000 | u | runtime/scene_paths.cpp | pose e bounds | não | sim | não |
| `point_roll` | Roll | número | Orientação |  | -3600 … 3600 | ° | runtime/scene_paths.cpp | pose e bounds | não | sim | não |

## Path Follow · `astra.path.follow` v1

Percorre curva em distância mundial e orienta +Z. **Consumidor:** runtime/scene_paths.cpp. **Invalida:** pose e bounds.

**Referência estudada:** [documentação oficial](https://docs.godotengine.org/en/4.5/classes/class_pathfollow3d.html).

**Durante Play:** estrutura em ponto seguro; propriedades em ponto seguro.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot | Tween numérico |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `progress_distance` | Distância inicial | número | Percurso | 0 | 0 … 1000000 | u | runtime/scene_paths.cpp | pose e bounds | sim | não | não |
| `speed` | Velocidade | número | Percurso | 1 | 0 … 100000 | u/s | runtime/scene_paths.cpp | pose e bounds | sim | não | não |
| `duration` | Duração | número | Percurso | 5 | 0.001 … 100000 | s | runtime/scene_paths.cpp | pose e bounds | sim | não | não |
| `offset_x` | Deslocamento lateral | número | Orientação | 0 | -10000 … 10000 | u | runtime/scene_paths.cpp | pose e bounds | sim | não | não |
| `offset_y` | Deslocamento vertical | número | Orientação | 0 | -10000 … 10000 | u | runtime/scene_paths.cpp | pose e bounds | sim | não | não |
| `offset_z` | Deslocamento tangente | número | Orientação | 0 | -10000 … 10000 | u | runtime/scene_paths.cpp | pose e bounds | sim | não | não |
| `enabled` | Ativo | booleano | Execução | verdadeiro | verdadeiro \| falso |  | runtime/scene_paths.cpp | pose e bounds | não | não | não |
| `autoplay` | Iniciar no Play | booleano | Execução | verdadeiro | verdadeiro \| falso |  | runtime/scene_paths.cpp | pose e bounds | não | não | não |
| `loop` | Repetir percurso | booleano | Execução | falso | verdadeiro \| falso |  | runtime/scene_paths.cpp | pose e bounds | não | não | não |
| `backwards` | Sentido inverso | booleano | Execução | falso | verdadeiro \| falso |  | runtime/scene_paths.cpp | pose e bounds | não | não | não |
| `orient` | Orientar +Z | booleano | Orientação | verdadeiro | verdadeiro \| falso |  | runtime/scene_paths.cpp | pose e bounds | não | não | não |
| `mode` | Avanço | enumeração | Percurso | Velocidade mundial | Velocidade mundial \| Duração do percurso |  | runtime/scene_paths.cpp | pose e bounds | não | não | não |
| `target` | Caminho | referência | Percurso | Escolher Path | astra.path · outro objeto |  | runtime/scene_paths.cpp | pose e bounds | não | não | não |

**Métodos em Play**

| Método | Rótulo | Argumentos | Retorno | Efeito |
|---|---|---|---|---|
| `restart` | Reiniciar | — | nada | Volta à distância inicial e avança |
| `stop` | Parar | — | nada | Congela pose e progresso |
| `progress` | Progresso | — | número | Distância percorrida no mundo |
| `playing` | Em movimento | — | booleano | Verdadeiro enquanto avança |

## Capacidades do motor

| Capacidade | Nome | Estado | Onde vive | Limite |
|---|---|---|---|---|
| `render.light.directional` | Sol direcional | implementada | renderer/punctual_lights.h + rhi/shaders/material_shading.glsl | — |
| `render.light.punctual` | Luzes pontuais e spot | implementada | renderer/punctual_lights.h (8 por quadro) | — |
| `render.light.temperature` | Temperatura de cor em kelvin | implementada | scene/light_units.h + runtime/scene_lights.cpp | — |
| `render.light.photometric` | Unidades fotométricas (lux, lumen, candela) | implementada | scene/light_units.h + runtime/scene_lights.cpp | — |
| `render.light.cookie` | Máscara projetada (cookie) | planejada | renderer/punctual_lights.h | Sem amostragem de textura por luz no shader de fragmento |
| `render.light.area` | Luz de área | planejada | renderer/punctual_lights.h | Sem integração de fonte com extensão |
| `render.shadow.directional` | Sombra do sol em cascatas | implementada | renderer/shadow_cascades.cpp + rhi/shaders/shadow_depth.vert | — |
| `render.shadow.punctual` | Sombra de luz pontual ou spot | implementada | renderer/shadow_atlas.h | Atlas em quadtree: spot ocupa um mapa, pontual seis faces |
| `render.ambient.hemispheric` | Ambiente hemisférico céu/chão | implementada | renderer/environment_lighting.h | — |
| `render.ambient.specular` | Reflexo especular do ambiente | implementada | renderer/environment_map.cpp | — |
| `render.gi.lightmap` | Lightmap indireto externo por submalha | limitada pelo aparelho | scene/lightmap_binding.h + rhi/shaders/dirt_road_shading.glsl | RGB linear de irradiância; UV1 autoral; exige bindless. Sem bake, unwrap ou probes |
| `render.probe.irradiance` | Sonda de irradiância por objeto | planejada | renderer/ (volume de sondas) | Sem recurso de sonda nem amostragem por instância |
| `render.probe.reflection` | Sonda de reflexão local | planejada | renderer/environment_map.cpp | Sem captura local, atlas nem blend por volume |
| `render.material.pbr` | Superfície PBR metálico/rugosidade | implementada | rhi/shaders/material_shading.glsl | — |
| `render.material.alpha_mask` | Recorte por alfa, inclusive na sombra | implementada | rhi/shaders/shadow_depth_masked.frag | — |
| `render.material.alpha_blend` | Transparência com mistura | implementada | renderer/map_draw_update.h | — |
| `render.material.double_sided` | Desenho de face dupla | implementada | renderer/map_draw_update.h | — |
| `render.material.uv_transform` | Conjunto de UV e transformação por binding | implementada | rhi/shaders/world_uv.glsl | — |
| `render.material.variants` | Especialização de pipeline por material | limitada pelo aparelho | renderer/rendering_policy.cpp | Drivers móveis podem regredir com muitos pipelines pequenos |
| `render.material.clearcoat` | Camada de verniz e transmissão | planejada | rhi/shaders/material_shading.glsl | Sem variante de BRDF com camada adicional |
| `render.texture.anisotropy` | Filtragem anisotrópica | limitada pelo aparelho | rhi/ (feature samplerAnisotropy) | Depende da GPU expor samplerAnisotropy |
| `render.texture.bindless` | Indexação sem limite de descritor | limitada pelo aparelho | rhi/bindless_registry | Depende de descriptor indexing no backend |
| `render.lod.package` | Níveis de detalhe do pacote de mapa | implementada | renderer/lod_selection.cpp | — |
| `render.lod.group` | Grupo de LOD autoral por objeto | implementada | runtime/lod_groups.h + renderer/lod_dither.glsl | Níveis são objetos do autor ou da convenção _LOD<n>; simplificação automática de malha ainda não existe |
| `render.visibility.hzb` | Oclusão por pirâmide de profundidade | implementada | renderer/hzb_visibility.cpp | — |
| `render.instancing.gpu` | Culling e compactação de desenho em GPU | implementada | renderer/gpu_draw_culling.cpp | — |
| `render.motion_vectors` | Vetores de movimento por pixel | implementada | platform/android/instanced_motion.inl + rhi/shaders/temporal_projection.glsl | Transparência e água usam as máscaras de reatividade/composição, não vetor próprio |
| `render.skinning` | Malha deformada por esqueleto | implementada | editor/editor_map_scene.cpp (paleta) + platform/android/instanced_skinning.inl (compute) | Até 4 influências por vértice e 256 juntas por skin; morph targets não são importados |
| `render.environment.atmosphere` | Céu atmosférico | implementada | runtime/scene_environment.cpp + rhi/shaders/dirt_road_sky.frag | — |
| `render.environment.physical_atmosphere` | Céu físico Rayleigh/Mie | implementada | renderer/scene_environment.cpp + rhi/shaders/dirt_road_sky.frag | — |
| `render.environment.fog` | Neblina por profundidade | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.environment.volumes` | Volumes de ambiente por câmera | implementada | runtime/scene_environment.cpp + renderer/scene_environment.cpp | — |
| `render.post.tonemap` | Exposição e mapeamento de tom | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.post.auto_exposure` | Exposição automática por histograma | implementada | platform/android/instanced_auto_exposure.inl | Histograma e adaptação por vista; o EV fica restrito à faixa autoral |
| `render.post.bloom` | Brilho estourado | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.post.film_grain` | Grão de filme | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.post.ambient_occlusion` | Oclusão ambiente em tela | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.aa.fxaa` | Antisserrilhado espacial | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.aa.temporal` | Antisserrilhado temporal | limitada pelo aparelho | rhi/shaders/post_process_temporal.frag | Exige histórico e profundidade alocáveis no backend |
| `render.upscale.temporal` | Ampliação temporal Arm ASR / AMD FSR 2 | limitada pelo aparelho | rhi/temporal_upscaler.cpp + platform/android/instanced_temporal_upscaler.inl | Exige float16/int16, formatos de storage e subgrupos no aparelho; recusa com motivo quando falta |
| `animation.clip` | Clipe de animação por nós | implementada | runtime/scene_animation.cpp + resources/skeletal_animation.cpp | Translação, rotação e escala; pesos de morph não; sem mistura entre clipes |
