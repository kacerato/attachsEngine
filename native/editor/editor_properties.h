#pragma once
#include "editor/editor_document.h"
#include "editor/editor_physics_body.h"
#include "editor/editor_character.h"
#include "editor/editor_camera_look.h"
#include "scene/component_properties.h"
#include "editor/editor_route_component.h"
#include "editor/editor_water_body_component.h"
#include "editor/editor_water_settings_component.h"
#include <array>
#include <cmath>
#include <type_traits>
namespace ae::editor {
enum class EditorPropertyGroup { Transform, Material, Environment, Water, WaterBody, Physics, Route, ScenePhysics, Character, CameraLook, Collider };
struct EditorNumericProperty {
  const char *name;
  const float &(*read)(const EditorEntity &,u32);
  float *(*write)(EditorEntity &,u32);
  u32 slot;
  float minimum,maximum,dragStep;
  EditorPropertyGroup group;
  const EditorComponentType *component=nullptr;
  std::string_view propertyId{};
};
// Stable order is also the numeric widget ID contract. Shared by Inspector,
// input validation and archives v2-v7; append properties instead of reordering.
// Accessors address typed members, never the object representation. This keeps
// property IDs independent of the ownership/layout of future optional components.
#define AE_ACCESS(expression) [](const EditorEntity &e,u32 slot) -> const float & { (void)slot; return expression; }, [](EditorEntity &e,u32 slot) -> float * { (void)slot; return &(expression); }
inline constexpr std::array<EditorNumericProperty,67> editorBaseNumericProperties{{
#define AE_TRANS(name,member,axis,lo,hi,step) {name,AE_ACCESS(e.transform.member[slot]),axis,lo,hi,step,EditorPropertyGroup::Transform}
  AE_TRANS("Posição X",position,0,-1e7f,1e7f,.02f), AE_TRANS("Posição Y",position,1,-1e7f,1e7f,.02f), AE_TRANS("Posição Z",position,2,-1e7f,1e7f,.02f),
  AE_TRANS("Rotação X",rotationDegrees,0,-1e7f,1e7f,1), AE_TRANS("Rotação Y",rotationDegrees,1,-1e7f,1e7f,1), AE_TRANS("Rotação Z",rotationDegrees,2,-1e7f,1e7f,1),
  AE_TRANS("Escala X",scale,0,.001f,1e5f,.01f), AE_TRANS("Escala Y",scale,1,.001f,1e5f,.01f), AE_TRANS("Escala Z",scale,2,.001f,1e5f,.01f),
#undef AE_TRANS
#define AE_MAT(name,member,axis,lo,hi,step) {name,[](const EditorEntity &e,u32)->const float& {return meshMaterial(e).member;},[](EditorEntity &e,u32)->float* {auto *m=editMeshRenderer(e);return m?&m->material.member:nullptr;},0,lo,hi,step,EditorPropertyGroup::Material}
  AE_MAT("Cor R",baseColor[0],0,0,1,.01f),AE_MAT("Cor G",baseColor[1],0,0,1,.01f),AE_MAT("Cor B",baseColor[2],0,0,1,.01f),
  AE_MAT("Rugosidade",roughness,0,0,1,.01f),AE_MAT("Metálico",metallic,0,0,1,.01f),AE_MAT("Intensidade da normal",normalScale,0,0,16,.02f),AE_MAT("Especular",specular,0,0,1,.01f),
  AE_MAT("Emissão R",emission[0],0,0,1,.01f),AE_MAT("Emissão G",emission[1],0,0,1,.01f),AE_MAT("Emissão B",emission[2],0,0,1,.01f),AE_MAT("Emissão potência",emissionStrength,0,0,10000,.1f),
#undef AE_MAT
  {"Sol potência x",AE_ACCESS(e.environment[slot]),0,0,100,.02f,EditorPropertyGroup::Environment},
  {"Ambiente potência x",AE_ACCESS(e.environment[slot]),1,0,100,.02f,EditorPropertyGroup::Environment},
  {"Exposição x",AE_ACCESS(e.environment[slot]),2,.001f,100,.02f,EditorPropertyGroup::Environment},
  {"Rotação do céu",AE_ACCESS(e.environment[slot]),3,-36000,36000,1,EditorPropertyGroup::Environment},
#define AE_WATER_ACCESS(offset) [](const EditorEntity &e,u32 slot)->const float& {return waterSettings(e).legacyField(offset+slot);}, [](EditorEntity &e,u32 slot)->float* {auto *settings=editWaterSettings(e);return settings?&settings->legacyField(offset+slot):nullptr;}
#define AE_WATER(name,axis,lo,hi,step) {name,AE_WATER_ACCESS(0),axis,lo,hi,step,EditorPropertyGroup::Water}
  AE_WATER("Altura das ondas",0,0,10,.02f),AE_WATER("Velocidade das ondas",1,0,5,.02f),
  AE_WATER("Inclinação",2,0,2,.01f),AE_WATER("Micro-ondas",3,0,8,.02f),
  AE_WATER("Opacidade",4,0,1,.01f),AE_WATER("Absorção",5,0,10,.02f),
  AE_WATER("Espuma",6,0,5,.02f),AE_WATER("Rugosidade da água",7,.01f,1,.01f),
  AE_WATER("Turbidez",8,0,1,.01f),AE_WATER("IOR",9,1,2,.01f),
  AE_WATER("Direção das ondas",10,-36000,36000,1),AE_WATER("Nível da água",11,-10000,10000,.1f),
  AE_WATER("Densidade do fluido",12,1,20000,10),
  AE_WATER("Vento m/s",13,0,100,.1f), AE_WATER("Pista de vento m",14,1,10000000,1000),
  AE_WATER("Profundidade m",15,.1f,10000,1), AE_WATER("Ondulação",16,0,2,.01f),
  AE_WATER("Dispersão",17,0,1,.01f), AE_WATER("Amortecimento curto",18,0,100,.01f),
  AE_WATER("Vento cruzado m/s",19,0,100,.1f), AE_WATER("Direção cruzada",20,-36000,36000,1),
  AE_WATER("Pista cruzada m",21,1,10000000,1000), AE_WATER("Ondulação cruzada",22,0,2,.01f),
  AE_WATER("Dispersão cruzada",23,0,1,.01f), AE_WATER("Peso cruzado",24,0,1,.01f),
  AE_WATER("Faixa 1 amplitude",25,0,3,.01f), AE_WATER("Faixa 2 amplitude",26,0,3,.01f),
  AE_WATER("Faixa 3 amplitude",27,0,3,.01f), AE_WATER("Faixa 1 crista",28,0,4,.01f),
  AE_WATER("Faixa 2 crista",29,0,4,.01f), AE_WATER("Faixa 3 crista",30,0,4,.01f),
  AE_WATER("Espuma limiar",31,0,2,.01f), AE_WATER("Espuma crescimento",32,0,100,.1f),
  AE_WATER("Espuma dissipação",33,0,100,.1f),
#undef AE_WATER
#define AE_LAYOUT(name,axis,lo,hi,step) {name,AE_WATER_ACCESS(34),axis,lo,hi,step,EditorPropertyGroup::Water}
  AE_LAYOUT("Número de cascatas",0,1,4,1), AE_LAYOUT("Tamanho FFT log2",1,5,8,1),
  AE_LAYOUT("Menor onda m",2,.01f,99999,.1f), AE_LAYOUT("Maior onda m",3,.02f,100000,10),
  AE_LAYOUT("Semente espectral",4,0,1000000,1), AE_LAYOUT("Escala do domínio",5,1,8,.1f),
  AE_LAYOUT("Orçamento FFT MiB",6,1,32,1), AE_LAYOUT("Faixa 4 amplitude",7,0,3,.01f),
  AE_LAYOUT("Faixa 4 crista",8,0,4,.01f)
#undef AE_LAYOUT
#undef AE_WATER_ACCESS
}};
inline constexpr u32 WaterBodyPropertyBase=67, PhysicsPropertyBase=74, RoutePropertyBase=79;
// Named fields avoid assuming padding-free WaterRoutePoint layout.
template<class Route>
inline auto &editorRouteField(Route &route,u32 slot) {
  auto &point=route.points[slot/8];
  switch(slot%8) {
  case 0: return point.position[0];
  case 1: return point.position[1];
  case 2: return point.position[2];
  case 3: return point.width;
  case 4: return point.depth;
  case 5: return point.speed;
  case 6: return point.foam;
  default: return point.tension;
  }
}
inline constexpr auto editorNumericProperties=[] {
  std::array<EditorNumericProperty,RoutePropertyBase+renderer::MaximumWaterRoutePoints*8+23> result{};
  for(u32 i=0;i<cameraLookNumbers.size();++i) {
    const auto &p=cameraLookNumbers[i];
    result[RoutePropertyBase+renderer::MaximumWaterRoutePoints*8+12+i]={p.name,
      [](const EditorEntity &e,u32 slot)->const float& {static const EditorCameraLook defaults;const auto *c=cameraLook(e);return cameraLookNumbers[slot].read(c?*c:defaults);},
      [](EditorEntity &e,u32 slot)->float* {auto *c=editCameraLook(e);return c?cameraLookNumbers[slot].write(*c):nullptr;},
      i,p.minimum,p.maximum,p.dragStep,EditorPropertyGroup::CameraLook,&EditorCameraLook::descriptor,p.id};
  }
  for(u32 i=0;i<characterNumbers.size();++i) {
    const auto &p=characterNumbers[i];
    result[RoutePropertyBase+renderer::MaximumWaterRoutePoints*8+6+i]={p.name,
      [](const EditorEntity &e,u32 slot)->const float& {static const EditorCharacter defaults;const auto *c=characterComponent(e);return characterNumbers[slot].read(c?*c:defaults);},
      [](EditorEntity &e,u32 slot)->float* {auto *c=editCharacter(e);return c?characterNumbers[slot].write(*c):nullptr;},
      i,p.minimum,p.maximum,p.dragStep,EditorPropertyGroup::Character,&EditorCharacter::descriptor,p.id};
  }
  for(u32 i=0;i<6;++i) {
    const auto &p=physicsBodyNumbers[i];
    constexpr u32 slots[]{207,211,212,222,223,224};
    result[slots[i]]={p.name,
      [](const EditorEntity &e,u32 slot)->const float& {static const EditorPhysicsBody defaults;const auto *body=physicsBody(e);return physicsBodyNumbers[slot].read(body?*body:defaults);},
      [](EditorEntity &e,u32 slot)->float* {auto *body=editPhysicsBody(e);return body?physicsBodyNumbers[slot].write(*body):nullptr;},
      i,p.minimum,p.maximum,p.dragStep,EditorPropertyGroup::ScenePhysics,&EditorPhysicsBody::descriptor,p.id};
  }
  for(u32 i=0;i<8;++i) {
    const auto &p=colliderNumbers[i];
    constexpr u32 slots[]{208,209,210,225,226,227,228,229};
    result[slots[i]]={p.name,
      [](const EditorEntity &e,u32 slot)->const float& {static const EditorCollider defaults;const auto *c=colliderComponent(e);return colliderNumbers[slot].read(c?*c:defaults);},
      [](EditorEntity &e,u32 slot)->float* {auto *c=editCollider(e);return c?colliderNumbers[slot].write(*c):nullptr;},
      i,p.minimum,p.maximum,p.dragStep,EditorPropertyGroup::Collider,&EditorCollider::descriptor,p.id};
  }
  for(u32 i=0;i<editorBaseNumericProperties.size();++i) result[i]=editorBaseNumericProperties[i];
  for(u32 i=0;i<waterBodyNumbers.size();++i) {
    const auto &p=waterBodyNumbers[i];
    result[67+i]={p.name,
      [](const EditorEntity &e,u32 slot) -> const float & { return waterBodyNumbers[slot].read(waterBody(e)); },
      [](EditorEntity &e,u32 slot) -> float * { auto *body=editWaterBody(e);return body ? waterBodyNumbers[slot].write(*body) : nullptr; },
      i,p.minimum,p.maximum,p.dragStep,EditorPropertyGroup::WaterBody};
  }
  const char *bodyNames[]{"Massa kg","Arrasto do fluido","Meia extensão do colisor X","Meia extensão do colisor Y","Meia extensão do colisor Z"};
  for(u32 i=0;i<5;++i) result[74+i]={bodyNames[i],AE_ACCESS(e.rigidBody[slot]),i,i==1?0.0f:.01f,i==0?1000000.0f:i==1?100.0f:10000.0f,.1f,EditorPropertyGroup::Physics};
  const char *pointNames[]{"Posição X","Posição Y","Posição Z","Largura m","Profundidade m","Fluxo m/s","Ganho de espuma","Tensão da curva"};
  for(u32 point=0;point<renderer::MaximumWaterRoutePoints;++point) for(u32 field=0;field<8;++field)
    result[RoutePropertyBase+point*8+field]={pointNames[field],[](const EditorEntity &e,u32 slot) -> const float & { return editorRouteField(waterRoute(e),slot); }, [](EditorEntity &e,u32 slot) -> float * { auto *route=editWaterRoute(e);return route ? &editorRouteField(*route,slot) : nullptr; },point*8+field,field<3?-1e7f:field==5?-100.0f:field==3||field==4?.1f:0.0f,field<3?1e7f:field==3||field==4?10000.0f:field==5?100.0f:field==6?10.0f:1.0f,field<3?.1f:.05f,EditorPropertyGroup::Route};
  return result;
}();
#undef AE_ACCESS
// A condição de aparecer é DECLARADA no descritor da propriedade, junto com o
// domínio e a unidade. Antes ela estava reescrita aqui em função do índice do
// widget — uma segunda cópia da mesma regra, que divergia da API assim que uma
// forma nova de colisor ou um modo novo de corpo entrasse só de um lado.
inline bool editorPropertyVisible(const EditorEntity &entity,u32 index) {
  const auto &p=editorNumericProperties[index];
  if(p.component) {
    const auto *value=entity.components.find(p.component->id);
    if(!value) return false;
    for(const auto &number:p.component->numbers)
      if(number.id==p.propertyId) return number.presentation.isVisible(*value);
  }
  return true;
}
inline float editorPropertyValue(const EditorEntity &entity,u32 index) {
  if(index>=editorNumericProperties.size()) return 0;
  const auto &property=editorNumericProperties[index];
  return property.read(entity,property.slot);
}
inline bool setEditorPropertyValue(EditorEntity &entity,u32 index,float value) {
  if(index>=editorNumericProperties.size()) return false;
  const auto &property=editorNumericProperties[index];
  if(!std::isfinite(value) || value<property.minimum || value>property.maximum) return false;
  if(property.component) {
    // Numeric widget indices remain a legacy UI adapter, not the public field ID.
    auto candidate=entity.components;
    if(!candidate.edit(*property.component) ||
       scene::setComponentProperty(candidate,property.component->id,property.propertyId,value)!=scene::ComponentPropertyStatus::Applied) return false;
    entity.components=std::move(candidate);return true;
  }
  if((index==58 || index==59 || index==62) && value!=std::floor(value)) return false;
  // Legacy archives contain default values for every route point. Reading
  // those defaults must not allocate a payload on a dry entity.
  if((property.group!=EditorPropertyGroup::Route && property.group!=EditorPropertyGroup::WaterBody) || property.read(entity,property.slot)!=value)
  {
    auto *destination=property.write(entity,property.slot);
    if(!destination) return false;
    *destination=value;
  }
  if(property.group==EditorPropertyGroup::Material) {auto *m=editMeshRenderer(entity);if(!m) return false;m->material.enabled=true;}
  if(property.group==EditorPropertyGroup::Water) {
    auto *settings=editWaterSettings(entity);if(!settings) return false;
    settings->enabled=true;
    if(index>=37 && index<67) settings->spectrumEnabled=true;
    if(index>=58 && index<67) settings->layoutEnabled=true;
  }
  return true;
}
inline bool validEditorAppearance(const EditorEntity &entity) {
  if(!entity.components.valid()) return false;
  const auto &settings=waterSettings(entity);
  if(settings.minimumWavelength>=settings.maximumWavelength ||
     settings.cascadeCount!=std::floor(settings.cascadeCount) ||
     settings.resolutionLog2!=std::floor(settings.resolutionLog2) ||
     settings.seed!=std::floor(settings.seed)) return false;
  for(u32 i=9;i<editorNumericProperties.size();++i) {
    const float value=editorPropertyValue(entity,i);const auto &property=editorNumericProperties[i];
    if(!std::isfinite(value) || value<property.minimum || value>property.maximum) return false;
  }
  return true;
}
}
