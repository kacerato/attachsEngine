#pragma once
#include "editor/editor_document.h"
#include <array>

namespace ae::editor {
class EditorWaterBodyComponent final : public EditorComponentValue {
public:
  float depth=3;
  float currentX=0,currentZ=0;
  float waveGain=1,foamGain=1,rippleGain=1,opticalGain=1;
  bool physicsEnabled=true;
  bool infinite=false;
  static const EditorComponentType descriptor;
  const EditorComponentType &type() const override { return descriptor; }
  std::unique_ptr<EditorComponentValue> clone() const override {
    return std::make_unique<EditorWaterBodyComponent>(*this);
  }
  bool valid() const override {
    for(const auto &property:descriptor.numbers) {
      const auto value=property.read(*this);
      if(!std::isfinite(value) || value<property.minimum || value>property.maximum) return false;
    }
    return true;
  }
  void write(std::ostream &out) const override {
    out << physicsEnabled << ' ' << infinite;
    for(const auto &property:descriptor.numbers) out << ' ' << property.read(*this);
  }
  bool read(std::istream &in,u32 version) override {
    if(version!=1 || !(in>>physicsEnabled>>infinite)) return false;
    for(const auto &property:descriptor.numbers) if(!(in>>*property.write(*this))) return false;
    return true;
  }
};
// Order is the v1 codec and old Inspector ID mapping; append with migration.
inline constexpr std::array<EditorComponentNumber,7> waterBodyNumbers{{
#define AE_BODY_NUMBER(name,field,lo,hi) {name,lo,hi,.1f,[](const EditorComponentValue &v) -> const float & { return static_cast<const EditorWaterBodyComponent &>(v).field; },[](EditorComponentValue &v) -> float * { return &static_cast<EditorWaterBodyComponent &>(v).field; }}
  AE_BODY_NUMBER("Profundidade m",depth,.1f,10000),
  AE_BODY_NUMBER("Corrente X m/s",currentX,-100,100),
  AE_BODY_NUMBER("Corrente Z m/s",currentZ,-100,100),
  AE_BODY_NUMBER("Camada de ondas",waveGain,0,4),
  AE_BODY_NUMBER("Camada de espuma",foamGain,0,4),
  AE_BODY_NUMBER("Camada de perturbações",rippleGain,0,4),
  AE_BODY_NUMBER("Camada óptica",opticalGain,0,4)
#undef AE_BODY_NUMBER
}};
inline const EditorComponentType EditorWaterBodyComponent::descriptor{
  "astra.water.body",1,[]() -> std::unique_ptr<EditorComponentValue> { return std::make_unique<EditorWaterBodyComponent>(); },waterBodyNumbers
};
inline const EditorWaterBodyComponent &waterBody(const EditorEntity &entity) {
  const auto *value=entity.components.find(EditorWaterBodyComponent::descriptor);
  static const EditorWaterBodyComponent defaults;
  return value ? *static_cast<const EditorWaterBodyComponent *>(value) : defaults;
}
inline EditorWaterBodyComponent *editWaterBody(EditorEntity &entity) {
  return static_cast<EditorWaterBodyComponent *>(entity.components.edit(EditorWaterBodyComponent::descriptor));
}
inline bool setWaterBodyFlags(EditorEntity &entity,bool physicsEnabled,bool infinite) {
  if(waterBody(entity).physicsEnabled==physicsEnabled && waterBody(entity).infinite==infinite) return true;
  auto *body=editWaterBody(entity);if(!body) return false;
  body->physicsEnabled=physicsEnabled;body->infinite=infinite;return true;
}
} // namespace ae::editor
