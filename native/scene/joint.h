#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
enum class JointKind : u32 { Point=0,Hinge=1,Slider=2,Distance=3 };
class Joint final : public ComponentValue {
public:
  JointKind kind=JointKind::Distance;
  u32 motor=0;
  u64 connectedBody=0;
  bool enabled=true;
  float anchorA[3]{},anchorB[3]{},axisA[3]{0,1,0},axisB[3]{0,1,0};
  float limitMin=0,limitMax=1,motorVelocity=0,motorPosition=0,motorForce=100,frequency=2,damping=1;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Joint>(*this);}
  bool valid() const override {
    if(static_cast<u32>(kind)>3||motor>3||connectedBody>std::numeric_limits<u32>::max()||limitMin>limitMax) return false;
    for(const auto &p:descriptor.numbers) {const auto v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    if(kind==JointKind::Distance && limitMin<0) return false;
    if(kind==JointKind::Hinge && (limitMin< -180 || limitMin>0 || limitMax<0 || limitMax>180)) return false;
    if((kind==JointKind::Point||kind==JointKind::Distance) && motor) return false;
    if(kind==JointKind::Hinge||kind==JointKind::Slider)
      for(const auto *axis:{axisA,axisB}) if(axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2]<1e-8f) return false;
    return true;
  }
  void write(std::ostream &out) const override {
    out<<static_cast<u32>(kind)<<' '<<motor<<' '<<connectedBody<<' '<<enabled;
    for(const auto &p:descriptor.numbers) out<<' '<<p.read(*this);
  }
  bool read(std::istream &in,u32 version) override {
    u32 k=0;if(version!=1||!(in>>k>>motor>>connectedBody>>enabled)||k>3) return false;kind=static_cast<JointKind>(k);
    for(const auto &p:descriptor.numbers) if(!(in>>*p.write(*this))) return false;
    return true;
  }
};
inline bool jointHasAxis(const ComponentValue &v) {
  const auto kind=static_cast<const Joint&>(v).kind;
  return kind==JointKind::Hinge||kind==JointKind::Slider;
}
inline bool jointHasLimits(const ComponentValue &v) {return static_cast<const Joint&>(v).kind!=JointKind::Point;}
inline bool jointMotorActive(const ComponentValue &v) {return jointHasAxis(v)&&static_cast<const Joint&>(v).motor!=0;}
inline constexpr auto jointNumbers=[] {
std::array<ComponentNumber,19> properties{{
  {"Âncora A · X",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).anchorA[0];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).anchorA[0];},"anchor_a_x"},
  {"Âncora A · Y",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).anchorA[1];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).anchorA[1];},"anchor_a_y"},
  {"Âncora A · Z",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).anchorA[2];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).anchorA[2];},"anchor_a_z"},
  {"Âncora B · X",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).anchorB[0];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).anchorB[0];},"anchor_b_x"},
  {"Âncora B · Y",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).anchorB[1];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).anchorB[1];},"anchor_b_y"},
  {"Âncora B · Z",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).anchorB[2];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).anchorB[2];},"anchor_b_z"},
  {"Eixo A · X",-1.0f,1.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axisA[0];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axisA[0];},"axis_a_x"},
  {"Eixo A · Y",-1.0f,1.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axisA[1];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axisA[1];},"axis_a_y"},
  {"Eixo A · Z",-1.0f,1.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axisA[2];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axisA[2];},"axis_a_z"},
  {"Eixo B · X",-1.0f,1.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axisB[0];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axisB[0];},"axis_b_x"},
  {"Eixo B · Y",-1.0f,1.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axisB[1];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axisB[1];},"axis_b_y"},
  {"Eixo B · Z",-1.0f,1.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axisB[2];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axisB[2];},"axis_b_z"},
  {"Limite mínimo",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).limitMin;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).limitMin;},"limit_min"},
  {"Limite máximo",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).limitMax;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).limitMax;},"limit_max"},
  {"Velocidade do motor",-1000.0f,1000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).motorVelocity;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).motorVelocity;},"motor_velocity"},
  {"Alvo do motor",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).motorPosition;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).motorPosition;},"motor_position"},
  {"Força / torque máximo",0.0f,1000000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).motorForce;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).motorForce;},"motor_force"},
  {"Frequência · Hz",0.001f,1000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).frequency;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).frequency;},"spring_frequency"},
  {"Amortecimento da mola",0.0f,10.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).damping;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).damping;},"spring_damping"}
}};
for(usize i=0;i<properties.size();++i) {
  auto &p=properties[i].presentation;
  p.group=i<6?"Âncoras":i<14?"Movimento":"Motor";
  if(i>=6&&i<12) p.visible=jointHasAxis;
  if(i>=12&&i<14) p.visible=jointHasLimits;
  if(i>=14) p.visible=jointMotorActive;
  if(i==17) p.unit="Hz";
}
return properties;
}();
inline constexpr std::array<ComponentEnumOption,4> jointKinds{{{0,"Ponto"},{1,"Dobradiça"},{2,"Deslizante"},{3,"Distância"}}};
inline constexpr std::array<ComponentEnumOption,4> jointMotors{{{0,"Desligado"},{1,"Velocidade"},{2,"Posição"},{3,"Posição e velocidade"}}};
inline constexpr std::array<ComponentEnum,2> jointEnums{{
  {"kind","Tipo",jointKinds,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Joint&>(v).kind);},[](ComponentValue &v,u32 k){auto &j=static_cast<Joint&>(v);j.kind=static_cast<JointKind>(k);
    if(k==0||k==3) j.motor=0;
    if(k==3) {j.limitMin=std::max(0.0f,j.limitMin);j.limitMax=std::max(j.limitMin,j.limitMax);}
    if(k==1) {j.limitMin=std::clamp(j.limitMin,-180.0f,0.0f);j.limitMax=std::clamp(j.limitMax,0.0f,180.0f);}}},
  {"motor","Motor",jointMotors,[](const ComponentValue &v){return static_cast<const Joint&>(v).motor;},[](ComponentValue &v,u32 k){static_cast<Joint&>(v).motor=k;},{"Motor","",nullptr,jointHasAxis}}
}};
inline constexpr std::array<ComponentBoolean,1> jointBooleans{{
  {"enabled","Ativa",[](const ComponentValue &v){return static_cast<const Joint&>(v).enabled;},[](ComponentValue &v,bool b){static_cast<Joint&>(v).enabled=b;}}
}};
inline constexpr std::array<ComponentObjectReference,1> jointReferences{{
  {"connected_body","Conectar corpo","astra.physics.body",ObjectReferenceScope::Other,"Escolher corpo",
    [](const ComponentValue &v){return static_cast<const Joint&>(v).connectedBody;},[](ComponentValue &v,u64 id){static_cast<Joint&>(v).connectedBody=id;},{"Âncoras"},
    [](const ComponentValue &v){return static_cast<const Joint&>(v).enabled;}}
}};
inline constexpr std::array<ComponentTriple,4> jointTriples{{
  {"anchor_a","Âncora A",{"anchor_a_x","anchor_a_y","anchor_a_z"}},
  {"anchor_b","Âncora B",{"anchor_b_x","anchor_b_y","anchor_b_z"}},
  {"axis_a","Eixo A",{"axis_a_x","axis_a_y","axis_a_z"}},
  {"axis_b","Eixo B",{"axis_b_x","axis_b_y","axis_b_z"}}
}};
inline const ComponentType Joint::descriptor{
  "astra.physics.joint",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Joint>();},jointNumbers,jointBooleans,jointEnums,nullptr,true,jointReferences,jointTriples
};
}
