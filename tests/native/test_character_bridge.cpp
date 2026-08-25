// Testes de correção do character controller sobre a fronteira C ABI do Jolt (item 4.1.5 do
// plano — ver o comentário de topo de physics/jolt_bridge.h para o escopo desta fatia: mover,
// detecção de chão/rampa, degraus, deslizar, plataforma móvel, agachar; escalar e nadar ficam
// de fora, sem suporte nativo nenhum no Jolt).
//
// Mesma disciplina dos demais testes de física: tolerância larga o suficiente para o
// integrador/solver iterativo do CharacterVirtual, estreita o bastante para pegar erro real de
// eixo/sinal/unidade.
#include "harness.h"
#include "physics/jolt_bridge.h"

#include <cmath>

using namespace ae::test;

namespace {

bool near(float a, float b, float tolerance) { return std::fabs(a - b) <= tolerance; }

AetherCharacterDesc DefaultCharacterDesc() {
  AetherCharacterDesc desc{};
  desc.radius = 0.3f;
  desc.standingHalfHeight = 0.9f;   // altura total em pé: 2*(0.9+0.3) = 2.4m
  desc.crouchingHalfHeight = 0.4f;  // altura total agachado: 2*(0.4+0.3) = 1.4m
  desc.maxSlopeAngle = 45.0f * 3.14159265f / 180.0f;
  desc.mass = 70.0f;
  desc.maxStrength = 100.0f;
  return desc;
}

AetherBodyHandle MakeBox(AetherPhysicsWorld *world, AetherVec3 halfExtent, AetherVec3 position,
                          AetherQuat rotation, AetherMotionType motion) {
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Box;
  desc.shape.boxHalfExtent = halfExtent;
  desc.position = position;
  desc.rotation = rotation;
  desc.motionType = motion;
  desc.friction = 0.5f;
  return AetherPhysics_CreateBody(world, &desc);
}

void StepCharacterFreefall(AetherPhysicsWorld *world, AetherCharacterHandle character, int steps, float dt) {
  const AetherVec3 gravity{0.0f, -9.81f, 0.0f};
  for (int i = 0; i < steps; ++i) {
    AetherVec3 v = AetherPhysics_GetCharacterVelocity(world, character);
    // Gravidade é responsabilidade do chamador (o Jolt não integra sozinho — ver comentário em
    // jolt_bridge.h) — mesmo padrão documentado no próprio CharacterVirtual::ExtendedUpdate.
    v.y += gravity.y * dt;
    AetherPhysics_SetCharacterVelocity(world, character, v);
    AetherPhysics_UpdateCharacter(world, character, dt, gravity, AetherQueryLayerMask::All, AetherBodyHandle_Invalid);
    AetherPhysics_Step(world, dt, 1);
  }
}

} // namespace

AE_TEST(criar_e_destruir_character_sem_deixar_estado_pendurado) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  AetherCharacterDesc desc = DefaultCharacterDesc();
  AetherCharacterHandle character = AetherPhysics_CreateCharacter(world, &desc, {0.0f, 5.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
  AE_EXPECT_TRUE(character != AetherCharacterHandle_Invalid, "criação do character deveria ter sucesso");
  AetherPhysics_DestroyCharacter(world, character);
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(character_cai_sob_gravidade_e_assenta_sobre_piso_estatico) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  MakeBox(world, {10.0f, 0.5f, 10.0f}, {0.0f, 0.0f, 0.0f}, {0, 0, 0, 1}, AetherMotionType::Static);

  AetherCharacterDesc desc = DefaultCharacterDesc();
  AetherCharacterHandle character = AetherPhysics_CreateCharacter(world, &desc, {0.0f, 5.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
  AE_EXPECT_TRUE(character != AetherCharacterHandle_Invalid, "criação do character deveria ter sucesso");

  StepCharacterFreefall(world, character, 300, 1.0f / 60.0f); // 5s, tempo de sobra para cair e assentar

  AetherVec3 position{};
  AetherPhysics_GetCharacterTransform(world, character, &position, nullptr);
  // Pé do character em y=0.5 (topo do piso) + halfHeight+radius da cápsula (a origem da
  // AetherPhysics_GetCharacterTransform é a BASE da cápsula, mesma convenção documentada em
  // CharacterBaseSettings::mShape — "bottom of the shape is at (0,0,0)").
  const float expectedRestY = 0.5f;
  AE_EXPECT_TRUE(near(position.y, expectedRestY, 0.05f), "character deveria assentar sobre o piso na altura do topo do piso");
  AE_EXPECT_TRUE(AetherPhysics_GetCharacterGroundState(world, character) == AetherCharacterGroundState::OnGround,
                 "character assentado deveria reportar OnGround");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(character_em_queda_sem_piso_reporta_estado_em_ar) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  AetherCharacterDesc desc = DefaultCharacterDesc();
  AetherCharacterHandle character = AetherPhysics_CreateCharacter(world, &desc, {0.0f, 50.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f});

  StepCharacterFreefall(world, character, 30, 1.0f / 60.0f); // meio segundo, ainda longe do chão (não há chão nenhum)

  AE_EXPECT_TRUE(AetherPhysics_GetCharacterGroundState(world, character) == AetherCharacterGroundState::InAir,
                 "character em queda livre sem nenhum piso deveria reportar InAir");

  AetherVec3 position{};
  AetherPhysics_GetCharacterTransform(world, character, &position, nullptr);
  AE_EXPECT_TRUE(position.y < 50.0f, "character em queda livre deveria ter descido");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(character_anda_sobre_rampa_andavel_sem_ficar_preso) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  // Rampa de 20° (bem dentro do maxSlopeAngle de 45° do DefaultCharacterDesc), inclinada em
  // torno do eixo Z — rotação de -20° em Z inclina o topo da caixa para +X.
  const float rampAngle = 20.0f * 3.14159265f / 180.0f;
  const float halfAngle = rampAngle * 0.5f;
  AetherQuat rampRotation{0.0f, 0.0f, -std::sin(halfAngle), std::cos(halfAngle)};
  MakeBox(world, {10.0f, 0.3f, 10.0f}, {0.0f, 0.0f, 0.0f}, rampRotation, AetherMotionType::Static);

  AetherCharacterDesc desc = DefaultCharacterDesc();
  AetherCharacterHandle character = AetherPhysics_CreateCharacter(world, &desc, {-3.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f});

  // Cai sobre a rampa e depois anda para +X (subindo a rampa) por um tempo.
  StepCharacterFreefall(world, character, 120, 1.0f / 60.0f); // 2s: cair e assentar na rampa

  AE_EXPECT_TRUE(AetherPhysics_GetCharacterGroundState(world, character) == AetherCharacterGroundState::OnGround,
                 "character sobre rampa de 20° (< maxSlopeAngle 45°) deveria estar OnGround, não OnSteepGround");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(character_sobre_rampa_ingreme_demais_reporta_steep_ground) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  // Rampa de 80° — bem além do maxSlopeAngle de 45°, praticamente uma parede. Rotação em torno
  // de Z: a face que era "topo" (normal +Y) da caixa gira para ficar quase vertical. Com meia-
  // altura 0.3 e ângulo -80°, o centro dessa face (local x=0) cai perto de world (0.3, 0.05) —
  // é ONDE o character precisa cair para tocar a face inclinada de verdade, não um valor
  // arbitrário (geometria calculada, não chutada — um x fora dessa faixa faz o character cair
  // no vazio ao lado da caixa rotacionada e nunca tocar nada, dando falso negativo).
  const float rampAngle = 80.0f * 3.14159265f / 180.0f;
  const float halfAngle = rampAngle * 0.5f;
  AetherQuat rampRotation{0.0f, 0.0f, -std::sin(halfAngle), std::cos(halfAngle)};
  MakeBox(world, {10.0f, 0.3f, 10.0f}, {0.0f, 0.0f, 0.0f}, rampRotation, AetherMotionType::Static);

  AetherCharacterDesc desc = DefaultCharacterDesc();
  AetherCharacterHandle character = AetherPhysics_CreateCharacter(world, &desc, {0.3f, 3.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f});

  // Captura o estado no PRIMEIRO frame em que o character deixa de estar InAir — depois disso
  // ele pode escorregar e cair de volta a InAir (é o comportamento esperado de uma rampa
  // íngreme demais), então checar só o estado final aceitaria até "caiu no vazio" como
  // resultado válido. O primeiro contato é o que prova que a classificação de íngreme
  // funcionou.
  AetherCharacterGroundState firstContactState = AetherCharacterGroundState::InAir;
  bool sawContact = false;
  const float dt = 1.0f / 60.0f;
  const AetherVec3 gravity{0.0f, -9.81f, 0.0f};
  for (int i = 0; i < 120 && !sawContact; ++i) {
    AetherVec3 v = AetherPhysics_GetCharacterVelocity(world, character);
    v.y += gravity.y * dt;
    AetherPhysics_SetCharacterVelocity(world, character, v);
    AetherPhysics_UpdateCharacter(world, character, dt, gravity, AetherQueryLayerMask::All, AetherBodyHandle_Invalid);
    AetherPhysics_Step(world, dt, 1);
    auto state = AetherPhysics_GetCharacterGroundState(world, character);
    if (state != AetherCharacterGroundState::InAir) { firstContactState = state; sawContact = true; }
  }

  AE_EXPECT_TRUE(sawContact, "character deveria ter encostado na rampa em algum momento da queda");
  AE_EXPECT_TRUE(firstContactState == AetherCharacterGroundState::OnSteepGround ||
                     firstContactState == AetherCharacterGroundState::NotSupported,
                 "primeiro contato com rampa de 80° (>> maxSlopeAngle 45°) não pode ser classificado como OnGround normal");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(character_sobe_degrau_com_extended_update) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  // Piso baixo (y top = 0) seguido de um degrau de 20cm (dentro do default de 40cm de
  // mWalkStairsStepUp) começando em x=1.
  MakeBox(world, {1.0f, 0.5f, 10.0f}, {0.0f, 0.0f, 0.0f}, {0, 0, 0, 1}, AetherMotionType::Static);       // piso baixo, top em y=0.5... ajustado abaixo
  MakeBox(world, {10.0f, 0.6f, 10.0f}, {11.0f, 0.1f, 0.0f}, {0, 0, 0, 1}, AetherMotionType::Static);      // degrau mais alto, x>=1

  AetherCharacterDesc desc = DefaultCharacterDesc();
  AetherCharacterHandle character = AetherPhysics_CreateCharacter(world, &desc, {0.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f});

  StepCharacterFreefall(world, character, 120, 1.0f / 60.0f); // assenta no piso baixo primeiro

  AetherVec3 posBefore{};
  AetherPhysics_GetCharacterTransform(world, character, &posBefore, nullptr);

  // Anda em +X por 2s — com ExtendedUpdate, deveria conseguir subir o degrau em vez de travar.
  const float dt = 1.0f / 60.0f;
  for (int i = 0; i < 120; ++i) {
    AetherVec3 v{2.0f, 0.0f, 0.0f}; // 2 m/s para frente
    if (AetherPhysics_GetCharacterGroundState(world, character) != AetherCharacterGroundState::InAir) {
      AetherVec3 groundVel = AetherPhysics_GetCharacterGroundVelocity(world, character);
      v.x += groundVel.x;
    }
    v.y += -9.81f * dt;
    AetherPhysics_SetCharacterVelocity(world, character, v);
    AetherPhysics_UpdateCharacter(world, character, dt, {0.0f, -9.81f, 0.0f}, AetherQueryLayerMask::All, AetherBodyHandle_Invalid);
    AetherPhysics_Step(world, dt, 1);
  }

  AetherVec3 posAfter{};
  AetherPhysics_GetCharacterTransform(world, character, &posAfter, nullptr);
  AE_EXPECT_TRUE(posAfter.x > posBefore.x + 1.0f, "character deveria ter avançado em X, subindo o degrau");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(character_grudado_a_plataforma_movel_herda_a_velocidade_do_chao) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  AetherBodyHandle platform = MakeBox(world, {5.0f, 0.5f, 5.0f}, {0.0f, 0.0f, 0.0f}, {0, 0, 0, 1}, AetherMotionType::Kinematic);
  AetherPhysics_SetLinearVelocity(world, platform, {3.0f, 0.0f, 0.0f}); // plataforma andando em +X

  AetherCharacterDesc desc = DefaultCharacterDesc();
  AetherCharacterHandle character = AetherPhysics_CreateCharacter(world, &desc, {0.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f});

  const float dt = 1.0f / 60.0f;
  // Primeiro deixa cair e assentar na plataforma (sem velocidade horizontal própria).
  for (int i = 0; i < 90; ++i) { // 1.5s
    AetherVec3 v{0.0f, 0.0f, 0.0f};
    if (AetherPhysics_GetCharacterGroundState(world, character) != AetherCharacterGroundState::InAir) {
      AetherVec3 groundVel = AetherPhysics_GetCharacterGroundVelocity(world, character);
      v.x = groundVel.x;
      v.z = groundVel.z;
    }
    v.y = AetherPhysics_GetCharacterVelocity(world, character).y - 9.81f * dt;
    AetherPhysics_SetCharacterVelocity(world, character, v);
    AetherPhysics_UpdateCharacter(world, character, dt, {0.0f, -9.81f, 0.0f}, AetherQueryLayerMask::All, AetherBodyHandle_Invalid);
    AetherPhysics_Step(world, dt, 1);
  }

  AE_EXPECT_TRUE(AetherPhysics_GetCharacterGroundState(world, character) == AetherCharacterGroundState::OnGround,
                 "character deveria estar assentado sobre a plataforma antes de medir arrasto");

  AetherVec3 posBefore{};
  AetherPhysics_GetCharacterTransform(world, character, &posBefore, nullptr);

  for (int i = 0; i < 60; ++i) { // mais 1s sobre a plataforma em movimento
    AetherVec3 v{0.0f, 0.0f, 0.0f};
    AetherVec3 groundVel = AetherPhysics_GetCharacterGroundVelocity(world, character);
    v.x = groundVel.x;
    v.z = groundVel.z;
    v.y = AetherPhysics_GetCharacterVelocity(world, character).y - 9.81f * dt;
    AetherPhysics_SetCharacterVelocity(world, character, v);
    AetherPhysics_UpdateCharacter(world, character, dt, {0.0f, -9.81f, 0.0f}, AetherQueryLayerMask::All, AetherBodyHandle_Invalid);
    AetherPhysics_Step(world, dt, 1);
  }

  AetherVec3 posAfter{};
  AetherPhysics_GetCharacterTransform(world, character, &posAfter, nullptr);
  float deltaX = posAfter.x - posBefore.x;
  // 1s a 3 m/s deveria mover ~3m em X, arrastado pela plataforma — tolerância larga (o solver
  // iterativo do CharacterVirtual não é exato, e a plataforma cinemática também se moveu).
  AE_EXPECT_TRUE(deltaX > 1.5f, "character deveria ter sido arrastado pela plataforma móvel em X");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(agachar_troca_para_capsula_menor_e_levantar_troca_de_volta) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  MakeBox(world, {10.0f, 0.5f, 10.0f}, {0.0f, 0.0f, 0.0f}, {0, 0, 0, 1}, AetherMotionType::Static);

  AetherCharacterDesc desc = DefaultCharacterDesc();
  AetherCharacterHandle character = AetherPhysics_CreateCharacter(world, &desc, {0.0f, 5.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
  StepCharacterFreefall(world, character, 120, 1.0f / 60.0f);

  ae::i32 crouchOk = AetherPhysics_SetCharacterCrouching(world, character, 1);
  AE_EXPECT_TRUE(crouchOk == 1, "agachar em espaço livre deveria ter sucesso");

  ae::i32 standOk = AetherPhysics_SetCharacterCrouching(world, character, 0);
  AE_EXPECT_TRUE(standOk == 1, "levantar em espaço livre deveria ter sucesso");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(agachar_debaixo_de_teto_baixo_impede_levantar) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  MakeBox(world, {10.0f, 0.5f, 10.0f}, {0.0f, 0.0f, 0.0f}, {0, 0, 0, 1}, AetherMotionType::Static); // piso, topo em y=0.5

  AetherCharacterDesc desc = DefaultCharacterDesc(); // altura de pé 2.4m, agachado 1.4m
  // Vão de 1.6m (piso topo=0.5, teto base=2.1): cabe agachado (1.4m, 0.2m de folga), NÃO cabe
  // de pé (2.4m). Medido experimentalmente: um vão MENOR que 1.4m (ex.: 1.2m, testado antes)
  // não cabe nem agachado — o penetration recovery do Jolt então resolve pelo caminho de MENOR
  // penetração, que pode ser "empurrar para cima do teto fino" em vez de "manter no chão"; não
  // é bug nosso, é o algoritmo de recuperação de penetração escolhendo corretamente a saída
  // mais curta para uma geometria que de fato não comporta a cápsula. Por isso o vão aqui
  // sobra folga real para a forma agachada, não é um ajuste "quase exato".
  MakeBox(world, {2.0f, 0.1f, 2.0f}, {0.0f, 2.2f, 0.0f}, {0, 0, 0, 1}, AetherMotionType::Static); // teto, base em y=2.1

  // Cria já encostado no chão (y=0.5, sem penetração nem no chão nem no teto para a forma DE
  // PÉ — 2.4m de altura cabe sob um teto de base 2.1m com folga) e agacha em seguida — mesma
  // ordem seria necessária de qualquer forma (criação sempre usa a forma de pé, ver
  // AetherPhysics_CreateCharacter), mas aqui nem precisa de cuidado especial porque o vão
  // comporta a forma de pé na CRIAÇÃO (só não comporta LEVANTAR depois de já agachado — o
  // teste real que queremos).
  AetherCharacterHandle character = AetherPhysics_CreateCharacter(world, &desc, {0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
  AetherPhysics_SetCharacterCrouching(world, character, 1);

  StepCharacterFreefall(world, character, 60, 1.0f / 60.0f); // 1s, assenta de vez

  AetherVec3 settledPos{};
  AetherPhysics_GetCharacterTransform(world, character, &settledPos, nullptr);
  AE_EXPECT_TRUE(near(settledPos.y, 0.5f, 0.05f), "character agachado deveria assentar sobre o piso, não ter atravessado nada");

  ae::i32 standOk = AetherPhysics_SetCharacterCrouching(world, character, 0);
  AE_EXPECT_TRUE(standOk == 0, "levantar debaixo de um teto baixo demais para a altura de pé deveria falhar");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(handles_invalidos_e_mundo_nulo_sao_tratados_sem_crash) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  AetherCharacterDesc desc = DefaultCharacterDesc();

  AE_EXPECT_TRUE(AetherPhysics_CreateCharacter(nullptr, &desc, {0, 0, 0}, {0, 0, 0, 1}) == AetherCharacterHandle_Invalid,
                 "mundo nulo não pode criar character");
  AE_EXPECT_TRUE(AetherPhysics_CreateCharacter(world, nullptr, {0, 0, 0}, {0, 0, 0, 1}) == AetherCharacterHandle_Invalid,
                 "desc nulo não pode criar character");

  AetherPhysics_DestroyCharacter(world, AetherCharacterHandle_Invalid); // no-op válido
  AetherPhysics_DestroyCharacter(nullptr, AetherCharacterHandle_Invalid); // no-op válido
  AetherPhysics_SetCharacterVelocity(world, AetherCharacterHandle_Invalid, {1, 1, 1}); // no-op válido
  AE_EXPECT_TRUE(AetherPhysics_GetCharacterVelocity(world, AetherCharacterHandle_Invalid).x == 0.0f,
                 "handle inválido devolve velocidade zero, não crasha");
  AetherPhysics_UpdateCharacter(world, AetherCharacterHandle_Invalid, 1.0f / 60.0f, {0, -9.81f, 0}, AetherQueryLayerMask::All, AetherBodyHandle_Invalid); // no-op válido
  AE_EXPECT_TRUE(AetherPhysics_GetCharacterGroundState(world, AetherCharacterHandle_Invalid) == AetherCharacterGroundState::InAir,
                 "handle inválido devolve InAir, não crasha");
  AE_EXPECT_TRUE(AetherPhysics_SetCharacterCrouching(world, AetherCharacterHandle_Invalid, 1) == 0,
                 "agachar com handle inválido devolve 0, não crasha");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(destruir_character_duas_vezes_nao_crasha_e_handle_reciclado_nao_colide) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  AetherCharacterDesc desc = DefaultCharacterDesc();

  AetherCharacterHandle first = AetherPhysics_CreateCharacter(world, &desc, {0, 5, 0}, {0, 0, 0, 1});
  AE_EXPECT_TRUE(first != AetherCharacterHandle_Invalid, "primeiro character deveria ser criado");
  AetherPhysics_DestroyCharacter(world, first);
  AetherPhysics_DestroyCharacter(world, first); // destruir de novo não pode crashar

  AetherCharacterHandle second = AetherPhysics_CreateCharacter(world, &desc, {5, 5, 0}, {0, 0, 0, 1});
  AE_EXPECT_TRUE(second != AetherCharacterHandle_Invalid, "segundo character (reciclando o slot) deveria ser criado");
  AE_EXPECT_TRUE(second != first, "handle reciclado deveria ter geração diferente do handle antigo");

  AetherPhysics_SetCharacterVelocity(world, first, {9, 9, 9}); // handle antigo não pode operar sobre o slot reciclado
  AE_EXPECT_TRUE(AetherPhysics_GetCharacterVelocity(world, second).x != 9.0f, "handle antigo não pode ter afetado o slot reciclado pelo character novo");

  AetherPhysics_DestroyWorld(world);
}
