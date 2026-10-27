#pragma once
#include "scene/motor_control.h"
#include "scene/components.h"
#include <array>
namespace ae::scene {
class Character final : public ComponentValue {
public:
  MotorControlPolicy control{};
  // 0 solo, 1 voo, 2 nado (dentro de volume de agua; solo fora dele), 3 dirigir.
  u32 movementMode=0;

#include "scene/generated/character_Character_fields0.inc"

#include "scene/generated/character_Character_fields1.inc"

#include "scene/generated/character_Character_fields2.inc"

  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Character>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {float v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return eyeHeight>radius&&u32(control.source)<=5&&movementMode<=3;
  }
  void write(std::ostream &out) const override {for(const auto &p:descriptor.numbers) out<<p.read(*this)<<' ';out<<inheritPlatformHorizontal<<' '<<u32(control.source)<<' '<<movementMode<<' '<<faceMovement<<' '<<cameraRelative;}
  bool read(std::istream &in,u32 version) override {
    if(version<1 || version>7) return false;
    control={};movementMode=0;acceleration=0;braking=0;airControl=1;swimSpeed=.6f;turnRate=90;
    faceTurnSpeed=720;rotationSmoothTime=.12f;faceMovement=false;cameraRelative=true;
    inheritPlatformHorizontal=false;jumpSpeed=5;stepHeight=.4f;floorSnapLength=.5f;gravity=9.81f;
    // v5 gravava 14 numeros; v6 acrescenta aceleracao, frenagem, controle no ar e nado;
    // v7, a direcao (velocidade e suavizacao do giro, girar para o movimento, relativo a camera).
    for(u32 i=0;i<(version==1?5u:version==2?6u:version<5?9u:version==5?14u:version==6?19u:descriptor.numbers.size());++i) if(!(in>>*descriptor.numbers[i].write(*this))) return false;
    if(version>=4){u32 inherit=0;if(!(in>>inherit)||inherit>1)return false;inheritPlatformHorizontal=inherit!=0;}
    if(version>=5){u32 source=0;if(!(in>>source)||source>5)return false;control.source=MotorControlSource(source);}
    if(version>=6&&!(in>>movementMode))return false;
    if(version>=7){u32 face=0,relative=0;if(!(in>>face>>relative)||face>1||relative>1)return false;faceMovement=face!=0;cameraRelative=relative!=0;}
    return valid();
  }
};
// A cápsula e a locomoção são grupos distintos pela mesma razão que o Character
// Controller da Unity separa a forma (Radius/Height/Center) do movimento
// (Slope Limit/Step Offset): mexer na forma invalida o contato, mexer na
// locomoção não.
// Direcao (Starter Assets ThirdPersonController: RotationSmoothTime). Os campos
// de giro so aparecem com "Girar para a direcao do movimento" ligado.
inline bool characterFacesMovement(const ComponentValue &value) {return static_cast<const Character&>(value).faceMovement;}
inline constexpr std::array<ComponentEnumOption,4> characterMovementModes{{{0,"Solo"},{1,"Voo"},{2,"Nado na água"},{3,"Dirigir"}}};
#include "scene/generated/character_characterNumbers.inc"
// These fields update the existing motor, without replacing its capsule/velocity.
#include "scene/generated/character_characterBooleans.inc"
#include "scene/generated/character_characterEnums.inc"
inline bool characterMotionProperty(std::string_view id) {return id=="speed"||id=="jump_speed"||id=="step_height"||id=="floor_snap_length"||id=="gravity"||id=="inherit_platform_horizontal"||
  id=="acceleration"||id=="braking"||id=="air_control"||id=="swim_speed"||id=="turn_rate"||id=="movement_mode"||
  id=="face_movement"||id=="camera_relative"||id=="face_turn_speed"||id=="rotation_smooth_time";}
// Comandos do motor em Play: agachar (recusa levantar sem espaco acima) e eixo
// vertical para voo e nado (-1 desce, 1 sobe).
inline constexpr std::array<ComponentParameter,1> characterCrouchParameters{{{"crouching","Agachado",ComponentValueKind::Boolean}}};
inline constexpr std::array<ComponentParameter,1> characterVerticalParameters{{{"axis","Eixo vertical",ComponentValueKind::Number}}};
inline constexpr std::array<ComponentMethod,3> characterMethods{{
  {"crouch","Agachar","Troca para a cápsula baixa; levantar falha sem espaço acima",characterCrouchParameters,ComponentValueKind::Boolean},
  {"set_vertical","Subir/descer","Eixo vertical do voo e do nado neste quadro (-1 a 1)",characterVerticalParameters},
  {"in_water","Na água","Verdadeiro enquanto o nado está em vigor",{},ComponentValueKind::Boolean}}};
// Unity 6000.0 CharacterController.OnControllerColliderHit: o movimento bateu
// num corpo. Uma vez por objeto tocado em cada passo físico com colisão real.
inline constexpr std::array<ComponentParameter,3> characterHitPayload{{
  {"other","Objeto tocado",ComponentValueKind::Object},{"point","Ponto",ComponentValueKind::Vector3,"m"},{"normal","Normal",ComponentValueKind::Vector3}}};
inline constexpr std::array<ComponentEvent,1> characterEvents{{
  {"collider_hit","Bateu num colisor","O movimento do personagem colidiu com um corpo; carrega o objeto, o ponto e a normal",characterHitPayload}
}};
inline const ComponentType Character::descriptor{
  "astra.physics.character",7,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Character>();},characterNumbers,characterBooleans,
  characterEnums,nullptr,false,{},{},{},{},{},{},characterMethods,characterEvents
};
}
