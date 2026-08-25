// Testes de correção da fronteira C ABI sobre o Jolt Physics
// (native/physics/jolt_bridge.h/.cpp — item 4.1.1 do plano).
//
// Diferente dos outros testes nativos (lógica pura, determinística por
// construção), estes testes exercitam uma simulação física real: os números
// esperados vêm de cinemática básica (queda livre, corpo em repouso sobre um
// piso) com margens de tolerância largas o suficiente para absorver o
// integrador do Jolt (semi-implícito, não é integração exata), mas estreitas
// o bastante para pegar um erro real de unidade/eixo/sinal na fronteira.
#include "harness.h"
#include "physics/jolt_bridge.h"

#include <cmath>

using namespace ae::test;

namespace {

bool near(float a, float b, float tolerance) { return std::fabs(a - b) <= tolerance; }

AetherBodyDesc BoxDesc(AetherVec3 halfExtent, AetherVec3 position, AetherMotionType motionType) {
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Box;
  desc.shape.boxHalfExtent = halfExtent;
  desc.position = position;
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = motionType;
  desc.friction = 0.5f;
  desc.restitution = 0.0f;
  return desc;
}

AetherBodyDesc SphereDesc(float radius, AetherVec3 position, AetherMotionType motionType) {
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Sphere;
  desc.shape.sphereRadius = radius;
  desc.position = position;
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = motionType;
  desc.friction = 0.5f;
  desc.restitution = 0.0f;
  return desc;
}

} // namespace

AE_TEST(mundo_cria_e_destroi_sem_deixar_estado_pendurado) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  AE_EXPECT_TRUE(world != nullptr, "criação de mundo com parâmetros válidos não pode falhar");
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(corpo_estatico_nao_se_move_sob_gravidade) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);

  AetherBodyDesc floorDesc = BoxDesc({10.0f, 0.5f, 10.0f}, {0.0f, 0.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle floor = AetherPhysics_CreateBody(world, &floorDesc);
  AE_EXPECT_TRUE(floor != AetherBodyHandle_Invalid, "criação do piso estático deveria ter sucesso");

  for (int i = 0; i < 120; ++i) {
    AetherPhysics_Step(world, 1.0f / 60.0f, 1);
  }

  AetherVec3 position{};
  AetherPhysics_GetTransform(world, floor, &position, nullptr);
  AE_EXPECT_TRUE(near(position.y, 0.0f, 0.0001f), "corpo estático não pode ser deslocado pela gravidade");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(esfera_dinamica_em_queda_livre_segue_cinematica_basica) {
  // Mundo grande o bastante e sem chão sob a esfera: nos primeiros instantes
  // de queda livre nenhuma colisão interfere, então a trajetória deve seguir
  // y(t) = y0 - 1/2 g t^2 dentro da tolerância do integrador.
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);

  AetherBodyDesc sphereDesc = SphereDesc(0.5f, {0.0f, 50.0f, 0.0f}, AetherMotionType::Dynamic);
  AetherBodyHandle sphere = AetherPhysics_CreateBody(world, &sphereDesc);
  AE_EXPECT_TRUE(sphere != AetherBodyHandle_Invalid, "criação da esfera dinâmica deveria ter sucesso");

  const float dt = 1.0f / 60.0f;
  const int steps = 30; // meio segundo — ainda bem longe do chão (y0=50)
  for (int i = 0; i < steps; ++i) {
    AetherPhysics_Step(world, dt, 1);
  }
  const float elapsed = dt * steps;

  AetherVec3 position{};
  AetherPhysics_GetTransform(world, sphere, &position, nullptr);
  const float expectedY = 50.0f - 0.5f * 9.81f * elapsed * elapsed;
  AE_EXPECT_TRUE(near(position.y, expectedY, 0.05f),
                 "queda livre deveria seguir y0 - 1/2 g t^2 dentro de uma margem pequena");
  AE_EXPECT_TRUE(position.x == 0.0f && position.z == 0.0f, "gravidade vertical não pode produzir deriva em x/z");

  AetherVec3 velocity = AetherPhysics_GetLinearVelocity(world, sphere);
  const float expectedVy = -9.81f * elapsed;
  // Tolerância mais larga que a da posição acima: por padrão o Jolt aplica um
  // amortecimento linear em todo corpo dinâmico (BodyCreationSettings::mLinearDamping
  // = 0.05, dv/dt = -c*v — ver Jolt/Physics/Body/BodyCreationSettings.h), então a
  // velocidade real converge para um pouco menos que -g*t mesmo em queda livre pura.
  // 0.15 cobre essa divergência esperada (medida ~0.06 m/s neste cenário) com folga,
  // mas continua muito menor que o erro que um bug real de eixo/sinal/unidade
  // produziria (esse seria da ordem de -g*t inteiro, não uma fração dele).
  AE_EXPECT_TRUE(near(velocity.y, expectedVy, 0.15f),
                 "velocidade vertical deveria seguir aproximadamente v = -g t (menos o amortecimento linear padrão do Jolt)");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(caixa_dinamica_assenta_sobre_piso_estatico_na_altura_esperada) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);

  const float floorHalfHeight = 0.5f;
  AetherBodyDesc floorDesc = BoxDesc({10.0f, floorHalfHeight, 10.0f}, {0.0f, 0.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle floor = AetherPhysics_CreateBody(world, &floorDesc);
  AE_EXPECT_TRUE(floor != AetherBodyHandle_Invalid, "criação do piso deveria ter sucesso");

  const float boxHalfHeight = 0.5f;
  AetherBodyDesc boxDesc = BoxDesc({0.5f, boxHalfHeight, 0.5f}, {0.0f, 5.0f, 0.0f}, AetherMotionType::Dynamic);
  AetherBodyHandle box = AetherPhysics_CreateBody(world, &boxDesc);
  AE_EXPECT_TRUE(box != AetherBodyHandle_Invalid, "criação da caixa dinâmica deveria ter sucesso");

  // Tempo generoso para a caixa cair, colidir, e o solver estabilizar o contato.
  const float dt = 1.0f / 60.0f;
  for (int i = 0; i < 300; ++i) { // 5s
    AetherPhysics_Step(world, dt, 1);
  }

  AetherVec3 position{};
  AetherPhysics_GetTransform(world, box, &position, nullptr);
  const float expectedRestY = floorHalfHeight + boxHalfHeight;
  AE_EXPECT_TRUE(near(position.y, expectedRestY, 0.02f),
                 "caixa deveria assentar sobre o piso na soma das meias-alturas");

  AetherVec3 velocity = AetherPhysics_GetLinearVelocity(world, box);
  AE_EXPECT_TRUE(near(velocity.y, 0.0f, 0.05f), "caixa em repouso sobre o piso não deveria continuar acelerando");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(raycast_acerta_corpo_no_caminho_e_ignora_corpo_fora_do_alcance) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);

  AetherBodyDesc floorDesc = BoxDesc({10.0f, 0.5f, 10.0f}, {0.0f, 0.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle floor = AetherPhysics_CreateBody(world, &floorDesc);
  AE_EXPECT_TRUE(floor != AetherBodyHandle_Invalid, "criação do piso deveria ter sucesso");

  // Raio de cima para baixo, alcance suficiente para acertar o topo do piso (y=0.5).
  AetherBodyHandle hitBody = AetherBodyHandle_Invalid;
  float hitFraction = -1.0f;
  ae::i32 hit = AetherPhysics_RayCastClosest(world, {0.0f, 10.0f, 0.0f}, {0.0f, -20.0f, 0.0f}, &hitBody, &hitFraction);
  AE_EXPECT_TRUE(hit == 1, "raio de cima para baixo deveria acertar o piso");
  AE_EXPECT_TRUE(hitBody == floor, "corpo acertado deveria ser o piso criado acima");
  // origem y=10, direção -20 (alcance até y=-10); topo do piso em y=0.5 -> fração (10-0.5)/20 = 0.475
  AE_EXPECT_TRUE(near(hitFraction, 0.475f, 0.01f), "fração do impacto deveria corresponder ao topo do piso");

  // Raio cujo alcance termina ANTES de chegar ao piso não pode acertar nada.
  AetherBodyHandle missBody = AetherBodyHandle_Invalid;
  float missFraction = -1.0f;
  ae::i32 miss = AetherPhysics_RayCastClosest(world, {0.0f, 10.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, &missBody, &missFraction);
  AE_EXPECT_TRUE(miss == 0, "raio curto demais para alcançar o piso não pode reportar acerto");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(set_e_get_linear_velocity_fazem_round_trip_exato) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16); // sem gravidade: velocidade não deveria mudar sozinha

  AetherBodyDesc sphereDesc = SphereDesc(0.5f, {0.0f, 0.0f, 0.0f}, AetherMotionType::Dynamic);
  AetherBodyHandle sphere = AetherPhysics_CreateBody(world, &sphereDesc);
  AE_EXPECT_TRUE(sphere != AetherBodyHandle_Invalid, "criação da esfera deveria ter sucesso");

  AetherPhysics_SetLinearVelocity(world, sphere, {3.0f, 4.0f, -5.0f});
  AetherVec3 velocity = AetherPhysics_GetLinearVelocity(world, sphere);
  AE_EXPECT_TRUE(near(velocity.x, 3.0f, 0.0001f) && near(velocity.y, 4.0f, 0.0001f) && near(velocity.z, -5.0f, 0.0001f),
                 "velocidade lida deveria ser exatamente a velocidade escrita, antes de qualquer passo de simulação");

  AetherPhysics_Step(world, 1.0f / 60.0f, 1);
  AetherVec3 position{};
  AetherPhysics_GetTransform(world, sphere, &position, nullptr);
  AE_EXPECT_TRUE(position.x > 0.0f, "corpo com velocidade em x positivo deveria ter se movido em x positivo após um passo");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(handle_invalido_e_mundo_nulo_sao_tratados_sem_crash) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);

  // Nenhuma destas chamadas pode crashar nem exigir verificação prévia do chamador —
  // é a mesma disciplina defensiva do resto da engine (ver core/assert.cpp e ECS/World.cs).
  AetherPhysics_DestroyBody(world, AetherBodyHandle_Invalid);
  AetherPhysics_Step(nullptr, 1.0f / 60.0f, 1);
  AetherVec3 position{1.0f, 1.0f, 1.0f};
  AetherPhysics_GetTransform(world, AetherBodyHandle_Invalid, &position, nullptr);
  AE_EXPECT_TRUE(position.x == 1.0f, "handle inválido não deveria escrever na saída");
  AE_EXPECT_TRUE(AetherPhysics_IsActive(world, AetherBodyHandle_Invalid) == 0, "handle inválido nunca está ativo");
  AE_EXPECT_TRUE(AetherPhysics_IsActive(nullptr, AetherBodyHandle_Invalid) == 0, "mundo nulo nunca reporta corpo ativo");

  AetherBodyHandle outBody = AetherBodyHandle_Invalid;
  float outFraction = 0.0f;
  ae::i32 hit = AetherPhysics_RayCastClosest(nullptr, {0, 0, 0}, {0, -1, 0}, &outBody, &outFraction);
  AE_EXPECT_TRUE(hit == 0, "raycast em mundo nulo deveria devolver 'sem acerto', não crashar");

  AetherPhysics_DestroyWorld(world);
  AetherPhysics_DestroyWorld(nullptr); // destruir mundo nulo é um no-op válido
}
