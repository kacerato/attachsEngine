#pragma once
#include "resources/animation_clip_asset.h"
#include "resources/rig_asset.h"
#include "editor/editor_document.h"
#include <optional>
#include <variant>

namespace ae::editor {
using AuthoringResourceValue=std::variant<resources::AnimationClipAsset,resources::RigAsset,
    resources::RigPose,resources::RetargetProfile>;
enum class AuthoringResourceOperation : u8 {Create,Edit,Remove};
struct AuthoringResourceEdit {
  AuthoringResourceOperation operation=AuthoringResourceOperation::Edit;
  resources::AssetGuid guid;
  std::string path,expectedHash;
  u32 expectedRevision=0;
  // Removal has no after-value. Creation/edit always carry a complete typed
  // candidate; no arbitrary JSON patch or opaque pointer crosses the SDK.
  std::optional<AuthoringResourceValue> value;
};
struct AuthoringSceneEdit {
  EditorEntityId id=0;
  EditorEntity before,after;
};
struct AuthoringBatch {
  static constexpr usize MaximumResources=32,MaximumBytes=64u<<20;
  std::string project,label;
  u64 epoch=0,sceneRevision=0;
  EditorEntityId owner=0;
  std::vector<AuthoringResourceEdit> resources;
  // The same group may establish a rig binding or change a pose. This remains
  // a typed scene-value edit, preflighted before any file or scene publication.
  std::vector<AuthoringSceneEdit> scene;
};
resources::AssetGuid authoringResourceGuid(const AuthoringResourceValue &);
u32 authoringResourceRevision(const AuthoringResourceValue &);
resources::AssetType authoringResourceType(const AuthoringResourceValue &);
std::string authoringResourceText(const AuthoringResourceValue &);
bool validAuthoringBatchEnvelope(const AuthoringBatch &,std::string &error);
} // namespace ae::editor
