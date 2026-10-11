// N04: IK por componente. Mesmos solvers no estudio e no Play; peso sem
// acumulo entre quadros; sem NaN, sem alongar osso; alvo removido diagnostica;
// persistencia; apoio do pe pela consulta de chao; criacao pelo editor com Undo.
#include "harness.h"
#include "runtime/scene_ik.h"
#include "runtime/game_world.h"
#include "editor/editor_session.h"
#include "editor/editor_document.h"
#include <cmath>
#include <sstream>

using namespace ae;
using namespace ae::runtime;
namespace {
bool close(float a,float b,float e=1e-3f){return std::abs(a-b)<e;}
float distance(const float *a,const float *b){return std::sqrt((a[0]-b[0])*(a[0]-b[0])+(a[1]-b[1])*(a[1]-b[1])+(a[2]-b[2])*(a[2]-b[2]));}
struct Leg {
  SceneGraph g;ObjectId owner=0,hip=0,knee=0,foot=0,target=0,pole=0;
  Leg() {
    owner=g.createEntity(g.root(),ObjectKind::Folder,"Robo");
    hip=g.createEntity(owner,ObjectKind::Folder,"Coxa");knee=g.createEntity(hip,ObjectKind::Folder,"Canela");foot=g.createEntity(knee,ObjectKind::Folder,"Pe");
    Transform t;t.position[1]=2;g.setTransform(hip,t);
    Transform k;k.position[1]=-1;k.position[2]=.1f;g.setTransform(knee,k);
    Transform f;f.position[1]=-1;g.setTransform(foot,f);
    target=g.createEntity(owner,ObjectKind::Folder,"Alvo");pole=g.createEntity(owner,ObjectKind::Folder,"Polo");
    Transform a;a.position[0]=.6f;a.position[1]=.6f;g.setTransform(target,a);
    Transform p;p.position[1]=1;p.position[2]=2;g.setTransform(pole,p);
  }
  void ik(float weight,bool pole=true) {
    auto v=*g.find(foot);auto *c=static_cast<scene::TwoBoneIk*>(v.components.add(scene::TwoBoneIk::descriptor));
    c->target=target;c->pole=pole?this->pole:0;c->weight=weight;g.applyEntityValues(foot,v);
  }
};
}

AE_TEST(ik_two_bone_component_reaches_target_in_play_without_stretch_or_accumulation) {
  Leg leg;leg.ik(1);
  GameWorld world;AE_EXPECT_TRUE(world.load(leg.g),"play world");
  SceneIk ik;
  for(u32 frame=0;frame<5;++frame) {world.beginFrame(1/60.);AE_EXPECT_TRUE(ik.advance(world),"advance");}
  float foot[16],target[16],hip[16],knee[16];
  worldMatrix(world.graph(),leg.foot,foot);worldMatrix(world.graph(),leg.target,target);worldMatrix(world.graph(),leg.hip,hip);worldMatrix(world.graph(),leg.knee,knee);
  AE_EXPECT_TRUE(distance(foot+12,target+12)<1e-3f,"tip reaches target");
  AE_EXPECT_TRUE(close(distance(hip+12,knee+12),std::sqrt(1.01f))&&close(distance(knee+12,foot+12),1),"bone lengths preserved");
  AE_EXPECT_TRUE(knee[14]>0,"pole keeps the knee forward");
  AE_EXPECT_TRUE(ik.diagnostics().empty(),"no diagnostic for reachable target");
}

AE_TEST(ik_weight_blends_from_the_frame_input_and_zero_restores) {
  Leg leg;leg.ik(.5f);
  GameWorld world;world.load(leg.g);SceneIk ik;
  float before[16];worldMatrix(world.graph(),leg.foot,before);
  float first[16],later[16];
  world.beginFrame(1/60.);ik.advance(world);worldMatrix(world.graph(),leg.foot,first);
  for(u32 i=0;i<10;++i){world.beginFrame(1/60.);ik.advance(world);}
  worldMatrix(world.graph(),leg.foot,later);
  AE_EXPECT_TRUE(distance(first+12,later+12)<1e-4f,"half weight does not creep toward the target across frames");
  AE_EXPECT_TRUE(distance(first+12,before+12)>.1f,"half weight moves the tip");
  // Peso zero (desativado) devolve imediatamente a pose de entrada.
  const auto component=world.findComponent(world.handle(leg.foot),"astra.ik.two_bone");
  AE_EXPECT_TRUE(world.setProperty(component,"enabled",scene::ComponentPropertyValue{false})==WorldStatus::Ok,"component editable in play");
  world.beginFrame(1/60.);ik.advance(world);
  float restored[16];worldMatrix(world.graph(),leg.foot,restored);
  AE_EXPECT_TRUE(distance(restored+12,before+12)<1e-4f,"disabling returns the previous contribution");
}

AE_TEST(ik_unreachable_and_singular_targets_stay_finite_and_report) {
  Leg leg;leg.ik(1,false);
  Transform far;far.position[0]=50;leg.g.setTransform(leg.target,far);
  GameWorld world;world.load(leg.g);SceneIk ik;world.beginFrame(1/60.);ik.advance(world);
  float hip[16],knee[16],foot[16];worldMatrix(world.graph(),leg.hip,hip);worldMatrix(world.graph(),leg.knee,knee);worldMatrix(world.graph(),leg.foot,foot);
  for(float v:foot)AE_EXPECT_TRUE(std::isfinite(v),"finite tip");
  AE_EXPECT_TRUE(close(distance(knee+12,foot+12),1),"no implicit stretching");
  bool reported=false;for(const auto &d:ik.diagnostics())reported=reported||d.issue==SceneIk::Issue::NotConverged;
  AE_EXPECT_TRUE(reported,"stretch toward unreachable target is reported");
  // Alvo exatamente sobre a raiz: singularidade recusada sem NaN.
  Transform onRoot;onRoot.position[1]=2;
  GameWorld second;leg.g.setTransform(leg.target,onRoot);second.load(leg.g);SceneIk ik2;second.beginFrame(1/60.);ik2.advance(second);
  float tip2[16];worldMatrix(second.graph(),leg.foot,tip2);for(float v:tip2)AE_EXPECT_TRUE(std::isfinite(v),"singular target finite");
}

AE_TEST(ik_removed_target_is_diagnosed_and_component_round_trips) {
  Leg leg;leg.ik(1);
  std::ostringstream out;leg.g.find(leg.foot)->components.find(scene::TwoBoneIk::descriptor)->write(out);
  scene::TwoBoneIk copy;std::istringstream in(out.str());
  AE_EXPECT_TRUE(copy.read(in,1)&&copy.target==leg.target&&copy.pole==leg.pole&&copy.weight==1,"serialization round trip");
  leg.g.destroyEntity(leg.target);
  GameWorld world;world.load(leg.g);SceneIk ik;world.beginFrame(1/60.);ik.advance(world);
  bool missing=false;for(const auto &d:ik.diagnostics())missing=missing||d.issue==SceneIk::Issue::MissingTarget;
  AE_EXPECT_TRUE(missing,"removed target leaves an actionable diagnostic");
}

AE_TEST(ik_chain_ccd_and_fabrik_converge_with_residual_report) {
  for(u32 solver=0;solver<2;++solver) {
    SceneGraph g;const auto owner=g.createEntity(g.root(),ObjectKind::Folder,"Cauda");
    ObjectId parent=owner,tip=0;
    for(u32 i=0;i<5;++i){tip=g.createEntity(parent,ObjectKind::Folder,"Seg");Transform t;t.position[0]=i?1.f:0.f;g.setTransform(tip,t);parent=tip;}
    const auto target=g.createEntity(owner,ObjectKind::Folder,"Alvo");Transform a;a.position[0]=2;a.position[1]=2.5f;g.setTransform(target,a);
    auto v=*g.find(tip);auto *c=static_cast<scene::ChainIk*>(v.components.add(scene::ChainIk::descriptor));
    c->target=target;c->chainLength=4;c->solver=solver;c->iterations=64;c->tolerance=.001f;g.applyEntityValues(tip,v);
    GameWorld world;world.load(g);SceneIk ik;world.beginFrame(1/60.);AE_EXPECT_TRUE(ik.advance(world),"chain advance");
    float tipWorld[16],goal[16];worldMatrix(world.graph(),tip,tipWorld);worldMatrix(world.graph(),target,goal);
    AE_EXPECT_TRUE(distance(tipWorld+12,goal+12)<.01f,solver?"FABRIK reaches":"CCD reaches");
    AE_EXPECT_TRUE(ik.diagnostics().empty(),"converged chain has no diagnostic");
  }
}

AE_TEST(ik_foot_grounding_follows_ground_query_and_skips_air_phase) {
  Leg leg;
  auto v=*leg.g.find(leg.foot);v.components.add(scene::FootGrounding::descriptor);leg.g.applyEntityValues(leg.foot,v);
  GameWorld world;world.load(leg.g);SceneIk ik;
  // Rampa: chao em y=0.25 com normal inclinada.
  const auto ramp=[](const float *origin,const float *direction,ObjectId,float *point,float *normal) {
    (void)direction;point[0]=origin[0];point[1]=.25f;point[2]=origin[2];normal[0]=0;normal[1]=.9701425f;normal[2]=.2425356f;return true;};
  world.beginFrame(1/60.);AE_EXPECT_TRUE(ik.advance(world,ramp),"grounded advance");
  float foot[16];worldMatrix(world.graph(),leg.foot,foot);
  AE_EXPECT_TRUE(close(foot[13],.25f,2e-3f),"foot rests on the surface within max correction");
  AE_EXPECT_TRUE(foot[6]>.05f,"foot tilts with the surface normal");
  // Sem chao: fase aerea, pose de entrada intacta.
  GameWorld air;air.load(leg.g);SceneIk ik2;air.beginFrame(1/60.);
  ik2.advance(air,[](const float*,const float*,ObjectId,float*,float*){return false;});
  float airFoot[16];worldMatrix(air.graph(),leg.foot,airFoot);
  AE_EXPECT_TRUE(close(airFoot[13],0,1e-4f),"air phase keeps the animated pose");
}

AE_TEST(ik_editor_creates_permanent_two_bone_ik_with_single_undo) {
  editor::EditorSession session;auto &doc=session.document();
  const auto owner=doc.createEntity(doc.root(),editor::EditorEntityKind::Folder,"Robo");
  const auto hip=doc.createEntity(owner,editor::EditorEntityKind::Folder,"Coxa"),knee=doc.createEntity(hip,editor::EditorEntityKind::Folder,"Canela"),foot=doc.createEntity(knee,editor::EditorEntityKind::Folder,"Pe");
  Transform k;k.position[1]=-1;k.position[2]=.1f;doc.setTransform(knee,k);Transform f;f.position[1]=-1;doc.setTransform(foot,f);
  std::string error;const usize before=doc.childrenOf(owner).size();
  AE_EXPECT_TRUE(session.createTwoBoneIkForTip(foot,error),error.c_str());
  const auto *ik=static_cast<const scene::TwoBoneIk*>(doc.find(foot)->components.find(scene::TwoBoneIk::descriptor));
  AE_EXPECT_TRUE(ik&&doc.exists(static_cast<ObjectId>(ik->target))&&doc.exists(static_cast<ObjectId>(ik->pole)),"target and pole are real scene objects");
  AE_EXPECT_EQ(doc.childrenOf(owner).size(),before+2,"target and pole live under the character root");
  AE_EXPECT_TRUE(session.history().undo(doc)&&!doc.find(foot)->components.find(scene::TwoBoneIk::descriptor)&&doc.childrenOf(owner).size()==before,"one undo removes component, target and pole");
}
