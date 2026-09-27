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
#include <utility>

#include "core/base.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_play_edit.h"
#include "editor/editor_numeric_expression.h"
#include "editor/editor_value_library.h"
#include "editor/editor_curve_view.h"
#include "runtime/scene_lights.h"
#include "runtime/scene_environment.h"
#include "runtime/lod_groups.h"
#include "editor/editor_character.h"
#include "editor/editor_scene_camera.h"
#include "editor/editor_camera_handles.h"
#include "editor/editor_component_handles.h"
#include "editor/editor_camera_preview.h"
#include "editor/editor_camera_look.h"
#include "platform/first_person_controller.h"
#include "resources/texture_asset.h"
#include "editor/editor_camera.h"
#include "editor/editor_commands.h"
#include "editor/editor_console.h"
#include "editor/editor_grid.h"
#include "editor/editor_map_scene.h"
#include "resources/gltf_import.h"
#include "resources/import_node_map.h"
#include "resources/material_asset.h"
#include "resources/environment_profile.h"
#include "resources/environment_map_asset.h"
#include "resources/texture_budget.h"
#include "resources/import_profile.h"
#include "resources/import_report.h"
#include "resources/glb_images.h"
#include "editor/editor_texture_preview.h"
#include "resources/texture_profile.h"
#include "editor/editor_import_reconcile.h"
#include "editor/editor_archive.h"
#include "editor/editor_document.h"
#include "editor/editor_history.h"
#include "editor/editor_component_presets.h"
#include "editor/editor_screen.h"
#include "ui/ui_font.h"
#include "ui/ui_icon_atlas.h"
#include "ui/ui_instance_builder.h"

#include <functional>
#include <span>
#include <string>
#include <vector>

namespace ae::editor {

enum class EditorTextPurpose { None, Rename, HierarchySearch, CreationSearch, Number, Code, ScriptName, CodeSearch, ScriptProperty, ComponentSearch, MeshSearch, ReferenceSearch, ResourceName, CodeLine, CodeFolder, ConsoleSearch, TextureSearch, ComponentPresetName, SceneViewName, PropertySearch, PhysicsLayerName, InputActionName, InputContext, InputNumber, ColorText };
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
  bool saveComponentPreset(EditorEntityId entity,u64 instance,std::string name,std::string &error);
  // A receita do objeto inteiro: todos os componentes com preset portátil, na
  // ordem em que estão nele. É o que permite salvar "porta interativa" e não
  // quatro presets soltos que o autor precisa lembrar de combinar.
  bool saveComponentRecipe(EditorEntityId entity,std::string name,std::string &error);
  bool applyComponentPreset(u64 preset,EditorEntityId entity,u64 instance,EditorSceneVersion expected,bool add,std::string &error);
  // Aplica uma receita em UMA transação: o que falta é adicionado (resolvendo
  // exigências), o que já existe tem os valores atualizados, e o histórico
  // registra um passo só.
  bool applyComponentRecipe(u64 preset,EditorEntityId entity,EditorSceneVersion expected,std::string &error);
  bool openComponentPresets(EditorEntityId entity,u64 instance);
  bool needsScriptRuntime() const {return isPlaying()&&runtime::ScriptBridge::hasScripts(document_);}
  void setScriptRuntime(scene::ScriptRuntimeApi api);
  // Pausa e foco do aplicativo, repassados aos comportamentos quando há Play.
  void applicationEvent(scene::ScriptLifecycleEvent event,bool value) {
    if(isPlaying()) playScene_.applicationEvent(event,value);
  }
  void configureScriptRendering(const renderer::ProjectRenderingSettings &authored,
                                const renderer::RenderingCapabilities &capabilities,
                                renderer::ThermalPressure thermal,
                                const renderer::ResolvedRenderingPolicy &effective) {
    playScene_.configureScriptRendering(authored,capabilities,thermal,effective,
      [this](u64 id,const renderer::ProjectRenderingSettings &settings,bool restoringAuthoring) {
        // Pedidos normais são serializados pelo estado runtime. A restauração
        // autoral de Stop é a única chamada que pode chegar com uma entrada na
        // fila; ela substitui o pedido não consumido para a configuração de
        // gameplay não escapar para o modo de edição.
        if(scriptRenderingRequestPending_&&!restoringAuthoring) return false;
        scriptRenderingRequestPending_=true;scriptRenderingRequestId_=id;scriptRenderingRequest_=settings;return true;
      },&assets_,&environmentProfiles_);
  }
  bool takeScriptRenderingRequest(u64 &id,renderer::ProjectRenderingSettings &settings) {
    if(!scriptRenderingRequestPending_) return false;
    scriptRenderingRequestPending_=false;id=scriptRenderingRequestId_;settings=scriptRenderingRequest_;return true;
  }
  bool completeScriptRenderingRequest(u64 id,bool success,const renderer::ResolvedRenderingPolicy &effective,
                                      bool effectiveAvailable=true) {
    return playScene_.completeScriptRenderingRequest(id,success,effective,effectiveAvailable);
  }
  // O que um script escreve vai para o console ANTES de ir para onde o
  // hospedeiro mandar. No aparelho o destino do hospedeiro e o `logcat`, que
  // nao existe para quem esta usando o editor: sem esta captura, um script que
  // fala nao tem onde ser ouvido.
  void setScriptLogSink(runtime::ScriptBridge::LogSink sink) {
    playScene_.setScriptLogSink([this,forward=std::move(sink)](u64 object,std::string_view text) {
      EditorConsoleEntry entry;
      entry.origin=EditorConsoleOrigin::Script;
      entry.message=std::string(text);
      if(isPlaying()) state_.playHudMessage=entry.message;
      entry.object=object;
      entry.project=files_.rootPath();entry.playSession=playScene_.world().worldId();
      entry.buildGeneration=runtimeCodeGeneration_;
      console_.add(std::move(entry));
      if(forward) forward(object,text);
    });
  }
  const EditorConsole &console() const noexcept { return console_; }
  EditorConsole &console() noexcept { return console_; }
  // Explicit copy command, consumed once by the platform clipboard bridge.
  std::string takeConsoleCopy() {return std::exchange(consoleCopy_,{});}
  // Um problema que o EDITOR tem a dizer, e nao o compilador nem um script.
  void reportProblem(EditorConsoleSeverity severity,std::string message) {
    EditorConsoleEntry entry;
    entry.severity=severity;entry.origin=EditorConsoleOrigin::Editor;
    entry.message=std::move(message);
    entry.project=files_.rootPath();entry.playSession=playScene_.world().worldId();
    entry.buildGeneration=state_.codeBuildBusy?codeBuildGeneration_:code_.publishedGeneration();
    console_.add(std::move(entry));
  }
  void setCodeCompilerAvailable(bool value) {state_.codeCompilerAvailable=value;}
  // Abre um arquivo de codigo do projeto e o torna o ativo.
  bool openCodeFile(const std::string &relative) {
    if(!code_.open(files_,relative)) { state_.status=code_.error(); return false; }
    state_.code=&code_;state_.workspace=EditorWorkspace::Code;
    return true;
  }
  // Digita no arquivo aberto. E o MESMO caminho que a ponte de texto usa: uma
  // colagem vira transacao propria e a digitacao contigua vira uma entrada so.
  bool typeCode(std::string_view text);
  std::string codeProjectRoot() const {return files_.rootPath();}
  bool navigateCode(const std::string &path,u32 line,u32 column) {
    if(!openCodeFile(path)) return false;
    code_.locate(line,column);state_.codeFiles=false;state_.codeMenu=false;
    state_.consoleExpanded=false;return true;
  }

  // Compilacao automatica: o build sai sozinho quando a digitacao PARA.
  //
  // Compilar a cada tecla seria compilar, sessenta vezes por segundo, um texto
  // que passa a maior parte do tempo sintaticamente quebrado: cada tecla
  // intermediaria produz um erro que o usuario nao cometeu. O atraso existe
  // para que o compilador veja uma pausa, e nao um meio-caminho.
  //
  // A pausa e a composicao sao fatos diferentes. Um IME pode segurar um span
  // composto por segundos sem mudar o texto; so o seu termino libera o debounce.
  void setCodeAutoBuildDelay(float seconds) noexcept {
    codeAutoBuildDelay_ = seconds > 0.0f ? seconds : 0.0f;
  }
  float codeAutoBuildDelay() const noexcept { return codeAutoBuildDelay_; }
  void pumpCodeAutoBuild(double seconds) {
    if(state_.codeRecoveryPending) return;
    // A bounded draft journal also works when the compiler is unavailable or
    // a build is running. It never publishes recovered code implicitly.
    if(seconds-codeCheckpointAt_>=1.0 && !state_.codeComposing && !files_.rootPath().empty()) {
      codeCheckpointAt_=seconds;
      if(!code_.checkpoint(files_)) reportProblem(EditorConsoleSeverity::Warning,code_.error());
    }
    if(state_.codeComposing) {codeQuietSince_=seconds;return;}
    if(codeAutoBuildDelay_<=0.0f || !state_.codeCompilerAvailable || state_.codeBuildBusy) return;
    if(files_.rootPath().empty()) return;
    const auto generation=code_.generation();
    if(generation!=codeSeenGeneration_) {
      codeSeenGeneration_=generation;codeQuietSince_=seconds;return;
    }
    if(generation==codeBuiltGeneration_ || codeQuietSince_<0.0) return;
    if(seconds-codeQuietSince_<static_cast<double>(codeAutoBuildDelay_)) return;
    if(!codeBuildRequest_.empty()) return;
    // Salvar ANTES de pedir: o compilador le o disco, e um build de um texto
    // que so existe na memoria compilaria a versao anterior e culparia o
    // usuario por um erro que ele acabou de corrigir.
    if(!code_.saveAll(files_)) { state_.status=code_.error(); codeQuietSince_=-1.0; return; }
    codeBuiltGeneration_=generation;
    codeBuildGeneration_=generation;
    codeBuildRequest_=files_.rootPath();
    state_.codeBuildBusy=true;
    state_.status="Compilando código do projeto";
    reportProblem(EditorConsoleSeverity::Info,"Compilação automática iniciada · revisão "+std::to_string(generation));
  }
  std::string takeCodeBuildRequest() {auto request=std::move(codeBuildRequest_);codeBuildRequest_.clear();return request;}
  bool completeCodeBuild(std::string_view report) {
    if(codeBuildGeneration_!=code_.generation()) {
      state_.codeBuildBusy=false;
      state_.status="Fonte mais recente; aguardando nova compilação";
      reportProblem(EditorConsoleSeverity::Info,"Resultado de build antigo descartado · revisão "+std::to_string(codeBuildGeneration_));
      return false;
    }
    state_.codeBuildBusy=false;const bool ok=code_.applyBuildReport(report,codeBuildGeneration_);
    // O bloco do compilador troca inteiro: um diagnostico de um arquivo que
    // agora compila e mentira, e mentira que o usuario persegue.
    console_.replaceCompiler(code_.diagnostics(),files_.rootPath());
    // A linha do editor so entra quando NAO ha diagnostico. Com o build
    // automatico, "compilacao com erros" a cada pausa de digitacao seria uma
    // linha nova por pausa dizendo o que as linhas do compilador logo acima ja
    // dizem melhor -- e com o lugar.
    if(!ok && code_.diagnostics().empty()) reportProblem(EditorConsoleSeverity::Error,code_.error());
    if(!ok && !code_.diagnostics().empty()) {
      state_.consoleCollapsed=false;state_.consoleProblems=true;state_.consoleOrigin=-1;
    }
    reportProblem(EditorConsoleSeverity::Info,ok?"Compilação concluída; aguardando publicação":
        "Compilação recusada; publicação anterior preservada");
    state_.status=ok?"Código compilado; publicando":code_.error();return ok;
  }
  // O que o catálogo publicado diz sobre si mesmo, em texto de interface. É a
  // resposta honesta para "o componente sumiu": ele não some, o catálogo é que
  // pode estar atrás do texto.
  const char *codeCatalogStatus() const {
    switch(code_.catalogState()) {
    case EditorCodeCatalogState::Empty: return "Nenhum código publicado";
    case EditorCodeCatalogState::Current: return "Código publicado corresponde ao texto";
    case EditorCodeCatalogState::Stale: return "Texto alterado desde a última publicação";
    case EditorCodeCatalogState::Failed: return "Compilação falhou; publicação anterior em uso";
    }
    return "";
  }
  void reportCodeCommit(bool accepted) {
    // Fronteira atômica: os tipos compilados só passam a existir para o inspetor
    // quando o hospedeiro confirma que carregou o assembly. Recusar não apaga o
    // catálogo anterior — o assembly anterior continua sendo o que executa.
    if(accepted) code_.publishBuild();
    else code_.discardBuild();
    state_.status=accepted?"Código aplicado ao projeto":"Não foi possível publicar a compilação";
    reportProblem(accepted?EditorConsoleSeverity::Info:EditorConsoleSeverity::Error,
        accepted?"Código publicado · revisão "+std::to_string(code_.publishedGeneration()):"Publicação recusada; versão anterior preservada");
    if(accepted) reportScriptSchemaChanges();
  }
  void reportScriptSchemaChanges();
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
  void usePlatformCodeView(bool enabled) {state_.platformCodeView=enabled;}
  void setCodeComposition(bool composing) {state_.codeComposing=composing;}
  void setCodeViewState(u64 id,u64 revision,u32 start,u32 end,float x,float y,bool focus);
  void codeHistoryAction(bool redo);
  bool applyCodeDelta(u64 id,u64 revision,usize start,usize erased,std::string_view text,bool transaction) {
    const bool ok=code_.editDelta(id,revision,start,erased,text,transaction);
    if(!ok) state_.status=code_.error();
    return ok;
  }
  void recoverCodeDraft(std::string_view text);
  EditorTextEdit pendingTextEdit() const;
  bool completeTextEdit(const EditorTextEdit &edit, std::string_view text, bool accept);
  // Texto vivo do IME, a cada tecla, ANTES de confirmar.
  //
  // O `AlertDialog` só devolvia o resultado final: até confirmar, o editor não
  // sabia o que estava sendo digitado e não podia desenhar nada. Com o campo
  // embutido a ponte entrega o texto e o cursor continuamente, e o editor
  // desenha o próprio campo.
  //
  // Rascunho, não comando: nada entra no documento nem no histórico aqui. A
  // única coisa que acontece antes de confirmar é a busca filtrar enquanto se
  // digita, que não toca em nada.
  bool updateTextDraft(const EditorTextEdit &edit, std::string_view text, u32 caret);
  // Quanto da superfície o teclado do sistema ocupa, 0..1. O campo se apoia
  // nesta borda em vez de ficar escondido atrás do teclado.
  void setPlatformImeFraction(float fraction);
private:
  u32 sceneUsersOf(const resources::AssetGuid &guid) const;
  void jumpToConsoleEntry(u32 index);
  bool assetRegistryDirty_ = false;
  void followCodeCaret();
  void placeCodeCaret(ui::UiPoint position);
public:

  // Reconstrói a lista de desenho e as instâncias do frame.
  // Vistas salvas (G6-A): comparar duas versões de um cenário exige repetir o
  // MESMO enquadramento. A vista guarda a câmera de órbita e a lente, entra no
  // documento e tem Desfazer como qualquer edição autoral.
  runtime::SceneView currentSceneView(std::string_view name) const {
    runtime::SceneView view;
    view.name = std::string(name);
    std::copy(camera_.target, camera_.target + 3, view.target);
    view.distance = camera_.distance;
    view.yaw = camera_.yaw;
    view.pitch = camera_.pitch;
    view.verticalFov = projection_.verticalFieldOfViewRadians;
    return view;
  }
  bool saveSceneView(std::string_view name) {
    if (isPlaying() || history_.isOpen()) return false;
    auto views = document_.views();
    if (!views.replace(currentSceneView(name))) return false;
    return history_.setViews(document_, views);
  }
  bool applySceneView(u32 index) {
    const auto *view = document_.views().at(index);
    if (!view) return false;
    finishCameraGesture(false);
    std::copy(view->target, view->target + 3, camera_.target);
    camera_.distance = view->distance;
    camera_.yaw = view->yaw;
    camera_.pitch = view->pitch;
    // Lente zero é "a do editor": uma vista antiga não impõe um campo de visão
    // que ela nunca guardou.
    if (view->verticalFov > 0) projection_.verticalFieldOfViewRadians = view->verticalFov;
    state_.viewSelected = index;
    state_.status = "Vista \"" + view->name + "\"";
    return true;
  }
  bool renameSceneView(u32 index, std::string_view name) {
    if (isPlaying() || history_.isOpen()) return false;
    auto views = document_.views();
    if (!views.rename(index, name)) return false;
    return history_.setViews(document_, views);
  }
  bool deleteSceneView(u32 index) {
    if (isPlaying() || history_.isOpen()) return false;
    auto views = document_.views();
    if (!views.remove(index)) return false;
    if (state_.viewSelected >= views.count()) state_.viewSelected = views.count() ? views.count() - 1 : 0;
    return history_.setViews(document_, views);
  }
  void update();
  // A linha "Na vista" do LOD Group selecionado, refeita a cada atualização.
  void refreshLodStatus();
  void refreshSkinningStatus();

  std::span<const ui::UiInstance> instances() const noexcept { return instances_; }
  const EditorScreenLayout &layout() const noexcept { return layout_; }
  const EditorScreenState &screen() const noexcept { return state_; }
  const EditorCamera &camera() const noexcept { return camera_; }
  const EditorViewport &view() const noexcept { return view_; }
  EditorDocument &document() noexcept { return document_; }
  const EditorMapScene &mapScene() const noexcept { return mapScene_; }
  EditorHistory &history() noexcept { return history_; }
  EditorEntityId selection() const noexcept { return state_.selection; }
  EditorSceneVersion sceneVersion() const noexcept { return {sceneEpoch_,document_.revision()}; }
  // Backend integration seam; no UI preview is advertised until a real target
  // renders this request and acknowledges publication successfully.
  EditorCameraPreview &cameraPreview() noexcept {return cameraPreview_;}
  EditorActionResult dispatch(const EditorActionRequest &request);
  bool playRequested() const noexcept { return playRequested_; }
  // Entra em Play como o botão (mesmas travas de código); falso enquanto travado.
  bool startPlay();
  void clearPlayRequest() noexcept { playRequested_ = false; }

  // **Editar é ver a cena PARADA.** A água não ondula, o casco não anda e a
  // física não integra enquanto alguém posiciona um objeto — senão o que se vê
  // não é um editor, é um vídeo com painéis por cima. A aba Play é o que solta.
  bool isPlaying() const noexcept { return state_.workspace == EditorWorkspace::Play; }
  // Hierarquia e Inspector abertos sobre o mundo em execução.
  bool playInspecting() const noexcept { return isPlaying() && playScene_.active() && state_.playInspect; }
  bool gameplayInputFocused() const noexcept {
    // Play hides authoring panels and their text fields. platformTextInput
    // advertises IME support for the whole Android session, not active focus.
    return isPlaying()&&!state_.playPaused;
  }
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
  bool importMap(std::span<const renderer::MapDrawRecord> draws, std::span<const renderer::MapMaterialRecord> materials = {}, bool instantiate = true, std::span<const u8> vertices = {}, std::span<const u32> indices = {}, u64 packageFingerprint = 0);

  // O pacote que o consumidor gráfico passou a ter depois de absorver a
  // geometria importada. A sessão adota exatamente isto — não uma reconstrução
  // própria —, senão o slot do editor e o da GPU poderiam divergir.
  struct PublishedGeometry {
    std::span<const renderer::MapDrawRecord> draws;
    std::span<const renderer::MapMaterialRecord> materials;
    std::span<const u8> vertices;
    std::span<const u32> indices;
  };
  // Quem sabe subir geometria para a GPU é o shell; a sessão não conhece Vulkan.
  // Recebe a biblioteca importada INTEIRA (ver `rebuildAuthoringLibrary`) e
  // devolve o pacote resultante.
  // As texturas vêm na MESMA chamada: `MapMaterialRecord::textureIndices` já
  // aponta para esta lista, e publicar uma sem a outra deixaria índices soltos.
  using GeometryPublisher = std::function<bool(std::span<const u8>, std::span<const u32>,
                                               std::span<const renderer::MapDrawRecord>,
                                               std::span<const renderer::MapMaterialRecord>,
                                               std::span<const renderer::SharedAuthoringTexture>,
                                               PublishedGeometry &)>;
  void setGeometryPublisher(GeometryPublisher publisher) { publishGeometry_ = std::move(publisher); }
  // Skin da biblioteca sendo publicada, lido pelo publicador DURANTE a chamada:
  // influências paralelas aos vértices recebidos e juntas por desenho recebido
  // (zero = estático). Fora da publicação, vazio.
  struct SkinningPublication {
    std::span<const u8> influences;
    std::span<const u32> drawJoints;
    // Blend shapes: deltas (MorphDeltaStride floats por alvo e vértice, vértice
    // a vértice) e, por desenho recebido, início em floats e número de alvos.
    std::span<const float> morphDeltas;
    std::span<const u32> drawMorphOffsets,drawMorphTargets;
    // S3: níveis de detalhe dos desenhos recebidos, na ordem desenho → nível.
    std::span<const renderer::MeshLodLevel> meshLods;
  };
  const SkinningPublication &skinningPublication() const noexcept { return skinningPublication_; }
  // Republica a geometria importada para um consumidor gráfico NOVO.
  //
  // Recriar a superfície reconstrói o renderer do zero, e a biblioteca de
  // autoria dele volta a ter só as primitivas internas. A cena continua
  // apontando para os desenhos do modelo importado, então a publicação do
  // documento passa a falhar a cada quadro: hierarquia intacta e viewport
  // vazio, que era exatamente o sintoma ao voltar do segundo plano.
  //
  // Não lê arquivo nenhum: os blocos por fonte já estão em memória. Verdadeiro
  // também quando não há nada importado, porque aí não há o que reidratar.
  bool republishGeometry(std::string &diagnostic);

  // O que uma operação no painel de arquivos mexeu, e em quem.
  struct ResourceChangeReport {
    // Recursos cujo caminho mudou. Renomear uma pasta move todos de uma vez.
    u32 retargeted = 0;
    // Objetos da cena que usam este recurso.
    u32 sceneUsers = 0;
    // Outros recursos que dependem dele (malha → material → textura).
    u32 registryDependents = 0;
    std::string diagnostic;
  };
  // Renomear e mover são a MESMA operação: o caminho muda, a identidade não.
  //
  // É por isso que a cena não é tocada aqui. Os objetos guardam o GUID do
  // recurso, não o caminho dele — mover um arquivo não pode quebrar um objeto,
  // e um teste de host exige exatamente isso.
  //
  // Falha fechada e atômica: o registro é conferido ANTES de o arquivo sair do
  // lugar, e se a gravação falhar o registro volta. Meia pasta apontando para o
  // caminho novo e meia para o velho não é um estado do qual se saia.
  bool moveResource(const std::string &relative, const std::string &destination,
                    ResourceChangeReport &report);
  // Apagar CONFRONTA quem usa. Sem `force` recusa e diz quantos objetos da cena
  // e quantos recursos dependem dele; com `force` apaga e os objetos que
  // apontavam para ele ficam sem malha, visivelmente, em vez de apontar para a
  // malha que por acaso ocupar o índice.
  bool deleteResource(const std::string &relative, bool force, ResourceChangeReport &report);
  // O registro precisa ir ao disco AGORA, não no próximo salvar.
  //
  // Renomear e apagar mexem no disco na hora. Até o registro acompanhar, ele
  // aponta para um caminho que não existe mais — e o projeto reaberto nesse
  // intervalo abre com os objetos sem malha. Medido no aparelho: renomear a
  // fonte e reabrir antes de salvar apagava o veículo da tela com a hierarquia
  // inteira preservada.
  bool assetRegistryDirty() const noexcept { return assetRegistryDirty_; }
  void clearAssetRegistryDirty() noexcept { assetRegistryDirty_ = false; }

  struct ModelImportReport {
    u32 objects = 0;
    // Verdadeiro quando a fonte já estava no projeto: a geometria é publicada
    // de novo e os objetos que já existem apontam para ela. Reimportar NÃO
    // duplica os objetos — quem quer outra cópia instancia de novo, e isso é
    // um ato diferente.
    bool reimported = false;
    // Objetos sem malha criados para segurar a hierarquia: grupos, pivôs,
    // alvos. Eles são o que faz mover a carroceria levar a porta junto.
    u32 groups = 0;
    // Campo legado do relatório. Matrizes incompatíveis agora recusam a
    // publicação, preservando a transformação em vez de aplicar identidade.
    u32 shearedNodes = 0;
    // LOD Groups gerados pela convenção `_LOD0`, `_LOD1`... dos nós, e o que
    // ficou de fora deles.
    u32 lodGroups = 0;
    // G6-B: componentes de skin e animação criados na instanciação.
    u32 skinnedMeshes = 0, animations = 0;
    std::vector<std::string> lodNotes;
    std::string diagnostic;
    bool cancelled = false;
    resources::AssetGuid source{};
    u32 skippedTextures = 0, skippedAnimations = 0, skippedSkins = 0;
    // M08.2: como a revisão nova se relaciona com a anterior, e o que a
    // reconciliação fez com as instâncias da cena.
    resources::ImportMatchReport match;
    ImportReconcileReport reconcile;
  };
  // Importa um GLB e instancia seus nós na cena, num único passo de desfazer.
  // `sourceName` é o caminho do arquivo dentro do projeto — é ele que vira o
  // caminho do recurso no registro, e trocá-lo depois não muda a identidade.
  bool importModel(std::span<const u8> bytes, std::string_view sourceName,
                   const resources::GltfImportProgress &progress, ModelImportReport &report);
  // Parsing can run on a worker; publication and instantiation belong to the
  // editor thread. Registering a resource never creates scene objects.
  // Ambiguidade na correspondência de nós nunca é resolvida pela ordem da lista
  // sem escolha explícita: `Refuse` devolve falso com `report.match` preenchido.
  // `excludedNodes`: nós que o perfil da fonte não traz para a cena (G2). A
  // lista chega explícita, e não de um estado escondido da sessão, porque é o
  // perfil QUE ESTÁ SENDO PUBLICADO que manda — não o salvo da vez anterior.
  bool publishModel(const resources::GltfImport &model, std::string_view hash,
                    std::string_view sourceName, ModelImportReport &report,
                    resources::ImportAmbiguityPolicy policy=resources::ImportAmbiguityPolicy::Refuse,
                    std::span<const resources::AssetGuid> excludedNodes={});
  // Reabertura do projeto em lote (R1). As fontes chegam já interpretadas por
  // um worker; todas entram no mesmo candidato, a biblioteca é publicada UMA vez
  // e só então cada fonte reconcilia a cena. Abrir N fontes deixa de custar N
  // publicações acumuladas (A, A+B, A+B+C...). Uma fonte recusada fica com o
  // diagnóstico no próprio relatório e não impede as outras.
  struct ReopenedSource {
    resources::GltfImport model;
    std::string hash;
    std::string sourceName;
  };
  bool reopenSources(std::vector<ReopenedSource> &sources, std::vector<ModelImportReport> &reports,
                     std::string &diagnostic);
  // R4: texturas do projeto que a cena salva usa, lidas ANTES da publicação da
  // reabertura. Assim elas sobem na mesma publicação das fontes, em vez de uma
  // segunda publicação quando a cena é carregada. `load` descarta a antecipação.
  void anticipateSceneTextures(const char *path, u64 fingerprint);
  bool instantiateModel(resources::AssetGuid source, ModelImportReport &report, bool wrapMultipleRoots=true);
  // Fonte em pasta (S0): `companions` são os arquivos do modelo já copiados
  // para o preparo, publicados na mesma transação que o principal; `contentHash`
  // é o do conteúdo inteiro (principal + manifesto), não só dos bytes do `.gltf`.
  struct FolderCompanion {std::string staged,relative;};
  bool commitModelImport(std::span<const u8> bytes, const resources::GltfImport &model,
                         const std::string &path, const std::string &expectedHash, ModelImportReport &report,
                         resources::ImportAmbiguityPolicy policy=resources::ImportAmbiguityPolicy::Refuse,
                         std::span<const resources::AssetGuid> excludedNodes={},
                         std::span<const FolderCompanion> companions={}, std::string_view contentHash={});
  // `prepared` é o perfil com que o worker preparou `model`.
  // `sourceReport`: a medição malha a malha (G6-A) já feita no trabalhador. Numa
  // cena grande ela custa segundos (Sponza: 2,7 s no host) e, feita aqui na
  // thread do editor, travava o toque (ANR). Nulo: medida aqui, como antes.
  void showImportPreview(std::string path, const resources::GltfImport &model, std::string_view contentHash={},
                         const resources::ImportProfile &prepared={},
                         const resources::ImportSourceReport *sourceReport=nullptr);
  using EnvironmentMapEntry=std::pair<resources::AssetGuid,renderer::SharedEnvironmentMap>;
  const std::vector<EnvironmentMapEntry> &environmentMaps() const {return environmentMaps_;}
  renderer::SharedEnvironmentMap findEnvironmentMap(const resources::AssetGuid &guid) const {
    for(const auto &[id,map]:environmentMaps_) if(id==guid) return map;
    return {};
  }
  // Worker e importação interativa convergem aqui: só recursos válidos e
  // registrados entram na biblioteca publicada pelo renderer.
  bool adoptEnvironmentMap(const resources::AssetGuid &guid,renderer::SharedEnvironmentMap map,
                           std::string &diagnostic);
  bool commitEnvironmentMap(const std::string &path,std::span<const u8> bytes,
                            const std::string &expectedHash,renderer::SharedEnvironmentMap prepared,
                            std::string &diagnostic);
  bool commitEnvironmentMap(const std::string &path,std::span<const u8> bytes,
                            const std::string &expectedHash,renderer::SharedEnvironmentMap prepared,
                            const resources::EnvironmentMapImportSettings &settings,std::string &diagnostic);
  void showEnvironmentImportPreview(std::string path,const renderer::EnvironmentMapResource &map,
                                    const resources::EnvironmentMapImportSettings &settings={});
  void beginEnvironmentImportPreparation(std::string_view path,
                                         const resources::EnvironmentMapImportSettings &settings) {
    beginImportPreparation(path);state_.importEnvironment=true;state_.importTexture=false;
    state_.environmentImportSettings=settings;state_.environmentImportReprepare=false;
  }
  const resources::EnvironmentMapImportSettings &environmentMapImportSettings() const {
    return state_.environmentImportSettings;
  }
  bool takeEnvironmentImportReprepare(resources::EnvironmentMapImportSettings &settings) {
    if(!std::exchange(state_.environmentImportReprepare,false)) return false;
    settings=state_.environmentImportSettings;return true;
  }
  // Linha extra na preparação, dita por quem preparou os bytes (por exemplo, as
  // dependências de um .gltf empacotadas no GLB).
  void noteImportPreview(std::string line) {state_.importSummary+="\n"+std::move(line);}
  // Formatos de textura que o renderer desta sessão amostra (Entrega 4): KTX2
  // só vira ASTC 4x4 quando o aparelho confirmou suporte. Vale para a importação
  // interativa e para a reabertura de fontes do projeto.
  void setImportAstc4x4(bool supported) {importLimits_.astc4x4=supported;}
  // R4: o renderer deste aparelho descarta faces de trás por material? Sem isso,
  // a escolha de faces fica guardada, mas o painel diz que não tem efeito aqui.
  void setMaterialCullingAvailable(bool available) {state_.materialCulling=available;}
  const resources::GltfImportLimits &importLimits() const {return importLimits_;}
  // R2: teto da residência de texturas de TODAS as fontes juntas, aplicado a
  // cada publicação. O relatório é o da última publicação.
  void setImportTextureBudget(u64 bytes) {importTextureBudget_=bytes;}
  u64 importTextureBudget() const {return importTextureBudget_;}
  const resources::TextureBudgetReport &textureResidency() const {return textureResidency_;}
  // Abre o importador em Propriedades. `path` já conhecido (reimportação) carrega
  // o perfil guardado daquela fonte; sem caminho, o padrão do projeto.
  void beginImportPreparation(std::string_view path={});
  // R3: perfis. Fonte → padrão do projeto → embutido; arquivo inválido cai no próximo.
  resources::ImportProfile importProfileFor(const resources::AssetGuid &source) const;
  resources::ImportProfile projectImportProfile() const;
  resources::ImportProfile importProfileForPath(std::string_view path) const;
  resources::ImportProfile newSourceImportProfile() const;
  bool saveImportProfile(const resources::AssetGuid &source, const resources::ImportProfile &profile);
  bool saveProjectImportProfile(const resources::ImportProfile &profile);
  // Rascunho do painel e os limites que ele produz neste aparelho.
  resources::ImportProfile importProfileDraft() const {
    resources::ImportProfile draft;
    draft.scale=state_.importScale;draft.maximumTextureDimension=state_.importTextureDimension;
    draft.normals=state_.importNormals;draft.normalWeighting=state_.importNormalWeighting;
    draft.smoothingAngle=state_.importSmoothingAngle;draft.tangents=state_.importTangents;
    draft.importCameras=state_.importCameras;draft.importLights=state_.importLights;
    draft.textureCompression=state_.importTextureCompression;
    draft.textureStreaming=state_.importTextureStreaming;
    draft.generateLods=state_.importGenerateLods;draft.maximumLodLevels=state_.importLodLevels;
    draft.optimizePolygonOrder=state_.importOptimizeOrder;
    draft.textureStreamingPriority=state_.importTextureStreamingPriority;
    draft.excludedNodes=state_.importExcludedNodes;
    return draft;
  }
  // O perfil com que a prévia na tela FOI preparada, no mesmo formato do
  // rascunho. Uma única função: campo novo esquecido aqui fazia a tela liberar
  // "Só recurso" enquanto a sessão recusava o toque (achado no aparelho, S1).
  resources::ImportProfile importProfilePrepared() const {
    resources::ImportProfile prepared;
    prepared.scale=state_.importPreparedScale;prepared.maximumTextureDimension=state_.importPreparedTextureDimension;
    prepared.normals=state_.importPreparedNormals;prepared.normalWeighting=state_.importPreparedNormalWeighting;
    prepared.smoothingAngle=state_.importPreparedSmoothingAngle;prepared.tangents=state_.importPreparedTangents;
    prepared.importCameras=state_.importPreparedCameras;prepared.importLights=state_.importPreparedLights;
    prepared.textureCompression=state_.importPreparedTextureCompression;
    prepared.generateLods=state_.importPreparedGenerateLods;prepared.maximumLodLevels=state_.importPreparedLodLevels;
    prepared.optimizePolygonOrder=state_.importPreparedOptimizeOrder;
    return prepared;
  }
  resources::GltfImportLimits importLimitsFor(const resources::ImportProfile &profile) const {
    return resources::applyImportProfile(importLimits_,profile);
  }
  // O painel pediu nova preparação com o rascunho; o shell refaz o worker.
  bool takeImportReprepare() {return std::exchange(state_.importReprepare,false);}
  resources::ImportAmbiguityPolicy importAmbiguityPolicy() const {
    return state_.importAmbiguityChoice==1?resources::ImportAmbiguityPolicy::MatchInOrder:
           state_.importAmbiguityChoice==2?resources::ImportAmbiguityPolicy::TreatAsNew:resources::ImportAmbiguityPolicy::Refuse;
  }
  // Mapa de nós publicado de uma fonte carregada neste processo.
  const resources::ImportNodeMap *importNodeMap(const resources::AssetGuid &source) const {
    for(const auto &block:importedSources_) if(block.guid==source && block.map.revision) return &block.map;
    return nullptr;
  }
  // Vínculo de instância (M08.2): o que difere da fonte e os comandos reais do
  // inspetor. Todos passam pelo histórico.
  u32 importLinkOverrides(EditorEntityId id) const {return importOverrides(document_,id);}
  // Materiais por slot (Entrega 2). O ALCANCE é sempre explícito: `Instance`
  // escreve a substituição no slot deste objeto (histórico); `Shared` escreve o
  // MaterialAsset e muda todos os slots que o usam.
  enum class MaterialScope : u8 { Instance, Shared };
  const std::vector<resources::MaterialAsset> &materialAssets() const {return materials_;}
  const resources::MaterialAsset *findMaterialAsset(const resources::AssetGuid &guid) const {
    for(const auto &material:materials_) if(material.guid==guid) return &material;
    return nullptr;
  }
  // Cria `Materiais/<nome>.material` com os valores efetivos do slot e liga o
  // slot a ele. Devolve a identidade, ou inválida com `diagnostic`.
  resources::AssetGuid createMaterialFromSlot(EditorEntityId id,u32 slot,std::string &diagnostic);
  const std::vector<resources::EnvironmentProfile> &environmentProfiles() const {return environmentProfiles_;}
  const resources::EnvironmentProfile *findEnvironmentProfile(const resources::AssetGuid &guid) const {
    for(const auto &profile:environmentProfiles_) if(profile.guid==guid) return &profile;
    return nullptr;
  }
  resources::AssetGuid createEnvironmentProfile(EditorEntityId id,u64 instance,std::string &diagnostic);
  bool updateEnvironmentProfile(EditorEntityId id,u64 instance,std::string &diagnostic);
  // Liga o slot a um material do projeto; identidade inválida volta à fonte.
  bool assignSlotMaterial(EditorEntityId id,u32 slot,const resources::AssetGuid &material);
  bool repairComponentResource(EditorEntityId id,u64 instance,resources::AssetGuid from,resources::AssetGuid to,
                               EditorSceneVersion expectedVersion,std::string &diagnostic);
  bool repairSharedTexture(resources::AssetGuid material,resources::AssetGuid from,resources::AssetGuid to,
                           u32 expectedMaterialRevision,EditorSceneVersion expectedVersion,std::string &diagnostic);
  bool repairSceneResource(resources::AssetGuid from,resources::AssetGuid to,EditorSceneVersion expectedVersion,std::string &diagnostic);
  // `field` indexa os números do componente de malha (cor, rugosidade...).
  bool setSlotMaterialValue(EditorEntityId id,u32 slot,MaterialScope scope,u32 field,float value,std::string &diagnostic);
  bool clearSlotMaterialOverride(EditorEntityId id,u32 slot);
  // R4: texturas do projeto. Um recurso `Texture` é o próprio arquivo PNG/JPEG
  // em `Texturas/`; a identidade no registro é o que slots e materiais guardam.
  struct ProjectTexture {
    resources::AssetGuid guid;
    std::string path,name;
    u32 width=0,height=0; // do cabeçalho; zero quando o arquivo não abre
    // O conteúdo do arquivo não bate com o registrado (foi trocado fora do editor).
    bool changed=false;
  };
  const std::vector<ProjectTexture> &projectTextures() const {return textures_;}
  // R4: textura escolhida em Arquivos mostrada em Propriedades (imagem, perfil,
  // usuários) e gerenciador de texturas (grade, busca e filtros).
  void openTextureInspector(u32 projectTextureIndex);
  void openTextureManager(std::string folder={});
  // 0 todas, 1 não usadas, 2 ausentes, 3 alteradas, 4 sem alfa, 5 acima do teto.
  static constexpr u8 TextureFilterCount=6;
  void setTextureFilter(u8 filter) {state_.textureFilter=filter<TextureFilterCount?filter:0;state_.textureManagerPage=0;}
  // Bloco F: lista do gerenciador — texturas do projeto ou das fontes importadas.
  void setTextureManagerSources(bool sources) {state_.textureManagerSources=sources;state_.textureManagerPage=0;}
  void setTextureQuery(std::string query) {state_.textureQuery=std::move(query);state_.textureManagerPage=0;}
  // R4: perfil de uma textura do projeto (interpretação, tamanho, mips, bordas,
  // anisotropia). Aplicar grava o arquivo e republica as texturas.
  resources::TextureProfile textureProfileFor(const resources::AssetGuid &texture) const;
  bool setTextureProfile(const resources::AssetGuid &texture,const resources::TextureProfile &profile,
                         std::string &diagnostic,bool recordHistory=true);
  // R4: o que subiu para a GPU de uma textura na última publicação, uma entrada
  // por combinação de uso (espaço de cor e sampler); vazio quando não é usada.
  struct TextureResidency {
    u32 width=0,height=0,levels=0;
    u64 bytes=0;
    bool srgb=true;
    u32 sampler=0;
  };
  std::vector<TextureResidency> textureResidencyOf(const resources::AssetGuid &texture) const;
  // Objetos da cena com um slot que usa a textura, mais materiais do projeto que a usam.
  u32 textureUsersOf(const resources::AssetGuid &guid) const;
  // R4: prévia de texturas. Uma miniatura por chamada (o seletor chama a cada
  // atualização); o visualizador escreve o nível e o canal escolhidos no atlas.
  bool generatePendingTextureThumbnail();
  bool openTextureViewer(u32 projectTextureIndex);
  void closeTextureViewer() {state_.textureViewer=false;state_.textureViewerSource=false;state_.textureProfileDirty=false;state_.textureViewerExpanded=false;}
  bool stepTextureViewerLevel(int delta);
  bool cycleTextureViewerChannel();
  bool cycleTextureViewerZoom();
  bool cycleTextureViewerBackground();
  // O atlas de prévia quando mudou desde a última entrega ao renderer; nulo senão.
  const std::vector<u8> *takePreviewAtlas() {
    if(!preview_.dirty()) return nullptr;
    preview_.markClean();
    return &preview_.pixels();
  }
  void republishPreviewAtlas() { preview_.markDirty(); }
  const ProjectTexture *findProjectTexture(const resources::AssetGuid &guid) const {
    for(const auto &texture:textures_) if(texture.guid==guid) return &texture;
    return nullptr;
  }
  // Extrai as imagens PNG/JPEG embutidas de uma fonte GLB do projeto para
  // `Texturas/<fonte>/`, com os bytes originais, e registra cada uma. Imagem com
  // o mesmo conteúdo de uma textura já registrada é reaproveitada, não duplicada.
  // Tudo ou nada: falha ao gravar não deixa registro parcial.
  struct TextureExtraction {
    u32 created=0,reused=0,skipped=0; // skipped: KTX2 ou formato sem arquivo próprio
    std::vector<resources::AssetGuid> textures;
  };
  bool extractSourceTextures(const std::string &sourcePath,TextureExtraction &report,std::string &diagnostic);
  // Troca a textura de um binding (0 cor base, 1 normal, 2 metálico/rugosidade,
  // 3 emissão). `Instance`: no slot deste objeto, pelo histórico. `Shared`: no
  // MaterialAsset do slot, em todos os usos. Identidade inválida herda;
  // `scene::MaterialTextureNone` tira a textura.
  // R4: modo de alfa, corte e faces do slot, no mesmo contrato de alcance.
  bool setSlotSurface(EditorEntityId id,u32 slot,MaterialScope scope,const scene::MaterialSurface &surface,std::string &diagnostic);
  // R4: conjunto de UV, repetição e filtro de um binding, na instância ou no material do projeto.
  bool setSlotSampling(EditorEntityId id,u32 slot,u32 binding,MaterialScope scope,const scene::MaterialSampling &sampling,
                       std::string &diagnostic);
  // R4: canais do metal/rugosidade, oclusão, inversão Y do normal e origem do alfa.
  bool setSlotChannels(EditorEntityId id,u32 slot,MaterialScope scope,const scene::MaterialChannels &channels,std::string &diagnostic);
  bool setSlotTexture(EditorEntityId id,u32 slot,u32 binding,MaterialScope scope,const resources::AssetGuid &texture,
                      std::string &diagnostic);
  // Nome do material da FONTE usado pela primitiva de identidade `draw`.
  std::string sourceMaterialName(const resources::AssetGuid &draw) const;
  // Editar um material compartilhado não muda a revisão do documento; o shell
  // consome este sinal para republicar os desenhos.
  bool takeAppearanceChanged() {return std::exchange(appearanceChanged_,false);}
  // Candidatos de toque do viewport, recalculados agora. Inspeção: é o que
  // prova que cada slot de um objeto seleciona o MESMO objeto.
  std::span<const EditorPickCandidate> pickCandidates() {buildPickCandidates();return candidates_;}
  bool revertImportLink(EditorEntityId id,u32 mask);
  u32 unlinkImport(EditorEntityId id);
  bool resolveImportOrphan(EditorEntityId id,bool keep);
  void showImportFailure(std::string message) {
    setImportStatus(message,EditorConsoleSeverity::Error);state_.importPanel=true;state_.importReady=false;
    state_.importError=true;state_.importPage=0;state_.importStatus="Importação não concluída";state_.importSummary=std::move(message);
  }
  void closeImportPreview() {state_.importPanel=false;state_.importReady=false;}
  bool takeImportAccept() {return std::exchange(state_.importAccept,false);}
  bool takeImportIntoScene() {return std::exchange(state_.importIntoScene,false);}
  bool takeImportCancel() {return std::exchange(state_.importCancel,false);}
  std::string takeReimportPath() {return std::exchange(reimportPath_,{});}
  std::string takeEnvironmentReimportPath() {return std::exchange(environmentReimportPath_,{});}
  std::string takeTextureReimportPath() {return std::exchange(textureReimportPath_,{});}
  // O plano da grade para o quadro: política do editor, desenho do renderer.
  // Fora do workspace de cena, com a grade desligada ou em execução, ele volta
  // desabilitado — a grade é ferramenta de autoria, não elemento do jogo.
  renderer::GridPlan gridPlan() const {
    if(!state_.showGrid || state_.workspace!=EditorWorkspace::Scene || isPlaying()) return {};
    return buildEditorGridPlan(view_);
  }
  const resources::AssetRegistry &assets() const { return assets_; }
  // Pedido de importação levantado pela interface, consumido pelo shell. O
  // editor não abre o seletor: ele não conhece Android.
  // Link externo pedido pela interface (ajuda do componente), só https.
  std::string consumeExternalLink() {std::string link;link.swap(state_.externalLink);return link;}
  bool consumeModelImportRequest() {
    const bool requested=state_.modelImportRequested;
    state_.modelImportRequested=false;
    if(requested) {state_.importEnvironment=false;state_.importTexture=false;}
    return requested;
  }
  bool consumeFolderImportRequest() {
    const bool requested=state_.folderImportRequested;
    state_.folderImportRequested=false;
    if(requested) {state_.importEnvironment=false;state_.importTexture=false;}
    return requested;
  }
  // Etapa corrente de um trabalho longo (R1): só a barra de estado, sem entrada
  // no console a cada troca, e reafirmada enquanto o trabalho durar para que um
  // aviso de outra origem não esconda que o projeto ainda está abrindo.
  void setWorkStatus(std::string_view message) {
    if(state_.status!=message) state_.status=std::string(message);
  }
  void setImportStatus(std::string message, EditorConsoleSeverity severity=EditorConsoleSeverity::Info) {
    state_.status=message;state_.importStatus=message;
    EditorConsoleEntry entry;entry.origin=EditorConsoleOrigin::Importer;entry.severity=severity;
    entry.message=std::move(message);entry.project=files_.rootPath();console_.add(std::move(entry));
  }
  // O registro do projeto, em texto, para o shell gravar ao lado da cena; e a
  // carga, feita antes de reimportar as fontes.
  std::string serializeAssets() const { return assets_.serialize(); }
  // O registro e, com ele, os materiais do projeto que ele declara.
  bool loadAssets(std::string_view text) {
    if(!resources::AssetRegistry::deserialize(text, assets_)) return false;
    environmentMaps_.clear();
    loadMaterialAssets();
    loadEnvironmentProfiles();
    return true;
  }
  bool extractMap(std::vector<renderer::MapDrawState> &out) const {
    if(!mapScene_.extract(document_, out)) return false;
    applyLodGroups(document_, out);
    return true;
  }
  // A vista que escolhe os níveis dos LOD Groups: a câmera da cena no Play,
  // quando existe, e a do editor no resto — a mesma que desenha o quadro.
  runtime::LodView lodView() const {
    runtime::LodView lod;
    if(isPlaying() && playScene_.active()) {
      const auto camera=resolveSceneCamera(playScene_.document());
      if(camera.entity) {
        std::copy(camera.position,camera.position+3,lod.position);
        lod.verticalFov=camera.verticalFov*.017453292519943295f;
        lod.orthographicHalfHeight=camera.projection==scene::CameraProjection::Orthographic?camera.orthographicHalfHeight:0;
        return lod;
      }
    }
    const auto &frustum=view_.frustum;
    std::copy(frustum.cameraPosition,frustum.cameraPosition+3,lod.position);
    lod.verticalFov=2*std::atan(frustum.tangentHalfVertical);
    lod.orthographicHalfHeight=renderer::isOrthographic(frustum)?frustum.orthographicHalfHeight:0;
    return lod;
  }
  // Verdadeiro quando a câmera cruzou uma transição desde a última extração.
  // Fora do Play a cena só é republicada quando a revisão muda; é esta pergunta
  // que faz a troca de nível chegar à tela — pela publicação de poses, porque
  // esconder desenhos não muda a topologia.
  bool lodSelectionChanged() const {
    const auto &graph=isPlaying()&&playScene_.active()?playScene_.document():document_;
    // Uma troca animada muda a cobertura a cada quadro até terminar.
    if(runtime::lodCrossFadeRunning(lodClock_,lastWallSeconds_)) return true;
    // A própria consulta inicia a troca quando a câmera cruza o limite. Esse
    // primeiro quadro ainda tem fator zero e pode ser visualmente idêntico ao
    // anterior; por isso o relógio real precisa reter a transição e o estado
    // "correndo" também conta como mudança. Copiar o relógio aqui descartava
    // o início, então a cena não era republicada nos quadros seguintes.
    const auto next=runtime::lodObjectStates(graph,lodView(),&lodClock_,lastWallSeconds_);
    return runtime::lodCrossFadeRunning(lodClock_,lastWallSeconds_)||next!=lodStates_;
  }
  // As luzes saem do MESMO grafo que a câmera e os desenhos: em execução, o
  // mundo de Play; fora dele, o documento autoral. É o que faz um script mover
  // ou apagar uma luz e a tela mudar, sem nenhum caminho separado de execução.
  bool extractLights(std::vector<renderer::SceneLight> &out) const {
    return runtime::collectSceneLights(isPlaying() && playScene_.active() ? playScene_.document() : document_, out);
  }
  bool extractEnvironment(renderer::SceneEnvironment &out) const {
    return runtime::collectSceneEnvironment(
        isPlaying() && playScene_.active() ? playScene_.document() : document_, out,environmentProfiles_);
  }
  bool extractEnvironmentVolumes(std::vector<renderer::SceneEnvironmentVolume> &out) const {
    return runtime::collectSceneEnvironmentVolumes(
        isPlaying() && playScene_.active() ? playScene_.document() : document_, out,environmentProfiles_);
  }
  SceneCameraPose sceneCameraPose() const {
    return resolveSceneCamera(isPlaying()&&playScene_.active()?playScene_.document():document_);
  }
  bool extractPlayMap(std::vector<renderer::MapDrawState> &out,
                      const runtime::InputDeviceState &platformInput={},bool platformFocus=true) {
    if(!isPlaying()) return false;
    if(!playScene_.active()) {
      if(EditorPlayScene::unresolvedEntity(document_)!=kInvalidEntity) {
        state_.status="Play indisponível: há componentes de tipo ausente";
        reportProblem(EditorConsoleSeverity::Error,state_.status);return false;
      }
      runtimeCodeGeneration_=code_.publishedGeneration();
      if(!playScene_.start(document_,mapScene_)) {
        state_.status=!playScene_.scriptDiagnostics().empty()?playScene_.scriptDiagnostics():playScene_.physicsError().empty()?"Falha ao preparar a cena para Play":playScene_.physicsError();
        reportProblem(EditorConsoleSeverity::Error,state_.status);
        return false;
      }
      reportProblem(EditorConsoleSeverity::Info,"Play iniciado · código publicado "+std::to_string(runtimeCodeGeneration_));
      playLastSeconds_=sceneTime_;
    }
    const double elapsed=std::max(0.0,static_cast<double>(sceneTime_)-playLastSeconds_);
    playLastSeconds_=sceneTime_;
    playScene_.pause(state_.playPaused);
    if(state_.playPaused) playTouches_.cancel();
    // O toque vira ESTADO DE DISPOSITIVO; quem decide o que ele significa é o
    // mapa de ações do projeto. Pausado, o gameplay perde o foco: as ações leem
    // zero e nenhum botão fica preso ao retomar.
    const auto actions=playTouches_.consumeInput();
    runtime::InputDeviceState device;
    device.moveX=actions.moveRight;device.moveY=actions.moveForward;
    device.lookX=actions.lookScreenX;device.lookY=actions.lookScreenY;
    device.touchButtons=(jumpPressed_?1u:0u)|(secondaryPressed_?2u:0u);
    device.keys=platformInput.keys;
    device.gamepadButtons=platformInput.gamepadButtons;
    device.gamepadAxes=platformInput.gamepadAxes;
    jumpPressed_=false;
    secondaryPressed_=false;
    playScene_.setInputFocus(gameplayInputFocused()&&platformFocus);
    playScene_.submitInput(device);
    const auto &input=playScene_.input();
    const auto &map=input.map();
    float look[2]{0,0},move[2]{0,0};
    input.axis2(map.lookAction(),look);
    input.axis2(map.moveAction(),move);
    auto view=resolveSceneCamera(playScene_.document());
    const auto *viewEntity=playScene_.document().find(view.entity);
    if(viewEntity&&cameraLook(*viewEntity)) {
      if(!applyCameraLook(*playScene_.executionGraph(),view.entity,look[0],look[1])) return false;
      view=resolveSceneCamera(playScene_.document());
      viewEntity=playScene_.document().find(view.entity);
    }
    const auto *controlled=playScene_.document().find(state_.selection);
    for(auto *ancestor=viewEntity;ancestor;ancestor=playScene_.document().find(ancestor->parent))
      if(characterComponent(*ancestor)) {controlled=ancestor;break;}
    if(!controlled) controlled=playScene_.document().find(state_.selection);
    if(controlled && characterComponent(*controlled)) {
      if(!playScene_.setCharacterMove(controlled->id,move[0],move[1],view.entity?view.yaw:0)) return false;
      if(input.justPressed(map.jumpAction())) playScene_.jumpCharacter(controlled->id);
    }
    if(state_.playStepRequested) {
      state_.playStepRequested=false;
      if(!playScene_.step()) return false;
    }
    if(!playScene_.advance(elapsed)) return false;
    if(!playScene_.scriptDiagnostics().empty()) state_.status=playScene_.scriptDiagnostics();
    if(!playScene_.extract(mapScene_,out)) return false;
    applyLodGroups(playScene_.document(),out);
    return true;
  }
  EditorEntityId instantiateAsset(u32 index, EditorEntityId parent, const float worldPosition[3], const EditorAssetInstantiation *options=nullptr);
  // Instancia DENTRO de uma transação já aberta. É o que permite um modelo de
  // cena inteiro caber num passo de Desfazer; `instantiateAsset` é esta mesma
  // operação com a transação própria do gesto avulso.
  EditorEntityId instantiateAssetInTransaction(u32 index, EditorEntityId parent, const float worldPosition[3],
                                               const EditorAssetInstantiation *options=nullptr);
  // Modelos de cena (editor/editor_scene_template.h): monta o cenário do modelo
  // `index` na cena aberta, com as vistas salvas que vêm com ele, num passo só.
  bool createSceneTemplate(u32 index);
  // Slot do cubo autoral na biblioteca, ou zero quando a biblioteca não o traz.
  u32 boxAssetSlot() const;
  // Configurações de renderização do projeto (painel Qualidade). O shell entrega
  // o que leu do projeto; o painel edita um rascunho; "Aplicar" levanta o pedido
  // que o shell consome para gravar o arquivo e reconstruir o renderer. O editor
  // não conhece Vulkan nem sabe reconstruir nada.
  const renderer::ProjectRenderingSettings &renderingSettings() const noexcept { return renderingSettings_; }
  void setRenderingSettings(const renderer::ProjectRenderingSettings &settings) {
    renderingSettings_ = settings;
    state_.qualityDraft = settings;
    state_.qualityApplied = settings;
    state_.qualityDirty = false;
    renderingSettingsRequested_ = false;
    renderingSettingsRequestInFlight_ = false;
    qualityRevision_ = 0;
    renderingSettingsRequestRevision_ = 0;
  }
  bool consumeEnvironmentImportRequest() {
    const bool requested=std::exchange(state_.environmentImportRequested,false);
    if(requested) {state_.importEnvironment=true;state_.importTexture=false;}
    return requested;
  }
  bool consumeTextureImportRequest() {
    const bool requested=std::exchange(state_.textureImportRequested,false);
    if(requested) {state_.importEnvironment=false;state_.importTexture=true;}
    return requested;
  }
  const resources::TextureProfile &textureImportSettings() const noexcept {return state_.textureImportSettings;}
  void beginTextureImportPreparation(std::string_view path,const resources::TextureProfile &settings) {
    beginImportPreparation(path);state_.importTexture=true;state_.importEnvironment=false;
    state_.textureImportSettings=state_.textureImportPreparedSettings=settings;
    state_.textureImportReprepare=false;
  }
  bool takeTextureImportReprepare(resources::TextureProfile &out) {
    if(!std::exchange(state_.textureImportReprepare,false)) return false;
    out=state_.textureImportSettings;return true;
  }
  void showTextureImportPreview(std::string path,const resources::PreparedTextureImport &prepared,
                                const resources::TextureProfile &settings);
  bool commitTextureImport(const std::string &path,std::span<const u8> bytes,const std::string &expectedHash,
                           const resources::PreparedTextureImport &prepared,const resources::TextureProfile &settings,
                           std::string &diagnostic);
  bool takeRenderingSettingsRequest(renderer::ProjectRenderingSettings &out) {
    if (!renderingSettingsRequested_) return false;
    renderingSettingsRequested_ = false;
    renderingSettingsRequestInFlight_ = true;
    out = requestedRenderingSettings_;
    return true;
  }
  // Confirma o commit feito pelo shell. Falha de disco mantém o rascunho e o
  // botão Aplicar ativos; só sucesso passa a ser o estado autoral aplicado.
  void completeRenderingSettingsRequest(bool success) {
    if (!renderingSettingsRequestInFlight_) return;
    renderingSettingsRequestInFlight_ = false;
    if (success) {
      renderingSettings_ = requestedRenderingSettings_;
      state_.qualityApplied = renderingSettings_;
      state_.qualityDirty = qualityRevision_ != renderingSettingsRequestRevision_;
      state_.status = state_.qualityDirty ? "Qualidade aplicada; há novas alterações" : "Qualidade aplicada";
    } else {
      state_.qualityDirty = true;
      state_.status = "Não foi possível salvar a qualidade; revise o armazenamento e tente novamente";
    }
  }
  // O que o renderer está fazendo agora, para o rodapé do painel.
  void setRenderStats(u32 width, u32 height, float gpuMilliseconds,
                      std::string_view detectedLevel);
  // Entradas temporais que o renderer produz nesta vista. `history` diz se o
  // histórico é do TAA nativo (as vistas de histórico e rejeição só existem ali).
  void setTemporalDebugAvailable(bool available,bool motionAvailable=false,bool history=true) {
    state_.qualityTemporalAvailable=available;
    state_.qualityMotionAvailable=available && motionAvailable;
    state_.qualityHistoryDiagnostics=available && history;
    if(!available || !temporalDebugViewAvailable(state_.qualityTemporalDebug)) state_.qualityTemporalDebug=0;
  }
  // Capacidade do aparelho e o que o renderer executou no último quadro.
  void setTemporalUpscalerStatus(renderer::TemporalUpscalerAvailability armAsr,
                                 renderer::TemporalUpscalerAvailability fsr2,
                                 renderer::UpscalingFilter executed,
                                 renderer::TemporalUpscalerAvailability status,bool nativeTaa) {
    state_.qualityArmAsr=armAsr;state_.qualityFsr2=fsr2;
    state_.qualityExecutedUpscaler=executed;state_.qualityExecutedStatus=status;
    state_.qualityTemporalAaExecuted=nativeTaa;
    playScene_.setScriptRenderingExecution(executed,status);
  }
  // S2: Stream Mipmap Levels e Priority de cada textura publicada, na ordem que
  // o consumidor gráfico recebeu. A revisão sobe a cada publicação: o shell
  // repassa ao renderer só quando ela muda.
  std::span<const renderer::TextureStreamingParameters> textureStreamingParameters() const {
    return textureStreamingParameters_;
  }
  u64 textureStreamingRevision() const {return textureStreamingRevision_;}
  // Níveis que o renderer relata por textura publicada: pedido pela tela e carregado.
  void setTextureStreamingLevels(std::span<const u32> desired,std::span<const u32> loaded);
  struct TextureStreamingLevels {bool known=false;u32 loaded=0,desired=0,loadedWidth=0;};
  TextureStreamingLevels textureStreamingLevelsOf(const resources::AssetGuid &texture) const;
  // Prioridade no toque: 0, 1, 2, 3, -3, -2, -1 e volta; valor de script fora
  // dessa faixa volta a 0. A faixa gravada é a da Unity (-128..127).
  static i32 nextStreamingPriority(i32 value) {return value>=3?-3:value< -3?0:value+1;}
  // S4: Explorador de luzes. Conta e lista as luzes da cena (página atual) e
  // escreve a intensidade em lote nas luzes do filtro, acendendo-as, em uma
  // transação só. Devolve quantas luzes mudaram.
  void refreshLightExplorer();
  u32 applyLightExplorerIntensity();
  // S5: estatísticas do quadro relatadas pelo renderer (overlay e Graphics.State).
  void setSceneStatistics(const renderer::SceneStatistics &statistics) {
    state_.sceneStatistics=statistics;
    playScene_.setScriptSceneStatistics(statistics);
  }
  // S2: relatório do streaming de mipmaps do renderer, a cada quadro.
  void setTextureStreamingStatus(const renderer::TextureStreamingStats &stats) {
    state_.qualityTextureStreaming=stats;
    playScene_.setScriptTextureStreaming(stats);
  }
  bool temporalDebugViewAvailable(u32 view) const {
    if(view==0) return true;
    if(!state_.qualityTemporalAvailable || view>6) return false;
    if(view==2||view==3) return state_.qualityHistoryDiagnostics;
    if(view==4) return state_.qualityMotionAvailable;
    return true;
  }
  u32 nextTemporalDebugView() const {
    for(u32 step=1;step<=7;++step) {
      const u32 view=(state_.qualityTemporalDebug+step)%7u;
      if(temporalDebugViewAvailable(view)) return view;
    }
    return 0;
  }
  EditorEntityId createWaterSurface(bool cameraRelative, EditorEntityId parent=kInvalidEntity);
  // Cria a receita composta `index` de editorCreationCatalog como UM comando de
  // histórico. Recusa sem efeito quando a composição, um valor inicial ou a
  // hierarquia física não são válidos; o motivo vai para o status.
  EditorEntityId createRecipe(u32 index, EditorEntityId parent);
  // Soltar um objeto da Hierarquia ou um recurso sobre um campo do Inspector
  // (Unity: arrastar para o campo de referência). `field` é o id do widget do
  // campo sob o dedo; a validação é a mesma do seletor. Falso quando o widget
  // não é um campo compatível ou a referência foi recusada.
  bool dropObjectOnField(u32 field, EditorEntityId dropped);
  bool dropAssetOnField(u32 field, u32 assetIndex);
  void reportWaterConfiguration(bool accepted) {
    state_.status=accepted?"Agua atualizada":"Configuracao de agua recusada; estado anterior mantido";
  }
  void reportPlayFailure() {
    playScene_.stop();runtimeTextures_.clear();state_.workspace=EditorWorkspace::Scene;
    if(!playScene_.scriptDiagnostics().empty()) state_.status=playScene_.scriptDiagnostics();
    else if(!playScene_.physicsError().empty()) state_.status=playScene_.physicsError();
    else if(state_.status.rfind("Play indisponível:",0)!=0)
      state_.status="Falha ao executar a cena; revise os recursos e componentes";
  }


private:
  // O que os LOD Groups fazem na vista atual, guardado pela última extração
  // para `lodSelectionChanged` comparar sem refazer o desenho, e o relógio das
  // trocas animadas (estado de execução, nunca gravado).
  mutable std::vector<runtime::LodObjectState> lodStates_;
  mutable runtime::LodCrossFadeClock lodClock_;
  void applyLodGroups(const runtime::SceneGraph &graph,std::vector<renderer::MapDrawState> &draws) const {
    lodStates_=runtime::lodObjectStates(graph,lodView(),&lodClock_,lastWallSeconds_);
    if(lodStates_.empty()) return;
    for(auto &draw:draws) {
      const auto found=std::lower_bound(lodStates_.begin(),lodStates_.end(),static_cast<runtime::ObjectId>(draw.objectId),
          [](const runtime::LodObjectState &state,runtime::ObjectId id){return state.object<id;});
      if(found==lodStates_.end()||found->object!=draw.objectId) continue;
      if(found->hidden) {draw.visible=false;continue;}
      // Cobertura do cross-fade no MESMO campo que o LOD do pacote usa
      // (GpuMeshInstance::normalColumns[7]); o shader faz o dither complementar.
      draw.pose.instance.normalColumns[7]=found->dither;
      // Durante a troca, só o nível que sai projeta sombra: as duas sombras
      // somadas escureceriam o chão no meio do fade.
      if(found->dither<0) draw.castShadow=false;
    }
  }
  // Geometria importada neste processo, UM bloco por arquivo de origem.
  EditorComponentPresets componentPresets_;
  // O mapa que a publicação gravaria, calculado na prévia. É dele que sai a
  // identidade de cada nó na aba Estrutura, e é nele que a exclusão é refeita a
  // cada toque — sem reler o arquivo, porque a exclusão não muda o importador.
  resources::ImportNodeMap importPreviewMap_;
  resources::ImportMatchReport importPreviewMatch_;
  resources::AssetGuid importPreviewSource_{};
  bool importPreviewMapped_=false;
  void refreshImportImpact();
  bool toggleImportNodeExclusion(usize row);
  void refreshComponentPresets();
  // Valida e reconcilia os recursos de um componente vindo de preset contra
  // ESTE projeto. `only` restringe aos endereços que vão de fato ser aplicados;
  // nulo cobre o componente inteiro.
  bool resolvePresetResources(scene::ComponentValue &value,const std::vector<scene::FieldAddress> *only,std::string &error);
  //
  // Por fonte, e não uma lista achatada, porque reimportar substitui o bloco
  // daquele arquivo e preserva os outros. Numa lista achatada, reimportar
  // exigiria remendar offsets no meio — e um offset errado lê a geometria do
  // vizinho sem nenhum erro.
  struct ImportedSource {
    resources::AssetGuid guid;
    std::vector<u8> vertices;
    std::vector<u32> indices;
    std::vector<renderer::MapDrawRecord> draws;
    std::vector<renderer::MapMaterialRecord> materials;
    std::vector<resources::AssetGuid> identities;
    std::vector<std::string> names;
    // A árvore do arquivo, preservada para reinstanciar e para a reimportação
    // saber que nó é qual.
    std::vector<resources::GltfImportNode> nodes;
    std::vector<u32> drawNodes;
    // Identidade persistente dos nós e das primitivas desta revisão.
    resources::ImportNodeMap map;
    // Nomes dos materiais da fonte, alinhados a `materials`.
    std::vector<std::string> materialNames;
    // Texturas decodificadas da fonte; `materials[i].textureIndices` indexa aqui.
    std::vector<renderer::SharedAuthoringTexture> textures;
    // G6-B: skins, skin por desenho da fonte (-1 estático), influências
    // paralelas a `vertices` (vazio sem skin) e clipes por nó.
    std::vector<resources::SkinDefinition> skins;
    std::vector<i32> drawSkins;
    std::vector<u8> skinInfluences;
    std::vector<resources::AnimationClip> animations;
    // Blend shapes por primitiva e o conjunto de cada desenho da fonte (-1 sem).
    std::vector<resources::MorphTargetSet> morphs;
    std::vector<i32> drawMorphs;
    // Bloco F: imagem de origem de cada textura (paralela a `textures`).
    std::vector<std::string> textureImages;
    // S3: níveis de detalhe dos desenhos da fonte (faixas extras de `indices`).
    std::vector<renderer::MeshLodLevel> meshLods;
    // Faixa que veio da fonte. Recursos derivados compartilham os vértices e
    // entram depois dela; regenerar começa sempre daqui.
    usize sourceIndexCount = 0;
    usize sourceDrawCount = 0;
  };
  struct ImportedLibrary {
    std::vector<u8> vertices;
    std::vector<u32> indices;
    std::vector<renderer::MapDrawRecord> draws;
    std::vector<renderer::MapMaterialRecord> materials;
    std::vector<resources::AssetGuid> identities;
    std::vector<std::string> names;
    // Texturas de todas as fontes, com os índices dos materiais já deslocados.
    std::vector<renderer::SharedAuthoringTexture> textures;
    // Fonte de cada textura acima: o perfil dela decide o streaming (S2).
    std::vector<resources::AssetGuid> textureSources;
    // Pivô por desenho, em espaço do mesh. Geometria importada gira em torno da
    // origem do NÓ; ver `EditorMapScene::adoptPackage`.
    std::vector<float> pivots;
    // G6-B: influências paralelas a `vertices` (vazio quando nenhuma fonte tem
    // skin), skin e juntas por desenho, e os clipes de cada fonte.
    std::vector<u8> skinInfluences;
    std::vector<u32> drawJoints;
    // Deltas de blend shapes de toda a biblioteca e, por desenho, o início (em
    // floats) e o número de alvos; `~0u` e zero num desenho sem eles.
    std::vector<float> morphDeltas;
    std::vector<u32> drawMorphOffsets,drawMorphTargets;
    // S3: níveis de detalhe com desenho e faixa já na numeração da biblioteca.
    std::vector<renderer::MeshLodLevel> meshLods;
    std::vector<std::shared_ptr<const EditorMapScene::DrawDeformation>> deformations;
    std::vector<std::shared_ptr<const runtime::SourceAnimations>> animations;
  };
  // Achata os blocos na ordem em que estão, remapeando offsets. A ordem é
  // estável: reimportar não reordena as fontes, então os slots das outras não
  // mudam por acidente.
  static ImportedLibrary flattenSources(const std::vector<ImportedSource> &sources);
  // Sobe a biblioteca ao consumidor gráfico e adota o pacote que voltou.
  bool publishAndAdopt(const ImportedLibrary &library, std::string &diagnostic,
                       usize *outPrimitives = nullptr);
  // Uma fonte validada e posta no candidato, ainda sem publicar. A reconciliação
  // precisa do pacote adotado (slots), então acontece depois, em outra fase.
  struct StagedSource {
    resources::AssetGuid source{};
    bool reimported = false;
    bool hasPrevious = false;
    resources::ImportNodeMap previousNodeMap;
  };
  bool stageSource(const resources::GltfImport &model, std::string_view hash, std::string_view sourceName,
                   resources::ImportAmbiguityPolicy policy, std::vector<ImportedSource> &candidateSources,
                   resources::AssetRegistry &nextAssets, ModelImportReport &report, StagedSource &staged,
                   std::span<const resources::AssetGuid> excludedNodes);
  bool applyCollisionMeshRecipes(ImportedSource &source, const resources::ImportProfile &profile,
                                 std::string &diagnostic) const;
  bool generateCollisionMesh(EditorEntityId entity, u64 componentInstance, u8 trianglePercent,
                              float maximumError, std::string &diagnostic);
  bool applyCollisionMeshEdit(const resources::AssetGuid &source,const resources::ImportProfile &profile,
                              EditorEntityId entity,const EditorEntity &values,std::string &diagnostic);
  void refreshCollisionMeshDraft();
  void reconcileStagedSource(const StagedSource &staged, ModelImportReport &report);
  // Liga a deformação (ossos, blend shapes) e os clipes de uma instância de
  // `tree`: `objectOfNode` é o objeto de cada nó da fonte (zero sem objeto) e
  // `roots` os objetos que recebem a Animação. Componente novo é criado;
  // existente é religado só onde a fonte mudou.
  bool bindImportedDeformation(const ImportedSource &tree,const std::vector<EditorEntityId> &objectOfNode,
                               std::span<const EditorEntityId> roots,u32 &meshes,u32 &animations);
  std::vector<ImportedSource> importedSources_;
  resources::GltfImportLimits importLimits_{};
  u64 importTextureBudget_ = u64{1} << 30;
  resources::TextureBudgetReport textureResidency_{};
  std::string reimportPath_;
  std::string environmentReimportPath_;
  std::string textureReimportPath_;
  bool previousImportMap(const resources::AssetGuid &source, resources::ImportNodeMap &out) const;
  bool persistImportMap(const resources::AssetGuid &source);
  void removeImportMapFile(const resources::AssetGuid &source);
  void reportImportReconcile(const ImportReconcileReport &report, const char *context);
  void refreshImportLinkView();
  u64 importInstanceCounter_=0;
  std::vector<resources::MaterialAsset> materials_;
  std::vector<resources::EnvironmentProfile> environmentProfiles_;
  std::vector<EnvironmentMapEntry> environmentMaps_;
  std::vector<ProjectTexture> textures_;
  struct DecodedProjectTexture {
    resources::AssetGuid guid;
    bool srgb=true;
    bool normal=false;
    u32 sampler=EditorMapScene::DefaultTextureSampler;
    std::string contentHash;
    renderer::SharedAuthoringTexture texture;
  };
  std::vector<DecodedProjectTexture> decodedTextures_;
  // Textura do projeto como sobe para a GPU: identidade, espaço de cor e sampler.
  struct UsedTexture {
    resources::AssetGuid guid;
    bool srgb=true;
    bool normal=false;
    u32 sampler=EditorMapScene::DefaultTextureSampler;
    friend bool operator==(const UsedTexture &a,const UsedTexture &b) {
      return a.guid==b.guid && a.srgb==b.srgb && a.normal==b.normal && a.sampler==b.sampler;
    }
  };
  std::vector<UsedTexture> anticipatedTextures_;
  // Variantes pedidas por scripts durante a sessão de Play. Permanecem na
  // biblioteca até Stop para uma republicação posterior não desfazer o binding.
  std::vector<UsedTexture> runtimeTextures_;
  std::vector<std::pair<resources::AssetGuid,resources::TextureProfile>> textureProfiles_;
  std::vector<std::pair<EditorMapScene::TextureBinding,TextureResidency>> publishedTextures_;
  std::vector<renderer::TextureStreamingParameters> textureStreamingParameters_;
  u64 textureStreamingRevision_=0;
  std::vector<u32> textureStreamingDesired_,textureStreamingLoaded_;
  // Seleção da cena quando a textura/gerenciador abriu: mudar a seleção devolve
  // Propriedades ao objeto.
  EditorEntityId texturePanelSelection_=kInvalidEntity;
  bool textureInspectorFromManager_=false;
  void refreshTexturePanels();
  TexturePreviewAtlas preview_;
  struct TextureThumbnail {
    resources::AssetGuid guid;
    std::string contentHash;
    ui::UiRect content{};
    bool opaque=false; // nenhum texel com alfa abaixo de 255
  };
  std::vector<TextureThumbnail> thumbnails_; // na ordem das texturas do projeto
  // Bloco F: texturas das fontes importadas, na ordem da biblioteca publicada
  // (a mesma do renderer), com a imagem de origem e a fonte dona.
  struct SourceTexture {
    u32 library=0;
    resources::AssetGuid source;
    std::string image,name,sourcePath;
    renderer::SharedAuthoringTexture texture;
    std::vector<resources::AssetGuid> meshes; // malhas da fonte cujo material a usa
  };
  std::vector<SourceTexture> sourceTextures_;
  // Célula do atlas de miniaturas ocupada por uma textura de fonte (a mesma
  // grade das do projeto: quem escreve numa célula invalida o outro dono).
  std::array<const renderer::AuthoringTexture *,TextureThumbnailCapacity> sourceThumbCells_{};
  std::array<ui::UiRect,TextureThumbnailCapacity> sourceThumbContent_{};
  // Prévia RGBA8 da textura de fonte aberta no Inspector.
  struct {const renderer::AuthoringTexture *texture=nullptr;renderer::AuthoringTexture rgba;u32 firstLevel=0;} sourceViewer_;
  void collectSourceTextures();
  // Expande em Arquivos as pastas até `relativePath`, seleciona e rola até ele.
  bool revealInFiles(const std::string &relativePath);
  bool generatePendingSourceThumbnail();
  bool refreshSourceTextureViewer();
public:
  // Abre no Inspector a textura de fonte `row` (índice na lista das fontes).
  bool openSourceTextureInspector(u32 row);
  // Textura de fonte cuja imagem é o arquivo `relativePath` do projeto; -1 sem.
  i32 sourceTextureForFile(std::string_view relativePath);
private:
  struct ViewerChain {
    resources::AssetGuid guid;
    std::string contentHash, recipe;
    renderer::SharedAuthoringTexture texture;
  } viewerChain_;
  bool refreshTextureViewerImage();
  bool appearanceChanged_=false;
  void publishMaterialLibrary();
  void loadMaterialAssets();
  void loadEnvironmentProfiles();
  void loadTextureAssets();
  // Textura do projeto decodificada com mips para um espaço de cor, em cache
  // enquanto o conteúdo registrado não muda. Nula quando o arquivo não abre.
  renderer::SharedAuthoringTexture decodeProjectTexture(const resources::AssetGuid &guid,bool srgb,
                                                        u32 sampler=EditorMapScene::DefaultTextureSampler,
                                                        bool normal=false);
  // Pares (textura, sRGB) usados por slots da cena e por materiais do projeto.
  void collectUsedTextures(std::vector<UsedTexture> &out) const;
  // Publica de novo só se alguma textura usada ainda não está na biblioteca.
  bool ensureTexturesPublished(std::string &diagnostic);
  void refreshMaterialSlotView();
  bool writeMaterialAsset(const resources::MaterialAsset &material,const std::string &path,std::string &diagnostic);
  bool commitSharedMaterial(const resources::MaterialAsset &candidate,std::string &diagnostic,bool recordHistory=true);
  bool writeEnvironmentProfile(const resources::EnvironmentProfile &profile,const std::string &path,std::string &diagnostic);
  bool commitEnvironmentProfile(const resources::EnvironmentProfile &candidate,std::string &diagnostic,bool recordHistory=true);
  void synchronizeEnvironmentProfile(const resources::EnvironmentProfile &profile);
  double codeCheckpointAt_=0;
  // A impressão digital do pacote base, guardada na importação inicial: é ela
  // que deriva a identidade das primitivas internas em toda adoção posterior.
  u64 packageFingerprint_ = 0;
  resources::AssetRegistry assets_;
  GeometryPublisher publishGeometry_;
  SkinningPublication skinningPublication_{};
  static u64 nextSceneEpoch() noexcept;
  struct ViewportPointer final {
    u32 id = 0;
    ui::UiPoint position{};
    bool moved = false;
  };

  void buildPickCandidates();
  void frameSubtree(EditorEntityId root);
  bool handleViewportPointer(const ui::UiPointerEvent &event, const ui::UiPointerRouting &routing);
  bool handlePointerNow(const ui::UiPointerEvent &event);
  bool completeTextEditNow(const EditorTextEdit &edit, std::string_view text, bool accept);
  bool updateTextDraftNow(const EditorTextEdit &edit, std::string_view text, u32 caret);
  runtime::ComponentResourceResolver runtimeResourceResolver();
  // Inspector em Play (editor/editor_play_edit.h). Os controles são os mesmos
  // da edição autoral; durante a chamada eles enxergam o ESPELHO do mundo no
  // lugar do documento, com histórico e época próprios, e o que mudou nele vai
  // ao mundo pela API pública. O documento autoral não é tocado, então parar o
  // Play é o próprio "reverter".
  template<class F> auto inPlayMirror(F &&body) {
    enterPlayMirror();
    if constexpr(std::is_void_v<std::invoke_result_t<F>>) {body();leavePlayMirror();}
    else {auto result=body();leavePlayMirror();return result;}
  }
  void enterPlayMirror();
  void leavePlayMirror();
  bool routeToPlayMirror(const ui::UiPointerEvent &event);
  void refreshPlayMirror();
  void endPlayInspect();
  void handleGizmoPointer(const ui::UiPointerRouting &routing, u32 axis);
  ViewportPointer *findViewportPointer(u32 id) noexcept;

  const ui::UiFont *font_ = nullptr;
  const ui::UiIconAtlas *icons_ = nullptr;
  EditorDocument document_;
  EditorFileSystem files_;
  EditorCodeWorkspace code_;
  std::string codeBuildRequest_;
  u64 codeBuildGeneration_=0;
  u64 runtimeCodeGeneration_=0;
  std::string requestedScenePath_;
  u64 sceneEpoch_=nextSceneEpoch();
  EditorMapScene mapScene_;
  EditorPlayScene playScene_;
  double playLastSeconds_=0;
  platform::FirstPersonTouchControls playTouches_;
  EditorHistory history_;
  EditorDocument playMirror_;
  runtime::SceneGraph playMirrorBase_;
  EditorHistory playHistory_;
  u64 playMirrorEpoch_=0;
  u64 playMirrorRevision_=0;
  EditorWorkspace playMirrorWorkspace_=EditorWorkspace::Play;
  // Válido: há um espelho tirado nesta sessão de Play. Ocupado: um gesto,
  // transação ou texto dele segue aberto e o espelho não pode ser trocado.
  bool playMirrorValid_=false,playMirrorBusy_=false,playMirrorOpen_=false;
  std::vector<u32> playEditPointers_;
  renderer::ProjectRenderingSettings renderingSettings_{};
  renderer::ProjectRenderingSettings requestedRenderingSettings_{};
  renderer::ProjectRenderingSettings scriptRenderingRequest_{};
  u64 scriptRenderingRequestId_=0;
  bool scriptRenderingRequestPending_=false;
  bool renderingSettingsRequested_ = false;
  bool renderingSettingsRequestInFlight_ = false;
  u64 qualityRevision_ = 0;
  u64 renderingSettingsRequestRevision_ = 0;
  EditorCamera camera_;
  renderer::PerspectiveVisibilitySettings projection_{};
  EditorViewport view_{};
  EditorScreenState state_{};
  EditorConsole console_;
  std::string consoleCopy_;
  float codeAutoBuildDelay_ = 1.25f;
  u64 codeSeenGeneration_ = 0, codeBuiltGeneration_ = 0;
  double codeQuietSince_ = -1.0;
  EditorScreenLayout layout_{};
  ui::UiDrawList list_;
  ui::UiInputRouter router_;
  std::vector<ui::UiInstance> instances_;
  std::vector<EditorPickCandidate> candidates_;
  std::vector<ViewportPointer> viewportPointers_;
  bool cameraGestureOpen_=false;
  EditorCameraPreview cameraPreview_{};
  bool lensDragOpen_=false;
  u32 lensDragPointer_=0,lensDragKind_=0;
  float lensDragStart_=0;
  EditorEntity lensDragInitial_{};
  EditorCameraHandle lensDragHandle_{};
  EditorViewport lensDragView_{};
  bool componentDragOpen_=false;
  u32 componentDragPointer_=0;
  float componentDragStart_=0;
  EditorEntity componentDragInitial_{};
  EditorComponentHandle componentDragHandle_{};
  EditorViewport componentDragView_{};
  EditorEntityId cameraGestureEntity_=0;
  u64 cameraGestureInstance_=0;
  void pilotCamera(ui::UiPoint delta,float dolly,bool translate);
  void finishCameraGesture(bool cancel);
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
  u32 reorderPointer_ = 0, reorderPagerHover_ = 0;
  u32 numericFieldSeen_ = 0;
  u32 scriptArrayPointer_ = 0, scriptArrayPagerHover_ = 0;
  // Janela de cor: amostras do projeto, abertura com a cor do campo e a cor
  // escolhida em RGB linear (com alfa e intensidade quando o campo tem).
  EditorValueLibraries colorLibraries_;
  void openColorWindow(u32 key,const float (&linearRgba)[4],bool alpha,bool hdr);
  void pickerFromLinear(const float (&linearRgba)[4]);
  void pickerLinear(float (&rgba)[4]) const;
  bool handleColorWindow(const ui::UiPointerEvent &event,const ui::UiPointerRouting &routing);
  bool commitColorWindow();
  void saveColorLibraries();
  // Editor de gradiente: rascunho em estrutura (o texto do estado é derivado),
  // presets do projeto e o arraste de uma parada.
  scene::ScriptGradient gradientEdit_;
  EditorValueLibraries gradientLibraries_;
  u32 gradientPointer_=0;
  float gradientPressY_=0;
  void openGradientEditor(u32 key,std::string_view value,std::string_view type);
  void publishGradientDraft();
  bool handleGradientEditor(const ui::UiPointerEvent &event,const ui::UiPointerRouting &routing);
  bool commitGradientEditor();
  void saveGradientLibraries();
  // Editor de curvas: rascunho em estrutura, presets, e o gesto em curso no
  // gráfico (chave, alça de entrada/saída ou deslocar a vista).
  scene::ScriptCurve curveEdit_;
  EditorValueLibraries curveLibraries_;
  enum class CurveDrag : u8 { None, Key, HandleIn, HandleOut, Pan };
  CurveDrag curveDrag_=CurveDrag::None;
  u32 curvePointer_=0;
  ui::UiPoint curvePress_{};
  float curvePressView_[4]{};
  float curvePressKey_[2]{};
  double curveLastTap_=-1;
  ui::UiPoint curveLastTapPoint_{};
  void openCurveEditor(u32 key,std::string_view value,std::string_view type);
  void publishCurveDraft();
  u32 settleCurveSelection(u32 selected);
  bool handleCurveEditor(const ui::UiPointerEvent &event,const ui::UiPointerRouting &routing);
  bool commitCurveEditor();
  void saveCurveLibraries();
  // Lista de campo de script: valor autoral atual (vazio quando é o padrão do
  // código) e gravação da lista inteira como um passo de Desfazer.
  std::vector<std::string> scriptArrayItems(const scene::ScriptBehavior &script,std::string_view id) const;
  bool setScriptArray(EditorEntityId entity,u64 instance,std::string_view id,std::string_view declaredType,
                      const std::vector<std::string> &items);
  const EditorScriptProperty *scriptProperty(std::string_view scriptType,std::string_view id) const;
  bool handleScriptArrayDrag(const ui::UiPointerEvent &event,const ui::UiPointerRouting &routing);
  // Barra do LOD Group: arraste de divisor numa transação (um passo de
  // Desfazer), escolha de segmento e inserir/apagar nível.
  bool handleLodBar(const ui::UiPointerEvent &event,const ui::UiPointerRouting &routing);
  u32 lodDivider_=0,lodPointer_=0;
  bool handleComponentReorder(const ui::UiPointerEvent &event, const ui::UiPointerRouting &routing);
  EditorEntity fieldInitial_{};
  bool playRequested_ = false;
  void preparePlay();
  // Pulso de um quadro: o botão de salto da interface vira um BOTÃO DE
  // DISPOSITIVO, e o mapa de ações decide o que ele aciona.
  bool jumpPressed_ = false;
  bool secondaryPressed_ = false;
  float sceneTime_ = 0.0f;
  float lastWallSeconds_ = 0.0f;
  bool clockPrimed_ = false;
};

} // namespace ae::editor
