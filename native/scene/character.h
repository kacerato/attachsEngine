#pragma once
#include "scene/motor_control.h"
#include "scene/components.h"
#include <array>
namespace ae::scene {
class Character final : public ComponentValue {
public:
  MotorControlPolicy control{};

#include "scene/generated/character_Character_fields0.inc"

#include "scene/generated/character_Character_fields1.inc"

#include "scene/generated/character_Character_fields2.inc"

  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Character>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {float v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return eyeHeight>radius&&u32(control.source)<=5;
  }
  void write(std::ostream &out) const override {for(const auto &p:descriptor.numbers) out<<p.read(*this)<<' ';out<<inheritPlatformHorizontal<<' '<<u32(control.source);}
  bool read(std::istream &in,u32 version) override {
    if(version<1 || version>5) return false;
    control={};
    inheritPlatformHorizontal=false;jumpSpeed=5;stepHeight=.4f;floorSnapLength=.5f;gravity=9.81f;
    for(u32 i=0;i<(version==1?5u:version==2?6u:version<5?9u:descriptor.numbers.size());++i) if(!(in>>*descriptor.numbers[i].write(*this))) return false;
    if(version>=4){u32 inherit=0;if(!(in>>inherit)||inherit>1)return false;inheritPlatformHorizontal=inherit!=0;}
    if(version==5){u32 source=0;if(!(in>>source)||source>5)return false;control.source=MotorControlSource(source);}
    return valid();
  }
};
// A cápsula e a locomoção são grupos distintos pela mesma razão que o Character
// Controller da Unity separa a forma (Radius/Height/Center) do movimento
// (Slope Limit/Step Offset): mexer na forma invalida o contato, mexer na
// locomoção não.
#include "scene/generated/character_characterNumbers.inc"
// These fields update the existing motor, without replacing its capsule/velocity.
#include "scene/generated/character_characterBooleans.inc"
#include "scene/generated/character_characterEnums.inc"
inline bool characterMotionProperty(std::string_view id) {return id=="speed"||id=="jump_speed"||id=="step_height"||id=="floor_snap_length"||id=="gravity"||id=="inherit_platform_horizontal";}
// Unity 6000.0 CharacterController.OnControllerColliderHit: o movimento bateu
// num corpo. Uma vez por objeto tocado em cada passo físico com colisão real.
inline constexpr std::array<ComponentParameter,3> characterHitPayload{{
  {"other","Objeto tocado",ComponentValueKind::Object},{"point","Ponto",ComponentValueKind::Vector3,"m"},{"normal","Normal",ComponentValueKind::Vector3}}};
inline constexpr std::array<ComponentEvent,1> characterEvents{{
  {"collider_hit","Bateu num colisor","O movimento do personagem colidiu com um corpo; carrega o objeto, o ponto e a normal",characterHitPayload}
}};
inline const ComponentType Character::descriptor{
  "astra.physics.character",5,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Character>();},characterNumbers,characterBooleans,
  characterEnums,nullptr,false,{},{},{},{},{},{},{},characterEvents
};
}
