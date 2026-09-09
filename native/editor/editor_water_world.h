#pragma once
#include "editor/editor_map_scene.h"
#include "renderer/water_world.h"

namespace ae::editor {
inline bool extractEditorWaterWorld(const EditorDocument &document,const renderer::WaterFieldSetup &base,
                                   renderer::WaterWorld &output) {
  renderer::WaterWorld replacement;
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto id:ids) {
    const auto &entity=*document.find(id);
    if(entity.kind!=EditorEntityKind::Water || !entity.waterPhysicsEnabled) continue;
    bool enabled=true;
    for(const auto *parent=&entity;parent;parent=document.find(parent->parent)) enabled&=parent->active && parent->visible;
    if(!enabled) continue;
    float world[16];if(!editorWorldMatrix(document,id,world)) return false;
    const float sx=std::hypot(world[0],world[2]),sz=std::hypot(world[8],world[10]);
    auto setup=base;
    setup.waveScale=entity.waterBody[3];setup.foamScale=entity.waterBody[4];setup.rippleScale=entity.waterBody[5];
    setup.currents.uniform={entity.waterBody[1],entity.waterBody[2]};
    if(entity.route.count) {
      setup.route=entity.route;
      std::copy(world,world+16,setup.routeTransform);
    } else {
      setup.baseHeight+=world[13];setup.bounded=true;
      float normal[12];if(!renderer::buildNormalMatrix(world,normal) || std::abs(normal[5])<.00001f) return false;
      setup.planeOrigin={world[12],world[14]};setup.planeSlope={-normal[4]/normal[5],-normal[6]/normal[5]};
      setup.boundary.shape=renderer::WaterExclusionShape::Box;
      setup.boundary.center={world[12],world[14]};setup.boundary.halfExtent={10*sx,10*sz};
      setup.boundary.rotationRadians=std::atan2(world[2],world[0]);setup.boundary.feather=0;
      setup.hasBathymetry=true;setup.bottomHeight=setup.baseHeight-entity.waterBody[0];
      // Camera grids have world-wide coverage; their explicit asset role is
      // resolved by the caller when creating the water component.
      if(entity.waterInfinite) setup.bounded=false;
    }
    if(!replacement.setVolume(id,setup,0,1u<<std::min(entity.layer,31u))) return false;
  }
  output=std::move(replacement);return true;
}
}
