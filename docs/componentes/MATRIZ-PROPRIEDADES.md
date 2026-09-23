# Matriz de propriedades dos componentes

**Gerado por `scene::componentMatrixMarkdown()`** a partir dos descritores de componente.
Não edite à mão: acrescente a propriedade no descritor e regenere.
Uma linha só existe aqui quando tem identidade persistente, consumidor declarado e
capacidade do motor disponível — as três condições que `auditComponentContracts()` exige.

## Corpo físico · `astra.physics.body` v3

Massa e resposta física. **Consumidor:** runtime/scene_physics.cpp → Jolt. **Invalida:** corpo físico.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot |
|---|---|---|---|---|---|---|---|---|---|---|
| `mass` | Massa kg | número | Corpo | 1 | 0.01 … 1000000 | kg | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não |
| `friction` | Atrito | número | Corpo | 0.5 | 0 … 1 |  | runtime/scene_physics.cpp → Jolt | corpo físico | não | não |
| `restitution` | Restituição | número | Corpo | 0 | 0 … 1 |  | runtime/scene_physics.cpp → Jolt | corpo físico | não | não |
| `velocity_x` | Velocidade inicial X | número | Início | 0 | -1000 … 1000 | m/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não |
| `velocity_y` | Velocidade inicial Y | número | Início | 0 | -1000 … 1000 | m/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não |
| `velocity_z` | Velocidade inicial Z | número | Início | 0 | -1000 … 1000 | m/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não |
| `angular_x` | Giro inicial X · rad/s | número | Início | 0 | -1000 … 1000 | rad/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não |
| `angular_y` | Giro inicial Y · rad/s | número | Início | 0 | -1000 … 1000 | rad/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não |
| `angular_z` | Giro inicial Z · rad/s | número | Início | 0 | -1000 … 1000 | rad/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não |
| `linear_damping` | Amortecimento linear | número | Amortecimento | 0.05 | 0 … 10 |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não |
| `angular_damping` | Amortecimento angular | número | Amortecimento | 0.05 | 0 … 10 |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não |
| `gravity_factor` | Multiplicador da gravidade | número | Amortecimento | 1 | -100 … 100 |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não |
| `sensor` | Sensor sem resposta | booleano | Corpo | falso | verdadeiro \| falso |  | runtime/scene_physics.cpp → Jolt | corpo físico | não | não |
| `allow_sleep` | Permitir repouso | booleano | Corpo | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim | não |
| `motion` | Movimento | enumeração | Corpo | Estático | Estático \| Cinemático \| Dinâmico |  | runtime/scene_physics.cpp → Jolt | corpo físico | não | não |

## Personagem · `astra.physics.character` v2

Locomoção com cápsula. **Consumidor:** runtime/scene_physics.cpp → CharacterVirtual. **Invalida:** forma física, corpo físico.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot |
|---|---|---|---|---|---|---|---|---|---|---|
| `radius` | Raio m | número | Cápsula | 0.45 | 0.01 … 10 | m | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não |
| `half_height` | Meia altura do cilindro m | número | Cápsula | 0.55 | 0.01 … 10 | m | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não |
| `eye_height` | Altura dos olhos m | número | Cápsula | 1.65 | 0.02 … 20 | m | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não |
| `speed` | Velocidade m/s | número | Locomoção | 8 | 0.01 … 100 | m/s | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não |
| `slope_degrees` | Inclinação máxima graus | número | Locomoção | 45 | 1 … 89 | ° | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não |
| `jump_speed` | Velocidade do salto m/s | número | Locomoção | 5 | 0 … 100 | m/s | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não | não |

## Olhar · `astra.camera.look` v1

Rotação da câmera por toque. **Consumidor:** runtime/game_world.cpp → pose da câmera. **Invalida:** entrada.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot |
|---|---|---|---|---|---|---|---|---|---|---|
| `yaw_sensitivity` | Sensibilidade horizontal graus/tela | número | Sensibilidade | 300 | 0 … 720 | ° | runtime/game_world.cpp → pose da câmera | entrada | não | não |
| `pitch_sensitivity` | Sensibilidade vertical graus/tela | número | Sensibilidade | 195 | 0 … 720 | ° | runtime/game_world.cpp → pose da câmera | entrada | não | não |
| `pitch_limit` | Limite vertical graus | número | Limites | 83 | 1 … 89 | ° | runtime/game_world.cpp → pose da câmera | entrada | não | não |

## Colisor 3D · `astra.physics.collider` v6

Volume de contato. **Consumidor:** runtime/scene_physics.cpp → forma do Jolt. **Invalida:** forma física.

**Recursos endereçados**

| Binding | Rótulo | Tipo de recurso | Herda | Ausência declarada |
|---|---|---|---|---|
| `collision_mesh` | Malha de colisão | mesh | sim | não |

**Propriedades**

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot |
|---|---|---|---|---|---|---|---|---|---|---|
| `half_x` | Meia extensão X | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `half_y` | Meia extensão Y | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `half_z` | Meia extensão Z | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `radius` | Raio | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `half_height` | Meia altura cilíndrica | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `center_x` | Centro X | número | Pose | 0 | -10000000 … 10000000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `center_y` | Centro Y | número | Pose | 0 | -10000000 … 10000000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `center_z` | Centro Z | número | Pose | 0 | -10000000 … 10000000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `rotation_x` | Rotação local X | número | Pose | 0 | -10000000 … 10000000 | ° | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `rotation_y` | Rotação local Y | número | Pose | 0 | -10000000 … 10000000 | ° | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `rotation_z` | Rotação local Z | número | Pose | 0 | -10000000 … 10000000 | ° | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `hull_tolerance` | Tolerância do casco | número | Cozimento | 0.001 | 0.00001 … 1 | u | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `active_edge_angle` | Ângulo de aresta ativa | número | Cozimento | 5 | 0 … 90 | ° | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `enabled` | Ativo | booleano |  | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics.cpp → forma do Jolt | forma física | não | não |
| `convex` | Convexo | booleano | Forma | falso | verdadeiro \| falso |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `weld_vertices` | Soldar vértices iguais | booleano | Cozimento | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `optimize_cooking` | Otimizar para o jogo | booleano | Cozimento | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim | não |
| `shape` | Forma | enumeração | Forma | Caixa | Caixa \| Esfera \| Cápsula \| Malha |  | runtime/scene_physics.cpp → forma do Jolt | forma física | não | não |
| `owner` | Corpo proprietário | referência | Vínculo | Neste objeto | astra.physics.body · neste objeto ou ancestral |  | runtime/scene_physics.cpp → forma do Jolt | forma física | não | não |

## Junta · `astra.physics.joint` v1

Conexão, limites e motor entre corpos. **Consumidor:** runtime/scene_physics.cpp → constraint do Jolt. **Invalida:** corpo físico.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot |
|---|---|---|---|---|---|---|---|---|---|---|
| `anchor_a_x` | Âncora A · X | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não |
| `anchor_a_y` | Âncora A · Y | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não |
| `anchor_a_z` | Âncora A · Z | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não |
| `anchor_b_x` | Âncora B · X | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não |
| `anchor_b_y` | Âncora B · Y | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não |
| `anchor_b_z` | Âncora B · Z | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não |
| `axis_a_x` | Eixo A · X | número | Movimento | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não |
| `axis_a_y` | Eixo A · Y | número | Movimento | 1 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não |
| `axis_a_z` | Eixo A · Z | número | Movimento | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não |
| `axis_b_x` | Eixo B · X | número | Movimento | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não |
| `axis_b_y` | Eixo B · Y | número | Movimento | 1 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não |
| `axis_b_z` | Eixo B · Z | número | Movimento | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não |
| `limit_min` | Limite mínimo | número | Movimento | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não |
| `limit_max` | Limite máximo | número | Movimento | 1 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não |
| `motor_velocity` | Velocidade do motor | número | Motor | 0 | -1000 … 1000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não |
| `motor_position` | Alvo do motor | número | Motor | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não |
| `motor_force` | Força / torque máximo | número | Motor | 100 | 0 … 1000000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não |
| `spring_frequency` | Frequência · Hz | número | Motor | 2 | 0.001 … 1000 | Hz | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não |
| `spring_damping` | Amortecimento da mola | número | Motor | 1 | 0 … 10 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não |
| `enabled` | Ativa | booleano |  | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não |
| `kind` | Tipo | enumeração |  | Distância | Ponto \| Dobradiça \| Deslizante \| Distância |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não |
| `motor` | Motor | enumeração | Motor | Desligado | Desligado \| Velocidade \| Posição \| Posição e velocidade |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim | não |
| `connected_body` | Conectar corpo | referência | Âncoras | Escolher corpo | astra.physics.body · outro objeto |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não | não |

## Câmera · `astra.camera` v3

Projeção e enquadramento. **Consumidor:** renderer/render_view.h → matriz de projeção e culling. **Invalida:** desenho.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot |
|---|---|---|---|---|---|---|---|---|---|---|
| `vertical_fov` | Campo vertical | número | Lente | 60 | 1 … 170 | ° | renderer/render_view.h → matriz de projeção e culling | desenho | sim | não |
| `near_plane` | Próximo | número | Lente | 0.1 | 0.001 … 10000 | m | renderer/render_view.h → matriz de projeção e culling | desenho | não | não |
| `far_plane` | Distante | número | Lente | 2000 | 0.01 … 1000000 | m | renderer/render_view.h → matriz de projeção e culling | desenho | não | não |
| `priority` | Prioridade | número | Saída | 0 | -10000 … 10000 |  | renderer/render_view.h → matriz de projeção e culling | desenho | não | não |
| `orthographic_half_height` | Meia altura | número | Lente | 5 | 0.001 … 100000 | m | renderer/render_view.h → matriz de projeção e culling | desenho | sim | não |
| `enabled` | Usar no Play | booleano | Saída | verdadeiro | verdadeiro \| falso |  | renderer/render_view.h → matriz de projeção e culling | desenho | não | não |
| `projection` | Projeção | enumeração | Lente | Perspectiva | Perspectiva \| Ortográfica |  | renderer/render_view.h → matriz de projeção e culling | desenho | não | não |
| `environment_mask` | Ambientes | enumeração | Saída | Todos os ambientes | Todos os ambientes \| Ambiente 0 \| Ambiente 1 \| Ambiente 2 \| Ambiente 3 \| Ambiente 4 \| Ambiente 5 \| Ambiente 6 \| Ambiente 7 |  | renderer/scene_environment.cpp | desenho | não | não |

## Malha · `astra.render.mesh` v8

Geometria e material. **Consumidor:** renderer/map_draw_update.h → instância e material efetivo. **Capacidade:** `render.material.pbr` (implementada). **Invalida:** desenho, material.

**Recursos endereçados**

| Binding | Rótulo | Tipo de recurso | Herda | Ausência declarada |
|---|---|---|---|---|
| `mesh` | Malha | mesh | não | não |
| `material` | Material | material | sim | sim |
| `texture.base_color` | Cor base | texture | sim | sim |
| `texture.normal` | Normal | texture | sim | sim |
| `texture.metallic_roughness` | Metal / rugosidade | texture | sim | sim |
| `texture.emissive` | Emissão | texture | sim | sim |
| `texture.occlusion` | Oclusão | texture | sim | sim |

**Propriedades**

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot |
|---|---|---|---|---|---|---|---|---|---|---|
| `base_color.r` | Cor R | número |  | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não |
| `base_color.g` | Cor G | número |  | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não |
| `base_color.b` | Cor B | número |  | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não |
| `roughness` | Rugosidade | número |  | 0.5 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não |
| `metallic` | Metálico | número |  | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não |
| `normal_scale` | Intensidade da normal | número |  | 1 | 0 … 16 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não |
| `specular` | Especular | número |  | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não |
| `emission.r` | Emissão R | número |  | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não |
| `emission.g` | Emissão G | número |  | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não |
| `emission.b` | Emissão B | número |  | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não |
| `emission_strength` | Potência de emissão | número |  | 1 | 0 … 10000 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não |
| `enabled` | Renderizar | booleano |  | verdadeiro | verdadeiro \| falso |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | não |
| `surface.alpha_cutoff` | Corte do alfa | número | Superfície | 0.5 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `channels.occlusion_strength` | Força da oclusão | número | Canais | -1 | -1 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `sampling.offset_u` | Deslocamento U | número | Amostragem | 0 | -100 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `sampling.offset_v` | Deslocamento V | número | Amostragem | 0 | -100 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `sampling.scale_u` | Escala U | número | Amostragem | 1 | 0.01 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `sampling.scale_v` | Escala V | número | Amostragem | 1 | 0.01 … 100 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `sampling.rotation` | Rotação da UV | número | Amostragem | 0 | -360 … 360 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `material.base_color.r` | Cor R | número | Cor | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `material.base_color.g` | Cor G | número | Cor | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `material.base_color.b` | Cor B | número | Cor | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `material.roughness` | Rugosidade | número | Superfície | 0.5 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `material.metallic` | Metálico | número | Superfície | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `material.normal_scale` | Intensidade da normal | número | Superfície | 1 | 0 … 16 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `material.specular` | Especular | número | Superfície | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `material.emission.r` | Emissão R | número | Emissão | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `material.emission.g` | Emissão G | número | Emissão | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `material.emission.b` | Emissão B | número | Emissão | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `material.emission_strength` | Potência de emissão | número | Emissão | 1 | 0 … 10000 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `material.override` | Material | enumeração | Cor | Herdado da fonte | Herdado da fonte \| Substituído nesta instância |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `surface.alpha_mode` | Tipo de superfície | enumeração | Superfície | Herdar | Herdar \| Opaco \| Recorte \| Mistura |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `surface.sides` | Faces | enumeração | Superfície | Herdar | Herdar \| Face única \| Face dupla |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `channels.roughness` | Canal da rugosidade | enumeração | Canais | Herdar | Herdar \| R \| G \| B \| A |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `channels.metallic` | Canal do metálico | enumeração | Canais | Herdar | Herdar \| R \| G \| B \| A |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `channels.occlusion` | Canal da oclusão | enumeração | Canais | Herdar | Herdar \| R \| G \| B \| A |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `channels.occlusion_source` | Origem da oclusão | enumeração | Canais | Herdar | Herdar \| Sem oclusão \| No mapa metal/rugosidade \| Textura própria |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `channels.normal_flip_y` | Inverter Y da normal | enumeração | Canais | Herdar | Herdar \| Não \| Sim |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `channels.alpha_source` | Origem do alfa | enumeração | Canais | Herdar | Herdar \| Alfa da cor base \| Sempre opaco \| Luminância da cor base |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `sampling.uv_set` | Conjunto de UV | enumeração | Amostragem | Herdar | Herdar \| UV 0 \| UV 1 \| Mundo (triplanar) |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `sampling.wrap` | Repetição | enumeração | Amostragem | Herdar | Herdar \| Repetir \| Fixar na borda \| Espelhar |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |
| `sampling.filter` | Filtro | enumeração | Amostragem | Herdar | Herdar \| Linear \| Vizinho mais próximo |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não | sim |

## Luz · `astra.render.light` v3

Direcional, pontual ou spot. **Consumidor:** runtime/scene_lights.cpp → renderer/punctual_lights.h. **Invalida:** seleção de luzes.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot |
|---|---|---|---|---|---|---|---|---|---|---|
| `color.r` | Cor R | número | Emissão | 1 | 0 … 1 |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não | não |
| `color.g` | Cor G | número | Emissão | 1 | 0 … 1 |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não | não |
| `color.b` | Cor B | número | Emissão | 1 | 0 … 1 |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não | não |
| `color_temperature` | Temperatura | número | Emissão | 6500 | 1667 … 25000 | K | scene/light_units.h → RGB linear | seleção de luzes | sim | não |
| `intensity` | Intensidade | número | Emissão | 1000 | 0 … 1000000 |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não | não |
| `range` | Alcance | número | Volume | 10 | 0.01 … 1000 | m | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | sim | não |
| `inner_angle` | Meio-cone interno | número | Volume | 20 | 0 … 89 | ° | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | sim | não |
| `outer_angle` | Meio-cone externo | número | Volume | 35 | 0 … 89 | ° | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | sim | não |
| `shadow_strength` | Força da sombra | número | Sombra | 1 | 0 … 1 |  | renderer/shadow_atlas.h → atlas local | seleção de luzes | sim | não |
| `shadow_bias` | Desvio | número | Sombra | 0.05 | 0 … 2 | texel | renderer/shadow_atlas.h → atlas local | seleção de luzes | sim | não |
| `shadow_normal_bias` | Desvio na normal | número | Sombra | 0.4 | 0 … 2 | texel | renderer/shadow_atlas.h → atlas local | seleção de luzes | sim | não |
| `shadow_near_plane` | Plano próximo da sombra | número | Sombra | 0.2 | 0.01 … 10 | m | renderer/shadow_atlas.h → atlas local | seleção de luzes | sim | não |
| `enabled` | Acesa | booleano | Geral | verdadeiro | verdadeiro \| falso |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não | não |
| `use_color_temperature` | Filtro por temperatura | booleano | Emissão | falso | verdadeiro \| falso |  | scene/light_units.h → RGB linear | seleção de luzes | não | não |
| `kind` | Modalidade | enumeração | Geral | Pontual | Direcional \| Pontual \| Spot |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não | não |
| `unit` | Unidade | enumeração | Emissão | Lux / lúmen | Interna (legada) \| Lux / candela \| Lux / lúmen |  | scene/light_units.h → irradiância linear | seleção de luzes | não | não |
| `shadow_mode` | Sombra | enumeração | Sombra | Nenhuma | Nenhuma \| Dura \| Suave |  | renderer/shadow_atlas.h → atlas local | seleção de luzes | sim | não |
| `shadow_resolution` | Resolução da sombra | enumeração | Sombra | Automática | Automática \| Baixa \| Média \| Alta \| Muito alta |  | renderer/shadow_atlas.h → atlas local | seleção de luzes | sim | não |

## Ambiente · `astra.render.environment` v5

Céu, atmosfera, neblina e pós globais ou por volume. **Consumidor:** runtime/scene_environment.cpp → renderer e pós. **Invalida:** desenho, política resolvida.

**Recursos endereçados**

| Binding | Rótulo | Tipo de recurso | Herda | Ausência declarada |
|---|---|---|---|---|
| `profile` | Perfil | environment_profile | sim | não |

**Propriedades**

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot |
|---|---|---|---|---|---|---|---|---|---|---|
| `priority` | Prioridade | número | Geral | 0 | -1000 … 1000 |  | runtime/scene_environment.cpp → seleção | desenho, política resolvida | não | não |
| `sky_zenith.r` | Zênite R | número | Atmosfera | 0.025 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não |
| `sky_zenith.g` | Zênite G | número | Atmosfera | 0.1 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não |
| `sky_zenith.b` | Zênite B | número | Atmosfera | 0.32 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não |
| `sky_horizon.r` | Horizonte R | número | Atmosfera | 0.28 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não |
| `sky_horizon.g` | Horizonte G | número | Atmosfera | 0.42 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não |
| `sky_horizon.b` | Horizonte B | número | Atmosfera | 0.62 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não |
| `ground.r` | Chão R | número | Atmosfera | 0.11 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não |
| `ground.g` | Chão G | número | Atmosfera | 0.12 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não |
| `ground.b` | Chão B | número | Atmosfera | 0.14 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não |
| `atmosphere` | Força atmosférica | número | Atmosfera | 1 | 0 … 1 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não |
| `sun_disk_degrees` | Diâmetro do sol | número | Atmosfera | 0.53 | 0.05 … 10 | ° | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não |
| `sun_disk_intensity` | Brilho do disco solar | número | Atmosfera | 8 | 0 … 100 |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | sim | não |
| `fog_color.r` | Neblina R | número | Neblina | 0.58 | 0 … 1 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `fog_color.g` | Neblina G | número | Neblina | 0.67 | 0 … 1 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `fog_color.b` | Neblina B | número | Neblina | 0.76 | 0 … 1 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `fog_light_energy` | Energia da neblina | número | Neblina | 1 | 0 … 65504 | × | platform/android/instanced_renderer.cpp → rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `fog_density` | Densidade | número | Neblina | 0.008 | 0 … 1 | 1/m | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `fog_start` | Início | número | Neblina | 8 | 0 … 10000 | m | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `exposure_ev` | Compensação | número | Pós | 0 | -16 … 16 | EV | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `bloom_threshold` | Limiar do bloom | número | Pós | 1 | 0 … 64 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `bloom_intensity` | Intensidade do bloom | número | Pós | 0.1 | 0 … 2 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `contrast` | Contraste | número | Pós | 1 | 0.5 … 2 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `saturation` | Saturação | número | Pós | 1 | 0 … 2 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `vignette_intensity` | Intensidade da vinheta | número | Pós | 0.18 | 0 … 1 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `film_grain_intensity` | Intensidade do grão | número | Pós | 0.05 | 0 … 1 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `ambient_occlusion_radius` | Raio | número | Oclusão ambiente | 1 | 0.05 … 10 | m | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `ambient_occlusion_intensity` | Intensidade | número | Oclusão ambiente | 1 | 0 … 4 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `ambient_occlusion_power` | Potência | número | Oclusão ambiente | 1.5 | 0.1 … 4 |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `ambient_occlusion_bias` | Viés | número | Oclusão ambiente | 0.02 | 0 … 1 | m | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `weight` | Peso | número | Volume | 1 | 0 … 1 |  | renderer/scene_environment.cpp | desenho, política resolvida | não | não |
| `blend_distance` | Distância de mistura | número | Volume | 0 | 0 … 100000 | m | renderer/scene_environment.cpp | desenho, política resolvida | sim | não |
| `box_size.x` | Tamanho X | número | Volume | 10 | 0.01 … 100000 | m | renderer/scene_environment.cpp | desenho, política resolvida | sim | não |
| `box_size.y` | Tamanho Y | número | Volume | 10 | 0.01 … 100000 | m | renderer/scene_environment.cpp | desenho, política resolvida | sim | não |
| `box_size.z` | Tamanho Z | número | Volume | 10 | 0.01 … 100000 | m | renderer/scene_environment.cpp | desenho, política resolvida | sim | não |
| `sphere_radius` | Raio | número | Volume | 5 | 0.01 … 100000 | m | renderer/scene_environment.cpp | desenho, política resolvida | sim | não |
| `enabled` | Ativo | booleano | Geral | verdadeiro | verdadeiro \| falso |  | runtime/scene_environment.cpp → renderer e pós | desenho, política resolvida | não | não |
| `fog` | Neblina | booleano | Neblina | falso | verdadeiro \| falso |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | não | não |
| `post` | Pós-processamento | booleano | Pós | verdadeiro | verdadeiro \| falso |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | não | não |
| `bloom` | Bloom | booleano | Pós | verdadeiro | verdadeiro \| falso |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `vignette` | Vinheta | booleano | Pós | falso | verdadeiro \| falso |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `film_grain` | Grão de filme | booleano | Pós | falso | verdadeiro \| falso |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `ambient_occlusion` | Oclusão ambiente | booleano | Oclusão ambiente | falso | verdadeiro \| falso |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `override_sky` | Sobrescrever céu | booleano | Volume | verdadeiro | verdadeiro \| falso |  | renderer/scene_environment.cpp | desenho, política resolvida | não | não |
| `override_fog` | Sobrescrever neblina | booleano | Volume | verdadeiro | verdadeiro \| falso |  | renderer/scene_environment.cpp | desenho, política resolvida | não | não |
| `override_post` | Sobrescrever pós | booleano | Volume | verdadeiro | verdadeiro \| falso |  | renderer/scene_environment.cpp | desenho, política resolvida | não | não |
| `sky` | Céu | enumeração | Atmosfera | Atmosfera | HDRI \| Atmosfera |  | rhi/shaders/dirt_road_sky.frag | desenho, política resolvida | não | não |
| `tone_mapper` | Tonemapping | enumeração | Pós | ACES | Neutro \| ACES |  | rhi/shaders/post_process_common.glsl | desenho, política resolvida | sim | não |
| `volume_shape` | Modo | enumeração | Volume | Global | Global \| Caixa \| Esfera |  | runtime/scene_environment.cpp | desenho, política resolvida | não | não |
| `volume_layer` | Camada | enumeração | Volume | Ambiente 0 | Ambiente 0 \| Ambiente 1 \| Ambiente 2 \| Ambiente 3 \| Ambiente 4 \| Ambiente 5 \| Ambiente 6 \| Ambiente 7 |  | renderer/scene_environment.cpp | desenho, política resolvida | não | não |

## LOD Group · `astra.render.lod_group` v2

Nível de detalhe pela altura na tela. **Consumidor:** runtime/lod_groups.h → visibilidade do desenho por vista. **Capacidade:** `render.lod.group` (implementada). **Invalida:** desenho.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot |
|---|---|---|---|---|---|---|---|---|---|---|
| `transition_0` | Transição LOD 0 | número | Níveis | 60 | 0.1 … 100 | % | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | não | não |
| `transition_1` | Transição LOD 1 | número | Níveis | 30 | 0.1 … 100 | % | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não |
| `transition_2` | Transição LOD 2 | número | Níveis | 10 | 0.1 … 100 | % | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não |
| `transition_3` | Transição LOD 3 | número | Níveis | 5 | 0.1 … 100 | % | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não |
| `size` | Tamanho | número | Limites | 1 | 0.001 … 1000000 |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | não | não |
| `fade_width_0` | Largura do fade LOD 0 | número | Fade | 0.2 | 0 … 1 |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não |
| `fade_width_1` | Largura do fade LOD 1 | número | Fade | 0.2 | 0 … 1 |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não |
| `fade_width_2` | Largura do fade LOD 2 | número | Fade | 0.2 | 0 … 1 |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não |
| `fade_width_3` | Largura do fade LOD 3 | número | Fade | 0.2 | 0 … 1 |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não |
| `animate_cross_fading` | Animate Cross-fading | booleano | Fade | falso | verdadeiro \| falso |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não |
| `level_count` | Níveis | enumeração | Níveis | 3 | 1 \| 2 \| 3 \| 4 |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | não | não |
| `fade_mode` | Fade Mode | enumeração | Fade | Nenhum | Nenhum \| Cross Fade |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | não | não |
| `force_level` | Forçar nível | enumeração | Execução | Automático | Automático \| LOD 0 \| LOD 1 \| LOD 2 \| LOD 3 |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não |
| `level_0` | Objetos LOD 0 | referência | Níveis | Nenhum | qualquer objeto · abaixo deste objeto |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | não | não |
| `level_1` | Objetos LOD 1 | referência | Níveis | Nenhum | qualquer objeto · abaixo deste objeto |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não |
| `level_2` | Objetos LOD 2 | referência | Níveis | Nenhum | qualquer objeto · abaixo deste objeto |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não |
| `level_3` | Objetos LOD 3 | referência | Níveis | Nenhum | qualquer objeto · abaixo deste objeto |  | runtime/lod_groups.h → visibilidade do desenho por vista | desenho | sim | não |

## Comportamento · `astra.script.behavior` v1

Código C# do projeto. **Consumidor:** runtime/script_bridge.cpp → runtime .NET. **Invalida:** comportamento.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot |
|---|---|---|---|---|---|---|---|---|---|---|
| `enabled` | Ativo | booleano | Execução | verdadeiro | verdadeiro \| falso |  | runtime/script_bridge.cpp → runtime .NET | comportamento | não | não |

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
| `render.gi.lightmap` | Iluminação indireta assada em lightmap | planejada | renderer/ (serviço de bake) | Sem serviço de bake, UV de lightmap nem atlas |
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
| `render.motion_vectors` | Vetores de movimento por objeto | planejada | renderer/frame_graph.cpp | Sem pose anterior por instância nem alvo de velocidade |
| `render.environment.atmosphere` | Céu atmosférico | implementada | runtime/scene_environment.cpp + rhi/shaders/dirt_road_sky.frag | — |
| `render.environment.fog` | Neblina por profundidade | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.environment.volumes` | Volumes de ambiente por câmera | implementada | runtime/scene_environment.cpp + renderer/scene_environment.cpp | — |
| `render.post.tonemap` | Exposição e mapeamento de tom | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.post.bloom` | Brilho estourado | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.post.film_grain` | Grão de filme | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.post.ambient_occlusion` | Oclusão ambiente em tela | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.aa.fxaa` | Antisserrilhado espacial | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.aa.temporal` | Antisserrilhado temporal | limitada pelo aparelho | rhi/shaders/post_process_temporal.frag | Exige histórico e profundidade alocáveis no backend |
