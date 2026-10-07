#include "harness.h"
#include "runtime/scene_physics.h"
#include "scene/character.h"
#include "scene/joint.h"
#include "scene/physics_body.h"

#include <sstream>
#include <vector>

using namespace ae;using namespace ae::runtime;

namespace {
struct Hanging {
  SceneGraph graph;ObjectId anchor=0,weight=0;u64 joint=0;
  // Caixa de 10 kg presa por junta fixa a uma âncora estática: a restrição
  // segura cerca de 98 N contra a gravidade.
  explicit Hanging(float breakForce) {
    const auto body=[&](const char *name,float y,scene::BodyMotion motion){
      const auto id=graph.createEntity(graph.root(),ObjectKind::Folder,name);auto v=*graph.find(id);v.transform.position[1]=y;
      auto *b=static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor));b->motion=motion;b->mass=10;
      auto *c=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));c->halfX=c->halfY=c->halfZ=.25f;
      graph.applyEntityValues(id,v);return id;};
    anchor=body("Âncora",5,scene::BodyMotion::Static);weight=body("Peso",4,scene::BodyMotion::Dynamic);
    auto v=*graph.find(weight);auto *j=static_cast<scene::Joint*>(v.components.add(scene::Joint::descriptor));
    j->kind=scene::JointKind::Fixed;j->connectedBody=anchor;j->breakForce=breakForce;joint=j->instanceId();
    graph.applyEntityValues(weight,v);
  }
};
struct Broken {ObjectId owner;u64 instance;float force;};
bool record(void *context,ObjectId owner,u64 instance,float force) {static_cast<std::vector<Broken>*>(context)->push_back({owner,instance,force});return true;}
float height(GameWorld &w,ObjectId id) {Transform t;w.localTransform(w.handle(id),t);return t.position[1];}
}

AE_TEST(joint_breaks_when_constraint_force_passes_the_authored_limit_and_stays_broken) {
  scene::Joint authored;authored.breakForce=50;authored.breakTorque=7;std::stringstream v3;authored.write(v3);scene::Joint back;
  AE_EXPECT_TRUE(back.read(v3,3)&&back.breakForce==50&&back.breakTorque==7,"junta v3 relê os limites de quebra");
  std::string text=v3.str();std::vector<std::string> tokens;{std::istringstream in(text);std::string w;while(in>>w)tokens.push_back(w);}
  // v2: os dois números de quebra (últimos da tabela de números) não existiam.
  const usize numbers=scene::Joint::descriptor.numbers.size();std::string v2;
  for(usize i=0;i<tokens.size();++i) if(i<4+numbers-2||i>=4+numbers) v2+=tokens[i]+' ';
  std::istringstream old(v2);scene::Joint previous;previous.breakForce=9;
  AE_EXPECT_TRUE(previous.read(old,2)&&previous.breakForce==0&&previous.breakTorque==0,"junta v2 lê como inquebrável");

  Hanging fragile(50);GameWorld w;ScenePhysics physics;std::vector<Broken> broken;
  AE_EXPECT_TRUE(w.load(fragile.graph)&&physics.start(w),physics.error().c_str());
  for(int i=0;i<30;++i) AE_EXPECT_TRUE(physics.advance(1.0/60,w,nullptr,&broken,nullptr,nullptr,record),"passo físico");
  AE_EXPECT_EQ(broken.size(),usize{1},"a junta quebra uma vez");
  AE_EXPECT_TRUE(!broken.empty()&&broken[0].owner==fragile.weight&&broken[0].instance==fragile.joint&&broken[0].force>50,"evento leva dono, instância e força acima do limite");
  AE_EXPECT_TRUE(height(w,fragile.weight)<3.5f,"sem a junta, o peso cai");
  AE_EXPECT_TRUE(physics.jointBroken(fragile.weight,fragile.joint),"registrada como quebrada");
  AE_EXPECT_TRUE(physics.rebuild(w,nullptr)&&physics.jointCount()==0,"reconstrução no mesmo Play não recria a junta");

  Hanging strong(1000);GameWorld w2;ScenePhysics held;std::vector<Broken> none;
  AE_EXPECT_TRUE(w2.load(strong.graph)&&held.start(w2),held.error().c_str());
  for(int i=0;i<30;++i) held.advance(1.0/60,w2,nullptr,&none,nullptr,nullptr,record);
  AE_EXPECT_TRUE(none.empty()&&height(w2,strong.weight)>3.9f,"abaixo do limite a junta segura o peso");
}

namespace {
bool recordHit(void *context,const CharacterHit &hit) {static_cast<std::vector<CharacterHit>*>(context)->push_back(hit);return true;}
}

AE_TEST(character_movement_reports_controller_collider_hit_with_object_point_and_normal) {
  SceneGraph g;
  const auto box=[&](const char *name,float x,float y,float hx,float hy){
    const auto id=g.createEntity(g.root(),ObjectKind::Folder,name);auto v=*g.find(id);v.transform.position[0]=x;v.transform.position[1]=y;
    static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor))->motion=scene::BodyMotion::Static;
    auto *c=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));c->halfX=hx;c->halfY=hy;c->halfZ=4;
    g.applyEntityValues(id,v);return id;};
  box("Chão",0,-.5f,8,.5f);const auto wall=box("Parede",2,1,.25f,2);
  const auto actor=g.createEntity(g.root(),ObjectKind::Folder,"Personagem");auto v=*g.find(actor);v.transform.position[1]=1;
  auto *character=static_cast<scene::Character*>(v.components.add(scene::Character::descriptor));character->speed=3;const auto instance=character->instanceId();
  g.applyEntityValues(actor,v);
  GameWorld w;ScenePhysics physics;std::vector<CharacterHit> hits;
  AE_EXPECT_TRUE(w.load(g)&&physics.start(w),physics.error().c_str());
  AE_EXPECT_TRUE(physics.setCharacterMove(actor,1,0,0),"anda para +X");
  for(int i=0;i<120;++i) physics.advance(1.0/60,w,nullptr,&hits,nullptr,nullptr,nullptr,recordHit);
  bool wallHit=false;
  for(const auto &hit:hits) if(hit.other==wall) {
    AE_EXPECT_TRUE(hit.character==actor&&hit.instance==instance,"evento identifica o personagem e a instância");
    wallHit|=hit.normal[0]<-.5f&&hit.point[0]>1.5f&&hit.point[0]<2.f;
  }
  AE_EXPECT_TRUE(wallHit,"bateu na parede com normal apontando de volta e ponto na face");
  usize perStep=0;for(const auto &hit:hits) perStep+=hit.other==wall;
  AE_EXPECT_TRUE(perStep<=120,"no máximo um evento por objeto em cada passo");
}
