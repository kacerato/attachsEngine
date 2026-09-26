// Família Câmera: projeção e comportamento de enquadramento. Incluído somente
// por scene/component_schema.h.
#pragma once
#include "scene/camera.h"
#include "scene/camera_follow.h"
#include "scene/camera_look.h"

namespace ae::scene {
inline constexpr std::array<ComponentRule, 1> lookRequirements{{
  {"astra.camera", "Adicione Câmera a este objeto"}
}};
inline constexpr std::array<ComponentRule,1> followRequirements{{
  {"astra.camera","Adicione Câmera a este objeto"}
}};
inline constexpr std::array<ComponentRule,2> followConflicts{{
  {"astra.physics.body","A câmera seguidora não pode receber pose do corpo físico"},
  {"astra.physics.character","A câmera seguidora não pode receber pose do personagem"}
}};
inline constexpr std::array<ComponentSchema, 3> cameraSchemas{{
  {.type=&Camera::descriptor, .name="Câmera", .description="Projeção e enquadramento",
   .family=ComponentFamily::Camera, .structuralInPlay=PlayMutability::SafePoint,
   .consumer="renderer/render_view.h → matriz de projeção e culling", .invalidates=Invalidate::Draw,
   .subfamily="Projeção", .icon="editor/author-camera", .searchTerms="Camera Camera3D Perspectiva Ortografica",
   .reference="https://docs.unity3d.com/6000.0/Documentation/Manual/class-Camera.html",
   .apiName="Camera"},
  {.type=&CameraLook::descriptor, .name="Olhar", .description="Rotação local da câmera por entrada ou script",
   .family=ComponentFamily::Camera, .requirements=lookRequirements, .structuralInPlay=PlayMutability::SafePoint,
   .consumer="runtime/game_world.cpp → pose da câmera", .invalidates=Invalidate::Input,
   .subfamily="Controle", .icon="component/look", .searchTerms="MouseLook CameraController PanTilt InputAxisController",
   .reference="https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachinePanTilt.html",
   .apiName="CameraLook"},
  {.type=&CameraFollow::descriptor, .name="Acompanhar alvo", .description="Posiciona a câmera após física e animação",
   .family=ComponentFamily::Camera, .requirements=followRequirements, .conflicts=followConflicts,
   .structuralInPlay=PlayMutability::SafePoint,
   .consumer="runtime/scene_camera_follow.h → pose de Play da câmera", .invalidates=Invalidate::Transform,
   .subfamily="Controle", .icon="component/camera-follow", .searchTerms="Follow Camera Tracking Damping CinemachineFollow",
   .reference="https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineFollow.html",
   .apiName="CameraFollow"}
}};
} // namespace ae::scene
