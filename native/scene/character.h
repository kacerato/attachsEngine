#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
class Character final : public ComponentValue {
public:
  float radius=.45f,halfHeight=.55f,eyeHeight=1.65f,speed=8,slopeDegrees=45,jumpSpeed=5;
  float stepHeight=.4f,floorSnapLength=.5f,gravity=9.81f;
  bool inheritPlatformHorizontal=false;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Character>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {float v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return eyeHeight>radius;
  }
  void write(std::ostream &out) const override {for(const auto &p:descriptor.numbers) out<<p.read(*this)<<' ';out<<inheritPlatformHorizontal;}
  bool read(std::istream &in,u32 version) override {
    if(version<1 || version>4) return false;
    inheritPlatformHorizontal=false;jumpSpeed=5;stepHeight=.4f;floorSnapLength=.5f;gravity=9.81f;
    for(u32 i=0;i<(version==1?5u:version==2?6u:9u);++i) if(!(in>>*descriptor.numbers[i].write(*this))) return false;
    if(version>=4){u32 inherit=0;if(!(in>>inherit)||inherit>1)return false;inheritPlatformHorizontal=inherit!=0;}
    return valid();
  }
};
// A cápsula e a locomoção são grupos distintos pela mesma razão que o Character
// Controller da Unity separa a forma (Radius/Height/Center) do movimento
// (Slope Limit/Step Offset): mexer na forma invalida o contato, mexer na
// locomoção não.
inline constexpr std::array<ComponentNumber,9> characterNumbers{{
#define AE_CHAR_NUMBER(id,label,field,lo,hi,group,unit) {label,lo,hi,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Character&>(v).field;},[](ComponentValue &v)->float*{return &static_cast<Character&>(v).field;},id,{group,unit}}
  AE_CHAR_NUMBER("radius","Raio m",radius,.01f,10,"Cápsula","m"),
  AE_CHAR_NUMBER("half_height","Meia altura do cilindro m",halfHeight,.01f,10,"Cápsula","m"),
  AE_CHAR_NUMBER("eye_height","Altura dos olhos m",eyeHeight,.02f,20,"Cápsula","m"),
  AE_CHAR_NUMBER("speed","Velocidade m/s",speed,.01f,100,"Locomoção","m/s"),
  AE_CHAR_NUMBER("slope_degrees","Inclinação máxima graus",slopeDegrees,1,89,"Locomoção","°"),
  AE_CHAR_NUMBER("jump_speed","Velocidade do salto m/s",jumpSpeed,0,100,"Locomoção","m/s"),
  AE_CHAR_NUMBER("step_height","Altura do degrau",stepHeight,0,10,"Chão","m"),
  AE_CHAR_NUMBER("floor_snap_length","Aderência ao chão",floorSnapLength,0,10,"Chão","m"),
  AE_CHAR_NUMBER("gravity","Gravidade",gravity,0,1000,"Locomoção","m/s²")
#undef AE_CHAR_NUMBER
}};
// These fields update the existing motor, without replacing its capsule/velocity.
inline constexpr std::array<ComponentBoolean,1> characterBooleans{{
  {"inherit_platform_horizontal","Impulso ao sair",[](const ComponentValue&v){return static_cast<const Character&>(v).inheritPlatformHorizontal;},[](ComponentValue&v,bool b){static_cast<Character&>(v).inheritPlatformHorizontal=b;},{"Chão","","Conserva X/Z da superfície no ar; desligar remove apenas esse impulso, sem recriar a cápsula."}}
}};
inline bool characterMotionProperty(std::string_view id) {return id=="speed"||id=="jump_speed"||id=="step_height"||id=="floor_snap_length"||id=="gravity"||id=="inherit_platform_horizontal";}
inline const ComponentType Character::descriptor{
  "astra.physics.character",4,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Character>();},characterNumbers,characterBooleans
};
}
