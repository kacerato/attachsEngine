#pragma once
#include "scene/components.h"
#include <array>
#include <cmath>
#include <limits>

namespace ae::scene {
// IK por componente (N04). Fica no objeto da PONTA da cadeia (mao, pe, pinca);
// os ossos acima sao resolvidos por runtime/scene_ik.h depois da animacao e
// das constraints de transformacao. Autoria persistida; estado de execucao
// (pose de entrada, residuo) pertence exclusivamente ao runtime.
//
// Referencias: Unity Animation Rigging 1.2 Two Bone IK / Chain IK
// (docs.unity3d.com/Packages/com.unity.animation.rigging@1.2), Godot 4.5
// SkeletonModifier3D e FinalIK Grounder como estudo de apoio de pe.
template<class T> inline bool ikNumbersValid(const T &v) {
  for(const auto &p:T::descriptor.numbers) {const float n=p.read(v);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}
  return true;
}

class TwoBoneIk final : public ComponentValue {
public:
  u64 target=0,pole=0;bool enabled=true,matchTargetRotation=false;float weight=1;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<TwoBoneIk>(*this);}
  bool valid() const override {return target<=std::numeric_limits<u32>::max()&&pole<=std::numeric_limits<u32>::max()&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<target<<' '<<pole<<' '<<enabled<<' '<<matchTargetRotation<<' '<<weight;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>target>>pole>>enabled>>matchTargetRotation>>weight)&&valid();}
};

enum class ChainIkSolverKind : u32 { Ccd=0, Fabrik=1 };
class ChainIk final : public ComponentValue {
public:
  u64 target=0;bool enabled=true;float weight=1,chainLength=3,iterations=12,tolerance=.001f;u32 solver=1;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<ChainIk>(*this);}
  bool valid() const override {return target<=std::numeric_limits<u32>::max()&&solver<=1&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<target<<' '<<enabled<<' '<<weight<<' '<<chainLength<<' '<<iterations<<' '<<tolerance<<' '<<solver;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>target>>enabled>>weight>>chainLength>>iterations>>tolerance>>solver)&&valid();}
};

// Apoio do pe no chao: raio para baixo a partir do pe, pela fisica real.
// Move a ponta ate a superficie (limitado) e resolve a perna com dois ossos;
// sem chao dentro do alcance, a pose animada fica intacta (fase aerea).
class FootGrounding final : public ComponentValue {
public:
  bool enabled=true,alignToSurface=true;float weight=1,rayHeight=.5f,rayDistance=1,footOffset=0,maxCorrection=.4f;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<FootGrounding>(*this);}
  bool valid() const override {return ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<alignToSurface<<' '<<weight<<' '<<rayHeight<<' '<<rayDistance<<' '<<footOffset<<' '<<maxCorrection;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>enabled>>alignToSurface>>weight>>rayHeight>>rayDistance>>footOffset>>maxCorrection)&&valid();}
};

#define AE_IK_NUM(T,id,label,member,lo,hi,step,group,unit) ComponentNumber{label,lo,hi,step,[](const ComponentValue&v)->const float&{return static_cast<const T&>(v).member;},[](ComponentValue&v){return &static_cast<T&>(v).member;},id,{group,unit}}
#define AE_IK_BOOL(T,id,label,member,group) ComponentBoolean{id,label,[](const ComponentValue&v){return static_cast<const T&>(v).member;},[](ComponentValue&v,bool n){static_cast<T&>(v).member=n;},{group}}

inline constexpr std::array<ComponentNumber,1> twoBoneIkNumbers{{AE_IK_NUM(TwoBoneIk,"weight","Peso",weight,0,1,.01f,"Influência","")}};
inline constexpr std::array<ComponentBoolean,2> twoBoneIkBooleans{{AE_IK_BOOL(TwoBoneIk,"enabled","Ativo",enabled,"Influência"),
  AE_IK_BOOL(TwoBoneIk,"match_target_rotation","Ponta segue rotação do alvo",matchTargetRotation,"Alvo")}};
inline constexpr std::array<ComponentObjectReference,2> twoBoneIkReferences{{
  {"target","Alvo","",ObjectReferenceScope::OtherNonDescendant,"Escolher alvo",[](const ComponentValue &v){return static_cast<const TwoBoneIk&>(v).target;},[](ComponentValue &v,u64 n){static_cast<TwoBoneIk&>(v).target=n;},{"Alvo"}},
  {"pole","Direção da dobra","",ObjectReferenceScope::OtherNonDescendant,"Seguir a pose",[](const ComponentValue &v){return static_cast<const TwoBoneIk&>(v).pole;},[](ComponentValue &v,u64 n){static_cast<TwoBoneIk&>(v).pole=n;},{"Alvo"}}}};
inline const ComponentType TwoBoneIk::descriptor{"astra.ik.two_bone",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<TwoBoneIk>();},twoBoneIkNumbers,twoBoneIkBooleans,{},nullptr,false,twoBoneIkReferences};

inline constexpr std::array<ComponentNumber,4> chainIkNumbers{{
  AE_IK_NUM(ChainIk,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_IK_NUM(ChainIk,"chain_length","Ossos na cadeia",chainLength,2,32,1,"Cadeia","ossos"),
  AE_IK_NUM(ChainIk,"iterations","Iterações máximas",iterations,1,128,1,"Solver",""),
  AE_IK_NUM(ChainIk,"tolerance","Tolerância",tolerance,.00001f,1,.0005f,"Solver","m")}};
inline constexpr std::array<ComponentBoolean,1> chainIkBooleans{{AE_IK_BOOL(ChainIk,"enabled","Ativo",enabled,"Influência")}};
inline constexpr std::array<ComponentEnumOption,2> chainIkSolverOptions{{{0,"CCD"},{1,"FABRIK"}}};
inline constexpr std::array<ComponentEnum,1> chainIkEnums{{{"solver","Solver",chainIkSolverOptions,[](const ComponentValue &v){return static_cast<const ChainIk&>(v).solver;},[](ComponentValue &v,u32 n){static_cast<ChainIk&>(v).solver=n;},{"Solver"}}}};
inline constexpr std::array<ComponentObjectReference,1> chainIkReferences{{
  {"target","Alvo","",ObjectReferenceScope::OtherNonDescendant,"Escolher alvo",[](const ComponentValue &v){return static_cast<const ChainIk&>(v).target;},[](ComponentValue &v,u64 n){static_cast<ChainIk&>(v).target=n;},{"Alvo"}}}};
inline const ComponentType ChainIk::descriptor{"astra.ik.chain",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<ChainIk>();},chainIkNumbers,chainIkBooleans,chainIkEnums,nullptr,false,chainIkReferences};

inline constexpr std::array<ComponentNumber,5> footGroundingNumbers{{
  AE_IK_NUM(FootGrounding,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_IK_NUM(FootGrounding,"ray_height","Início do raio acima do pé",rayHeight,0,10,.01f,"Sondagem","m"),
  AE_IK_NUM(FootGrounding,"ray_distance","Alcance abaixo do pé",rayDistance,.01f,20,.01f,"Sondagem","m"),
  AE_IK_NUM(FootGrounding,"foot_offset","Altura do pé sobre a superfície",footOffset,-1,1,.005f,"Apoio","m"),
  AE_IK_NUM(FootGrounding,"max_correction","Correção máxima",maxCorrection,0,5,.01f,"Apoio","m")}};
inline constexpr std::array<ComponentBoolean,2> footGroundingBooleans{{AE_IK_BOOL(FootGrounding,"enabled","Ativo",enabled,"Influência"),
  AE_IK_BOOL(FootGrounding,"align_to_surface","Alinhar à superfície",alignToSurface,"Apoio")}};
inline const ComponentType FootGrounding::descriptor{"astra.ik.foot_grounding",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<FootGrounding>();},footGroundingNumbers,footGroundingBooleans,{},nullptr,false,{}};
#undef AE_IK_NUM
#undef AE_IK_BOOL
} // namespace ae::scene
