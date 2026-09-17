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
#include "editor/editor_map_scene.h"
#include "editor/editor_screen.h"
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
