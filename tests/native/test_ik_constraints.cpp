// N04: IK por componente. Mesmos solvers no estudio e no Play; peso sem
// acumulo entre quadros; sem NaN, sem alongar osso; alvo removido diagnostica;
// persistencia; apoio do pe pela consulta de chao; criacao pelo editor com Undo.
#include "harness.h"
#include "runtime/scene_ik.h"
#include "runtime/ik_solvers.h"
#include "runtime/transform_math.h"
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

namespace {
template<class T> T *addComponent(SceneGraph &g,ObjectId id) {
  auto v=*g.find(id);auto *c=static_cast<T*>(v.components.add(T::descriptor));T copy=*c;(void)copy;g.applyEntityValues(id,v);
  return static_cast<T*>(g.editComponents(id)->edit(T::descriptor));
}
float forwardDot(const SceneGraph &g,ObjectId id,u32 axis,const float target[3]) {
  float m[16];worldMatrix(g,id,m);float a[3];ikAxisWorld(m,axis,a);
  float d[3]{target[0]-m[12],target[1]-m[13],target[2]-m[14]};const float n=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
  return (a[0]*d[0]+a[1]*d[1]+a[2]*d[2])/n;
}
struct Spine {
  SceneGraph g;ObjectId owner=0,hips=0,chest=0,head=0,target=0;
  Spine() {
    owner=g.createEntity(g.root(),ObjectKind::Folder,"Corpo");
    hips=g.createEntity(owner,ObjectKind::Folder,"Quadril");chest=g.createEntity(hips,ObjectKind::Folder,"Peito");head=g.createEntity(chest,ObjectKind::Folder,"Cabeca");
    Transform t;t.position[1]=1;g.setTransform(chest,t);g.setTransform(head,t);
    target=g.createEntity(owner,ObjectKind::Folder,"Alvo");Transform a;a.position[0]=3;a.position[1]=2;g.setTransform(target,a);
  }
};
}

AE_TEST(ik_look_at_turns_spine_and_head_toward_target_within_clamp) {
  Spine s;auto *c=addComponent<scene::LookAtIk>(s.g,s.head);c->target=s.target;c->chainLength=2;c->bodyWeight=.3f;c->headWeight=1;c->axis=2;
  GameWorld world;world.load(s.g);SceneIk ik;
  float targetWorld[16];worldMatrix(world.graph(),s.target,targetWorld);
  const float before=forwardDot(world.graph(),s.head,2,targetWorld+12);
  world.beginFrame(1/60.);AE_EXPECT_TRUE(ik.advance(world),"look at");
  const float after=forwardDot(world.graph(),s.head,2,targetWorld+12);
  AE_EXPECT_TRUE(after>.99f&&after>before,"head faces the target");
  float chestRot[16];worldMatrix(world.graph(),s.chest,chestRot);
  AE_EXPECT_TRUE(std::abs(chestRot[8])>1e-3f,"body weight turns the spine too");
  // Limite de 20 graus: a cabeca nao gira alem disso.
  Spine s2;auto *c2=addComponent<scene::LookAtIk>(s2.g,s2.head);c2->target=s2.target;c2->chainLength=1;c2->clampAngle=20;c2->axis=2;
  GameWorld w2;w2.load(s2.g);SceneIk ik2;w2.beginFrame(1/60.);ik2.advance(w2);
  float m[16];worldMatrix(w2.graph(),s2.head,m);
  AE_EXPECT_TRUE(std::abs(std::acos(std::clamp(m[10],-1.f,1.f))*180/3.14159265f-20)<1.f,"clamp angle respected");
}

AE_TEST(ik_aim_points_tip_axis_at_target) {
  Spine s;auto *c=addComponent<scene::AimIk>(s.g,s.head);c->target=s.target;c->chainLength=2;c->iterations=8;c->falloff=0;c->axis=2;
  GameWorld world;world.load(s.g);SceneIk ik;world.beginFrame(1/60.);AE_EXPECT_TRUE(ik.advance(world),"aim");
  float t[16];worldMatrix(world.graph(),s.target,t);
  AE_EXPECT_TRUE(forwardDot(world.graph(),s.head,2,t+12)>.995f,"aim axis on target");
}

AE_TEST(ik_rotation_limit_clamps_solver_output) {
  Leg leg;leg.ik(1);
  auto *limit=addComponent<scene::RotationLimit>(leg.g,leg.knee);limit->kind=1;limit->axis=0;limit->minimum=-10;limit->maximum=10;
  Transform close;close.position[0]=.1f;close.position[1]=1.2f;close.position[2]=.2f;leg.g.setTransform(leg.target,close);
  GameWorld world;world.load(leg.g);SceneIk ik;world.beginFrame(1/60.);ik.advance(world);
  const auto &knee=world.graph().find(leg.knee)->transform;
  float q[4];transformRotationQuaternion(knee,q);
  AE_EXPECT_TRUE(2*std::acos(std::clamp(std::abs(q[3]),0.f,1.f))*180/3.14159265f<=10.5f,"hinge limit applied after the solver");
}

AE_TEST(ik_grounder_lowers_pelvis_for_the_lowest_foot) {
  Leg leg;
  auto *g=addComponent<scene::GrounderBody>(leg.g,leg.owner);g->pelvis=leg.hip;g->maxPelvisOffset=.5f;
  auto *f=addComponent<scene::FootGrounding>(leg.g,leg.foot);f->rayDistance=2;f->maxCorrection=.5f;
  GameWorld world;world.load(leg.g);SceneIk ik;
  float before[16];worldMatrix(world.graph(),leg.hip,before);
  world.beginFrame(1/60.);
  ik.advance(world,[](const float *o,const float *,ObjectId,float *p,float *n){p[0]=o[0];p[1]=-.3f;p[2]=o[2];n[0]=0;n[1]=1;n[2]=0;return true;});
  float after[16],foot[16];worldMatrix(world.graph(),leg.hip,after);worldMatrix(world.graph(),leg.foot,foot);
  AE_EXPECT_TRUE(after[13]<before[13]-.25f,"pelvis lowered toward the lower ground");
  AE_EXPECT_TRUE(std::abs(foot[13]+.3f)<5e-3f,"foot reaches the lower ground");
}

AE_TEST(ik_finger_curl_offset_and_twist_relaxer_act_on_real_bones) {
  Spine s;auto *finger=addComponent<scene::FingerCurl>(s.g,s.chest);finger->curl=1;finger->maxAngle=30;finger->chainLength=2;finger->axis=0;
  GameWorld world;world.load(s.g);SceneIk ik;world.beginFrame(1/60.);ik.advance(world);
  AE_EXPECT_TRUE(std::abs(world.graph().find(s.chest)->transform.rotationDegrees[0]-30)<.1f&&std::abs(world.graph().find(s.head)->transform.rotationDegrees[0]-30)<.1f,"curl bends each bone");
  Leg leg;leg.ik(1);auto *offset=addComponent<scene::IkOffset>(leg.g,leg.foot);offset->offset[1]=.2f;
  GameWorld w2;w2.load(leg.g);SceneIk ik2;w2.beginFrame(1/60.);ik2.advance(w2);
  float foot[16],target[16];worldMatrix(w2.graph(),leg.foot,foot);worldMatrix(w2.graph(),leg.target,target);
  AE_EXPECT_TRUE(std::abs(foot[13]-(target[13]+.2f))<2e-3f,"effector offset shifts the reached point");
  Spine t;Transform twisted=t.g.find(t.head)->transform;twisted.rotationDegrees[1]=60;t.g.setTransform(t.head,twisted);
  addComponent<scene::TwistRelaxer>(t.g,t.chest);
  GameWorld w3;w3.load(t.g);float headBefore[16];worldMatrix(w3.graph(),t.head,headBefore);SceneIk ik3;w3.beginFrame(1/60.);ik3.advance(w3);
  float headAfter[16];worldMatrix(w3.graph(),t.head,headAfter);
  AE_EXPECT_TRUE(std::abs(w3.graph().find(t.chest)->transform.rotationDegrees[1]-30)<.5f,"half of the twist moves to the middle bone");
  for(u32 i=0;i<12;++i)AE_EXPECT_TRUE(std::abs(headAfter[i]-headBefore[i])<1e-3f,"child keeps its world pose");
}
