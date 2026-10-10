#pragma once
#include "editor/editor_document.h"
#include "editor/editor_map_scene.h"
#include "resources/import_node_map.h"
namespace ae::editor {
// A compatible rig is an exact hierarchy/rest-pose binding, not retargeting.
// The caller explicitly chooses sourceRoot and the scene owner. No scene writes.
bool validateFbxClipRig(const EditorDocument &,const EditorMapScene &,EditorEntityId owner,
    resources::AssetGuid sourceGuid,const resources::ImportNodeMap &,const resources::AnimationClip &,u32 sourceRoot,
    std::span<const resources::MorphTargetSet *const> sourceMorphs,
    std::vector<EditorEntityId> &targets,std::string &diagnostic);
}
