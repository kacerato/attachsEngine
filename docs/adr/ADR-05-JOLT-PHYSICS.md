# ADR-05 — Jolt Physics em vez de solver próprio

- **Estado:** aceita e implementada
- **Data:** 26/08/2026
- **Item do plano:** 0.4.1, 4.1
- **Decisores:** arquitetura, física

## Contexto

Um solver de física rígida robusto (broad/narrow phase, ilhas, dormência,
juntas com motor, character controller) é 2-3 anos-pessoa de trabalho
especializado, e não é diferencial competitivo do produto — o diferencial é
o editor mobile e o AetherFlow (ADR-06), não ter um solver proprietário.

## Decisão

Jolt Physics vendorizado como biblioteca de física 3D. Nenhum solver próprio
é escrito.

## Alternativas descartadas

1. **Escrever solver do zero.** Rejeitada pelo custo/risco — 2-3 anos-pessoa
   que não constrói vantagem competitiva.
2. **PhysX.** Licenciamento e superfície de binário maiores que o
   necessário; Jolt é código aberto (MIT), moderno, com suporte ARM/mobile
   de primeira classe desde o design.
3. **Bullet.** Manutenção mais lenta e API mais antiga que Jolt no momento
   da decisão.

## Evidência

- Vendorizado em `native/third_party/JoltPhysics/`, commit
  `78d483dc3d375581203cf070ea2790e8045e0879` (2026-08-24), registrado em
  `VENDORED_COMMIT.txt`.
- `native/physics/jolt_bridge.h` (592 linhas) / `.cpp` (1441 linhas):
  fronteira C ABI blittable completa — mundo, corpos (estático/cinemático/
  dinâmico), formas (caixa/esfera/cápsula), dormência, camadas de colisão,
  juntas (Point/Hinge/Slider/Distance) com motor, queries (RayCastAll,
  ShapeCastClosest, OverlapShape), character controller sobre
  `JPH::CharacterVirtual`, trigger/sensor persistente.
- Fachada C#: `managed/Aether.Core/Physics/PhysicsWorld.cs`,
  `PhysicsComponents.cs`, `PhysicsSyncSystem.cs`, `JointSyncSystem.cs`,
  `NativePhysics.cs`.
- **Testes C++** (56 no total): `test_jolt_bridge.cpp` (9),
  `test_joint_bridge.cpp` (11), `test_query_bridge.cpp` (10),
  `test_character_bridge.cpp` (11), `test_trigger_bridge.cpp` (6),
  `test_body_batch_bridge.cpp` (3), `test_physics_capacity.cpp` (3),
  `test_determinism.cpp` (3).
- **Testes C#**: `PhysicsTests.cs` (15), `PhysicsJointTests.cs` (13),
  `PhysicsQueryTests.cs` (9), `PhysicsCharacterTests.cs` (17).
- Modo de determinismo cross-platform opcional (item 4.1.8):
  `AETHER_PHYSICS_DETERMINISTIC` em `native/CMakeLists.txt`, usando
  `CROSS_PLATFORM_DETERMINISTIC` nativo do próprio Jolt.

## Nota — física 2D não reabre esta decisão

`docs/adr/ADR-013-PHYSICS-2D-BACKEND.md` registra que Box2D v3.1.1 foi
vendorizado **somente para benchmark comparativo** contra o Jolt restrito ao
plano XY (`AetherAllowedDOFs.Plane2D`), sem entrar na ABI, ECS ou runtime.
Essa investigação reforça, não contradiz, esta ADR: Jolt continua sendo o
único backend de física em produção; a ADR-013 decide apenas se um segundo
backend especializado para 2D se justifica, o que é uma questão ortogonal a
"escrever solver próprio vs. usar biblioteca madura".

## Consequências

- Nenhum código de produto deve chamar a API C++ do Jolt diretamente — só
  através de `jolt_bridge.h`, que mantém a ABI blittable e isola o resto da
  engine de detalhes internos do Jolt (ex.: `JPH::EAllowedDOFs::All` !=
  `AetherAllowedDOFs::All`, documentado explicitamente no bridge).
- Atualizações de versão do Jolt exigem reexecutar a suíte completa de
  física (56 testes C++ + 54 testes C#) antes de merge, dado o volume de
  comportamento coberto.
