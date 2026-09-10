#pragma once
#include "editor/editor_document.h"
#include "renderer/water_route.h"

namespace ae::editor {
// Legacy route adapter: the generic document/collection do not know this type.
class EditorRouteComponent final : public EditorComponentValue {
public:
  renderer::WaterRoute route{};
  static const EditorComponentType descriptor;
  const EditorComponentType &type() const override { return descriptor; }
  std::unique_ptr<EditorComponentValue> clone() const override {
    return std::make_unique<EditorRouteComponent>(*this);
  }
  bool valid() const override {
    if(route.count && !renderer::validateWaterRoute(route)) return false;
    // Inactive points are persisted too; validate them before archive/history.
    for(const auto &p:route.points) {
      for(float v:p.position) if(!std::isfinite(v) || std::abs(v)>1e7f) return false;
      if(!std::isfinite(p.width) || p.width<.1f || p.width>10000 ||
         !std::isfinite(p.depth) || p.depth<.1f || p.depth>10000 ||
         !std::isfinite(p.speed) || std::abs(p.speed)>100 ||
         !std::isfinite(p.foam) || p.foam<0 || p.foam>10 ||
         !std::isfinite(p.tension) || p.tension<0 || p.tension>1) return false;
    }
    return true;
  }
  void write(std::ostream &out) const override {
    out << route.count;
    for(const auto &p:route.points) {
      for(float v:p.position) out << ' ' << v;
      out << ' ' << p.width << ' ' << p.depth << ' ' << p.speed << ' ' << p.foam << ' ' << p.tension;
    }
  }
  bool read(std::istream &in,u32 version) override {
    if(version!=1 || !(in>>route.count)) return false;
    for(auto &p:route.points) {
      for(float &v:p.position) if(!(in>>v)) return false;
      if(!(in>>p.width>>p.depth>>p.speed>>p.foam>>p.tension)) return false;
    }
    return true;
  }
};
inline const EditorComponentType EditorRouteComponent::descriptor{
  "astra.water.route",1,[]() -> std::unique_ptr<EditorComponentValue> { return std::make_unique<EditorRouteComponent>(); }
};
inline const renderer::WaterRoute &waterRoute(const EditorEntity &entity) {
  const auto *value=entity.components.find(EditorRouteComponent::descriptor);
  static const renderer::WaterRoute defaults{};
  return value ? static_cast<const EditorRouteComponent *>(value)->route : defaults;
}
inline renderer::WaterRoute *editWaterRoute(EditorEntity &entity) {
  auto *value=entity.components.edit(EditorRouteComponent::descriptor);
  return value ? &static_cast<EditorRouteComponent *>(value)->route : nullptr;
}
inline bool hasWaterRoute(const EditorEntity &entity) {
  return entity.components.find(EditorRouteComponent::descriptor)!=nullptr;
}
} // namespace ae::editor
