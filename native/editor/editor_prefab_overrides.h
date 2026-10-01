#pragma once
#include "runtime/scene_graph.h"
#include "scene/component_preset.h"

namespace ae::editor {
enum class PrefabOverrideKind { Object, Field, ScriptField, Component, AddedComponent, RemovedComponent, ComponentOrder };
enum class PrefabOverrideOrigin { Local, Inherited, Conflict };
// Owned addresses, not pointers into a temporary component or a row index.
struct PrefabOverride {
  PrefabOverrideKind kind=PrefabOverrideKind::Object;
  u64 component=0;
  std::string field,label,current,source;
  u32 slot=0;
  scene::FieldKind fieldKind=scene::FieldKind::Number;
  bool applicable=true;
  PrefabOverrideOrigin origin=PrefabOverrideOrigin::Local;
  std::string baseline{};
  bool applyable=false;
  std::string applyReason{};
  bool sameAddress(const PrefabOverride &other) const {
    if(kind==other.kind && component==other.component &&
       (kind==PrefabOverrideKind::AddedComponent || kind==PrefabOverrideKind::RemovedComponent)) return true;
    return kind==other.kind && component==other.component && field==other.field &&
      slot==other.slot && fieldKind==other.fieldKind;
  }
};
struct PrefabOverrideView {
  runtime::ObjectId object=0;
  u64 revision=0;
  resources::AssetGuid asset;
  std::string sourceHash,error;
  std::vector<PrefabOverride> rows;
  bool hasSourceChanges=false;
};
struct PrefabApplyReport {
  u32 instances=0,objects=0,propagatedFields=0,conflicts=0;
  std::string sourceHash;
};
} // namespace ae::editor
