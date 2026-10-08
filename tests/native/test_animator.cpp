#include "harness.h"
#include "runtime/component_operations.h"
#include "runtime/scene_animator_graph.h"
#include "runtime/scene_animation.h"
#include "runtime/scene_physics.h"
#include "scene/physics_body.h"
#include "scene/collider.h"
#include "scene/character.h"
#include "scene/dynamic_body_motor.h"
#include "runtime/prefab.h"
#include "scene/animation.h"
#include "scene/animator.h"
#include "scene/component_schema.h"

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

using namespace ae;using namespace ae::runtime;namespace blend=ae::runtime::animator_detail;

namespace {
// Clipes de 1 s que põem um nó numa posição X constante: a pose final mede os pesos.
struct Library final : AnimationLibrary {
  SourceAnimations source;
  Library() {
    source.source=resources::assetGuidFromSeed("fonte");
    source.nodes={resources::assetGuidFromSeed("no-quadril"),resources::assetGuidFromSeed("no-braco")};
    source.nodeNames={"Quadril","Braço"};
  }
  resources::AssetGuid add(const char *name,u32 node,float x,float duration=1) {
    resources::AnimationClip clip;clip.name=name;clip.duration=duration;
    resources::AnimationChannel c;c.node=node;c.path=resources::AnimationPath::Translation;c.times={0,duration};c.values={x,0,0,x,0,0};
    clip.channels.push_back(c);source.clips.push_back(clip);
    const auto id=resources::assetGuidFromSeed(std::string("clipe-")+name);source.clipIds.push_back(id);return id;
  }
  bool findClip(const resources::AssetGuid &clip,AnimationClipView &out) const override {
    for(usize i=0;i<source.clipIds.size();++i) if(source.clipIds[i]==clip) {out.clip=&source.clips[i];out.source=&source;out.name=source.clips[i].name;return true;}
    return false;
  }
};
struct Rig {
  Library library;SceneGraph g;ObjectId owner=0,hips=0,spine=0,arm=0;
  resources::AssetGuid idle,walk,run,jump,wave;
  Rig() {
    idle=library.add("Parado",0,0);walk=library.add("Andar",0,1);run=library.add("Correr",0,2);jump=library.add("Pular",0,3,.5f);wave=library.add("Acenar",1,10);
    owner=g.createEntity(g.root(),ObjectKind::Folder,"Personagem");
    hips=g.createEntity(owner,ObjectKind::Folder,"Quadril");spine=g.createEntity(hips,ObjectKind::Folder,"Coluna");
    arm=g.createEntity(spine,ObjectKind::Folder,"Braço");
  }
  scene::Animator &animator() {
    auto *c=g.editComponents(owner);if(auto *a=c->edit(scene::Animator::descriptor)) return scene::animator(*a);
    return scene::animator(*c->add(scene::Animator::descriptor));
  }
};
scene::AnimatorParameter &param(scene::Animator &a,const char *name,scene::AnimatorParameterType type,float value=0) {
  a.parameters.push_back({a.allocateId(),name,type,value});return a.parameters.back();
}
scene::AnimatorState &state(scene::Animator &a,const char *name,std::vector<resources::AssetGuid> clips,u32 layer=0) {
  scene::AnimatorState s;s.id=a.allocateId();s.name=name;for(auto c:clips) s.motions.push_back({c});
  a.layers[layer].states.push_back(s);return a.layers[layer].states.back();
}
scene::AnimatorTransition &transition(scene::Animator &a,u64 from,u64 to,float duration,std::vector<scene::AnimatorCondition> conditions,u32 layer=0) {
  scene::AnimatorTransition t;t.id=a.allocateId();t.from=from;t.to=to;t.duration=duration;t.conditions=std::move(conditions);
  a.layers[layer].transitions.push_back(t);return a.layers[layer].transitions.back();
}
struct Play {
  GameWorld world;SceneAnimator mixer;SceneAnimatorGraphs graphs;ComponentEventQueue events;Rig &rig;
  explicit Play(Rig &r):rig(r) {
    world.load(r.g);mixer.begin(world.poseGraph(),r.library);graphs.setEvents(&events);
    events.attach(ComponentEventQueue::Consumer::Scripts,true);
  }
  void step(float dt) {
    world.beginFrame(dt);std::vector<SceneAnimator::ExternalSample> samples;
    graphs.advance(world,rig.library,dt,dt,samples);mixer.setExternalSamples(std::move(samples));mixer.advance(dt,nullptr);
  }
  float x(ObjectId id) const {return world.graph().find(id)->transform.position[0];}
  std::vector<std::string> drain() {
    std::vector<std::string> out;
    events.consume(ComponentEventQueue::Consumer::Scripts,[&](const ComponentEventRecord &r,u64){
      out.push_back(std::string(r.type->events[r.event].id)+":"+std::to_string(r.values[r.count-1].integer));});
    return out;
  }
  ComponentHandle handle() {return world.findComponent(world.handle(rig.owner),"astra.animation.animator");}
};
bool near(float a,float b,float e=1e-3f) {return std::abs(a-b)<=e;}
}

AE_TEST(animator_blend_1d_and_2d_weights) {
  std::vector<scene::AnimatorMotion> line{{{},2},{{},0},{{},1}};std::vector<blend::WeightedMotion> w;
  blend::blend1D(line,.25f,w);
  AE_EXPECT_TRUE(w.size()==2&&w[0].motion==1&&near(w[0].weight,.75f)&&w[1].motion==2&&near(w[1].weight,.25f),"1D entre os vizinhos ordenados por limiar");
  blend::blend1D(line,5,w);AE_EXPECT_TRUE(w.size()==1&&w[0].motion==0&&w[0].weight==1,"acima da faixa: o maior limiar");
  // Cruz de 5 pontos (parado no centro, quatro direções).
  std::vector<scene::AnimatorMotion> cross{{{},0,0,0},{{},0,0,1},{{},0,1,0},{{},0,0,-1},{{},0,-1,0}};
  blend::blend2D(cross,0,0,w);AE_EXPECT_TRUE(w.size()==1&&w[0].motion==0&&near(w[0].weight,1),"no ponto: só ele");
  blend::blend2D(cross,.5f,.5f,w);float sum=0,center=0,front=0,right=0;
  for(const auto &x:w) {sum+=x.weight;center+=x.motion==0?x.weight:0;front+=x.motion==1?x.weight:0;right+=x.motion==2?x.weight:0;}
  AE_EXPECT_TRUE(near(sum,1)&&near(center,0)&&near(front,.5f)&&near(right,.5f),"dentro do triângulo: baricêntricas");
  blend::blend2D(cross,0,3,w);AE_EXPECT_TRUE(w.size()==1&&w[0].motion==1,"fora: o ponto mais próximo da casca");
  std::vector<scene::AnimatorMotion> collinear{{{},0,0,0},{{},0,1,0},{{},0,2,0}};
  blend::blend2D(collinear,1.5f,.3f,w);
  AE_EXPECT_TRUE(w.size()==2&&near(w[0].weight+w[1].weight,1)&&((w[0].motion==1&&near(w[0].weight,.5f))||(w[1].motion==1&&near(w[1].weight,.5f))),"pontos colineares viram 1D");
  AE_EXPECT_TRUE(blend::crossed(.8f,1.3f,.2f,true)&&!blend::crossed(.3f,.8f,.2f,true)&&blend::crossed(.3f,.8f,.5f,false)&&!blend::crossed(1.1f,1.3f,.5f,false),"passagem por marca em laço e sem laço");
}

AE_TEST(animator_contract_round_trip_and_validation) {
  scene::Animator a;scene::initializeAnimator(a);
  AE_EXPECT_TRUE(a.layers.size()==1&&a.layers[0].states.size()==1&&a.layers[0].defaultState==a.layers[0].states[0].id&&a.valid(),"grafo novo com camada Base e estado Parado");
  auto &speed=param(a,"Velocidade",scene::AnimatorParameterType::Float);const auto speedId=speed.id;
  param(a,"Pular",scene::AnimatorParameterType::Trigger);
  auto &walk=state(a,"Andar com \"aspas\"",{resources::assetGuidFromSeed("x")});walk.events.push_back({.5f,7});
  AE_EXPECT_TRUE(!a.valid(),"aspas no nome recusadas");
  a.layers[0].states.back().name="Andar";
  transition(a,a.layers[0].defaultState,a.layers[0].states.back().id,.25f,{{speedId,scene::AnimatorConditionMode::Greater,.5f}});
  AE_EXPECT_TRUE(a.valid(),"grafo válido");
  std::stringstream text;a.write(text);scene::Animator back;
  AE_EXPECT_TRUE(back.read(text,2)&&back.parameters==a.parameters&&back.layers==a.layers&&back.nextId==a.nextId,"relido igual");
  back.layers[0].transitions[0].conditions[0].mode=scene::AnimatorConditionMode::Equals;
  AE_EXPECT_TRUE(!back.valid(),"Float não aceita Igual");
  back.layers[0].transitions[0].conditions[0]={back.parameters[1].id,scene::AnimatorConditionMode::IfNot,0};
  AE_EXPECT_TRUE(!back.valid(),"gatilho só aceita Se");
  scene::Components components;components.add(scene::Animation::descriptor);
  AE_EXPECT_TRUE(!scene::planComponentAddition(components,"astra.animation.animator").ready,"Animator e Animação não convivem");
}

AE_TEST(animator_transition_crossfades_consumes_triggers_and_returns_by_exit_time) {
  Rig r;auto &a=r.animator();
  const auto speed=param(a,"Velocidade",scene::AnimatorParameterType::Float).id;
  const auto jump=param(a,"Pular",scene::AnimatorParameterType::Trigger).id;
  auto &base=a.layers[0];base.states[0].motions.push_back({r.idle});const auto idle=base.states[0].id;
  const auto walk=state(a,"Andar",{r.walk}).id;
  auto &jumping=state(a,"Pular",{r.jump});jumping.loop=false;const auto jumpId=jumping.id;
  transition(a,idle,walk,.5f,{{speed,scene::AnimatorConditionMode::Greater,.5f}});
  transition(a,0,jumpId,0,{{jump,scene::AnimatorConditionMode::If,0}});
  auto &back=transition(a,jumpId,idle,0,{});back.hasExitTime=true;back.exitTime=1;
  AE_EXPECT_TRUE(a.valid(),"grafo");
  Play p(r);p.step(.1f);
  AE_EXPECT_TRUE(near(p.x(r.hips),0),"começa no padrão");
  AE_EXPECT_TRUE(p.drain()==std::vector<std::string>{"state_entered:"+std::to_string(idle)},"entrada no estado padrão");
  float result=0;
  AE_EXPECT_TRUE(p.graphs.parameter(p.world,r.owner,p.handle().instance,"Velocidade",SceneAnimatorGraphs::ParameterOperation::SetInt,1,result)==SceneAnimatorGraphs::Status::WrongType,"tipo errado recusado");
  AE_EXPECT_TRUE(p.graphs.parameter(p.world,r.owner,p.handle().instance,"Nada",SceneAnimatorGraphs::ParameterOperation::SetFloat,1,result)==SceneAnimatorGraphs::Status::UnknownParameter,"nome desconhecido recusado");
  p.graphs.parameter(p.world,r.owner,p.handle().instance,"Velocidade",SceneAnimatorGraphs::ParameterOperation::SetFloat,1,result);
  p.step(.1f);  // transição começa
  p.step(.25f);
  AE_EXPECT_TRUE(near(p.x(r.hips),.5f,.02f),"metade da transição de 0,5 s");
  ComponentOperationServices services;services.world=&p.world;services.animators=&p.graphs;scene::ComponentOperationValue value;
  AE_EXPECT_TRUE(invokeComponentMethod(services,p.handle(),"in_transition",{},value)==WorldStatus::Ok&&value.boolean==1,"in_transition pela ABI");
  p.step(.3f);AE_EXPECT_TRUE(near(p.x(r.hips),1),"chegou em Andar");
  p.drain();
  p.graphs.parameter(p.world,r.owner,p.handle().instance,"Velocidade",SceneAnimatorGraphs::ParameterOperation::SetFloat,0,result);
  p.graphs.parameter(p.world,r.owner,p.handle().instance,"Pular",SceneAnimatorGraphs::ParameterOperation::SetTrigger,0,result);
  p.step(.1f);AE_EXPECT_TRUE(near(p.x(r.hips),3),"Qualquer estado → Pular sem mistura");
  p.graphs.parameter(p.world,r.owner,p.handle().instance,"Pular",SceneAnimatorGraphs::ParameterOperation::Get,0,result);
  AE_EXPECT_TRUE(result==0,"gatilho consumido pela transição");
  for(u32 k=0;k<6;++k) p.step(.1f);
  SceneAnimatorGraphs::Info info;p.graphs.info(p.world,r.owner,p.handle().instance,0,info);
  AE_EXPECT_TRUE(info.name=="Parado"&&near(p.x(r.hips),0),"tempo de saída devolve ao Parado depois de 0,5 s");
  const auto events=p.drain();
  AE_EXPECT_TRUE(events.size()==2&&events[0]=="state_entered:"+std::to_string(jumpId)&&events[1]=="state_entered:"+std::to_string(idle),"entradas na ordem");
  // Play e CrossFade por nome.
  AE_EXPECT_TRUE(p.graphs.play(p.world,r.owner,p.handle().instance,0,"Pular",0)==SceneAnimatorGraphs::Status::Ok,"Play");
  p.step(.05f);AE_EXPECT_TRUE(near(p.x(r.hips),3),"Play troca na hora");
  AE_EXPECT_TRUE(p.graphs.play(p.world,r.owner,p.handle().instance,0,"Inexistente",0)==SceneAnimatorGraphs::Status::UnknownState,"estado desconhecido");
}

AE_TEST(animator_blend_tree_state_follows_its_parameter_and_speed) {
  Rig r;auto &a=r.animator();
  const auto speed=param(a,"Velocidade",scene::AnimatorParameterType::Float,1.5f).id;
  auto &loco=a.layers[0].states[0];loco.name="Locomoção";loco.kind=scene::AnimatorMotionKind::Blend1D;loco.blendX=speed;
  loco.motions={{r.idle,0},{r.walk,1},{r.run,2}};
  Play p(r);p.step(.1f);
  AE_EXPECT_TRUE(near(p.x(r.hips),1.5f),"1,5 entre Andar e Correr");
  float result=0;p.graphs.parameter(p.world,r.owner,p.handle().instance,"Velocidade",SceneAnimatorGraphs::ParameterOperation::SetFloat,.25f,result);
  p.step(.1f);AE_EXPECT_TRUE(near(p.x(r.hips),.25f),"o estado acompanha o parâmetro a cada quadro");
}

AE_TEST(animator_layer_with_mask_overrides_only_its_subtree_and_emits_events) {
  Rig r;auto &a=r.animator();
  a.layers[0].states[0].motions.push_back({r.walk});a.layers[0].states[0].events.push_back({.5f,9});
  scene::AnimatorLayer upper;upper.id=a.allocateId();upper.name="Tronco";upper.weight=.5f;upper.mask=r.spine;
  scene::AnimatorState wave;wave.id=a.allocateId();wave.name="Acenar";wave.motions.push_back({r.wave});
  upper.defaultState=wave.id;upper.states.push_back(wave);a.layers.push_back(upper);
  AE_EXPECT_TRUE(a.valid(),"duas camadas");
  Play p(r);p.drain();
  p.step(.3f);p.drain();p.step(.3f);
  AE_EXPECT_TRUE(near(p.x(r.hips),1),"quadril só recebe a camada base");
  AE_EXPECT_TRUE(near(p.x(r.arm),5,.01f),"braço: metade do aceno (peso 0,5 sobre o repouso 0)");
  const auto events=p.drain();
  AE_EXPECT_TRUE(events.size()==1&&events[0]=="state_event:9","evento em 0,5 do estado base");
  for(u32 k=0;k<10;++k) p.step(.1f);
  AE_EXPECT_TRUE(p.drain().size()==1,"uma vez por volta");
  a.enabled=false;  // autoria não afeta o mundo já carregado: liga/desliga pelo mundo
  ComponentHandle h=p.handle();
  AE_EXPECT_TRUE(p.world.setProperty(h,"enabled",false)==WorldStatus::Ok,"desligar no Play");
  const float frozen=p.x(r.hips);p.step(.4f);
  AE_EXPECT_TRUE(p.x(r.hips)==frozen,"desligado congela a pose");
}

AE_TEST(animator_v2_binding_roundtrip_and_v1_manual_migration) {
  scene::Animator a;scene::initializeAnimator(a);a.motionSource=42;
  auto &speed=param(a,"Velocidade medida",scene::AnimatorParameterType::Float);
  speed.source=scene::AnimatorParameterSource::PlanarSpeed;speed.response=.15f;speed.scale=.5f;
  auto &ground=param(a,"Apoio",scene::AnimatorParameterType::Bool);ground.source=scene::AnimatorParameterSource::Grounded;
  std::stringstream stream;a.write(stream);scene::Animator restored;
  AE_EXPECT_TRUE(restored.read(stream,2)&&restored.motionSource==42&&restored.parameters==a.parameters,"typed sources and response survive archive");
  std::stringstream old("1 0 0 1 4 1 3 \"Peso\" 0 0.5 1 1 \"Base\" 1 0 2 1 2 \"Parado\" 0 0 0 1 0 1 0 0 0 0 0");
  scene::Animator migrated;AE_EXPECT_TRUE(migrated.read(old,1)&&migrated.motionSource==0&&migrated.parameters[0].source==scene::AnimatorParameterSource::Manual,"old graph remains script controlled");
  restored.parameters[1].source=scene::AnimatorParameterSource::PlanarSpeed;
  AE_EXPECT_TRUE(!restored.valid(),"bool speed binding rejected");

  SceneGraph sceneGraph;const auto root=sceneGraph.createEntity(sceneGraph.root(),ObjectKind::Folder,"Mecanismo");
  const auto visual=sceneGraph.createEntity(root,ObjectKind::Folder,"Parte visual");
  auto &controller=scene::animator(*sceneGraph.editComponents(root)->add(scene::Animator::descriptor));controller.target=visual;controller.motionSource=root;
  Prefab prefab;std::string diagnostic;AE_EXPECT_TRUE(prefab.capture(sceneGraph,root,resources::assetGuidFromSeed("mecanismo-prefab"),diagnostic),diagnostic.c_str());
  SceneGraph destination;destination.createEntity(destination.root(),ObjectKind::Folder,"Outra entidade");ObjectCloneMap mapping;
  const auto copy=prefab.instantiate(destination,destination.root(),mapping,diagnostic);AE_EXPECT_TRUE(copy,diagnostic.c_str());
  const auto &mapped=scene::animator(*destination.find(copy)->components.find(scene::Animator::descriptor));
  AE_EXPECT_TRUE(mapped.motionSource==copy&&mapped.target==mapping.at(visual),"visual and motion references remap independently in reusable prefab");
}

AE_TEST(animator_physical_binding_real_body_pose_and_missing_source_diagnostic) {
  Rig r;auto &a=r.animator();
  const auto machine=r.g.createEntity(r.g.root(),ObjectKind::Folder,"Mecanismo físico");
  auto *parts=r.g.editComponents(machine);auto &body=static_cast<scene::PhysicsBody&>(*parts->add(scene::PhysicsBody::descriptor));body.motion=scene::BodyMotion::Dynamic;
  parts->add(scene::Collider::descriptor);a.motionSource=machine;
  auto &speed=param(a,"Velocidade",scene::AnimatorParameterType::Float);speed.source=scene::AnimatorParameterSource::PlanarSpeed;speed.response=.2f;speed.scale=2;
  auto &vertical=param(a,"Vertical",scene::AnimatorParameterType::Float);vertical.source=scene::AnimatorParameterSource::VerticalSpeed;
  auto &s=a.layers[0].states[0];s.kind=scene::AnimatorMotionKind::Blend1D;s.blendX=a.parameters[0].id;s.motions={{r.idle,0},{r.walk,6}};
  Play p(r);ScenePhysics physics;AE_EXPECT_TRUE(physics.start(p.world),"real Jolt rigid body without player or motor");
  p.graphs.setPhysics(&physics);float velocity[]{3,4,0};AE_EXPECT_TRUE(physics.setBodyVelocity(machine,velocity),"physical velocity");
  p.step(.1f);float value=0;
  const float expected=6*(-std::expm1(-.1f/.2f));
  AE_EXPECT_TRUE(p.graphs.parameter(p.world,r.owner,p.handle().instance,"Velocidade",SceneAnimatorGraphs::ParameterOperation::Get,0,value)==SceneAnimatorGraphs::Status::Ok&&std::fabs(value-expected)<1e-4f,"actual motion, scale and response feed resolved value");
  AE_EXPECT_TRUE(std::fabs(p.x(r.hips)-expected/6)<1e-4f,"measured filtered motion drives actual pose mixer");
  AE_EXPECT_TRUE(p.graphs.parameter(p.world,r.owner,p.handle().instance,"Vertical",SceneAnimatorGraphs::ParameterOperation::Get,0,value)==SceneAnimatorGraphs::Status::Ok&&std::fabs(value-4)<1e-4f,"vertical component independent");
  AE_EXPECT_TRUE(p.graphs.parameter(p.world,r.owner,p.handle().instance,"Velocidade",SceneAnimatorGraphs::ParameterOperation::SetFloat,9,value)==SceneAnimatorGraphs::Status::BoundParameter,"script cannot silently compete with bound source");
  AE_EXPECT_TRUE(p.world.setProperty(p.handle(),"motion_source",scene::ObjectReference{r.hips})==WorldStatus::Ok,"retarget source at safe point");
  p.step(.1f);const auto *live=p.graphs.find(r.owner,p.handle().instance);
  AE_EXPECT_TRUE(live&&!live->motionAvailable&&!live->motionDiagnostic.empty()&&std::fabs(p.x(r.hips))<1e-4f,"missing physical source is diagnosed and cannot keep phantom movement");
}

AE_TEST(animator_motion_bindings_character_and_dynamic_motor_use_resolved_motion_and_support) {
  for(bool character:{true,false}) {
    Rig r;auto &a=r.animator();const auto actor=r.g.createEntity(r.g.root(),ObjectKind::Folder,"Fonte de movimento independente");
    auto source=*r.g.find(actor);source.transform.position[1]=2;
    if(character) source.components.add(scene::Character::descriptor);
    else {auto *body=static_cast<scene::PhysicsBody*>(source.components.add(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Dynamic;
      for(auto &axis:body->freezeRotation) axis=true;
      source.components.add(scene::Collider::descriptor);source.components.add(scene::DynamicBodyMotor::descriptor);}
    r.g.applyEntityValues(actor,source);const auto floor=r.g.createEntity(r.g.root(),ObjectKind::Folder,"Apoio");auto support=*r.g.find(floor);support.transform.position[1]=-.5f;
    support.components.add(scene::PhysicsBody::descriptor);auto *shape=static_cast<scene::Collider*>(support.components.add(scene::Collider::descriptor));shape->halfX=shape->halfZ=30;shape->halfY=.5f;r.g.applyEntityValues(floor,support);
    a.motionSource=actor;param(a,"Medida",scene::AnimatorParameterType::Float).source=scene::AnimatorParameterSource::PlanarSpeed;
    param(a,"No chão",scene::AnimatorParameterType::Bool).source=scene::AnimatorParameterSource::Grounded;
    Play p(r);ScenePhysics physics;AE_EXPECT_TRUE(physics.start(p.world),"real independent motor source");p.graphs.setPhysics(&physics);
    for(int frame=0;frame<180;++frame) {physics.advance(1./60,p.world);p.step(1.f/60);}
    float value=0;AE_EXPECT_TRUE(p.graphs.parameter(p.world,r.owner,p.handle().instance,"No chão",SceneAnimatorGraphs::ParameterOperation::Get,0,value)==SceneAnimatorGraphs::Status::Ok&&value==1,"support comes from resolved solver state");
    for(int frame=0;frame<45;++frame) {
      AE_EXPECT_TRUE(character?physics.setCharacterMove(actor,1,0,0):physics.setDynamicMotorMove(actor,1,0,0),"actual movement intent");
      physics.advance(1./60,p.world);p.step(1.f/60);
    }
    AE_EXPECT_TRUE(p.graphs.parameter(p.world,r.owner,p.handle().instance,"Medida",SceneAnimatorGraphs::ParameterOperation::Get,0,value)==SceneAnimatorGraphs::Status::Ok&&value>1,"actual post-solver speed drives generic graph");
    physics.stop();p.step(1.f/60);const auto *live=p.graphs.find(r.owner,p.handle().instance);
    AE_EXPECT_TRUE(live&&!live->motionAvailable&&!live->motionDiagnostic.empty(),"stopped physics loses binding explicitly");
  }
}
