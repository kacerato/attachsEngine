#pragma once
#include "editor/editor_camera_handles.h"
#include "scene/component_properties.h"
#include "scene/environment.h"
#include "scene/light.h"
#include "scene/light_probe_group.h"
#include "scene/reflection_probe.h"
#include <cmath>

namespace ae::editor {

enum class EditorComponentHandleKind : u32 {
  LightRange, LightInnerAngle, LightOuterAngle,
  BoxX, BoxY, BoxZ, SphereRadius, BlendDistance,
  // PK-6: a sonda selecionada de um Grupo de sondas de luz, pelos eixos locais.
  ProbePointX, ProbePointY, ProbePointZ, Count
};

struct EditorComponentHandle : EditorCameraHandle {
  EditorComponentHandleKind kind=EditorComponentHandleKind::LightRange;
  u64 instance=0;
  float worldUnitsPerProperty=1;
  // Ponto do grupo que a alça move (ProbePoint*).
  u64 element=0;
};

inline std::string_view componentHandleProperty(EditorComponentHandleKind kind) {
  switch(kind) {
    case EditorComponentHandleKind::LightRange:return "range";
    case EditorComponentHandleKind::LightInnerAngle:return "inner_spot_angle";
    case EditorComponentHandleKind::LightOuterAngle:return "spot_angle";
    case EditorComponentHandleKind::BoxX:return "box_size.x";
    case EditorComponentHandleKind::BoxY:return "box_size.y";
    case EditorComponentHandleKind::BoxZ:return "box_size.z";
    case EditorComponentHandleKind::SphereRadius:return "sphere_radius";
    case EditorComponentHandleKind::BlendDistance:return "blend_distance";
    default:return {};
  }
}

inline bool componentHandleGeometry(const runtime::SceneGraph &document,EditorEntityId id,u64 instance,
                                    EditorComponentHandleKind kind,EditorComponentHandle &out,u64 element=0) {
  const auto *entity=document.find(id);
  const auto *component=entity?entity->components.findInstance(instance):nullptr;
  float world[16],pose[16];
  if(!component||!component->valid()||!editorWorldMatrix(document,id,world)) return false;
  const bool probePoint=kind>=EditorComponentHandleKind::ProbePointX&&kind<=EditorComponentHandleKind::ProbePointZ;
  if(&component->type()==&scene::LightProbeGroup::descriptor) {
    // Alça fora da esfera da sonda, no eixo local do grupo; uma unidade da
    // posição local anda o comprimento da coluna (a escala do objeto).
    const auto *point=probePoint?static_cast<const scene::LightProbeGroup&>(*component).point(element):nullptr;
    if(!point) return false;
    const u32 axis=static_cast<u32>(kind)-static_cast<u32>(EditorComponentHandleKind::ProbePointX);
    float column[3]{world[axis*4],world[axis*4+1],world[axis*4+2]};
    const float length=std::sqrt(column[0]*column[0]+column[1]*column[1]+column[2]*column[2]);
    if(!std::isfinite(length)||length<1e-6f) return false;
    for(u32 row=0;row<3;++row) {
      out.axis[row]=column[row]/length;
      out.point[row]=world[row]*point->position[0]+world[4+row]*point->position[1]+world[8+row]*point->position[2]+world[12+row]+
                     out.axis[row]*.45f;
    }
    out.kind=kind;out.instance=instance;out.element=element;out.worldUnitsPerProperty=length;
    return true;
  }
  if(probePoint) return false;
  if(&component->type()==&scene::ReflectionProbe::descriptor) {
    // A caixa da sonda segue só a posição do objeto (eixos do mundo, sem escala).
    const auto &value=static_cast<const scene::ReflectionProbe&>(*component);
    float local[3]{value.boxOffset[0],value.boxOffset[1],value.boxOffset[2]},direction[3]{};
    float worldUnits=1;
    if(kind>=EditorComponentHandleKind::BoxX&&kind<=EditorComponentHandleKind::BoxZ) {
      const u32 axis=static_cast<u32>(kind)-static_cast<u32>(EditorComponentHandleKind::BoxX);
      local[axis]+=value.boxSize[axis]*.5f;direction[axis]=1;worldUnits=.5f;
    } else if(kind==EditorComponentHandleKind::BlendDistance) {
      local[0]+=-value.boxSize[0]*.5f+value.blendDistance;direction[0]=1;
    } else return false;
    for(u32 row=0;row<3;++row) {out.point[row]=world[12+row]+local[row];out.axis[row]=direction[row];}
    out.kind=kind;out.instance=instance;out.worldUnitsPerProperty=worldUnits;
    return true;
  }
  const bool light=&component->type()==&scene::Light::descriptor;
  const bool environment=&component->type()==&scene::Environment::descriptor;
  if(!light&&!environment) return false;
  if(light) {if(!editorOpticalFrame(world,pose)) return false;}
  else std::copy(world,world+16,pose);
  float local[3]{},direction[3]{};
  float worldUnits=1;
  if(light) {
    const auto &value=static_cast<const scene::Light&>(*component);
    if(value.kind==scene::LightKind::Directional) return false;
    if(kind==EditorComponentHandleKind::LightRange) {
      if(value.kind==scene::LightKind::Point) {local[0]=value.range;direction[0]=1;}
      else {
        const float angle=value.outerHalfAngle()*.01745329252f;
        local[1]=value.range*std::sin(angle);local[2]=value.range*std::cos(angle);
        direction[1]=std::sin(angle);direction[2]=std::cos(angle);
      }
    } else if(kind==EditorComponentHandleKind::LightInnerAngle||kind==EditorComponentHandleKind::LightOuterAngle) {
      if(value.kind!=scene::LightKind::Spot) return false;
      // A propriedade é a abertura total (Spot Angle); a alça fica na borda,
      // a meio-ângulo do eixo, e um grau da propriedade move meio grau de arco.
      const float angle=(kind==EditorComponentHandleKind::LightInnerAngle?value.innerHalfAngle():value.outerHalfAngle())*.01745329252f;
      const float side=kind==EditorComponentHandleKind::LightInnerAngle?-1.f:1.f;
      local[0]=side*value.range*std::sin(angle);local[2]=value.range*std::cos(angle);
      direction[0]=side*std::cos(angle);direction[2]=-std::sin(angle);
      worldUnits=value.range*.01745329252f*.5f;
    } else return false;
  } else {
    const auto &value=static_cast<const scene::Environment&>(*component);
    if(value.shape==renderer::EnvironmentVolumeShape::Global) return false;
    u32 axis=0;
    if(kind>=EditorComponentHandleKind::BoxX&&kind<=EditorComponentHandleKind::BoxZ) {
      if(value.shape!=renderer::EnvironmentVolumeShape::Box) return false;
      axis=static_cast<u32>(kind)-static_cast<u32>(EditorComponentHandleKind::BoxX);
      local[axis]=value.boxSize[axis]*.5f;direction[axis]=1;
      worldUnits=.5f;
    } else if(kind==EditorComponentHandleKind::SphereRadius) {
      if(value.shape!=renderer::EnvironmentVolumeShape::Sphere) return false;
      local[0]=value.sphereRadius;direction[0]=1;
    } else if(kind==EditorComponentHandleKind::BlendDistance) {
      const float extent=value.shape==renderer::EnvironmentVolumeShape::Box?
          value.boxSize[0]*.5f:value.sphereRadius;
      const float length=std::sqrt(world[0]*world[0]+world[1]*world[1]+world[2]*world[2]);
      if(!std::isfinite(length)||length<1e-6f) return false;
      local[0]=-extent-value.blendDistance/length;direction[0]=-1;
      worldUnits=1/length;
    } else return false;
  }
  float axis[3]{};
  for(u32 row=0;row<3;++row) {
    out.point[row]=pose[12+row]+pose[row]*local[0]+pose[4+row]*local[1]+pose[8+row]*local[2];
    axis[row]=pose[row]*direction[0]+pose[4+row]*direction[1]+pose[8+row]*direction[2];
  }
  const float length=std::sqrt(axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2]);
  if(!std::isfinite(length)||length<1e-6f) return false;
  for(u32 row=0;row<3;++row) out.axis[row]=axis[row]/length;
  out.kind=kind;out.instance=instance;out.worldUnitsPerProperty=worldUnits*length;
  return std::isfinite(out.worldUnitsPerProperty)&&out.worldUnitsPerProperty>0;
}

inline bool applyComponentHandleDelta(EditorEntity &entity,const EditorComponentHandle &handle,float worldDelta) {
  if(!std::isfinite(worldDelta)||!std::isfinite(handle.worldUnitsPerProperty)||
     handle.worldUnitsPerProperty<=0) return false;
  const auto *component=entity.components.findInstance(handle.instance);
  if(!component) return false;
  if(handle.kind>=EditorComponentHandleKind::ProbePointX&&handle.kind<=EditorComponentHandleKind::ProbePointZ) {
    auto *edit=entity.components.editInstance(handle.instance);
    if(!edit||&edit->type()!=&scene::LightProbeGroup::descriptor) return false;
    auto &group=static_cast<scene::LightProbeGroup&>(*edit);
    const auto *point=group.point(handle.element);
    if(!point) return false;
    float position[3]{point->position[0],point->position[1],point->position[2]};
    position[static_cast<u32>(handle.kind)-static_cast<u32>(EditorComponentHandleKind::ProbePointX)]+=worldDelta/handle.worldUnitsPerProperty;
    return group.movePoint(handle.element,position);
  }
  const auto id=componentHandleProperty(handle.kind);
  const scene::ComponentNumber *number=nullptr;
  for(const auto &entry:component->type().numbers) if(entry.id==id) {number=&entry;break;}
  if(!number||!number->presentation.isEditable(*component)) return false;
  float lower=number->minimum,upper=number->maximum;
  if(&component->type()==&scene::Light::descriptor) {
    const auto &light=static_cast<const scene::Light&>(*component);
    if(handle.kind==EditorComponentHandleKind::LightInnerAngle) upper=std::min(upper,light.spotAngle);
    if(handle.kind==EditorComponentHandleKind::LightOuterAngle) lower=std::max(lower,light.innerSpotAngle);
  }
  const float desired=number->read(*component)+worldDelta/handle.worldUnitsPerProperty;
  if(!std::isfinite(desired)) return false;
  return scene::setComponentProperty(entity.components,component->type().id,id,
           std::clamp(desired,lower,upper),handle.instance)==scene::ComponentPropertyStatus::Applied;
}

} // namespace ae::editor
