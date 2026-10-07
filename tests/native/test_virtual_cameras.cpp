#include "harness.h"
#include "editor/editor_scene_camera.h"
#include "runtime/component_operations.h"
#include "runtime/scene_event_connections.h"
#include "runtime/scene_physics.h"
#include "runtime/scene_virtual_cameras.h"
#include "scene/camera.h"
#include "scene/camera_follow.h"
#include "scene/component_schema.h"
#include "scene/event_connection.h"
#include "scene/collider.h"
#include "scene/physics_body.h"
#include "scene/virtual_camera.h"

#include <cmath>
#include <functional>
#include <tuple>
#include <sstream>
#include <string>
#include <vector>

using namespace ae;using namespace ae::runtime;

namespace {
struct Rig {
  SceneGraph g;ObjectId camera=0,target=0;u64 brainInstance=0;
  ObjectId object(const char *name,float x,float y,float z) {
    const auto id=g.createEntity(g.root(),ObjectKind::Folder,name);auto v=*g.find(id);
    v.transform.position[0]=x;v.transform.position[1]=y;v.transform.position[2]=z;g.applyEntityValues(id,v);return id;
  }
  void brain(scene::CameraBlendStyle style,float seconds) {
    camera=object("Câmera",0,0,0);auto v=*g.find(camera);
    v.components.add(scene::Camera::descriptor);
    auto *b=static_cast<scene::CameraBrain*>(v.components.add(scene::CameraBrain::descriptor));
    b->defaultBlend=style;b->defaultBlendTime=seconds;brainInstance=b->instanceId();g.applyEntityValues(camera,v);
  }
  ObjectId vcam(const char *name,float priority,float x,float y,float z,const std::function<void(scene::VirtualCamera&)> &setup={}) {
    const auto id=object(name,x,y,z);auto v=*g.find(id);
    auto *c=static_cast<scene::VirtualCamera*>(v.components.add(scene::VirtualCamera::descriptor));c->priority=priority;
    if(setup) {setup(*c);}
    g.applyEntityValues(id,v);return id;
  }
};
bool frame(GameWorld &w,SceneVirtualCameras &cameras,double dt,const ScenePhysics *physics=nullptr,float lookX=0,float lookY=0) {
  const float look[2]{lookX,lookY};return w.beginFrame(dt)&&cameras.advance(w,physics,look);
}
Transform pose(const GameWorld &w,ObjectId id) {Transform t;w.worldTransform(w.handle(id),t);return t;}
float forwardY(const GameWorld &w,ObjectId id) {float m[16];worldMatrix(w.graph(),id,m);return m[9]/std::sqrt(m[8]*m[8]+m[9]*m[9]+m[10]*m[10]);}
bool near(float a,float b,float e=1e-3f) {return std::abs(a-b)<=e;}
std::vector<std::string> drain(ComponentEventQueue &q) {
  std::vector<std::string> out;
  q.consume(ComponentEventQueue::Consumer::Scripts,[&](const ComponentEventRecord &r,u64){out.push_back(std::string(r.type->events[r.event].id));});
  return out;
}
}

AE_TEST(virtual_camera_contract_round_trip_and_blend_curves) {
  scene::VirtualCamera c;c.trackingTarget=7;c.position=scene::VirtualCameraPosition::Orbit;c.rotation=scene::VirtualCameraRotation::LookAt;
  c.orbitYaw=33;c.priority=12;c.avoidObstacles=true;c.collisionLayer=3;c.blendStyle=scene::CameraBlendStyle::HardOut;c.noiseAmplitude=2;
  std::stringstream text;c.write(text);scene::VirtualCamera back;
  AE_EXPECT_TRUE(back.read(text,1)&&back.trackingTarget==7&&back.position==scene::VirtualCameraPosition::Orbit&&back.orbitYaw==33&&
                 back.priority==12&&back.collisionLayer==3&&back.blendStyle==scene::CameraBlendStyle::HardOut&&back.noiseAmplitude==2,"câmera virtual relida");
  back.priority=1.5f;AE_EXPECT_TRUE(!back.valid(),"prioridade fracionária recusada");
  back.priority=1;back.orbitPitchMin=50;back.orbitPitchMax=10;AE_EXPECT_TRUE(!back.valid(),"limites verticais invertidos recusados");
  scene::CameraBrain b;b.defaultBlend=scene::CameraBlendStyle::Linear;b.defaultBlendTime=3;std::stringstream bt;b.write(bt);scene::CameraBrain bb;
  AE_EXPECT_TRUE(bb.read(bt,1)&&bb.defaultBlend==scene::CameraBlendStyle::Linear&&bb.defaultBlendTime==3,"Cérebro relido");
  bb.defaultBlend=scene::CameraBlendStyle::BrainDefault;AE_EXPECT_TRUE(!bb.valid(),"o Cérebro não aceita 'padrão do Cérebro'");
  for(u32 s=1;s<=7;++s) {
    const auto style=static_cast<scene::CameraBlendStyle>(s);
    AE_EXPECT_TRUE(near(scene::cameraBlendWeight(style,1),1)&&(s==1||near(scene::cameraBlendWeight(style,0),0)),"curvas vão de 0 a 1");
  }
  AE_EXPECT_TRUE(scene::cameraBlendWeight(scene::CameraBlendStyle::EaseIn,.25f)<.25f&&scene::cameraBlendWeight(scene::CameraBlendStyle::EaseOut,.25f)>.25f,"entrada suave começa devagar; saída suave começa rápido");
  float low=0,high=0;for(u32 i=0;i<400;++i) {const float n=scene::cameraNoise(i*.137f);low=std::min(low,n);high=std::max(high,n);}
  AE_EXPECT_TRUE(low<-.2f&&high>.2f&&low>=-1.5f&&high<=1.5f,"ruído oscila nos dois sentidos e é limitado");
  AE_EXPECT_TRUE(near(scene::cameraNoise(3.f),scene::cameraNoise(3.f))&&std::abs(scene::cameraNoise(3.001f)-scene::cameraNoise(3.f))<.02f,"ruído determinístico e contínuo");
}

AE_TEST(camera_brain_cuts_to_the_highest_priority_then_blends_on_change) {
  Rig r;r.brain(scene::CameraBlendStyle::Linear,1);
  const auto low=r.vcam("Baixa",0,10,0,0),high=r.vcam("Alta",5,0,0,-10,[](scene::VirtualCamera &c){c.verticalFov=40;});
  GameWorld w;AE_EXPECT_TRUE(w.load(r.g),"mundo");
  ComponentEventQueue q;q.attach(ComponentEventQueue::Consumer::Scripts,true);SceneVirtualCameras cams;cams.setEvents(&q);
  AE_EXPECT_TRUE(frame(w,cams,1.0/60),"primeiro quadro");
  AE_EXPECT_TRUE(near(pose(w,r.camera).position[2],-10)&&cams.brain(r.camera)->live==high,"a maior prioridade entra em corte no primeiro quadro");
  const auto *lens=static_cast<const scene::Camera*>(w.graph().find(r.camera)->components.find(scene::Camera::descriptor));
  AE_EXPECT_TRUE(lens->verticalFov==40,"a lente da câmera virtual chega à Câmera");
  AE_EXPECT_TRUE(editor::resolveSceneCamera(w.graph()).verticalFov==40&&near(editor::resolveSceneCamera(w.graph()).position[2],-10),"o resolvedor de render vê pose e lente do Cérebro");
  auto events=drain(q);
  AE_EXPECT_TRUE(events.size()==3&&events[0]=="camera_activated"&&events[1]=="camera_cut"&&events[2]=="activated","ativação, corte e evento da câmera virtual");
  // A outra passa a ter prioridade maior: transição linear de 1 s.
  const auto lowHandle=w.handle(low);
  AE_EXPECT_TRUE(w.setProperty(w.findComponent(lowHandle,"astra.camera.virtual"),"priority",scene::ComponentPropertyValue{9.f})==WorldStatus::Ok,"prioridade em Play");
  AE_EXPECT_TRUE(frame(w,cams,.25)&&frame(w,cams,.25),"meio da transição (o relógio limita cada quadro a 0,25 s)");
  const auto mid=pose(w,r.camera);
  AE_EXPECT_TRUE(cams.brain(r.camera)->blending&&near(mid.position[0],5,.01f)&&near(mid.position[2],-5,.01f),"linear: metade do caminho em 0,5 s");
  AE_EXPECT_TRUE(near(lens->verticalFov,50,.01f),"lente misturada");
  events=drain(q);
  AE_EXPECT_TRUE(events.size()==3&&events[0]=="camera_activated"&&events[1]=="activated"&&events[2]=="deactivated","troca com transição não é corte");
  AE_EXPECT_TRUE(frame(w,cams,.25)&&frame(w,cams,.25),"fim da transição");
  AE_EXPECT_TRUE(!cams.brain(r.camera)->blending&&near(pose(w,r.camera).position[0],10)&&lens->verticalFov==60,"fim: só a câmera nova");
  events=drain(q);AE_EXPECT_TRUE(events.size()==1&&events[0]=="blend_finished","transição concluída");
  // Desligar a ao vivo devolve a anterior; a câmera virtual pode pedir corte.
  const auto highHandle=w.handle(high);
  w.setProperty(w.findComponent(highHandle,"astra.camera.virtual"),"blend_style",scene::ComponentPropertyValue{u32{1}});
  w.setActive(lowHandle,false);
  AE_EXPECT_TRUE(frame(w,cams,1.0/60)&&cams.brain(r.camera)->live==high&&!cams.brain(r.camera)->blending&&near(pose(w,r.camera).position[2],-10),"objeto inativo sai da disputa; entrada em corte da câmera virtual");
  // O documento autoral nunca é tocado.
  AE_EXPECT_TRUE(r.g.find(r.camera)->transform.position[2]==0,"autoria preservada");
}

AE_TEST(camera_brain_interrupted_blend_starts_from_what_is_on_screen) {
  Rig r;r.brain(scene::CameraBlendStyle::Linear,1);
  const auto a=r.vcam("A",3,0,0,0),b=r.vcam("B",1,10,0,0),c=r.vcam("C",0,0,0,10);
  GameWorld w;w.load(r.g);SceneVirtualCameras cams;
  frame(w,cams,1.0/60);
  w.setProperty(w.findComponent(w.handle(b),"astra.camera.virtual"),"priority",scene::ComponentPropertyValue{5.f});
  frame(w,cams,.25);frame(w,cams,.25);AE_EXPECT_TRUE(near(pose(w,r.camera).position[0],5,.01f),"A→B na metade");
  w.setProperty(w.findComponent(w.handle(c),"astra.camera.virtual"),"priority",scene::ComponentPropertyValue{8.f});
  frame(w,cams,0);const auto start=pose(w,r.camera);
  AE_EXPECT_TRUE(near(start.position[0],5,.01f)&&near(start.position[2],0,.01f),"a nova transição parte do quadro mostrado, sem salto");
  frame(w,cams,.25);frame(w,cams,.25);const auto half=pose(w,r.camera);
  AE_EXPECT_TRUE(near(half.position[0],2.5f,.01f)&&near(half.position[2],5,.01f),"e segue até C");
  (void)a;
}

AE_TEST(camera_brain_blend_keeps_the_horizon_level) {
  Rig r;r.brain(scene::CameraBlendStyle::Linear,1);
  const auto down=r.vcam("Para baixo",2,0,10,0),side=r.vcam("De lado",1,6,1,0);
  for(const auto &[id,pitch,yaw]:{std::tuple{down,60.f,0.f},std::tuple{side,0.f,150.f}}) {
    auto v=*r.g.find(id);v.transform.rotationDegrees[0]=pitch;v.transform.rotationDegrees[1]=yaw;r.g.applyEntityValues(id,v);
  }
  GameWorld w;w.load(r.g);SceneVirtualCameras cams;frame(w,cams,1.0/60);
  w.setProperty(w.findComponent(w.handle(side),"astra.camera.virtual"),"priority",scene::ComponentPropertyValue{5.f});
  for(int i=0;i<2;++i) frame(w,cams,.25);
  float m[16];worldMatrix(w.graph(),r.camera,m);
  const float right=m[1]/std::sqrt(m[0]*m[0]+m[1]*m[1]+m[2]*m[2]);
  AE_EXPECT_TRUE(cams.brain(r.camera)->blending&&std::abs(right)<1e-3f,"no meio da transição o eixo lateral continua horizontal (sem rolagem)");
  float q[4];transformRotationQuaternion(pose(w,r.camera),q);const float z[3]{0,0,1};float f[3];SceneVirtualCameras::rotate(q,z,f);
  AE_EXPECT_TRUE(f[1]<-.1f&&f[1]>-.87f,"a inclinação fica entre as duas câmeras");
}

AE_TEST(virtual_camera_priority_tie_and_prioritize_method) {
  Rig r;r.brain(scene::CameraBlendStyle::Cut,0);
  const auto first=r.vcam("Primeira",2,1,0,0),second=r.vcam("Segunda",2,2,0,0);
  GameWorld w;w.load(r.g);SceneVirtualCameras cams;frame(w,cams,1.0/60);
  AE_EXPECT_TRUE(cams.brain(r.camera)->live==first,"empate na largada: menor ID");
  ComponentOperationServices services;services.world=&w;services.cameras=&cams;
  scene::ComponentOperationValue result;
  const ComponentHandle secondCamera=w.findComponent(w.handle(second),"astra.camera.virtual");
  AE_EXPECT_TRUE(invokeComponentMethod(services,secondCamera,"prioritize",{},result)==WorldStatus::Ok,"Priorizar");
  frame(w,cams,1.0/60);
  AE_EXPECT_TRUE(cams.brain(r.camera)->live==second&&near(pose(w,r.camera).position[0],2),"no empate vence a priorizada");
  AE_EXPECT_TRUE(invokeComponentMethod(services,secondCamera,"is_live",{},result)==WorldStatus::Ok&&result.boolean==1,"Ao vivo");
  const ComponentHandle brain{w.handle(r.camera),r.brainInstance};
  AE_EXPECT_TRUE(invokeComponentMethod(services,brain,"live_camera",{},result)==WorldStatus::Ok&&result.object==second,"Câmera ao vivo");
  AE_EXPECT_TRUE(invokeComponentMethod(services,brain,"blending",{},result)==WorldStatus::Ok&&result.boolean==0,"sem transição em corte");
  // Reativar uma câmera conta como ativação recente.
  w.setActive(w.handle(first),false);frame(w,cams,1.0/60);w.setActive(w.handle(first),true);frame(w,cams,1.0/60);
  AE_EXPECT_TRUE(cams.brain(r.camera)->live==first,"reativada vence o empate");
  ComponentOperationServices none;none.world=&w;
  AE_EXPECT_TRUE(invokeComponentMethod(none,secondCamera,"snap",{},result)==WorldStatus::NotRunning,"sem serviço de câmeras: recusa explícita");
}

AE_TEST(virtual_camera_follow_look_at_damping_and_target_yaw) {
  Rig r;r.brain(scene::CameraBlendStyle::Cut,0);
  r.target=r.object("Alvo",0,0,0);
  const auto target=r.target;
  const auto cam=r.vcam("Seguidora",1,50,50,50,[&](scene::VirtualCamera &c){
    c.trackingTarget=target;c.position=scene::VirtualCameraPosition::Follow;c.rotation=scene::VirtualCameraRotation::LookAt;
    c.followOffset[0]=0;c.followOffset[1]=3;c.followOffset[2]=-4;c.positionDamping=.5f;c.aimOffset[1]=1;});
  GameWorld w;w.load(r.g);SceneVirtualCameras cams;
  frame(w,cams,1.0/60);
  auto p=pose(w,cam);
  AE_EXPECT_TRUE(near(p.position[1],3)&&near(p.position[2],-4),"primeiro quadro encaixa sem amortecimento");
  // Olha do (0,3,-4) para (0,1,0): para baixo e para frente.
  AE_EXPECT_TRUE(near(forwardY(w,cam),-2/std::sqrt(20.f),1e-3f),"mira no alvo com ajuste");
  Transform moved=pose(w,target);moved.position[0]=10;w.setWorldTransform(w.handle(target),moved);
  frame(w,cams,.25);frame(w,cams,.25);p=pose(w,cam);
  const float expected=10*(1-std::exp(-1.f));
  AE_EXPECT_TRUE(near(p.position[0],expected,.01f),"amortecimento exponencial de 0,5 s");
  // Snap descarta o amortecimento.
  ComponentOperationServices services;services.world=&w;services.cameras=&cams;scene::ComponentOperationValue none;
  invokeComponentMethod(services,w.findComponent(w.handle(cam),"astra.camera.virtual"),"snap",{},none);
  frame(w,cams,.01);AE_EXPECT_TRUE(near(pose(w,cam).position[0],10),"Encaixar vai direto ao alvo");
  // Guinada do alvo: o alvo vira 90° e o deslocamento gira junto.
  w.setProperty(w.findComponent(w.handle(cam),"astra.camera.virtual"),"binding",scene::ComponentPropertyValue{u32{1}});
  w.setProperty(w.findComponent(w.handle(cam),"astra.camera.virtual"),"position_damping",scene::ComponentPropertyValue{0.f});
  moved=pose(w,target);moved.rotationDegrees[1]=90;w.setWorldTransform(w.handle(target),moved);
  frame(w,cams,1.0/60);p=pose(w,cam);
  AE_EXPECT_TRUE(near(p.position[0],10-4,.01f)&&near(p.position[2],0,.01f),"deslocamento (0,3,−4) girado 90° fica em −X do alvo");
}

AE_TEST(virtual_camera_orbit_uses_look_input_only_while_live) {
  Rig r;r.brain(scene::CameraBlendStyle::Cut,0);
  r.target=r.object("Alvo",0,1,0);const auto target=r.target;
  const auto orbit=r.vcam("Órbita",3,0,0,0,[&](scene::VirtualCamera &c){
    c.trackingTarget=target;c.position=scene::VirtualCameraPosition::Orbit;c.rotation=scene::VirtualCameraRotation::LookAt;
    c.orbitRadius=4;c.orbitYaw=90;c.orbitPitch=0;c.positionDamping=0;c.orbitYawSensitivity=100;c.orbitPitchSensitivity=100;});
  const auto standby=r.vcam("Espera",1,0,0,0,[&](scene::VirtualCamera &c){
    c.trackingTarget=target;c.position=scene::VirtualCameraPosition::Orbit;c.orbitRadius=4;c.positionDamping=0;});
  GameWorld w;w.load(r.g);SceneVirtualCameras cams;
  frame(w,cams,1.0/60);
  auto p=pose(w,orbit);
  AE_EXPECT_TRUE(near(p.position[0],-4)&&near(p.position[1],1)&&near(p.position[2],0),"ângulo horizontal 90°: a câmera fica em −X, olhando para +X");
  frame(w,cams,1.0/60,nullptr,.3f,.2f);
  const auto *c=static_cast<const scene::VirtualCamera*>(w.graph().find(orbit)->components.find(scene::VirtualCamera::descriptor));
  AE_EXPECT_TRUE(near(c->orbitYaw,120)&&near(c->orbitPitch,20),"a entrada de olhar gira a órbita e o valor fica no componente");
  p=pose(w,orbit);
  AE_EXPECT_TRUE(near(p.position[1],1+4*std::sin(20*.0174532925f),.01f),"ângulo vertical eleva a câmera");
  const auto *s=static_cast<const scene::VirtualCamera*>(w.graph().find(standby)->components.find(scene::VirtualCamera::descriptor));
  AE_EXPECT_TRUE(s->orbitYaw==0,"a câmera em espera não consome a entrada");
  frame(w,cams,1.0/60,nullptr,0,10);
  AE_EXPECT_TRUE(near(c->orbitPitch,70),"limite vertical máximo respeitado");
}

AE_TEST(virtual_camera_deoccluder_pulls_in_front_of_a_real_jolt_wall) {
  Rig r;r.brain(scene::CameraBlendStyle::Cut,0);
  r.target=r.object("Personagem",0,0,0);const auto target=r.target;
  {auto v=*r.g.find(target);auto *b=static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor));b->motion=scene::BodyMotion::Kinematic;
   v.components.add(scene::Collider::descriptor);r.g.applyEntityValues(target,v);}
  const auto wall=r.object("Parede",0,0,-3);
  {auto v=*r.g.find(wall);v.components.add(scene::PhysicsBody::descriptor);auto *col=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));
   col->halfX=3;col->halfY=3;col->halfZ=.2f;r.g.applyEntityValues(wall,v);}
  const auto cam=r.vcam("Terceira pessoa",1,0,0,0,[&](scene::VirtualCamera &c){
    c.trackingTarget=target;c.position=scene::VirtualCameraPosition::Follow;c.rotation=scene::VirtualCameraRotation::LookAt;
    c.followOffset[0]=0;c.followOffset[1]=0;c.followOffset[2]=-6;c.positionDamping=0;c.avoidObstacles=true;c.cameraRadius=.2f;c.minimumDistance=.5f;c.collisionDamping=.5f;});
  GameWorld w;AE_EXPECT_TRUE(w.load(r.g),"mundo");ScenePhysics physics;AE_EXPECT_TRUE(physics.start(w),physics.error().c_str());
  physics.advance(1.0/60,w);
  SceneVirtualCameras cams;frame(w,cams,1.0/60,&physics);
  const auto shown=pose(w,r.camera).position[2];
  AE_EXPECT_TRUE(shown>-2.8f&&shown<-2.3f,"a câmera para à frente da parede (face em z=−2,8, raio 0,2)");
  AE_EXPECT_TRUE(near(pose(w,cam).position[2],-6),"o objeto da câmera virtual guarda a pose crua: a correção não realimenta");
  // Parede removida: volta à distância livre com amortecimento, sem salto.
  w.setActive(w.handle(wall),false);physics.releaseObject(wall);physics.advance(1.0/60,w);
  frame(w,cams,.25,&physics);const auto back=pose(w,r.camera).position[2];
  AE_EXPECT_TRUE(back<shown-.1f&&back>-6+.1f,"liberada, recua aos poucos");
  for(int i=0;i<60;++i) {frame(w,cams,.1,&physics);}
  AE_EXPECT_TRUE(near(pose(w,r.camera).position[2],-6,.01f),"e chega à distância autorada");
}

AE_TEST(virtual_camera_noise_dutch_and_brain_ignore_time_scale) {
  Rig r;r.brain(scene::CameraBlendStyle::Cut,0);
  const auto cam=r.vcam("Tremida",1,0,2,0,[](scene::VirtualCamera &c){c.noiseAmplitude=3;c.noiseFrequency=2;c.dutch=15;});
  GameWorld w;w.load(r.g);SceneVirtualCameras cams;
  frame(w,cams,1.0/60);float q0[4];transformRotationQuaternion(pose(w,r.camera),q0);
  for(int i=0;i<20;++i) frame(w,cams,1.0/60);
  float q1[4];transformRotationQuaternion(pose(w,r.camera),q1);
  const float dot=std::abs(q0[0]*q1[0]+q0[1]*q1[1]+q0[2]*q1[2]+q0[3]*q1[3]);
  AE_EXPECT_TRUE(dot<.99999f&&dot>.99f,"o tremor move a câmera mostrada dentro da amplitude");
  const auto raw=pose(w,cam);
  AE_EXPECT_TRUE(raw.rotationDegrees[0]==0&&raw.rotationDegrees[1]==0&&raw.rotationDegrees[2]==0,"o objeto da câmera virtual não acumula tremor nem inclinação");
  // Escala de tempo zero congela o tremor; o Cérebro pode ignorar a escala.
  w.setTimeScale(0);
  frame(w,cams,1.0/60);float frozen[4];transformRotationQuaternion(pose(w,r.camera),frozen);
  frame(w,cams,1.0/60);float still[4];transformRotationQuaternion(pose(w,r.camera),still);
  AE_EXPECT_TRUE(frozen[0]==still[0]&&frozen[1]==still[1]&&frozen[3]==still[3],"tempo de jogo parado: tremor parado");
  w.setProperty({w.handle(r.camera),r.brainInstance},"ignore_time_scale",scene::ComponentPropertyValue{true});
  frame(w,cams,1.0/60);float moving[4];transformRotationQuaternion(pose(w,r.camera),moving);
  AE_EXPECT_TRUE(moving[0]!=still[0]||moving[1]!=still[1],"ignorando a escala, a câmera segue o tempo real");
}

AE_TEST(camera_brain_schema_rules_and_event_connection_identity) {
  scene::Components components;
  AE_EXPECT_TRUE(!scene::planComponentAddition(components,"astra.camera.brain").ready||
                 scene::planComponentAddition(components,"astra.camera.brain").candidate.find(scene::Camera::descriptor),"o Cérebro traz ou exige a Câmera");
  auto plan=scene::planComponentAddition(components,"astra.camera");components=std::move(plan.candidate);
  components.add(scene::CameraFollow::descriptor);
  AE_EXPECT_TRUE(!scene::planComponentAddition(components,"astra.camera.brain").ready,"Acompanhar alvo e Cérebro não convivem");
  scene::Components vcam;vcam.add(scene::VirtualCamera::descriptor);
  AE_EXPECT_TRUE(!scene::planComponentAddition(vcam,"astra.physics.body").ready,"câmera virtual sem corpo físico");
  AE_EXPECT_TRUE(runtime::auditEventConnectionCatalog().empty(),"eventos e métodos novos têm identidade de conexão");
  AE_EXPECT_TRUE(scene::findEventConnectionEvent("astra.camera.brain","camera_activated")->value==17&&scene::eventConnectionCarriesObject(17),"evento do Cérebro filtra pela câmera que entrou");
}
