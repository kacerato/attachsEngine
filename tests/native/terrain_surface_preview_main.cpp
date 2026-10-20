// Production terrain surface + production UI instances, without loading the
// editor session, physics or image codecs. This verifies layout, not gameplay.
#include "editor/editor_screen.h"
#include "editor/editor_theme.h"
#include "ui_software_raster.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <set>
using namespace ae;
int main(int argc, char **argv) {
  if (argc < 2)
    return 2;
  const u32 width = argc > 2 ? u32(std::atoi(argv[2])) : 1280,
            height = argc > 3 ? u32(std::atoi(argv[3])) : 720;
  if (width < 320 || width > 2560 || height < 240 || height > 1440)
    return 2;
  const auto read = [](const char *name) {
    std::ifstream f(name, std::ios::binary);
    return std::vector<u8>(std::istreambuf_iterator<char>(f), {});
  };
  const auto fontBytes = read("assets/astra-visual/ui/astra-ui-font.aeuf"),
             iconBytes = read("assets/astra-visual/ui/astra-ui-icons.aeui");
  ui::UiFont font;
  ui::UiIconAtlas icons;
  if (!font.load(fontBytes) || !icons.load(iconBytes))
    return 3;
  editor::EditorScreenState state;
  state.surface = {0, 0, float(width), float(height)};
  state.terrainOpen = true;
  state.terrainTab = 0;
  state.terrainSection = 2;
  state.terrainDraft.lodLevels = 4;
  state.terrainDraft.controlCells = 128;
  state.terrainMessage = "Pintura e LOD · confira e aplique";
  // Optional argument 5: lod (legacy default), library, layer, maps or paint.
  // Argument 6 chooses the map role 0..6. This is a deterministic authoring
  // state rendered by production UI, not evidence of GPU terrain shading.
  const std::string_view scenario = argc > 5 ? argv[5] : "lod";
  if (scenario != "lod") {
    if (scenario != "library" && scenario != "layer" && scenario != "maps" &&
        scenario != "paint")
      return 2;
    state.terrainTab = 2;
    state.terrainMaterialView = scenario == "library" ? 1u
                                : scenario == "layer" ? 2u
                                : scenario == "maps"  ? 3u
                                                      : 0u;
    state.terrainMapRole = argc > 6 ? u32(std::atoi(argv[6])) : 0;
    if (state.terrainMapRole > 6)
      return 2;
    auto catalog = std::make_shared<resources::TerrainSurfaceCatalog>();
    // Names/IDs mirror the first six supplied catalog rows. Loading, hashing
    // and image publication are covered by the editor integration tests.
    const char *names[] = {"layer_Sand and Rocks",   "layer_Cliff",
                           "layer_Grass and Clover", "SandLayer",
                           "RoughRockLayer",         "DirtLayer"};
    const char *ids[] = {
        "2c15fa1c5f2e84d4cb5639da6acdeed9", "496367f095928f44994c336d5a4ac797",
        "5b8ae2f8c0c78a84ca0d97b05588e022", "0834ef114040a42ffb0acb7095171e9a",
        "1d06ead0f1f64485f9d6ada38156ddbe", "5254a2672ae194b228033c789d8c7483"};
    for (u32 i = 0; i < 6; ++i) {
      resources::TerrainCatalogMaterial material;
      material.id = ids[i];
      material.layer.name = names[i];
      material.layer.sourceCatalogId = ids[i];
      material.layer.identity = {1, i + 1};
      material.layer.scaleX = material.layer.scaleY = 5;
      material.layer.heightBlend = true;
      material.layer.maps[0].texture = {2, i * 2 + 1};
      material.layer.maps[0].channel = resources::TerrainSurfaceChannel::RGB;
      material.layer.maps[0].linear = false;
      material.layer.maps[1].texture = {2, i * 2 + 2};
      material.layer.maps[1].channel =
          resources::TerrainSurfaceChannel::NormalRGB;
      material.layer.maps[2].factor = 0;
      material.layer.maps[5].factor = material.layer.maps[6].factor = 0;
      catalog->materials.push_back(std::move(material));
    }
    state.terrainCatalog = catalog;
    for (u32 i = 0; i < 4; ++i)
      state.terrainDraft.surface.layers.push_back(catalog->materials[i].layer);
    state.terrainMessage = "Materiais originais · confira a prévia e aplique";
  }

  const auto panel = editor::terrainPanelRect(state);
  const auto &theme = editor::editorTheme();
  const auto metrics = font.metrics(ui::UiFontWeight::Regular);
  ui::UiDrawList list;
  ui::UiInputRouter router;
  const auto render = [&]() {
    list.begin(state.surface, metrics);
    router.beginFrame();
    editor::buildTerrainSurface(state, theme, list, router, panel);
  };
  std::set<u32> reachable;
  const float limit = editor::terrainScrollLimit(state, panel);
  for (float scroll = 0; scroll <= limit + 8; scroll += 8) {
    state.terrainScroll = std::min(scroll, limit);
    render();
    for (float y = panel.y; y < panel.bottom(); y += 4)
      for (float x = panel.x; x < panel.right(); x += 4)
        reachable.insert(router.hitTest({x, y}).widgetId);
  }
  const u32 base = editor::widgetId(editor::EditorWidget::TerrainNumberBase);
  if (scenario == "lod") {
    if (!reachable.contains(base + 28) || !reachable.contains(base + 29) ||
        !reachable.contains(base + 60))
      return 4;
  } else {
    if (!reachable.contains(
            editor::widgetId(editor::EditorWidget::TerrainMaterialLibrary)) ||
        !reachable.contains(
            editor::widgetId(editor::EditorWidget::TerrainMaterialMaps)))
      return 4;
    if (scenario == "library" &&
        (!reachable.contains(
             editor::widgetId(editor::EditorWidget::TerrainCatalogItemBase)) ||
         !reachable.contains(
             editor::widgetId(editor::EditorWidget::TerrainCatalogItemBase) +
             5)))
      return 4;
    if (scenario == "layer" &&
        (!reachable.contains(base + 64) || !reachable.contains(base + 76)))
      return 4;
    if (scenario == "maps" && !reachable.contains(base + 96))
      return 4;
  }
  state.terrainScroll = argc > 4 ? std::strtof(argv[4], nullptr) : 0;
  render();
  std::vector<ui::UiInstance> instances;
  const auto built = ui::buildUiInstances(list, font, icons, 65536, instances);
  if (built.dropped || built.missingGlyphRuns)
    return 5;
  test::UiSoftwareTarget image;
  image.resize(width, height, .098f, .11f, .13f);
  test::rasterizeUi(instances, font, icons, image);
  std::FILE *file = std::fopen(argv[1], "wb");
  if (!file)
    return 6;
  std::fprintf(file, "P6\n%u %u\n255\n", width, height);
  std::vector<u8> rgb(usize(width) * height * 3);
  for (usize p = 0; p < usize(width) * height; ++p)
    for (u32 c = 0; c < 3; ++c)
      rgb[p * 3 + c] =
          u8(std::clamp(image.pixels[p * 4 + c], 0.f, 1.f) * 255 + .5f);
  const bool ok = std::fwrite(rgb.data(), 1, rgb.size(), file) == rgb.size();
  std::fclose(file);
  std::printf("%ux%u production surface; scenario %s controls reachable; %u "
              "instances; scroll %.0f/%.0f\n",
              width, height, std::string(scenario).c_str(), built.emitted,
              state.terrainScroll, limit);
  return ok ? 0 : 7;
}
