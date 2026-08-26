#include "harness.h"
#include "physics/jolt_bridge.h"

using namespace ae::test;

namespace {

AetherBodyDescV2 BoxV2(AetherVec3 halfExtent, AetherVec3 position,
                        AetherMotionType motionType, bool sensor,
                        AetherQueryLayerMask mask = AetherQueryLayerMask::All) {
  AetherBodyDescV2 desc{};
  desc.structSize = sizeof(desc);
  desc.apiVersion = AetherBodyApiVersionV2;
  desc.shape.kind = AetherShapeKind::Box;
  desc.shape.boxHalfExtent = halfExtent;
  desc.position = position;
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = motionType;
  desc.friction = 0.5f;
  desc.allowedDOFs = AetherAllowedDOFs::All;
  desc.isSensor = sensor ? 1u : 0u;
  desc.eventLayerMask = static_cast<ae::u32>(mask);
  return desc;
}

AetherBodyDescV2 SphereV2(float radius, AetherVec3 position,
                           AetherMotionType motionType, bool sensor,
                           AetherQueryLayerMask mask = AetherQueryLayerMask::All) {
  AetherBodyDescV2 desc = BoxV2({0.5f, 0.5f, 0.5f}, position, motionType, sensor, mask);
  desc.shape.kind = AetherShapeKind::Sphere;
  desc.shape.sphereRadius = radius;
  return desc;
}

bool Contains(const AetherTriggerEvent *events, int count, AetherBodyHandle sensor,
              AetherBodyHandle other, AetherTriggerEventType type) {
  for (int i = 0; i < count; ++i)
    if (events[i].sensor == sensor && events[i].other == other && events[i].type == type)
      return true;
  return false;
}

} // namespace

AE_TEST(trigger_publica_enter_stay_exit_sem_resposta_fisica) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  AetherBodyDescV2 sensorDesc = BoxV2({1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                                      AetherMotionType::Kinematic, true);
  AetherBodyDescV2 otherDesc = SphereV2(0.5f, {0.0f, 0.0f, 0.0f},
                                        AetherMotionType::Static, false);
  AetherBodyHandle sensor = AetherPhysics_CreateBodyV2(world, &sensorDesc);
  AetherBodyHandle other = AetherPhysics_CreateBodyV2(world, &otherDesc);
  AE_EXPECT_TRUE(sensor != AetherBodyHandle_Invalid && other != AetherBodyHandle_Invalid,
                 "sensor e corpo alvo V2 deveriam ser criados");

  AetherTriggerEvent events[8]{};
  AE_EXPECT_TRUE(AetherPhysics_GetTriggerEvents(world, events, 8) == 0,
                 "antes do primeiro Step não há fotografia de eventos");

  AetherPhysics_Step(world, 1.0f / 60.0f, 1);
  int count = AetherPhysics_GetTriggerEvents(world, events, 8);
  AE_EXPECT_TRUE(Contains(events, count, sensor, other, AetherTriggerEventType::Enter),
                 "primeiro contato deveria publicar Enter");

  AetherPhysics_Step(world, 1.0f / 60.0f, 1);
  count = AetherPhysics_GetTriggerEvents(world, events, 8);
  AE_EXPECT_TRUE(Contains(events, count, sensor, other, AetherTriggerEventType::Stay),
                 "contato persistente deveria publicar Stay");

  AE_EXPECT_TRUE(AetherPhysics_MoveKinematicV2(world, sensor, {10.0f, 0.0f, 0.0f},
                                                {0.0f, 0.0f, 0.0f, 1.0f}, 1.0f / 60.0f) == 1,
                 "sensor cinemático deveria aceitar movimento para fora");
  bool exited = false;
  for (int i = 0; i < 3 && !exited; ++i) {
    AetherPhysics_Step(world, 1.0f / 60.0f, 1);
    count = AetherPhysics_GetTriggerEvents(world, events, 8);
    exited = Contains(events, count, sensor, other, AetherTriggerEventType::Exit);
  }
  AE_EXPECT_TRUE(exited, "separação do último subcontato deveria publicar Exit");
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(trigger_respeita_filtro_de_camadas_e_reporta_contagem_real) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  AetherBodyDescV2 sensorDesc = BoxV2({2.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 0.0f},
                                      AetherMotionType::Kinematic, true,
                                      AetherQueryLayerMask::Static);
  AetherBodyDescV2 staticDesc = SphereV2(0.25f, {-0.5f, 0.0f, 0.0f},
                                        AetherMotionType::Static, false);
  AetherBodyDescV2 dynamicDesc = SphereV2(0.25f, {0.5f, 0.0f, 0.0f},
                                         AetherMotionType::Dynamic, false);
  AetherBodyHandle sensor = AetherPhysics_CreateBodyV2(world, &sensorDesc);
  AetherBodyHandle staticBody = AetherPhysics_CreateBodyV2(world, &staticDesc);
  AetherBodyHandle dynamicBody = AetherPhysics_CreateBodyV2(world, &dynamicDesc);

  AetherPhysics_Step(world, 1.0f / 60.0f, 1);
  AetherTriggerEvent one[1]{};
  int realCount = AetherPhysics_GetTriggerEvents(world, one, 1);
  AE_EXPECT_TRUE(realCount == 1, "filtro Static deveria eliminar o evento do corpo Dynamic");
  AE_EXPECT_TRUE(one[0].sensor == sensor && one[0].other == staticBody &&
                     one[0].type == AetherTriggerEventType::Enter,
                 "único evento deveria apontar para o corpo estático aceito");
  AE_EXPECT_TRUE(!Contains(one, 1, sensor, dynamicBody, AetherTriggerEventType::Enter),
                 "camada Dynamic filtrada não pode vazar para a fotografia");
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(sensor_estatico_nao_sustenta_corpo_dinamico) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 16);
  AetherBodyDescV2 sensorDesc = BoxV2({3.0f, 0.5f, 3.0f}, {0.0f, 0.0f, 0.0f},
                                      AetherMotionType::Static, true);
  AetherBodyDescV2 ballDesc = SphereV2(0.5f, {0.0f, 2.0f, 0.0f},
                                       AetherMotionType::Dynamic, false);
  AetherPhysics_CreateBodyV2(world, &sensorDesc);
  AetherBodyHandle ball = AetherPhysics_CreateBodyV2(world, &ballDesc);
  for (int i = 0; i < 90; ++i) AetherPhysics_Step(world, 1.0f / 60.0f, 1);

  AetherVec3 position{};
  AetherPhysics_GetTransform(world, ball, &position, nullptr);
  AE_EXPECT_TRUE(position.y < -1.0f,
                 "mIsSensor deve remover resposta física: a esfera precisa atravessar o volume");
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(trigger_persistente_nao_emite_exit_so_por_corpo_dormir) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  AetherBodyDescV2 sensorDesc = BoxV2({1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                                      AetherMotionType::Static, true);
  AetherBodyDescV2 bodyDesc = SphereV2(0.25f, {0.0f, 0.0f, 0.0f},
                                       AetherMotionType::Dynamic, false);
  AetherBodyHandle sensor = AetherPhysics_CreateBodyV2(world, &sensorDesc);
  AetherBodyHandle body = AetherPhysics_CreateBodyV2(world, &bodyDesc);
  AetherTriggerEvent events[4]{};
  bool falseExit = false;
  for (int i = 0; i < 300; ++i) {
    AetherPhysics_Step(world, 1.0f / 60.0f, 1);
    int count = AetherPhysics_GetTriggerEvents(world, events, 4);
    falseExit |= Contains(events, count, sensor, body, AetherTriggerEventType::Exit);
  }
  AE_EXPECT_TRUE(!falseExit,
                 "um corpo adormecido ainda sobreposto não pode parecer que saiu do trigger");
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(destruir_corpo_sobreposto_publica_exit_no_step_seguinte) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  AetherBodyDescV2 sensorDesc = BoxV2({1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                                      AetherMotionType::Static, true);
  AetherBodyDescV2 bodyDesc = SphereV2(0.25f, {0.0f, 0.0f, 0.0f},
                                       AetherMotionType::Dynamic, false);
  AetherBodyHandle sensor = AetherPhysics_CreateBodyV2(world, &sensorDesc);
  AetherBodyHandle body = AetherPhysics_CreateBodyV2(world, &bodyDesc);
  AetherPhysics_Step(world, 1.0f / 60.0f, 1);
  AetherPhysics_DestroyBody(world, body);
  AetherPhysics_Step(world, 1.0f / 60.0f, 1);
  AetherTriggerEvent events[4]{};
  int count = AetherPhysics_GetTriggerEvents(world, events, 4);
  AE_EXPECT_TRUE(Contains(events, count, sensor, body, AetherTriggerEventType::Exit),
                 "destroy durante overlap deve encerrar o par, não deixá-lo ativo para sempre");
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(create_body_v2_rejeita_layout_ou_filtro_invalido) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 4);
  AetherBodyDescV2 desc = BoxV2({0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 0.0f},
                                AetherMotionType::Static, true);
  desc.structSize = sizeof(desc) - 1;
  AE_EXPECT_TRUE(AetherPhysics_CreateBodyV2(world, &desc) == AetherBodyHandle_Invalid,
                 "struct truncada não pode atravessar ABI versionada");
  desc.structSize = sizeof(desc);
  desc.eventLayerMask = 1u << 12;
  AE_EXPECT_TRUE(AetherPhysics_CreateBodyV2(world, &desc) == AetherBodyHandle_Invalid,
                 "bit de camada desconhecido deve ser recusado");
  AetherPhysics_DestroyWorld(world);
}
