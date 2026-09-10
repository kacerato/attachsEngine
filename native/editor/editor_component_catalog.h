#pragma once
#include "editor/editor_properties.h"
#include "ui/ui_icon_id.h"
#include "scene/joint.h"

namespace ae::editor {
// Editor metadata for the component types with an implemented runtime consumer.
// The screen iterates this catalog; applicability belongs to the type contract.
enum class EditorComponentCategory : u32 { Camera=1, Visual=2, Physics=3 };
struct EditorComponentEntry {
  const EditorComponentType *type;
  const char *name;
  EditorPropertyGroup properties;
  const char *(*unavailable)(const EditorEntity &);
  const char *description="";
  ui::UiIcon icon=ui::UiIcon::EditorAuthorObject;
  EditorComponentCategory category=EditorComponentCategory::Physics;
};
inline const std::array<EditorComponentEntry,7> editorComponentCatalog{{
  {&EditorPhysicsBody::descriptor,"Corpo físico",EditorPropertyGroup::ScenePhysics,
    [](const EditorEntity &e)->const char* {return characterComponent(e)?"Incompatível com personagem cápsula":nullptr;},
    "Massa e resposta física",ui::UiIcon::ComponentPhysics},
  {&EditorCharacter::descriptor,"Personagem",EditorPropertyGroup::Character,
    [](const EditorEntity &e)->const char* {return physicsBody(e)?"Incompatível com corpo físico":colliderComponent(e)?"O personagem já possui cápsula própria":nullptr;},
    "Locomoção com cápsula",ui::UiIcon::ComponentCharacter},
  {&EditorCameraLook::descriptor,"Olhar",EditorPropertyGroup::CameraLook,
    [](const EditorEntity &e)->const char* {return cameraComponent(e)?nullptr:"Adicione Câmera a este objeto";},
    "Rotação da câmera por toque",ui::UiIcon::ComponentLook,EditorComponentCategory::Camera},
  {&EditorCollider::descriptor,"Colisor 3D",EditorPropertyGroup::Collider,
    [](const EditorEntity &e)->const char* {return characterComponent(e)?"O personagem já possui cápsula própria":nullptr;},
    "Volume de contato",ui::UiIcon::ComponentCollider},
  {&scene::Joint::descriptor,"Junta",EditorPropertyGroup::ScenePhysics,
    [](const EditorEntity &e)->const char* {return physicsBody(e)?nullptr:"Adicione Corpo físico a este objeto";},
    "Conexão, limites e motor entre corpos",ui::UiIcon::ComponentJoint},
  {&scene::Camera::descriptor,"Câmera",EditorPropertyGroup::CameraLook,
    [](const EditorEntity &)->const char* {return nullptr;},
    "Perspectiva e enquadramento",ui::UiIcon::EditorAuthorCamera,EditorComponentCategory::Camera},
  {&scene::MeshRenderer::descriptor,"Malha",EditorPropertyGroup::Material,
    [](const EditorEntity &)->const char* {return nullptr;},
    "Geometria e material",ui::UiIcon::EditorAuthorObject,EditorComponentCategory::Visual}
}};
inline const EditorComponentEntry *findEditorComponent(std::string_view id) {
  for(const auto &entry:editorComponentCatalog) if(entry.type->id==id) return &entry;
  return nullptr;
}
} // namespace ae::editor
