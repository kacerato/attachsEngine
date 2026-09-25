#pragma once
#include "editor/editor_properties.h"
#include "scene/component_schema.h"
#include "ui/ui_icon_id.h"
#include "scene/joint.h"

namespace ae::editor {
// O catálogo anexável do inspetor.
//
// **Nome, descrição, categoria, exigências e incompatibilidades vêm do schema**
// (scene/component_schema.h), que também alimenta o mundo de execução e, por
// ele, a API em C#. Aqui ficam só as decisões que são de interface: qual ícone
// e qual grupo de propriedades desenhar. Antes desta separação existiam duas
// listas com as mesmas regras escritas à mão, e uma regra acrescentada em uma
// delas fazia a API aceitar o que a interface recusava.
using EditorComponentCategory = scene::ComponentCategory;

struct EditorComponentEntry {
  const scene::ComponentSchema *schema;
  const EditorComponentType *type;
  const char *name;
  const char *description;
  EditorComponentCategory category;
  EditorPropertyGroup properties;
  ui::UiIcon icon;
  // Termos de descoberta de outras engines; não alteram o TypeId nem prometem
  // equivalência de API. A composição continua vindo do schema Astra.
  const char *searchTerms;
  // Motivo pelo qual o tipo não pode ser anexado a esta entidade, ou nullptr.
  const char *unavailable(const EditorEntity &entity) const {
    return scene::componentAdditionBlockedReason(*schema, entity.components);
  }
};

namespace detail {
inline EditorComponentEntry catalogEntry(std::string_view id, EditorPropertyGroup properties, ui::UiIcon icon,
                                         const char *searchTerms) {
  const auto *schema = scene::findComponentSchema(id);
  // Uma entrada de catálogo sem schema seria um componente sem contrato: sem
  // cardinalidade, sem exigências e invisível para a API. Não existe.
  return schema ? EditorComponentEntry{schema, schema->type, schema->name, schema->description,
                                       schema->category, properties, icon, searchTerms}
                : EditorComponentEntry{nullptr, nullptr, "", "", EditorComponentCategory::Physics, properties, icon, searchTerms};
}
} // namespace detail

// Os doze tipos com consumidor implementado. Comportamento C# é anexado pela
// área de código, não por esta lista, e por isso não aparece aqui.
inline const std::array<EditorComponentEntry, 12> editorComponentCatalog{{
  detail::catalogEntry("astra.physics.body", EditorPropertyGroup::ScenePhysics, ui::UiIcon::ComponentPhysics, "Rigidbody RigidBody3D"),
  detail::catalogEntry("astra.physics.character", EditorPropertyGroup::Character, ui::UiIcon::ComponentCharacter, "CharacterController CharacterBody3D"),
  detail::catalogEntry("astra.camera.look", EditorPropertyGroup::CameraLook, ui::UiIcon::ComponentLook, "MouseLook CameraController"),
  detail::catalogEntry("astra.physics.collider", EditorPropertyGroup::Collider, ui::UiIcon::ComponentCollider, "BoxCollider SphereCollider CapsuleCollider CollisionShape3D"),
  detail::catalogEntry("astra.physics.joint", EditorPropertyGroup::ScenePhysics, ui::UiIcon::ComponentJoint, "HingeJoint Joint3D"),
  detail::catalogEntry("astra.camera", EditorPropertyGroup::CameraLook, ui::UiIcon::EditorAuthorCamera, "Camera3D"),
  detail::catalogEntry("astra.render.mesh", EditorPropertyGroup::Material, ui::UiIcon::EditorAuthorObject, "MeshFilter MeshRenderer MeshInstance3D"),
  detail::catalogEntry("astra.render.light", EditorPropertyGroup::Material, ui::UiIcon::LightingSun, "DirectionalLight3D OmniLight3D SpotLight3D"),
  detail::catalogEntry("astra.render.environment", EditorPropertyGroup::Material, ui::UiIcon::LightingSun, "WorldEnvironment Volume"),
  detail::catalogEntry("astra.render.lod_group", EditorPropertyGroup::Material, ui::UiIcon::SceneLayers, "LODGroup VisibilityRange"),
  detail::catalogEntry("astra.render.skinned_mesh", EditorPropertyGroup::Material, ui::UiIcon::ComponentJoint, "SkinnedMeshRenderer Skeleton3D"),
  detail::catalogEntry("astra.animation", EditorPropertyGroup::Material, ui::UiIcon::AssetsAnimation, "AnimationPlayer")
}};

inline const EditorComponentEntry *findEditorComponent(std::string_view id) {
  for(const auto &entry:editorComponentCatalog) if(entry.type && entry.type->id==id) return &entry;
  return nullptr;
}
} // namespace ae::editor
