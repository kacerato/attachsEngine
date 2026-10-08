#include "harness.h"
#include "runtime/component_operations.h"
#include "runtime/scene_animator_graph.h"
#include "runtime/scene_animation.h"
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
  AE_EXPECT_TRUE(back.read(text,1)&&back.parameters==a.parameters&&back.layers==a.layers&&back.nextId==a.nextId,"relido igual");
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
