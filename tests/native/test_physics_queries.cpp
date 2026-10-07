#include "harness.h"
#include "runtime/component_operations.h"
#include "runtime/scene_physics_queries.h"

#include <cmath>
#include <sstream>

using namespace ae;using namespace ae::runtime;

namespace {
struct QueryScene {
  SceneGraph graph;ObjectId floor=0,wall=0,ray=0,selfRay=0,layered=0,sweep=0,arm=0,camera=0;
  u64 rayInstance=0,selfInstance=0,layeredInstance=0,sweepInstance=0,armInstance=0;
  ObjectId staticBox(const char *name,float x,float y,float z,float hx,float hy,float hz,u32 layer=0) {
    const auto id=graph.createEntity(graph.root(),ObjectKind::Folder,name);auto v=*graph.find(id);
    v.transform.position[0]=x;v.transform.position[1]=y;v.transform.position[2]=z;v.layer=layer;
    static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor))->motion=scene::BodyMotion::Static;
    auto *c=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));c->halfX=hx;c->halfY=hy;c->halfZ=hz;
    graph.applyEntityValues(id,v);return id;
  }
  QueryScene() {
    floor=staticBox("Chão",0,-.5f,0,20,.5f,20);
    wall=staticBox("Parede",10,1,3,4,2,.25f,3);
    {ray=graph.createEntity(graph.root(),ObjectKind::Folder,"Raio");auto v=*graph.find(ray);v.transform.position[1]=2;
     auto *r=static_cast<scene::RayCast*>(v.components.add(scene::RayCast::descriptor));r->target[1]=-5;rayInstance=r->instanceId();graph.applyEntityValues(ray,v);}
    {selfRay=graph.createEntity(graph.root(),ObjectKind::Folder,"Corpo com raio");auto v=*graph.find(selfRay);v.transform.position[0]=4;v.transform.position[1]=1;
     static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor))->motion=scene::BodyMotion::Static;
     v.components.add(scene::Collider::descriptor);
     auto *r=static_cast<scene::RayCast*>(v.components.add(scene::RayCast::descriptor));r->target[1]=-5;selfInstance=r->instanceId();graph.applyEntityValues(selfRay,v);}
    {layered=graph.createEntity(graph.root(),ObjectKind::Folder,"Raio na camada 3");auto v=*graph.find(layered);v.transform.position[0]=10;v.transform.position[1]=1;
     auto *r=static_cast<scene::RayCast*>(v.components.add(scene::RayCast::descriptor));r->target[2]=10;r->filter.layer=4;layeredInstance=r->instanceId();graph.applyEntityValues(layered,v);}
    {sweep=graph.createEntity(graph.root(),ObjectKind::Folder,"Varredura");auto v=*graph.find(sweep);v.transform.position[0]=-4;v.transform.position[1]=3;
     auto *s=static_cast<scene::ShapeCast*>(v.components.add(scene::ShapeCast::descriptor));s->radius=.5f;s->target[1]=-5;sweepInstance=s->instanceId();graph.applyEntityValues(sweep,v);}
    {arm=graph.createEntity(graph.root(),ObjectKind::Folder,"Braço");auto v=*graph.find(arm);v.transform.position[0]=10;v.transform.position[1]=1;v.transform.position[2]=-2;
     auto *a=static_cast<scene::SpringArm*>(v.components.add(scene::SpringArm::descriptor));a->length=8;a->margin=.1f;armInstance=a->instanceId();graph.applyEntityValues(arm,v);
     camera=graph.createEntity(arm,ObjectKind::Folder,"Câmera");}
  }
};
bool near(float a,float b,float tolerance=.02f) {return std::abs(a-b)<tolerance;}
}

AE_TEST(physics_query_components_ray_shape_and_spring_arm_against_real_jolt) {
  scene::ShapeCast authored;authored.shape=scene::ShapeCastShape::Capsule;authored.target[2]=3;authored.filter.layer=2;
  std::stringstream payload;authored.write(payload);scene::ShapeCast back;
  AE_EXPECT_TRUE(back.read(payload,1)&&back.shape==scene::ShapeCastShape::Capsule&&back.target[2]==3&&back.filter.layer==2,"varredura v1 relida");
  scene::RayCast zero;zero.target[1]=0;AE_EXPECT_TRUE(!zero.valid(),"raio sem comprimento recusado");

  QueryScene scene;GameWorld w;ScenePhysics physics;ScenePhysicsQueries queries;
  AE_EXPECT_TRUE(w.load(scene.graph)&&physics.start(w),physics.error().c_str());
  AE_EXPECT_TRUE(queries.advance(w,physics),"consultas avaliadas depois da física");
  const auto *ray=queries.result(scene.ray,scene.rayInstance);
  AE_EXPECT_TRUE(ray&&ray->hit&&ray->collider==scene.floor&&near(ray->point[1],0)&&near(ray->distance,2)&&near(ray->normal[1],1),"raio acerta o chão a 2 m com normal +Y");
  const auto *self=queries.result(scene.selfRay,scene.selfInstance);
  AE_EXPECT_TRUE(self&&self->hit&&self->collider==scene.floor,"o próprio corpo é ignorado e o raio chega ao chão");
  const auto *layered=queries.result(scene.layered,scene.layeredInstance);
  AE_EXPECT_TRUE(layered&&layered->hit&&layered->collider==scene.wall,"filtro da camada 3 acerta a parede na camada 3");
  const auto *sweep=queries.result(scene.sweep,scene.sweepInstance);
  AE_EXPECT_TRUE(sweep&&sweep->hit&&sweep->collider==scene.floor&&near(sweep->distance,2.5f,.05f),"esfera de raio .5 encosta no chão depois de 2,5 m");
  Transform child;w.localTransform(w.handle(scene.camera),child);
  AE_EXPECT_TRUE(near(child.position[2],4.65f,.05f)&&child.position[0]==0&&child.position[1]==0,"braço põe o filho na parede menos a margem");

  // Porta da ABI: os mesmos resultados pelos métodos do descritor.
  ComponentOperationServices services{&w,nullptr,nullptr,nullptr,nullptr,nullptr,&queries,&physics};
  scene::ComponentOperationValue value;
  AE_EXPECT_TRUE(invokeComponentMethod(services,{w.handle(scene.ray),scene.rayInstance},"colliding",{},value)==WorldStatus::Ok&&value.boolean==1,"colliding");
  AE_EXPECT_TRUE(invokeComponentMethod(services,{w.handle(scene.ray),scene.rayInstance},"collider",{},value)==WorldStatus::Ok&&value.object==scene.floor,"collider");
  AE_EXPECT_TRUE(invokeComponentMethod(services,{w.handle(scene.arm),scene.armInstance},"hit_length",{},value)==WorldStatus::Ok&&near(static_cast<float>(value.number),4.65f,.05f),"hit_length");
  // Mover o raio para fora do chão e pedir "update" sem esperar o quadro.
  Transform moved;w.localTransform(w.handle(scene.ray),moved);moved.position[0]=50;w.setLocalTransform(w.handle(scene.ray),moved);
  AE_EXPECT_TRUE(invokeComponentMethod(services,{w.handle(scene.ray),scene.rayInstance},"update",{},value)==WorldStatus::Ok,"update");
  AE_EXPECT_TRUE(!queries.result(scene.ray,scene.rayInstance)->hit,"fora do chão não acerta");
  ComponentOperationServices missing{&w};
  AE_EXPECT_TRUE(invokeComponentMethod(missing,{w.handle(scene.ray),scene.rayInstance},"colliding",{},value)==WorldStatus::NotRunning,"sem avaliador recusa explicitamente");
}
