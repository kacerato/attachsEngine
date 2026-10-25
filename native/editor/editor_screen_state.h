// Estado autoral/transitorio da tela. Nao inclui os IDs nem a API de desenho.
#pragma once
namespace ae::ui { class UiDrawList; }
#include <functional>

#include "resources/physics_material.h"
#include "resources/material_asset.h"
#include "resources/library_index.h"
#include "resources/animator_controller.h"
#include "navigation/navigation_mesh.h"
#include "editor/editor_probes.h"
#include "editor/editor_lightmaps.h"
#include "resources/animation_clip_asset.h"
#include "resources/animation_clip_selection.h"
#include "resources/animation_clip_clipboard.h"
#include "resources/animation_clip_timeline.h"
#include "editor/editor_animation_pose_tools.h"
#include "editor/editor_animation_pose_selection.h"
#include "resources/animation_clip_bake.h"
#include "renderer/rendering_policy.h"
#include "renderer/texture_streaming.h"
#include "renderer/scene_statistics.h"
#include "resources/environment_map_asset.h"
#include "resources/texture_profile.h"
#include "core/base.h"
#include "runtime/script_inspection.h"
#include "runtime/game_view.h"
#include "editor/editor_document.h"
#include "editor/editor_gui_tree.h"
#include "editor/editor_prefab_overrides.h"
#include "editor/editor_filesystem.h"
#include "editor/editor_code_workspace.h"
#include "editor/editor_console.h"
#include "editor/editor_gizmo.h"
#include "editor/editor_history.h"
#include "editor/editor_view.h"
#include "scene/component_preset.h"
#include "ui/ui_icon_id.h"
#include <algorithm>
#include <array>
#include "ui/ui_theme.h"
#include "editor/editor_rig_screen.h"
#include <string>

namespace ae::runtime {class SceneTimers;class SceneTweens;class SceneTweenSequences;class ScenePhysicsQueries;class SceneVirtualCameras;class SceneAudio;class SceneAnimatorGraphs;class ScenePhysics;class GameWorld;class SceneNavigation;class SceneParticles;}
namespace ae::editor {
class EditorMapScene;
struct ColliderTopology;

// Uma linha do diff de um preset, já com a escolha do autor.
//
// O endereço vem de `scene::FieldAddress` e aponta para a identidade
// persistente da propriedade — nunca para o índice da linha na tela. Uma lista
// reordenada, paginada ou filtrada continua aplicando o mesmo campo.
struct EditorPresetField {
  scene::FieldAddress address;
  std::string label,current,candidate;
  bool differs=false,applicable=false,selected=false;
};

// O que o viewport está mostrando. É a aba do topo, e trocá-la troca o contexto
// inteiro — não é um botão que dispara algo.
// Seis contextos fixos. Configuração de projeto (camadas, entrada, água) é UMA
// workspace com seções; um tipo de componente ou uma lista de ajustes nunca
// vira workspace própria — ver docs/planos/EXPANSAO-OBJETOS-COMPONENTES-API.
enum class EditorWorkspace : u8 { Scene, Assets, Lighting, Play, Project, Code, Gui, Library };
enum class EditorProjectSection : u8 { Layers, Input, Water, Tags };
enum class EditorNavigationMode : u8 { Orbit, Pan, Zoom };
enum class EditorInspectorTab : u8 { Transform, Material, Properties };
enum class EditorInspectorSurface : u8 { Inspection, Components };

enum class EditorSearchProvider : u8 { All = 0, Scene, Project, Create };

struct EditorSearchResult {
  EditorSearchProvider provider = EditorSearchProvider::Scene;
  // Objeto: id da entidade; receita: índice no catálogo; arquivo: índice no
  // índice de arquivos passado à busca.
  u64 key = 0;
  std::string title, detail;
  ui::UiIcon icon = ui::UiIcon::None;
  u8 rank = 0;  // 0 começa com a palavra, 1 contém numa fronteira, 2 contém
};

struct EditorSearchCounts { u32 scene = 0, project = 0, create = 0; };

// Arranjo dos painéis do editor (Unity: Layout). Larguras em dp; zero pede a
// proporção padrão. Não é dado da cena: fica nas preferências do projeto.
struct EditorLayout {
  std::string name;
  float hierarchyWidth = 0, inspectorWidth = 0;
  bool hierarchyVisible = true, inspectorVisible = true, filesCollapsed = false, diagnosticDock = false;
  bool operator==(const EditorLayout &other) const {
    return hierarchyWidth == other.hierarchyWidth && inspectorWidth == other.inspectorWidth &&
           hierarchyVisible == other.hierarchyVisible && inspectorVisible == other.inspectorVisible &&
           filesCollapsed == other.filesCollapsed && diagnosticDock == other.diagnosticDock;
  }
};

// Os cinco arranjos prontos, na largura `surface` (as larguras em dp dos
// painéis são frações dela).
inline std::array<EditorLayout, 5> editorBuiltinLayouts(float surface) {
  std::array<EditorLayout, 5> layouts{};
  layouts[0].name = "Padrão";
  layouts[1].name = "Cena ampla";
  layouts[1].hierarchyVisible = false;
  layouts[2].name = "Autoria";
  layouts[2].inspectorWidth = std::max(260.0f, surface * 0.4f);
  layouts[2].filesCollapsed = true;
  layouts[3].name = "Diagnóstico";
  layouts[3].diagnosticDock = true;
  layouts[4].name = "Só a cena";
  layouts[4].hierarchyVisible = layouts[4].inspectorVisible = false;
  return layouts;
}

// Trabalho longo fora da thread do editor (importação, abertura do projeto,
// compilação...). `progress` em 0..1, ou negativo quando não há medida real —
// a barra mostra só a atividade, sem porcentagem inventada.
struct EditorBackgroundTask {
  enum class Kind : u8 { Other, ModelImport, EnvironmentImport, TextureImport, ProjectOpen, FolderCopy, CodeBuild, CodeAnalysis };
  Kind kind = Kind::Other;
  std::string label, detail;
  float progress = -1;
  bool cancelable = false;
};

// Superfície de navegação selecionada: bake, recurso e malha para o viewport.
struct EditorNavigationView {
  EditorEntityId surface=0;
  u64 instance=0;
  bool baking=false,hasData=false,missing=false,stale=false,staleKnown=false;
  float progress=0,bakeSeconds=0;
  navigation::NavMeshStats stats{};
  std::string status;
  bool showMesh=true;
  std::vector<float> triangles,edges;   // mundo; 9 floats por triângulo, 6 por aresta
  std::vector<u8> areas;
};
struct EditorScreenState final {
  struct LibraryRow {
    resources::AssetGuid id{};std::string name,context,diagnostic;
    resources::LibraryResolutionState state=resources::LibraryResolutionState::Pending;
    resources::LibraryItemKind kind=resources::LibraryItemKind::Other;
    // Malha do projeto (índice no pacote carregado): o cartão vai "Na cena"
    // em vez de abrir um item de pacote. ~0u para itens de pacote.
    u32 projectAsset=~0u;
    // Modelo que a cena referencia sem registro em .astra/assets.astra.
    bool unregistered=false;
    // G3a: classe do material Unity na auditoria (LibraryAudit*).
    u8 audit=0;
  };
  // G3a: auditoria de materiais Unity do pacote/busca atuais.
  static constexpr u8 LibraryAuditNone=0,LibraryAuditComplete=1,LibraryAuditApproximated=2,LibraryAuditUnsupported=3;
  u32 libraryMaterialAudit[3]{};
  // Classe filtrada na grade (LibraryAudit*; None mostra todas).
  u8 libraryAuditFilter=0;
  struct LibraryPackageRow {resources::AssetGuid id{};std::string name;};
  std::vector<LibraryRow> libraryRows;
  std::vector<LibraryPackageRow> libraryPackages;
  resources::AssetGuid libraryPackage{};
  resources::AssetGuid libraryDetailItem{};
  bool libraryConflict=false,libraryCanInstantiate=false,libraryCanAssignController=false,libraryCanReimport=false;
  std::string libraryQuery;
  std::string libraryDiagnostic;
  std::string libraryDetailName;
  u32 libraryKind=0;
  // Itens por tipo (0 = todos) no pacote/busca atuais, para o trilho de tipos.
  // Trilho: Todos, dez tipos de pacote e Projeto (malhas do projeto).
  u32 libraryKindCounts[12]{};
  // Lista e diagnóstico da Biblioteca rolam pelo dedo (coluna, sem páginas).
  float libraryScroll=0,libraryDiagnosticScroll=0;
  bool libraryImportRequested=false,searchingLibrary=false;
  // Superfície inteira em pixels lógicos (dp), incluindo o que fica sob o
  // recorte da câmera. As áreas seguras entram por `safeArea`.
  ui::UiRect surface{};
  ui::UiInsets safeArea{};
  // O que a Hierarquia e o Inspector mostram: o documento autoral, ou o grafo
  // do mundo de execução quando o Play é inspecionado. Os dois são
  // runtime::SceneGraph; a escrita autoral continua indo ao EditorDocument.
  const runtime::SceneGraph *document = nullptr;
  const EditorFileSystem *files=nullptr;
  const EditorCodeWorkspace *code=nullptr;
  std::vector<runtime::ScriptFieldIssue> scriptFieldIssues;
  EditorEntityId scriptInspectionEntity=0;
  std::string scriptInspectionError;
  const EditorMapScene *resources=nullptr;
  const resources::AssetRegistry *assetRegistry=nullptr;
  bool codeCompilerAvailable=false,codeBuildBusy=false;
  bool editingCode=false,creatingScript=false,searchingCode=false;
  u64 codeSearchRequest=0;
  // O console: compilador, scripts e editor no mesmo lugar.
  const EditorConsole *console = nullptr;
  // Quantas linhas o usuário subiu a partir do fim. Zero é o fim, que é onde um
  // console deve estar: a linha que acabou de aparecer é a que interessa.
  u32 consoleScroll = 0;
  bool consoleCollapsed = true;
  bool consoleProblems=false,consoleExpanded=false,consoleFollow=true,searchingConsole=false;
  float consoleFraction=.58f;
  u64 consoleSelected=0,consoleAnchor=0;
  u32 consoleDetailPage=0;
  float consoleDragRemainder=0;
  int consoleOrigin=-1;
  std::string consoleQuery;
  // O menu da barra do IDE. Uma lista que abre num ícone é o que tira da barra
  // tudo o que não é frequente, sem escondê-lo atrás de um gesto.
  bool codeMenu = false;
  bool codeTools=false,codeToolsBusy=false;
  u32 codeToolsPage=0;
  std::vector<EditorToolInfo> codeToolCatalog;
  bool codeFiles = false,platformCodeView=false,codeComposing=false;
  bool goingToLine=false,creatingCodeFolder=false;
  u32 codeFirstTab=0;
  // O arquivo escolhido no painel. As ações de recurso agem sobre ele, e é o
  // painel que decide qual é — não a seleção da cena, que é outra coisa.
  std::string selectedFile;
  // Vários arquivos escolhidos: `selectedFile` é o ativo (o último tocado).
  // Com o modo aceso, cada toque numa linha soma ou tira.
  std::vector<std::string> selectedFiles;
  bool filesMultiSelect=false;
  bool isFileSelected(std::string_view path) const {return std::find(selectedFiles.begin(),selectedFiles.end(),path)!=selectedFiles.end();}
  // O Inspector de vários recursos (Unity 6000.0 Manual/InspectorManageComponents,
  // "Inspect multiple assets"): tipos presentes para estreitar, a lista e, quando
  // todos são texturas do projeto, o perfil de importação comum com "—" no que difere.
  struct MultiAssetView {
    enum class Kind : u8 {Mixed, Textures, Materials, EnvironmentMaps, Profiles, Other};
    struct Group {std::string label;u32 count=0,icon=0;};
    struct Item {std::string name,path,detail;u32 icon=0;bool active=false;};
    Kind kind=Kind::Mixed;
    std::vector<Group> groups;
    std::vector<Item> items;
    std::string title,note;
    std::array<std::string,10> fields;  // "Rótulo: valor", "Rótulo: —" quando difere
    u16 mixed=0;
    u32 pending=0;                      // texturas cujo rascunho difere do aplicado
  };
  MultiAssetView multiAsset;
  float multiAssetScroll=0;
  // Vários materiais: o Inspector é o do ativo, a edição vale para todos e
  // estes campos (chaves de buildMaterialSlots) diferem entre eles: "—".
  std::vector<std::string> materialMixed;
  bool materialMixedHas(std::string_view key) const {return std::find(materialMixed.begin(),materialMixed.end(),key)!=materialMixed.end();}
  bool renamingResource=false;
  // Apagar pede confirmação em DOIS toques, e não num diálogo. Guarda o caminho
  // que o primeiro toque recusou; o segundo, no mesmo caminho, confirma.
  std::string pendingResourceDelete;
  // Escolha do modelo antes de pedir o nome da classe. `~0u` é o arquivo
  // vazio, que continua sendo o primeiro item da lista.
  bool choosingTemplate=false;
  u32 scriptTemplate=~0u;
  std::string codeQuery;
  u32 fileScroll=0;
  float fileScrollOffset=0;
  float filePanelRatio=.46f;
  bool filesCollapsed=false;
  EditorEntityId selection = kInvalidEntity;
  std::span<const EditorGuiRow> guiRows{};
  EditorGuiTarget guiSelection{};
  std::vector<EditorGuiTarget> collapsedGui;
  bool guiInspector=false;
  // Widget sob o dedo agora, para o realce de pressionado.
  u32 pressedWidget = 0;
  EditorGizmoMode tool = EditorGizmoMode::Translate;
  EditorWorkspace workspace = EditorWorkspace::Scene;
  bool standalonePlayer=false;
  EditorProjectSection projectSection=EditorProjectSection::Layers;
  u32 physicsLayer=0,physicsMatrixPage=0;
  bool editingPhysicsLayerName=false;
  bool tagPicker=false,editingTagName=false,editingTagSearch=false;
  bool groupPicker=false,editingGroupName=false;
  EditorEntityId groupEntity=0;
  u32 groupPage=0,groupEditSlot=0;
  u32 tagPage=0;
  std::string tagQuery,tagSelected="Untagged";
  u32 inputActionIndex=0,inputBindingIndex=0,inputTab=0,inputEditField=0;
  u32 inputActionPage=0,inputBindingPage=0;
  bool inputDetails=false;
  bool editingInputActionName=false,editingInputContext=false;
  bool inputCapturing=false;
  bool inputCaptureNegative=false;
  std::string inputCapturePrompt,inputCaptureFeedback;
  bool playHasScripts=false;
  bool playFirstPerson=false,playHasCharacter=false;
  // Joystick flutuante do Play (relativo ao viewport): base, botão e raio.
  bool playStickActive=false;float playStickOrigin[2]{},playStickKnob[2]{},playStickRadius=0;
  std::string playSecondaryActionLabel,playHudMessage;
  bool playPaused=false;
  bool playStepRequested=false;
  float playTimeScale=1;
  const runtime::ScenePhysics *characterRuntime=nullptr;
  const runtime::GameWorld *characterWorld=nullptr;
  const runtime::SceneTweens *tweenRuntime=nullptr;
  // Partículas da vista (prévia na edição, mundo no Play): estado e teto por sistema.
  const runtime::SceneParticles *particleRuntime=nullptr;
  float particleSimulationMs=0;
  const runtime::SceneTweenSequences *tweenSequenceRuntime=nullptr;
  const runtime::ScenePhysicsQueries *physicsQueryRuntime=nullptr;
  const EditorNavigationView *navigationView=nullptr;
  // PK-6: a sonda (de reflexo ou grupo de luz) selecionada no Inspector.
  const EditorProbeView *probeView=nullptr;
  // G3c: janela Iluminação › Lightmaps (Generate Lighting) e a aba aberta
  // (0 Ambiente, 1 Lightmaps).
  const EditorLightmapView *lightmapView=nullptr;
  u8 lightingTab=0;
  const runtime::SceneNavigation *navigationRuntime=nullptr;
  const runtime::SceneVirtualCameras *virtualCameraRuntime=nullptr;
  const runtime::SceneAudio *audioRuntime=nullptr;
  const runtime::SceneAnimatorGraphs *animatorRuntime=nullptr;
  // Editor de grafo do Animator (tela cheia sobre o editor).
  bool animatorOpen=false,animatorConnecting=false,editingAnimatorName=false;
  // One clip surface: shared stable selection for keys/curves and isolated pose.
  // None of this UI state is stored as runtime animation data.
  bool clipOpen=false,clipCurves=false,clipPlaying=false,clipLoop=true,clipExpanded=false,clipPicker=false,clipNewPicker=false,clipTangentPicker=false;
  bool clipCurveView=false,clipCurveNormalized=false,clipCurveIsolated=false;
  bool clipRegionsShown=false,clipRegionDetails=false;
  bool clipPropertiesShown=false,clipPropertyCatalog=false,clipPropertyRemap=false,clipPropertyCurves=false;
  std::string clipPropertyQuery;
  u32 clipPropertyComponent=0,clipPropertyCurveMode=0;
  bool clipDriversShown=false,clipDriverFields=false;
  u32 clipDriverRoute=0,clipDriverPage=0,clipDriverCurveKey=0,clipDriverCurveMode=0;
  u64 clipDriverNode=0;
  const resources::AnimationClipAsset *clipDriverDraft=nullptr;
  struct ClipDriverEntry {EditorEntityId target=0;resources::AnimationDriverInput input;std::string label;};
  std::vector<ClipDriverEntry> clipDriverEntries;
  std::string clipDriverInspection;
  std::function<void(ui::UiDrawList &,const ui::UiRect &)> clipGuiPreviewDraw;
  u64 clipPropertyTrack=0,clipPropertyKey=0;
  u32 clipPropertyPage=0,clipPropertyDimensions=0;
  float clipPropertyValues[4]{};
  bool clipPropertyBoolean=false;
  u32 clipPropertyEnum=0;
  struct ClipPropertyEntry {
    resources::AnimationPropertyAddress address;std::string label;
    std::string unit{},help{};float minimum=0,maximum=0;
  };
  std::vector<ClipPropertyEntry> clipPropertyEntries;
  std::vector<std::pair<u32,std::string>> clipPropertyOptions;
  struct ClipPropertyKey {u64 id=0;float time=0;};
  std::vector<ClipPropertyKey> clipPropertyKeys;
  std::string clipPropertyLabel,clipPropertyError,clipPropertyUnit,clipPropertyInfo;
  u64 clipRegion=0,clipPreviewRegion=0;
  u32 clipRegionPage=0;
  u32 clipCurveViewTask=0,clipCurveReferencePage=0;
  struct ClipCurveRange {u64 track=0;u32 component=0;double minimum=0,maximum=1;};
  std::vector<ClipCurveRange> clipCurvePins,clipCurveRanges;
  u32 clipModeSide=0;
  u32 clipAuthoringPicker=0,clipTargetPage=0,clipAuthorReturn=0;
  EditorEntityId clipTargetBranch=0,clipTargetNode=0,clipTarget=0;
  // Estúdio: coluna de inspector (0 chave, 1 rig, 2 pose) e rolagem da árvore de ossos.
  // Estado do estudio (rig, IK, pacotes novos): lido pela tela via ponteiro,
  // para que campos novos nao recompilem todo o editor.
  const struct EditorStudioState *studio=nullptr;
  u8 clipStudioTab=0;u32 clipBoneScroll=0;
  // IK de dois ossos: arrastar o alvo da ponta no viewport gira raiz e meio.
  bool clipIkMode=false,clipIkDragging=false,clipIkPoleSet=false;ui::UiPoint clipIkHandle{-1,-1};
  // Biblioteca de poses do rig carregado com o clipe e pares de espelho.
  std::vector<std::string> clipPoseLibrary;u32 clipRigMirrorPairs=0;bool clipRigLoaded=false;
  // Fantasma: posicoes mundiais das juntas antes/depois do tempo atual (xyz por junta).
  bool clipGhost=false;std::vector<float> clipGhostBefore,clipGhostAfter;std::vector<EditorEntityId> clipGhostIds;
  // Timeline filtrada pelo osso selecionado: bindings permitidos (vazio = todos).
  bool clipFilterSelected=false;std::vector<u64> clipFilterBindings;
  bool clipPoseShown=false;u32 clipPosePage=0;
  // Painel do osso (estúdio): trilha por canal (posição, rotação, escala) do
  // osso tocado, se há chave no tempo atual e os valores locais da pose vista.
  u64 clipBoneTracks[3]{};bool clipBoneKeyed[3]{};float clipBoneValues[3][3]{};
  // Ossos com trilha na camada atual (ponto na árvore).
  std::vector<EditorEntityId> clipAnimatedBones;
  bool clipPoseAutoKey=true,clipPosePending=false;
  AnimationPoseRecordingPolicy clipPoseRecording;
  u64 clipPoseLocks[4]{};
  bool clipPoseToolsShown=false,clipPoseWorldSpace=false,clipPoseClipboardAvailable=false,clipPoseClipboardWorld=false;
  u32 clipPoseToolsPage=0;
  bool clipPoseSelectionAdd=false,clipPoseSelectionEmpty=false,clipPoseSelectionIsolated=false;
  u32 clipPoseSetPage=0;
  std::vector<EditorEntityId> clipPoseSelection;
  std::vector<AnimationPoseSelectionSet> clipPoseSets;
  bool clipPoseIsolated=false;
  bool clipPoseNumbers=false,clipPoseJoints=true,clipPreviewExpanded=false;
  // A prévia do clipe está montada: é o que o Play exige. A linha de status
  // (clipDiagnostic) também leva avisos da Pose e não decide o botão.
  bool clipPreviewReady=false;
  EditorEntityId clipPreviewTarget=0;
  std::vector<EditorEntityId> clipPreviewJoints;
  const runtime::SceneGraph *clipPoseGraph=nullptr;
  EditorGizmoHandle clipPoseAxis=EditorGizmoHandle::None;
  float clipPoseValues[resources::MaximumMorphTargets]{};u32 clipPoseCount=0;
  EditorEntityId clipOwner=0;
  resources::AssetGuid clipGuid;
  const resources::AnimationClipAsset *clipAsset=nullptr;
  const std::vector<resources::AnimationClipAsset> *clipAssets=nullptr;
  struct ClipSourceEntry {resources::AssetGuid guid;std::string name;float duration=0;};
  std::vector<ClipSourceEntry> clipSourceEntries;
  bool clipImportedPicker=false;
    u64 clipTrack=0,clipKey=0;u32 clipComponent=0,clipRow=0,clipPage=0;
    std::vector<resources::AnimationKeyAddress> clipSelection;
    bool clipSelectionMode=false,clipSelectionAdd=false,clipSelecting=false;
    ui::UiRect clipSelectionBox{};
    bool clipEditPicker=false;
    bool clipCues=false,clipCueOptions=false;u64 clipCue=0;
    std::vector<u64> clipCueSelection;
    std::shared_ptr<const resources::AnimationCueClipboard> clipCueClipboard;
    bool clipRangeShown=false;
    float clipRangeStart=0,clipRangeEnd=0,clipRangeDestination=0;
    u32 clipRangeTask=0,clipRangeCopies=1;
    float clipRangeTolerance=.001f;
    u64 clipLayer=0;bool clipLayersShown=false,clipLayerPicker=false;u32 clipLayerPage=0;
    bool clipBakeShown=false,clipBakeHasReport=false;
    bool clipBakeConsolidate=false,clipBakeReferenceShown=false,clipBakeReportConsolidated=false;
    resources::AnimationBakeSettings clipBakeSettings{};
    resources::AnimationBakeReport clipBakeReport{};
    u32 clipBakeRevision=0;u64 clipBakeTrack=0;
    std::shared_ptr<const resources::AnimationKeyClipboard> clipClipboard;
  float clipTime=0,clipStart=0,clipEnd=2,clipMinimum=-1,clipMaximum=1;
  std::string clipDiagnostic;
  EditorEntityId animatorEntity=0;u64 animatorInstance=0;u32 animatorLayer=0;
  u64 animatorState=0,animatorTransition=0,animatorParameter=0,animatorConnectFrom=0;
  float animatorPan[2]{0,0},animatorZoom=1;
  struct AnimatorRoute {u64 layer=0,machine=0;float x=0,y=0,zoom=1;};
  u64 animatorMachine=0;
  std::vector<AnimatorRoute> animatorRoutes;
  u32 animatorDrawer=0; // 0: graph, 1: parameters, 2: selected properties
  bool animatorEditShared=false;
  const scene::Animator *animatorResolved=nullptr;
  const std::vector<resources::AnimatorControllerAsset> *animatorControllers=nullptr;
  std::string animatorControllerDiagnostic;
  float animatorDetailsScroll=0,animatorParamsScroll=0;
  // Seletor aberto: 1 + índice do clipe da mistura; 0x10000 máscara da camada.
  u32 animatorPicker=0,animatorPickerPage=0,animatorNameField=0;
  const runtime::SceneTimers *timerRuntime=nullptr; // session-owned runtime inspection only
  // Hierarquia e Inspector abertos com o Play rodando (Unity: o Inspector
  // continua editável em Play e tudo volta ao sair). Mostram o mundo de
  // execução; a escrita vai ao mundo, nunca ao documento autoral.
  bool playInspect=false;
  // Resultado da última edição feita em Play, na faixa do Inspector. Recusada
  // pelo mundo, a faixa passa à cor de aviso com o motivo.
  std::string playEditNote;
  bool playEditRefused=false;
  // Arraste do cabeçalho para reordenar (Unity: arrastar o componente no
  // Inspector). Índice+1 do cartão levantado e do cabeçalho sob o dedo.
  u32 componentReorder=0,componentReorderTarget=0;
  ui::UiPoint componentReorderPoint{};
  EditorInspectorTab tab = EditorInspectorTab::Transform;
  // Duas rotas do mesmo alvo. Preferência editorial, nunca dado de runtime.
  EditorInspectorSurface inspectorSurface=EditorInspectorSurface::Inspection;
  float componentOverviewScroll=0;
  // Inspector em coluna rolável (ITsMagic Properties, Unity Inspector): quanto
  // a coluna desceu, no principal e no focado. Cartões e subseções dobram por
  // TIPO, como o foldout da Unity, e a lista guarda só o que o autor inverteu:
  // cartões e subseções abrem por padrão (o cartão Objeto não).
  float inspectorScroll=0,focusedInspectorScroll=0;
  std::vector<std::string> inspectorFolded,inspectorSections;
  // PK-4: menu ⋮ aberto de um Volume Override ((componente+1)<<8 | grupo) e a
  // lista "Adicionar override" aberta (componente+1). No Inspector do recurso
  // Perfil: grupo+1 do ⋮ e a lista aberta.
  u32 volumeMenu=0,volumeAddMenu=0,profileVolumeMenu=0;
  bool profileAddMenu=false;
  static bool listed(const std::vector<std::string> &list,std::string_view key) {
    for(const auto &item:list) if(item==key) return true;
    return false;
  }
  static void flip(std::vector<std::string> &list,std::string_view key) {
    for(auto at=list.begin();at!=list.end();++at) if(*at==key) {list.erase(at);return;}
    list.emplace_back(key);
  }
  bool inspectorCardOpen(std::string_view key) const {return (key!="astra.object")!=listed(inspectorFolded,key);}
  // Toque no cabeçalho: fechado abre e ganha o foco; aberto sem foco só ganha o
  // foco (gizmo, busca); aberto com foco fecha. Devolve se termina com foco.
  bool tapInspectorCard(std::string_view key,bool focused) {
    const bool open=inspectorCardOpen(key);
    if(!open || focused) flip(inspectorFolded,key);
    return !(open && focused);
  }
  // Pesquisa de flags da Unity: estado editorial, sem gravar bits sem consumidor.
  bool inspectorReferenceFlags=false;
  // Diagnósticos da sessão são ferramentas editoriais, fora da cena e do Undo.
  bool diagnosticDockOpen=false;
  // Folding is editor-only, keyed by object and stable component instance identity.
  EditorEntityId componentSelection=0;
  std::string expandedComponent;
  u64 expandedNative=0,nativeMenu=0;
  // Tween de propriedade com o seletor de propriedade aberto (instância).
  u64 propertyTweenPicker=0;
  u64 referenceInstance=0;
  std::string referenceProperty,referenceQuery;
  bool referenceScript=false,editingReferenceSearch=false;
  // Seletor avançado (preferência do projeto), visão 0 lista/1 grade/2 tabela,
  // filtro de tipo ligado e o resultado destacado (painel de inspeção).
  bool pickerAdvanced=false,referenceTypeFilter=true;
  // Inspector travado num objeto (Unity: cadeado); 0 segue a seleção. O alvo
  // de quem abriu o modal aberto (seletor, lista de enumeração, Add) — o
  // Inspector travado, um focado ou a seleção.
  EditorEntityId inspectorLocked=0,inspectorTarget=0;
  // Modo Debug do Inspector (Unity ⋮ > Debug): valores crus, só leitura.
  bool inspectorDebug=false;
  // Inspectors focados (Unity 6000.0 Manual/InspectorFocused): cada um presa
  // a um objeto, ou a um componente dele (instância), sem seguir a seleção.
  // Um objeto (e opcionalmente um componente dele) ou um recurso do projeto:
  // material, mapa HDRI ou perfil de ambiente (Unity: Properties de um asset).
  enum class FocusedAsset : u8 { None, Material, EnvironmentMap, EnvironmentProfile };
  struct FocusedInspector {
    EditorEntityId entity=0; u64 component=0;
    FocusedAsset kind=FocusedAsset::None; resources::AssetGuid asset{}; std::string name;
  };
  std::vector<FocusedInspector> focusedInspectors;
  u32 focusedActive=0;       // aba visível (índice+1)
  bool focusedCollapsed=false,focusedMenu=false;
  // Histórico de Desfazer: aberto pelo toque longo em Desfazer/Refazer. As
  // entradas vêm do EditorHistory (mais antiga primeiro); `undoApplied` é
  // quantas estão aplicadas, isto é, o ponto atual.
  bool undoHistory=false,undoNewestFirst=true;
  u32 undoHistoryPage=0,undoApplied=0;
  std::vector<std::string> undoEntries;
  // Camadas escondidas e não selecionáveis na vista da cena: estado do editor
  // (preferência do projeto), nunca da cena — o jogo e o Play não mudam.
  u32 hiddenLayers=0,unpickableLayers=0,sceneLayersPage=0;
  // Multisseleção: o conjunto selecionado (inclui o ativo, `selection`). No
  // modo "Selecionar vários" tocar numa linha ou objeto soma ou tira do conjunto.
  std::vector<EditorEntityId> selectionSet;
  bool multiSelect=false;
  // Visibilidade e seleção na vista, por objeto (Unity 6000.0 Scene visibility
  // e Scene picking): estado do editor, fora da cena e do Desfazer. O olho da
  // Hierarquia esconde o objeto e os descendentes (toque longo: só ele); a mão
  // tira da seleção por toque na vista. A visibilidade autoral (`visible`)
  // continua no cartão Objeto.
  std::vector<EditorEntityId> sceneHidden,scenePickOff;
  bool sceneHiddenHas(EditorEntityId id) const {return std::find(sceneHidden.begin(),sceneHidden.end(),id)!=sceneHidden.end();}
  bool scenePickOffHas(EditorEntityId id) const {return std::find(scenePickOff.begin(),scenePickOff.end(),id)!=scenePickOff.end();}
  // A sessão republica o desenho e guarda nas preferências quando muda.
  bool sceneVisibilityChanged=false;
  bool isSelected(EditorEntityId id) const {return std::find(selectionSet.begin(),selectionSet.end(),id)!=selectionSet.end();}
  // Inspector na multisseleção (Unity 6000.0 Manual/Multi-object editing): a
  // sessão compara os selecionados com o ativo. Chaves: "name", "visible",
  // "castShadow", "layer", "active", "t.<linha><eixo>" e "<instância do
  // ativo>/<propriedade>" (script: o id do campo; trio: o de cada canal).
  struct MultiEditView {
    u32 count=0,hidden=0;
    std::vector<std::string> mixed;
    std::vector<u64> common,unsupported;
    bool isMixed(std::string_view key) const {return std::find(mixed.begin(),mixed.end(),key)!=mixed.end();}
    bool isCommon(u64 instance) const {return std::find(common.begin(),common.end(),instance)!=common.end();}
    bool isUnsupported(u64 instance) const {return std::find(unsupported.begin(),unsupported.end(),instance)!=unsupported.end();}
  };
  MultiEditView multi;
  // "Definir como o valor de…": o campo (chave como acima) e, por objeto
  // selecionado, o nome e o valor dele para escolher.
  struct SetValueMenu {std::string key,label;std::vector<std::pair<EditorEntityId,std::string>> rows;};
  SetValueMenu setValueMenu;
  // Busca global: consulta, provedor escolhido (chip), página e resultados que
  // a sessão recalcula enquanto a janela está aberta.
  bool globalSearch=false,editingGlobalSearch=false;
  std::string globalQuery;
  EditorSearchProvider globalProvider=EditorSearchProvider::All;
  u32 globalPage=0;
  std::vector<EditorSearchResult> globalResults;
  EditorSearchCounts globalCounts;
  // Layouts salvos pelo usuário (até 12) e o painel que os lista.
  std::vector<EditorLayout> userLayouts;
  // Trabalhos em andamento (do shell e da sessão) e a janela que os lista.
  std::vector<EditorBackgroundTask> backgroundTasks;
  bool backgroundPanel=false;
  bool layoutsPanel=false,namingLayout=false;
  bool sceneLayersPanel=false;
  // Ping: objeto destacado na Hierarquia até `pingUntil` (relógio `uiTime`).
  EditorEntityId pingEntity=0;
  double pingUntil=0,uiTime=0;
  u8 pickerView=0;
  u64 referenceHighlight=0;
  // Tipo declarado do campo de script aberto no seletor: "object" ou
  // "component:<id>" (referência a componente).
  std::string referenceScriptType;
  u32 referencePage=0;
  // Destino do seletor de recurso. Instância zero mantém o seletor de malha
  // visual existente; valor não zero endereça um binding refletido.
  u64 resourceInstance=0;
  std::string resourceProperty;
  u32 resourceSlot=0;
  // Receita em edição no seletor da malha física. Passos finitos deixam o
  // controle utilizável por toque e mantêm a mesma validação da persistência.
  u8 collisionTrianglePercent=25;
  float collisionMaximumError=.02f;
  bool addingComponent=false;
  u32 componentPreview=0; // índice do catálogo + 1; zero mantém a lista
  bool componentPreviewValues=false;
  u32 componentPreviewPage=0;
  std::string scriptPreviewType;
  u64 scriptPreviewGeneration=0;
  bool editingComponentSearch=false,editingMeshSearch=false,meshPicker=false;
  std::string componentQuery,meshQuery;
  bool editingPropertySearch=false;
  std::string propertyQuery;
  u32 componentCategory=0,meshPage=0,meshTab=0;
  // Catálogo do Add: linha do topo da lista rolável e os tipos adicionados por
  // último nesta sessão (ids de schema), mais recente primeiro.
  u32 addScroll=0,addRailScroll=0;
  float addScrollRemainder=0;
  std::vector<std::string> recentComponents;
  u32 componentPage=0,scriptPropertyPage=0;
  u64 expandedScript=0,scriptMenu=0,editingScriptInstance=0;
  // Lista de script aberta (id do campo) e o elemento escolhido (índice+1):
  // "−" remove o escolhido, ou o último quando nenhum está.
  std::string expandedScriptArray;
  u32 scriptArraySelected=0;
  // Texto em edição: elemento (índice+1) da lista aberta, ou o Tamanho.
  u32 editingScriptElement=0;
  bool editingScriptArraySize=false;
  // Seletor de referência aberto para um elemento (índice+1).
  u32 referenceScriptElement=0;
  // Arraste pela alça para reordenar elementos (índice+1 do levantado e do alvo).
  u32 scriptArrayDrag=0,scriptArrayDragTarget=0;
  ui::UiPoint scriptArrayDragPoint{};
  EditorEntityId editingScriptEntity=0;
  std::string editingScriptProperty,editingScriptType;
  std::shared_ptr<const EditorComponentValue> componentClipboard;
  // Link externo pedido pela interface (ajuda do componente); o shell do
  // Android abre no navegador e limpa. Só https.
  std::string externalLink;
  // Lista de opções aberta por um campo de enumeração: o id do campo
  // (ComponentEnumBase + componente + campo<<8), ou zero fechada.
  u32 enumPicker=0;
  bool presetPanel=false,presetNaming=false,presetRenaming=false,presetDeleteConfirm=false;
  // Vistas salvas da cena: painel aberto, linha escolhida e o fluxo de nome
  // (salvar uma nova ou renomear a escolhida).
  bool viewsPanel=false,viewNaming=false,viewRenaming=false;
  // Escolha do modelo de cena a montar.
  bool templatePanel=false;
  u32 templatePage=0;
  // Painel Qualidade: o rascunho que o autor edita, se difere do aplicado, e o
  // que o renderer está fazendo de fato agora (resolução real e custo de GPU).
  bool qualityPanel=false,qualityDirty=false;
  u32 qualityTab=0;
  u32 qualityPage=0;
  renderer::ProjectRenderingSettings qualityDraft{};
  std::string qualityStats,qualityDetected;
  u32 qualityTemporalDebug=0;
  bool qualityTemporalAvailable=false;
  bool qualityMotionAvailable=false;
  // Diagnósticos que dependem do histórico do TAA nativo (histórico usado e
  // rejeição). Com Arm ASR/FSR 2 o histórico é da biblioteca e não é exibido.
  bool qualityHistoryDiagnostics=false;
  // Ampliação temporal: o que o aparelho suporta, o que está salvo no projeto
  // e o que o renderer realmente executou no último quadro.
  renderer::TemporalUpscalerAvailability qualityArmAsr=renderer::TemporalUpscalerAvailability::NotProbed;
  renderer::TemporalUpscalerAvailability qualityFsr2=renderer::TemporalUpscalerAvailability::NotProbed;
  renderer::ProjectRenderingSettings qualityApplied{};
  renderer::UpscalingFilter qualityExecutedUpscaler=renderer::UpscalingFilter::Bilinear;
  renderer::TemporalUpscalerAvailability qualityExecutedStatus=renderer::TemporalUpscalerAvailability::Available;
  bool qualityTemporalAaExecuted=false;
  // S2: o que o streaming de mipmaps fez no último quadro do aparelho.
  renderer::TextureStreamingStats qualityTextureStreaming{};
  // Vista de depuração do streaming na Scene View; ferramenta, fora do Undo.
  bool qualityTextureStreamingDebug=false;
  // S4: Explorador de luzes — linhas da página atual, contagens da cena e a
  // intensidade que "Aplicar" escreve nas luzes do filtro (unidade de cada luz).
  struct LightExplorerRow {
    EditorEntityId entity=kInvalidEntity;
    u64 instance=0;
    std::string name,detail;
    bool enabled=true,dark=false;
  };
  bool lightExplorer=false,lightExplorerDarkOnly=false;
  u32 lightExplorerPage=0,lightExplorerTotal=0,lightExplorerDark=0,lightExplorerFiltered=0;
  float lightExplorerIntensity=1000;
  std::vector<LightExplorerRow> lightExplorerRows;
  // Bloco F: texturas das fontes importadas (as imagens de um modelo) no mesmo
  // gerenciador e Inspector das texturas do projeto. A linha i da lista usa a
  // célula i % 50 do atlas de miniaturas; o Inspector troca o perfil (que é da
  // fonte) pela origem e pela fonte dona.
  bool textureManagerSources=false,textureViewerSource=false;
  u32 textureViewerSourceIndex=0; // posição na lista de texturas das fontes
  std::vector<std::string> sourceTextureNames;
  std::vector<ui::UiRect> sourceTextureThumbs;
  // Inspector de textura em cartões (projeto e fontes). A sessão preenche os
  // dados; a tela decide só a forma (grade, chips, trilha).
  struct TextureField {std::string label,value;};
  struct TextureOrigin {std::string label,value,copy;};
  std::vector<TextureField> textureProperties;  // além de Canal e Mip, que são controles
  std::vector<TextureOrigin> textureOrigin;
  std::vector<std::string> textureBreadcrumb;   // pastas do projeto até o arquivo
  std::string textureDimensions,textureGpuNote,textureMipValue;
  u32 textureCardsClosed=0;                     // bit por TextureInspectorCard recolhido
  float textureInspectorScroll=0;
  bool textureViewerExpanded=false,textureUsersExpanded=false;
  // S5: overlay de estatísticas no viewport e o que o renderer relatou.
  bool sceneStatisticsVisible=false;
  renderer::SceneStatistics sceneStatistics{};
  u32 viewSelected=0;
  std::string viewName;
  // Salvar o OBJETO como receita é um terceiro destino do mesmo campo de nome;
  // sem distinguir, renomear e salvar disputariam o mesmo estado.
  bool presetRecipeNaming=false;
  // O preset em foco traz vários componentes; o painel troca o rótulo da ação.
  bool presetSelectedIsRecipe=false;
  EditorEntityId presetEntity=0;
  u64 presetInstance=0,presetSelected=0,presetEpoch=0,presetRevision=0;
  u32 presetPage=0;
  std::vector<std::pair<u64,std::string>> presetChoices;
  std::vector<std::string> presetPreview;
  std::vector<std::string> presetInputNames;
  std::vector<u64> presetInputValues; // Unpublished authoring arguments, never runtime state.
  bool presetRecipeReady=false;
  std::string presetRecipeName,presetRecipeStatus;
  bool presetRecipeDetails=false;
  u32 presetRecipePage=0,presetRecipeCount=0;
  u32 presetInputIndex=0; // 1-based active argument picker.
  const scene::ComponentObjectReference *presetInputProperty=nullptr; // Static descriptor, not a component pointer.
  std::vector<EditorEntityId> presetInputChoices;
  // O diff campo a campo do preset selecionado. `selected` é a escolha do autor
  // sobre LEVAR aquele campo; começa marcado no que difere e é aplicável, que é
  // o comportamento antigo de "aplicar tudo" expresso pelo mesmo caminho.
  std::vector<EditorPresetField> presetFields;
  u32 presetFieldPage=0;
  std::string presetName;
  // Largura dos painéis em dp. Zero pede o padrão proporcional; qualquer outro
  // valor é o que o usuário arrastou e é preservado entre frames.
  float hierarchyWidth = 0.0f;
  float inspectorWidth = 0.0f;
  enum class CompactPanel { Viewport, Hierarchy, Inspector, Files };
  CompactPanel compactPanel = CompactPanel::Viewport;
  bool compactPanelMenu=false;
  bool hierarchyVisible = true;
  bool inspectorVisible = true;
  // Quantas linhas a hierarquia já rolou, em linhas inteiras.
  u32 hierarchyScroll = 0;
  float hierarchyScrollRemainder = 0.0f;
  // Câmera do viewport. Sem ela a cena continua aparecendo, mas sem grade nem
  // gizmo — que é exatamente a diferença entre um preview e um editor.
  const EditorViewport *view = nullptr;
  // Play: linhas Debug.DrawLine e a vista de jogo que as projeta (a câmera do
  // quadro, que pode não ser a do editor). Nulo fora do Play.
  const runtime::DebugLines *debugLines = nullptr;
  EditorViewport debugView{};
  bool showGrid = true;
  bool showComponentVisuals=true;
  u32 activeColliderHandle=0; // kind + 1, zero is idle
  const ColliderTopology *colliderTopology=nullptr;
  bool colliderAuthoringDetails=false;
  bool physicsDiagnosticOpen=false,physicsDiagnosticVisual=true,physicsDiagnosticCollision=true,physicsDiagnosticSupport=true;
  std::string physicsDiagnosticText,physicsDiagnosticError;
  EditorEntityId physicsDiagnosticTarget=0;
  float physicsDiagnosticCom[3]{},physicsDiagnosticPoint[3]{},physicsDiagnosticNormal[3]{};
  bool physicsDiagnosticHasCom=false,physicsDiagnosticHasSupport=false,physicsDiagnosticLive=false;
  bool physicsDiagnosticHasCapsule=false;
  float physicsDiagnosticCapsuleBottom[3]{},physicsDiagnosticCapsuleTop[3]{},physicsDiagnosticCapsuleRadius=0;
  std::vector<std::array<float,3>> physicsDiagnosticProbes;
  double physicsDiagnosticMs=0,colliderUiMs=0;u64 physicsDiagnosticBuilds=0;
  std::span<const EditorPickCandidate> colliderHandleOccluders;
  // Opções da câmera editorial, equivalentes à barra de visualização da Scene
  // View. São estado do editor: não alteram o Ambiente salvo nem entram no
  // histórico da cena.
  bool sceneLighting=true,sceneEffects=true,sceneSky=true,sceneFog=true,scenePost=true;
  // G3c: modo de visão Baked Lightmap do Scene view (só o lightmap, sem albedo).
  bool sceneBakedLightmap=false;
  bool sceneEffectsMenu=false;
  EditorEntityId cameraViewEntity=0;
  EditorEntityId cameraPreviewEntity=0;
  bool cameraPreviewReady=false,cameraPreviewFailed=false;
  // Play com duas câmeras na tela (PK-7): o retângulo da câmera de cima na
  // vista de jogo e se a imagem dela já foi desenhada pelo renderer.
  ui::UiRect playStackRect{};
  bool playStackReady=false;
  bool cameraPreviewMaterial=false;
  bool cameraPreviewActive=false,cameraPreviewStandalone=false;
  u8 materialPreviewShape=0;
  std::string cameraPreviewDiagnostic;
  u32 cameraPreviewWidth=640,cameraPreviewHeight=360;
  float cameraPreviewFrequency=15;
  bool cameraPiloting=false;
  std::string componentGroup;
  EditorNavigationMode navigation = EditorNavigationMode::Orbit;
  EditorGizmoHandle activeGizmoAxis = EditorGizmoHandle::None;
  const char *projectName = "Untitled";
  u64 impactInstance=0;
  EditorEntityId impactEntity=0;
  u32 impactPage=0;
  resources::AssetGuid impactAsset{};
  std::vector<std::pair<resources::AssetGuid,u32>> impactTrail;
  bool impactRepair=false;
  bool impactRemoval=false;
  u64 impactRemovalEpoch=0,impactRemovalRevision=0;
  bool impactRepairScene=false;
  resources::AssetGuid impactReplacement{};
  u64 impactRepairRevision=0;
  u64 impactRepairEpoch=0;
  resources::AssetGuid impactRepairMaterial{};
  u32 impactRepairMaterialRevision=0;
  u32 colorField=0;
  EditorEntityId colorEntity=0;
  u64 colorInstance=0,colorRevision=0;
  std::string colorProperty;
  float colorHue=0,colorSaturation=0,colorValue=1;
  // Alfa e intensidade HDR (stops: cor = base × 2^intensidade) quando o campo
  // os tem; a cor com que a janela abriu, em RGB linear, para voltar a ela.
  float colorAlpha=1,colorIntensity=0;
  float colorOriginal[4]{1,1,1,1};
  bool colorHasAlpha=false,colorHdr=false;
  // 0 = RGB 0–255, 1 = RGB 0–1, 2 = HSV.
  u8 colorMode=0;
  // Alvo: 0 = triple de componente, 1 = campo de script, 2 = elemento de lista.
  u8 colorTarget=0;
  std::string colorScriptType;
  u32 colorScriptElement=0;
  // Menus da faixa de amostras: amostra (índice+1) e lista de bibliotecas.
  u32 colorSwatchMenu=0;
  bool colorLibraryMenu=false;
  // Texto em edição na janela: 1 = hexadecimal, 2 = nome da amostra, 3 = nova biblioteca.
  u8 colorText=0;
  // Conta-gotas (Unity: eyedropper): a janela some, o próximo toque na tela
  // vira um pedido de amostra ao renderer; `colorSampling` enquanto a cor não volta.
  bool colorPicking=false,colorSampling=false;
  const class EditorValueLibraries *colorLibraries=nullptr;
  // Alvo 3 da janela de cor: a parada de cor escolhida no editor de gradiente.
  // Editor de gradiente aberto (chave do campo), rascunho no formato do campo,
  // parada escolhida (índice+1, de alfa ou de cor) e parada sendo arrastada
  // para fora (vai ser apagada ao soltar).
  u32 gradientField=0;
  std::string gradientDraft,gradientType;
  EditorEntityId gradientEntity=0;
  u64 gradientInstance=0;
  std::string gradientProperty;
  u32 gradientElement=0,gradientSelected=0;
  bool gradientSelectedAlpha=false,gradientRemoving=false;
  u32 gradientPresetMenu=0;
  bool gradientLibraryMenu=false;
  u8 gradientText=0;
  const class EditorValueLibraries *gradientLibraries=nullptr;
  // Editor de curvas aberto (chave do campo), rascunho no formato do campo,
  // chave escolhida (índice+1), faixa visível {t0,v0,t1,v1} e menus de preset.
  u32 curveField=0;
  std::string curveDraft,curveType;
  EditorEntityId curveEntity=0;
  u64 curveInstance=0;
  std::string curveProperty;
  u32 curveElement=0,curveSelected=0;
  // PK-5: curva de Color Curves em edição (canal+1); do Perfil aberto ou do
  // Ambiente (curveEntity/curveInstance).
  u32 curveVolumeChannel=0;bool curveVolumeProfile=false;
  float curveView[4]{0,0,1,1};
  u32 curvePresetMenu=0;
  bool curveLibraryMenu=false;
  u8 curveText=0;
  const class EditorValueLibraries *curveLibraries=nullptr;
  // Barra do LOD Group: nível escolhido (índice+1; levelCount+1 = Culled),
  // menu do segmento aberto e a altura da vista atual em % (-1 sem leitura).
  u32 lodSelected=0,lodMenu=0;
  float lodViewPercent=-1;
  u32 numericField = 0;
  u64 numericInstance=0;
  std::string numericProperty;
  EditorEntityId numericEntity = kInvalidEntity;
  char numericText[48]{};
  // Valor do campo quando ele abriu: base de `+=` e da prévia do resultado.
  double numericCurrent=0;
  // Teclado de texto no lugar do numérico do sistema, para digitar expressões
  // (operadores, parênteses, pi, L e R não existem no teclado numérico).
  bool numericExpression=false;
  bool numericReplace = false;
  bool numericError = false;
  bool platformTextInput = false;
  // Texto VIVO vindo do IME do sistema, e o cursor em bytes dentro dele.
  //
  // O teclado é do Android; o CAMPO é do editor. A ponte não desenha nada: ela
  // carrega o IME, entrega o texto a cada tecla e diz quanto da tela o teclado
  // ocupa. Antes disso a edição inteira acontecia num `AlertDialog` que cobria
  // a tela, e o usuário não via o que estava editando enquanto editava.
  std::string platformDraft;
  u32 platformCaret = 0;
  // Fração da altura da superfície ocupada pelo teclado do sistema, 0..1. Vem
  // como fração e não como pixels porque o Android mede em pixels físicos e
  // esta superfície é lógica; converter no meio do caminho seria mais uma
  // unidade para errar.
  float platformImeFraction = 0.0f;
  bool entityMenu = false;
  EditorEntityId characterConversionTarget=0;
  // Personagem que as receitas de Criar › Personagem vão usar (da seleção ou
  // o único da cena); 0 desliga a categoria.
  EditorEntityId characterRecipeTarget=0;
  std::string characterConversionError;
  float characterConversionRadius=0,characterConversionHeight=0;
  EditorEntityId motorSetupTarget=0;
  u32 motorSetupPolicy=0;
  std::string motorSetupError,motorSetupSummary;
  u32 motorBakeBudget=1,motorBakePage=0;
  // Regeneration reviews identity, local edits and correspondence together.
  // One part per page keeps every candidate reachable, even when the full
  // Inspector chrome leaves less room than the surface height suggests.
  u32 motorBakePartsPerPage() const noexcept { return motorBakeRegenerating||surface.height<600?1u:3u; }
  bool motorBakeRunning=false,motorBakeReady=false;
  float motorBakeProgress=0;
  std::string motorBakeStage;
  std::vector<bool> motorBakeEnabled;
  struct MotorBakePreviewPart {std::vector<std::array<float,9>> triangles;};
  std::vector<MotorBakePreviewPart> motorBakePreview;
  struct MotorBakeSourceRow {std::string label,error;bool selected=false;};
  bool motorBakeSourcesOpen=false,motorBakeSettingsOpen=false;
  u32 motorBakeSettingsPage=0;
  resources::ConvexBakeSettings motorBakeDraft{};
  u32 motorBakeSourcePage=0,motorBakeSelectedSources=0;
  std::vector<MotorBakeSourceRow> motorBakeSourceRows;
  bool motorBakeRecipeAvailable=false,motorBakeRegenerating=false,motorBakeMappingConfirmed=true;
  std::vector<u64> motorBakePreviousParts,motorBakePartMapping;
  std::vector<std::string> motorBakePartNotes;
  std::string motorBakeMappingSummary;
  bool creationMenu=false;
  bool creationAsChild=false;
  // Menu de contexto (ITsMagic, Unity): o mesmo componente para o "+" e o "⋮"
  // da Hierarquia e o toque longo numa linha. Nível 0: ações do objeto; 1:
  // Criar (categorias); 2+n: receitas da categoria n. A raiz é o nível em que
  // ele abriu, para o voltar fechar.
  bool contextMenu=false;
  u32 contextMenuLevel=0,contextMenuRootLevel=0;
  ui::UiPoint contextMenuAnchor{};
  float contextMenuScroll=0;
  // A ação tocada no menu cria com o destino escolhido nele (raiz ou filho),
  // pelo mesmo caminho da folha de criação.
  bool contextMenuCreating=false;
  unsigned creationCategory=0,creationSelection=0,creationScroll=0;
  std::vector<u8> creationAvailable{1,1}; // Basic object and camera until resource availability is resolved.
  // O editor não conhece Android: ele levanta o pedido e o shell abre o seletor.
  bool modelImportRequested=false,environmentImportRequested=false,textureImportRequested=false,folderImportRequested=false;
  bool waveImportRequested=false;
  bool importPanel=false,importBatch=false,importReady=false,importAccept=false,importCancel=false,importIntoScene=false,importError=false;
  bool importEnvironment=false,importTexture=false;
  resources::EnvironmentMapImportSettings environmentImportSettings{};
  bool environmentImportReprepare=false;
  resources::TextureProfile textureImportSettings{},textureImportPreparedSettings{};
  bool textureImportReprepare=false;
  u32 textureImportSourceWidth=0,textureImportSourceHeight=0,textureImportDroppedMips=0;
  bool textureImportSourceHasAlpha=false;
  bool textureImportTiff=false,textureImportTiffUndefinedExtra=false,textureImportTiffHasIcc=false,textureImportTiffMatrixSrgb=false;
  u16 textureImportTiffBits=0;
  ui::UiRect textureImportImage{};
  std::string importSummary,importPath;
  u32 importPage=0;
  bool codeRecoveryPending=false;
  // Última mensagem da importação, para a barra de status.
  std::string importStatus;
  // M08.2: ambiguidades da reimportação e a escolha EXPLÍCITA do usuário
  // (0 nenhuma, 1 associar pela ordem, 2 tratar como novos).
  u32 importAmbiguities=0;
  u32 importAmbiguityChoice=0;
  // R3: o importador vive em Propriedades, com dados estruturados por aba em vez
  // de um texto paginado numa janela que bloqueava o editor inteiro.
  enum class ImportTab : u8 { Summary, Structure, Meshes, Textures, Profile, Rig };
  ImportTab importTab=ImportTab::Summary;
  // Uma linha da aba Estrutura. `node` é a identidade que o nó terá no mapa —
  // a MESMA que a publicação vai gravar —, e é por ela que a exclusão é pedida.
  // `excluded` já vem propagado: filho de nó excluído também não vem.
  struct ImportNodeRow { std::string name; u32 depth=0, draws=0; resources::AssetGuid node{}; bool excluded=false; };
  // `format` é o de `renderer::AuthoringTexture` (RGBA8 ou ASTC NxN) que vai para a GPU.
  struct ImportTextureRow { u32 width=0,height=0,levels=0,uses=0,format=0; bool srgb=true; u64 bytes=0; };
  // G6-A: uma linha por malha da fonte, com os fatos que o Mesh asset Inspector
  // da Unity mostra (contagem e canais) e a medida que ela não traz — densidade
  // de texel. `level` é 0 sem nada a apontar, 1 atenção, 2 erro.
  struct ImportMeshRow {
    std::string name,material,channels,counts,size,density,stretch;
    std::vector<std::string> issues;
    u8 level=0;
  };
  std::vector<ImportNodeRow> importNodes;
  std::vector<ImportTextureRow> importTextures;
  std::vector<ImportMeshRow> importMeshes;
  // Leitura do arquivo inteiro (hierarquia, escala e densidade) e a malha
  // aberta no cartão de detalhe.
  std::string importMeshSummary;
  u32 importMeshSelected=0;
  bool importMeshDetail=false;
  // Tamanho do modelo preparado, em unidades da cena, já com a escala preparada.
  float importExtent[3]{};
  bool importHasExtent=false;
  // Perfil em edição (rascunho) e o perfil com que a prévia atual foi preparada.
  // Publicar exige os dois iguais: a prévia tem de ser a do perfil mostrado.
  float importScale=1,importPreparedScale=1;
  u32 importTextureDimension=2048,importPreparedTextureDimension=2048;
  // Geometria derivada do perfil (G2). O campo `Prepared` é o que a prévia na
  // tela já reflete; divergir dele é o que habilita "Preparar com este perfil".
  u8 importNormals=0,importPreparedNormals=0;
  u8 importNormalWeighting=0,importPreparedNormalWeighting=0;
  u32 importSmoothingAngle=60,importPreparedSmoothingAngle=60;
  u8 importTangents=0,importPreparedTangents=0;
  bool importCameras=false,importPreparedCameras=false;
  bool importLights=false,importPreparedLights=false;
  // S1: compressão das texturas no aparelho (0 RGBA8, 4/6/8 ASTC NxN) e se o
  // aparelho amostra ASTC — sem isso a escolha fica salva e o painel diz que não vale aqui.
  u8 importTextureCompression=0,importPreparedTextureCompression=0;
  // S3: LOD na própria malha e ordem de índices; pedem nova preparação.
  bool importGenerateLods=false,importPreparedGenerateLods=false;
  bool importOptimizeOrder=false,importPreparedOptimizeOrder=false;
  u8 importLodLevels=4,importPreparedLodLevels=4;
  bool importFbxDeformation=true,importPreparedFbxDeformation=true;
  bool importFbxReduceWeights=false,importPreparedFbxReduceWeights=false;
  bool importFbxApproximateSteps=false,importPreparedFbxApproximateSteps=false;
  u8 importFbxBakeRate=30,importPreparedFbxBakeRate=30;
  // G3c: Generate Lightmap UVs + Hard Angle + Pack Margin; pedem nova preparação.
  bool importLightmapUvs=false,importPreparedLightmapUvs=false;
  u8 importLightmapHardAngle=88,importPreparedLightmapHardAngle=88;
  u8 importLightmapPackMargin=4,importPreparedLightmapPackMargin=4;
  bool importRetryable=false,importPublishedView=false;
  struct ImportTakeRow {std::string name;float duration=0;u32 channels=0,morphChannels=0;};
  std::vector<ImportTakeRow> importTakes;
  u32 importTake=0,importTakeRoot=0,importRigJoints=0,importRigMorphs=0;
  resources::AssetGuid importTakeSource{};
  std::string importRigDiagnostic;
  // S2: streaming das texturas desta fonte; não pede nova preparação.
  bool importTextureStreaming=true;
  i32 importTextureStreamingPriority=0;
  bool importAstcSupported=false;
  // Nós que o autor tirou da importação. Não pede nova preparação: a saída do
  // importador é a mesma; muda o que a reconciliação instancia.
  std::vector<resources::AssetGuid> importExcludedNodes;
  // O que a publicação faz com a cena aberta. Separado do resumo porque muda a
  // cada nó marcado ou desmarcado, sem reler o arquivo.
  std::string importImpact;
  bool importReprepare=false;
  // Vínculo do objeto selecionado com a fonte, preparado pela sessão.
  struct ImportLinkView { bool linked=false,orphan=false,root=false; std::string source,node; u32 overrides=0; };
  ImportLinkView importLink;
  bool importLinkMenu=false;
  // Material por slot (Entrega 2), preparado pela sessão para o objeto
  // selecionado. `values` está no ALCANCE escolhido: a instância mostra o que
  // é desenhado neste objeto; o compartilhado mostra o recurso do projeto.
  struct MaterialSlotView {
    u32 slots=0;
    std::string name;
    bool shared=false,missing=false,overridden=false;
    float values[11]{};
    // R4: textura efetiva de cada binding no alcance em edição e de onde vem.
    std::string textureNames[4],textureOrigins[4];
    // R4: modo de alfa, corte e faces efetivos no alcance em edição.
    std::string alphaLabel,alphaOrigin,cutoffLabel,sidesLabel,sidesOrigin;
    float alphaCutoff=.5f;
    bool cutoffEditable=false;
    // R4: oclusão, canais, normal, origem do alfa e isolamento no alcance em edição.
    std::string occlusionLabel,occlusionOrigin,occlusionTextureLabel,occlusionStrengthLabel;
    std::string channelLabels[3];
    std::string normalFlipLabel,normalFlipOrigin,alphaSourceLabel,alphaSourceOrigin,isolateLabel;
    // G3a: rugosidade direta ou suavidade (Standard da Unity) e de onde vem.
    std::string roughnessSourceLabel,roughnessSourceOrigin;
    // PK-9: o material do projeto aberto, inteiro, para o Inspector por shader
    // (editor_material_fields.h), e o nome da textura de cada binding do
    // seletor (0..3 do pacote, 4 oclusão, 5..9 altura, detalhe e verniz).
    bool assetValid=false;
    resources::MaterialAsset asset{};
    std::string fieldTextures[10];
  };
  MaterialSlotView materialSlotView;
  // Material do projeto aberto em Propriedades (tocar num .material em
  // Arquivos). Válido: o Inspector mostra o recurso, e os campos de material
  // editam ele — todos os slots que o usam mudam juntos.
  resources::AssetGuid materialInspector{};
  std::string materialInspectorPath;
  u32 materialInspectorRevision=0,materialInspectorSlots=0,materialInspectorUse=0;
  std::vector<EditorEntityId> materialInspectorObjects;
  // Mapa HDRI do projeto em Propriedades (tocar num .hdr/.exr em Arquivos). A
  // receita é rascunho até Aplicar, que reimporta com ela; a exposição só muda
  // a prévia (panorama no atlas de prévia, com tonemap).
  resources::AssetGuid environmentInspector{};
  std::string environmentInspectorPath;
  resources::EnvironmentMapImportSettings environmentDraft{},environmentSaved{};
  std::vector<std::string> environmentDerived;
  ui::UiRect environmentPreview{};
  float environmentExposure=0;
  u32 environmentUse=0;
  std::vector<EditorEntityId> environmentObjects;
  u32 environmentProfiles=0;
  u8 environmentMixed=0;u32 environmentPending=0;
  // Perfil de ambiente do projeto em Propriedades (Unity: Volume Profile). As
  // linhas são as propriedades do componente Ambiente que o perfil guarda,
  // descobertas pelo esquema; a sessão as prepara para o grupo escolhido.
  // PK-4 (V-06): seções recolhíveis no lugar das abas paginadas. Cabeçalho
  // (um por override do perfil, com o interruptor), a linha ALL/NONE, as
  // propriedades (com a caixa de override: `parameter`), as curvas e o botão
  // Adicionar override.
  struct ProfileRow {
    enum class Kind : u8 { Boolean, Enum, Number, Triple, EnvironmentMap, Header, AllNone, Curve, AddOverride, AddChoice, Actions } kind=Kind::Boolean;
    std::string_view id;
    std::string label,value;
    bool on=false,editable=true,color=false,mixed=false,overridden=true;
    float rgb[3]{};
    u32 group=0,parameter=~0u,curve=0;
  };
  resources::AssetGuid profileInspector{};
  std::string profileInspectorName,profileInspectorPath;
  // Material físico aberto em Propriedades: cópia do recurso, caminho e usuários.
  resources::AssetGuid physicsMaterialInspector{};
  resources::PhysicsMaterialAsset physicsMaterialView{};
  std::string physicsMaterialPath;u32 physicsMaterialUsers=0;
  u32 profileInspectorRevision=0,profileGroup=0,profileUse=0;
  std::vector<std::string> profileGroups;
  std::vector<ProfileRow> profileRows;
  u32 profileSectionsCollapsed=0;float profileInspectorScroll=0;bool profileSkybox=false;
  renderer::ColorCurve profileCurves[renderer::ColorCurveCount]{};
  std::vector<EditorEntityId> profileObjects;
  // Contexto do Inspector de recurso (material, HDRI, perfil). A janela focada
  // guarda o seu neste instantâneo, trocado com o principal (swapAssetInspector)
  // enquanto ela é atualizada, desenhada e tocada: os tratadores e os
  // desenhos leem sempre "o recurso aberto", sem saber de qual janela é.
  struct AssetInspectorSnapshot {
    resources::AssetGuid materialInspector{};bool materialShared=false;MaterialSlotView materialSlotView;
    std::string materialInspectorPath;u32 materialInspectorRevision=0,materialInspectorSlots=0,materialInspectorUse=0;
    std::vector<EditorEntityId> materialInspectorObjects;
    bool texturePicker=false;u32 textureBinding=0;float texturePickerScroll=0;
    std::string textureUvLabel,textureWrapLabel,textureFilterLabel,textureUvLabels[5];bool textureSamplerEditable=false;
    resources::AssetGuid environmentInspector{};std::string environmentInspectorPath;
    resources::EnvironmentMapImportSettings environmentDraft{},environmentSaved{};std::vector<std::string> environmentDerived;
    ui::UiRect environmentPreview{};float environmentExposure=0;u32 environmentUse=0,environmentProfiles=0;
    std::vector<EditorEntityId> environmentObjects;
    resources::AssetGuid profileInspector{};std::string profileInspectorName,profileInspectorPath;
    u32 profileInspectorRevision=0,profileGroup=0,profileUse=0;
    std::vector<std::string> profileGroups;std::vector<ProfileRow> profileRows;std::vector<EditorEntityId> profileObjects;
    u32 profileSectionsCollapsed=0;float profileInspectorScroll=0;bool profileSkybox=false;
    renderer::ColorCurve profileCurves[renderer::ColorCurveCount]{};
    u32 propertyPage=0;
  };
  AssetInspectorSnapshot focusedAsset;
  void swapAssetInspector(AssetInspectorSnapshot &other) {
    using std::swap;auto &o=other;
    swap(materialInspector,o.materialInspector);swap(materialShared,o.materialShared);swap(materialSlotView,o.materialSlotView);
    swap(materialInspectorPath,o.materialInspectorPath);swap(materialInspectorRevision,o.materialInspectorRevision);
    swap(materialInspectorSlots,o.materialInspectorSlots);swap(materialInspectorUse,o.materialInspectorUse);
    swap(materialInspectorObjects,o.materialInspectorObjects);swap(texturePicker,o.texturePicker);swap(textureBinding,o.textureBinding);swap(texturePickerScroll,o.texturePickerScroll);
    swap(textureUvLabel,o.textureUvLabel);swap(textureWrapLabel,o.textureWrapLabel);swap(textureFilterLabel,o.textureFilterLabel);
    for(u32 i=0;i<5;++i) swap(textureUvLabels[i],o.textureUvLabels[i]);
    swap(textureSamplerEditable,o.textureSamplerEditable);
    swap(environmentInspector,o.environmentInspector);swap(environmentInspectorPath,o.environmentInspectorPath);
    swap(environmentDraft,o.environmentDraft);swap(environmentSaved,o.environmentSaved);swap(environmentDerived,o.environmentDerived);
    swap(environmentPreview,o.environmentPreview);swap(environmentExposure,o.environmentExposure);swap(environmentUse,o.environmentUse);
    swap(environmentProfiles,o.environmentProfiles);swap(environmentObjects,o.environmentObjects);
    swap(profileInspector,o.profileInspector);swap(profileInspectorName,o.profileInspectorName);swap(profileInspectorPath,o.profileInspectorPath);
    swap(profileInspectorRevision,o.profileInspectorRevision);swap(profileGroup,o.profileGroup);swap(profileUse,o.profileUse);
    swap(profileGroups,o.profileGroups);swap(profileRows,o.profileRows);swap(profileObjects,o.profileObjects);
    swap(profileSectionsCollapsed,o.profileSectionsCollapsed);swap(profileInspectorScroll,o.profileInspectorScroll);
    swap(profileSkybox,o.profileSkybox);swap(profileCurves,o.profileCurves);
    swap(propertyPage,o.propertyPage);
  }
  std::vector<std::string> projectMaterials;
  u32 materialSlot=0,materialPage=0;
  // PK-9: rolagem do Inspector do Material, seções recolhidas (bit por
  // MaterialSection), a lista aberta de um campo de escolha (índice + 1) e o
  // valor de um slider enquanto o dedo o arrasta (campo + 1).
  float materialInspectorScroll=0;
  u32 materialSectionsCollapsed=0,materialChoiceField=0,materialSliderField=0;
  float materialSliderValue=0;
  // R4: dado do material isolado na prévia (MaterialIsolate*); transitório.
  u8 materialIsolate=0;
  // R4: textura escolhida em Arquivos mostrada em Propriedades e gerenciador de texturas.
  bool textureInspector=false,textureManager=false,searchingTextures=false;
  u8 textureFilter=0;
  u32 textureManagerPage=0;
  std::string textureQuery,textureFolder;
  std::vector<u32> textureManagerRows;
  std::vector<std::string> textureUserLabels;
  std::vector<EditorEntityId> textureUserEntities;
  bool materialShared=false,materialPicker=false;
  // R4: seletor de textura de um binding e as texturas do projeto para ele.
  bool texturePicker=false;
  float texturePickerScroll=0;
  // R4: o renderer do aparelho descarta faces por material (culling dinâmico).
  bool materialCulling=true;
  u32 textureBinding=0;
  std::vector<std::string> projectTextureNames,projectTextureDetails;
  // R4: miniatura de cada textura do projeto no atlas de prévia (vazia enquanto não gerada).
  std::vector<ui::UiRect> projectTextureThumbs;
  // R4: amostragem do binding aberto no seletor, no alcance em edição.
  std::string textureUvLabel,textureWrapLabel,textureFilterLabel;
  bool textureSamplerEditable=false;
  // Deslocamento U/V, escala U/V e rotação efetivos da UV do binding.
  std::string textureUvLabels[5];
  // R4: visualizador de textura em Propriedades.
  bool textureViewer=false;
  u32 textureViewerIndex=0,textureViewerLevel=0,textureViewerLevels=0;
  u8 textureViewerChannel=0,textureViewerZoom=0,textureViewerBackground=0;
  ui::UiRect textureViewerImage{};
  std::string textureViewerTitle,textureViewerLevelLabel,textureViewerChannelLabel,textureViewerInfo;
  std::string textureViewerZoomLabel,textureViewerBackgroundLabel;
  // R4: perfil da textura (interpretação, tamanho, mips, bordas, anisotropia) e residência.
  // Três páginas de quatro: as duas últimas da terceira são leitura do
  // streaming (mip carregado e o que a tela pede), sem toque.
  std::string textureProfileLabels[12],textureResidencyLabel;
  resources::TextureProfile textureProfileDraft{},textureProfileSaved{};
  bool textureProfileDirty=false;
  u32 textureProfilePage=0;
  // Menu de ações do objeto (⋮ do cabeçalho do inspetor) e do card Transformação.
  bool inspectorMenu=false,transformMenu=false;
  bool prefabOverridesOpen=false;
  // Rig articulado: folha que substitui Propriedades, viewport sempre livre.
  bool rigToolOpen=false;
  EditorRigScreenState rigTool;
  std::string prefabOperationStatus;
  std::string constraintStatus;
  bool constraintStatusWarning=false;
  std::string audioStatus;
  bool audioStatusWarning=false;
  bool pathEditorOpen=false,pathPointList=false,pathTangents=false,pathOrientation=false;
  EditorEntityId pathEntity=0;
  u64 pathInstance=0,pathPointId=0;
  u32 pathPointPage=0;
  std::string pathStatus,followStatus;
  bool pathStatusWarning=false,followStatusWarning=false;
  PrefabOverrideView prefabOverrides;
  // Transformação copiada, para colar em outro objeto.
  bool hasTransformClipboard=false;
  EditorTransform transformClipboard{};
  bool editingCreationSearch=false;
  char creationSearch[kEditorNameCapacity]{};
  bool editingHierarchySearch=false;
  char hierarchySearch[kEditorNameCapacity]{};
  u32 waterTab=0,routePoint=0;
  std::vector<EditorEntityId> collapsedEntities;
  EditorEntityId renameEntity = kInvalidEntity;
  char renameText[kEditorNameCapacity]{};
  bool renameUppercase = false;
  u32 propertyPage = 0;
  EditorEntityId reparentEntity = kInvalidEntity;
  u32 assetCount = 0;
  u32 assetScroll = 0;
  bool draggingAsset = false;
  EditorEntityId draggingEntity = kInvalidEntity;
  ui::UiPoint assetDragPosition{};
  bool saveRequested = false;
  std::string status;
  // Nível que a câmera da vista escolhe no LOD Group selecionado, com a altura
  // relativa — a leitura da barra de LOD do Inspector da Unity. Vazio sem grupo.
  std::string lodStatus;
  // G6-B: skin e juntas resolvidos da Malha com esqueleto, e o clipe da
  // Animação selecionada (nome, duração e, no Play, o tempo corrente).
  std::string skinStatus, animationStatus;
  // Nomes dos blend shapes da malha selecionada, na ordem dos endereços de peso.
  std::vector<std::string> blendShapeNames;
  bool canUndo = false;
  bool canRedo = false;
};


} // namespace ae::editor
