// Animator da Astra (bloco I, F064): máquina de estados de animação com
// parâmetros, camadas, estados (clipe, mistura 1D, mistura 2D), transições com
// condições e tempo de saída, e eventos por estado. Avaliado por
// runtime/scene_animator_graph.h, que entrega amostras ao SceneAnimator.
//
// Referências estudadas:
// - Unity 6000.0 Animator Controller: https://docs.unity3d.com/6000.0/Documentation/Manual/class-AnimatorController.html
// - Unity Animation Transitions: https://docs.unity3d.com/6000.0/Documentation/Manual/class-Transition.html
// - Unity Blend Trees: https://docs.unity3d.com/6000.0/Documentation/Manual/class-BlendTree.html
// - Unity Animation Layers / Avatar Mask: https://docs.unity3d.com/6000.0/Documentation/Manual/AnimationLayers.html
// - Godot 4.5 AnimationNodeBlendSpace2D (triangulação): https://docs.godotengine.org/en/4.5/classes/class_animationnodeblendspace2d.html
// Adaptações: o grafo mora no próprio componente (não num asset separado);
// a máscara de camada é uma subárvore de objetos; só camadas de substituição
// (sem aditivas); sem sub-máquinas de estado.
#pragma once
#include "resources/asset_registry.h"
#include "scene/components.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <string>
#include <vector>

namespace ae::scene {

enum class AnimatorParameterType : u32 {Float=0,Int=1,Bool=2,Trigger=3};
enum class AnimatorMotionKind : u32 {Clip=0,Blend1D=1,Blend2D=2};
enum class AnimatorConditionMode : u32 {If=0,IfNot=1,Greater=2,Less=3,Equals=4,NotEqual=5};

struct AnimatorParameter {
  u64 id=0;std::string name;AnimatorParameterType type=AnimatorParameterType::Float;float value=0;
  bool operator==(const AnimatorParameter &) const=default;
};
struct AnimatorMotion {
  resources::AssetGuid clip{};float threshold=0,x=0,y=0;
  bool operator==(const AnimatorMotion &) const=default;
};
struct AnimatorStateEvent {
  float time=0;  // tempo normalizado do estado, 0..1
  u32 tag=0;     // número que chega ao evento `state_event`
  bool operator==(const AnimatorStateEvent &) const=default;
};
struct AnimatorState {
  u64 id=0;std::string name;AnimatorMotionKind kind=AnimatorMotionKind::Clip;
  std::vector<AnimatorMotion> motions;
  u64 blendX=0,blendY=0;          // parâmetros Float das misturas
  float speed=1;u64 speedParameter=0;bool loop=true;
  float x=0,y=0;                  // posição no grafo do editor
  std::vector<AnimatorStateEvent> events;
  bool operator==(const AnimatorState &) const=default;
};
struct AnimatorCondition {
  u64 parameter=0;AnimatorConditionMode mode=AnimatorConditionMode::If;float threshold=0;
  bool operator==(const AnimatorCondition &) const=default;
};
struct AnimatorTransition {
  u64 id=0,from=0,to=0;           // from zero: Qualquer estado
  bool hasExitTime=false;float exitTime=1,duration=.25f;
  std::vector<AnimatorCondition> conditions;
  bool operator==(const AnimatorTransition &) const=default;
};
struct AnimatorLayer {
  u64 id=0;std::string name;float weight=1;u64 mask=0;u64 defaultState=0;
  std::vector<AnimatorState> states;std::vector<AnimatorTransition> transitions;
  bool operator==(const AnimatorLayer &) const=default;
  const AnimatorState *state(u64 stateId) const {for(const auto &s:states) if(s.id==stateId) return &s;return nullptr;}
  AnimatorState *state(u64 stateId) {for(auto &s:states) if(s.id==stateId) return &s;return nullptr;}
};

class Animator final : public ComponentValue {
public:
  static constexpr usize MaximumParameters=32,MaximumLayers=4,MaximumStates=24,MaximumTransitions=48,
                         MaximumMotions=8,MaximumConditions=4,MaximumEvents=8,MaximumName=63;
  bool enabled=true,unscaledTime=false;
  u64 target=0;   // raiz animada; zero: este objeto
  float speed=1;
  std::vector<AnimatorParameter> parameters;
  std::vector<AnimatorLayer> layers;
  u64 nextId=1;

  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Animator>(*this);}

  u64 allocateId() {return nextId<std::numeric_limits<u64>::max()?nextId++:0;}
  static bool validName(const std::string &name) {
    if(name.empty()||name.size()>MaximumName) return false;
    for(unsigned char c:name) if(c<32||c==127||c=='"'||c=='\\') return false;
    return true;
  }
  const AnimatorParameter *parameter(u64 id) const {for(const auto &p:parameters) if(p.id==id) return &p;return nullptr;}
  const AnimatorParameter *parameter(std::string_view name) const {for(const auto &p:parameters) if(p.name==name) return &p;return nullptr;}
  const AnimatorLayer *layer(u64 id) const {for(const auto &l:layers) if(l.id==id) return &l;return nullptr;}
  AnimatorLayer *layer(u64 id) {for(auto &l:layers) if(l.id==id) return &l;return nullptr;}

  bool valid() const override {
    if(!std::isfinite(speed)||speed<-10||speed>10||target>std::numeric_limits<u32>::max()||
       parameters.size()>MaximumParameters||layers.size()>MaximumLayers||!nextId) return false;
    std::vector<u64> ids;
    const auto fresh=[&](u64 id){if(!id||id>=nextId||std::find(ids.begin(),ids.end(),id)!=ids.end()) return false;ids.push_back(id);return true;};
    for(usize i=0;i<parameters.size();++i) {
      const auto &p=parameters[i];
      if(!fresh(p.id)||!validName(p.name)||static_cast<u32>(p.type)>3||!std::isfinite(p.value)) return false;
      for(usize j=0;j<i;++j) if(parameters[j].name==p.name) return false;
    }
    const auto floatParameter=[&](u64 id){const auto *p=parameter(id);return id==0||(p&&(p->type==AnimatorParameterType::Float||p->type==AnimatorParameterType::Int));};
    for(const auto &l:layers) {
      if(!fresh(l.id)||!validName(l.name)||!std::isfinite(l.weight)||l.weight<0||l.weight>1||l.mask>std::numeric_limits<u32>::max()||
         l.states.size()>MaximumStates||l.transitions.size()>MaximumTransitions) return false;
      if(l.defaultState&&!l.state(l.defaultState)) return false;
      for(const auto &s:l.states) {
        if(!fresh(s.id)||!validName(s.name)||static_cast<u32>(s.kind)>2||s.motions.size()>MaximumMotions||s.events.size()>MaximumEvents||
           !std::isfinite(s.speed)||s.speed<-10||s.speed>10||!std::isfinite(s.x)||!std::isfinite(s.y)||
           !floatParameter(s.blendX)||!floatParameter(s.blendY)||!floatParameter(s.speedParameter)) return false;
        for(const auto &m:s.motions) if(!std::isfinite(m.threshold)||!std::isfinite(m.x)||!std::isfinite(m.y)) return false;
        for(const auto &e:s.events) if(!std::isfinite(e.time)||e.time<0||e.time>1) return false;
      }
      for(const auto &t:l.transitions) {
        if(!fresh(t.id)||(t.from&&!l.state(t.from))||!l.state(t.to)||!std::isfinite(t.exitTime)||t.exitTime<0||t.exitTime>100||
           !std::isfinite(t.duration)||t.duration<0||t.duration>60||t.conditions.size()>MaximumConditions) return false;
        for(const auto &c:t.conditions) {
          const auto *p=parameter(c.parameter);
          if(!p||static_cast<u32>(c.mode)>5||!std::isfinite(c.threshold)) return false;
          const bool boolean=p->type==AnimatorParameterType::Bool||p->type==AnimatorParameterType::Trigger;
          if(boolean!=(c.mode==AnimatorConditionMode::If||c.mode==AnimatorConditionMode::IfNot)) return false;
          if(p->type==AnimatorParameterType::Float&&(c.mode==AnimatorConditionMode::Equals||c.mode==AnimatorConditionMode::NotEqual)) return false;
          if(p->type==AnimatorParameterType::Trigger&&c.mode!=AnimatorConditionMode::If) return false;
        }
      }
    }
    return true;
  }
  void write(std::ostream &o) const override {
    const auto guid=[](const resources::AssetGuid &g){return g.valid()?g.text():std::string("-");};
    o<<enabled<<' '<<unscaledTime<<' '<<target<<' '<<speed<<' '<<nextId<<' '<<parameters.size();
    for(const auto &p:parameters) o<<' '<<p.id<<' '<<std::quoted(p.name)<<' '<<static_cast<u32>(p.type)<<' '<<p.value;
    o<<' '<<layers.size();
    for(const auto &l:layers) {
      o<<' '<<l.id<<' '<<std::quoted(l.name)<<' '<<l.weight<<' '<<l.mask<<' '<<l.defaultState<<' '<<l.states.size();
      for(const auto &s:l.states) {
        o<<' '<<s.id<<' '<<std::quoted(s.name)<<' '<<static_cast<u32>(s.kind)<<' '<<s.blendX<<' '<<s.blendY<<' '<<s.speed<<' '<<s.speedParameter
         <<' '<<s.loop<<' '<<s.x<<' '<<s.y<<' '<<s.motions.size();
        for(const auto &m:s.motions) o<<' '<<guid(m.clip)<<' '<<m.threshold<<' '<<m.x<<' '<<m.y;
        o<<' '<<s.events.size();
        for(const auto &e:s.events) o<<' '<<e.time<<' '<<e.tag;
      }
      o<<' '<<l.transitions.size();
      for(const auto &t:l.transitions) {
        o<<' '<<t.id<<' '<<t.from<<' '<<t.to<<' '<<t.hasExitTime<<' '<<t.exitTime<<' '<<t.duration<<' '<<t.conditions.size();
        for(const auto &c:t.conditions) o<<' '<<c.parameter<<' '<<static_cast<u32>(c.mode)<<' '<<c.threshold;
      }
    }
  }
  bool read(std::istream &i,u32 version) override {
    if(version!=1) return false;
    parameters.clear();layers.clear();
    usize count=0;
    if(!(i>>enabled>>unscaledTime>>target>>speed>>nextId>>count)||count>MaximumParameters) return false;
    parameters.resize(count);
    for(auto &p:parameters) {u32 type=0;if(!(i>>p.id>>std::quoted(p.name)>>type>>p.value)) return false;p.type=static_cast<AnimatorParameterType>(type);}
    if(!(i>>count)||count>MaximumLayers) return false;
    layers.resize(count);
    for(auto &l:layers) {
      usize states=0;
      if(!(i>>l.id>>std::quoted(l.name)>>l.weight>>l.mask>>l.defaultState>>states)||states>MaximumStates) return false;
      l.states.resize(states);
      for(auto &s:l.states) {
        u32 kind=0;usize motions=0,events=0;
        if(!(i>>s.id>>std::quoted(s.name)>>kind>>s.blendX>>s.blendY>>s.speed>>s.speedParameter>>s.loop>>s.x>>s.y>>motions)||motions>MaximumMotions) return false;
        s.kind=static_cast<AnimatorMotionKind>(kind);s.motions.resize(motions);
        for(auto &m:s.motions) {
          std::string text;if(!(i>>text>>m.threshold>>m.x>>m.y)) return false;
          m.clip={};if(text!="-"&&!resources::AssetGuid::parse(text,m.clip)) return false;
        }
        if(!(i>>events)||events>MaximumEvents) return false;
        s.events.resize(events);
        for(auto &e:s.events) if(!(i>>e.time>>e.tag)) return false;
      }
      usize transitions=0;
      if(!(i>>transitions)||transitions>MaximumTransitions) return false;
      l.transitions.resize(transitions);
      for(auto &t:l.transitions) {
        usize conditions=0;
        if(!(i>>t.id>>t.from>>t.to>>t.hasExitTime>>t.exitTime>>t.duration>>conditions)||conditions>MaximumConditions) return false;
        t.conditions.resize(conditions);
        for(auto &c:t.conditions) {u32 mode=0;if(!(i>>c.parameter>>mode>>c.threshold)) return false;c.mode=static_cast<AnimatorConditionMode>(mode);}
      }
    }
    return valid();
  }
};
inline const Animator &animator(const ComponentValue &v) {return static_cast<const Animator &>(v);}
inline Animator &animator(ComponentValue &v) {return static_cast<Animator &>(v);}

// Grafo novo: uma camada "Base" com um estado vazio "Parado" como padrão.
inline void initializeAnimator(Animator &a) {
  if(!a.layers.empty()) return;
  AnimatorLayer base;base.id=a.allocateId();base.name="Base";
  AnimatorState idle;idle.id=a.allocateId();idle.name="Parado";idle.x=0;idle.y=0;
  base.defaultState=idle.id;base.states.push_back(idle);a.layers.push_back(std::move(base));
}

inline constexpr std::array<ComponentNumber,1> animatorNumbers{{
  {"Velocidade",-10,10,.05f,[](const ComponentValue &v)->const float &{return animator(v).speed;},
   [](ComponentValue &v)->float *{return &animator(v).speed;},"speed",{"Animator","×","Multiplica o tempo de todos os estados",nullptr},true},
}};
inline constexpr std::array<ComponentBoolean,2> animatorBooleans{{
  {"enabled","Ativo",[](const ComponentValue &v){return animator(v).enabled;},[](ComponentValue &v,bool b){animator(v).enabled=b;},
   {"Animator","","Desligado congela a pose atual; parâmetros e estados são preservados"}},
  {"unscaled_time","Ignorar escala de tempo",[](const ComponentValue &v){return animator(v).unscaledTime;},[](ComponentValue &v,bool b){animator(v).unscaledTime=b;},
   {"Animator","","Anima em tempo real mesmo com o jogo pausado por escala (menus, cutscenes)"}},
}};
inline constexpr std::array<ComponentObjectReference,1> animatorReferences{{
  {"target","Raiz animada","",ObjectReferenceScope::Any,"Este objeto",
   [](const ComponentValue &v){return animator(v).target;},[](ComponentValue &v,u64 x){animator(v).target=x;},
   {"Animator","","Objeto cuja hierarquia os clipes animam (o modelo importado)"}},
}};
inline constexpr std::array<ComponentParameter,2> animatorStatePayload{{
  {"layer","Camada",ComponentValueKind::Integer},{"state","Estado",ComponentValueKind::Integer}}};
inline constexpr std::array<ComponentParameter,3> animatorEventPayload{{
  {"layer","Camada",ComponentValueKind::Integer},{"state","Estado",ComponentValueKind::Integer},{"tag","Marca",ComponentValueKind::Integer}}};
inline constexpr std::array<ComponentEvent,2> animatorEvents{{
  {"state_entered","Entrou no estado","Emitido quando um estado começa (no início da transição para ele)",animatorStatePayload},
  {"state_event","Evento do estado","Emitido quando o tempo do estado passa por um evento marcado nele",animatorEventPayload},
}};
inline constexpr std::array<ComponentMethod,1> animatorMethods{{
  {"in_transition","Em transição","Verdadeiro enquanto a camada base mistura dois estados",{},ComponentValueKind::Boolean},
}};
inline const ComponentType Animator::descriptor{
  "astra.animation.animator",1,[]()->std::unique_ptr<ComponentValue>{auto a=std::make_unique<Animator>();initializeAnimator(*a);return a;},
  animatorNumbers,animatorBooleans,{},nullptr,false,animatorReferences,{},{},{},{},{},animatorMethods,animatorEvents};

} // namespace ae::scene
