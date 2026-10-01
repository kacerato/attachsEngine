# Physics2D pública e ABI v21 — 2026-09-30

O novo tail de `ScriptSceneAccess` acrescenta duas funções após `setTriple`, preservando todos os offsets anteriores: `body2DCommand` e `query2D`. NativeBehaviorRuntime exige versão 21, tamanho exato e ambas as funções presentes; um runtime anterior não é aceito como se tivesse o solver. `ScriptBridge::setPhysics2D` recebe o solver real do Play antes de Start. Não altera ScriptRuntimeApi.

[Rigidbody2D, Unity 6000.0](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Rigidbody2D.html) e seu [binding oficial 6000.0](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Modules/Physics2D/ScriptBindings/Physics2D.bindings.cs) separam operações Vector2 e consultas do mundo 2D. [PhysicsDirectSpaceState2D, Godot 4.5](https://docs.godotengine.org/en/4.5/classes/class_physicsdirectspacestate2d.html) e [PhysicsServer2D 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/servers/physics_server_2d.cpp) mostram consultas com máscaras/exclusões no servidor dimensional próprio. Astra mantém seu ScenePhysics2D/Box2D independente, sem encaminhar consultas ao Jolt 3D restrito ao plano.

## Operações executadas

`Behavior.Physics2D.Body(owner)` fornece `Body2DAccess`: ler/escrever `BodyVelocity2D(Vector2 Linear, float AngularDegrees)`, AddForce, AddImpulse, AddTorque, AddAngularImpulse e MoveKinematic(Vector2 position, rotationDegrees). A fachada passa mundo, ID e geração do objeto; a ponte valida esse handle antes de tocar o solver. Valores não finitos, mundo estrangeiro, geração obsoleta, ausência do serviço e operações inválidas são recusados. A elegibilidade Dynamic/Kinematic é decidida pelo solver real, não simulada na fachada. Velocidade angular e orientação são graus; torque e impulso angular permanecem unidades físicas do solver (N.m e N.m.s).

`Physics2DAccess.RayCast(origin, translation, results, filter)` usa um segmento XY: translation é deslocamento inteiro, não direção normalizada. `OverlapCircle(center, radius, results, filter)` requer raio positivo. Filtro reutiliza máscaras de camada, tipos estático/dinâmico, sensores e ID ignorado; a consulta pertence ao mundo do Behavior. A contagem total pode exceder o buffer. Buffer vazio consulta somente quantidade. Capacidade máxima da ponte4096; resultados escritos são a parte que cabe.

`PhysicsHit2D` expõe GameObject, ColliderInstance, Vector2 Point, Vector2? Normal, Distance, Fraction e IsSensor. Internamente reutiliza ScriptQueryHit blittable com Z=0. Raio oferece normal XY, fração e distância do segmento; overlap não inventa normal e deixa distância/fração zero. Ordenação e identidade de colisor vêm do solver. Não são consultas 3D nem suportam shape casts 2D adicionais neste pacote.

## Lifecycle, limites e custo

Estado de corpo pertence ao ScenePhysics2D; a API não cria outro cache de velocidades nem persiste valores transitórios no authoring. Persistência Body2D/Collider2D pertence ao schema real. Rebuild/remoção/Stop são realizados pelo lifecycle do Play. O adapter gerenciado preserva o guard de thread/sessão existente. Eventos Trigger/Collision genéricos são reutilizados pelo host por serem identidades e fases dimensionalmente neutras, com normal XY/Z0; não existem callbacks 2D especiais fingidos.

Força/impulso atuam no centro; não há ponto de aplicação, material físico reutilizável ou consultas por polígono nesta API. MoveKinematic respeita o passo fixo1/60 do solver existente. Falhas de tipo de corpo ou corpo não existente retornam InvalidArgument; ainda não há diagnóstico ABI especializado para cada recusa física.

Consultas nativas usam buffer temporário limitado ao capacity, e o solver coleta/ordena seus acertos. A fachada usa stack buffer cru até32hits e array acima disso, além dos GameObject retornados. Não se anuncia consulta sem alocação universal nem caminho adequado a milhares de queries por quadro sem medição.

## Cenário de validação preparado

`test_script_physics2d.cpp` executa EditorPlayScene → ScriptBridge → ABI → ScenePhysics2D real: set/get velocidade angular/linear, impulso com efeito, geração/mundo inválidos, raio XY com identidade de collider, overlap contado sem buffer e normal ausente. O serviço C# é duplo de captura da ABI nesse teste; portanto ele prova o consumidor nativo e layout, não execução do CLR. Build/teste centralizados pelo agente principal; nenhum ADB foi usado. A compilação gerenciada e validação em aparelho devem ser registradas separadamente quando executadas.


Validação central: cenário nativo ABI→Box2D passou; build C# inclui Physics2D e ABI21 sem erros; filtros Astra 49/49 e Save 5/5 passaram (com sobreposição). Joint2D usa também a fachada de componente gerada e os setters refletidos; não depende de uma API específica de joints. Resultados e limites em EXECUCAO-AUDITORIA-UNITY-ASTRA-2026-09-30.md. Nenhum ADB.
