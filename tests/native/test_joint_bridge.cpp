// Testes de correção das juntas/motores sobre a fronteira C ABI do Jolt (item 4.1.3 do
// plano — ver o comentário de topo de physics/jolt_bridge.h para o escopo desta fatia:
// Point, Hinge, Slider, Distance; SixDOF e as demais ficam de fora).
//
// Mesma disciplina de test_jolt_bridge.cpp: números esperados vêm de cinemática/dinâmica
// básica, com tolerância larga o suficiente para o integrador do Jolt mas estreita o
// bastante para pegar erro real de eixo/sinal/unidade na fronteira.
#include "harness.h"
#include "physics/jolt_bridge.h"

#include <cmath>

using namespace ae::test;

namespace {

bool near(float a, float b, float tolerance) { return std::fabs(a - b) <= tolerance; }

AetherJointMotorDesc NoMotor() {
  AetherJointMotorDesc m{};
  m.state = AetherMotorState::Off;
  return m;
}

AetherJointMotorDesc VelocityMotor(float targetVelocity, float maxForceOrTorque) {
  AetherJointMotorDesc m{};
  m.state = AetherMotorState::Velocity;
  m.targetVelocity = targetVelocity;
  m.maxForceOrTorque = maxForceOrTorque;
  return m;
}

AetherBodyHandle MakeSphere(AetherPhysicsWorld *world, AetherVec3 position, AetherMotionType motion) {
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Sphere;
  desc.shape.sphereRadius = 0.5f;
  desc.position = position;
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = motion;
  desc.friction = 0.5f;
  desc.restitution = 0.0f;
  return AetherPhysics_CreateBody(world, &desc);
}

} // namespace

AE_TEST(criar_junta_ponto_prende_corpo_dinamico_a_ancora_estatica) {
  // Um corpo dinâmico preso por PointConstraint a um corpo estático não pode cair sob
  // gravidade — o ponto de ancoragem trava as 3 translações.
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);

  AetherBodyHandle anchor = MakeSphere(world, {0.0f, 10.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle bob = MakeSphere(world, {0.0f, 8.0f, 0.0f}, AetherMotionType::Dynamic);
  AE_EXPECT_TRUE(anchor != AetherBodyHandle_Invalid && bob != AetherBodyHandle_Invalid, "corpos deveriam ser criados");

  AetherJointDesc desc{};
  desc.kind = AetherJointKind::Point;
  desc.point1 = {0.0f, 8.0f, 0.0f}; // no referencial mundial (EConstraintSpace::WorldSpace)
  desc.point2 = {0.0f, 8.0f, 0.0f};
  desc.motor = NoMotor();

  AetherJointHandle joint = AetherPhysics_CreateJoint(world, anchor, bob, &desc);
  AE_EXPECT_TRUE(joint != AetherJointHandle_Invalid, "criação da junta ponto deveria ter sucesso");

  for (int i = 0; i < 120; ++i) AetherPhysics_Step(world, 1.0f / 60.0f, 1);

  AetherVec3 position{};
  AetherPhysics_GetTransform(world, bob, &position, nullptr);
  AE_EXPECT_TRUE(near(position.y, 8.0f, 0.05f), "corpo preso por PointConstraint não pode cair sob gravidade");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(junta_dobradica_com_limites_nao_ultrapassa_o_limite_configurado) {
  // Pêndulo: dobradiça sem motor, sem atrito, caindo sob gravidade a partir da horizontal,
  // com limite configurado para travar antes da vertical. O ângulo lido nunca deve
  // ultrapassar o limite (o Jolt pode "vazar" um pouco por causa do solver iterativo —
  // tolerância cobre isso, não indica limite quebrado).
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);

  AetherBodyHandle anchor = MakeSphere(world, {0.0f, 10.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle arm = MakeSphere(world, {1.0f, 10.0f, 0.0f}, AetherMotionType::Dynamic);

  AetherJointDesc desc{};
  desc.kind = AetherJointKind::Hinge;
  desc.point1 = {0.0f, 10.0f, 0.0f};
  desc.point2 = {0.0f, 10.0f, 0.0f};
  desc.axis1 = {0.0f, 0.0f, 1.0f}; // eixo da dobradiça: Z — pêndulo balança no plano XY
  desc.axis2 = {0.0f, 0.0f, 1.0f};
  desc.limitsMin = -1.4f; // radianos; ~-80°, um pouco antes da vertical (-pi/2) para o teste não depender de precisão exata no limite
  desc.limitsMax = 1.4f;
  desc.motor = NoMotor();

  AetherJointHandle joint = AetherPhysics_CreateJoint(world, anchor, arm, &desc);
  AE_EXPECT_TRUE(joint != AetherJointHandle_Invalid, "criação da junta dobradiça deveria ter sucesso");

  float maxAbsAngleSeen = 0.0f;
  for (int i = 0; i < 300; ++i) { // 5s — tempo generoso para o pêndulo cair e estabilizar contra o limite
    AetherPhysics_Step(world, 1.0f / 60.0f, 1);
    float angle = AetherPhysics_GetJointPosition(world, joint);
    maxAbsAngleSeen = std::fmax(maxAbsAngleSeen, std::fabs(angle));
  }

  AE_EXPECT_TRUE(maxAbsAngleSeen <= 1.4f + 0.1f, "ângulo da dobradiça não pode ultrapassar o limite configurado (com folga do solver)");
  AE_EXPECT_TRUE(maxAbsAngleSeen > 0.3f, "o pêndulo deveria ter caído de fato, não ficado parado perto de zero");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(motor_de_dobradica_gira_o_braco_na_velocidade_alvo) {
  // Sem gravidade (isola o efeito do motor): motor de velocidade deveria girar o braço
  // aproximadamente na velocidade alvo, medida por GetJointPosition (ângulo) ao longo do tempo.
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);

  AetherBodyHandle anchor = MakeSphere(world, {0.0f, 0.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle arm = MakeSphere(world, {1.0f, 0.0f, 0.0f}, AetherMotionType::Dynamic);

  AetherJointDesc desc{};
  desc.kind = AetherJointKind::Hinge;
  desc.point1 = {0.0f, 0.0f, 0.0f};
  desc.point2 = {0.0f, 0.0f, 0.0f};
  desc.axis1 = {0.0f, 0.0f, 1.0f};
  desc.axis2 = {0.0f, 0.0f, 1.0f};
  // JPH::HingeConstraintSettings exige mLimitsMin em [-pi,0] e mLimitsMax em [0,pi] (assert
  // em HingeConstraint.cpp:158) — não há "sem limite" fora dessa faixa para Hinge (ao
  // contrário de Slider, que aceita FLT_MAX). [-pi,pi] é a volta completa: suficiente para
  // este teste, que só gira meio segundo a 1 rad/s (~0.5 rad, bem dentro da faixa).
  desc.limitsMin = -3.14159265f;
  desc.limitsMax = 3.14159265f;
  // maxTorque alto o bastante para o motor atingir a velocidade alvo quase instantaneamente
  // (torque limitado demais faz o motor "rampar" — medido experimentalmente: 1000 N*m
  // levava ~meio segundo só para se aproximar de 1 rad/s nesta esfera, o que o motor de
  // teste original confundia com motor não-funcional; não é imprecisão do teste, é torque
  // insuficiente para a inércia do corpo, mesmo tipo de pegadinha do mLinearDamping em
  // test_jolt_bridge.cpp).
  desc.motor = VelocityMotor(/*targetVelocity*/ 1.0f, /*maxTorque*/ 100000.0f);

  AetherJointHandle joint = AetherPhysics_CreateJoint(world, anchor, arm, &desc);
  AE_EXPECT_TRUE(joint != AetherJointHandle_Invalid, "criação da junta motorizada deveria ter sucesso");

  const float dt = 1.0f / 60.0f;
  const int warmup = 5; // ignora a rampa inicial de aceleração do motor até a velocidade alvo
  for (int i = 0; i < warmup; ++i) AetherPhysics_Step(world, dt, 1);
  float angleAtWarmupEnd = AetherPhysics_GetJointPosition(world, joint);

  const int measuredSteps = 25;
  for (int i = 0; i < measuredSteps; ++i) AetherPhysics_Step(world, dt, 1);
  float angleAfter = AetherPhysics_GetJointPosition(world, joint);

  float measuredVelocity = (angleAfter - angleAtWarmupEnd) / (dt * measuredSteps);
  AE_EXPECT_TRUE(near(measuredVelocity, 1.0f, 0.1f), "motor de velocidade deveria manter ~1 rad/s depois da rampa inicial de aceleração");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(junta_slider_com_motor_de_posicao_converge_para_o_alvo) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);

  AetherBodyHandle anchor = MakeSphere(world, {0.0f, 0.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle piston = MakeSphere(world, {0.0f, 0.0f, 0.0f}, AetherMotionType::Dynamic);

  AetherJointDesc desc{};
  desc.kind = AetherJointKind::Slider;
  desc.point1 = {0.0f, 0.0f, 0.0f};
  desc.point2 = {0.0f, 0.0f, 0.0f};
  desc.axis1 = {1.0f, 0.0f, 0.0f};
  desc.axis2 = {1.0f, 0.0f, 0.0f};
  desc.limitsMin = -5.0f;
  desc.limitsMax = 5.0f;
  desc.motor.state = AetherMotorState::Position;
  desc.motor.targetPosition = 2.0f;
  // maxForceOrTorque alto o bastante para a mola do motor (stiffness derivada de
  // springFrequency) nunca saturar antes de alcançar o alvo — 1000N saturava e o motor
  // convergia para um equilíbrio de força limitada bem mais perto de 0 que do alvo (medido
  // experimentalmente: convergia para ~-0.98, não +2.0 — não era erro de sinal/eixo, era
  // limite de força insuficiente, mesma classe de pegadinha do motor de hinge acima).
  desc.motor.maxForceOrTorque = 1000000.0f;
  desc.motor.springFrequency = 4.0f;
  desc.motor.springDamping = 1.0f; // crítico — converge sem oscilar, mais fácil de testar com tolerância apertada

  AetherJointHandle joint = AetherPhysics_CreateJoint(world, anchor, piston, &desc);
  AE_EXPECT_TRUE(joint != AetherJointHandle_Invalid, "criação da junta slider motorizada deveria ter sucesso");

  const float dt = 1.0f / 60.0f;
  for (int i = 0; i < 180; ++i) AetherPhysics_Step(world, dt, 1); // 3s — tempo generoso para convergir

  float position = AetherPhysics_GetJointPosition(world, joint);
  AE_EXPECT_TRUE(near(position, 2.0f, 0.1f), "motor de posição deveria convergir para a posição alvo");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(junta_distancia_limita_afastamento_entre_dois_corpos_dinamicos) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);

  AetherBodyHandle a = MakeSphere(world, {0.0f, 10.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle b = MakeSphere(world, {0.0f, 8.0f, 0.0f}, AetherMotionType::Dynamic);

  AetherJointDesc desc{};
  desc.kind = AetherJointKind::Distance;
  desc.point1 = {0.0f, 10.0f, 0.0f};
  desc.point2 = {0.0f, 8.0f, 0.0f};
  desc.limitsMin = 0.0f;
  desc.limitsMax = 3.0f; // corpo b não pode cair mais que 3m abaixo do ponto de ancoragem
  desc.motor = NoMotor();

  AetherJointHandle joint = AetherPhysics_CreateJoint(world, a, b, &desc);
  AE_EXPECT_TRUE(joint != AetherJointHandle_Invalid, "criação da junta distância deveria ter sucesso");

  for (int i = 0; i < 300; ++i) AetherPhysics_Step(world, 1.0f / 60.0f, 1); // 5s, tempo de sobra para esticar e estabilizar

  AetherVec3 position{};
  AetherPhysics_GetTransform(world, b, &position, nullptr);
  float distanceFromAnchor = 10.0f - position.y;
  AE_EXPECT_TRUE(distanceFromAnchor <= 3.0f + 0.05f, "corpo não pode se afastar além do limite máximo da junta distância");
  AE_EXPECT_TRUE(distanceFromAnchor >= 3.0f - 0.3f, "sob gravidade sustentada, a junta deveria terminar esticada perto do limite máximo");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(destruir_junta_libera_o_corpo_que_volta_a_cair_livremente) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);

  AetherBodyHandle anchor = MakeSphere(world, {0.0f, 10.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle bob = MakeSphere(world, {0.0f, 8.0f, 0.0f}, AetherMotionType::Dynamic);

  AetherJointDesc desc{};
  desc.kind = AetherJointKind::Point;
  desc.point1 = {0.0f, 8.0f, 0.0f};
  desc.point2 = {0.0f, 8.0f, 0.0f};
  desc.motor = NoMotor();
  AetherJointHandle joint = AetherPhysics_CreateJoint(world, anchor, bob, &desc);

  for (int i = 0; i < 60; ++i) AetherPhysics_Step(world, 1.0f / 60.0f, 1);
  AetherVec3 heldPosition{};
  AetherPhysics_GetTransform(world, bob, &heldPosition, nullptr);
  AE_EXPECT_TRUE(near(heldPosition.y, 8.0f, 0.05f), "corpo deveria estar preso antes da destruição da junta");

  AetherPhysics_DestroyJoint(world, joint);

  for (int i = 0; i < 60; ++i) AetherPhysics_Step(world, 1.0f / 60.0f, 1); // 1s de queda livre após soltar

  AetherVec3 freePosition{};
  AetherPhysics_GetTransform(world, bob, &freePosition, nullptr);
  AE_EXPECT_TRUE(freePosition.y < heldPosition.y - 1.0f, "corpo deveria ter caído livremente depois que a junta foi destruída");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(handles_invalidos_e_mundo_nulo_sao_tratados_sem_crash) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  AetherBodyHandle onlyBody = MakeSphere(world, {0.0f, 0.0f, 0.0f}, AetherMotionType::Dynamic);

  AetherJointDesc desc{};
  desc.kind = AetherJointKind::Point;
  desc.motor.state = AetherMotorState::Off;

  // Nenhuma destas chamadas pode crashar — mesma disciplina defensiva do resto da fronteira.
  AE_EXPECT_TRUE(AetherPhysics_CreateJoint(nullptr, onlyBody, onlyBody, &desc) == AetherJointHandle_Invalid, "mundo nulo não pode criar junta");
  AE_EXPECT_TRUE(AetherPhysics_CreateJoint(world, AetherBodyHandle_Invalid, onlyBody, &desc) == AetherJointHandle_Invalid, "handle de corpo 1 inválido não pode criar junta");
  AE_EXPECT_TRUE(AetherPhysics_CreateJoint(world, onlyBody, AetherBodyHandle_Invalid, &desc) == AetherJointHandle_Invalid, "handle de corpo 2 inválido não pode criar junta");
  AE_EXPECT_TRUE(AetherPhysics_CreateJoint(world, onlyBody, onlyBody, nullptr) == AetherJointHandle_Invalid, "desc nulo não pode criar junta");

  AetherPhysics_DestroyJoint(world, AetherJointHandle_Invalid); // no-op válido
  AetherPhysics_DestroyJoint(nullptr, AetherJointHandle_Invalid); // no-op válido
  AetherPhysics_SetJointMotor(world, AetherJointHandle_Invalid, &desc.motor); // no-op válido
  AE_EXPECT_TRUE(AetherPhysics_GetJointPosition(world, AetherJointHandle_Invalid) == 0.0f, "handle inválido devolve 0, não crasha");
  AE_EXPECT_TRUE(AetherPhysics_GetJointPosition(nullptr, AetherJointHandle_Invalid) == 0.0f, "mundo nulo devolve 0, não crasha");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(destruir_junta_duas_vezes_nao_crasha_e_handle_reciclado_nao_colide) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  AetherBodyHandle a = MakeSphere(world, {0.0f, 0.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle b = MakeSphere(world, {1.0f, 0.0f, 0.0f}, AetherMotionType::Dynamic);

  AetherJointDesc desc{};
  desc.kind = AetherJointKind::Point;
  desc.point1 = {0.5f, 0.0f, 0.0f};
  desc.point2 = {0.5f, 0.0f, 0.0f};
  desc.motor = NoMotor();

  AetherJointHandle first = AetherPhysics_CreateJoint(world, a, b, &desc);
  AE_EXPECT_TRUE(first != AetherJointHandle_Invalid, "primeira junta deveria ser criada");
  AetherPhysics_DestroyJoint(world, first);
  AetherPhysics_DestroyJoint(world, first); // destruir de novo não pode crashar

  AetherBodyHandle c = MakeSphere(world, {2.0f, 0.0f, 0.0f}, AetherMotionType::Dynamic);
  AetherJointHandle second = AetherPhysics_CreateJoint(world, a, c, &desc);
  AE_EXPECT_TRUE(second != AetherJointHandle_Invalid, "segunda junta (reciclando o slot) deveria ser criada");
  AE_EXPECT_TRUE(second != first, "handle reciclado deveria ter geração diferente do handle antigo");

  // O handle ANTIGO (já destruído) não pode operar sobre o slot reciclado pela junta nova.
  AetherPhysics_SetJointMotor(world, first, &desc.motor); // deveria ser no-op (geração não bate)
  AE_EXPECT_TRUE(AetherPhysics_GetJointPosition(world, first) == 0.0f, "handle antigo não pode ler posição do slot reciclado");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(junta_v2_local_ao_corpo_1_converte_pontos_para_o_mundo) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  AetherBodyHandle anchor = MakeSphere(world, {10.0f, 5.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle bob = MakeSphere(world, {12.0f, 5.0f, 0.0f}, AetherMotionType::Dynamic);

  AetherJointDescV2 desc{};
  desc.structSize = sizeof(desc);
  desc.apiVersion = AetherJointApiVersionV2;
  desc.kind = AetherJointKind::Point;
  desc.space = AetherJointSpace::LocalToBody1;
  desc.point1 = {2.0f, 0.0f, 0.0f};
  desc.point2 = {2.0f, 0.0f, 0.0f};
  desc.motor = NoMotor();

  AetherJointHandle joint = AetherPhysics_CreateJointV2(world, anchor, bob, &desc);
  AE_EXPECT_TRUE(joint != AetherJointHandle_Invalid, "V2 local ao corpo 1 deveria criar a junta");
  for (int i = 0; i < 120; ++i) AetherPhysics_Step(world, 1.0f / 60.0f, 1);

  AetherVec3 position{};
  AetherPhysics_GetTransform(world, bob, &position, nullptr);
  AE_EXPECT_TRUE(near(position.x, 12.0f, 0.05f) && near(position.y, 5.0f, 0.05f),
                 "pontos locais deveriam ser convertidos pelo transform do corpo 1");
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(junta_v2_local_ao_corpo_2_converte_eixo_rotacionado) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  AetherBodyHandle anchor = MakeSphere(world, {0.0f, 0.0f, 0.0f}, AetherMotionType::Static);

  AetherBodyDesc bodyDesc{};
  bodyDesc.shape.kind = AetherShapeKind::Sphere;
  bodyDesc.shape.sphereRadius = 0.5f;
  bodyDesc.position = {0.0f, 0.0f, 0.0f};
  bodyDesc.rotation = {0.0f, 0.0f, 0.70710678f, 0.70710678f}; // +90 graus em Z
  bodyDesc.motionType = AetherMotionType::Dynamic;
  bodyDesc.friction = 0.5f;
  AetherBodyHandle sliderBody = AetherPhysics_CreateBody(world, &bodyDesc);

  AetherJointDescV2 desc{};
  desc.structSize = sizeof(desc);
  desc.apiVersion = AetherJointApiVersionV2;
  desc.kind = AetherJointKind::Slider;
  desc.space = AetherJointSpace::LocalToBody2;
  desc.axis1 = {1.0f, 0.0f, 0.0f};
  desc.axis2 = {1.0f, 0.0f, 0.0f};
  desc.limitsMin = -3.0f;
  desc.limitsMax = 3.0f;
  desc.motor.state = AetherMotorState::Position;
  desc.motor.targetPosition = 2.0f;
  desc.motor.maxForceOrTorque = 1000000.0f;
  desc.motor.springFrequency = 4.0f;
  desc.motor.springDamping = 1.0f;

  AetherJointHandle joint = AetherPhysics_CreateJointV2(world, anchor, sliderBody, &desc);
  AE_EXPECT_TRUE(joint != AetherJointHandle_Invalid, "V2 local ao corpo 2 deveria criar slider");
  for (int i = 0; i < 180; ++i) AetherPhysics_Step(world, 1.0f / 60.0f, 1);

  AetherVec3 position{};
  AetherPhysics_GetTransform(world, sliderBody, &position, nullptr);
  AE_EXPECT_TRUE(std::fabs(position.y) > 1.5f,
                 "eixo X local rotacionado 90 graus deveria produzir movimento no eixo Y mundial");
  AE_EXPECT_TRUE(std::fabs(position.x) < 0.2f,
                 "conversão do eixo não pode deixar o slider no X mundial original");
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(junta_v2_valida_versao_e_contrato_continuo_da_dobradica) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  AetherBodyHandle anchor = MakeSphere(world, {0.0f, 0.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle arm = MakeSphere(world, {1.0f, 0.0f, 0.0f}, AetherMotionType::Dynamic);

  AetherJointDescV2 desc{};
  desc.structSize = sizeof(desc);
  desc.apiVersion = AetherJointApiVersionV2;
  desc.kind = AetherJointKind::Hinge;
  desc.space = AetherJointSpace::World;
  desc.axis1 = {0.0f, 0.0f, 1.0f};
  desc.axis2 = {0.0f, 0.0f, 1.0f};
  desc.limitsMin = -3.14159265f;
  desc.limitsMax = 3.14159265f;
  desc.motor = NoMotor();

  AetherJointHandle continuous = AetherPhysics_CreateJointV2(world, anchor, arm, &desc);
  AE_EXPECT_TRUE(continuous != AetherJointHandle_Invalid,
                 "[-pi,+pi] exatos devem representar hinge contínua válida");
  AetherPhysics_DestroyJoint(world, continuous);

  desc.limitsMax = 4.0f;
  AE_EXPECT_TRUE(AetherPhysics_CreateJointV2(world, anchor, arm, &desc) == AetherJointHandle_Invalid,
                 "limite acima de +pi deve ser recusado, não truncado");
  desc.limitsMax = 3.14159265f;
  desc.axis2 = {0.0f, 0.0f, 0.0f};
  AE_EXPECT_TRUE(AetherPhysics_CreateJointV2(world, anchor, arm, &desc) == AetherJointHandle_Invalid,
                 "eixo degenerado deve ser recusado antes de chegar ao Jolt");
  desc.axis2 = {0.0f, 0.0f, 1.0f};
  desc.apiVersion++;
  AE_EXPECT_TRUE(AetherPhysics_CreateJointV2(world, anchor, arm, &desc) == AetherJointHandle_Invalid,
                 "versão desconhecida deve ser recusada");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(mechanisms_block_angular_limits_drives_and_v3_boundary) {
  // Exercise the angular constraints, not merely the common pivot constraint.
  for(const auto kind:{AetherJointKind::Fixed,AetherJointKind::Cone,AetherJointKind::SwingTwist,AetherJointKind::SixDOF}) {
    auto *world=AetherPhysics_CreateWorld({0,0,0},16);
    const auto anchor=MakeSphere(world,{0,9,0},AetherMotionType::Static);
    const auto body=MakeSphere(world,{0,7,0},AetherMotionType::Dynamic);
    AetherJointDescV3 d{};d.structSize=sizeof(d);d.apiVersion=3;
    auto &b=d.base;b.structSize=sizeof(b);b.apiVersion=2;b.kind=kind;
    b.point1=b.point2={0,7,0};b.axis1=b.axis2={1,0,0};d.normal1=d.normal2={0,1,0};
    d.swingY=d.swingZ=.6f;d.twistMin=-.25f;d.twistMax=.25f;
    for(auto &a:d.axes){a.minimum=-1;a.maximum=1;a.motor=NoMotor();}
    if(kind==AetherJointKind::SixDOF) {d.axes[5].motion=1;d.axes[5].minimum=-.2f;d.axes[5].maximum=.2f;d.axes[5].motor=VelocityMotor(2,100);}
    auto invalid=d;invalid.apiVersion=4;
    AE_EXPECT_TRUE(AetherPhysics_CreateJointV3(world,anchor,body,&invalid)==AetherJointHandle_Invalid,"wrong ABI version rejected before solver");
    AE_EXPECT_TRUE(AetherPhysics_CreateJointV2(world,anchor,body,&b)==AetherJointHandle_Invalid,"legacy descriptor cannot pretend to carry extended fields");
    const auto joint=AetherPhysics_CreateJointV3(world,anchor,body,&d);
    AE_EXPECT_TRUE(joint!=AetherJointHandle_Invalid,"new native constraint created");
    AE_EXPECT_TRUE(AetherPhysics_SetBodyAngularVelocityV1(world,body,kind==AetherJointKind::SwingTwist?AetherVec3{5,0,0}:AetherVec3{0,0,5}),"spin the constrained body");
    for(int i=0;i<180;++i)AetherPhysics_Step(world,1.f/120,1);
    AetherQuat rotation{};AetherPhysics_GetTransform(world,body,nullptr,&rotation);
    const float angle=2*std::atan2(std::abs(kind==AetherJointKind::SwingTwist?rotation.x:rotation.z),std::abs(rotation.w));
    const float limit=kind==AetherJointKind::Fixed?.02f:kind==AetherJointKind::Cone?.63f:kind==AetherJointKind::SwingTwist?.28f:.23f;
    AE_EXPECT_TRUE(angle<limit,"rotation is bounded by the actual constraint");
    if(kind!=AetherJointKind::Fixed) AE_EXPECT_TRUE(angle>.08f,"allowed rotational freedom is not welded");
    AetherPhysics_DestroyJoint(world,joint);
    AetherPhysics_DestroyWorld(world);
  }
}
