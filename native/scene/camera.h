#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
enum class CameraProjection : u32 { Perspective, Orthographic };
class Camera final : public ComponentValue {
public:
  bool enabled=true;
  float verticalFov=60,nearPlane=.1f,farPlane=2000,priority=0;
  CameraProjection projection=CameraProjection::Perspective;
  float orthographicHalfHeight=5;
  u32 environmentMask=~0u;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Camera>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {const auto v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    const bool maskValid=environmentMask==~0u || (environmentMask && !(environmentMask&(environmentMask-1)) && environmentMask<=128u);
    return farPlane>nearPlane && priority==std::floor(priority) && static_cast<u32>(projection)<=1 && maskValid;
  }
  void write(std::ostream &out) const override {out<<enabled<<' ';for(const auto &p:descriptor.numbers) out<<p.read(*this)<<' ';out<<static_cast<u32>(projection)<<' '<<environmentMask<<' ';}
  bool read(std::istream &in,u32 version) override {
    if((version<1||version>3)||!(in>>enabled>>verticalFov>>nearPlane>>farPlane>>priority)) return false;
    projection=CameraProjection::Perspective;orthographicHalfHeight=5;environmentMask=~0u;
    if(version==2) {u32 mode=0;if(!(in>>orthographicHalfHeight>>mode)||mode>1) return false;projection=static_cast<CameraProjection>(mode);}
    if(version==3) {u32 mode=0;if(!(in>>orthographicHalfHeight>>mode>>environmentMask)||mode>1) return false;projection=static_cast<CameraProjection>(mode);}
    return true;
  }
};
inline constexpr std::array<ComponentNumber,5> cameraNumbers{{
#define AE_CAMERA_NUMBER(id,label,field,lo,hi,step,group,unit) {label,lo,hi,step,[](const ComponentValue &v)->const float&{return static_cast<const Camera&>(v).field;},[](ComponentValue &v)->float*{return &static_cast<Camera&>(v).field;},id,{group,unit}}
  {"Campo vertical",1,170,1,[](const ComponentValue &v)->const float&{return static_cast<const Camera&>(v).verticalFov;},[](ComponentValue &v)->float*{return &static_cast<Camera&>(v).verticalFov;},"vertical_fov",{"Lente","°",nullptr,[](const ComponentValue &v){return static_cast<const Camera&>(v).projection==CameraProjection::Perspective;}},true},
  AE_CAMERA_NUMBER("near_plane","Próximo",nearPlane,.001f,10000,.01f,"Lente","m"),
  AE_CAMERA_NUMBER("far_plane","Distante",farPlane,.01f,1000000,10,"Lente","m"),
  AE_CAMERA_NUMBER("priority","Prioridade",priority,-10000,10000,1,"Saída",""),
  {"Meia altura",.001f,100000, .1f,[](const ComponentValue &v)->const float&{return static_cast<const Camera&>(v).orthographicHalfHeight;},[](ComponentValue &v)->float*{return &static_cast<Camera&>(v).orthographicHalfHeight;},"orthographic_half_height",{"Lente","m","Metade da altura visível. Zoom altera esta extensão.",[](const ComponentValue &v){return static_cast<const Camera&>(v).projection==CameraProjection::Orthographic;}},true}
#undef AE_CAMERA_NUMBER
}};
inline constexpr std::array<ComponentBoolean,1> cameraBooleans{{
  {"enabled","Usar no Play",[](const ComponentValue &v){return static_cast<const Camera&>(v).enabled;},[](ComponentValue &v,bool b){static_cast<Camera&>(v).enabled=b;},{"Saída"}}
}};
inline constexpr std::array<ComponentEnumOption,2> cameraProjectionOptions{{{0,"Perspectiva"},{1,"Ortográfica"}}};
inline constexpr std::array<ComponentEnumOption,9> cameraEnvironmentMaskOptions{{
  {~0u,"Todos os ambientes"},{1u,"Ambiente 0"},{2u,"Ambiente 1"},{4u,"Ambiente 2"},{8u,"Ambiente 3"},
  {16u,"Ambiente 4"},{32u,"Ambiente 5"},{64u,"Ambiente 6"},{128u,"Ambiente 7"}
}};
inline constexpr std::array<ComponentEnum,2> cameraEnums{{
  {"projection","Projeção",cameraProjectionOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Camera&>(v).projection);},[](ComponentValue &v,u32 mode){static_cast<Camera&>(v).projection=static_cast<CameraProjection>(mode);},{"Lente"}}
  ,{"environment_mask","Ambientes",cameraEnvironmentMaskOptions,[](const ComponentValue &v){return static_cast<const Camera&>(v).environmentMask;},[](ComponentValue &v,u32 mask){static_cast<Camera&>(v).environmentMask=mask;},{"Saída","","Volumes que esta câmera consulta",nullptr,nullptr,"render.environment.volumes","renderer/scene_environment.cpp",Invalidate::Draw}}
}};
inline const ComponentType Camera::descriptor{
  "astra.camera",3,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Camera>();},cameraNumbers,cameraBooleans,cameraEnums
};
}
