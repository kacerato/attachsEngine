// A sessão de edição: o objeto que a plataforma segura.
//
// Ela existe para que a camada Android não precise saber que existem documento,
// histórico, câmera, roteador, lista de desenho e construtor de instâncias.
// `android_main` alimenta toques e pede instâncias; tudo entre uma coisa e outra
// é decisão do editor, e portanto testável sem aparelho.
//
// **O que o dedo faz aqui é o que ele faria num editor de verdade:**
//   • um dedo no vazio da cena orbita a câmera;
//   • dois dedos deslocam o alvo e afastam/aproximam;
//   • um toque curto na cena seleciona o que está sob ele;
//   • arrastar uma alça do gizmo move o objeto, num único passo de desfazer;
//   • tudo que muda o documento passa pelo histórico.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_character.h"
#include "editor/editor_scene_camera.h"
#include "editor/editor_camera_look.h"
#include "platform/first_person_controller.h"
#include "editor/editor_camera.h"
#include "editor/editor_commands.h"
#include "editor/editor_map_scene.h"
#include "editor/editor_archive.h"
#include "editor/editor_document.h"
#include "editor/editor_history.h"
#include "editor/editor_screen.h"
#include "ui/ui_font.h"
#include "ui/ui_icon_atlas.h"
#include "ui/ui_instance_builder.h"

#include <span>
#include <vector>

namespace ae::editor {

enum class EditorTextPurpose { None, Rename, HierarchySearch, CreationSearch, Number, Code, ScriptName, CodeSearch, ScriptProperty, ComponentSearch, MeshSearch, ReferenceSearch };
struct EditorTextEdit {
  EditorTextPurpose purpose=EditorTextPurpose::None;
  EditorSceneVersion version{};
  EditorEntityId entity=0;
  u32 field=0;
  std::string text;
  u64 bufferId=0,bufferRevision=0,componentInstance=0;
  std::string propertyId,propertyType;
};

struct EditorAssetInstantiation {
  std::string_view name;
  float scale[3]{1,1,1};
  bool rigidBody=false;
};

class EditorSession final {
public:
  static constexpr u32 kMaximumInstances = 16384;

  // Os atlas continuam pertencendo ao chamador e precisam sobreviver à sessão.
  void initialize(const ui::UiFont *font, const ui::UiIconAtlas *icons);

  // A superfície é sempre o espaço LÓGICO em paisagem, sem pré-rotação.
  //
  // A rotação do display é aplicada uma única vez, no vertex shader da interface,
  // ao converter pixel lógico em NDC. Aplicá-la também na projeção da cena
  // rodaria a grade e o gizmo dentro de um retângulo que já está no espaço
  // certo — foi exatamente o que aconteceu na primeira execução no aparelho, e
  // a grade saiu atravessando a tela na diagonal.
  void setSurface(const ui::UiRect &surface, const ui::UiInsets &safeArea);
  void setProjectName(const char *name);
  bool setProjectDirectory(const char *path);
  bool needsScriptRuntime() const {return isPlaying()&&runtime::ScriptBridge::hasScripts(document_);}
  void setScriptRuntime(scene::ScriptRuntimeApi api) {playScene_.setScriptRuntime(api,files_.rootPath());}
  void setScriptLogSink(runtime::ScriptBridge::LogSink sink) {playScene_.setScriptLogSink(std::move(sink));}
  void setCodeCompilerAvailable(bool value) {state_.codeCompilerAvailable=value;}
  std::string takeCodeBuildRequest() {auto request=std::move(codeBuildRequest_);codeBuildRequest_.clear();return request;}
  bool completeCodeBuild(std::string_view report) {
    state_.codeBuildBusy=false;const bool ok=code_.applyBuildReport(report,codeBuildGeneration_);
    state_.status=ok?"Código compilado; pronto para aplicar":code_.error();return ok;
  }
  void reportCodeCommit(bool accepted) {
    if(!accepted) code_.invalidateBuild();
    state_.status=accepted?"Código aplicado ao projeto":"Não foi possível publicar a compilação";
  }
  const std::string &requestedScenePath() const { return requestedScenePath_; }
  void clearSceneOpenRequest() { requestedScenePath_.clear(); }
  void reportSceneOpenFailure() { state_.status="Não foi possível abrir a cena; cena atual preservada"; }
  void setCameraPose(const float position[3], float yaw, float pitch);
  void setProjection(renderer::PerspectiveVisibilitySettings settings) { projection_=settings; }

  // Seleção inicial vinda de fora, para uma cena recém-carregada já abrir com
  // algo no Inspector em vez de "nada selecionado".
  void setSelection(EditorEntityId entity);

  // Um evento de ponteiro já em coordenadas lógicas. Devolve true quando a
  // interface o consumiu — a plataforma usa isso para não repassar o mesmo
  // toque ao controlador de jogo.
  bool handlePointer(const ui::UiPointerEvent &event);
  void cancelPointers();
  void usePlatformTextInput(bool enabled) { state_.platformTextInput=enabled; }
  EditorTextEdit pendingTextEdit() const;
  bool completeTextEdit(const EditorTextEdit &edit, std::string_view text, bool accept);

  // Reconstrói a lista de desenho e as instâncias do frame.
  void update();

  std::span<const ui::UiInstance> instances() const noexcept { return instances_; }
  const EditorScreenLayout &layout() const noexcept { return layout_; }
  const EditorScreenState &screen() const noexcept { return state_; }
  const EditorCamera &camera() const noexcept { return camera_; }
  const EditorViewport &view() const noexcept { return view_; }
  EditorDocument &document() noexcept { return document_; }
  EditorHistory &history() noexcept { return history_; }
  EditorEntityId selection() const noexcept { return state_.selection; }
  EditorSceneVersion sceneVersion() const noexcept { return {sceneEpoch_,document_.revision()}; }
  EditorActionResult dispatch(const EditorActionRequest &request);
  bool playRequested() const noexcept { return playRequested_; }
  void clearPlayRequest() noexcept { playRequested_ = false; }

  // **Editar é ver a cena PARADA.** A água não ondula, o casco não anda e a
  // física não integra enquanto alguém posiciona um objeto — senão o que se vê
  // não é um editor, é um vídeo com painéis por cima. A aba Play é o que solta.
  bool isPlaying() const noexcept { return state_.workspace == EditorWorkspace::Play; }
  // Relógio da CENA, separado do relógio de parede. Ele só avança em Play, e é
  // ele que alimenta a animação e a simulação — congelar apenas o desenho
  // mostraria uma imagem parada sobre um estado que continua mudando, e apertar
  // Play daria um salto.
  float sceneTime() const noexcept { return sceneTime_; }
  // `wallSeconds` é o relógio contínuo da plataforma. A sessão deriva o próprio
  // passo dele, o que a torna imune a um primeiro quadro com valor arbitrário.
  void advanceClock(float wallSeconds) noexcept;
  // Enquadra o objeto selecionado. É o gesto de "onde ele está?", e sem ele um
  // objeto longe do alvo da órbita fica inalcançável.
  void frameSelection();
  void frameAll();
  bool saveRequested() const { return state_.saveRequested; }
  bool save(const char *path, u64 fingerprint);
  bool load(const char *path, u64 fingerprint);
  bool importMap(std::span<const renderer::MapDrawRecord> draws, std::span<const renderer::MapMaterialRecord> materials = {}, bool instantiate = true, std::span<const u8> vertices = {}, std::span<const u32> indices = {});
  bool extractMap(std::vector<renderer::MapDrawState> &out) const { return mapScene_.extract(document_, out); }
  SceneCameraPose sceneCameraPose() const {
    return resolveSceneCamera(isPlaying()&&playScene_.active()?playScene_.document():document_);
  }
  bool extractPlayMap(std::vector<renderer::MapDrawState> &out) {
    if(!isPlaying()) return false;
    if(!playScene_.active()) {
      if(EditorPlayScene::unresolvedEntity(document_)!=kInvalidEntity) {
        state_.status="Play indisponível: há componentes de tipo ausente";return false;
      }
      if(!playScene_.start(document_,mapScene_)) {
        state_.status=!playScene_.scriptDiagnostics().empty()?playScene_.scriptDiagnostics():playScene_.physicsError().empty()?"Falha ao preparar a cena para Play":playScene_.physicsError();
        return false;
      }
      playLastSeconds_=sceneTime_;
    }
    const double elapsed=std::max(0.0,static_cast<double>(sceneTime_)-playLastSeconds_);
    playLastSeconds_=sceneTime_;
    playScene_.pause(state_.playPaused);
    if(state_.playPaused) playTouches_.cancel();
    const auto actions=playTouches_.consumeInput();
    auto view=resolveSceneCamera(playScene_.document());
    const auto *viewEntity=playScene_.document().find(view.entity);
    if(viewEntity&&cameraLook(*viewEntity)) {
      if(!applyCameraLook(*playScene_.executionGraph(),view.entity,actions.lookScreenX,actions.lookScreenY)) return false;
      view=resolveSceneCamera(playScene_.document());
    }
    const auto *controlled=playScene_.document().find(state_.selection);
    if(controlled && characterComponent(*controlled)) {
      if(!playScene_.setCharacterMove(controlled->id,actions.moveRight,actions.moveForward,view.entity?view.yaw:0)) return false;
    }
    if(state_.playStepRequested) {
      state_.playStepRequested=false;
      if(!playScene_.step()) return false;
    }
    if(!playScene_.advance(elapsed)) return false;
    if(!playScene_.scriptDiagnostics().empty()) state_.status=playScene_.scriptDiagnostics();
    return playScene_.extract(mapScene_,out);
  }
  EditorEntityId instantiateAsset(u32 index, EditorEntityId parent, const float worldPosition[3], const EditorAssetInstantiation *options=nullptr);
  EditorEntityId createWaterSurface(bool cameraRelative);
  void reportWaterConfiguration(bool accepted) {
    state_.status=accepted?"Agua atualizada":"Configuracao de agua recusada; estado anterior mantido";
  }
  void reportPlayFailure() {
    playScene_.stop();state_.workspace=EditorWorkspace::Scene;
    if(!playScene_.scriptDiagnostics().empty()) state_.status=playScene_.scriptDiagnostics();
    else if(!playScene_.physicsError().empty()) state_.status=playScene_.physicsError();
    else if(state_.status.rfind("Play indisponível:",0)!=0)
      state_.status="Falha ao executar a cena; revise os recursos e componentes";
  }


private:
  static u64 nextSceneEpoch() noexcept;
  struct ViewportPointer final {
    u32 id = 0;
    ui::UiPoint position{};
    bool moved = false;
  };

  void buildPickCandidates();
  void frameSubtree(EditorEntityId root);
  bool handleViewportPointer(const ui::UiPointerEvent &event, const ui::UiPointerRouting &routing);
  void handleGizmoPointer(const ui::UiPointerRouting &routing, u32 axis);
  ViewportPointer *findViewportPointer(u32 id) noexcept;

  const ui::UiFont *font_ = nullptr;
  const ui::UiIconAtlas *icons_ = nullptr;
  EditorDocument document_;
  EditorFileSystem files_;
  EditorCodeWorkspace code_;
  std::string codeBuildRequest_;
  u64 codeBuildGeneration_=0;
  std::string requestedScenePath_;
  u64 sceneEpoch_=nextSceneEpoch();
  EditorMapScene mapScene_;
  EditorPlayScene playScene_;
  double playLastSeconds_=0;
  platform::FirstPersonTouchControls playTouches_;
  EditorHistory history_;
  EditorCamera camera_;
  renderer::PerspectiveVisibilitySettings projection_{};
  EditorViewport view_{};
  EditorScreenState state_{};
  EditorScreenLayout layout_{};
  ui::UiDrawList list_;
  ui::UiInputRouter router_;
  std::vector<ui::UiInstance> instances_;
  std::vector<EditorPickCandidate> candidates_;
  std::vector<ViewportPointer> viewportPointers_;
  // Distância entre dois dedos no frame anterior, para derivar a pinça.
  float pinchDistance_ = 0.0f;
  EditorGizmoDrag gizmoDrag_{};
  EditorViewport rotationView_{};
  float rotationWorld_[16]{}, rotationParent_[16]{};
  float planeStart_[3]{};
  float rotationLastAngle_=0, rotationTotalAngle_=0;
  bool gizmoTransactionOpen_ = false;
  u32 gizmoPointer_ = 0;
  float parentInverseTranspose_[12]{};
  EditorGizmoMode dragMode_ = EditorGizmoMode::Translate;
  EditorEntityId dragEntity_ = kInvalidEntity;
  u32 fieldPointer_ = 0;
  u32 fieldWidget_ = 0;
  u32 assetPointer_ = 0;
  u32 hierarchyPointer_ = 0;
  EditorEntity fieldInitial_{};
  bool playRequested_ = false;
  float sceneTime_ = 0.0f;
  float lastWallSeconds_ = 0.0f;
  bool clockPrimed_ = false;
};

} // namespace ae::editor
