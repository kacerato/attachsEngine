#pragma once
#include "scene/components.h"
#include "scene/material_parameters.h"
#include <array>
namespace ae::scene {
class MeshRenderer final : public ComponentValue {
public:
  // Package-local reference, qualified by the scene archive fingerprint.
  // Zero deliberately represents an unassigned resource, not a default cube.
  u32 mesh=0;
  bool enabled=true;
  MaterialParameters material;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<MeshRenderer>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {const auto v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return true;
  }
  void write(std::ostream &out) const override {
    out<<mesh<<' '<<enabled<<' '<<material.enabled<<' ';
    for(const auto &p:descriptor.numbers) out<<p.read(*this)<<' ';
  }
  bool read(std::istream &in,u32 version) override {
    bool overridden=false;
    if(version!=1||!(in>>mesh>>enabled>>overridden)) return false;
    for(const auto &p:descriptor.numbers) if(!(in>>*p.write(*this))) return false;
    material.enabled=overridden;return true;
  }
};
inline constexpr std::array<ComponentNumber,11> meshRendererNumbers{{
#define AE_MESH_NUMBER(id,label,field,lo,hi,step) {label,lo,hi,step,[](const ComponentValue &v)->const float&{return static_cast<const MeshRenderer&>(v).material.field;},[](ComponentValue &v)->float*{auto &m=static_cast<MeshRenderer&>(v).material;m.enabled=true;return &m.field;},id}
  AE_MESH_NUMBER("base_color.r","Cor R",baseColor[0],0,1,.01f),
  AE_MESH_NUMBER("base_color.g","Cor G",baseColor[1],0,1,.01f),
  AE_MESH_NUMBER("base_color.b","Cor B",baseColor[2],0,1,.01f),
  AE_MESH_NUMBER("roughness","Rugosidade",roughness,0,1,.01f),
  AE_MESH_NUMBER("metallic","Metálico",metallic,0,1,.01f),
  AE_MESH_NUMBER("normal_scale","Intensidade da normal",normalScale,0,16,.02f),
  AE_MESH_NUMBER("specular","Especular",specular,0,1,.01f),
  AE_MESH_NUMBER("emission.r","Emissão R",emission[0],0,1,.01f),
  AE_MESH_NUMBER("emission.g","Emissão G",emission[1],0,1,.01f),
  AE_MESH_NUMBER("emission.b","Emissão B",emission[2],0,1,.01f),
  AE_MESH_NUMBER("emission_strength","Potência de emissão",emissionStrength,0,10000,.1f)
#undef AE_MESH_NUMBER
}};
inline constexpr std::array<ComponentBoolean,1> meshRendererBooleans{{
  {"enabled","Renderizar",[](const ComponentValue &v){return static_cast<const MeshRenderer&>(v).enabled;},[](ComponentValue &v,bool b){static_cast<MeshRenderer&>(v).enabled=b;}}
}};
inline const ComponentType MeshRenderer::descriptor{
  "astra.render.mesh",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<MeshRenderer>();},meshRendererNumbers,meshRendererBooleans
};
}
