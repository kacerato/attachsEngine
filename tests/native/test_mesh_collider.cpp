// Colisor de malha (Mesh Collider da Unity) montado a partir da geometria do
// próprio objeto.
//
// O que estes testes protegem: a forma é a malha que o objeto desenha (com a
// escala do objeto), a regra de convexidade é a mesma da
// Unity — triângulos soltos só em corpo estático ou cinemático —, e uma consulta
// que acerta a malha devolve o colisor certo mesmo quando o corpo tem várias
// formas.
#include "harness.h"
#include "editor/editor_document.h"
#include "editor/editor_physics_body.h"
#include "runtime/scene_physics.h"

#include <cmath>
#include <sstream>

using namespace ae;
using namespace ae::editor;
using ae::runtime::GameWorld;
using ae::runtime::QueryFilter;
using ae::runtime::QueryHit;
using ae::runtime::ScenePhysics;

namespace {
// Malha 1: um piso de 20×20 em Y=0. Malha 2: um cubo de lado 1 centrado na
// origem. Malha 3: um triângulo plano (não tem casco sólido).
class Geometry final : public runtime::CollisionGeometrySource {
public:
  bool meshTriangles(u32 mesh, std::vector<float> &out) const override {
    const auto push = [&](std::initializer_list<float> values) { out.insert(out.end(), values); };
    if (mesh == 1) {
      push({-10, 0, -10, -10, 0, 10, 10, 0, 10});
      push({-10, 0, -10, 10, 0, 10, 10, 0, -10});
      return true;
    }
    if (mesh == 2) {
      // Doze triângulos, vértice repetido por triângulo como na malha de desenho.
      const float c[8][3]{{-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f},
                          {-.5f, -.5f, .5f},  {.5f, -.5f, .5f},  {.5f, .5f, .5f},  {-.5f, .5f, .5f}};
      const u32 faces[12][3]{{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
                             {3, 6, 2}, {3, 7, 6}, {0, 4, 7}, {0, 7, 3}, {1, 2, 6}, {1, 6, 5}};
      for (const auto &f : faces)
        for (u32 v : f) push({c[v][0], c[v][1], c[v][2]});
      return true;
    }
    if (mesh == 3) {
      push({0, 0, 0, 1, 0, 0, 0, 0, 1});
      return true;
    }
    return false;
  }
  bool meshTriangles(const resources::AssetGuid &asset, std::vector<float> &out) const override {
    return asset==resources::assetGuidFromSeed("teste:malha-colisao:cubo") && meshTriangles(2,out);
  }
};

EditorEntityId meshBody(EditorDocument &doc, const char *name, u32 mesh, scene::BodyMotion motion, bool convex,
                        float y = 0, float scale = 1) {
  const auto id = doc.createEntity(doc.root(), EditorEntityKind::Mesh, name);
  auto values = *doc.find(id);
  values.transform.position[1] = y;
  for (float &s : values.transform.scale) s = scale;
  editMeshRenderer(values)->mesh = mesh;
  editPhysicsBody(values)->motion = motion;
  auto *collider = editCollider(values);
  collider->shape = scene::ColliderShape::Mesh;
  collider->convex = convex;
  return doc.applyEntityValues(id, values) ? id : 0;
}

bool start(GameWorld &world, ScenePhysics &physics, const EditorDocument &doc, const Geometry *geometry) {
  return world.load(doc) && physics.start(world, geometry);
}
} // namespace

AE_TEST(static_mesh_collider_uses_the_drawn_triangles_and_the_object_scale) {
  EditorDocument doc;
  const auto floor = meshBody(doc, "Piso", 1, scene::BodyMotion::Static, false, -1, 2);
  AE_EXPECT_TRUE(floor != 0, "piso de malha criado");
  const Geometry geometry;
  GameWorld world;
  ScenePhysics physics;
  AE_EXPECT_TRUE(start(world, physics, doc, &geometry), physics.error().c_str());

  // Com escala 2 o piso vai de -20 a 20: um raio em X=15 só acerta se a escala
  // foi aplicada aos vértices.
  const float origin[3]{15, 5, 0}, down[3]{0, -10, 0};
  QueryHit hit;
  AE_EXPECT_TRUE(physics.rayCast(origin, down, QueryFilter{}, hit), "o raio acerta a malha escalada");
  AE_EXPECT_EQ(hit.object, floor, "identidade do objeto que desenha a malha");
  AE_EXPECT_TRUE(std::abs(hit.point[1] + 1) < .01f, "o ponto está no plano da malha, na pose do objeto");
  AE_EXPECT_TRUE(hit.normal[1] > .9f, "a face olha para cima");

  const float outside[3]{25, 5, 0};
  AE_EXPECT_TRUE(!physics.rayCast(outside, down, QueryFilter{}, hit), "fora da malha não há chão");
}

AE_TEST(non_convex_mesh_is_refused_on_a_dynamic_body_like_unity) {
  EditorDocument doc;
  meshBody(doc, "Caixote", 2, scene::BodyMotion::Dynamic, false, 3);
  const Geometry geometry;
  GameWorld world;
  ScenePhysics physics;
  AE_EXPECT_TRUE(!start(world, physics, doc, &geometry), "triângulos soltos não têm massa");
  AE_EXPECT_TRUE(physics.error().find("Convexo") != std::string::npos, "o motivo diz o que ligar");
}

AE_TEST(convex_mesh_falls_and_rests_on_a_static_mesh) {
  EditorDocument doc;
  meshBody(doc, "Piso", 1, scene::BodyMotion::Static, false);
  const auto crate = meshBody(doc, "Caixote", 2, scene::BodyMotion::Dynamic, true, 2);
  const Geometry geometry;
  GameWorld world;
  ScenePhysics physics;
  AE_EXPECT_TRUE(start(world, physics, doc, &geometry), physics.error().c_str());
  for (u32 step = 0; step < 180; ++step)
    AE_EXPECT_TRUE(physics.advance(1.0 / 60.0, world), "passo aceito");
  const auto *object = world.graph().find(crate);
  AE_EXPECT_TRUE(object != nullptr, "o caixote existe");
  // O cubo tem meia altura .5 e o casco convexo tem raio de canto, então ele
  // repousa perto de Y=.5 — nunca atravessando o piso nem parado no ar.
  const float y = object->transform.position[1];
  AE_EXPECT_TRUE(y > .4f && y < .6f, "o casco convexo repousa sobre a malha estática");
}

AE_TEST(flat_mesh_has_no_convex_hull) {
  EditorDocument doc;
  meshBody(doc, "Folha", 3, scene::BodyMotion::Dynamic, true, 1);
  const Geometry geometry;
  GameWorld world;
  ScenePhysics physics;
  AE_EXPECT_TRUE(!start(world, physics, doc, &geometry), "casco degenerado é recusado, não vira caixa");
}

AE_TEST(compound_body_v3_validates_and_accepts_authored_mesh_cooking) {
  AetherPhysicsWorld *native=AetherPhysics_CreateWorld({0,-9.81f,0},16);
  AE_EXPECT_TRUE(native!=nullptr,"mundo Jolt disponível");
  const AetherVec3 points[]{{-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f},
                            {-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f}};
  AetherCompoundPartV3 part{};part.base.rotation={0,0,0,1};part.geometry=AetherPartGeometry::ConvexHull;
  part.vertices=points;part.vertexCount=8;part.cooking=AetherMeshCookingDefaultsV1;
  part.cooking.flags=0;part.cooking.hullTolerance=.01f;part.cooking.activeEdgeAngleDegrees=27;
  AetherBodyDescV2 desc{};desc.structSize=sizeof(desc);desc.apiVersion=AetherBodyApiVersionV2;
  desc.rotation={0,0,0,1};desc.motionType=AetherMotionType::Static;desc.friction=.5f;
  AetherBodyDynamicsV1 dynamics{sizeof(AetherBodyDynamicsV1),1,0,0,1,{0,0,0},1};
  const auto body=AetherPhysics_CreateCompoundBodyV3(native,&desc,&part,1,&dynamics);
  AE_EXPECT_TRUE(body!=AetherBodyHandle_Invalid,"V3 cria o casco com cooking autoral");
  AetherPhysics_DestroyBody(native,body);
  part.cooking.hullTolerance=0;
  AE_EXPECT_TRUE(AetherPhysics_CreateCompoundBodyV3(native,&desc,&part,1,&dynamics)==AetherBodyHandle_Invalid,
                 "tolerância fora do contrato é recusada na fronteira");
  AetherPhysics_DestroyWorld(native);
}

AE_TEST(mesh_collider_without_renderer_or_geometry_refuses_with_a_reason) {
  EditorDocument doc;
  const auto id = meshBody(doc, "Piso", 1, scene::BodyMotion::Static, false);
  const Geometry geometry;
  {
    GameWorld world;
    ScenePhysics physics;
    AE_EXPECT_TRUE(!start(world, physics, doc, nullptr), "sem fonte de geometria não há forma");
    AE_EXPECT_TRUE(physics.error().find("geometria") != std::string::npos, "e o motivo diz isso");
  }
  auto values = *doc.find(id);
  editMeshRenderer(values)->mesh = 0;
  AE_EXPECT_TRUE(doc.applyEntityValues(id, values), "renderer sem malha");
  GameWorld world;
  ScenePhysics physics;
  AE_EXPECT_TRUE(!start(world, physics, doc, &geometry), "sem malha no objeto não há forma");
  AE_EXPECT_TRUE(physics.error().find("Renderizador de malha") != std::string::npos, "o motivo nomeia o requisito");
}

AE_TEST(mesh_collider_can_use_an_explicit_resource_without_a_visual_renderer) {
  EditorDocument doc;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Colisão simplificada");
  auto values=*doc.find(id);
  editPhysicsBody(values)->motion=scene::BodyMotion::Static;
  auto *collider=editCollider(values);collider->shape=scene::ColliderShape::Mesh;
  collider->collisionMesh=resources::assetGuidFromSeed("teste:malha-colisao:cubo");
  AE_EXPECT_TRUE(doc.applyEntityValues(id,values),"colisor explícito criado sem renderer");
  const Geometry geometry;GameWorld world;ScenePhysics physics;
  AE_EXPECT_TRUE(start(world,physics,doc,&geometry),physics.error().c_str());
  const float origin[3]{0,5,0},down[3]{0,-10,0};QueryHit hit;
  AE_EXPECT_TRUE(physics.rayCast(origin,down,QueryFilter{},hit),"a forma usa o recurso físico");
  AE_EXPECT_EQ(hit.object,id,"a consulta mantém a identidade do objeto autoral");
}

AE_TEST(query_reports_the_collider_that_was_hit_in_a_multi_shape_body) {
  // Um corpo estático com duas formas: a malha do piso e uma caixa acima dele.
  // Antes, toda consulta devolvia o primeiro colisor do corpo.
  EditorDocument doc;
  const auto id = meshBody(doc, "Piso", 1, scene::BodyMotion::Static, false);
  auto values = *doc.find(id);
  auto *box = static_cast<scene::Collider *>(values.components.add(scene::Collider::descriptor));
  AE_EXPECT_TRUE(box != nullptr, "segunda forma anexada");
  box->shape = scene::ColliderShape::Box;
  box->centerX = 5;
  box->centerY = 2;
  AE_EXPECT_TRUE(doc.applyEntityValues(id, values), "corpo com duas formas");
  const auto &stored = *doc.find(id);
  u64 meshInstance = 0, boxInstance = 0;
  for (usize i = 0; i < stored.components.size(); ++i) {
    const auto *c = stored.components.at(i);
    if (&c->type() != &scene::Collider::descriptor) continue;
    (static_cast<const scene::Collider *>(c)->shape == scene::ColliderShape::Mesh ? meshInstance : boxInstance) = c->instanceId();
  }
  const Geometry geometry;
  GameWorld world;
  ScenePhysics physics;
  AE_EXPECT_TRUE(start(world, physics, doc, &geometry), physics.error().c_str());
  const float down[3]{0, -10, 0}, onBox[3]{5, 5, 0}, onFloor[3]{-5, 5, 0};
  QueryHit hit;
  AE_EXPECT_TRUE(physics.rayCast(onBox, down, QueryFilter{}, hit), "acerta a caixa");
  AE_EXPECT_EQ(hit.colliderInstance, boxInstance, "a caixa é a segunda forma do corpo");
  AE_EXPECT_TRUE(physics.rayCast(onFloor, down, QueryFilter{}, hit), "acerta o piso");
  AE_EXPECT_EQ(hit.colliderInstance, meshInstance, "o piso é a malha");
}

AE_TEST(collider_file_reads_older_versions_and_keeps_convex) {
  scene::Collider collider;
  collider.shape = scene::ColliderShape::Mesh;
  collider.convex = true;
  collider.hullTolerance=.025f;collider.activeEdgeAngle=17;collider.weldVertices=false;collider.optimizeCooking=false;
  collider.collisionMesh=resources::assetGuidFromSeed("teste:serializacao-colisao");
  std::stringstream out;
  collider.write(out);
  scene::Collider read;
  AE_EXPECT_TRUE(read.read(out, 6), "a versão atual relê");
  AE_EXPECT_TRUE(read.shape == scene::ColliderShape::Mesh && read.convex && read.collisionMesh==collider.collisionMesh &&
                 std::abs(read.hullTolerance-.025f)<1e-6f && read.activeEdgeAngle==17 &&
                 !read.weldVertices&&!read.optimizeCooking,"forma, recurso e cooking voltam");

  std::stringstream v5("3 .5 .5 .5 .5 .5 0 0 0 0 0 0 0 1 1 -");
  scene::Collider previous;
  AE_EXPECT_TRUE(previous.read(v5,5),"a versão 5 continua legível");
  AE_EXPECT_TRUE(previous.weldVertices&&previous.optimizeCooking&&std::abs(previous.hullTolerance-.001f)<1e-7f&&
                 previous.activeEdgeAngle==5,"arquivo anterior recebe o cooking que reproduz o comportamento antigo");

  // A versão 3 só conhecia as primitivas: lê Convexo desligado e recusa a forma 3.
  std::stringstream v3("0 .5 .5 .5 .5 .5 0 0 0 0 0 0 0 1");
  scene::Collider old;
  AE_EXPECT_TRUE(old.read(v3, 3), "arquivo antigo relê");
  AE_EXPECT_TRUE(old.shape == scene::ColliderShape::Box && !old.convex, "com convexo desligado");
  std::stringstream future("3 .5 .5 .5 .5 .5 0 0 0 0 0 0 0 1");
  AE_EXPECT_TRUE(!old.read(future, 3), "a versão 3 não tinha a forma Malha");
}
