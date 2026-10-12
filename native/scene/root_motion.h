#pragma once
// Root motion (N05): o deslocamento que a animacao da ao osso raiz vira
// movimento do personagem. Fica no objeto que anda (o mesmo do Animator ou da
// Animacao, ou o dono fisico); o osso volta ao lugar para nao andar duas vezes.
//
// Referencias estudadas: Unity 6 Root Motion
// (docs.unity3d.com/6000.0/Documentation/Manual/RootMotion.html) e Godot 4.5
// AnimationMixer.root_motion_track
// (docs.godotengine.org/en/4.5/classes/class_animationmixer.html). O caminho de
// aplicacao e escolhido pelo que o objeto ja tem: Personagem, corpo fisico
// dinamico ou transformacao livre.
#include "scene/ik_constraints.h"

namespace ae::scene {
class RootMotion final : public ComponentValue {
public:
  u64 bone=0;bool enabled=true,horizontal=true,vertical=false,rotation=true,keepBoneInPlace=true;
  float weight=1,loopJump=.5f,loopTurn=45;u32 axis=2;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<RootMotion>(*this);}
  bool valid() const override {return bone<=std::numeric_limits<u32>::max()&&axis<=5&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {
    out<<bone<<' '<<enabled<<' '<<horizontal<<' '<<vertical<<' '<<rotation<<' '<<keepBoneInPlace<<' '<<weight<<' '<<loopJump<<' '<<loopTurn<<' '<<axis;
  }
  bool read(std::istream &in,u32 version) override {
    return version==1&&(in>>bone>>enabled>>horizontal>>vertical>>rotation>>keepBoneInPlace>>weight>>loopJump>>loopTurn>>axis)&&valid();
  }
};

#define AE_RM_NUM(id,label,member,lo,hi,step,group,unit) ComponentNumber{label,lo,hi,step,[](const ComponentValue&v)->const float&{return static_cast<const RootMotion&>(v).member;},[](ComponentValue&v){return &static_cast<RootMotion&>(v).member;},id,{group,unit}}
#define AE_RM_BOOL(id,label,member,group) ComponentBoolean{id,label,[](const ComponentValue&v){return static_cast<const RootMotion&>(v).member;},[](ComponentValue&v,bool n){static_cast<RootMotion&>(v).member=n;},{group}}
inline constexpr std::array<ComponentNumber,3> rootMotionNumbers{{AE_RM_NUM("weight","Peso",weight,0,1,.01f,"Movimento",""),
  AE_RM_NUM("loop_jump","Salto de laço",loopJump,.01f,10,.01f,"Laço","m"),AE_RM_NUM("loop_turn","Giro de laço",loopTurn,1,180,1,"Laço","graus")}};
inline constexpr std::array<ComponentBoolean,5> rootMotionBooleans{{AE_RM_BOOL("enabled","Ativo",enabled,"Movimento"),
  AE_RM_BOOL("horizontal","Andar no plano",horizontal,"Movimento"),AE_RM_BOOL("vertical","Subir e descer",vertical,"Movimento"),
  AE_RM_BOOL("rotation","Girar",rotation,"Movimento"),AE_RM_BOOL("keep_bone_in_place","Osso fica no lugar",keepBoneInPlace,"Osso")}};
inline constexpr std::array<ComponentEnum,1> rootMotionEnums{{ComponentEnum{"axis","Frente do osso",ikAxisOptions,[](const ComponentValue &v){return static_cast<const RootMotion&>(v).axis;},[](ComponentValue &v,u32 n){static_cast<RootMotion&>(v).axis=n;},{"Osso"}}}};
inline constexpr std::array<ComponentObjectReference,1> rootMotionReferences{{ComponentObjectReference{"bone","Osso raiz","",ObjectReferenceScope::Descendant,"Escolher osso",
  [](const ComponentValue &v){return static_cast<const RootMotion&>(v).bone;},[](ComponentValue &v,u64 n){static_cast<RootMotion&>(v).bone=n;},{"Osso"}}}};
inline const ComponentType RootMotion::descriptor{"astra.animation.root_motion",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<RootMotion>();},
  rootMotionNumbers,rootMotionBooleans,rootMotionEnums,nullptr,false,rootMotionReferences};
#undef AE_RM_NUM
#undef AE_RM_BOOL
} // namespace ae::scene
