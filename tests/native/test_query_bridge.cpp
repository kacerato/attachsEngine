// Testes de correção das queries sobre a fronteira C ABI do Jolt (item 4.1.4 do plano):
// RayCastAll (multi-hit), ShapeCastClosest (varredura) e OverlapShape (overlap parado).
// AetherPhysics_RayCastClosest (single-hit) já é coberto por test_jolt_bridge.cpp.
//
// Mesma disciplina dos demais testes de física: tolerância larga o suficiente para o
// integrador/solver do Jolt, estreita o bastante para pegar erro real de eixo/sinal/unidade.
#include "harness.h"
#include "physics/jolt_bridge.h"

#include <cmath>

using namespace ae::test;

namespace {

bool near(float a, float b, float tolerance) { return std::fabs(a - b) <= tolerance; }

AetherBodyHandle MakeSphere(AetherPhysicsWorld *world, AetherVec3 position, AetherMotionType motion) {
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Sphere;
  desc.shape.sphereRadius = 0.5f;
  desc.position = position;
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = motion;
  desc.friction = 0.5f;
  return AetherPhysics_CreateBody(world, &desc);
}

AetherBodyHandle MakeBox(AetherPhysicsWorld *world, AetherVec3 halfExtent, AetherVec3 position, AetherMotionType motion) {
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Box;
  desc.shape.boxHalfExtent = halfExtent;
  desc.position = position;
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = motion;
  desc.friction = 0.5f;
  return AetherPhysics_CreateBody(world, &desc);
}

} // namespace

AE_TEST(raycast_all_acerta_todos_os_corpos_empilhados_ao_longo_do_raio) {
  // Três esferas estáticas empilhadas em Y, um raio vertical de cima para baixo deveria
  // acertar as três — RayCastClosest só acertaria a mais próxima.
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);

  AetherBodyHandle a = MakeSphere(world, {0.0f, 8.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle b = MakeSphere(world, {0.0f, 5.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle c = MakeSphere(world, {0.0f, 2.0f, 0.0f}, AetherMotionType::Static);

  AetherBodyHandle bodies[8];
  float fractions[8];
  ae::i32 count = AetherPhysics_RayCastAll(world, {0.0f, 10.0f, 0.0f}, {0.0f, -20.0f, 0.0f},
                                            AetherQueryLayerMask::All, AetherBodyHandle_Invalid,
                                            bodies, fractions, 8);
  AE_EXPECT_TRUE(count == 3, "raio vertical deveria acertar as 3 esferas empilhadas");
  // Sort() do collector garante ordem por fração crescente — mais perto do raio primeiro.
  AE_EXPECT_TRUE(bodies[0] == a && bodies[1] == b && bodies[2] == c, "hits deveriam vir ordenados do mais próximo ao mais distante");
  AE_EXPECT_TRUE(fractions[0] < fractions[1] && fractions[1] < fractions[2], "frações deveriam crescer monotonicamente");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(raycast_all_respeita_o_buffer_do_chamador_mas_devolve_contagem_real) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  MakeSphere(world, {0.0f, 8.0f, 0.0f}, AetherMotionType::Static);
  MakeSphere(world, {0.0f, 5.0f, 0.0f}, AetherMotionType::Static);
  MakeSphere(world, {0.0f, 2.0f, 0.0f}, AetherMotionType::Static);

  AetherBodyHandle onlyOne[1];
  float onlyOneFraction[1];
  ae::i32 count = AetherPhysics_RayCastAll(world, {0.0f, 10.0f, 0.0f}, {0.0f, -20.0f, 0.0f},
                                            AetherQueryLayerMask::All, AetherBodyHandle_Invalid,
                                            onlyOne, onlyOneFraction, 1);
  AE_EXPECT_TRUE(count == 3, "contagem devolvida deveria ser o total real de hits, mesmo com buffer menor");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(raycast_all_com_layer_mask_static_ignora_corpos_dinamicos) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  MakeSphere(world, {0.0f, 8.0f, 0.0f}, AetherMotionType::Static);
  MakeSphere(world, {0.0f, 5.0f, 0.0f}, AetherMotionType::Dynamic);

  AetherBodyHandle bodies[8];
  float fractions[8];
  ae::i32 count = AetherPhysics_RayCastAll(world, {0.0f, 10.0f, 0.0f}, {0.0f, -20.0f, 0.0f},
                                            AetherQueryLayerMask::Static, AetherBodyHandle_Invalid,
                                            bodies, fractions, 8);
  AE_EXPECT_TRUE(count == 1, "máscara Static deveria filtrar fora o corpo dinâmico");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(raycast_all_ignora_o_corpo_especificado_em_ignoreBody) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  AetherBodyHandle a = MakeSphere(world, {0.0f, 8.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle b = MakeSphere(world, {0.0f, 5.0f, 0.0f}, AetherMotionType::Static);

  AetherBodyHandle bodies[8];
  float fractions[8];
  ae::i32 count = AetherPhysics_RayCastAll(world, {0.0f, 10.0f, 0.0f}, {0.0f, -20.0f, 0.0f},
                                            AetherQueryLayerMask::All, a,
                                            bodies, fractions, 8);
  AE_EXPECT_TRUE(count == 1 && bodies[0] == b, "ignoreBody deveria excluir o corpo 'a' do resultado");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(shapecast_closest_acerta_piso_antes_do_ponto_de_contato_da_esfera_de_consulta) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  const float floorHalfHeight = 0.5f;
  MakeBox(world, {10.0f, floorHalfHeight, 10.0f}, {0.0f, 0.0f, 0.0f}, AetherMotionType::Static);

  AetherShapeDesc probe{};
  probe.kind = AetherShapeKind::Sphere;
  probe.sphereRadius = 0.5f;

  AetherShapeQueryHit hit{};
  ae::i32 found = AetherPhysics_ShapeCastClosest(world, &probe, {0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
                                                  {0.0f, -20.0f, 0.0f}, AetherQueryLayerMask::All,
                                                  AetherBodyHandle_Invalid, &hit);
  AE_EXPECT_TRUE(found == 1, "esfera varrida de cima para baixo deveria acertar o piso");
  // A esfera de raio 0.5 toca o piso (topo em y=0.5) quando seu centro está em y=1.0 —
  // origem y=10, alcance 20 (até y=-10): fração (10-1.0)/20 = 0.45.
  AE_EXPECT_TRUE(near(hit.fraction, 0.45f, 0.02f), "fração do shapecast deveria corresponder ao ponto onde a esfera toca o piso");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(shapecast_closest_sem_alcance_suficiente_nao_acerta_nada) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  MakeBox(world, {10.0f, 0.5f, 10.0f}, {0.0f, 0.0f, 0.0f}, AetherMotionType::Static);

  AetherShapeDesc probe{};
  probe.kind = AetherShapeKind::Sphere;
  probe.sphereRadius = 0.5f;

  AetherShapeQueryHit hit{};
  ae::i32 found = AetherPhysics_ShapeCastClosest(world, &probe, {0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
                                                  {0.0f, -1.0f, 0.0f}, AetherQueryLayerMask::All,
                                                  AetherBodyHandle_Invalid, &hit);
  AE_EXPECT_TRUE(found == 0, "alcance curto demais não pode acertar o piso distante");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(overlap_shape_encontra_corpos_dentro_do_volume_e_ignora_os_de_fora) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  AetherBodyHandle inside1 = MakeSphere(world, {0.2f, 0.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle inside2 = MakeSphere(world, {-0.2f, 0.0f, 0.0f}, AetherMotionType::Static);
  MakeSphere(world, {10.0f, 0.0f, 0.0f}, AetherMotionType::Static); // bem longe, não deveria aparecer

  AetherShapeDesc volume{};
  volume.kind = AetherShapeKind::Box;
  volume.boxHalfExtent = {1.0f, 1.0f, 1.0f};

  AetherShapeQueryHit hits[8];
  ae::i32 count = AetherPhysics_OverlapShape(world, &volume, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
                                              AetherQueryLayerMask::All, AetherBodyHandle_Invalid, hits, 8);
  AE_EXPECT_TRUE(count == 2, "volume deveria sobrepor as 2 esferas próximas, não a distante");

  bool foundInside1 = false, foundInside2 = false;
  for (int i = 0; i < count; ++i) {
    if (hits[i].body == inside1) foundInside1 = true;
    if (hits[i].body == inside2) foundInside2 = true;
  }
  AE_EXPECT_TRUE(foundInside1 && foundInside2, "os dois corpos dentro do volume deveriam aparecer no resultado");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(overlap_shape_sem_nenhum_corpo_dentro_devolve_contagem_zero) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  MakeSphere(world, {50.0f, 0.0f, 0.0f}, AetherMotionType::Static);

  AetherShapeDesc volume{};
  volume.kind = AetherShapeKind::Box;
  volume.boxHalfExtent = {1.0f, 1.0f, 1.0f};

  AetherShapeQueryHit hits[8];
  ae::i32 count = AetherPhysics_OverlapShape(world, &volume, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
                                              AetherQueryLayerMask::All, AetherBodyHandle_Invalid, hits, 8);
  AE_EXPECT_TRUE(count == 0, "volume vazio de corpos deveria devolver contagem zero");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(overlap_shape_ignora_o_corpo_especificado_em_ignoreBody) {
  // Overlap consigo mesmo é o caso de uso mais comum de ignoreBody: um "trigger" ancorado no
  // próprio corpo do jogador não deveria se auto-detectar.
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  AetherBodyHandle self = MakeSphere(world, {0.0f, 0.0f, 0.0f}, AetherMotionType::Static);
  AetherBodyHandle other = MakeSphere(world, {0.3f, 0.0f, 0.0f}, AetherMotionType::Static);

  AetherShapeDesc volume{};
  volume.kind = AetherShapeKind::Sphere;
  volume.sphereRadius = 1.0f;

  AetherShapeQueryHit hits[8];
  ae::i32 count = AetherPhysics_OverlapShape(world, &volume, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
                                              AetherQueryLayerMask::All, self, hits, 8);
  AE_EXPECT_TRUE(count == 1 && hits[0].body == other, "ignoreBody deveria excluir 'self' e deixar só 'other'");

  AetherPhysics_DestroyWorld(world);
}

AE_TEST(queries_com_mundo_nulo_ou_shape_nulo_sao_tratadas_sem_crash) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, 0.0f, 0.0f}, 16);
  AetherShapeDesc shape{};
  shape.kind = AetherShapeKind::Sphere;
  shape.sphereRadius = 0.5f;

  AetherBodyHandle bodies[4];
  float fractions[4];
  AE_EXPECT_TRUE(AetherPhysics_RayCastAll(nullptr, {0, 0, 0}, {0, -1, 0}, AetherQueryLayerMask::All,
                                           AetherBodyHandle_Invalid, bodies, fractions, 4) == 0,
                 "RayCastAll em mundo nulo devolve 0, não crasha");

  AetherShapeQueryHit hit{};
  AE_EXPECT_TRUE(AetherPhysics_ShapeCastClosest(nullptr, &shape, {0, 0, 0}, {0, 0, 0, 1}, {0, -1, 0},
                                                 AetherQueryLayerMask::All, AetherBodyHandle_Invalid, &hit) == 0,
                 "ShapeCastClosest em mundo nulo devolve 0, não crasha");
  AE_EXPECT_TRUE(AetherPhysics_ShapeCastClosest(world, nullptr, {0, 0, 0}, {0, 0, 0, 1}, {0, -1, 0},
                                                 AetherQueryLayerMask::All, AetherBodyHandle_Invalid, &hit) == 0,
                 "ShapeCastClosest com shape nulo devolve 0, não crasha");

  AetherShapeQueryHit hits[4];
  AE_EXPECT_TRUE(AetherPhysics_OverlapShape(nullptr, &shape, {0, 0, 0}, {0, 0, 0, 1}, AetherQueryLayerMask::All,
                                             AetherBodyHandle_Invalid, hits, 4) == 0,
                 "OverlapShape em mundo nulo devolve 0, não crasha");
  AE_EXPECT_TRUE(AetherPhysics_OverlapShape(world, nullptr, {0, 0, 0}, {0, 0, 0, 1}, AetherQueryLayerMask::All,
                                             AetherBodyHandle_Invalid, hits, 4) == 0,
                 "OverlapShape com shape nulo devolve 0, não crasha");

  AetherPhysics_DestroyWorld(world);
}
