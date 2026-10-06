#pragma once
#include "scene/components.h"
#include "scene/character.h"
#include "scene/dynamic_body_motor.h"
#include "scene/physics_body.h"
#include <array>

namespace ae::scene {
// The component owns presentation and a document reference, never interaction
// state or a second Transform authority. Its instanceId is local to its owner.
class UiCanvas final : public ComponentValue {
public:
  resources::AssetGuid document{};
  u64 inputReceiver=0,inputCamera=0;u32 movementSpace=0;
  bool enabled=true,occlusion=true;
  u32 mode=0;
  float offset[3]{},rotation[3]{},resolution[2]{800,600};
  float unitsPerPixel=.005f,order=0;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<UiCanvas>(*this);}
  bool valid() const override {
    if(mode>1||movementSpace>2)return false;
    for(const auto &p:descriptor.numbers){const auto n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}
    return std::floor(order)==order;
  }
  void write(std::ostream &out) const override {
    out<<document.high<<' '<<document.low<<' '<<enabled<<' '<<occlusion<<' '<<mode;
    for(const auto &p:descriptor.numbers)out<<' '<<p.read(*this);
    out<<' '<<inputReceiver<<' '<<inputCamera<<' '<<movementSpace;
  }
  bool read(std::istream &in,u32 version) override {
    if(version<1||version>2||!(in>>document.high>>document.low>>enabled>>occlusion>>mode))return false;
    for(const auto &p:descriptor.numbers)if(!(in>>*p.write(*this)))return false;
    inputReceiver=inputCamera=0;movementSpace=0;
    if(version>=2&&!(in>>inputReceiver>>inputCamera>>movementSpace))return false;
    return valid();
  }
};
inline bool uiCanvasWorld(const ComponentValue &v){return static_cast<const UiCanvas&>(v).mode==1;}
inline constexpr std::array<ComponentNumber,10> uiCanvasNumbers{{
  {"Posição X",-100000,100000,.01f,[](const ComponentValue &v)->const float&{return static_cast<const UiCanvas&>(v).offset[0];},[](ComponentValue &v){return &static_cast<UiCanvas&>(v).offset[0];},"offset_x",{"Apresentação","m","Aplica-se ao Canvas no mundo; Tela usa o viewport.",uiCanvasWorld}},
  {"Posição Y",-100000,100000,.01f,[](const ComponentValue &v)->const float&{return static_cast<const UiCanvas&>(v).offset[1];},[](ComponentValue &v){return &static_cast<UiCanvas&>(v).offset[1];},"offset_y",{"Apresentação","m","Aplica-se ao Canvas no mundo; Tela usa o viewport.",uiCanvasWorld}},
  {"Posição Z",-100000,100000,.01f,[](const ComponentValue &v)->const float&{return static_cast<const UiCanvas&>(v).offset[2];},[](ComponentValue &v){return &static_cast<UiCanvas&>(v).offset[2];},"offset_z",{"Apresentação","m","Aplica-se ao Canvas no mundo; Tela usa o viewport.",uiCanvasWorld}},
  {"Rotação X",-36000,36000,1,[](const ComponentValue &v)->const float&{return static_cast<const UiCanvas&>(v).rotation[0];},[](ComponentValue &v){return &static_cast<UiCanvas&>(v).rotation[0];},"rotation_x",{"Apresentação","°","Aplica-se ao Canvas no mundo; Tela usa o viewport.",uiCanvasWorld}},
  {"Rotação Y",-36000,36000,1,[](const ComponentValue &v)->const float&{return static_cast<const UiCanvas&>(v).rotation[1];},[](ComponentValue &v){return &static_cast<UiCanvas&>(v).rotation[1];},"rotation_y",{"Apresentação","°","Aplica-se ao Canvas no mundo; Tela usa o viewport.",uiCanvasWorld}},
  {"Rotação Z",-36000,36000,1,[](const ComponentValue &v)->const float&{return static_cast<const UiCanvas&>(v).rotation[2];},[](ComponentValue &v){return &static_cast<UiCanvas&>(v).rotation[2];},"rotation_z",{"Apresentação","°","Aplica-se ao Canvas no mundo; Tela usa o viewport.",uiCanvasWorld}},
  {"Largura",1,16384,1,[](const ComponentValue &v)->const float&{return static_cast<const UiCanvas&>(v).resolution[0];},[](ComponentValue &v){return &static_cast<UiCanvas&>(v).resolution[0];},"width",{"Canvas","px","Aplica-se ao Canvas no mundo; Tela usa o viewport.",uiCanvasWorld}},
  {"Altura",1,16384,1,[](const ComponentValue &v)->const float&{return static_cast<const UiCanvas&>(v).resolution[1];},[](ComponentValue &v){return &static_cast<UiCanvas&>(v).resolution[1];},"height",{"Canvas","px","Aplica-se ao Canvas no mundo; Tela usa o viewport.",uiCanvasWorld}},
  {"Unidades por pixel",.00001f,10,.001f,[](const ComponentValue &v)->const float&{return static_cast<const UiCanvas&>(v).unitsPerPixel;},[](ComponentValue &v){return &static_cast<UiCanvas&>(v).unitsPerPixel;},"units_per_pixel",{"Canvas","m/px","Aplica-se ao Canvas no mundo; Tela usa o viewport.",uiCanvasWorld}},
  {"Ordem",-10000,10000,1,[](const ComponentValue &v)->const float&{return static_cast<const UiCanvas&>(v).order;},[](ComponentValue &v){return &static_cast<UiCanvas&>(v).order;},"order",{"Canvas"}},
}};
inline constexpr std::array<ComponentBoolean,2> uiCanvasBooleans{{
  {"enabled","Habilitado",[](const ComponentValue &v){return static_cast<const UiCanvas&>(v).enabled;},[](ComponentValue &v,bool b){static_cast<UiCanvas&>(v).enabled=b;},{"Canvas"}},
  {"occlusion","Oclusão no mundo",[](const ComponentValue &v){return static_cast<const UiCanvas&>(v).occlusion;},[](ComponentValue &v,bool b){static_cast<UiCanvas&>(v).occlusion=b;},{"Canvas","","Depth e hit de Canvas no mundo.",uiCanvasWorld}},
}};
inline constexpr std::array<ComponentEnumOption,2> uiCanvasModes{{{0,"Tela"},{1,"Mundo / objeto"}}};
inline constexpr std::array<ComponentEnumOption,3> uiInputSpaces{{{0,"Mundo"},{1,"Local do jogador"},{2,"Camera de entrada"}}};
inline constexpr std::array<ComponentEnum,2> uiCanvasEnums{{
  {"mode","Apresentação",uiCanvasModes,[](const ComponentValue &v){return static_cast<const UiCanvas&>(v).mode;},[](ComponentValue &v,u32 m){static_cast<UiCanvas&>(v).mode=m;},{"Canvas"}},
  {"movement_space","Espaço do movimento",uiInputSpaces,[](const ComponentValue &v){return static_cast<const UiCanvas&>(v).movementSpace;},[](ComponentValue &v,u32 m){static_cast<UiCanvas&>(v).movementSpace=m;},{"Entrada"}}
}};
inline bool uiInputReceiverAccepts(const Components &components) {
  if(components.find(Character::descriptor))return true;
  const auto *value=components.find(DynamicBodyMotor::descriptor),*body=components.find(PhysicsBody::descriptor);
  return value&&body&&
    static_cast<const PhysicsBody&>(*body).motion==BodyMotion::Dynamic&&!static_cast<const PhysicsBody&>(*body).sensor;
}
inline constexpr std::array<ComponentObjectReference,2> uiCanvasReferences{{
  {"input_receiver","Jogador / receptor","",ObjectReferenceScope::Any,"Acoes globais",
   [](const ComponentValue &v){return static_cast<const UiCanvas&>(v).inputReceiver;},[](ComponentValue &v,u64 id){static_cast<UiCanvas&>(v).inputReceiver=id;},{"Entrada","","Character ou motor de corpo dinâmico ativo."},nullptr,uiInputReceiverAccepts},
  {"input_camera","Camera de entrada","astra.camera",ObjectReferenceScope::Any,"Nenhuma",
   [](const ComponentValue &v){return static_cast<const UiCanvas&>(v).inputCamera;},[](ComponentValue &v,u64 id){static_cast<UiCanvas&>(v).inputCamera=id;},{"Entrada"}}
}};
inline constexpr std::array<ComponentTriple,3> uiCanvasTriples{{
  {"offset","Deslocamento local",{"offset_x","offset_y","offset_z"}},
  {"rotation","Rotação local",{"rotation_x","rotation_y","rotation_z"}},
  {"resolution","Resolução",{"width","height",""},ComponentTripleKind::Vector2},
}};
inline constexpr std::array<ComponentResourceBinding,1> uiCanvasResources{{
  {"document","Documento UI",resources::AssetType::UiDocument,[](const ComponentValue&){return 1u;},
   [](const ComponentValue&v,u32){return static_cast<const UiCanvas&>(v).document;},
   [](ComponentValue&v,u32 slot,resources::AssetGuid g){if(slot)return false;static_cast<UiCanvas&>(v).document=g;return true;},{"Canvas"}}
}};
inline const ComponentType UiCanvas::descriptor{
  "astra.ui.canvas",2,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<UiCanvas>();},
  uiCanvasNumbers,uiCanvasBooleans,uiCanvasEnums,nullptr,true,uiCanvasReferences,uiCanvasTriples,uiCanvasResources
};
}
