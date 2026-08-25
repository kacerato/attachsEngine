// Testes de reprodutibilidade (item 4.1.8 do plano: "Determinismo em ponto fixo (modo
// opcional) e testes de reprodutibilidade"). Ver o comentário completo em
// native/CMakeLists.txt (opção AETHER_PHYSICS_DETERMINISTIC) e docs/ESTADO.md para a
// investigação que motivou a decisão: o Jolt não tem modo fixed-point (reescrever seu
// solver/matemática para ponto fixo seria reescrever código de terceiros vendorizado por
// inteiro); o que existe e é usado aqui é CROSS_PLATFORM_DETERMINISTIC (nativo do Jolt),
// que desliga FMA e trava o nível de SIMD em SSE4.
//
// O QUE ESTES TESTES PROVAM E NÃO PROVAM, honestamente: rodamos a MESMA simulação duas vezes
// no MESMO processo/binário/plataforma e comparamos bit a bit — isso prova determinismo
// run-to-run (pré-requisito necessário para determinismo cross-platform: se a mesma máquina
// não reproduz a própria simulação, nenhuma outra máquina reproduziria também). NÃO prova
// determinismo cross-platform de verdade (isso exigiria rodar em hardware/SO diferentes e
// comparar — fora do alcance de uma suíte de teste rodando numa única máquina de CI/dev).
// A garantia cross-platform vem da CONFIGURAÇÃO de build (CROSS_PLATFORM_DETERMINISTIC +
// SIMD travado em SSE4, documentada e aceita pelo próprio Jolt), não de algo que este teste
// possa verificar sozinho sem uma segunda plataforma real para comparar.
//
// Este arquivo é compilado nas DUAS configurações (normal e AETHER_PHYSICS_DETERMINISTIC=ON)
// — os mesmos testes de reprodutibilidade run-to-run devem passar em ambas (determinismo
// run-to-run não é exclusividade do modo determinístico: o Jolt normal também é determinístico
// rodando na MESMA build/plataforma, só não entre plataformas DIFERENTES — ver README do Jolt
// vendorizado, "the simulation runs deterministically... replicate a simulation to a remote
// client"). O que muda entre os dois modos é a garantia adicional cross-platform, que este
// teste não consegue verificar sozinho.
#include "harness.h"
#include "physics/jolt_bridge.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace ae::test;

namespace {

// Cenário representativo: um cabo de corpos conectados por juntas Hinge com motor, caindo
// sobre uma rampa, com raycast contra o resultado final — mistura de subsistemas (corpos,
// juntas, motor, gravidade, contato) que exercita mais caminhos de código do solver que um
// teste de corpo isolado, aumentando a chance de pegar uma divergência real se houvesse uma.
struct SimulationSnapshot {
  std::vector<AetherVec3> positions;
  std::vector<AetherQuat> rotations;
  std::vector<AetherVec3> velocities;
};

AetherBodyHandle MakeBox(AetherPhysicsWorld *world, AetherVec3 halfExtent, AetherVec3 position, AetherMotionType motion) {
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Box;
  desc.shape.boxHalfExtent = halfExtent;
  desc.position = position;
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = motion;
  desc.friction = 0.4f;
  desc.restitution = 0.1f;
  return AetherPhysics_CreateBody(world, &desc);
}

SimulationSnapshot RunDeterminismScenario() {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);

  // Rampa inclinada (não um piso plano — quinas/ângulos exercitam mais do narrow-phase que
  // uma queda vertical trivial).
  const float rampAngle = 15.0f * 3.14159265f / 180.0f;
  AetherQuat rampRotation{0.0f, 0.0f, -std::sin(rampAngle * 0.5f), std::cos(rampAngle * 0.5f)};
  AetherBodyDesc rampDesc{};
  rampDesc.shape.kind = AetherShapeKind::Box;
  rampDesc.shape.boxHalfExtent = {10.0f, 0.5f, 10.0f};
  rampDesc.position = {0.0f, 0.0f, 0.0f};
  rampDesc.rotation = rampRotation;
  rampDesc.motionType = AetherMotionType::Static;
  rampDesc.friction = 0.4f;
  AetherPhysics_CreateBody(world, &rampDesc);

  // Um "cabo" de 4 caixas conectadas por juntas Hinge com motor — exercita corpos, juntas,
  // motor e contato com a rampa ao mesmo tempo.
  const int linkCount = 4;
  std::vector<AetherBodyHandle> links;
  AetherBodyHandle previous = AetherBodyHandle_Invalid;
  for (int i = 0; i < linkCount; ++i) {
    AetherVec3 pos{static_cast<float>(i) * 0.6f - 1.0f, 6.0f, 0.0f};
    AetherBodyHandle link = MakeBox(world, {0.25f, 0.1f, 0.1f}, pos, AetherMotionType::Dynamic);
    links.push_back(link);

    if (previous != AetherBodyHandle_Invalid) {
      AetherJointDesc jointDesc{};
      jointDesc.kind = AetherJointKind::Hinge;
      AetherVec3 midpoint{pos.x - 0.3f, pos.y, pos.z};
      jointDesc.point1 = midpoint;
      jointDesc.point2 = midpoint;
      jointDesc.axis1 = {0.0f, 0.0f, 1.0f};
      jointDesc.axis2 = {0.0f, 0.0f, 1.0f};
      jointDesc.limitsMin = -1.0f;
      jointDesc.limitsMax = 1.0f;
      jointDesc.motor.state = AetherMotorState::Velocity;
      jointDesc.motor.targetVelocity = 0.5f;
      jointDesc.motor.maxForceOrTorque = 20.0f;
      AetherPhysics_CreateJoint(world, previous, link, &jointDesc);
    }
    previous = link;
  }

  const float dt = 1.0f / 60.0f;
  for (int i = 0; i < 240; ++i) AetherPhysics_Step(world, dt, 1); // 4s

  SimulationSnapshot snapshot{};
  for (AetherBodyHandle link : links) {
    AetherVec3 position{};
    AetherQuat rotation{};
    AetherPhysics_GetTransform(world, link, &position, &rotation);
    snapshot.positions.push_back(position);
    snapshot.rotations.push_back(rotation);
    snapshot.velocities.push_back(AetherPhysics_GetLinearVelocity(world, link));
  }

  AetherPhysics_DestroyWorld(world);
  return snapshot;
}

bool BitExact(const SimulationSnapshot &a, const SimulationSnapshot &b) {
  if (a.positions.size() != b.positions.size()) return false;
  for (size_t i = 0; i < a.positions.size(); ++i) {
    if (std::memcmp(&a.positions[i], &b.positions[i], sizeof(AetherVec3)) != 0) return false;
    if (std::memcmp(&a.rotations[i], &b.rotations[i], sizeof(AetherQuat)) != 0) return false;
    if (std::memcmp(&a.velocities[i], &b.velocities[i], sizeof(AetherVec3)) != 0) return false;
  }
  return true;
}

} // namespace

AE_TEST(mesma_simulacao_rodada_duas_vezes_produz_resultado_bit_exato) {
  // Determinismo run-to-run: pré-requisito necessário (não suficiente sozinho) para
  // determinismo cross-platform — ver comentário de topo do arquivo.
  SimulationSnapshot first = RunDeterminismScenario();
  SimulationSnapshot second = RunDeterminismScenario();

  AE_EXPECT_TRUE(first.positions.size() == 4, "cenário deveria ter criado os 4 elos do cabo");
  AE_EXPECT_TRUE(BitExact(first, second), "duas execuções independentes da mesma simulação deveriam produzir resultado bit-exato (posição, rotação, velocidade)");
}

AE_TEST(mesma_simulacao_rodada_tres_vezes_produz_resultado_bit_exato_nas_tres) {
  // Roda uma terceira vez para descartar coincidência de duas execuções batendo por acaso
  // (não deveria acontecer com float determinístico, mas reforça a evidência).
  SimulationSnapshot first = RunDeterminismScenario();
  SimulationSnapshot second = RunDeterminismScenario();
  SimulationSnapshot third = RunDeterminismScenario();

  AE_EXPECT_TRUE(BitExact(first, second), "1ª e 2ª execuções deveriam ser bit-exatas");
  AE_EXPECT_TRUE(BitExact(second, third), "2ª e 3ª execuções deveriam ser bit-exatas");
}

AE_TEST(cenario_produz_movimento_real_nao_um_falso_positivo_de_corpos_parados) {
  // Guarda contra o teste de determinismo passar trivialmente porque nada se moveu (ex.: se
  // o cabo travou imóvel logo na criação por algum erro de geometria, "bit-exato" seria
  // verdade só porque o valor nunca mudou de default). Confirma que houve queda/movimento
  // real ao longo da simulação.
  SimulationSnapshot snapshot = RunDeterminismScenario();
  bool anyMoved = false;
  for (const AetherVec3 &pos : snapshot.positions) {
    if (pos.y < 5.5f) { anyMoved = true; break; } // caíram de y=6.0 inicial
  }
  AE_EXPECT_TRUE(anyMoved, "o cabo deveria ter caído/se movido de verdade durante a simulação, não ficado parado no ponto de criação");
}
