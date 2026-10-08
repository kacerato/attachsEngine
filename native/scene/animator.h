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
// Adaptações: grafo local ou recurso .aeanimator com overrides por instância;
// a máscara de camada é uma subárvore de objetos; composição por substituição
// ou aditiva relativa à pose inicial/clipe de referência; hierarquia por IDs.
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
enum class AnimatorLayerBlend : u32 {Override=0,Additive=1};
enum class AnimatorInterruption : u32 {None=0,Current=1,Next=2,CurrentThenNext=3,NextThenCurrent=4};
enum class AnimatorConditionMode : u32 {If=0,IfNot=1,Greater=2,Less=3,Equals=4,NotEqual=5};
// Manual parameters retain the v1 script contract. Bound values read the
// resolved physics state after simulation, never the requested input speed.
enum class AnimatorParameterSource : u32 {Manual=0,PlanarSpeed=1,VerticalSpeed=2,Grounded=3,Speed=4};
inline const char *animatorSourceName(AnimatorParameterSource source) {
  static constexpr const char *names[]{"Script / manual","Velocidade no plano","Velocidade vertical","Apoio no chão","Velocidade total"};
  return names[static_cast<u32>(source)<=4?static_cast<u32>(source):0];
}
inline bool animatorSourceCompatible(AnimatorParameterType type,AnimatorParameterSource source) {
  return source==AnimatorParameterSource::Manual||(source==AnimatorParameterSource::Grounded?type==AnimatorParameterType::Bool:type==AnimatorParameterType::Float);
}

struct AnimatorParameter {
  u64 id=0;std::string name;AnimatorParameterType type=AnimatorParameterType::Float;float value=0;
  AnimatorParameterSource source=AnimatorParameterSource::Manual;
  float response=0,scale=1;
  bool operator==(const AnimatorParameter &) const=default;
};
struct AnimatorMotion {
  resources::AssetGuid clip{};float threshold=0,x=0,y=0;
  bool operator==(const AnimatorMotion &) const=default;
};
struct AnimatorClipOverride {
  resources::AssetGuid original{},replacement{};
  bool operator==(const AnimatorClipOverride &) const=default;
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
  u64 machine=0;                 // zero: root of this layer
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
  u64 machine=0;                // scope of Any State and Exit
  AnimatorInterruption interruption=AnimatorInterruption::None;
  bool orderedInterruption=true,canTransitionToSelf=false;
  float offset=0;bool fixedDuration=true;
  bool entry=false;              // from zero is Entry rather than Any State
  bool operator==(const AnimatorTransition &) const=default;
};
struct AnimatorMachine {
  u64 id=0,parent=0,defaultState=0;std::string name;float x=0,y=0;
  bool operator==(const AnimatorMachine &) const=default;
};
struct AnimatorLayer {
  u64 id=0;std::string name;float weight=1;u64 mask=0;u64 defaultState=0;
  AnimatorLayerBlend blend=AnimatorLayerBlend::Override;
  resources::AssetGuid referenceClip{};
  float referenceTime=0; // seconds; invalid GUID uses initial object pose
  std::vector<AnimatorState> states;std::vector<AnimatorTransition> transitions;
  std::vector<AnimatorMachine> machines;
  bool operator==(const AnimatorLayer &) const=default;
  const AnimatorState *state(u64 stateId) const {for(const auto &s:states) if(s.id==stateId) return &s;return nullptr;}
  AnimatorState *state(u64 stateId) {for(auto &s:states) if(s.id==stateId) return &s;return nullptr;}
  const AnimatorMachine *machine(u64 id) const {for(const auto &m:machines) if(m.id==id) return &m;return nullptr;}
  AnimatorMachine *machine(u64 id) {for(auto &m:machines) if(m.id==id) return &m;return nullptr;}
  bool node(u64 id) const {return state(id)||machine(id);}
  u64 parent(u64 id) const {if(const auto *s=state(id)) return s->machine;if(const auto *m=machine(id)) return m->parent;return 0;}
  bool contains(u64 scope,u64 id) const {
    if(!node(id)) return false;
    for(usize depth=0;depth<=machines.size();++depth) {
      if(id==scope) return true;
      id=parent(id);if(!id) return scope==0;
    }
    return false;
  }
  u64 entry(u64 id) const {
    if(!id) id=defaultState;
    for(usize depth=0;depth<=machines.size();++depth) {
      if(state(id)) return id;
      const auto *m=machine(id);if(!m) return 0;id=m->defaultState;
    }
    return 0;
  }
  std::string path(u64 id) const {
    std::string result;
    for(usize depth=0;id&&depth<=machines.size();++depth) {
      const auto *s=state(id);const auto *m=machine(id);if(!s&&!m) return {};
      result=(s?s->name:m->name)+(result.empty()?"":"/"+result);id=parent(id);
    }
    return id?std::string{}:result;
  }
  // Full paths are authoritative; a short name works only when unambiguous.
  u64 findPath(std::string_view name) const {
    u64 exact=0,shortName=0;usize shortCount=0;
    const auto consider=[&](u64 id,const std::string &label) {
      if(path(id)==name) exact=id;
      if(label==name) {shortName=id;++shortCount;}
    };
    for(const auto &s:states) consider(s.id,s.name);
    for(const auto &m:machines) consider(m.id,m.name);
    return exact?exact:shortCount==1?shortName:0;
  }
  u64 defaultOf(u64 scope) const {const auto *m=machine(scope);return scope?(m?m->defaultState:0):defaultState;}
  bool setDefault(u64 scope,u64 id) {
    if(id&&(!node(id)||parent(id)!=scope)) return false;
    if(!scope) defaultState=id;else if(auto *m=machine(scope)) m->defaultState=id;else return false;
    return true;
  }
  void repairDefaults() {
    const auto repair=[&](u64 scope,u64 &id) {
      if(id&&node(id)&&parent(id)==scope) return;
      id=0;for(const auto &s:states) if(s.machine==scope) {id=s.id;return;}
      for(const auto &m:machines) if(m.parent==scope) {id=m.id;return;}
    };
    repair(0,defaultState);for(auto &m:machines) repair(m.id,m.defaultState);
  }
  bool move(u64 id,u64 scope) {
    if(!node(id)||(scope&&!machine(scope))||contains(id,scope)) return false;
    const auto *s=state(id);const std::string name=s?s->name:machine(id)->name;
    for(const auto &n:states) if(n.id!=id&&n.machine==scope&&n.name==name) return false;
    for(const auto &n:machines) if(n.id!=id&&n.parent==scope&&n.name==name) return false;
    if(auto *n=state(id)) n->machine=scope;else machine(id)->parent=scope;
    for(auto &t:transitions) if(t.from==id||(t.entry&&t.to==id)) t.machine=scope;
    repairDefaults();return true;
  }
  void eraseNode(u64 id) {
    std::vector<u64> doomed;
    for(const auto &s:states) if(contains(id,s.id)) doomed.push_back(s.id);
    for(const auto &m:machines) if(contains(id,m.id)) doomed.push_back(m.id);
    const auto removed=[&](u64 n){return std::find(doomed.begin(),doomed.end(),n)!=doomed.end();};
    std::erase_if(states,[&](const auto &s){return removed(s.id);});
    std::erase_if(machines,[&](const auto &m){return removed(m.id);});
    std::erase_if(transitions,[&](const auto &t){return removed(t.from)||removed(t.to)||removed(t.machine);});
    repairDefaults();
  }
};

class Animator final : public ComponentValue {
public:
  static constexpr usize MaximumParameters=32,MaximumLayers=4,MaximumStates=24,MaximumTransitions=48,
                         MaximumMotions=8,MaximumConditions=4,MaximumEvents=8,MaximumName=63,MaximumMachines=32,MaximumDepth=16;
  bool enabled=true,unscaledTime=false;
  u64 target=0;   // raiz animada; zero: este objeto
  u64 motionSource=0; // physics owner; zero: this object, independent of the visual root
  float speed=1;
  std::vector<AnimatorParameter> parameters;
  std::vector<AnimatorLayer> layers;
  u64 nextId=1;
  resources::AssetGuid controller{};
  std::vector<AnimatorClipOverride> clipOverrides;
  static constexpr usize MaximumOverrides=MaximumLayers*MaximumStates*MaximumMotions;

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
    if(clipOverrides.size()>MaximumOverrides||(!controller.valid()&&!clipOverrides.empty())) return false;
    for(usize k=0;k<clipOverrides.size();++k) {
      const auto &o=clipOverrides[k];
      if(!o.original.valid()||!o.replacement.valid()||o.original==o.replacement) return false;
      for(usize j=0;j<k;++j) if(clipOverrides[j].original==o.original) return false;
    }
    if(!std::isfinite(speed)||speed<-10||speed>10||target>std::numeric_limits<u32>::max()||motionSource>std::numeric_limits<u32>::max()||
       parameters.size()>MaximumParameters||layers.size()>MaximumLayers||!nextId) return false;
    std::vector<u64> ids;
    const auto fresh=[&](u64 id){if(!id||id>=nextId||std::find(ids.begin(),ids.end(),id)!=ids.end()) return false;ids.push_back(id);return true;};
    for(usize i=0;i<parameters.size();++i) {
      const auto &p=parameters[i];
      if(!fresh(p.id)||!validName(p.name)||static_cast<u32>(p.type)>3||!std::isfinite(p.value)) return false;
      if(static_cast<u32>(p.source)>4||!animatorSourceCompatible(p.type,p.source)||!std::isfinite(p.response)||p.response<0||p.response>10||
         !std::isfinite(p.scale)||p.scale<-100||p.scale>100) return false;
      for(usize j=0;j<i;++j) if(parameters[j].name==p.name) return false;
    }
    const auto floatParameter=[&](u64 id){const auto *p=parameter(id);return id==0||(p&&(p->type==AnimatorParameterType::Float||p->type==AnimatorParameterType::Int));};
    for(const auto &l:layers) {
      if(!fresh(l.id)||!validName(l.name)||!std::isfinite(l.weight)||l.weight<0||l.weight>1||l.mask>std::numeric_limits<u32>::max()||
         static_cast<u32>(l.blend)>1||!std::isfinite(l.referenceTime)||l.referenceTime<0||l.referenceTime>86400||
         l.states.size()>MaximumStates||l.transitions.size()>MaximumTransitions||l.machines.size()>MaximumMachines) return false;
      if(l.defaultState&&(!l.node(l.defaultState)||l.parent(l.defaultState))) return false;
      for(const auto &m:l.machines) {
        if(!fresh(m.id)||!validName(m.name)||m.name.find('/')!=std::string::npos||!std::isfinite(m.x)||!std::isfinite(m.y)||
           (m.parent&&!l.machine(m.parent))||(m.defaultState&&(!l.node(m.defaultState)||l.parent(m.defaultState)!=m.id))) return false;
        u64 ancestor=m.id;usize depth=0;
        while(ancestor&&depth++<=MaximumDepth) ancestor=l.parent(ancestor);
        if(ancestor||depth>MaximumDepth) return false;
        for(const auto &other:l.machines) if(other.id!=m.id&&other.parent==m.parent&&other.name==m.name) return false;
      }
      for(const auto &s:l.states) {
        if(!fresh(s.id)||!validName(s.name)||static_cast<u32>(s.kind)>2||s.motions.size()>MaximumMotions||s.events.size()>MaximumEvents||
           !std::isfinite(s.speed)||s.speed<-10||s.speed>10||!std::isfinite(s.x)||!std::isfinite(s.y)||
           !floatParameter(s.blendX)||!floatParameter(s.blendY)||!floatParameter(s.speedParameter)||
           (s.machine&&!l.machine(s.machine))||(!l.machines.empty()&&s.name.find('/')!=std::string::npos)) return false;
        if(!l.machines.empty()) {
          for(const auto &other:l.states) if(other.id!=s.id&&other.machine==s.machine&&other.name==s.name) return false;
          for(const auto &other:l.machines) if(other.parent==s.machine&&other.name==s.name) return false;
        }
        for(const auto &m:s.motions) if(!std::isfinite(m.threshold)||!std::isfinite(m.x)||!std::isfinite(m.y)) return false;
        for(const auto &e:s.events) if(!std::isfinite(e.time)||e.time<0||e.time>1) return false;
      }
      for(const auto &t:l.transitions) {
        if(!fresh(t.id)||(t.from&&!l.node(t.from))||(t.to&&!l.node(t.to))||(!t.to&&!t.machine)||
           (t.machine&&!l.machine(t.machine))||!std::isfinite(t.exitTime)||t.exitTime<0||t.exitTime>100||
           !std::isfinite(t.duration)||t.duration<0||t.duration>60||t.conditions.size()>MaximumConditions||
           static_cast<u32>(t.interruption)>4||!std::isfinite(t.offset)||t.offset<0||t.offset>100||
           ((t.entry||l.machine(t.from))&&(t.hasExitTime||t.duration!=0||t.offset!=0||
             t.interruption!=AnimatorInterruption::None||!t.orderedInterruption||t.canTransitionToSelf||!t.fixedDuration))) return false;
        if((t.entry&&(t.from||!t.to||l.parent(t.to)!=t.machine))||(t.from&&t.machine!=l.parent(t.from))) return false;
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
    o<<enabled<<' '<<unscaledTime<<' '<<target<<' '<<motionSource<<' '<<speed<<' '<<nextId<<' '<<parameters.size();
    for(const auto &p:parameters) o<<' '<<p.id<<' '<<std::quoted(p.name)<<' '<<static_cast<u32>(p.type)<<' '<<p.value<<' '<<static_cast<u32>(p.source)<<' '<<p.response<<' '<<p.scale;
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
    o<<' '<<guid(controller)<<' '<<clipOverrides.size();
    for(const auto &entry:clipOverrides) o<<' '<<guid(entry.original)<<' '<<guid(entry.replacement);
    o<<' '<<layers.size();
    for(const auto &l:layers) o<<' '<<l.id<<' '<<static_cast<u32>(l.blend)<<' '<<guid(l.referenceClip)<<' '<<l.referenceTime;
    o<<' '<<layers.size();
    for(const auto &l:layers) {
      o<<' '<<l.id<<' '<<l.machines.size();
      for(const auto &m:l.machines) o<<' '<<m.id<<' '<<m.parent<<' '<<m.defaultState<<' '<<std::quoted(m.name)<<' '<<m.x<<' '<<m.y;
      o<<' '<<l.states.size();for(const auto &s:l.states) o<<' '<<s.id<<' '<<s.machine;
      o<<' '<<l.transitions.size();for(const auto &t:l.transitions)
        o<<' '<<t.id<<' '<<t.machine<<' '<<static_cast<u32>(t.interruption)<<' '<<t.orderedInterruption<<' '<<t.canTransitionToSelf<<' '<<t.offset<<' '<<t.fixedDuration<<' '<<t.entry;
    }
  }
  bool read(std::istream &i,u32 version) override {
    if(version<1||version>5) return false;
    controller={};clipOverrides.clear();
    parameters.clear();layers.clear();
    usize count=0;
    motionSource=0;
    if(!(i>>enabled>>unscaledTime>>target)) return false;
    if(version>=2&&!(i>>motionSource)) return false;
    if(!(i>>speed>>nextId>>count)||count>MaximumParameters) return false;
    parameters.resize(count);
    for(auto &p:parameters) {u32 type=0;if(!(i>>p.id>>std::quoted(p.name)>>type>>p.value)) return false;p.type=static_cast<AnimatorParameterType>(type);
      if(version>=2) {u32 source=0;if(!(i>>source>>p.response>>p.scale)) return false;p.source=static_cast<AnimatorParameterSource>(source);}}
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
    if(version>=3) {
      std::string text;usize overrides=0;
      if(!(i>>text>>overrides)||overrides>MaximumOverrides) return false;
      if(text!="-"&&!resources::AssetGuid::parse(text,controller)) return false;
      clipOverrides.resize(overrides);
      for(auto &entry:clipOverrides) {
        std::string original,replacement;
        if(!(i>>original>>replacement)||!resources::AssetGuid::parse(original,entry.original)||
           !resources::AssetGuid::parse(replacement,entry.replacement)) return false;
      }
    }
    if(version>=4) {
      usize n=0;if(!(i>>n)||n!=layers.size()) return false;
      for(auto &l:layers) {
        u64 id=0;u32 mode=0;std::string ref;
        if(!(i>>id>>mode>>ref>>l.referenceTime)||id!=l.id) return false;
        l.blend=static_cast<AnimatorLayerBlend>(mode);
        if(ref!="-"&&!resources::AssetGuid::parse(ref,l.referenceClip)) return false;
      }
    }
    if(version>=5) {
      usize n=0;if(!(i>>n)||n!=layers.size()) return false;
      for(auto &l:layers) {
        u64 id=0;usize count=0;
        if(!(i>>id>>count)||id!=l.id||count>MaximumMachines) return false;
        l.machines.resize(count);
        for(auto &m:l.machines) if(!(i>>m.id>>m.parent>>m.defaultState>>std::quoted(m.name)>>m.x>>m.y)) return false;
        if(!(i>>count)||count!=l.states.size()) return false;
        for(auto &s:l.states) if(!(i>>id>>s.machine)||id!=s.id) return false;
        if(!(i>>count)||count!=l.transitions.size()) return false;
        for(auto &t:l.transitions) {
          u32 mode=0;if(!(i>>id>>t.machine>>mode>>t.orderedInterruption>>t.canTransitionToSelf>>t.offset>>t.fixedDuration>>t.entry)||id!=t.id) return false;
          t.interruption=static_cast<AnimatorInterruption>(mode);
        }
      }
    }
    // Versions 1-3 always evaluated the base layer at full weight.
    if(version<4&&!layers.empty()) layers[0].weight=1;
    return valid();
  }
};
inline const Animator &animator(const ComponentValue &v) {return static_cast<const Animator &>(v);}
inline Animator &animator(ComponentValue &v) {return static_cast<Animator &>(v);}
// Replacing authoring values must never replace the collection's component ID.
inline void replaceAnimatorData(Animator &destination,const Animator &source) {
  destination.enabled=source.enabled;destination.unscaledTime=source.unscaledTime;destination.target=source.target;
  destination.motionSource=source.motionSource;destination.speed=source.speed;destination.nextId=source.nextId;
  destination.parameters=source.parameters;destination.layers=source.layers;destination.controller=source.controller;
  destination.clipOverrides=source.clipOverrides;
}

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
template<usize Index> constexpr ComponentObjectReference animatorMaskReference(const char *id,const char *name) {
  return {id,name,"",ObjectReferenceScope::Any,"Toda a hierarquia",
    [](const ComponentValue &v)->u64 {const auto &a=animator(v);return Index<a.layers.size()?a.layers[Index].mask:0;},
    [](ComponentValue &v,u64 value){auto &a=animator(v);if(Index<a.layers.size()) a.layers[Index].mask=value;},
    {"Máscaras","","Máscara da camada por instância; remapeada em hierarquia/prefab",
      [](const ComponentValue &v){return Index<animator(v).layers.size();},
      [](const ComponentValue &v){return Index<animator(v).layers.size();}}};
}
inline constexpr std::array<ComponentObjectReference,6> animatorReferences{{
  {"target","Raiz animada","",ObjectReferenceScope::Any,"Este objeto",
   [](const ComponentValue &v){return animator(v).target;},[](ComponentValue &v,u64 x){animator(v).target=x;},
   {"Animator","","Objeto cuja hierarquia os clipes animam (o modelo importado)"}},
  {"motion_source","Corpo / motor","",ObjectReferenceScope::Any,"Este objeto",
   [](const ComponentValue &v){return animator(v).motionSource;},[](ComponentValue &v,u64 x){animator(v).motionSource=x;},
   {"Animator","","Fonte dos parâmetros físicos; independe da malha e não move o corpo"}},
  animatorMaskReference<0>("layer_mask_0","Máscara · Base"),animatorMaskReference<1>("layer_mask_1","Máscara · Camada 2"),
  animatorMaskReference<2>("layer_mask_2","Máscara · Camada 3"),animatorMaskReference<3>("layer_mask_3","Máscara · Camada 4"),
}};
inline constexpr std::array<ComponentParameter,2> animatorStatePayload{{
  {"layer","Camada",ComponentValueKind::Integer},{"state","Estado",ComponentValueKind::Integer}}};
inline constexpr std::array<ComponentParameter,3> animatorEventPayload{{
  {"layer","Camada",ComponentValueKind::Integer},{"state","Estado",ComponentValueKind::Integer},{"tag","Marca",ComponentValueKind::Integer}}};
inline constexpr std::array<ComponentParameter,2> animatorMachinePayload{{
  {"layer","Camada",ComponentValueKind::Integer},{"machine","Grupo",ComponentValueKind::Integer}}};
inline constexpr std::array<ComponentParameter,3> animatorInterruptionPayload{{
  {"layer","Camada",ComponentValueKind::Integer},{"transition","Transição cancelada",ComponentValueKind::Integer},{"destination","Novo estado",ComponentValueKind::Integer}}};
inline constexpr std::array<ComponentEvent,5> animatorEvents{{
  {"state_entered","Entrou no estado","Emitido quando um estado começa (no início da transição para ele)",animatorStatePayload},
  {"state_event","Evento do estado","Emitido quando o tempo do estado passa por um evento marcado nele",animatorEventPayload},
  {"machine_entered","Entrou no grupo","Um grupo tornou-se ativo; da raiz até o grupo interno",animatorMachinePayload},
  {"machine_exited","Saiu do grupo","Um grupo deixou de participar da reprodução; do grupo interno à raiz",animatorMachinePayload},
  {"transition_interrupted","Mistura interrompida","Pose composta preservada; ID zero indica CrossFade solicitado pela API",animatorInterruptionPayload},
}};
inline constexpr std::array<ComponentParameter,1> animatorLayerArgument{{{"layer","Camada",ComponentValueKind::Integer}}};
inline constexpr std::array<ComponentParameter,2> animatorLayerNumberArguments{{{"layer","Camada",ComponentValueKind::Integer},{"value","Valor",ComponentValueKind::Number}}};
inline constexpr std::array<ComponentParameter,2> animatorLayerBlendArguments{{{"layer","Camada",ComponentValueKind::Integer},{"mode","0 Override / 1 Additive",ComponentValueKind::Integer}}};
inline constexpr std::array<ComponentParameter,3> animatorLayerReferenceArguments{{{"layer","Camada",ComponentValueKind::Integer},{"high","GUID alto",ComponentValueKind::Integer},{"low","GUID baixo",ComponentValueKind::Integer}}};
inline constexpr std::array<ComponentMethod,11> animatorMethods{{
  {"in_transition","Em transição","Verdadeiro enquanto a camada base mistura dois estados",{},ComponentValueKind::Boolean},
  {"get_layer_weight","Ler peso","Peso efetivo da instância",animatorLayerArgument,ComponentValueKind::Number},
  {"set_layer_weight","Peso da instância","Não altera o recurso compartilhado",animatorLayerNumberArguments},
  {"get_layer_blend","Ler composição","0 Override / 1 Additive",animatorLayerArgument,ComponentValueKind::Integer},
  {"set_layer_blend","Composição da instância","0 Override / 1 Additive",animatorLayerBlendArguments},
  {"get_layer_reference_time","Ler tempo de referência","Segundos no clipe",animatorLayerArgument,ComponentValueKind::Number},
  {"set_layer_reference_time","Tempo de referência","Segundos no clipe",animatorLayerNumberArguments},
  {"set_layer_reference","Referência da instância","GUID zero usa a pose inicial; desconhecido é recusado",animatorLayerReferenceArguments},
  {"get_layer_reference_high","GUID alto","Bits da referência efetiva",animatorLayerArgument,ComponentValueKind::Integer},
  {"get_layer_reference_low","GUID baixo","Bits da referência efetiva",animatorLayerArgument,ComponentValueKind::Integer},
  {"reset_layer_overrides","Restaurar camada","Restaura a composição autorada, preservando estado e relógio",animatorLayerArgument},
}};
inline constexpr std::array<ComponentResourceBinding,1> animatorResources{{
  {"controller","Controller",resources::AssetType::AnimatorController,
   [](const ComponentValue &){return 1u;},
   [](const ComponentValue &v,u32){return animator(v).controller;},
   [](ComponentValue &v,u32 slot,resources::AssetGuid value){
     if(slot) return false;
     auto &a=animator(v);if(a.controller!=value) a.clipOverrides.clear();a.controller=value;return true;
   },{"Animator","","Grafo compartilhado; ausente interrompe a avaliação e expõe diagnóstico"}},
}};
inline const ComponentType Animator::descriptor{
  "astra.animation.animator",5,[]()->std::unique_ptr<ComponentValue>{auto a=std::make_unique<Animator>();initializeAnimator(*a);return a;},
  animatorNumbers,animatorBooleans,{},nullptr,false,animatorReferences,{},animatorResources,{},{},{},animatorMethods,animatorEvents};

} // namespace ae::scene
