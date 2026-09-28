#include "editor/editor_route_component.h"
// Renderiza a tela do editor em um arquivo, sem GPU e sem aparelho.
//
// A interface é desenhada pela engine. Sem isto, a única forma de ver se um
// painel ficou onde deveria seria montar um APK, instalar e olhar — um ciclo de
// minutos para um ajuste de oito pixels. Aqui o ciclo é de segundos, e o que
// rasteriza é o espelho do fragment shader (tests/native/ui_software_raster.h),
// então o que aparece aqui é o que a GPU vai desenhar.
//
// Não faz parte do produto: é um alvo de ferramenta, ao lado dos testes.
//
// Uso:
//   aether_ui_preview [saida.ppm] [largura] [altura]
#include "editor/editor_history.h"
#include "editor/editor_code_workspace.h"
#include "editor/editor_value_library.h"
#include "editor/editor_curve_view.h"
#include "scene/script_behavior.h"
#include "editor/editor_map_scene.h"
#include "editor/editor_screen.h"
#include "editor/editor_component_catalog.h"
#include "editor/editor_creation_catalog.h"
#include "renderer/water_authoring_geometry.h"
#include "ui_software_raster.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace ae;

namespace {

bool readAsset(const char *relative, std::vector<u8> &out) {
  const std::string path = std::string(AETHER_REPOSITORY_ROOT) + "/" + relative;
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    std::fprintf(stderr, "nao abriu %s\n", path.c_str());
    return false;
  }
  std::fseek(file, 0, SEEK_END);
  const long size = std::ftell(file);
  std::fseek(file, 0, SEEK_SET);
  out.resize(size > 0 ? static_cast<usize>(size) : 0);
  const bool ok = size > 0 && std::fread(out.data(), 1, out.size(), file) == out.size();
  std::fclose(file);
  return ok;
}

bool writePpm(const char *path, const test::UiSoftwareTarget &target) {
  std::FILE *file = std::fopen(path, "wb");
  if (file == nullptr) return false;
  std::fprintf(file, "P6\n%u %u\n255\n", target.width, target.height);
  std::vector<u8> row(static_cast<usize>(target.width) * 3);
  for (u32 y = 0; y < target.height; ++y) {
    for (u32 x = 0; x < target.width; ++x) {
      const usize source = (static_cast<usize>(y) * target.width + x) * 4;
      for (u32 channel = 0; channel < 3; ++channel) {
        const float value = target.pixels[source + channel];
        row[static_cast<usize>(x) * 3 + channel] =
            static_cast<u8>((value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value)) * 255.0f + 0.5f);
      }
    }
    std::fwrite(row.data(), 1, row.size(), file);
  }
  std::fclose(file);
  return true;
}

} // namespace

int main(int argc, char **argv) {
  const char *output = argc > 1 ? argv[1] : "build/editor-preview.ppm";
  const u32 width = argc > 3 ? static_cast<u32>(std::atoi(argv[2])) : 1600;
  const u32 height = argc > 3 ? static_cast<u32>(std::atoi(argv[3])) : 900;

  std::vector<u8> fontBytes;
  std::vector<u8> iconBytes;
  if (!readAsset("assets/astra-visual/ui/astra-ui-font.aeuf", fontBytes)) return 1;
  if (!readAsset("assets/astra-visual/ui/astra-ui-icons.aeui", iconBytes)) return 1;

  ui::UiFont font;
  ui::UiIconAtlas icons;
  if (!font.load(fontBytes)) {
    std::fprintf(stderr, "fonte recusada\n");
    return 1;
  }
  if (!icons.load(iconBytes)) {
    std::fprintf(stderr, "atlas de icones recusado\n");
    return 1;
  }

  editor::EditorDocument document;
  editor::EditorHistory history;
  editor::EditorMapScene map;
  editor::EditorEntityId selection = editor::kInvalidEntity;
  // No synthetic scene. An optional repository-relative AEMAP uses the same
  // import contract as Android, so every hierarchy row has package geometry.
  if(argc>5) {
    std::vector<u8> bytes;renderer::MapPackageView package;
    if(!readAsset(argv[5],bytes) || !renderer::decodeMapPackage(bytes,package) ||
       !map.import(document,package.draws,package.materials)) {
      std::fprintf(stderr,"pacote de cena recusado\n");return 1;
    }
    const auto children=document.childrenOf(document.root());
    if(!children.empty()) selection=children.front();
  }

  editor::EditorScreenState state{};
  editor::EditorConsole console;
  editor::EditorCodeWorkspace code;
  editor::EditorValueLibraries swatches;
  if(argc>4 && std::string(argv[4]).starts_with("river")) {
    std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
    if(!renderer::appendWaterAuthoringGeometry(renderer::MapVertexStride,32,vertices,indices,draws,materials) || !map.import(document,draws,materials,false)) return 1;
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Water,"River");
    auto value=*document.find(selection);editMeshRenderer(value)->mesh=3;editor::editWaterRoute(value)->count=3;
    editor::editWaterRoute(value)->points[0].position[2]=-12;editor::editWaterRoute(value)->points[1].position[0]=8;editor::editWaterRoute(value)->points[2].position[2]=12;
    if(!document.applyEntityValues(selection,value)) return 1;
    state.waterTab=std::string(argv[4])=="river-physics"?2:std::string(argv[4])=="river-effects"?3:1;
  }
  if(argc>4 && std::string(argv[4])=="create") state.creationMenu=true;
  // Add Component aberto sobre um objeto vazio: o painel inteiro do catálogo.
  if(argc>4 && std::string(argv[4]).starts_with("add")) {
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Objeto vazio");
    state.componentSelection=selection;state.addingComponent=true;
    if(std::string(argv[4])=="add-search") state.componentQuery="cam";
    // Objeto com Câmera: o catálogo sugere Olhar e Acompanhar alvo, e a prévia
    // de Olhar mostra a composição no painel de detalhe.
    if(std::string(argv[4])=="add-preview") {
      auto value=*document.find(selection);editCamera(value);document.applyEntityValues(selection,value);
      state.componentPreview=editor::editorComponentIndex("astra.camera.look")+1;
    }
  }
  if(argc>4 && std::string(argv[4]).starts_with("create")) state.creationAvailable=editor::creationAlwaysAvailable();
  // Menu do componente e lista de opções de enumeração sobre um colisor.
  if(argc>4 && (std::string(argv[4])=="menu" || std::string(argv[4])=="enum")) {
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Porta");
    auto value=*document.find(selection);
    value.components.add(scene::PhysicsBody::descriptor);
    const auto *collider=value.components.add(scene::Collider::descriptor);
    document.applyEntityValues(selection,value);
    state.componentSelection=selection;
    if(std::string(argv[4])=="menu") state.nativeMenu=collider->instanceId();
    else state.enumPicker=editor::widgetId(editor::EditorWidget::ComponentEnumBase)+1;
  }
  // Play com Hierarquia e Inspector abertos sobre o mundo em execução.
  if(argc>4 && std::string(argv[4]).starts_with("play-inspect")) {
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Poste");
    auto value=*document.find(selection);
    value.components.add(scene::Light::descriptor);
    value.components.add(scene::PhysicsBody::descriptor);
    value.components.add(scene::Collider::descriptor);
    document.applyEntityValues(selection,value);
    document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Chão");
    state.componentSelection=selection;
    state.workspace=editor::EditorWorkspace::Play;state.playInspect=true;
    state.playEditNote="Alterado em Play · volta ao parar";
    if(std::string(argv[4])=="play-inspect-refused") {
      state.playEditNote="Recusado em Play · Estático · ver console";
      state.playEditRefused=true;
    }
  }
  // Comportamento com campos de componente (Unity: `public Rigidbody alvo;`):
  // um atribuído, um cuja instância sumiu e um número, e o seletor filtrado.
  if(argc>4 && std::string(argv[4]).starts_with("script-component")) {
    code.applyBuildReport("ASTRA_CODE 1 1 0 1 \"project.Seguidor\" \"Seguidor\" \"Scripts/Seguidor.cs\" 3 "
                          "\"alvo\" \"Alvo\" \"component:astra.physics.body\" "
                          "\"camera\" \"Câmera\" \"component:astra.camera\" "
                          "\"velocidade\" \"Velocidade\" \"float\"",code.generation());
    code.publishBuild();
    state.code=&code;
    const auto crate=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Caixa");
    auto crateValue=*document.find(crate);
    const u64 body=crateValue.components.add(scene::PhysicsBody::descriptor)->instanceId();
    crateValue.components.add(scene::Collider::descriptor);
    document.applyEntityValues(crate,crateValue);
    const auto barrel=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Barril");
    auto barrelValue=*document.find(barrel);
    barrelValue.components.add(scene::PhysicsBody::descriptor);
    barrelValue.components.add(scene::Collider::descriptor);
    barrelValue.components.add(scene::Collider::descriptor);
    document.applyEntityValues(barrel,barrelValue);
    document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Luz do poste");
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Jogador");
    auto value=*document.find(selection);
    auto *script=static_cast<scene::ScriptBehavior *>(value.components.add(scene::ScriptBehavior::descriptor));
    script->scriptType="project.Seguidor";script->source="Scripts/Seguidor.cs";
    script->setProperty("alvo","component:astra.physics.body",scene::scriptComponentValue(crate,body));
    script->setProperty("camera","component:astra.camera",scene::scriptComponentValue(crate,77));
    script->setProperty("velocidade","float","4.5");
    const u64 instance=script->instanceId();
    document.applyEntityValues(selection,value);
    state.componentSelection=selection;state.expandedScript=instance;
    if(std::string(argv[4])=="script-component-picker") {
      state.referenceInstance=instance;state.referenceProperty="alvo";state.referenceScript=true;
      state.referenceScriptType="component:astra.physics.body";
    }
  }
  // Arraste do cabeçalho: a Luz levantada sobre a Malha.
  if(argc>4 && std::string(argv[4])=="reorder") {
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Poste");
    auto value=*document.find(selection);
    value.components.add(scene::MeshRenderer::descriptor);
    value.components.add(scene::Light::descriptor);
    document.applyEntityValues(selection,value);
    state.componentSelection=selection;
    state.componentReorder=2;state.componentReorderTarget=1;
    state.componentReorderPoint={static_cast<float>(width)-150.0f,190.0f};
  }
  // Listas em campos de script (Unity Manual/InspectorArray): uma de números e
  // uma de componentes aberta, com um elemento escolhido e um ausente; um campo
  // [HideInInspector] que não aparece.
  if(argc>4 && std::string(argv[4])=="script-array") {
    code.applyBuildReport("ASTRA_CODE 3 1 0 1 \"project.Patrulha\" \"Patrulha\" \"Scripts/Patrulha.cs\" 4 "
                          "\"ativo\" \"Ativa\" \"bool\" 0 "
                          "\"esperas\" \"Esperas\" \"array:float\" 0 "
                          "\"pontos\" \"Pontos\" \"array:component:astra.physics.body\" 0 "
                          "\"semente\" \"Semente\" \"int32\" 1",code.generation());
    code.publishBuild();
    state.code=&code;
    std::vector<std::string> bodies;
    for(const char *name:{"Poste A","Poste B"}) {
      const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,name);
      auto value=*document.find(id);
      const u64 body=value.components.add(scene::PhysicsBody::descriptor)->instanceId();
      value.components.add(scene::Collider::descriptor);
      document.applyEntityValues(id,value);
      bodies.push_back(scene::scriptComponentValue(id,body));
    }
    bodies.push_back(scene::scriptComponentValue(bodies.size(),99));
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Guarda");
    auto value=*document.find(selection);
    auto *script=static_cast<scene::ScriptBehavior *>(value.components.add(scene::ScriptBehavior::descriptor));
    script->scriptType="project.Patrulha";script->source="Scripts/Patrulha.cs";
    script->setProperty("ativo","bool","true");
    script->setProperty("esperas","array:float",scene::scriptArrayValue({"1.5","2","4"}));
    script->setProperty("pontos","array:component:astra.physics.body",scene::scriptArrayValue(bodies));
    script->setProperty("semente","int32","7");
    const u64 instance=script->instanceId();
    document.applyEntityValues(selection,value);
    state.componentSelection=selection;state.expandedScript=instance;
    state.expandedScriptArray="pontos";state.scriptArraySelected=2;
  }
  // Janela de cor com alfa, HDR e uma biblioteca de amostras.
  if(argc>4 && std::string(argv[4]).starts_with("color")) {
    state.colorField=editor::widgetId(editor::EditorWidget::ScriptFieldBase);
    state.colorHue=.58f;state.colorSaturation=.72f;state.colorValue=.86f;state.colorAlpha=.8f;state.colorIntensity=1.5f;
    state.colorHasAlpha=true;state.colorHdr=true;
    state.colorOriginal[0]=.9f;state.colorOriginal[1]=.4f;state.colorOriginal[2]=.1f;state.colorOriginal[3]=1;
    swatches.add("Laranja","1 0.35 0.05 1");swatches.add("Céu","0.2 0.5 1 1");swatches.add("Folha","0.1 0.6 0.15 1");
    swatches.add("Neon","4 0.2 2 1");swatches.add("Cinza","0.2 0.2 0.2 1");
    state.colorLibraries=&swatches;
    if(std::string(argv[4])=="color-hsv") state.colorMode=2;
    if(std::string(argv[4])=="color-swatch") state.colorSwatchMenu=2;
  }
  // Editor de gradiente: pôr do sol com transparência e uma parada escolhida.
  if(argc>4 && std::string(argv[4]).starts_with("gradient")) {
    state.gradientField=editor::widgetId(editor::EditorWidget::ScriptFieldBase);state.gradientType="gradient";
    state.gradientDraft="2 4 0 0.02 0.03 0.2 0.35 0.9 0.25 0.08 0.7 1 0.8 0.3 1 0.9 0.95 1 3 0 1 0.6 0.4 1 0.2";
    state.gradientSelected=2;
    swatches.kind=editor::EditorLibraryKind::Gradient;
    swatches.add("Fogo","0 3 0 1 0 0 0.5 1 0.6 0 1 1 1 0.4 2 0 1 1 1");
    swatches.add("Mar","2 2 0 0 0.1 0.3 1 0.2 0.8 0.9 1 0 1");
    swatches.add("Degraus","1 3 0 1 0 0 0.5 0 1 0 1 0 0 1 1 0 1");
    state.gradientLibraries=&swatches;
    if(std::string(argv[4])=="gradient-alpha") {state.gradientSelectedAlpha=true;state.gradientSelected=2;}
  }
  // Editor de curvas: quatro chaves, a segunda escolhida (e quebrada na variante).
  if(argc>4 && std::string(argv[4]).starts_with("curve")) {
    scene::ScriptCurve curve;
    curve.post=scene::CurveWrapMode::PingPong;
    curve.keys={{0,0},{0.35f,1.2f},{0.7f,0.4f},{1,1}};
    if(std::string(argv[4])=="curve-broken") {
      curve.keys[1].broken=true;curve.keys[1].left=scene::CurveTangentMode::Linear;curve.keys[1].right=scene::CurveTangentMode::Constant;
    }
    curve.updateTangents();
    state.curveField=editor::widgetId(editor::EditorWidget::ScriptFieldBase);state.curveType="curve";
    state.curveDraft=scene::scriptCurveValue(curve);state.curveSelected=2;
    editor::frameCurve(curve,state.curveView);state.curveView[2]+=.6f;
    swatches.kind=editor::EditorLibraryKind::Curve;
    for(const auto &preset:scene::curveFactoryPresets) swatches.add(preset.name,preset.value);
    state.curveLibraries=&swatches;
  }
  // LOD Group aberto: barra dividida com o marcador da vista e um nível escolhido.
  if(argc>4 && std::string(argv[4]).starts_with("lod")) {
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Árvore");
    auto value=*document.find(selection);
    const auto *group=value.components.add(scene::LodGroup::descriptor);
    document.applyEntityValues(selection,value);
    state.componentSelection=selection;state.expandedNative=group->instanceId();
    state.lodSelected=2;state.lodViewPercent=42;
    state.lodStatus="Na vista: LOD 1 · 42% da tela";
    if(std::string(argv[4])=="lod-menu") state.lodMenu=2;
  }
  // Seletor avançado sobre o campo "Conectar corpo" da junta.
  if(argc>4 && std::string(argv[4]).starts_with("picker")) {
    for(const char *name:{"Porta","Portão","Ponte"}) {
      const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,name);
      auto value=*document.find(id);value.components.add(scene::PhysicsBody::descriptor);value.components.add(scene::Collider::descriptor);
      document.applyEntityValues(id,value);
    }
    document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Enfeite");
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Dobradiça");
    auto value=*document.find(selection);
    value.components.add(scene::PhysicsBody::descriptor);value.components.add(scene::Collider::descriptor);
    const auto *joint=value.components.add(scene::Joint::descriptor);
    document.applyEntityValues(selection,value);
    state.componentSelection=selection;state.expandedNative=joint->instanceId();
    state.referenceInstance=joint->instanceId();state.referenceProperty="connected_body";
    state.pickerAdvanced=true;
    if(std::string(argv[4])=="picker-grid") state.pickerView=1;
    if(std::string(argv[4])=="picker-table") {state.pickerView=2;state.referenceTypeFilter=false;}
    if(std::string(argv[4])=="picker") state.referenceHighlight=2;
  }
  // Inspector travado num objeto e em modo Debug, com outro objeto selecionado.
  if(argc>4 && std::string(argv[4]).starts_with("inspector-debug")) {
    const auto locked=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Lanterna");
    auto value=*document.find(locked);value.components.add(scene::Light::descriptor);value.components.add(scene::PhysicsBody::descriptor);
    value.components.add(scene::Collider::descriptor);
    document.applyEntityValues(locked,value);
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Outro objeto");
    state.inspectorLocked=locked;state.componentSelection=locked;
    state.inspectorDebug=std::string(argv[4])=="inspector-debug";
    state.pingEntity=locked;state.pingUntil=1;state.uiTime=0;
  }
  // Camadas na vista: duas nomeadas, uma escondida, uma fora da seleção.
  if(argc>4 && std::string(argv[4])=="scene-layers") {
    auto layers=document.layers();layers.setName(3,"Cenário");layers.setName(5,"Interface");layers.setName(8,"Gatilhos");
    document.setLayers(layers);
    for(u32 i=0;i<3;++i) {
      const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,i?"Árvore":"Casa");
      auto value=*document.find(id);value.layer=3;document.applyEntityValues(id,value);
    }
    const auto trigger=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Zona de porta");
    auto value=*document.find(trigger);value.layer=8;document.applyEntityValues(trigger,value);
    state.sceneLayersPanel=true;state.hiddenLayers=1u<<8;state.unpickableLayers=1u<<3;
  }
  // Material do projeto em Propriedades (a vista é a que a sessão prepara).
  if(argc>4 && std::string(argv[4])=="material-asset") {
    state.materialInspector=resources::assetGuidFromSeed("preview-material");state.materialShared=true;
    auto &view=state.materialSlotView;view.slots=1;view.shared=true;view.name="Madeira envernizada";
    state.materialInspectorPath="Materiais/Madeira envernizada.material";state.materialInspectorRevision=7;
    state.materialInspectorSlots=5;state.materialInspectorObjects={1,2,3};
    view.textureNames[0]="madeira-cor.png";view.textureOrigins[0]="do material do projeto";
    view.textureNames[1]="madeira-normal.png";view.textureOrigins[1]="do material do projeto · amostragem própria";
    view.textureNames[2]="Textura da fonte de cada uso";view.textureOrigins[2]="da fonte de cada uso";
    view.textureNames[3]="Sem textura";view.textureOrigins[3]="do material do projeto";
  }
  // Barra de status com mensagem do console e trabalhos; "status-tasks" abre a janela.
  if(argc>4 && std::string(argv[4]).starts_with("status")) {
    editor::EditorConsoleEntry warning;warning.severity=editor::EditorConsoleSeverity::Warning;
    warning.message="Textura sem mipmaps: porta-madeira.png";console.add(warning);
    editor::EditorConsoleEntry error;error.severity=editor::EditorConsoleSeverity::Error;
    error.message="Scripts/Porta.cs(12,5): ';' esperado";console.add(error);
    state.console=&console;state.uiTime=0.4;
    using Task=editor::EditorBackgroundTask;
    state.backgroundTasks={{Task::Kind::ProjectOpen,"Abrindo projeto","Recurso 3 de 8 · Lendo casa.glb",.3f,false},
                           {Task::Kind::ModelImport,"Importando modelo","Lendo e preparando a fonte",-1,true},
                           {Task::Kind::CodeBuild,"Compilando scripts","C# do projeto",-1,false}};
    state.backgroundPanel=std::string(argv[4])=="status-tasks";
  }
  // Layouts: um salvo ativo e outro salvo.
  if(argc>4 && std::string(argv[4])=="layouts") {
    state.layoutsPanel=true;
    editor::EditorLayout review;review.name="Revisão";review.hierarchyWidth=190;review.diagnosticDock=true;
    editor::EditorLayout wide;wide.name="Tablet largo";wide.inspectorWidth=340;wide.filesCollapsed=true;
    state.userLayouts={review,wide};
    state.hierarchyWidth=190;state.diagnosticDockOpen=true;
  }
  // Busca global com resultados dos três provedores.
  if(argc>4 && std::string(argv[4])=="global-search") {
    state.globalSearch=true;state.globalQuery="porta";
    state.globalResults={
      {editor::EditorSearchProvider::Scene,0,"Porta da frente","Cena / Casa · 4 componentes",ui::UiIcon::SceneObject,0},
      {editor::EditorSearchProvider::Scene,0,"Porta dos fundos","Cena / Casa / Cozinha · 3 componentes",ui::UiIcon::SceneObject,0},
      {editor::EditorSearchProvider::Project,0,"PortaAutomatica.cs","Projeto / Scripts/Portas",ui::UiIcon::IdeCode,0},
      {editor::EditorSearchProvider::Project,0,"porta-madeira.png","Projeto / Texturas",ui::UiIcon::AssetsTexture,1},
      {editor::EditorSearchProvider::Create,0,"Porta com dobradiça","Criar · corpo, colisor e junta de dobradiça",ui::UiIcon::SceneObjectAdd,2}};
    state.globalCounts={2,2,1};
  }
  // Histórico de Desfazer com passos aplicados e desfeitos.
  if(argc>4 && std::string(argv[4]).starts_with("undo-history")) {
    state.undoHistory=true;state.undoNewestFirst=std::string(argv[4])!="undo-history-oldest";
    state.undoEntries={"Criar objeto \xC2\xB7 Poste","Transformação \xC2\xB7 Cubo","Adicionar componente \xC2\xB7 Poste",
                       "Intensidade das luzes","Renomear \xC2\xB7 Poste alto","Excluir \xC2\xB7 2 objetos"};
    state.undoApplied=4;
  }
  // Inspectors focados: um de objeto, um só do componente de luz; ativo o de objeto.
  if(argc>4 && std::string(argv[4]).starts_with("focused")) {
    const auto lamp=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Lanterna");
    auto value=*document.find(lamp);const auto *light=value.components.add(scene::Light::descriptor);
    value.components.add(scene::PhysicsBody::descriptor);value.components.add(scene::Collider::descriptor);
    document.applyEntityValues(lamp,value);
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Outro objeto");
    state.focusedInspectors.push_back({lamp,0});state.focusedInspectors.push_back({lamp,light->instanceId()});
    state.focusedActive=std::string(argv[4])=="focused-component"?2:1;
    state.focusedCollapsed=std::string(argv[4])=="focused-collapsed";
    state.focusedMenu=std::string(argv[4])=="focused-menu";
  }
  if(argc>4 && std::string(argv[4]).starts_with("project")) {
    state.workspace=editor::EditorWorkspace::Project;
    state.projectSection=std::string(argv[4])=="project-input"?editor::EditorProjectSection::Input:editor::EditorProjectSection::Layers;
  }
  if(argc>4 && std::string(argv[4])=="create-physics") {
    state.creationMenu=true;state.creationCategory=3;
    editor::findCreationRecipe("physics.dynamic_sphere",&state.creationSelection);
  }
  if(argc>4 && std::string(argv[4])=="river-diagnostics") {
    state.diagnosticDockOpen=true;state.console=&console;
  }
  state.surface = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
  state.document = &document;
  state.selection = selection;
  state.projectName = argc>5 ? "Package preview" : "Empty Scene";
  if(argc>4 && std::string(argv[4])=="rotate") state.tool=editor::EditorGizmoMode::Rotate;
  if(argc>4 && std::string(argv[4])=="rename") {
    state.renameEntity=state.selection;
    std::snprintf(state.renameText,sizeof(state.renameText),"Objeto editavel");
  }
  if(argc>4 && std::string(argv[4])=="numeric") {
    state.numericField=editor::transformFieldWidget(0,0);
    std::snprintf(state.numericText,sizeof(state.numericText),"12.5");
  }
  if(argc>4 && std::string(argv[4])=="material") {

    state.tab=editor::EditorInspectorTab::Material;
  }
  if(argc>4 && std::string(argv[4])=="lighting") {
    state.selection=document.root();state.workspace=editor::EditorWorkspace::Lighting;
  }
  state.canUndo = history.canUndo();
  state.canRedo = history.canRedo();

  // Uma camera de editor olhando o objeto selecionado de cima e de lado. Sem
  // ela nao ha grade nem gizmo, e a previa mostraria a interface sobre o vazio.
  editor::EditorViewport view{};
  const float eye[3] = {6.0f, 4.5f, -9.0f};
  renderer::PerspectiveVisibilitySettings visibility{};
  view.frustum = renderer::buildPerspectiveFrustum(
      eye, 0.35f, 0.42f, static_cast<float>(width) / static_cast<float>(height), visibility);
  state.view = &view;

  ui::UiDrawList list;
  list.begin(state.surface, font.metrics(ui::UiFontWeight::Regular));
  ui::UiInputRouter router;
  router.beginFrame();
  // O retangulo da vista so existe depois do layout, e o layout precisa da vista
  // para desenhar a grade. Duas passagens resolvem: a primeira descobre onde a
  // cena mora, a segunda desenha com a projecao certa.
  editor::EditorScreenLayout layout =
      editor::buildEditorScreen(state, ui::defaultTheme(), list, router);
  view.rect = layout.viewport;
  list.begin(state.surface, font.metrics(ui::UiFontWeight::Regular));
  router.beginFrame();
  layout = editor::buildEditorScreen(state, ui::defaultTheme(), list, router);

  std::vector<ui::UiInstance> instances;
  const ui::UiInstanceBuildResult built =
      ui::buildUiInstances(list, font, icons, 16384, instances);

  test::UiSoftwareTarget target;
  // Um cinza-azulado no lugar da cena 3D: preto esconderia um painel preto que
  // não foi desenhado, e é justamente isso que a pré-visualização tem de expor.
  target.resize(width, height, 0.16f, 0.18f, 0.20f);
  test::rasterizeUi(instances, font, icons, target);

  if (!writePpm(output, target)) {
    std::fprintf(stderr, "nao escreveu %s\n", output);
    return 1;
  }
  std::printf("%s %ux%u\n", output, width, height);
  std::printf("comandos=%u instancias=%u descartadas=%u sem_fonte=%u recortados=%u\n",
              list.commandCount(), built.emitted, built.dropped, built.missingGlyphRuns,
              list.culledCommandCount());
  std::printf("hierarquia=%u linhas viewport=%.0fx%.0f\n", layout.hierarchyRowCount,
              static_cast<double>(layout.viewport.width),
              static_cast<double>(layout.viewport.height));
  return 0;
}
