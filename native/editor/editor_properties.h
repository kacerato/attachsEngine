#pragma once
#include "editor/editor_document.h"
#include <array>
#include <cstddef>
#include <cmath>
#include <type_traits>
namespace ae::editor {
static_assert(std::is_standard_layout_v<EditorEntity>);
enum class EditorPropertyGroup { Transform, Material, Environment, Water, WaterBody, Physics, Route };
struct EditorNumericProperty {
  const char *name;
  usize offset;
  float minimum,maximum,dragStep;
  EditorPropertyGroup group;
};
// Stable order is also the numeric widget ID contract. Shared by Inspector,
// input validation and archive v2; append properties instead of reordering.
inline constexpr std::array<EditorNumericProperty,67> editorBaseNumericProperties{{
#define AE_TRANS(name,member,axis,lo,hi,step) {name,offsetof(EditorEntity,transform)+offsetof(EditorTransform,member)+sizeof(float)*axis,lo,hi,step,EditorPropertyGroup::Transform}
  AE_TRANS("Posição X",position,0,-1e7f,1e7f,.02f), AE_TRANS("Posição Y",position,1,-1e7f,1e7f,.02f), AE_TRANS("Posição Z",position,2,-1e7f,1e7f,.02f),
  AE_TRANS("Rotação X",rotationDegrees,0,-1e7f,1e7f,1), AE_TRANS("Rotação Y",rotationDegrees,1,-1e7f,1e7f,1), AE_TRANS("Rotação Z",rotationDegrees,2,-1e7f,1e7f,1),
  AE_TRANS("Escala X",scale,0,.001f,1e5f,.01f), AE_TRANS("Escala Y",scale,1,.001f,1e5f,.01f), AE_TRANS("Escala Z",scale,2,.001f,1e5f,.01f),
#undef AE_TRANS
#define AE_MAT(name,member,axis,lo,hi,step) {name,offsetof(EditorEntity,material)+offsetof(renderer::MaterialOverride,member)+sizeof(float)*axis,lo,hi,step,EditorPropertyGroup::Material}
  AE_MAT("Cor R",baseColor,0,0,1,.01f),AE_MAT("Cor G",baseColor,1,0,1,.01f),AE_MAT("Cor B",baseColor,2,0,1,.01f),
  AE_MAT("Rugosidade",roughness,0,0,1,.01f),AE_MAT("Metálico",metallic,0,0,1,.01f),AE_MAT("Intensidade da normal",normalScale,0,0,16,.02f),AE_MAT("Especular",specular,0,0,1,.01f),
  AE_MAT("Emissão R",emission,0,0,1,.01f),AE_MAT("Emissão G",emission,1,0,1,.01f),AE_MAT("Emissão B",emission,2,0,1,.01f),AE_MAT("Emissão potência",emissionStrength,0,0,10000,.1f),
#undef AE_MAT
  {"Sol potência x",offsetof(EditorEntity,environment),0,100,.02f,EditorPropertyGroup::Environment},
  {"Ambiente potência x",offsetof(EditorEntity,environment)+sizeof(float),0,100,.02f,EditorPropertyGroup::Environment},
  {"Exposição x",offsetof(EditorEntity,environment)+2*sizeof(float),.001f,100,.02f,EditorPropertyGroup::Environment},
  {"Rotação do céu",offsetof(EditorEntity,environment)+3*sizeof(float),-36000,36000,1,EditorPropertyGroup::Environment},
#define AE_WATER(name,axis,lo,hi,step) {name,offsetof(EditorEntity,water)+sizeof(float)*axis,lo,hi,step,EditorPropertyGroup::Water}
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
#define AE_LAYOUT(name,axis,lo,hi,step) {name,offsetof(EditorEntity,waterLayout)+sizeof(float)*axis,lo,hi,step,EditorPropertyGroup::Water}
  AE_LAYOUT("Número de cascatas",0,1,4,1), AE_LAYOUT("Tamanho FFT log2",1,5,8,1),
  AE_LAYOUT("Menor onda m",2,.01f,99999,.1f), AE_LAYOUT("Maior onda m",3,.02f,100000,10),
  AE_LAYOUT("Semente espectral",4,0,1000000,1), AE_LAYOUT("Escala do domínio",5,1,8,.1f),
  AE_LAYOUT("Orçamento FFT MiB",6,1,32,1), AE_LAYOUT("Faixa 4 amplitude",7,0,3,.01f),
  AE_LAYOUT("Faixa 4 crista",8,0,4,.01f)
#undef AE_LAYOUT
}};
inline constexpr u32 WaterBodyPropertyBase=67, PhysicsPropertyBase=74, RoutePropertyBase=79;
inline constexpr auto editorNumericProperties=[] {
  std::array<EditorNumericProperty,RoutePropertyBase+renderer::MaximumWaterRoutePoints*8> result{};
  for(u32 i=0;i<editorBaseNumericProperties.size();++i) result[i]=editorBaseNumericProperties[i];
  const char *waterNames[]{"Profundidade m","Corrente X m/s","Corrente Z m/s","Camada de ondas","Camada de espuma","Camada de perturbações","Camada óptica"};
  for(u32 i=0;i<7;++i) result[67+i]={waterNames[i],offsetof(EditorEntity,waterBody)+i*sizeof(float),i==0?.1f:i<3?-100.0f:0.0f,i==0?10000.0f:i<3?100.0f:4.0f,.1f,EditorPropertyGroup::WaterBody};
  const char *bodyNames[]{"Massa kg","Arrasto do fluido","Meia extensão do colisor X","Meia extensão do colisor Y","Meia extensão do colisor Z"};
  for(u32 i=0;i<5;++i) result[74+i]={bodyNames[i],offsetof(EditorEntity,rigidBody)+i*sizeof(float),i==1?0.0f:.01f,i==0?1000000.0f:i==1?100.0f:10000.0f,.1f,EditorPropertyGroup::Physics};
  const char *pointNames[]{"Posição X","Posição Y","Posição Z","Largura m","Profundidade m","Fluxo m/s","Ganho de espuma","Tensão da curva"};
  for(u32 point=0;point<renderer::MaximumWaterRoutePoints;++point) for(u32 field=0;field<8;++field)
    result[RoutePropertyBase+point*8+field]={pointNames[field],offsetof(EditorEntity,route)+offsetof(renderer::WaterRoute,points)+point*sizeof(renderer::WaterRoutePoint)+field*sizeof(float),field<3?-1e7f:field==5?-100.0f:field==3||field==4?.1f:0.0f,field<3?1e7f:field==3||field==4?10000.0f:field==5?100.0f:field==6?10.0f:1.0f,field<3?.1f:.05f,EditorPropertyGroup::Route};
  return result;
}();
inline float editorPropertyValue(const EditorEntity &entity,u32 index) {
  if(index>=editorNumericProperties.size()) return 0;
  return *reinterpret_cast<const float *>(reinterpret_cast<const unsigned char *>(&entity)+editorNumericProperties[index].offset);
}
inline bool setEditorPropertyValue(EditorEntity &entity,u32 index,float value) {
  if(index>=editorNumericProperties.size()) return false;
  const auto &property=editorNumericProperties[index];
  if(!std::isfinite(value) || value<property.minimum || value>property.maximum) return false;
  if((index==58 || index==59 || index==62) && value!=std::floor(value)) return false;
  *reinterpret_cast<float *>(reinterpret_cast<unsigned char *>(&entity)+property.offset)=value;
  if(property.group==EditorPropertyGroup::Material) entity.material.enabled=true;
  if(property.group==EditorPropertyGroup::Water) entity.waterEnabled=true;
  if(index>=37 && index<67) entity.waterSpectrumEnabled=true;
  if(index>=58 && index<67) entity.waterLayoutEnabled=true;
  return true;
}
inline bool validEditorAppearance(const EditorEntity &entity) {
  if(entity.route.count && !renderer::validateWaterRoute(entity.route)) return false;
  if(entity.waterLayout[2]>=entity.waterLayout[3] ||
     entity.waterLayout[0]!=std::floor(entity.waterLayout[0]) ||
     entity.waterLayout[1]!=std::floor(entity.waterLayout[1]) ||
     entity.waterLayout[4]!=std::floor(entity.waterLayout[4])) return false;
  for(u32 i=9;i<editorNumericProperties.size();++i) {
    const float value=editorPropertyValue(entity,i);const auto &property=editorNumericProperties[i];
    if(!std::isfinite(value) || value<property.minimum || value>property.maximum) return false;
  }
  return true;
}
}
