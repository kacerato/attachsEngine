#pragma once
#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_scene_template.h"
#include "renderer/authoring_geometry.h"
#include "renderer/primitive_geometry.h"
#include "scene/character.h"
#include <filesystem>
#include <string_view>

namespace ae::test {
// Projeto "Giro do Personagem": o modelo de cena "Personagem jogável" (receita
// de player: Personagem com "Girar para a direção do movimento" ligado e
// câmera seguidora), para a captura do corpo virando ao andar em círculo.
inline int writePlayableFacingProject(const std::filesystem::path &out) {
  using namespace editor;
  if(std::filesystem::exists(out))return 181;
  std::filesystem::create_directories(out/"scenes");
  EditorSession session;
  if(!session.setProjectDirectory(out.generic_string().c_str()))return 182;
  // Mesma biblioteca do shell (primitivas, com o cubo autoral do modelo).
  std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  if(!renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials)||
     !session.importMap(draws,materials,false,vertices,indices,0))return 183;
  const auto templates=sceneTemplates();u32 index=~0u;
  for(u32 i=0;i<templates.size();++i)if(templates[i].id==std::string_view{"personagem_jogavel"})index=i;
  if(index==~0u||!session.createSceneTemplate(index))return 184;
  std::vector<EditorEntityId> ids;session.document().collectSubtree(session.document().root(),ids);
  bool facing=false;
  for(auto id:ids)if(const auto *c=static_cast<const scene::Character*>(session.document().find(id)->components.find(scene::Character::descriptor)))
    facing=c->faceMovement&&c->cameraRelative;
  if(!facing)return 185;
  if(!session.save((out/"scenes/editor.aescene").generic_string().c_str(),0))return 186;
  std::filesystem::create_directories(out/".astra");
  if(!EditorImportTransaction::writeText(out/".astra/assets.astra",session.assets().serialize()))return 187;
  const std::string descriptor="{\"format\":\"ASTRA-PROJECT-1\",\"resourceSource\":\"independent\",\"project\":{\"name\":\"Giro do Personagem\",\"template\":\"empty\",\"scenes\":1,\"assets\":0},\"mainScene\":\"scenes/editor.aescene\",\"editorScene\":\"scenes/editor.aescene\"}";
  if(!EditorImportTransaction::writeText(out/"project.json",descriptor)||
     !EditorImportTransaction::writeText(out/"LEIA-ME.md","# Giro do Personagem\n\nModelo de cena Personagem jogavel. O Personagem tem Girar para a direcao do movimento ligado (Starter Assets ThirdPersonController, RotationSmoothTime 0,12 s). De Play e ande em circulo com o joystick.\n"))return 188;
  return 0;
}
} // namespace ae::test
