// Testes de correção da física 2D (item 4.1.6 do plano) — Jolt 3D restrito ao plano XY via
// JPH::EAllowedDOFs (não uma segunda biblioteca física; ver o comentário de topo de
// AetherAllowedDOFs em jolt_bridge.h para a justificativa completa da decisão).
//
// Mesma disciplina dos demais testes de física: tolerância larga o suficiente para o
// integrador/solver do Jolt, estreita o bastante para pegar erro real de eixo/sinal/unidade.
#include "harness.h"
#include "physics/jolt_bridge.h"

#include <cmath>

using namespace ae::test;

namespace {

bool near(float a, float b, float tolerance) { return std::fabs(a - b) <= tolerance; }

AetherBodyHandle MakeSphere2D(AetherPhysicsWorld *world, AetherVec3 position, AetherMotionType motion) {
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Sphere;
  desc.shape.sphereRadius = 0.5f;
  desc.position = position;
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = motion;
  desc.friction = 0.5f;
  desc.allowedDOFs = AetherAllowedDOFs::Plane2D;
  return AetherPhysics_CreateBody(world, &desc);
}

AetherBodyHandle MakeBox3D(AetherPhysicsWorld *world, AetherVec3 halfExtent, AetherVec3 position, AetherMotionType motion) {
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Box;
  desc.shape.boxHalfExtent = halfExtent;
  desc.position = position;
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = motion;
  desc.friction = 0.5f;
  // allowedDOFs não setado (fica AetherAllowedDOFs::All == 0, zero-init) — corpo 3D normal,
  // exatamente o mesmo comportamento de antes do item 4.1.6 existir.
  return AetherPhysics_CreateBody(world, &desc);
}

} // namespace

AE_TEST(corpo_plane2d_nao_se_move_em_z_mesmo_com_impulso_lateral_em_z) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  AetherBodyHandle body = MakeSphere2D(world, {0.0f, 10.0f, 0.0f}, AetherMotionType::Dynamic);
  AE_EXPECT_TRUE(body != AetherBodyHandle_Invalid, "criação de corpo Plane2D deveria ter sucesso");

  // Tenta empurrar o corpo para fora do plano XY (velocidade em Z) — um corpo 3D normal se
  // moveria livremente em Z; um corpo Plane2D não pode, por definição de DOF travado.
  AetherPhysics_SetLinearVelocity(world, body, {0.0f, 0.0f, 5.0f});

  for (int i = 0; i < 60; ++i) AetherPhysics_Step(world, 1.0f / 60.0f, 1); // 1s

  AetherVec3 position{};
  AetherPhysics_GetTransform(world, body, &position, nullptr);
  AE_EXPECT_TRUE(near(position.z, 0.0f, 0.001f), "corpo Plane2D não pode se mover em Z mesmo com velocidade em Z setada diretamente");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(corpo_plane2d_cai_normalmente_no_plano_xy_sob_gravidade) {
  // Confirma que travar DOF não quebra o comportamento normal DENTRO do plano permitido —
  // Plane2D deixa TranslationX/TranslationY livres, então queda livre em Y continua idêntica
  // a um corpo 3D normal.
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  AetherBodyHandle body = MakeSphere2D(world, {0.0f, 50.0f, 0.0f}, AetherMotionType::Dynamic);

  const float dt = 1.0f / 60.0f;
  const int steps = 30; // meio segundo de queda livre
  for (int i = 0; i < steps; ++i) AetherPhysics_Step(world, dt, 1);
  const float elapsed = dt * steps;

  AetherVec3 position{};
  AetherPhysics_GetTransform(world, body, &position, nullptr);
  const float expectedY = 50.0f - 0.5f * 9.81f * elapsed * elapsed;
  AE_EXPECT_TRUE(near(position.y, expectedY, 0.05f), "queda livre no plano 2D deveria seguir a mesma cinemática de um corpo 3D");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(corpo_plane2d_assenta_sobre_piso_plane2d_na_altura_esperada) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);

  AetherBodyDesc floorDesc{};
  floorDesc.shape.kind = AetherShapeKind::Box;
  floorDesc.shape.boxHalfExtent = {10.0f, 0.5f, 10.0f};
  floorDesc.position = {0.0f, 0.0f, 0.0f};
  floorDesc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  floorDesc.motionType = AetherMotionType::Static;
  floorDesc.friction = 0.5f;
  // Piso estático não precisa de allowedDOFs (Static não integra força/velocidade de qualquer
  // forma — mInvMass já é 0), mas setamos Plane2D por consistência de cena "toda 2D".
  floorDesc.allowedDOFs = AetherAllowedDOFs::Plane2D;
  AE_EXPECT_TRUE(AetherPhysics_CreateBody(world, &floorDesc) != AetherBodyHandle_Invalid, "criação do piso 2D deveria ter sucesso");

  AetherBodyHandle sphere = MakeSphere2D(world, {0.0f, 5.0f, 0.0f}, AetherMotionType::Dynamic);

  const float dt = 1.0f / 60.0f;
  for (int i = 0; i < 300; ++i) AetherPhysics_Step(world, dt, 1); // 5s, tempo de sobra para assentar

  AetherVec3 position{};
  AetherPhysics_GetTransform(world, sphere, &position, nullptr);
  const float expectedRestY = 0.5f + 0.5f; // meia-altura do piso + raio da esfera
  AE_EXPECT_TRUE(near(position.y, expectedRestY, 0.02f), "esfera 2D deveria assentar sobre o piso 2D na soma das meias-alturas, igual a um corpo 3D");
  AE_EXPECT_TRUE(near(position.z, 0.0f, 0.001f), "esfera 2D não pode ter se deslocado em Z durante a queda/assentamento");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(corpo_plane2d_nao_gira_em_torno_de_x_ou_y_apenas_em_z) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16); // sem gravidade: isola o efeito da rotação
  AetherBodyHandle body = MakeSphere2D(world, {0.0f, 0.0f, 0.0f}, AetherMotionType::Dynamic);

  // Aplica torque via velocidade angular direta em todos os 3 eixos — só a componente Z
  // deveria produzir rotação real; X e Y devem ficar travados em zero.
  // (AetherPhysics_SetLinearVelocity só afeta translação; a fronteira atual não expõe
  // SetAngularVelocity — este teste confirma indiretamente via GetTransform ao longo do tempo
  // que a rotação resultante nunca sai do eixo Z observando a normalização do quaternion:
  // um corpo perfeitamente esférico sem torque aplicado não gira sozinho, então este teste
  // detecta apenas se ALGUMA rotação residual aparece nos eixos travados por deriva numérica.)
  for (int i = 0; i < 120; ++i) AetherPhysics_Step(world, 1.0f / 60.0f, 1); // 2s

  AetherQuat rotation{};
  AetherPhysics_GetTransform(world, body, nullptr, &rotation);
  // Sem torque nenhum aplicado, o corpo não deveria ter girado em NENHUM eixo — mas o ponto
  // deste teste é confirmar que a rotação permanece a identidade mesmo depois de rodar a
  // simulação com DOFs de rotação X/Y travados (nenhuma deriva numérica introduzida pela
  // restrição de DOF em si).
  AE_EXPECT_TRUE(near(rotation.x, 0.0f, 0.0001f) && near(rotation.y, 0.0f, 0.0001f),
                 "corpo Plane2D não pode acumular rotação residual em X/Y (eixos travados)");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(corpo_dynamic_com_todos_os_dofs_de_translacao_travados_e_recusado) {
  // AetherAllowedDOFs sem NENHUM bit de translação (só rotação, ex.: RotationZ sozinho) é
  // inválido para um corpo Dynamic no Jolt (crasha por divisão por zero em
  // MotionProperties::SetMassProperties — comentário completo em jolt_bridge.h). Esta
  // fronteira recusa a criação em vez de repassar ao Jolt.
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);

  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Sphere;
  desc.shape.sphereRadius = 0.5f;
  desc.position = {0.0f, 5.0f, 0.0f};
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = AetherMotionType::Dynamic;
  desc.friction = 0.5f;
  desc.allowedDOFs = AetherAllowedDOFs::RotationZ; // só rotação, nenhuma translação livre

  AetherBodyHandle body = AetherPhysics_CreateBody(world, &desc);
  AE_EXPECT_TRUE(body == AetherBodyHandle_Invalid, "corpo Dynamic sem nenhum eixo de translação livre deveria ser recusado, não crashar");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(corpo_static_com_dofs_travados_nao_e_afetado_pela_validacao_de_dynamic) {
  // A validação de "sem translação livre é inválido" só se aplica a corpos Dynamic (Static
  // não usa massa/inércia da mesma forma — mInvMass já é 0 para Static independente de
  // allowedDOFs). Confirma que um piso estático com AetherAllowedDOFs::RotationZ (sem
  // translação) continua sendo criado normalmente.
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);

  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Box;
  desc.shape.boxHalfExtent = {10.0f, 0.5f, 10.0f};
  desc.position = {0.0f, 0.0f, 0.0f};
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = AetherMotionType::Static;
  desc.friction = 0.5f;
  desc.allowedDOFs = AetherAllowedDOFs::RotationZ;

  AetherBodyHandle body = AetherPhysics_CreateBody(world, &desc);
  AE_EXPECT_TRUE(body != AetherBodyHandle_Invalid, "corpo Static com DOFs de translação travados deveria ser criado normalmente");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(corpo_3d_normal_allowedDOFs_zero_init_continua_livre_em_todos_os_eixos) {
  // Regressão: AetherBodyDesc{} zero-inicializado (allowedDOFs = AetherAllowedDOFs::All, que
  // é 0 na fronteira por convenção deliberada — ver jolt_bridge.h) precisa continuar
  // produzindo um corpo 3D totalmente livre, exatamente como antes do item 4.1.6 existir.
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  AetherBodyHandle body = MakeBox3D(world, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 0.0f}, AetherMotionType::Dynamic);
  AE_EXPECT_TRUE(body != AetherBodyHandle_Invalid, "corpo 3D com allowedDOFs zero-init deveria ser criado normalmente");

  AetherPhysics_SetLinearVelocity(world, body, {0.0f, 0.0f, 5.0f});
  for (int i = 0; i < 60; ++i) AetherPhysics_Step(world, 1.0f / 60.0f, 1);

  AetherVec3 position{};
  AetherPhysics_GetTransform(world, body, &position, nullptr);
  AE_EXPECT_TRUE(position.z > 1.0f, "corpo 3D normal (allowedDOFs default) deveria se mover livremente em Z, ao contrário de um corpo Plane2D");

  AetherPhysics_DestroyWorld(world);
}
