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

// Solver: Pose (plano da dobra da pose atual ou do alvo de direcao) ou Plano
// rotativo (rotate-plane, como o IK do UMotion Pro 1.29 e o RP solver do Maya):
// a cadeia parte da Referencia (animacao ou repouso), a dobra fica no plano
// definido pelo Angulo do plano em torno do eixo raiz->ponta, a Rotacao do polo
// gira a cadeia resolvida em torno do eixo raiz->alvo, e o Peso mistura cada
// osso da animacao para o IK por rotacao.
class TwoBoneIk final : public ComponentValue {
public:
  u64 target=0,pole=0;bool enabled=true,matchTargetRotation=false;float weight=1,priority=0;
  u32 solver=0,reference=0;float planeAngle=0,poleRotation=0;
  // Zero do Angulo do plano no espaco do osso raiz da cadeia; (0,0,0) usa a
  // perpendicular do osso do meio no repouso (regra do UMotion).
  float planeZero[3]{0,0,0};
  // Desvio da rotacao do alvo (graus XYZ, no espaco do alvo) quando a ponta
  // segue a rotacao do alvo, como Maintain Target Offset da Unity.
  float targetOffset[3]{0,0,0};
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<TwoBoneIk>(*this);}
  bool valid() const override {return solver<=1&&reference<=1&&target<=std::numeric_limits<u32>::max()&&pole<=std::numeric_limits<u32>::max()&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<target<<' '<<pole<<' '<<enabled<<' '<<matchTargetRotation<<' '<<weight<<' '<<priority<<' '<<solver<<' '<<reference<<' '<<planeAngle<<' '<<poleRotation<<' '<<planeZero[0]<<' '<<planeZero[1]<<' '<<planeZero[2]<<' '<<targetOffset[0]<<' '<<targetOffset[1]<<' '<<targetOffset[2];}
  bool read(std::istream &in,u32 version) override {
    if(version<1||version>4||!(in>>target>>pole>>enabled>>matchTargetRotation>>weight))return false;
    priority=0;solver=reference=0;planeAngle=poleRotation=0;
    for(u32 i=0;i<3;++i)planeZero[i]=targetOffset[i]=0;
    if(version>=2&&!(in>>priority))return false;
    if(version>=3&&!(in>>solver>>reference>>planeAngle>>poleRotation))return false;
    if(version>=4&&!(in>>planeZero[0]>>planeZero[1]>>planeZero[2]>>targetOffset[0]>>targetOffset[1]>>targetOffset[2]))return false;
    return valid();
  }
};

// Solver 2, Plano rotativo: a cadeia inteira dobra no plano do Angulo do plano
// (CCD restrito ao plano, como o IK de cadeia longa do UMotion Pro 1.29).
enum class ChainIkSolverKind : u32 { Ccd=0, Fabrik=1, RotatePlane=2 };
class ChainIk final : public ComponentValue {
public:
  u64 target=0,pole=0;bool enabled=true,matchTargetRotation=false;float weight=1,chainLength=3,iterations=12,tolerance=.001f,priority=0;u32 solver=1,reference=0;
  float planeAngle=0,poleRotation=0,planeZero[3]{0,0,0},targetOffset[3]{0,0,0};
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<ChainIk>(*this);}
  bool valid() const override {return target<=std::numeric_limits<u32>::max()&&pole<=std::numeric_limits<u32>::max()&&solver<=2&&reference<=1&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<target<<' '<<enabled<<' '<<weight<<' '<<chainLength<<' '<<iterations<<' '<<tolerance<<' '<<solver<<' '<<priority<<' '<<pole<<' '<<matchTargetRotation<<' '<<reference<<' '<<planeAngle<<' '<<poleRotation<<' '<<planeZero[0]<<' '<<planeZero[1]<<' '<<planeZero[2]<<' '<<targetOffset[0]<<' '<<targetOffset[1]<<' '<<targetOffset[2];}
  bool read(std::istream &in,u32 version) override {
    if(version<1||version>4||!(in>>target>>enabled>>weight>>chainLength>>iterations>>tolerance>>solver))return false;
    priority=0;pole=0;matchTargetRotation=false;reference=0;planeAngle=poleRotation=0;
    for(u32 i=0;i<3;++i)planeZero[i]=targetOffset[i]=0;
    if(version>=2&&!(in>>priority))return false;
    if(version>=3&&!(in>>pole>>matchTargetRotation>>reference>>planeAngle>>poleRotation))return false;
    if(version>=4&&!(in>>planeZero[0]>>planeZero[1]>>planeZero[2]>>targetOffset[0]>>targetOffset[1]>>targetOffset[2]))return false;
    return valid();
  }
};

// Apoio do pe no chao: raio para baixo a partir do pe, pela fisica real.
// Move a ponta ate a superficie (limitado) e resolve a perna com dois ossos;
// sem chao dentro do alcance, a pose animada fica intacta (fase aerea).
class FootGrounding final : public ComponentValue {
public:
  // Trava (anti-deslize): enquanto o pe animado esta no apoio, fica no ponto
  // onde tocou; solta quando sobe acima de Altura de contato ou quando ficar
  // preso esticaria a perna alem da Correcao maxima.
  bool enabled=true,alignToSurface=true,lockFoot=false;float weight=1,rayHeight=.5f,rayDistance=1,footOffset=0,maxCorrection=.4f,lockHeight=.05f;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<FootGrounding>(*this);}
  bool valid() const override {return ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<alignToSurface<<' '<<weight<<' '<<rayHeight<<' '<<rayDistance<<' '<<footOffset<<' '<<maxCorrection<<' '<<lockFoot<<' '<<lockHeight;}
  bool read(std::istream &in,u32 version) override {
    if(version<1||version>2||!(in>>enabled>>alignToSurface>>weight>>rayHeight>>rayDistance>>footOffset>>maxCorrection))return false;
    lockFoot=false;lockHeight=.05f;
    return (version==1||static_cast<bool>(in>>lockFoot>>lockHeight))&&valid();
  }
};

// Os parametros de IK sao lidos pelo solver a cada quadro: animaveis.
#define AE_IK_NUM(T,id,label,member,lo,hi,step,group,unit) ComponentNumber{label,lo,hi,step,[](const ComponentValue&v)->const float&{return static_cast<const T&>(v).member;},[](ComponentValue&v){return &static_cast<T&>(v).member;},id,{group,unit},true}
#define AE_IK_BOOL(T,id,label,member,group) ComponentBoolean{id,label,[](const ComponentValue&v){return static_cast<const T&>(v).member;},[](ComponentValue&v,bool n){static_cast<T&>(v).member=n;},{group}}

inline bool twoBoneRotatePlane(const ComponentValue &v){return static_cast<const TwoBoneIk&>(v).solver==1;}
inline bool twoBoneFollowsTarget(const ComponentValue &v){return static_cast<const TwoBoneIk&>(v).matchTargetRotation;}
inline constexpr std::array<ComponentNumber,10> twoBoneIkNumbers{{AE_IK_NUM(TwoBoneIk,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_IK_NUM(TwoBoneIk,"priority","Prioridade",priority,-100,100,1,"Influência",""),
  ComponentNumber{"Ângulo do plano",-360,360,.5f,[](const ComponentValue&v)->const float&{return static_cast<const TwoBoneIk&>(v).planeAngle;},[](ComponentValue&v){return &static_cast<TwoBoneIk&>(v).planeAngle;},"plane_angle",{"Plano","°","Gira o plano da dobra em torno do eixo raiz→ponta; o zero é perpendicular ao osso do meio no repouso.",twoBoneRotatePlane},true},
  ComponentNumber{"Rotação do polo",-360,360,.5f,[](const ComponentValue&v)->const float&{return static_cast<const TwoBoneIk&>(v).poleRotation;},[](ComponentValue&v){return &static_cast<TwoBoneIk&>(v).poleRotation;},"pole_rotation",{"Plano","°","Gira a cadeia resolvida em torno do eixo raiz→alvo (sem Direção da dobra).",twoBoneRotatePlane},true},
  ComponentNumber{"Zero do plano X",-1,1,.01f,[](const ComponentValue&v)->const float&{return static_cast<const TwoBoneIk&>(v).planeZero[0];},[](ComponentValue&v){return &static_cast<TwoBoneIk&>(v).planeZero[0];},"plane_zero_x",{"Plano","","Direção do zero do ângulo no espaço do osso raiz; zerado usa a perpendicular do osso do meio.",twoBoneRotatePlane},false},
  ComponentNumber{"Zero do plano Y",-1,1,.01f,[](const ComponentValue&v)->const float&{return static_cast<const TwoBoneIk&>(v).planeZero[1];},[](ComponentValue&v){return &static_cast<TwoBoneIk&>(v).planeZero[1];},"plane_zero_y",{"Plano","","Direção do zero do ângulo no espaço do osso raiz; zerado usa a perpendicular do osso do meio.",twoBoneRotatePlane},false},
  ComponentNumber{"Zero do plano Z",-1,1,.01f,[](const ComponentValue&v)->const float&{return static_cast<const TwoBoneIk&>(v).planeZero[2];},[](ComponentValue&v){return &static_cast<TwoBoneIk&>(v).planeZero[2];},"plane_zero_z",{"Plano","","Direção do zero do ângulo no espaço do osso raiz; zerado usa a perpendicular do osso do meio.",twoBoneRotatePlane},false},
  ComponentNumber{"Desvio da rotação do alvo X",-360,360,.5f,[](const ComponentValue&v)->const float&{return static_cast<const TwoBoneIk&>(v).targetOffset[0];},[](ComponentValue&v){return &static_cast<TwoBoneIk&>(v).targetOffset[0];},"target_offset_x",{"Alvo","°","Rotação somada à do alvo quando a ponta segue a rotação do alvo (eixos do alvo).",twoBoneFollowsTarget},true},
  ComponentNumber{"Desvio da rotação do alvo Y",-360,360,.5f,[](const ComponentValue&v)->const float&{return static_cast<const TwoBoneIk&>(v).targetOffset[1];},[](ComponentValue&v){return &static_cast<TwoBoneIk&>(v).targetOffset[1];},"target_offset_y",{"Alvo","°","Rotação somada à do alvo quando a ponta segue a rotação do alvo (eixos do alvo).",twoBoneFollowsTarget},true},
  ComponentNumber{"Desvio da rotação do alvo Z",-360,360,.5f,[](const ComponentValue&v)->const float&{return static_cast<const TwoBoneIk&>(v).targetOffset[2];},[](ComponentValue&v){return &static_cast<TwoBoneIk&>(v).targetOffset[2];},"target_offset_z",{"Alvo","°","Rotação somada à do alvo quando a ponta segue a rotação do alvo (eixos do alvo).",twoBoneFollowsTarget},true}}};
inline constexpr std::array<ComponentEnumOption,2> twoBoneIkSolvers{{{0,"Pose"},{1,"Plano rotativo"}}};
inline constexpr std::array<ComponentEnumOption,2> twoBoneIkReferenceModes{{{0,"Animação"},{1,"Pose de repouso"}}};
inline constexpr std::array<ComponentEnum,2> twoBoneIkEnums{{
  ComponentEnum{"solver","Solver",twoBoneIkSolvers,[](const ComponentValue &v){return static_cast<const TwoBoneIk&>(v).solver;},[](ComponentValue &v,u32 n){static_cast<TwoBoneIk&>(v).solver=n;},{"Plano"}},
  ComponentEnum{"reference","Referência",twoBoneIkReferenceModes,[](const ComponentValue &v){return static_cast<const TwoBoneIk&>(v).reference;},[](ComponentValue &v,u32 n){static_cast<TwoBoneIk&>(v).reference=n;},{"Plano","","De onde a cadeia parte antes de resolver.",twoBoneRotatePlane}}}};
inline constexpr std::array<ComponentBoolean,2> twoBoneIkBooleans{{AE_IK_BOOL(TwoBoneIk,"enabled","Ativo",enabled,"Influência"),
  AE_IK_BOOL(TwoBoneIk,"match_target_rotation","Ponta segue rotação do alvo",matchTargetRotation,"Alvo")}};
inline constexpr std::array<ComponentObjectReference,2> twoBoneIkReferences{{
  {"target","Alvo","",ObjectReferenceScope::OtherNonDescendant,"Escolher alvo",[](const ComponentValue &v){return static_cast<const TwoBoneIk&>(v).target;},[](ComponentValue &v,u64 n){static_cast<TwoBoneIk&>(v).target=n;},{"Alvo"}},
  {"pole","Direção da dobra","",ObjectReferenceScope::OtherNonDescendant,"Seguir a pose",[](const ComponentValue &v){return static_cast<const TwoBoneIk&>(v).pole;},[](ComponentValue &v,u64 n){static_cast<TwoBoneIk&>(v).pole=n;},{"Alvo"}}}};
inline const ComponentType TwoBoneIk::descriptor{"astra.ik.two_bone",4,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<TwoBoneIk>();},twoBoneIkNumbers,twoBoneIkBooleans,twoBoneIkEnums,nullptr,false,twoBoneIkReferences};

inline bool chainRotatePlane(const ComponentValue &v){return static_cast<const ChainIk&>(v).solver==2;}
inline bool chainFollowsTarget(const ComponentValue &v){return static_cast<const ChainIk&>(v).matchTargetRotation;}
inline constexpr std::array<ComponentNumber,13> chainIkNumbers{{
  AE_IK_NUM(ChainIk,"weight","Peso",weight,0,1,.01f,"Influência",""),
  ComponentNumber{"Ângulo do plano",-360,360,.5f,[](const ComponentValue&v)->const float&{return static_cast<const ChainIk&>(v).planeAngle;},[](ComponentValue&v){return &static_cast<ChainIk&>(v).planeAngle;},"plane_angle",{"Plano","°","Gira o plano da dobra em torno do eixo raiz→ponta.",chainRotatePlane},true},
  ComponentNumber{"Rotação do polo",-360,360,.5f,[](const ComponentValue&v)->const float&{return static_cast<const ChainIk&>(v).poleRotation;},[](ComponentValue&v){return &static_cast<ChainIk&>(v).poleRotation;},"pole_rotation",{"Plano","°","Gira a cadeia resolvida em torno do eixo raiz→alvo.",chainRotatePlane},true},
  AE_IK_NUM(ChainIk,"priority","Prioridade",priority,-100,100,1,"Influência",""),
  AE_IK_NUM(ChainIk,"chain_length","Ossos na cadeia",chainLength,2,32,1,"Cadeia","ossos"),
  AE_IK_NUM(ChainIk,"iterations","Iterações máximas",iterations,1,128,1,"Solver",""),
  AE_IK_NUM(ChainIk,"tolerance","Tolerância",tolerance,.00001f,1,.0005f,"Solver","m"),
  ComponentNumber{"Zero do plano X",-1,1,.01f,[](const ComponentValue&v)->const float&{return static_cast<const ChainIk&>(v).planeZero[0];},[](ComponentValue&v){return &static_cast<ChainIk&>(v).planeZero[0];},"plane_zero_x",{"Plano","","Direção do zero do ângulo no espaço do osso raiz; zerado usa a perpendicular do osso do meio.",chainRotatePlane},false},
  ComponentNumber{"Zero do plano Y",-1,1,.01f,[](const ComponentValue&v)->const float&{return static_cast<const ChainIk&>(v).planeZero[1];},[](ComponentValue&v){return &static_cast<ChainIk&>(v).planeZero[1];},"plane_zero_y",{"Plano","","Direção do zero do ângulo no espaço do osso raiz; zerado usa a perpendicular do osso do meio.",chainRotatePlane},false},
  ComponentNumber{"Zero do plano Z",-1,1,.01f,[](const ComponentValue&v)->const float&{return static_cast<const ChainIk&>(v).planeZero[2];},[](ComponentValue&v){return &static_cast<ChainIk&>(v).planeZero[2];},"plane_zero_z",{"Plano","","Direção do zero do ângulo no espaço do osso raiz; zerado usa a perpendicular do osso do meio.",chainRotatePlane},false},
  ComponentNumber{"Desvio da rotação do alvo X",-360,360,.5f,[](const ComponentValue&v)->const float&{return static_cast<const ChainIk&>(v).targetOffset[0];},[](ComponentValue&v){return &static_cast<ChainIk&>(v).targetOffset[0];},"target_offset_x",{"Alvo","°","Rotação somada à do alvo quando a ponta segue a rotação do alvo (eixos do alvo).",chainFollowsTarget},true},
  ComponentNumber{"Desvio da rotação do alvo Y",-360,360,.5f,[](const ComponentValue&v)->const float&{return static_cast<const ChainIk&>(v).targetOffset[1];},[](ComponentValue&v){return &static_cast<ChainIk&>(v).targetOffset[1];},"target_offset_y",{"Alvo","°","Rotação somada à do alvo quando a ponta segue a rotação do alvo (eixos do alvo).",chainFollowsTarget},true},
  ComponentNumber{"Desvio da rotação do alvo Z",-360,360,.5f,[](const ComponentValue&v)->const float&{return static_cast<const ChainIk&>(v).targetOffset[2];},[](ComponentValue&v){return &static_cast<ChainIk&>(v).targetOffset[2];},"target_offset_z",{"Alvo","°","Rotação somada à do alvo quando a ponta segue a rotação do alvo (eixos do alvo).",chainFollowsTarget},true}}};
inline constexpr std::array<ComponentBoolean,2> chainIkBooleans{{AE_IK_BOOL(ChainIk,"enabled","Ativo",enabled,"Influência"),
  ComponentBoolean{"match_target_rotation","Ponta segue rotação do alvo",[](const ComponentValue&v){return static_cast<const ChainIk&>(v).matchTargetRotation;},[](ComponentValue&v,bool n){static_cast<ChainIk&>(v).matchTargetRotation=n;},{"Alvo","","",chainRotatePlane}}}};
inline constexpr std::array<ComponentEnumOption,3> chainIkSolverOptions{{{0,"CCD"},{1,"FABRIK"},{2,"Plano rotativo"}}};
inline constexpr std::array<ComponentEnum,2> chainIkEnums{{{"solver","Solver",chainIkSolverOptions,[](const ComponentValue &v){return static_cast<const ChainIk&>(v).solver;},[](ComponentValue &v,u32 n){static_cast<ChainIk&>(v).solver=n;},{"Solver"}},
  ComponentEnum{"reference","Referência",twoBoneIkReferenceModes,[](const ComponentValue &v){return static_cast<const ChainIk&>(v).reference;},[](ComponentValue &v,u32 n){static_cast<ChainIk&>(v).reference=n;},{"Plano","","De onde a cadeia parte antes de resolver.",chainRotatePlane}}}};
inline constexpr std::array<ComponentObjectReference,2> chainIkReferences{{
  {"target","Alvo","",ObjectReferenceScope::OtherNonDescendant,"Escolher alvo",[](const ComponentValue &v){return static_cast<const ChainIk&>(v).target;},[](ComponentValue &v,u64 n){static_cast<ChainIk&>(v).target=n;},{"Alvo"}},
  {"pole","Direção da dobra","",ObjectReferenceScope::OtherNonDescendant,"Ângulo do plano",[](const ComponentValue &v){return static_cast<const ChainIk&>(v).pole;},[](ComponentValue &v,u64 n){static_cast<ChainIk&>(v).pole=n;},{"Plano","","",chainRotatePlane}}}};
inline const ComponentType ChainIk::descriptor{"astra.ik.chain",4,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<ChainIk>();},chainIkNumbers,chainIkBooleans,chainIkEnums,nullptr,false,chainIkReferences};

inline constexpr std::array<ComponentNumber,6> footGroundingNumbers{{
  AE_IK_NUM(FootGrounding,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_IK_NUM(FootGrounding,"ray_height","Início do raio acima do pé",rayHeight,0,10,.01f,"Sondagem","m"),
  AE_IK_NUM(FootGrounding,"ray_distance","Alcance abaixo do pé",rayDistance,.01f,20,.01f,"Sondagem","m"),
  AE_IK_NUM(FootGrounding,"foot_offset","Altura do pé sobre a superfície",footOffset,-1,1,.005f,"Apoio","m"),
  AE_IK_NUM(FootGrounding,"max_correction","Correção máxima",maxCorrection,0,5,.01f,"Apoio","m"),
  AE_IK_NUM(FootGrounding,"lock_height","Altura de contato",lockHeight,0,1,.005f,"Trava","m")}};
inline constexpr std::array<ComponentBoolean,3> footGroundingBooleans{{AE_IK_BOOL(FootGrounding,"enabled","Ativo",enabled,"Influência"),
  AE_IK_BOOL(FootGrounding,"align_to_surface","Alinhar à superfície",alignToSurface,"Apoio"),
  AE_IK_BOOL(FootGrounding,"lock_foot","Travar pé no apoio",lockFoot,"Trava")}};
inline const ComponentType FootGrounding::descriptor{"astra.ik.foot_grounding",2,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<FootGrounding>();},footGroundingNumbers,footGroundingBooleans,{},nullptr,false,{}};
#undef AE_IK_NUM
#undef AE_IK_BOOL
// ---- Pacote A (FinalIK no jeito Astra) ----
// Eixos locais escolhidos por enum: 0 +X,1 +Y,2 +Z,3 -X,4 -Y,5 -Z.
inline constexpr std::array<ComponentEnumOption,6> ikAxisOptions{{{0,"+X"},{1,"+Y"},{2,"+Z"},{3,"-X"},{4,"-Y"},{5,"-Z"}}};

// Look At: cadeia de N ossos (este objeto e a cabeca) gira para o alvo, peso do
// corpo distribuido pela coluna e peso proprio da cabeca; limite de angulo.
class LookAtIk final : public ComponentValue {
public:
  u64 target=0;bool enabled=true;float weight=1,bodyWeight=.4f,headWeight=1,chainLength=3,clampAngle=90,priority=0;u32 axis=2;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<LookAtIk>(*this);}
  bool valid() const override {return target<=std::numeric_limits<u32>::max()&&axis<=5&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<target<<' '<<enabled<<' '<<weight<<' '<<bodyWeight<<' '<<headWeight<<' '<<chainLength<<' '<<clampAngle<<' '<<axis<<' '<<priority;}
  bool read(std::istream &in,u32 version) override {
    if(version<1||version>2||!(in>>target>>enabled>>weight>>bodyWeight>>headWeight>>chainLength>>clampAngle>>axis))return false;
    priority=0;return (version==1||static_cast<bool>(in>>priority))&&valid();
  }
};
// Aim: este objeto (cano, mao, ferramenta) aponta o eixo para o alvo girando
// os N ossos acima, com iteracoes; o peso cai do topo para a base.
class AimIk final : public ComponentValue {
public:
  u64 target=0;bool enabled=true;float weight=1,chainLength=2,iterations=4,falloff=.5f,priority=0;u32 axis=2;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<AimIk>(*this);}
  bool valid() const override {return target<=std::numeric_limits<u32>::max()&&axis<=5&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<target<<' '<<enabled<<' '<<weight<<' '<<chainLength<<' '<<iterations<<' '<<falloff<<' '<<axis<<' '<<priority;}
  bool read(std::istream &in,u32 version) override {
    if(version<1||version>2||!(in>>target>>enabled>>weight>>chainLength>>iterations>>falloff>>axis))return false;
    priority=0;return (version==1||static_cast<bool>(in>>priority))&&valid();
  }
};
// Limite de rotacao do osso, aplicado depois dos solvers aos ossos escritos.
// Referencia = rotacao local do osso ao entrar em Play.
class RotationLimit final : public ComponentValue {
public:
  bool enabled=true;u32 kind=1,axis=0;float minimum=-90,maximum=90,cone=45;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<RotationLimit>(*this);}
  bool valid() const override {return kind<=2&&axis<=2&&minimum<=maximum&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<kind<<' '<<axis<<' '<<minimum<<' '<<maximum<<' '<<cone;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>enabled>>kind>>axis>>minimum>>maximum>>cone)&&valid();}
};
// Grounder do corpo: na raiz do personagem. Usa os Apoios do pe da hierarquia e
// desce a pelve o necessario para o pe mais baixo alcancar o chao.
class GrounderBody final : public ComponentValue {
public:
  u64 pelvis=0;bool enabled=true;float weight=1,maxPelvisOffset=.4f;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<GrounderBody>(*this);}
  bool valid() const override {return pelvis<=std::numeric_limits<u32>::max()&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<pelvis<<' '<<enabled<<' '<<weight<<' '<<maxPelvisOffset;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>pelvis>>enabled>>weight>>maxPelvisOffset)&&valid();}
};
// Curvatura de dedo: na base do dedo, gira a cadeia de N ossos no eixo local.
class FingerCurl final : public ComponentValue {
public:
  bool enabled=true;float weight=1,curl=0,maxAngle=80,chainLength=3;u32 axis=0;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<FingerCurl>(*this);}
  bool valid() const override {return axis<=5&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<weight<<' '<<curl<<' '<<maxAngle<<' '<<chainLength<<' '<<axis;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>enabled>>weight>>curl>>maxAngle>>chainLength>>axis)&&valid();}
};
// Offset do efetor: desloca o alvo do IK desta ponta (espaco do mundo).
class IkOffset final : public ComponentValue {
public:
  bool enabled=true;float offset[3]{};
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<IkOffset>(*this);}
  bool valid() const override {return ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<offset[0]<<' '<<offset[1]<<' '<<offset[2];}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>enabled>>offset[0]>>offset[1]>>offset[2])&&valid();}
};
// Relaxar torcao: no osso do meio (antebraco), absorve parte da torcao do filho.
class TwistRelaxer final : public ComponentValue {
public:
  bool enabled=true;float weight=.5f;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<TwistRelaxer>(*this);}
  bool valid() const override {return ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<weight;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>enabled>>weight)&&valid();}
};

#define AE_IK_NUM(T,id,label,member,lo,hi,step,group,unit) ComponentNumber{label,lo,hi,step,[](const ComponentValue&v)->const float&{return static_cast<const T&>(v).member;},[](ComponentValue&v){return &static_cast<T&>(v).member;},id,{group,unit}}
#define AE_IK_BOOL(T,id,label,member,group) ComponentBoolean{id,label,[](const ComponentValue&v){return static_cast<const T&>(v).member;},[](ComponentValue&v,bool n){static_cast<T&>(v).member=n;},{group}}
#define AE_IK_ENUM(T,id,label,options,member,group) ComponentEnum{id,label,options,[](const ComponentValue &v){return static_cast<const T&>(v).member;},[](ComponentValue &v,u32 n){static_cast<T&>(v).member=n;},{group}}
#define AE_IK_REF(T,id,label,member,none,group) ComponentObjectReference{id,label,"",ObjectReferenceScope::OtherNonDescendant,none,[](const ComponentValue &v){return static_cast<const T&>(v).member;},[](ComponentValue &v,u64 n){static_cast<T&>(v).member=n;},{group}}

inline constexpr std::array<ComponentNumber,6> lookAtIkNumbers{{AE_IK_NUM(LookAtIk,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_IK_NUM(LookAtIk,"priority","Prioridade",priority,-100,100,1,"Influência",""),
  AE_IK_NUM(LookAtIk,"body_weight","Peso do corpo",bodyWeight,0,1,.01f,"Distribuição",""),AE_IK_NUM(LookAtIk,"head_weight","Peso da cabeça",headWeight,0,1,.01f,"Distribuição",""),
  AE_IK_NUM(LookAtIk,"chain_length","Ossos na cadeia",chainLength,1,16,1,"Distribuição","ossos"),AE_IK_NUM(LookAtIk,"clamp_angle","Ângulo máximo",clampAngle,0,180,1,"Limite","graus")}};
inline constexpr std::array<ComponentBoolean,1> lookAtIkBooleans{{AE_IK_BOOL(LookAtIk,"enabled","Ativo",enabled,"Influência")}};
inline constexpr std::array<ComponentEnum,1> lookAtIkEnums{{AE_IK_ENUM(LookAtIk,"axis","Eixo frontal",ikAxisOptions,axis,"Distribuição")}};
inline constexpr std::array<ComponentObjectReference,1> lookAtIkReferences{{AE_IK_REF(LookAtIk,"target","Alvo",target,"Escolher alvo","Alvo")}};
inline const ComponentType LookAtIk::descriptor{"astra.ik.look_at",2,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<LookAtIk>();},lookAtIkNumbers,lookAtIkBooleans,lookAtIkEnums,nullptr,false,lookAtIkReferences};

inline constexpr std::array<ComponentNumber,5> aimIkNumbers{{AE_IK_NUM(AimIk,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_IK_NUM(AimIk,"priority","Prioridade",priority,-100,100,1,"Influência",""),
  AE_IK_NUM(AimIk,"chain_length","Ossos na cadeia",chainLength,1,16,1,"Cadeia","ossos"),AE_IK_NUM(AimIk,"iterations","Iterações",iterations,1,32,1,"Cadeia",""),
  AE_IK_NUM(AimIk,"falloff","Queda do peso para a base",falloff,0,1,.01f,"Cadeia","")}};
inline constexpr std::array<ComponentBoolean,1> aimIkBooleans{{AE_IK_BOOL(AimIk,"enabled","Ativo",enabled,"Influência")}};
inline constexpr std::array<ComponentEnum,1> aimIkEnums{{AE_IK_ENUM(AimIk,"axis","Eixo de mira",ikAxisOptions,axis,"Cadeia")}};
inline constexpr std::array<ComponentObjectReference,1> aimIkReferences{{AE_IK_REF(AimIk,"target","Alvo",target,"Escolher alvo","Alvo")}};
inline const ComponentType AimIk::descriptor{"astra.ik.aim",2,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<AimIk>();},aimIkNumbers,aimIkBooleans,aimIkEnums,nullptr,false,aimIkReferences};

inline constexpr std::array<ComponentEnumOption,3> rotationLimitKinds{{{0,"Livre"},{1,"Dobradiça"},{2,"Cone e torção"}}};
inline constexpr std::array<ComponentEnumOption,3> rotationLimitAxes{{{0,"X"},{1,"Y"},{2,"Z"}}};
inline bool rotationLimitActive(const ComponentValue &v){return static_cast<const RotationLimit&>(v).kind!=0;}
inline bool rotationLimitHinge(const ComponentValue &v){return static_cast<const RotationLimit&>(v).kind==1;}
inline bool rotationLimitCone(const ComponentValue &v){return static_cast<const RotationLimit&>(v).kind==2;}
inline const std::array<ComponentNumber,3> rotationLimitNumbers{{
  ComponentNumber{"Mínimo",-180,180,1,[](const ComponentValue&v)->const float&{return static_cast<const RotationLimit&>(v).minimum;},[](ComponentValue&v){return &static_cast<RotationLimit&>(v).minimum;},"minimum",{"Limite","graus",nullptr,rotationLimitActive}},
  ComponentNumber{"Máximo",-180,180,1,[](const ComponentValue&v)->const float&{return static_cast<const RotationLimit&>(v).maximum;},[](ComponentValue&v){return &static_cast<RotationLimit&>(v).maximum;},"maximum",{"Limite","graus",nullptr,rotationLimitActive}},
  ComponentNumber{"Cone",0,180,1,[](const ComponentValue&v)->const float&{return static_cast<const RotationLimit&>(v).cone;},[](ComponentValue&v){return &static_cast<RotationLimit&>(v).cone;},"cone",{"Limite","graus",nullptr,rotationLimitCone}}}};
inline constexpr std::array<ComponentBoolean,1> rotationLimitBooleans{{AE_IK_BOOL(RotationLimit,"enabled","Ativo",enabled,"Limite")}};
inline constexpr std::array<ComponentEnum,2> rotationLimitEnums{{AE_IK_ENUM(RotationLimit,"kind","Tipo",rotationLimitKinds,kind,"Limite"),AE_IK_ENUM(RotationLimit,"axis","Eixo",rotationLimitAxes,axis,"Limite")}};
inline const ComponentType RotationLimit::descriptor{"astra.ik.rotation_limit",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<RotationLimit>();},rotationLimitNumbers,rotationLimitBooleans,rotationLimitEnums};

inline constexpr std::array<ComponentNumber,2> grounderBodyNumbers{{AE_IK_NUM(GrounderBody,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_IK_NUM(GrounderBody,"max_pelvis_offset","Descida máxima da pelve",maxPelvisOffset,0,5,.01f,"Pelve","m")}};
inline constexpr std::array<ComponentBoolean,1> grounderBodyBooleans{{AE_IK_BOOL(GrounderBody,"enabled","Ativo",enabled,"Influência")}};
inline constexpr std::array<ComponentObjectReference,1> grounderBodyReferences{{ComponentObjectReference{"pelvis","Pelve","",ObjectReferenceScope::Descendant,"Escolher pelve",[](const ComponentValue &v){return static_cast<const GrounderBody&>(v).pelvis;},[](ComponentValue &v,u64 n){static_cast<GrounderBody&>(v).pelvis=n;},{"Pelve"}}}};
inline const ComponentType GrounderBody::descriptor{"astra.ik.grounder",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<GrounderBody>();},grounderBodyNumbers,grounderBodyBooleans,{},nullptr,false,grounderBodyReferences};

inline constexpr std::array<ComponentNumber,4> fingerCurlNumbers{{AE_IK_NUM(FingerCurl,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_IK_NUM(FingerCurl,"curl","Fechamento",curl,-1,1,.01f,"Dedo",""),AE_IK_NUM(FingerCurl,"max_angle","Ângulo por osso",maxAngle,0,180,1,"Dedo","graus"),
  AE_IK_NUM(FingerCurl,"chain_length","Ossos no dedo",chainLength,1,6,1,"Dedo","ossos")}};
inline constexpr std::array<ComponentBoolean,1> fingerCurlBooleans{{AE_IK_BOOL(FingerCurl,"enabled","Ativo",enabled,"Influência")}};
inline constexpr std::array<ComponentEnum,1> fingerCurlEnums{{AE_IK_ENUM(FingerCurl,"axis","Eixo de dobra",ikAxisOptions,axis,"Dedo")}};
inline const ComponentType FingerCurl::descriptor{"astra.ik.finger_curl",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<FingerCurl>();},fingerCurlNumbers,fingerCurlBooleans,fingerCurlEnums};

inline constexpr std::array<ComponentNumber,3> ikOffsetNumbers{{AE_IK_NUM(IkOffset,"offset_x","Offset X",offset[0],-10,10,.005f,"Offset","m"),
  AE_IK_NUM(IkOffset,"offset_y","Offset Y",offset[1],-10,10,.005f,"Offset","m"),AE_IK_NUM(IkOffset,"offset_z","Offset Z",offset[2],-10,10,.005f,"Offset","m")}};
inline constexpr std::array<ComponentBoolean,1> ikOffsetBooleans{{AE_IK_BOOL(IkOffset,"enabled","Ativo",enabled,"Offset")}};
inline constexpr std::array<ComponentTriple,1> ikOffsetTriples{{{"offset","Offset do alvo",{"offset_x","offset_y","offset_z"}}}};
inline const ComponentType IkOffset::descriptor{"astra.ik.offset",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<IkOffset>();},ikOffsetNumbers,ikOffsetBooleans,{},nullptr,false,{},ikOffsetTriples};

inline constexpr std::array<ComponentNumber,1> twistRelaxerNumbers{{AE_IK_NUM(TwistRelaxer,"weight","Parte da torção",weight,0,1,.01f,"Torção","")}};
inline constexpr std::array<ComponentBoolean,1> twistRelaxerBooleans{{AE_IK_BOOL(TwistRelaxer,"enabled","Ativo",enabled,"Torção")}};
inline const ComponentType TwistRelaxer::descriptor{"astra.ik.twist_relaxer",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<TwistRelaxer>();},twistRelaxerNumbers,twistRelaxerBooleans};
#undef AE_IK_NUM
#undef AE_IK_BOOL
#undef AE_IK_ENUM
#undef AE_IK_REF
} // namespace ae::scene
