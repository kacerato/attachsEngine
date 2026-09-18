# Matriz de propriedades dos componentes

**Gerado por `scene::componentMatrixMarkdown()`** a partir dos descritores de componente.
Não edite à mão: acrescente a propriedade no descritor e regenere.
Uma linha só existe aqui quando tem identidade persistente, consumidor declarado e
capacidade do motor disponível — as três condições que `auditComponentContracts()` exige.

## Corpo físico · `astra.physics.body` v3

Massa e resposta física. **Consumidor:** runtime/scene_physics.cpp → Jolt. **Invalida:** corpo físico.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional |
|---|---|---|---|---|---|---|---|---|---|
| `mass` | Massa kg | número | Corpo | 1 | 0.01 … 1000000 | kg | runtime/scene_physics.cpp → Jolt | corpo físico | sim |
| `friction` | Atrito | número | Corpo | 0.5 | 0 … 1 |  | runtime/scene_physics.cpp → Jolt | corpo físico | não |
| `restitution` | Restituição | número | Corpo | 0 | 0 … 1 |  | runtime/scene_physics.cpp → Jolt | corpo físico | não |
| `velocity_x` | Velocidade inicial X | número | Início | 0 | -1000 … 1000 | m/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim |
| `velocity_y` | Velocidade inicial Y | número | Início | 0 | -1000 … 1000 | m/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim |
| `velocity_z` | Velocidade inicial Z | número | Início | 0 | -1000 … 1000 | m/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim |
| `angular_x` | Giro inicial X · rad/s | número | Início | 0 | -1000 … 1000 | rad/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim |
| `angular_y` | Giro inicial Y · rad/s | número | Início | 0 | -1000 … 1000 | rad/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim |
| `angular_z` | Giro inicial Z · rad/s | número | Início | 0 | -1000 … 1000 | rad/s | runtime/scene_physics.cpp → Jolt | corpo físico | sim |
| `linear_damping` | Amortecimento linear | número | Amortecimento | 0.05 | 0 … 10 |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim |
| `angular_damping` | Amortecimento angular | número | Amortecimento | 0.05 | 0 … 10 |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim |
| `gravity_factor` | Multiplicador da gravidade | número | Amortecimento | 1 | -100 … 100 |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim |
| `sensor` | Sensor sem resposta | booleano | Corpo | falso | verdadeiro \| falso |  | runtime/scene_physics.cpp → Jolt | corpo físico | não |
| `allow_sleep` | Permitir repouso | booleano | Corpo | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics.cpp → Jolt | corpo físico | sim |
| `motion` | Movimento | enumeração | Corpo | Estático | Estático \| Cinemático \| Dinâmico |  | runtime/scene_physics.cpp → Jolt | corpo físico | não |

## Personagem · `astra.physics.character` v2

Locomoção com cápsula. **Consumidor:** runtime/scene_physics.cpp → CharacterVirtual. **Invalida:** forma física, corpo físico.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional |
|---|---|---|---|---|---|---|---|---|---|
| `radius` | Raio m | número | Cápsula | 0.45 | 0.01 … 10 | m | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não |
| `half_height` | Meia altura do cilindro m | número | Cápsula | 0.55 | 0.01 … 10 | m | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não |
| `eye_height` | Altura dos olhos m | número | Cápsula | 1.65 | 0.02 … 20 | m | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não |
| `speed` | Velocidade m/s | número | Locomoção | 8 | 0.01 … 100 | m/s | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não |
| `slope_degrees` | Inclinação máxima graus | número | Locomoção | 45 | 1 … 89 | ° | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não |
| `jump_speed` | Velocidade do salto m/s | número | Locomoção | 5 | 0 … 100 | m/s | runtime/scene_physics.cpp → CharacterVirtual | forma física, corpo físico | não |

## Olhar · `astra.camera.look` v1

Rotação da câmera por toque. **Consumidor:** runtime/game_world.cpp → pose da câmera. **Invalida:** entrada.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional |
|---|---|---|---|---|---|---|---|---|---|
| `yaw_sensitivity` | Sensibilidade horizontal graus/tela | número | Sensibilidade | 300 | 0 … 720 | ° | runtime/game_world.cpp → pose da câmera | entrada | não |
| `pitch_sensitivity` | Sensibilidade vertical graus/tela | número | Sensibilidade | 195 | 0 … 720 | ° | runtime/game_world.cpp → pose da câmera | entrada | não |
| `pitch_limit` | Limite vertical graus | número | Limites | 83 | 1 … 89 | ° | runtime/game_world.cpp → pose da câmera | entrada | não |

## Colisor 3D · `astra.physics.collider` v3

Volume de contato. **Consumidor:** runtime/scene_physics.cpp → forma do Jolt. **Invalida:** forma física.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional |
|---|---|---|---|---|---|---|---|---|---|
| `half_x` | Meia extensão X | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim |
| `half_y` | Meia extensão Y | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim |
| `half_z` | Meia extensão Z | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim |
| `radius` | Raio | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim |
| `half_height` | Meia altura cilíndrica | número | Forma | 0.5 | 0.01 … 10000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | sim |
| `center_x` | Centro X | número | Pose | 0 | -10000000 … 10000000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | não |
| `center_y` | Centro Y | número | Pose | 0 | -10000000 … 10000000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | não |
| `center_z` | Centro Z | número | Pose | 0 | -10000000 … 10000000 |  | runtime/scene_physics.cpp → forma do Jolt | forma física | não |
| `rotation_x` | Rotação local X | número | Pose | 0 | -10000000 … 10000000 | ° | runtime/scene_physics.cpp → forma do Jolt | forma física | não |
| `rotation_y` | Rotação local Y | número | Pose | 0 | -10000000 … 10000000 | ° | runtime/scene_physics.cpp → forma do Jolt | forma física | não |
| `rotation_z` | Rotação local Z | número | Pose | 0 | -10000000 … 10000000 | ° | runtime/scene_physics.cpp → forma do Jolt | forma física | não |
| `enabled` | Ativo | booleano |  | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics.cpp → forma do Jolt | forma física | não |
| `shape` | Forma | enumeração | Forma | Caixa | Caixa \| Esfera \| Cápsula |  | runtime/scene_physics.cpp → forma do Jolt | forma física | não |
| `owner` | Corpo proprietário | referência | Vínculo | Neste objeto | astra.physics.body · neste objeto ou ancestral |  | runtime/scene_physics.cpp → forma do Jolt | forma física | não |

## Junta · `astra.physics.joint` v1

Conexão, limites e motor entre corpos. **Consumidor:** runtime/scene_physics.cpp → constraint do Jolt. **Invalida:** corpo físico.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional |
|---|---|---|---|---|---|---|---|---|---|
| `anchor_a_x` | Âncora A · X | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não |
| `anchor_a_y` | Âncora A · Y | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não |
| `anchor_a_z` | Âncora A · Z | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não |
| `anchor_b_x` | Âncora B · X | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não |
| `anchor_b_y` | Âncora B · Y | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não |
| `anchor_b_z` | Âncora B · Z | número | Âncoras | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não |
| `axis_a_x` | Eixo A · X | número | Movimento | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim |
| `axis_a_y` | Eixo A · Y | número | Movimento | 1 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim |
| `axis_a_z` | Eixo A · Z | número | Movimento | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim |
| `axis_b_x` | Eixo B · X | número | Movimento | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim |
| `axis_b_y` | Eixo B · Y | número | Movimento | 1 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim |
| `axis_b_z` | Eixo B · Z | número | Movimento | 0 | -1 … 1 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim |
| `limit_min` | Limite mínimo | número | Movimento | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim |
| `limit_max` | Limite máximo | número | Movimento | 1 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim |
| `motor_velocity` | Velocidade do motor | número | Motor | 0 | -1000 … 1000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim |
| `motor_position` | Alvo do motor | número | Motor | 0 | -100000 … 100000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim |
| `motor_force` | Força / torque máximo | número | Motor | 100 | 0 … 1000000 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim |
| `spring_frequency` | Frequência · Hz | número | Motor | 2 | 0.001 … 1000 | Hz | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim |
| `spring_damping` | Amortecimento da mola | número | Motor | 1 | 0 … 10 |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim |
| `enabled` | Ativa | booleano |  | verdadeiro | verdadeiro \| falso |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não |
| `kind` | Tipo | enumeração |  | Distância | Ponto \| Dobradiça \| Deslizante \| Distância |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não |
| `motor` | Motor | enumeração | Motor | Desligado | Desligado \| Velocidade \| Posição \| Posição e velocidade |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | sim |
| `connected_body` | Conectar corpo | referência | Âncoras | Escolher corpo | astra.physics.body · outro objeto |  | runtime/scene_physics.cpp → constraint do Jolt | corpo físico | não |

## Câmera · `astra.camera` v2

Projeção e enquadramento. **Consumidor:** renderer/render_view.h → matriz de projeção e culling. **Invalida:** desenho.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional |
|---|---|---|---|---|---|---|---|---|---|
| `vertical_fov` | Campo vertical | número | Lente | 60 | 1 … 170 | ° | renderer/render_view.h → matriz de projeção e culling | desenho | sim |
| `near_plane` | Próximo | número | Lente | 0.1 | 0.001 … 10000 | m | renderer/render_view.h → matriz de projeção e culling | desenho | não |
| `far_plane` | Distante | número | Lente | 2000 | 0.01 … 1000000 | m | renderer/render_view.h → matriz de projeção e culling | desenho | não |
| `priority` | Prioridade | número | Saída | 0 | -10000 … 10000 |  | renderer/render_view.h → matriz de projeção e culling | desenho | não |
| `orthographic_half_height` | Meia altura | número | Lente | 5 | 0.001 … 100000 | m | renderer/render_view.h → matriz de projeção e culling | desenho | sim |
| `enabled` | Usar no Play | booleano | Saída | verdadeiro | verdadeiro \| falso |  | renderer/render_view.h → matriz de projeção e culling | desenho | não |
| `projection` | Projeção | enumeração | Lente | Perspectiva | Perspectiva \| Ortográfica |  | renderer/render_view.h → matriz de projeção e culling | desenho | não |

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

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional |
|---|---|---|---|---|---|---|---|---|---|
| `base_color.r` | Cor R | número |  | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não |
| `base_color.g` | Cor G | número |  | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não |
| `base_color.b` | Cor B | número |  | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não |
| `roughness` | Rugosidade | número |  | 0.5 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não |
| `metallic` | Metálico | número |  | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não |
| `normal_scale` | Intensidade da normal | número |  | 1 | 0 … 16 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não |
| `specular` | Especular | número |  | 1 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não |
| `emission.r` | Emissão R | número |  | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não |
| `emission.g` | Emissão G | número |  | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não |
| `emission.b` | Emissão B | número |  | 0 | 0 … 1 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não |
| `emission_strength` | Potência de emissão | número |  | 1 | 0 … 10000 |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não |
| `enabled` | Renderizar | booleano |  | verdadeiro | verdadeiro \| falso |  | renderer/map_draw_update.h → instância e material efetivo | desenho, material | não |

## Luz · `astra.render.light` v1

Direcional, pontual ou spot. **Consumidor:** runtime/scene_lights.cpp → renderer/punctual_lights.h. **Invalida:** seleção de luzes.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional |
|---|---|---|---|---|---|---|---|---|---|
| `color.r` | Cor R | número | Emissão | 1 | 0 … 1 |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não |
| `color.g` | Cor G | número | Emissão | 1 | 0 … 1 |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não |
| `color.b` | Cor B | número | Emissão | 1 | 0 … 1 |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não |
| `intensity` | Intensidade | número | Emissão | 8 | 0 … 10000 |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não |
| `range` | Alcance | número | Volume | 10 | 0.01 … 1000 | m | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | sim |
| `inner_angle` | Meio-cone interno | número | Volume | 20 | 0 … 89 | ° | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | sim |
| `outer_angle` | Meio-cone externo | número | Volume | 35 | 0 … 89 | ° | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | sim |
| `enabled` | Acesa | booleano |  | verdadeiro | verdadeiro \| falso |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não |
| `kind` | Modalidade | enumeração |  | Pontual | Direcional \| Pontual \| Spot |  | runtime/scene_lights.cpp → renderer/punctual_lights.h | seleção de luzes | não |

## Comportamento · `astra.script.behavior` v1

Código C# do projeto. **Consumidor:** runtime/script_bridge.cpp → runtime .NET. **Invalida:** comportamento.

| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional |
|---|---|---|---|---|---|---|---|---|---|
| `enabled` | Ativo | booleano | Execução | verdadeiro | verdadeiro \| falso |  | runtime/script_bridge.cpp → runtime .NET | comportamento | não |

## Capacidades do motor

| Capacidade | Nome | Estado | Onde vive | Limite |
|---|---|---|---|---|
| `render.light.directional` | Sol direcional | implementada | renderer/punctual_lights.h + rhi/shaders/material_shading.glsl | — |
| `render.light.punctual` | Luzes pontuais e spot | implementada | renderer/punctual_lights.h (8 por quadro) | — |
| `render.light.temperature` | Temperatura de cor em kelvin | planejada | scene/light_units.h | Sem conversão kelvin→RGB linear no caminho de autoria |
| `render.light.photometric` | Unidades fotométricas (lux, lumen, candela) | planejada | scene/light_units.h | Sem calibração entre unidade autoral e irradiância do shader |
| `render.light.cookie` | Máscara projetada (cookie) | planejada | renderer/punctual_lights.h | Sem amostragem de textura por luz no shader de fragmento |
| `render.light.area` | Luz de área | planejada | renderer/punctual_lights.h | Sem integração de fonte com extensão |
| `render.shadow.directional` | Sombra do sol em cascatas | implementada | renderer/shadow_cascades.cpp + rhi/shaders/shadow_depth.vert | — |
| `render.shadow.punctual` | Sombra de luz pontual ou spot | planejada | renderer/punctual_lights.h | Sem atlas nem cubemap de profundidade para luz local |
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
| `render.lod.group` | Grupo de LOD autoral por objeto | planejada | renderer/lod_selection.cpp | Sem componente de grupo nem malhas derivadas por nível |
| `render.visibility.hzb` | Oclusão por pirâmide de profundidade | implementada | renderer/hzb_visibility.cpp | — |
| `render.instancing.gpu` | Culling e compactação de desenho em GPU | implementada | renderer/gpu_draw_culling.cpp | — |
| `render.motion_vectors` | Vetores de movimento por objeto | planejada | renderer/frame_graph.cpp | Sem pose anterior por instância nem alvo de velocidade |
| `render.post.tonemap` | Exposição e mapeamento de tom | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.post.bloom` | Brilho estourado | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.post.ambient_occlusion` | Oclusão ambiente em tela | planejada | renderer/frame_graph.cpp | Sem passe de oclusão em espaço de tela |
| `render.aa.fxaa` | Antisserrilhado espacial | implementada | rhi/shaders/post_process_common.glsl | — |
| `render.aa.temporal` | Antisserrilhado temporal | limitada pelo aparelho | rhi/shaders/post_process_temporal.frag | Exige histórico e profundidade alocáveis no backend |
