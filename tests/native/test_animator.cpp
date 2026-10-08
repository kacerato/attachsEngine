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
#include "scene/skinned_mesh.h"
#include "resources/animation_composition.h"
#include "runtime/transform_math.h"

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
    world.load(r.g);mixer.begin(world.poseGraph(),r.library);graphs.setEvents(&events);graphs.setLibrary(&r.library);
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

AE_TEST(animator_additive_pose_runtime_api_retired_channels_and_physics_authority) {
  Rig r;auto &a=r.animator();a.layers[0].states[0].motions={{r.walk}};
  scene::AnimatorLayer additive;additive.id=a.allocateId();additive.name="Delta";additive.blend=scene::AnimatorLayerBlend::Additive;
  additive.weight=.5f;additive.referenceClip=r.run;
  a.layers.push_back(additive);auto &s=state(a,"Deslocar",{r.jump},1);a.layers[1].defaultState=s.id;
  state(a,"Maior",{r.library.add("Maior delta",0,5)},1);
  state(a,"Sem movimento",{},1);
  Play play(r);play.step(.1f);
  AE_EXPECT_TRUE(near(play.x(r.hips),1.5f),"base 1 + half of (sample 3 - reference 2)");
  AE_EXPECT_TRUE(play.graphs.play(play.world,r.owner,play.handle().instance,1,"Maior",.2f)==SceneAnimatorGraphs::Status::Ok,"crossfade additive motions");
  play.step(.1f);AE_EXPECT_TRUE(near(play.x(r.hips),2),"transition mixes relative deltas with one layer weight");
  play.graphs.play(play.world,r.owner,play.handle().instance,1,"Deslocar",0);play.step(0);
  ComponentOperationServices services;services.world=&play.world;services.animators=&play.graphs;
  using V=scene::ComponentOperationValue;V result;
  const auto call=[&](const char *name,std::initializer_list<V> args) {return invokeComponentMethod(services,play.handle(),name,{args.begin(),args.size()},result);};
  AE_EXPECT_TRUE(call("set_layer_weight",{V::makeInteger(1),V::makeNumber(0)})==WorldStatus::Ok,"API accepts instance weight");
  play.step(.1f);AE_EXPECT_TRUE(near(play.x(r.hips),1),"zero weight actually removes the delta");
  AE_EXPECT_TRUE(call("set_layer_weight",{V::makeInteger(1),V::makeNumber(1)})==WorldStatus::Ok,"full delta");
  play.step(.1f);AE_EXPECT_TRUE(near(play.x(r.hips),2),"API changes actual pose, not authoring graph");
  AE_EXPECT_TRUE(call("set_layer_weight",{V::makeInteger(1),V::makeNumber(2)})==WorldStatus::InvalidArgument&&
    call("get_layer_weight",{V::makeInteger(88)})==WorldStatus::InvalidArgument,"invalid inputs rejected");
  const auto missing=resources::assetGuidFromSeed("absent-reference");
  AE_EXPECT_TRUE(call("set_layer_reference",{V::makeInteger(1),V::makeInteger(static_cast<i64>(missing.high)),V::makeInteger(static_cast<i64>(missing.low))})==WorldStatus::UnknownResource,"unknown reference rejected without silently switching baseline");
  AE_EXPECT_TRUE(call("reset_layer_overrides",{V::makeInteger(1)})==WorldStatus::Ok,"restore authored composition");
  play.step(.1f);AE_EXPECT_TRUE(near(play.x(r.hips),1.5f)&&a.layers[1].weight==.5f,"runtime changes did not mutate authoring");
  AE_EXPECT_TRUE(play.graphs.play(play.world,r.owner,play.handle().instance,1,"Sem movimento",0)==SceneAnimatorGraphs::Status::Ok,"switch active graph to empty state");
  play.step(.1f);AE_EXPECT_TRUE(near(play.x(r.hips),1),"retired channels restore baseline instead of freezing delta");
  std::vector<SceneAnimator::ExternalSample> samples{{r.owner,r.jump,0,1,1,0,true,r.run,0}};
  play.mixer.setExternalSamples(std::move(samples));play.mixer.advance(0,[&](ObjectId target){return target!=r.hips;});
  AE_EXPECT_TRUE(near(play.x(r.hips),1),"animation does not write physics-owned transform");
}

AE_TEST(animator_additive_reference_math_mask_and_invalid_reference) {
  Rig r;auto hips=*r.g.find(r.hips);hips.transform.position[0]=5;hips.transform.scale[0]=2;hips.transform.rotationDegrees[2]=30;
  static_cast<scene::SkinnedMesh*>(hips.components.add(scene::SkinnedMesh::descriptor))->blendShapeWeights={20};r.g.applyEntityValues(r.hips,hips);
  const auto clip=r.library.add("DeltaTRS",0,7);
  auto &channels=r.library.source.clips.back().channels;
  resources::AnimationChannel scale;scale.node=0;scale.path=resources::AnimationPath::Scale;scale.times={0,1};scale.values={4,1,1,4,1,1};channels.push_back(scale);
  resources::AnimationChannel rotation;rotation.node=0;rotation.path=resources::AnimationPath::Rotation;rotation.times={0,1};
  const float sin60=std::sin(3.14159265359f/3),cos60=.5f;rotation.values={0,0,sin60,cos60,0,0,sin60,cos60};channels.push_back(rotation);
  resources::AnimationChannel morph;morph.node=0;morph.path=resources::AnimationPath::Weights;morph.weightCount=1;morph.times={0,1};morph.values={.8f,.8f};channels.push_back(morph);
  Play p(r);p.mixer.setExternalSamples({{r.owner,clip,0,.5f,0,0,true,{},0}});p.mixer.advance(0,nullptr);
  const auto *posed=p.world.graph().find(r.hips);float q[4];transformRotationQuaternion(posed->transform,q);
  AE_EXPECT_TRUE(near(p.x(r.hips),6)&&near(posed->transform.scale[0],3)&&near(std::abs(q[2]),std::sin(75.f*3.14159265359f/360)),"nonidentity baseline: translation delta, scale ratio and quaternion half-angle");
  AE_EXPECT_TRUE(near(static_cast<const scene::SkinnedMesh*>(posed->components.find(scene::SkinnedMesh::descriptor))->blendShapeWeights[0],50),"morph delta reaches actual deformer weights in percent");
  p.mixer.setExternalSamples({{r.owner,clip,0,.5f,0,0,true,{},0},{r.owner,r.run,0,.5f,1,0}});p.mixer.advance(0,nullptr);
  AE_EXPECT_TRUE(near(p.x(r.hips),4)&&near(p.world.graph().find(r.hips)->transform.scale[0],3),"higher override attenuates lower delta only on its authored properties");
  p.mixer.setExternalSamples({{r.owner,clip,0,1,0,r.arm,true,{},0}});p.mixer.advance(0,nullptr);
  AE_EXPECT_TRUE(near(p.x(r.hips),5)&&near(p.world.graph().find(r.hips)->transform.scale[0],2),"mask change removes old channels");
  p.mixer.setExternalSamples({{r.owner,clip,0,1,0,0,true,r.walk,0}});p.mixer.advance(0,nullptr);
  AE_EXPECT_TRUE(near(p.x(r.hips),11)&&near(p.world.graph().find(r.hips)->transform.scale[0],2)&&!p.mixer.compositionDiagnostic().empty(),"partial explicit reference reports missing TRS channels and preserves their baseline");
  const auto absent=resources::assetGuidFromSeed("missing");p.mixer.setExternalSamples({{r.owner,clip,0,1,0,0,true,absent,0}});p.mixer.advance(0,nullptr);
  AE_EXPECT_TRUE(near(p.x(r.hips),5)&&!p.mixer.compositionDiagnostic().empty(),"missing explicit reference cannot silently become initial pose");
  std::vector<float> delta;
  AE_EXPECT_TRUE(!resources::relativeAnimationPose(resources::AnimationPath::Scale,std::vector<float>{1,1,1},std::vector<float>{0,1,1},delta),"zero reference scale has explicit failure");
  std::vector<float> opposite;
  AE_EXPECT_TRUE(resources::relativeAnimationPose(resources::AnimationPath::Rotation,std::vector<float>{0,0,1,0},std::vector<float>{0,0,0,1},delta)&&
    resources::relativeAnimationPose(resources::AnimationPath::Rotation,std::vector<float>{0,0,-1,0},std::vector<float>{0,0,0,1},opposite)&&delta==opposite,"180-degree antipodal representations yield the same weighted delta");
}

AE_TEST(animator_additive_archive_migration_and_shared_instance_isolation) {
  Rig r;auto &a=r.animator();auto &layer=a.layers[0];layer.blend=scene::AnimatorLayerBlend::Additive;layer.weight=.25f;layer.referenceClip=r.walk;layer.referenceTime=.4f;layer.states[0].motions={{r.jump}};
  resources::AnimatorControllerAsset asset;asset.guid=resources::assetGuidFromSeed("shared-additive");asset.name="Universal";asset.graph=resources::AnimatorControllerAsset::portableGraph(a);
  resources::AnimatorControllerAsset back;AE_EXPECT_TRUE(resources::AnimatorControllerAsset::deserialize(asset.serialize(),back)&&back.graph.layers==asset.graph.layers,"v2 controller preserves complete additive settings");
  std::ostringstream tail;tail<<' '<<a.layers.size();for(const auto &l:a.layers) tail<<' '<<l.id<<' '<<u32(l.blend)<<' '<<l.referenceClip.text()<<' '<<l.referenceTime;
  std::ostringstream raw;a.write(raw);const auto oldGraph=raw.str().substr(0,raw.str().size()-tail.str().size());
  std::istringstream old(oldGraph);scene::Animator migrated;
  AE_EXPECT_TRUE(migrated.read(old,3)&&migrated.layers[0].blend==scene::AnimatorLayerBlend::Override&&migrated.layers[0].weight==1&&!migrated.layers[0].referenceClip.valid(),"v3 preserves old full-weight override semantics");
  std::ostringstream legacy;legacy<<"AEANIMATOR 1 "<<asset.guid.text()<<" 1 \"Universal\" "<<oldGraph;
  AE_EXPECT_TRUE(resources::AnimatorControllerAsset::deserialize(legacy.str(),back)&&back.graph.layers[0].weight==1,"v1 shared assets remain readable");
  a.controller=asset.guid;
  const auto second=r.g.createEntity(r.g.root(),ObjectKind::Folder,"Outro objeto"),secondHips=r.g.createEntity(second,ObjectKind::Folder,"Quadril");
  auto &other=scene::animator(*r.g.editComponents(second)->add(scene::Animator::descriptor));other.controller=asset.guid;other.clipOverrides={{r.walk,r.run}};
  Play p(r);p.graphs.setControllers(std::span(&asset,1));p.step(.1f);
  AE_EXPECT_TRUE(near(p.x(r.hips),.5f)&&near(p.x(secondHips),.25f),"reference substitution changes actual delta per instance");
  SceneAnimatorGraphs::LayerSettings result;
  AE_EXPECT_TRUE(p.graphs.layerControl(p.world,r.owner,p.handle().instance,0,1,1,{},result)==WorldStatus::Ok,"independent runtime composition");
  p.step(.1f);AE_EXPECT_TRUE(near(p.x(r.hips),2)&&near(p.x(secondHips),.25f)&&asset.graph.layers[0].weight==.25f,"shared resource remains unchanged and second instance remains independent");
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
  AE_EXPECT_TRUE(back.read(text,4)&&back.parameters==a.parameters&&back.layers==a.layers&&back.nextId==a.nextId,"relido igual");
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

AE_TEST(animator_shared_controller_overrides_pose_archive_missing_and_revision) {
  Rig rig;auto &inlineGraph=rig.animator();
  inlineGraph.layers[0].states[0].name="Mover";inlineGraph.layers[0].states[0].motions={{rig.walk}};
  param(inlineGraph,"Ajuste",scene::AnimatorParameterType::Float);
  resources::AnimatorControllerAsset asset;asset.guid=resources::assetGuidFromSeed("controller-compartilhado");
  asset.name="Mecanismo";asset.graph=resources::AnimatorControllerAsset::portableGraph(inlineGraph);
  resources::AnimatorControllerAsset read;
  AE_EXPECT_TRUE(asset.valid()&&resources::AnimatorControllerAsset::deserialize(asset.serialize(),read),"resource archive is portable and typed");
  auto invalid=asset;invalid.graph.target=rig.owner;
  AE_EXPECT_TRUE(!invalid.valid()&&!resources::AnimatorControllerAsset::deserialize(asset.serialize()+" garbage",read),"object IDs and trailing corrupt data rejected");
  inlineGraph.controller=asset.guid;
  const auto second=rig.g.createEntity(rig.g.root(),ObjectKind::Folder,"Segundo mecanismo");
  const auto secondHips=rig.g.createEntity(second,ObjectKind::Folder,"Quadril");
  auto &secondGraph=scene::animator(*rig.g.editComponents(second)->add(scene::Animator::descriptor));
  secondGraph.controller=asset.guid;secondGraph.clipOverrides={{rig.walk,rig.run}};
  const u64 instance=inlineGraph.instanceId(),secondInstance=secondGraph.instanceId();
  Play play(rig);play.graphs.setControllers(std::span(&asset,1));play.step(.1f);
  AE_EXPECT_TRUE(near(play.x(rig.hips),1)&&near(play.x(secondHips),2),"one shared graph produces different actual poses through local overrides");
  AE_EXPECT_TRUE(play.graphs.configuration(play.world,second,secondInstance)->layers[0].states[0].id==asset.graph.layers[0].states[0].id,"stable state identity shared, runtime state separate");
  float result=0;
  AE_EXPECT_TRUE(play.graphs.parameter(play.world,second,secondInstance,"Ajuste",SceneAnimatorGraphs::ParameterOperation::SetFloat,7,result)==SceneAnimatorGraphs::Status::Ok,"shared parameter API reaches effective graph");
  AE_EXPECT_TRUE(play.graphs.parameter(play.world,rig.owner,instance,"Ajuste",SceneAnimatorGraphs::ParameterOperation::Get,0,result)==SceneAnimatorGraphs::Status::Ok&&result==0,"runtime parameters independent");
  std::ostringstream out;secondGraph.write(out);std::istringstream in(out.str());scene::Animator back;
  AE_EXPECT_TRUE(back.read(in,4)&&back.controller==asset.guid&&back.clipOverrides==secondGraph.clipOverrides,"instance reference and substitutions persist");
  back.clipOverrides.push_back(back.clipOverrides.front());AE_EXPECT_TRUE(!back.valid(),"duplicate override authorities rejected");
  auto revision=asset;++revision.revision;revision.graph.layers[0].states[0].speed=2;
  play.graphs.setControllers(std::span(&revision,1));play.step(.1f);
  AE_EXPECT_TRUE(near(play.x(secondHips),2)&&play.graphs.configuration(play.world,second,secondInstance)->layers[0].states[0].speed==2,"new source revision keeps instance substitution");
  play.graphs.setControllers({});play.step(.1f);
  const auto *missing=play.graphs.find(second,secondInstance);
  AE_EXPECT_TRUE(missing&&!missing->controllerAvailable&&!missing->controllerDiagnostic.empty(),"missing resource is explicit and never plays stale inline graph");
  AE_EXPECT_TRUE(play.graphs.parameter(play.world,second,secondInstance,"Ajuste",SceneAnimatorGraphs::ParameterOperation::Get,0,result)==SceneAnimatorGraphs::Status::MissingController,"API rejects unavailable resource");
  auto resolved=secondGraph;std::string diagnostic;
  AE_EXPECT_TRUE(resources::resolveAnimatorController(secondGraph,std::span(&asset,1),resolved,diagnostic),"detach resolves effective graph");
  resolved.controller={};resolved.clipOverrides.clear();scene::replaceAnimatorData(secondGraph,resolved);
  AE_EXPECT_TRUE(secondGraph.instanceId()==secondInstance&&secondGraph.layers[0].states[0].motions[0].clip==rig.run,"detach preserves component identity and effective clips");
}

AE_TEST(animator_v2_binding_roundtrip_and_v1_manual_migration) {
  scene::Animator a;scene::initializeAnimator(a);a.motionSource=42;
  auto &speed=param(a,"Velocidade medida",scene::AnimatorParameterType::Float);
  speed.source=scene::AnimatorParameterSource::PlanarSpeed;speed.response=.15f;speed.scale=.5f;
  auto &ground=param(a,"Apoio",scene::AnimatorParameterType::Bool);ground.source=scene::AnimatorParameterSource::Grounded;
  std::stringstream stream;a.write(stream);scene::Animator restored;
  AE_EXPECT_TRUE(restored.read(stream,4)&&restored.motionSource==42&&restored.parameters==a.parameters,"typed sources and response survive archive");
  auto v2=stream.str();v2.resize(v2.rfind(" - 0"));std::stringstream legacy(v2);scene::Animator migratedV2;
  AE_EXPECT_TRUE(migratedV2.read(legacy,2)&&!migratedV2.controller.valid()&&migratedV2.clipOverrides.empty()&&migratedV2.parameters==a.parameters,"v2 remains inline with physical bindings preserved");
  std::stringstream old("1 0 0 1 4 1 3 \"Peso\" 0 0.5 1 1 \"Base\" 1 0 2 1 2 \"Parado\" 0 0 0 1 0 1 0 0 0 0 0");
  scene::Animator migrated;AE_EXPECT_TRUE(migrated.read(old,1)&&migrated.motionSource==0&&migrated.parameters[0].source==scene::AnimatorParameterSource::Manual,"old graph remains script controlled");
  restored.parameters[1].source=scene::AnimatorParameterSource::PlanarSpeed;
  AE_EXPECT_TRUE(!restored.valid(),"bool speed binding rejected");

  SceneGraph sceneGraph;const auto root=sceneGraph.createEntity(sceneGraph.root(),ObjectKind::Folder,"Mecanismo");
  const auto visual=sceneGraph.createEntity(root,ObjectKind::Folder,"Parte visual");
  auto &controller=scene::animator(*sceneGraph.editComponents(root)->add(scene::Animator::descriptor));controller.target=visual;controller.motionSource=root;
  controller.layers[0].mask=visual;controller.controller=resources::assetGuidFromSeed("shared-resource");
  controller.clipOverrides={{resources::assetGuidFromSeed("original"),resources::assetGuidFromSeed("replacement")}};
  Prefab prefab;std::string diagnostic;AE_EXPECT_TRUE(prefab.capture(sceneGraph,root,resources::assetGuidFromSeed("mecanismo-prefab"),diagnostic),diagnostic.c_str());
  SceneGraph destination;destination.createEntity(destination.root(),ObjectKind::Folder,"Outra entidade");ObjectCloneMap mapping;
  const auto copy=prefab.instantiate(destination,destination.root(),mapping,diagnostic);AE_EXPECT_TRUE(copy,diagnostic.c_str());
  const auto &mapped=scene::animator(*destination.find(copy)->components.find(scene::Animator::descriptor));
  AE_EXPECT_TRUE(mapped.motionSource==copy&&mapped.target==mapping.at(visual),"visual and motion references remap independently in reusable prefab");
  AE_EXPECT_TRUE(mapped.layers[0].mask==mapping.at(visual)&&mapped.controller==controller.controller&&mapped.clipOverrides==controller.clipOverrides,"prefab remaps local masks while preserving shared resource and override identity");
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
