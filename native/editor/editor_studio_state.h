#pragma once
// Estado do estudio de animacao (rig, IK, transferencia) fora do editor_session.h.
// Campos novos entram aqui e so recompilam os arquivos que incluem este header,
// em vez dos ~60 que incluem a sessao.
#include "resources/rig_asset.h"
#include "editor/editor_document.h"
#include "resources/asset_registry.h"
#include <memory>
#include <string>
#include <vector>

namespace ae::editor {
struct EditorStudioState {
  // Rig carregado/capturado para a raiz do estudio ou da folha de Propriedades.
  resources::RigAsset rigDraft;
  std::vector<resources::RigPose> rigPoses;
  EditorEntityId rigRoot=0,rigSyncedTarget=0;
  u32 rigControl=0,rigJoint=0,rigPose=0,rigMirrorLeft=0,rigMirrorRight=0,rigPoseWeight=100;
  bool rigCaptured=false,rigDirty=false;
  u32 rigRetargetIndex=0;
  // Gesto de IK no viewport do estudio.
  u32 clipIkPointer=0;float clipIkPlane[6]{};EditorEntityId clipIkEnd=0;
  float clipIkPole[3]{};bool clipIkPoleDrag=false;
  // Child-Of gravado: objeto escolhido como pai para "Seguir pai".
  EditorEntityId followParent=0;
  // Bake da pose final: job em andamento, progresso 0..1, ultimo relatorio.
  std::shared_ptr<struct StudioBakeJob> bake;
  float bakeProgress=0;bool bakeRootToOwner=false;
  std::string bakeReport;resources::AssetGuid bakeGuid{};
};
EditorStudioState *createStudioState();
void destroyStudioState(EditorStudioState *);
} // namespace ae::editor
