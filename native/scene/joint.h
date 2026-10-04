#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
enum class JointKind : u32 { Point=0,Hinge=1,Slider=2,Distance=3, Fixed=4, Cone=5, SwingTwist=6, SixDOF=7, Spring=8 };
// Six independent channels; 0=locked, 1=limited, 2=free. Angles in degrees.
struct JointAxis {
  u32 motion=0, motor=0;
  float minimum=-1, maximum=1, friction=0, velocity=0, position=0;
  float force=100, frequency=2, damping=1;
};
class Joint final : public ComponentValue {
public:
  JointKind kind=JointKind::Distance;
  u32 motor=0;
  u64 connectedBody=0;

#include "scene/generated/joint_Joint_fields0.inc"

  float anchorA[3]{},anchorB[3]{},axisA[3]{0,1,0},axisB[3]{0,1,0};
  float limitMin=0,limitMax=1,motorVelocity=0,motorPosition=0,motorForce=100,frequency=2,damping=1;
  float normalA[3]{1,0,0},normalB[3]{1,0,0};

#include "scene/generated/joint_Joint_fields4.inc"

  JointAxis axes[6]{};
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Joint>(*this);}
  bool valid() const override {
    if(static_cast<u32>(kind)>8||motor>3||connectedBody>std::numeric_limits<u32>::max()||limitMin>limitMax) return false;
    for(const auto &p:descriptor.numbers) {const auto v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    if((kind==JointKind::Distance||kind==JointKind::Spring) && limitMin<0) return false;
    if(kind==JointKind::Hinge && (limitMin< -180 || limitMin>0 || limitMax<0 || limitMax>180)) return false;
    if((kind!=JointKind::Hinge&&kind!=JointKind::Slider) && motor) return false;
    if(kind!=JointKind::Point&&kind!=JointKind::Distance&&kind!=JointKind::Spring)
      for(const auto *axis:{axisA,axisB}) if(axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2]<1e-8f) return false;
    if(twistMin>twistMax) return false;
    if(kind==JointKind::Fixed||kind==JointKind::SwingTwist||kind==JointKind::SixDOF) {
      for(u32 i=0;i<2;++i) {const auto *a=i?axisB:axisA,*n=i?normalB:normalA;
        const float x=a[1]*n[2]-a[2]*n[1],y=a[2]*n[0]-a[0]*n[2],z=a[0]*n[1]-a[1]*n[0];
        if(x*x+y*y+z*z<1e-8f) return false;
      }
    }
    for(u32 i=0;i<6;++i) {const auto &a=axes[i];
      if(a.motion>2||a.motor>3||a.minimum>=a.maximum) return false;
      if(kind==JointKind::SixDOF&&a.motion==0&&a.motor!=0) return false;
    }
    return true;
  }
  void write(std::ostream &out) const override {
    out<<static_cast<u32>(kind)<<' '<<motor<<' '<<connectedBody<<' '<<enabled;
    for(const auto &p:descriptor.numbers) out<<' '<<p.read(*this);
    for(const auto &a:axes) out<<' '<<a.motion<<' '<<a.motor;
  }
  bool read(std::istream &in,u32 version) override {
    u32 k=0;if((version!=1&&version!=2)||!(in>>k>>motor>>connectedBody>>enabled)||k>(version==1?3u:8u)) return false;kind=static_cast<JointKind>(k);
    if(version==1) {for(auto &a:axes)a=JointAxis{};normalA[0]=normalB[0]=1;normalA[1]=normalA[2]=normalB[1]=normalB[2]=0;swingY=swingZ=45;twistMin=-45;twistMax=45;}
    const usize count=version==1?19:descriptor.numbers.size();
    for(usize i=0;i<count;++i) if(!(in>>*descriptor.numbers[i].write(*this))) return false;
    if(version>=2) for(auto &a:axes) if(!(in>>a.motion>>a.motor)) return false;
    return valid();
  }
};
inline bool jointHasAxis(const ComponentValue &v) {
  const auto kind=static_cast<const Joint&>(v).kind;
  return kind!=JointKind::Point&&kind!=JointKind::Distance&&kind!=JointKind::Spring;
}
inline bool jointHasLimits(const ComponentValue &v) {const auto k=static_cast<const Joint&>(v).kind;return k==JointKind::Hinge||k==JointKind::Slider||k==JointKind::Distance||k==JointKind::Spring;}
inline bool jointHasMotor(const ComponentValue &v) {const auto k=static_cast<const Joint&>(v).kind;return k==JointKind::Hinge||k==JointKind::Slider;}
inline bool jointHasFrame(const ComponentValue &v) {const auto k=static_cast<const Joint&>(v).kind;return k==JointKind::Fixed||k==JointKind::SwingTwist||k==JointKind::SixDOF;}
inline bool jointHasSwing(const ComponentValue &v) {const auto k=static_cast<const Joint&>(v).kind;return k==JointKind::Cone||k==JointKind::SwingTwist;}
inline bool jointHasTwist(const ComponentValue &v) {return static_cast<const Joint&>(v).kind==JointKind::SwingTwist;}
inline bool jointIsSixDOF(const ComponentValue &v) {return static_cast<const Joint&>(v).kind==JointKind::SixDOF;}
inline bool jointHasSpring(const ComponentValue &v) {return static_cast<const Joint&>(v).kind==JointKind::Spring|| (jointHasMotor(v)&&static_cast<const Joint&>(v).motor>=2);}
template<u32 I> bool jointAxisLimited(const ComponentValue &v) {return jointIsSixDOF(v)&&static_cast<const Joint&>(v).axes[I].motion==1;}
template<u32 I> bool jointAxisMoving(const ComponentValue &v) {return jointIsSixDOF(v)&&static_cast<const Joint&>(v).axes[I].motion!=0;}
template<u32 I> bool jointAxisMotor(const ComponentValue &v) {return jointAxisMoving<I>(v)&&static_cast<const Joint&>(v).axes[I].motor!=0;}
template<u32 I> bool jointAxisPosition(const ComponentValue &v) {return jointAxisMotor<I>(v)&&static_cast<const Joint&>(v).axes[I].motor>=2;}
inline bool jointMotorActive(const ComponentValue &v) {return jointHasMotor(v)&&static_cast<const Joint&>(v).motor!=0;}
template<u32 I> bool jointAxisFriction(const ComponentValue &v) {return jointAxisMoving<I>(v)&&static_cast<const Joint&>(v).axes[I].motor==0;}
template<u32 I> bool jointAxisVelocity(const ComponentValue &v) {const auto m=static_cast<const Joint&>(v).axes[I].motor;return jointAxisMoving<I>(v)&&(m==1||m==3);}
inline constexpr auto jointBaseNumbers=[] {
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
  p.group=i<6?"Âncoras":i<12?"Eixos":i<14?"Movimento":"Motor";
  if(i>=6&&i<12) p.visible=jointHasAxis;
  if(i>=12&&i<14) p.visible=jointHasLimits;
  if(i>=14) p.visible=jointMotorActive;
  if(i>=17) p.visible=jointHasSpring;
  if(i==17) p.unit="Hz";
}
return properties;
}();
#include "scene/generated/joint_jointExtraNumbers.inc"
inline constexpr auto jointNumbers=[] {std::array<ComponentNumber,jointBaseNumbers.size()+jointExtraNumbers.size()> a{};usize n=0;for(auto p:jointBaseNumbers)a[n++]=p;for(auto p:jointExtraNumbers)a[n++]=p;return a;}();
inline constexpr std::array<ComponentEnumOption,9> jointKinds{{{0,"Ponto"},{1,"Dobradiça"},{2,"Deslizante"},{3,"Distância"},{4,"Fixa"},{5,"Cone"},{6,"Swing / Twist"},{7,"Configurável 6DOF"},{8,"Mola"}}};
inline constexpr std::array<ComponentEnumOption,4> jointMotors{{{0,"Desligado"},{1,"Velocidade"},{2,"Posição"},{3,"Posição e velocidade"}}};
inline constexpr std::array<ComponentEnumOption,3> jointMotionModes{{{0,"Travado"},{1,"Limitado"},{2,"Livre"}}};
inline constexpr std::array<ComponentEnum,14> jointEnums{{
  {"kind","Tipo",jointKinds,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Joint&>(v).kind);},[](ComponentValue &v,u32 k){auto &j=static_cast<Joint&>(v);j.kind=static_cast<JointKind>(k);
    if(k!=1&&k!=2) j.motor=0;
    if(k==3||k==8) {j.limitMin=std::max(0.0f,j.limitMin);j.limitMax=std::max(j.limitMin,j.limitMax);}
    if(k==1) {j.limitMin=std::clamp(j.limitMin,-180.0f,0.0f);j.limitMax=std::clamp(j.limitMax,0.0f,180.0f);}}},
  {"motor","Motor",jointMotors,[](const ComponentValue &v){return static_cast<const Joint&>(v).motor;},[](ComponentValue &v,u32 k){static_cast<Joint&>(v).motor=k;},{"Motor","",nullptr,jointHasMotor}},
  {"linear_x_motion","Movimento",jointMotionModes,[](const ComponentValue &v){return static_cast<const Joint&>(v).axes[0].motion;},[](ComponentValue &v,u32 k){auto &j=static_cast<Joint&>(v);j.axes[0].motion=k;if(k==0)j.axes[0].motor=0;},{"Translação X","",nullptr,jointIsSixDOF}},
  {"linear_x_motor","Motor",jointMotors,[](const ComponentValue &v){return static_cast<const Joint&>(v).axes[0].motor;},[](ComponentValue &v,u32 k){auto &j=static_cast<Joint&>(v);j.axes[0].motor=k;},{"Translação X","",nullptr,jointAxisMoving<0>}},
  {"linear_y_motion","Movimento",jointMotionModes,[](const ComponentValue &v){return static_cast<const Joint&>(v).axes[1].motion;},[](ComponentValue &v,u32 k){auto &j=static_cast<Joint&>(v);j.axes[1].motion=k;if(k==0)j.axes[1].motor=0;},{"Translação Y","",nullptr,jointIsSixDOF}},
  {"linear_y_motor","Motor",jointMotors,[](const ComponentValue &v){return static_cast<const Joint&>(v).axes[1].motor;},[](ComponentValue &v,u32 k){auto &j=static_cast<Joint&>(v);j.axes[1].motor=k;},{"Translação Y","",nullptr,jointAxisMoving<1>}},
  {"linear_z_motion","Movimento",jointMotionModes,[](const ComponentValue &v){return static_cast<const Joint&>(v).axes[2].motion;},[](ComponentValue &v,u32 k){auto &j=static_cast<Joint&>(v);j.axes[2].motion=k;if(k==0)j.axes[2].motor=0;},{"Translação Z","",nullptr,jointIsSixDOF}},
  {"linear_z_motor","Motor",jointMotors,[](const ComponentValue &v){return static_cast<const Joint&>(v).axes[2].motor;},[](ComponentValue &v,u32 k){auto &j=static_cast<Joint&>(v);j.axes[2].motor=k;},{"Translação Z","",nullptr,jointAxisMoving<2>}},
  {"angular_x_motion","Movimento",jointMotionModes,[](const ComponentValue &v){return static_cast<const Joint&>(v).axes[3].motion;},[](ComponentValue &v,u32 k){auto &j=static_cast<Joint&>(v);j.axes[3].motion=k;if(k==0)j.axes[3].motor=0;},{"Rotação X","",nullptr,jointIsSixDOF}},
  {"angular_x_motor","Motor",jointMotors,[](const ComponentValue &v){return static_cast<const Joint&>(v).axes[3].motor;},[](ComponentValue &v,u32 k){auto &j=static_cast<Joint&>(v);j.axes[3].motor=k;},{"Rotação X","",nullptr,jointAxisMoving<3>}},
  {"angular_y_motion","Movimento",jointMotionModes,[](const ComponentValue &v){return static_cast<const Joint&>(v).axes[4].motion;},[](ComponentValue &v,u32 k){auto &j=static_cast<Joint&>(v);j.axes[4].motion=k;if(k==0)j.axes[4].motor=0;},{"Rotação Y","",nullptr,jointIsSixDOF}},
  {"angular_y_motor","Motor",jointMotors,[](const ComponentValue &v){return static_cast<const Joint&>(v).axes[4].motor;},[](ComponentValue &v,u32 k){auto &j=static_cast<Joint&>(v);j.axes[4].motor=k;},{"Rotação Y","",nullptr,jointAxisMoving<4>}},
  {"angular_z_motion","Movimento",jointMotionModes,[](const ComponentValue &v){return static_cast<const Joint&>(v).axes[5].motion;},[](ComponentValue &v,u32 k){auto &j=static_cast<Joint&>(v);j.axes[5].motion=k;if(k==0)j.axes[5].motor=0;},{"Rotação Z","",nullptr,jointIsSixDOF}},
  {"angular_z_motor","Motor",jointMotors,[](const ComponentValue &v){return static_cast<const Joint&>(v).axes[5].motor;},[](ComponentValue &v,u32 k){auto &j=static_cast<Joint&>(v);j.axes[5].motor=k;},{"Rotação Z","",nullptr,jointAxisMoving<5>}}
}};
#include "scene/generated/joint_jointBooleans.inc"
inline constexpr std::array<ComponentObjectReference,1> jointReferences{{
  {"connected_body","Conectar corpo","astra.physics.body",ObjectReferenceScope::Other,"Escolher corpo",
    [](const ComponentValue &v){return static_cast<const Joint&>(v).connectedBody;},[](ComponentValue &v,u64 id){static_cast<Joint&>(v).connectedBody=id;},{"Âncoras"},
    [](const ComponentValue &v){return static_cast<const Joint&>(v).enabled;}}
}};
inline constexpr std::array<ComponentTriple,6> jointTriples{{
  {"anchor_a","Âncora A",{"anchor_a_x","anchor_a_y","anchor_a_z"}},
  {"anchor_b","Âncora B",{"anchor_b_x","anchor_b_y","anchor_b_z"}},
  {"axis_a","Eixo A",{"axis_a_x","axis_a_y","axis_a_z"}},
  {"axis_b","Eixo B",{"axis_b_x","axis_b_y","axis_b_z"}},
  {"normal_a","Plano A",{"normal_a_x","normal_a_y","normal_a_z"}},
  {"normal_b","Plano B",{"normal_b_x","normal_b_y","normal_b_z"}}
}};
inline const ComponentType Joint::descriptor{
  "astra.physics.joint",2,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Joint>();},jointNumbers,jointBooleans,jointEnums,nullptr,true,jointReferences,jointTriples
};
}
