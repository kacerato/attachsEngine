// Production terrain surface + production UI instances, without loading the
// editor session, physics or image codecs. This verifies layout, not gameplay.
#include "editor/editor_screen.h"
#include "editor/editor_theme.h"
#include "editor/editor_terrain_noise_controls.h"
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
        scenario != "paint" && scenario != "noise" && scenario != "noise-source" && scenario != "normal" && scenario != "anti-tile" && scenario != "resample" && scenario != "resample-noise" && scenario != "cluster" && scenario != "variant" && scenario != "variant-maps" && scenario != "cluster-source" && scenario != "variant-picker")
      return 2;
    state.terrainTab = 2;
    state.terrainMaterialView = scenario == "library" ? 1u
                                : scenario == "layer" ? 2u
                                : scenario == "maps"  ? 3u
                                                      : 0u;
    if(scenario=="noise" || scenario=="noise-source") state.terrainMaterialView=scenario=="noise"?4u:8u;
    state.terrainMapRole = argc > 6 ? u32(std::atoi(argv[6])) : 0;
    if (state.terrainMapRole > 6)
      return 2;
    auto catalog = std::make_shared<resources::TerrainSurfaceCatalog>();
    const auto catalogBytes=read("assets/terrain/library/catalog.json");
    std::string catalogError;
    if(!resources::loadTerrainSurfaceCatalog(std::string_view(reinterpret_cast<const char *>(catalogBytes.data()),catalogBytes.size()),*catalog,catalogError)) {
      std::fprintf(stderr,"catalog: %s\n",catalogError.c_str());return 3;
    }
    state.terrainCatalog = catalog;
    for (u32 i = 0; i < 4; ++i)
      state.terrainDraft.surface.layers.push_back(catalog->materials[i].layer);
    if(scenario=="noise" || scenario=="noise-source") {
      auto &noise=state.terrainDraft.surface.detailNoise;
      noise.enabled=true;noise.map=state.terrainDraft.surface.layers[0].maps[0];
      noise.map.channel=state.terrainMapRole==4?resources::TerrainSurfaceChannel::RGB:resources::TerrainSurfaceChannel::R;
    }
    if(scenario=="normal" || scenario=="anti-tile") {
      state.terrainMaterialView=scenario=="normal"?12u:30u;
      auto &noise=state.terrainDraft.surface.normalNoise[0];
      noise.enabled=true;noise.map=state.terrainDraft.surface.layers[0].maps[1];
      for(const auto &material:catalog->materials)if(material.layer.maps[1].texture.valid()){noise.map=material.layer.maps[1];break;}
      resources::TerrainAntiTileEntry entry;entry.identity={55,1};entry.normal=noise;
      state.terrainDraft.surface.antiTileArray.enabled=true;
      state.terrainDraft.surface.antiTileArray.entries.push_back(entry);
      state.terrainDraft.surface.layers[0].antiTileEntry=entry.identity;
    }
    if(scenario=="resample" || scenario=="resample-noise") {
      state.terrainMaterialView=scenario=="resample"?50u:51u;
      auto &layer=state.terrainDraft.surface.layers[0];
      layer.resampleAlbedo=layer.resampleNormal=layer.resampleHeightBlend=true;
      layer.resampleScale=4;layer.resampleBlend=resources::TerrainDistanceBlend::Noise;
      layer.resampleNoise.enabled=true;layer.resampleNoise.map=layer.maps[0];
    }
    if(scenario=="cluster" || scenario=="variant" || scenario=="variant-maps" || scenario=="cluster-source" || scenario=="variant-picker") {
      for(const auto &material:catalog->materials)if(material.layer.cluster.enabled) {
        state.terrainDraft.surface.layers[0]=material.layer;break;
      }
      state.terrainMaterialView=scenario=="cluster"?60u:scenario=="variant"?70u:scenario=="variant-maps"?100u:scenario=="cluster-source"?62u:80u;
      auto &cluster=state.terrainDraft.surface.layers[0].cluster;
      cluster.stochastic=true;cluster.selection.enabled=true;cluster.selection.map=cluster.variants[0].maps[0];
    }
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
  const auto requireField=[&](u32 field){
    const auto id=editor::widgetId(editor::EditorWidget::TerrainNumberBase)+field;
    if(!reachable.contains(id)){std::fprintf(stderr,"unreachable field %u in %s\n",field,std::string(scenario).c_str());return false;}return true;
  };
  if(scenario=="noise")for(u32 field:{128u,129u,130u,131u,132u,133u,134u,135u})if(!requireField(field))return 4;
  if(scenario=="normal")for(u32 field:{128u,129u,130u,131u,148u,133u,136u,137u,140u,141u,144u,145u})if(!requireField(field))return 4;
  if(scenario=="anti-tile")for(u32 field:{149u,150u,151u})if(!requireField(field))return 4;
  if(scenario=="resample")for(u32 field:{152u,153u,154u})if(!requireField(field))return 4;
  if(scenario=="cluster")for(u32 field:{164u,165u})if(!requireField(field))return 4;
  if(scenario=="variant")for(u32 field:{160u,161u,162u,163u})if(!requireField(field))return 4;
  if(scenario=="variant-maps" && !requireField(176))return 4;
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
