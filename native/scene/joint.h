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
  bool enabled=true;
  float anchorA[3]{},anchorB[3]{},axisA[3]{0,1,0},axisB[3]{0,1,0};
  float limitMin=0,limitMax=1,motorVelocity=0,motorPosition=0,motorForce=100,frequency=2,damping=1;
  float normalA[3]{1,0,0},normalB[3]{1,0,0};
  float swingY=45,swingZ=45,twistMin=-45,twistMax=45;
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
inline constexpr std::array<ComponentNumber,58> jointExtraNumbers{{
  {"Plano A · X",-1.0f,1.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).normalA[0];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).normalA[0];},"normal_a_x",{"Eixos","",nullptr,jointHasFrame}},
  {"Plano A · Y",-1.0f,1.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).normalA[1];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).normalA[1];},"normal_a_y",{"Eixos","",nullptr,jointHasFrame}},
  {"Plano A · Z",-1.0f,1.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).normalA[2];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).normalA[2];},"normal_a_z",{"Eixos","",nullptr,jointHasFrame}},
  {"Plano B · X",-1.0f,1.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).normalB[0];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).normalB[0];},"normal_b_x",{"Eixos","",nullptr,jointHasFrame}},
  {"Plano B · Y",-1.0f,1.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).normalB[1];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).normalB[1];},"normal_b_y",{"Eixos","",nullptr,jointHasFrame}},
  {"Plano B · Z",-1.0f,1.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).normalB[2];},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).normalB[2];},"normal_b_z",{"Eixos","",nullptr,jointHasFrame}},
  {"Cone · semiângulo Y",0.0f,180.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).swingY;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).swingY;},"swing_y",{"Rotação","°",nullptr,jointHasSwing}},
  {"Cone · semiângulo Z",0.0f,180.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).swingZ;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).swingZ;},"swing_z",{"Rotação","°",nullptr,jointHasTwist}},
  {"Torção mínima",-180.0f,180.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).twistMin;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).twistMin;},"twist_min",{"Rotação","°",nullptr,jointHasTwist}},
  {"Torção máxima",-180.0f,180.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).twistMax;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).twistMax;},"twist_max",{"Rotação","°",nullptr,jointHasTwist}},
  {"Limite mínimo",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[0].minimum;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[0].minimum;},"linear_x_minimum",{"Translação X","u",nullptr,jointAxisLimited<0>}},
  {"Limite máximo",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[0].maximum;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[0].maximum;},"linear_x_maximum",{"Translação X","u",nullptr,jointAxisLimited<0>}},
  {"Atrito máximo",0.0f,1000000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[0].friction;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[0].friction;},"linear_x_friction",{"Translação X","N",nullptr,jointAxisFriction<0>}},
  {"Velocidade alvo",-1000.0f,1000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[0].velocity;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[0].velocity;},"linear_x_velocity",{"Translação X","u/s",nullptr,jointAxisVelocity<0>}},
  {"Posição alvo",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[0].position;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[0].position;},"linear_x_position",{"Translação X","u",nullptr,jointAxisPosition<0>}},
  {"Força máxima",0.0f,1000000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[0].force;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[0].force;},"linear_x_force",{"Translação X","N",nullptr,jointAxisMotor<0>}},
  {"Frequência",0.001f,1000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[0].frequency;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[0].frequency;},"linear_x_frequency",{"Translação X","Hz",nullptr,jointAxisPosition<0>}},
  {"Amortecimento",0.0f,10.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[0].damping;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[0].damping;},"linear_x_damping",{"Translação X","",nullptr,jointAxisPosition<0>}},
  {"Limite mínimo",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[1].minimum;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[1].minimum;},"linear_y_minimum",{"Translação Y","u",nullptr,jointAxisLimited<1>}},
  {"Limite máximo",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[1].maximum;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[1].maximum;},"linear_y_maximum",{"Translação Y","u",nullptr,jointAxisLimited<1>}},
  {"Atrito máximo",0.0f,1000000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[1].friction;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[1].friction;},"linear_y_friction",{"Translação Y","N",nullptr,jointAxisFriction<1>}},
  {"Velocidade alvo",-1000.0f,1000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[1].velocity;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[1].velocity;},"linear_y_velocity",{"Translação Y","u/s",nullptr,jointAxisVelocity<1>}},
  {"Posição alvo",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[1].position;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[1].position;},"linear_y_position",{"Translação Y","u",nullptr,jointAxisPosition<1>}},
  {"Força máxima",0.0f,1000000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[1].force;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[1].force;},"linear_y_force",{"Translação Y","N",nullptr,jointAxisMotor<1>}},
  {"Frequência",0.001f,1000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[1].frequency;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[1].frequency;},"linear_y_frequency",{"Translação Y","Hz",nullptr,jointAxisPosition<1>}},
  {"Amortecimento",0.0f,10.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[1].damping;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[1].damping;},"linear_y_damping",{"Translação Y","",nullptr,jointAxisPosition<1>}},
  {"Limite mínimo",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[2].minimum;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[2].minimum;},"linear_z_minimum",{"Translação Z","u",nullptr,jointAxisLimited<2>}},
  {"Limite máximo",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[2].maximum;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[2].maximum;},"linear_z_maximum",{"Translação Z","u",nullptr,jointAxisLimited<2>}},
  {"Atrito máximo",0.0f,1000000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[2].friction;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[2].friction;},"linear_z_friction",{"Translação Z","N",nullptr,jointAxisFriction<2>}},
  {"Velocidade alvo",-1000.0f,1000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[2].velocity;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[2].velocity;},"linear_z_velocity",{"Translação Z","u/s",nullptr,jointAxisVelocity<2>}},
  {"Posição alvo",-100000.0f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[2].position;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[2].position;},"linear_z_position",{"Translação Z","u",nullptr,jointAxisPosition<2>}},
  {"Força máxima",0.0f,1000000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[2].force;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[2].force;},"linear_z_force",{"Translação Z","N",nullptr,jointAxisMotor<2>}},
  {"Frequência",0.001f,1000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[2].frequency;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[2].frequency;},"linear_z_frequency",{"Translação Z","Hz",nullptr,jointAxisPosition<2>}},
  {"Amortecimento",0.0f,10.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[2].damping;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[2].damping;},"linear_z_damping",{"Translação Z","",nullptr,jointAxisPosition<2>}},
  {"Limite mínimo",-180.0f,180.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[3].minimum;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[3].minimum;},"angular_x_minimum",{"Rotação X","°",nullptr,jointAxisLimited<3>}},
  {"Limite máximo",-180.0f,180.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[3].maximum;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[3].maximum;},"angular_x_maximum",{"Rotação X","°",nullptr,jointAxisLimited<3>}},
  {"Atrito máximo",0.0f,1000000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[3].friction;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[3].friction;},"angular_x_friction",{"Rotação X","Nm",nullptr,jointAxisFriction<3>}},
  {"Velocidade alvo",-1000.0f,1000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[3].velocity;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[3].velocity;},"angular_x_velocity",{"Rotação X","°/s",nullptr,jointAxisVelocity<3>}},
  {"Posição alvo",-180.0f,180.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[3].position;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[3].position;},"angular_x_position",{"Rotação X","°",nullptr,jointAxisPosition<3>}},
  {"Força máxima",0.0f,1000000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[3].force;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[3].force;},"angular_x_force",{"Rotação X","Nm",nullptr,jointAxisMotor<3>}},
  {"Frequência",0.001f,1000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[3].frequency;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[3].frequency;},"angular_x_frequency",{"Rotação X","Hz",nullptr,jointAxisPosition<3>}},
  {"Amortecimento",0.0f,10.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[3].damping;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[3].damping;},"angular_x_damping",{"Rotação X","",nullptr,jointAxisPosition<3>}},
  {"Limite mínimo",-180.0f,180.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[4].minimum;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[4].minimum;},"angular_y_minimum",{"Rotação Y","°",nullptr,jointAxisLimited<4>}},
  {"Limite máximo",-180.0f,180.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[4].maximum;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[4].maximum;},"angular_y_maximum",{"Rotação Y","°",nullptr,jointAxisLimited<4>}},
  {"Atrito máximo",0.0f,1000000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[4].friction;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[4].friction;},"angular_y_friction",{"Rotação Y","Nm",nullptr,jointAxisFriction<4>}},
  {"Velocidade alvo",-1000.0f,1000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[4].velocity;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[4].velocity;},"angular_y_velocity",{"Rotação Y","°/s",nullptr,jointAxisVelocity<4>}},
  {"Posição alvo",-180.0f,180.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[4].position;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[4].position;},"angular_y_position",{"Rotação Y","°",nullptr,jointAxisPosition<4>}},
  {"Força máxima",0.0f,1000000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[4].force;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[4].force;},"angular_y_force",{"Rotação Y","Nm",nullptr,jointAxisMotor<4>}},
  {"Frequência",0.001f,1000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[4].frequency;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[4].frequency;},"angular_y_frequency",{"Rotação Y","Hz",nullptr,jointAxisPosition<4>}},
  {"Amortecimento",0.0f,10.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[4].damping;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[4].damping;},"angular_y_damping",{"Rotação Y","",nullptr,jointAxisPosition<4>}},
  {"Limite mínimo",-180.0f,180.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[5].minimum;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[5].minimum;},"angular_z_minimum",{"Rotação Z","°",nullptr,jointAxisLimited<5>}},
  {"Limite máximo",-180.0f,180.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[5].maximum;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[5].maximum;},"angular_z_maximum",{"Rotação Z","°",nullptr,jointAxisLimited<5>}},
  {"Atrito máximo",0.0f,1000000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[5].friction;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[5].friction;},"angular_z_friction",{"Rotação Z","Nm",nullptr,jointAxisFriction<5>}},
  {"Velocidade alvo",-1000.0f,1000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[5].velocity;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[5].velocity;},"angular_z_velocity",{"Rotação Z","°/s",nullptr,jointAxisVelocity<5>}},
  {"Posição alvo",-180.0f,180.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[5].position;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[5].position;},"angular_z_position",{"Rotação Z","°",nullptr,jointAxisPosition<5>}},
  {"Força máxima",0.0f,1000000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[5].force;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[5].force;},"angular_z_force",{"Rotação Z","Nm",nullptr,jointAxisMotor<5>}},
  {"Frequência",0.001f,1000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[5].frequency;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[5].frequency;},"angular_z_frequency",{"Rotação Z","Hz",nullptr,jointAxisPosition<5>}},
  {"Amortecimento",0.0f,10.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint&>(v).axes[5].damping;},[](ComponentValue &v)->float*{return &static_cast<Joint&>(v).axes[5].damping;},"angular_z_damping",{"Rotação Z","",nullptr,jointAxisPosition<5>}}
}};
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
inline constexpr std::array<ComponentBoolean,1> jointBooleans{{
  {"enabled","Ativa",[](const ComponentValue &v){return static_cast<const Joint&>(v).enabled;},[](ComponentValue &v,bool b){static_cast<Joint&>(v).enabled=b;}}
}};
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
