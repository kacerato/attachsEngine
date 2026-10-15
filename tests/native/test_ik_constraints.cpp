// N04: IK por componente. Mesmos solvers no estudio e no Play; peso sem
// acumulo entre quadros; sem NaN, sem alongar osso; alvo removido diagnostica;
// persistencia; apoio do pe pela consulta de chao; criacao pelo editor com Undo.
#include "harness.h"
#include "runtime/scene_ik.h"
#include "scene/ik_effects.h"
#include "scene/physics_body.h"
#include "scene/collider.h"
#include "runtime/scene_physics.h"
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
  AE_EXPECT_TRUE(copy.read(in,2)&&copy.target==leg.target&&copy.pole==leg.pole&&copy.weight==1,"serialization round trip");
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

// ---- Pacote C: efeitos procedurais, interacao e corpo inteiro ----
namespace {
void step(GameWorld &world,SceneIk &ik,u32 frames=1,const SceneIk::GroundQuery &ground={}) {
  for(u32 i=0;i<frames;++i) {world.beginFrame(1/60.);ik.advance(world,ground);world.flush();}
}
float worldX(const GameWorld &w,ObjectId id,u32 component=0) {float m[16];worldMatrix(w.graph(),id,m);return m[12+component];}
}

AE_TEST(ik_hit_reaction_pushes_the_chain_and_recovers) {
  Spine s;auto *c=addComponent<scene::HitReaction>(s.g,s.head);c->strength=30;c->duration=.4f;c->chainLength=2;
  GameWorld world;world.load(s.g);SceneIk ik;step(world,ik);
  const float rest=worldX(world,s.head);
  const float right[3]{1,0,0};
  AE_EXPECT_TRUE(ik.hit(world,s.head,right,1)==WorldStatus::Ok,"hit accepted");
  AE_EXPECT_TRUE(ik.hit(world,s.hips,right,1)==WorldStatus::ComponentMissing,"hit refuses a bone without the component");
  step(world,ik,6);
  AE_EXPECT_TRUE(worldX(world,s.head)>rest+.1f,"head pushed along the blow");
  step(world,ik,30);
  AE_EXPECT_TRUE(std::abs(worldX(world,s.head)-rest)<1e-3f,"pose recovers after the duration");
  const float normal[3]{-1,0,0};ik.collision(world,s.head,normal);step(world,ik,6);
  AE_EXPECT_TRUE(worldX(world,s.head)>rest+.1f,"solid contact triggers the same reaction");
}

AE_TEST(ik_recoil_lifts_and_kicks_back_then_returns) {
  Spine s;auto *c=addComponent<scene::Recoil>(s.g,s.head);c->kickAngle=20;c->kickBack=.1f;c->duration=.3f;c->chainLength=2;c->axis=2;
  GameWorld world;world.load(s.g);SceneIk ik;step(world,ik);
  float before[16];worldMatrix(world.graph(),s.head,before);const float hipsZ=worldX(world,s.hips,2);
  AE_EXPECT_TRUE(ik.fire(world,s.head,1)==WorldStatus::Ok,"fire accepted");
  step(world,ik,3);
  float now[16];worldMatrix(world.graph(),s.head,now);
  AE_EXPECT_TRUE(now[9]>before[9]+.05f,"barrel axis lifted");
  AE_EXPECT_TRUE(worldX(world,s.hips,2)<hipsZ-.03f,"chain base kicked back");
  step(world,ik,30);worldMatrix(world.graph(),s.head,now);
  for(u32 i=0;i<16;++i)AE_EXPECT_TRUE(std::abs(now[i]-before[i])<1e-3f,"recoil returns to the frame pose");
}

AE_TEST(ik_inertia_lags_behind_motion_and_settles) {
  Spine s;addComponent<scene::Inertia>(s.g,s.chest)->stiffness=60;
  GameWorld world;world.load(s.g);SceneIk ik;step(world,ik);
  Transform moved;moved.position[0]=1;world.setLocalTransform(world.handle(s.owner),moved);
  step(world,ik);
  AE_EXPECT_TRUE(worldX(world,s.head)<worldX(world,s.chest)-.01f,"tip trails the sudden move");
  step(world,ik,240);
  AE_EXPECT_TRUE(std::abs(worldX(world,s.head)-worldX(world,s.chest))<2e-3f,"spring settles on the animated pose");
}

AE_TEST(ik_amplifier_exaggerates_the_animated_rotation) {
  Spine s;addComponent<scene::Amplifier>(s.g,s.chest)->amount=1;
  GameWorld world;world.load(s.g);SceneIk ik;step(world,ik);
  Transform animated=world.graph().find(s.chest)->transform;animated.rotationDegrees[0]=10;world.setLocalTransform(world.handle(s.chest),animated);
  step(world,ik);
  AE_EXPECT_TRUE(std::abs(world.graph().find(s.chest)->transform.rotationDegrees[0]-20)<.2f,"twice the animated delta");
  step(world,ik,3);
  AE_EXPECT_TRUE(std::abs(world.graph().find(s.chest)->transform.rotationDegrees[0]-20)<.2f,"no accumulation across frames");
}

AE_TEST(ik_body_tilt_leans_into_turns) {
  Spine s;auto *c=addComponent<scene::BodyTilt>(s.g,s.chest);c->tiltPerTurn=.2f;c->maxAngle=20;c->smoothing=0;
  GameWorld world;world.load(s.g);SceneIk ik;step(world,ik);
  const auto turn=[&](float yaw) {Transform t;t.rotationDegrees[1]=yaw;world.setLocalTransform(world.handle(s.owner),t);step(world,ik);};
  for(u32 i=1;i<=5;++i)turn(3.f*float(i));
  float m[16];worldMatrix(world.graph(),s.chest,m);
  AE_EXPECT_TRUE(m[5]<std::cos(15*3.14159265f/180),"chest tilted while turning");
  const float a=15*3.14159265f/180,side=m[4]*std::cos(a)-m[6]*std::sin(a);
  for(u32 i=1;i<=10;++i)turn(15-3.f*float(i));
  worldMatrix(world.graph(),s.chest,m);
  const float b=-15*3.14159265f/180,other=m[4]*std::cos(b)-m[6]*std::sin(b);
  AE_EXPECT_TRUE(side*other<0,"opposite turn leans to the other side");
}

AE_TEST(ik_hand_poser_copies_reference_rotations_by_order) {
  SceneGraph g;const auto hand=g.createEntity(g.root(),ObjectKind::Folder,"Mao");
  const auto f1=g.createEntity(hand,ObjectKind::Folder,"Dedo");const auto f2=g.createEntity(f1,ObjectKind::Folder,"Ponta");
  const auto ref=g.createEntity(g.root(),ObjectKind::Folder,"Pose");
  const auto r1=g.createEntity(ref,ObjectKind::Folder,"R1");const auto r2=g.createEntity(r1,ObjectKind::Folder,"R2");
  Transform bent;bent.rotationDegrees[0]=40;g.setTransform(r1,bent);g.setTransform(r2,bent);
  addComponent<scene::HandPoser>(g,hand)->reference=ref;
  GameWorld world;world.load(g);SceneIk ik;step(world,ik);
  AE_EXPECT_TRUE(std::abs(world.graph().find(f1)->transform.rotationDegrees[0]-40)<.1f&&std::abs(world.graph().find(f2)->transform.rotationDegrees[0]-40)<.1f,"fingers take the reference pose");
  AE_EXPECT_TRUE(std::abs(world.graph().find(hand)->transform.rotationDegrees[0])<1e-4f,"the hand itself stays with the IK/animation");
}

AE_TEST(ik_penetration_avoidance_stops_target_at_the_wall) {
  Leg leg;leg.ik(1);addComponent<scene::PenetrationAvoidance>(leg.g,leg.foot)->radius=.05f;
  GameWorld world;world.load(leg.g);SceneIk ik;
  const SceneIk::GroundQuery wall=[](const float *o,const float *d,ObjectId,float *p,float *n) {
    if(d[0]<=0)return false;
    const float t=(.3f-o[0])/d[0];
    if(t<0||t>1)return false;
    for(u32 i=0;i<3;++i)p[i]=o[i]+d[i]*t;
    n[0]=-1;n[1]=0;n[2]=0;return true;};
  step(world,ik,1,wall);
  AE_EXPECT_TRUE(std::abs(worldX(world,leg.foot)-.25f)<3e-3f,"tip stops at the surface minus the clearance");
}

AE_TEST(ik_interaction_reaches_picks_up_and_releases_with_events) {
  Leg leg;
  addComponent<scene::InteractionSystem>(leg.g,leg.owner);
  const auto prop=leg.g.createEntity(leg.g.root(),ObjectKind::Folder,"Caixa");
  Transform at;at.position[0]=.6f;at.position[1]=.6f;leg.g.setTransform(prop,at);
  auto *object=addComponent<scene::InteractionObject>(leg.g,prop);object->reachTime=.2f;object->releaseTime=.1f;object->pickUp=true;
  GameWorld world;world.load(leg.g);SceneIk ik;step(world,ik);
  bool started=false;
  AE_EXPECT_TRUE(ik.interactionStart(world,leg.owner,leg.foot,leg.target,started)==WorldStatus::Ok&&!started,"plain object is not interactable");
  AE_EXPECT_TRUE(ik.interactionStart(world,leg.owner,leg.foot,prop,started)==WorldStatus::Ok&&started,"interaction starts");
  std::vector<SceneIk::Event> events;
  for(u32 i=0;i<14;++i) {step(world,ik);for(auto &e:ik.takeEvents())events.push_back(e);}
  AE_EXPECT_TRUE(!events.empty()&&std::string(events.front().id)=="reached"&&events.front().object==prop,"reached event with hand and object");
  AE_EXPECT_TRUE(world.graph().find(prop)->parent==leg.foot,"object picked up into the hand");
  float propWorld[16];worldMatrix(world.graph(),prop,propWorld);
  float handWorld[16];worldMatrix(world.graph(),leg.foot,handWorld);
  AE_EXPECT_TRUE(std::abs(propWorld[12]-handWorld[12])<1e-2f&&std::abs(propWorld[13]-handWorld[13])<1e-2f,"picked object is carried exactly at the hand");
  AE_EXPECT_TRUE(ik.interactionStop(world,leg.owner,leg.foot)==WorldStatus::Ok,"stop");
  for(u32 i=0;i<12;++i) {step(world,ik);for(auto &e:ik.takeEvents())events.push_back(e);}
  AE_EXPECT_TRUE(world.graph().find(prop)->parent==world.graph().root(),"stop drops the object back");
  AE_EXPECT_TRUE(!events.empty()&&std::string(events.back().id)=="released","released event");
}

// Final IK 2.2, InteractionTarget: a pegada gira sobre o pivo para encarar a
// mao (objeto simetrico pego de qualquer lado). Referencia oficial: XRI 3.0
// XRGrabInteractable (attach transform, match attach rotation).
AE_TEST(ik_interaction_grip_turns_the_pivot_so_the_grip_faces_the_hand) {
  Leg leg;addComponent<scene::InteractionSystem>(leg.g,leg.owner);
  const auto prop=leg.g.createEntity(leg.g.root(),ObjectKind::Folder,"Caneca");
  Transform at;at.position[0]=.6f;at.position[1]=.6f;leg.g.setTransform(prop,at);
  const auto grip=leg.g.createEntity(prop,ObjectKind::Folder,"Asa");
  Transform g;g.position[0]=.3f;leg.g.setTransform(grip,g);
  auto *object=addComponent<scene::InteractionObject>(leg.g,prop);object->grip=grip;object->reachTime=.2f;
  auto *pivot=addComponent<scene::InteractionGrip>(leg.g,prop);pivot->grip=grip;pivot->axis=1;pivot->twistWeight=1;
  GameWorld world;world.load(leg.g);SceneIk ik;step(world,ik);
  float before[16];worldMatrix(world.graph(),grip,before);
  AE_EXPECT_TRUE(before[12]>.8f,"the handle starts pointing away from the hand (+X)");
  bool started=false;AE_EXPECT_TRUE(ik.interactionStart(world,leg.owner,leg.foot,prop,started)==WorldStatus::Ok&&started,"start");
  step(world,ik);
  float after[16],hand[16],propWorld[16];worldMatrix(world.graph(),grip,after);worldMatrix(world.graph(),leg.foot,hand);worldMatrix(world.graph(),prop,propWorld);
  const float tangent[2]{after[12]-propWorld[12],after[14]-propWorld[14]},toHand[2]{hand[12]-propWorld[12],hand[14]-propWorld[14]};
  const float cosine=(tangent[0]*toHand[0]+tangent[1]*toHand[1])/(std::hypot(tangent[0],tangent[1])*std::hypot(toHand[0],toHand[1]));
  AE_EXPECT_TRUE(cosine>.95f,"the handle now points at the hand around the Y pivot");
  AE_EXPECT_TRUE(std::abs(propWorld[13]-.6f)<1e-4f,"twist keeps the object in place");
  step(world,ik,12);
  float later[16];worldMatrix(world.graph(),grip,later);
  AE_EXPECT_TRUE(std::abs(later[12]-after[12])<.3f,"rotate once: the pivot does not keep chasing the hand");
}

// Final IK 2.2, InteractionTrigger: faixa (raio + angulo) a frente do objeto;
// o personagem dentro dela dispara a interacao. Referencia oficial: XRI 3.0
// XR Direct Interactor (selecao por proximidade).
AE_TEST(ik_interaction_trigger_fires_when_the_character_is_in_range) {
  Leg leg;auto *system=addComponent<scene::InteractionSystem>(leg.g,leg.owner);system->autoEffector=leg.foot;
  const auto prop=leg.g.createEntity(leg.g.root(),ObjectKind::Folder,"Alavanca");
  Transform at;at.position[0]=.3f;at.position[1]=.6f;at.position[2]=.2f;leg.g.setTransform(prop,at);
  auto *object=addComponent<scene::InteractionObject>(leg.g,prop);object->reachTime=.2f;
  auto *trigger=addComponent<scene::InteractionTrigger>(leg.g,prop);trigger->radius=.5f;trigger->maxAngle=180;trigger->automatic=false;
  GameWorld world;world.load(leg.g);SceneIk ik;
  std::vector<SceneIk::Event> events;
  for(u32 i=0;i<3;++i) {step(world,ik);for(auto &e:ik.takeEvents())events.push_back(e);}
  AE_EXPECT_TRUE(events.size()==1&&std::string(events.front().id)=="entered"&&events.front().object==prop,"entering the range raises one event and nothing else");
  bool started=false;AE_EXPECT_TRUE(ik.interactionTrigger(world,leg.owner,started)==WorldStatus::Ok&&started,"trigger starts the interaction with the auto hand");
  for(u32 i=0;i<14;++i) {step(world,ik);for(auto &e:ik.takeEvents())events.push_back(e);}
  AE_EXPECT_TRUE(std::string(events.back().id)=="reached"&&events.back().effector==leg.foot,"the hand reaches the lever");
  // Fora da faixa: nada dispara e o gatilho avisa que saiu.
  trigger->radius=.1f;
  Leg far;auto *system2=addComponent<scene::InteractionSystem>(far.g,far.owner);system2->autoEffector=far.foot;
  const auto prop2=far.g.createEntity(far.g.root(),ObjectKind::Folder,"Longe");Transform away;away.position[0]=3;far.g.setTransform(prop2,away);
  addComponent<scene::InteractionObject>(far.g,prop2);auto *t2=addComponent<scene::InteractionTrigger>(far.g,prop2);t2->radius=.5f;t2->automatic=true;
  GameWorld world2;world2.load(far.g);SceneIk ik2;step(world2,ik2,3);
  AE_EXPECT_TRUE(ik2.takeEvents().empty(),"out of range: no event, no interaction");
  bool started2=true;AE_EXPECT_TRUE(ik2.interactionTrigger(world2,far.owner,started2)==WorldStatus::Ok&&!started2,"manual trigger out of range refuses");
}

// Final IK 2.2, InteractionLookAt: a cabeca olha o objeto enquanto a mao vai
// ate ele e volta depois. Referencia oficial: Godot 4.3 LookAtModifier3D.
AE_TEST(ik_interaction_look_at_turns_the_head_while_reaching) {
  Spine s;auto *system=addComponent<scene::InteractionSystem>(s.g,s.owner);system->lookAt=s.head;system->lookSpeed=8;system->lookTurn=30;
  auto *look=addComponent<scene::LookAtIk>(s.g,s.head);look->target=0;look->chainLength=1;look->headWeight=1;look->axis=2;
  const auto prop=s.g.createEntity(s.g.root(),ObjectKind::Folder,"Copo");Transform at;at.position[0]=2;at.position[1]=2;s.g.setTransform(prop,at);
  auto *object=addComponent<scene::InteractionObject>(s.g,prop);object->reachTime=1;object->releaseTime=.1f;
  GameWorld world;world.load(s.g);SceneIk ik;step(world,ik);
  AE_EXPECT_TRUE(ik.diagnostics().empty(),"look-at without a target waits for the interaction instead of complaining");
  float head[16];worldMatrix(world.graph(),s.head,head);
  AE_EXPECT_TRUE(std::abs(head[8])<1e-3f,"head starts looking forward");
  bool started=false;AE_EXPECT_TRUE(ik.interactionStart(world,s.owner,s.head,prop,started)==WorldStatus::Ok&&started,"start");
  step(world,ik,20);worldMatrix(world.graph(),s.head,head);
  AE_EXPECT_TRUE(head[8]>.5f,"while reaching, the head turns toward the object");
  AE_EXPECT_TRUE(ik.interactionStop(world,s.owner,s.head)==WorldStatus::Ok,"stop");
  step(world,ik,60);worldMatrix(world.graph(),s.head,head);
  AE_EXPECT_TRUE(std::abs(head[8])<.1f,"after release the look-at fades out and the head returns");
}

AE_TEST(ik_full_body_pulls_pelvis_to_reach_far_targets) {
  Leg leg;
  const auto far=leg.g.createEntity(leg.g.root(),ObjectKind::Folder,"Longe");Transform t;t.position[1]=-.5f;leg.g.setTransform(far,t);
  auto *c=addComponent<scene::FullBodyIk>(leg.g,leg.owner);c->pelvis=leg.hip;c->tips[2]=leg.foot;c->targets[2]=far;c->bodyPull=1;c->maxBodyOffset=1;
  GameWorld world;world.load(leg.g);SceneIk ik;
  const float hipBefore=worldX(world,leg.hip,1);step(world,ik);
  const float hipFirst=worldX(world,leg.hip,1);
  float foot[16];worldMatrix(world.graph(),leg.foot,foot);
  AE_EXPECT_TRUE(worldX(world,leg.hip,1)<hipBefore-.4f,"pelvis pulled toward the far target");
  AE_EXPECT_TRUE(std::abs(foot[13]+.5f)<1e-2f&&std::abs(foot[12])<1e-2f,"limb reaches after the body moves");
  step(world,ik,5);
  AE_EXPECT_TRUE(std::abs(worldX(world,leg.hip,1)-hipFirst)<1e-3f,"no accumulation of the pelvis pull");
}

AE_TEST(ik_pack_c_components_round_trip) {
  const scene::ComponentType *types[]{&scene::HitReaction::descriptor,&scene::Recoil::descriptor,&scene::Inertia::descriptor,&scene::Amplifier::descriptor,
    &scene::BodyTilt::descriptor,&scene::HandPoser::descriptor,&scene::PenetrationAvoidance::descriptor,&scene::InteractionObject::descriptor,
    &scene::InteractionSystem::descriptor,&scene::FullBodyIk::descriptor};
  for(const auto *type:types) {
    const auto value=type->create();std::ostringstream out;value->write(out);
    const auto copy=type->create();std::istringstream in(out.str());
    AE_EXPECT_TRUE(value->valid()&&copy->read(in,type->version)&&copy->valid(),"component persists");
  }
}

// ---- Sem pendencias: prioridade, FABRIK em arvore, corpo por alvos, membros longos ----
namespace {
struct Fork {
  SceneGraph g;ObjectId owner=0,root=0,a1=0,a2=0,b1=0,b2=0,ta=0,tb=0;
  Fork() {
    owner=g.createEntity(g.root(),ObjectKind::Folder,"Garra");root=g.createEntity(owner,ObjectKind::Folder,"Raiz");
    a1=g.createEntity(root,ObjectKind::Folder,"A1");a2=g.createEntity(a1,ObjectKind::Folder,"A2");
    b1=g.createEntity(root,ObjectKind::Folder,"B1");b2=g.createEntity(b1,ObjectKind::Folder,"B2");
    Transform r;r.position[1]=1;g.setTransform(root,r);
    Transform px;px.position[0]=1;g.setTransform(a1,px);g.setTransform(a2,px);
    Transform nx;nx.position[0]=-1;g.setTransform(b1,nx);g.setTransform(b2,nx);
    ta=g.createEntity(g.root(),ObjectKind::Folder,"AlvoA");tb=g.createEntity(g.root(),ObjectKind::Folder,"AlvoB");
  }
  void place(ObjectId id,float x,float y) {Transform t;t.position[0]=x;t.position[1]=y;g.setTransform(id,t);}
};
float gap(const GameWorld &w,ObjectId a,ObjectId b) {float m[16],n[16];worldMatrix(w.graph(),a,m);worldMatrix(w.graph(),b,n);return distance(m+12,n+12);}
}

AE_TEST(ik_priority_orders_solvers_that_share_bones) {
  for(const bool aFirst:{true,false}) {
    Fork f;f.place(f.ta,1.4f,2.2f);f.place(f.tb,-1.2f,.2f);
    auto *a=addComponent<scene::ChainIk>(f.g,f.a2);a->target=f.ta;a->chainLength=2;a->priority=aFirst?10:0;
    auto *b=addComponent<scene::ChainIk>(f.g,f.b2);b->target=f.tb;b->chainLength=2;b->priority=aFirst?0:10;
    GameWorld world;world.load(f.g);SceneIk ik;step(world,ik);
    // Quem resolve por ultimo tem a palavra final no osso comum (Raiz).
    const float last=aFirst?gap(world,f.b2,f.tb):gap(world,f.a2,f.ta);
    AE_EXPECT_TRUE(last<2e-3f,"lower priority solves last and reaches");
  }
  std::istringstream old("7 0 1 0 1");scene::TwoBoneIk legacy;
  AE_EXPECT_TRUE(legacy.read(old,1)&&legacy.priority==0&&legacy.target==7,"version 1 scenes open with priority zero");
}

AE_TEST(ik_fabrik_root_moves_the_shared_root_so_all_chains_reach) {
  Fork f;f.place(f.ta,2,1.6f);f.place(f.tb,-2,1.6f);
  auto *c=addComponent<scene::FabrikRoot>(f.g,f.root);c->tips[0]=f.a2;c->targets[0]=f.ta;c->tips[1]=f.b2;c->targets[1]=f.tb;c->rootPin=0;c->iterations=16;
  GameWorld world;world.load(f.g);SceneIk ik;step(world,ik);
  AE_EXPECT_TRUE(gap(world,f.a2,f.ta)<1e-2f&&gap(world,f.b2,f.tb)<1e-2f,"both chains reach after the root moves");
  AE_EXPECT_TRUE(worldX(world,f.root,1)>1.4f,"shared root rose toward the targets");
  Fork pinned;pinned.place(pinned.ta,2,1.6f);pinned.place(pinned.tb,-2,1.6f);
  auto *p=addComponent<scene::FabrikRoot>(pinned.g,pinned.root);p->tips[0]=pinned.a2;p->targets[0]=pinned.ta;p->tips[1]=pinned.b2;p->targets[1]=pinned.tb;p->rootPin=1;
  GameWorld w2;w2.load(pinned.g);SceneIk ik2;step(w2,ik2);
  AE_EXPECT_TRUE(std::abs(worldX(w2,pinned.root,1)-1)<1e-4f,"pinned root stays");
  step(world,ik,5);
  AE_EXPECT_TRUE(gap(world,f.a2,f.ta)<1e-2f,"stable across frames");
}

AE_TEST(ik_body_by_targets_drives_pelvis_spine_head_and_hands) {
  SceneGraph g;const auto owner=g.createEntity(g.root(),ObjectKind::Folder,"Avatar");
  const auto hips=g.createEntity(owner,ObjectKind::Folder,"Quadril");const auto chest=g.createEntity(hips,ObjectKind::Folder,"Peito");
  const auto head=g.createEntity(chest,ObjectKind::Folder,"Cabeca");
  const auto arm=g.createEntity(chest,ObjectKind::Folder,"Braco");const auto fore=g.createEntity(arm,ObjectKind::Folder,"Antebraco");const auto hand=g.createEntity(fore,ObjectKind::Folder,"Mao");
  Transform t;t.position[1]=1;g.setTransform(hips,t);Transform up;up.position[1]=.5f;g.setTransform(chest,up);g.setTransform(head,up);
  Transform side;side.position[0]=.2f;g.setTransform(arm,side);Transform down;down.position[1]=-.3f;down.position[2]=.05f;g.setTransform(fore,down);g.setTransform(hand,down);
  const auto headTarget=g.createEntity(g.root(),ObjectKind::Folder,"AlvoCabeca");Transform ht;ht.position[0]=.3f;ht.position[1]=2;ht.rotationDegrees[1]=30;g.setTransform(headTarget,ht);
  const auto handTarget=g.createEntity(g.root(),ObjectKind::Folder,"AlvoMao");Transform hd;hd.position[0]=.7f;hd.position[1]=1.3f;hd.position[2]=.2f;g.setTransform(handTarget,hd);
  auto *c=addComponent<scene::BodyByTargets>(g,owner);c->pelvis=hips;c->chest=chest;c->head=head;c->headTarget=headTarget;c->rightHand=hand;c->rightTarget=handTarget;
  GameWorld world;world.load(g);SceneIk ik;step(world,ik);
  AE_EXPECT_TRUE(std::abs(worldX(world,hips)-.3f)<1e-2f,"pelvis follows the head target");
  AE_EXPECT_TRUE(gap(world,head,headTarget)<2e-2f,"head reaches its target");
  float hm[16];worldMatrix(world.graph(),head,hm);
  AE_EXPECT_TRUE(std::abs(hm[8]-std::sin(30*3.14159265f/180))<2e-2f,"head copies the target rotation");
  AE_EXPECT_TRUE(gap(world,hand,handTarget)<2e-3f,"hand reaches its target");
  step(world,ik,5);
  AE_EXPECT_TRUE(std::abs(worldX(world,hips)-.3f)<1e-2f,"no accumulation");
}

AE_TEST(ik_long_limbs_in_full_body_and_interaction_and_hit_front) {
  // Membro de tres ossos no corpo inteiro, com tronco.
  SceneGraph g;const auto owner=g.createEntity(g.root(),ObjectKind::Folder,"Robo");
  const auto pelvis=g.createEntity(owner,ObjectKind::Folder,"Base");const auto chest=g.createEntity(pelvis,ObjectKind::Folder,"Torre");
  const auto s1=g.createEntity(chest,ObjectKind::Folder,"S1");const auto s2=g.createEntity(s1,ObjectKind::Folder,"S2");
  const auto s3=g.createEntity(s2,ObjectKind::Folder,"S3");const auto tip=g.createEntity(s3,ObjectKind::Folder,"Pinca");
  Transform b;b.position[1]=1;g.setTransform(pelvis,b);g.setTransform(chest,b);
  Transform seg;seg.position[0]=.5f;g.setTransform(s1,seg);g.setTransform(s2,seg);g.setTransform(s3,seg);g.setTransform(tip,seg);
  const auto far=g.createEntity(g.root(),ObjectKind::Folder,"Longe");Transform ft;ft.position[0]=2.2f;ft.position[1]=1.5f;g.setTransform(far,ft);
  auto *fb=addComponent<scene::FullBodyIk>(g,owner);fb->pelvis=pelvis;fb->chest=chest;fb->tips[0]=tip;fb->targets[0]=far;fb->limbBones=3;fb->bodyPull=1;fb->maxBodyOffset=1;
  GameWorld world;world.load(g);SceneIk ik;step(world,ik);
  AE_EXPECT_TRUE(gap(world,tip,far)<1e-2f,"three-bone member reaches after body pull and spine bend");
  // Interacao com braco de tres ossos e golpe de frente por conexao.
  SceneGraph h;const auto actor=h.createEntity(h.root(),ObjectKind::Folder,"Ator");
  const auto base=h.createEntity(actor,ObjectKind::Folder,"Ombro");const auto u1=h.createEntity(base,ObjectKind::Folder,"U1");
  const auto u2=h.createEntity(u1,ObjectKind::Folder,"U2");const auto u3=h.createEntity(u2,ObjectKind::Folder,"U3");const auto grip=h.createEntity(u3,ObjectKind::Folder,"Mao");
  Transform sb;sb.position[1]=1;h.setTransform(base,sb);Transform sg;sg.position[0]=.4f;h.setTransform(u1,sg);h.setTransform(u2,sg);h.setTransform(u3,sg);h.setTransform(grip,sg);
  addComponent<scene::InteractionSystem>(h,actor)->limbBones=3;
  const auto box=h.createEntity(h.root(),ObjectKind::Folder,"Caixa");Transform bx;bx.position[0]=.9f;bx.position[1]=1.6f;h.setTransform(box,bx);
  addComponent<scene::InteractionObject>(h,box)->reachTime=.1f;
  auto *hr=addComponent<scene::HitReaction>(h,u2);hr->axis=0;hr->chainLength=1;hr->reactToCollisions=false;
  GameWorld w2;w2.load(h);SceneIk ik2;step(w2,ik2);
  bool started=false;AE_EXPECT_TRUE(ik2.interactionStart(w2,actor,grip,box,started)==WorldStatus::Ok&&started,"three-bone arm accepted");
  step(w2,ik2,10);
  AE_EXPECT_TRUE(gap(w2,grip,box)<1e-2f,"three-bone hand reaches the object");
  AE_EXPECT_TRUE(ik2.interactionStop(w2,actor,grip)==WorldStatus::Ok,"release");step(w2,ik2,30);
  const float before=worldX(w2,u3);
  AE_EXPECT_TRUE(ik2.hitFront(w2,u2,1)==WorldStatus::Ok,"hit from the front accepted");
  step(w2,ik2,6);
  AE_EXPECT_TRUE(std::abs(worldX(w2,u3,1)-1)>.02f||std::abs(worldX(w2,u3)-before)>.02f,"front hit bends the chain");
}

AE_TEST(ik_versioned_components_read_previous_versions) {
  std::istringstream fb1("1 2 0 3 0 0 0 0 0 1 1 .6 .5");scene::FullBodyIk fb;
  AE_EXPECT_TRUE(fb.read(fb1,1)&&fb.limbBones==2&&fb.chest==0,"full body v1");
  std::istringstream hr1("1 1 1 25 .4 3");scene::HitReaction hr;AE_EXPECT_TRUE(hr.read(hr1,1)&&hr.axis==2,"hit reaction v1");
  std::istringstream is1("1 1");scene::InteractionSystem is;AE_EXPECT_TRUE(is.read(is1,1)&&is.limbBones==2,"interaction system v1");
  const scene::ComponentType *types[]{&scene::FabrikRoot::descriptor,&scene::BodyByTargets::descriptor};
  for(const auto *type:types) {
    const auto value=type->create();std::ostringstream out;value->write(out);
    const auto copy=type->create();std::istringstream in(out.str());
    AE_EXPECT_TRUE(copy->read(in,type->version)&&copy->valid(),"new component persists");
  }
}

AE_TEST(ik_interaction_carries_a_physics_body_as_kinematic_and_restores_it) {
  Leg leg;addComponent<scene::InteractionSystem>(leg.g,leg.owner);
  const auto crate=leg.g.createEntity(leg.g.root(),ObjectKind::Folder,"Caixa");
  Transform at;at.position[0]=.6f;at.position[1]=.6f;leg.g.setTransform(crate,at);
  auto *object=addComponent<scene::InteractionObject>(leg.g,crate);object->reachTime=.1f;object->releaseTime=.1f;object->pickUp=true;
  addComponent<scene::PhysicsBody>(leg.g,crate)->motion=scene::BodyMotion::Dynamic;
  GameWorld world;world.load(leg.g);world.setAuthority(crate,TransformAuthority::PhysicsBody);
  SceneIk ik;u32 moves=0;float last[7]{};
  ik.setBodyMover([&](ObjectId body,const float pose[7]){if(body!=crate)return false;++moves;std::copy(pose,pose+7,last);return true;});
  step(world,ik);bool started=false;
  AE_EXPECT_TRUE(ik.interactionStart(world,leg.owner,leg.foot,crate,started)==WorldStatus::Ok&&started,"start");
  step(world,ik,12);
  scene::ComponentPropertyValue motion;const auto body=world.findComponent(world.handle(crate),"astra.physics.body");
  AE_EXPECT_TRUE(world.getProperty(body,"motion",motion)==WorldStatus::Ok&&std::get<u32>(motion)==1,"held physics body becomes kinematic");
  float footNow[16];worldMatrix(world.graph(),leg.foot,footNow);
  AE_EXPECT_TRUE(moves>0&&std::abs(last[0]-footNow[12])<2e-2f&&std::abs(last[1]-footNow[13])<2e-2f,"body is moved with the hand");
  AE_EXPECT_TRUE(ik.interactionStop(world,leg.owner,leg.foot)==WorldStatus::Ok,"stop");step(world,ik);
  AE_EXPECT_TRUE(world.getProperty(body,"motion",motion)==WorldStatus::Ok&&std::get<u32>(motion)==2,"release restores dynamic motion");
}

AE_TEST(ik_interaction_carries_a_real_physics_body_through_scene_physics) {
  Leg leg;addComponent<scene::InteractionSystem>(leg.g,leg.owner);
  const auto crate=leg.g.createEntity(leg.g.root(),ObjectKind::Folder,"Caixa");
  Transform at;at.position[0]=.6f;at.position[1]=.6f;leg.g.setTransform(crate,at);
  auto *object=addComponent<scene::InteractionObject>(leg.g,crate);object->reachTime=.1f;object->releaseTime=.1f;object->pickUp=true;
  auto *body=addComponent<scene::PhysicsBody>(leg.g,crate);body->motion=scene::BodyMotion::Dynamic;body->gravityFactor=0;body->linearDamping=0;body->angularDamping=0;
  auto *box=addComponent<scene::Collider>(leg.g,crate);box->halfX=box->halfY=box->halfZ=.05f;
  GameWorld world;AE_EXPECT_TRUE(world.load(leg.g),"world");ScenePhysics physics;AE_EXPECT_TRUE(physics.start(world),physics.error().c_str());
  SceneIk ik;ik.setBodyMover([&](ObjectId id,const float pose[7]){return physics.moveKinematic(id,pose);});
  const auto frame=[&](u32 count) {
    for(u32 i=0;i<count;++i) {
      world.beginFrame(1/60.);
      if(world.consumeInvalidation()&(scene::Invalidate::PhysicsBody|scene::Invalidate::PhysicsShape))AE_EXPECT_TRUE(physics.rebuild(world,nullptr),physics.error().c_str());
      AE_EXPECT_TRUE(physics.advance(1/60.,world),"physics step");ik.advance(world);world.flush();
    }
  };
  frame(1);bool started=false;
  AE_EXPECT_TRUE(ik.interactionStart(world,leg.owner,leg.foot,crate,started)==WorldStatus::Ok&&started,"start");
  frame(30);
  float foot[16],held[16];worldMatrix(world.graph(),leg.foot,foot);worldMatrix(world.graph(),crate,held);
  AE_EXPECT_TRUE(distance(foot+12,held+12)<2e-2f,"the simulated body is carried by the hand");
  AE_EXPECT_TRUE(ik.diagnostics().empty(),"no carry diagnostic");
  AE_EXPECT_TRUE(ik.interactionStop(world,leg.owner,leg.foot)==WorldStatus::Ok,"release");frame(3);
  scene::ComponentPropertyValue motion;
  AE_EXPECT_TRUE(world.getProperty(world.findComponent(world.handle(crate),"astra.physics.body"),"motion",motion)==WorldStatus::Ok&&std::get<u32>(motion)==2,"dynamic again after release");
  physics.stop();
}

AE_TEST(ik_foot_lock_plants_the_foot_until_it_lifts_or_overstretches) {
  Leg leg;auto *f=addComponent<scene::FootGrounding>(leg.g,leg.foot);f->lockFoot=true;f->rayDistance=2;f->maxCorrection=.4f;f->alignToSurface=false;
  GameWorld world;world.load(leg.g);SceneIk ik;
  const SceneIk::GroundQuery flat=[](const float *o,const float *,ObjectId,float *p,float *n){p[0]=o[0];p[1]=0;p[2]=o[2];n[0]=0;n[1]=1;n[2]=0;return true;};
  const auto slide=[&](float x){Transform t;t.position[0]=x;world.setLocalTransform(world.handle(leg.owner),t);world.beginFrame(1/60.);ik.advance(world,flat);};
  slide(0);slide(.1f);slide(.2f);
  AE_EXPECT_TRUE(std::abs(worldX(world,leg.foot))<5e-3f,"planted foot does not slide with the body");
  slide(.5f);
  AE_EXPECT_TRUE(worldX(world,leg.foot)>.4f,"lock releases instead of overstretching the leg");
  Transform up;up.position[1]=-1;up.position[2]=.3f;world.setLocalTransform(world.handle(leg.knee),up);
  Transform lifted;lifted.position[1]=-.8f;world.setLocalTransform(world.handle(leg.foot),lifted);
  slide(.6f);
  AE_EXPECT_TRUE(std::abs(worldX(world,leg.foot)-.6f)<2e-2f,"lifted foot follows the animation");
  std::istringstream old("1 1 1 .5 1 0 .4");scene::FootGrounding legacy;
  AE_EXPECT_TRUE(legacy.read(old,1)&&!legacy.lockFoot,"version 1 keeps the foot unlocked");
}

AE_TEST(ik_socket_attaches_accessories_after_ik_and_reports_missing_bones) {
  Spine s;const auto hat=s.g.createEntity(s.g.root(),ObjectKind::Folder,"Chapeu");
  auto *socket=addComponent<scene::Socket>(s.g,hat);socket->bone=s.head;socket->position[1]=.2f;
  auto *aim=addComponent<scene::AimIk>(s.g,s.head);aim->target=s.target;aim->chainLength=2;aim->axis=1;
  GameWorld world;world.load(s.g);SceneIk ik;world.beginFrame(1/60.);ik.advance(world);
  float head[16],h[16];worldMatrix(world.graph(),s.head,head);worldMatrix(world.graph(),hat,h);
  const float expected[3]{head[12]+head[4]*.2f,head[13]+head[5]*.2f,head[14]+head[6]*.2f};
  AE_EXPECT_TRUE(distance(h+12,expected)<1e-3f,"hat sits on the IK-solved head");
  AE_EXPECT_TRUE(std::abs(head[5]-1)>1e-3f,"the head really moved by IK");
  world.destroyObject(world.handle(s.head));world.flush();world.beginFrame(1/60.);ik.advance(world);
  bool missing=false;for(const auto &d:ik.diagnostics())missing=missing||(d.object==hat&&d.issue==SceneIk::Issue::MissingTarget);
  AE_EXPECT_TRUE(missing,"missing bone is diagnosed instead of attaching elsewhere");
}
