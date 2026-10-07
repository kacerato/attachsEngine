# Consultas físicas persistentes (bloco F, F043)

Componentes da família Física 3D, subfamília Consultas, avaliados por `runtime/scene_physics_queries.h` em cada quadro de Play, depois da física e antes de tweens e câmera.

| Componente | Id | Referência Godot 4.5 | O que faz |
|---|---|---|---|
| Raio | `astra.physics.raycast` v1 | [RayCast3D](https://docs.godotengine.org/en/4.5/classes/class_raycast3d.html) | Raio da origem até o alvo local (inclui rotação e escala do objeto); guarda o primeiro acerto |
| Varredura de forma | `astra.physics.shapecast` v1 | [ShapeCast3D](https://docs.godotengine.org/en/4.5/classes/class_shapecast3d.html) | Esfera, caixa, cápsula ou cilindro varrendo até o alvo; primeiro acerto |
| Braço de mola | `astra.physics.spring_arm` v1 | [SpringArm3D](https://docs.godotengine.org/en/4.5/classes/class_springarm3d.html) | Raio (ou esfera, com raio > 0) ao longo de +Z local; põe os filhos diretos em (0,0,distância livre − margem) |

Filtro comum: camada (todas ou uma), ignorar o próprio corpo (o do objeto ou do ancestral mais próximo com Corpo físico), incluir sensores, estáticos e dinâmicos — o mesmo `QueryFilter` das consultas por script.

Métodos (scripts pela fachada gerada e Conexão de evento): `colliding`, `collider`, `point`, `normal`, `distance`, `update` (Raio e Varredura; `update` também na Conexão: métodos 24 e 25) e `hit_length` (Braço). No Play, o Inspector mostra "Acertando X a d m", "Sem acerto" ou "Comprimento atual"; a viewport desenha o raio/braço até o alcance. Ícones novos: `component/raycast`, `component/shapecast`, `component/spring-arm`.

| Aspecto | Astra | Classificação |
|---|---|---|
| Resultado múltiplo da ShapeCast | Só o primeiro acerto | Adaptação explícita: overlaps múltiplos ficam em `Physics.Overlap` por script |
| Braço escreve a pose dos filhos | Só quando nenhum sistema tem autoridade sobre o filho | Equivalente, com autoridade de pose explícita |
| Depuração visual | Linha no editor; sem forma de debug no jogo | Adaptação explícita |

## Validação (07/10/2026)

- Host: `physics_query_components_*` — raio acerta o chão a 2 m com normal +Y, ignora o próprio corpo, filtra a camada 3; esfera de raio 0,5 encosta depois de 2,5 m; braço põe a câmera a 4,65 m (parede − margem); métodos pela ABI, `update` imediato e recusa `NotRunning` sem avaliador.
- Aparelho (`FisicaF-20261007`): `raio acerta=True distancia=2.00`, `braco comprimento=2.65 camera=2.65` — PASS (`docs/validacao/evidencias/physics-f-20261007/aparelho-logcat.txt`).
