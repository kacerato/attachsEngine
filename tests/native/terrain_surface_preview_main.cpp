// Production terrain surface + production UI instances, without loading the
// editor session, physics or image codecs. This verifies layout, not gameplay.
#include "editor/editor_screen.h"
#include "editor/editor_theme.h"
#include "editor/editor_terrain_noise_controls.h"
#include "editor/editor_terrain_modules_controls.h"
#include "editor/editor_terrain_catalog_search.h"
#include "editor/editor_terrain_snow_controls.h"
#include "editor/editor_terrain_fx_controller.h"
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
        scenario != "paint" && scenario != "noise" && scenario != "noise-source" && scenario != "normal" && scenario != "anti-tile" && scenario != "resample" && scenario != "resample-noise" && scenario != "cluster" && scenario != "variant" && scenario != "variant-maps" && scenario != "cluster-source" && scenario != "variant-picker" && scenario!="geo" && scenario!="geo-range" && scenario!="geo-layer" && scenario!="geo-curve" && scenario!="geo-tangents" && scenario!="geo-axis" && scenario!="geo-albedo-map" && scenario!="geo-normal-map" && scenario!="geo-source" && scenario!="global" && scenario!="global-tint" && scenario!="global-normal" && scenario!="global-saom" && scenario!="global-emission" && scenario!="global-saom-maps" && scenario!="global-normal-map" && scenario!="global-source" && scenario!="geo-curve-signed" && scenario!="geo-curve-empty" && scenario!="geo-curve-single" && scenario!="geo-tangents-signed" && scenario!="geo-fontes" && scenario!="global-empty" && scenario!="geo-curve-off" && scenario!="geo-off" && scenario!="catalog-search" && scenario!="catalog-empty" && scenario!="map-source" && scenario!="noise-search" && scenario!="global-search" && !scenario.starts_with("snow-") && !scenario.starts_with("fx-") && scenario!="quality" && scenario!="pbr-normal-source" && scenario!="geo-normal-source" && scenario!="global-normal-source" && scenario!="normal-noise-source" && scenario!="variant-normal-source" && scenario!="anti-normal-source")
      return 2;
    state.terrainTab = 2;
    state.terrainMaterialView = (scenario == "library" || scenario=="catalog-search" || scenario=="catalog-empty") ? 1u
                                : scenario == "layer" ? 2u
                                : scenario == "maps"  ? 3u
                                                      : 0u;
    if(scenario=="noise" || scenario=="noise-source" || scenario=="noise-search") state.terrainMaterialView=scenario=="noise"?4u:8u;
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
    if(scenario=="noise" || scenario=="noise-source" || scenario=="noise-search") {
      auto &noise=state.terrainDraft.surface.detailNoise;
      noise.enabled=true;noise.map=state.terrainDraft.surface.layers[0].maps[0];
      noise.map.channel=state.terrainMapRole==4?resources::TerrainSurfaceChannel::RGB:resources::TerrainSurfaceChannel::R;
    }
    if(scenario=="normal" || scenario=="anti-tile" || scenario=="normal-noise-source" || scenario=="anti-normal-source") {
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
    if(scenario=="cluster" || scenario=="variant" || scenario=="variant-maps" || scenario=="cluster-source" || scenario=="variant-picker" || scenario=="variant-normal-source") {
      for(const auto &material:catalog->materials)if(material.layer.cluster.enabled) {
        state.terrainDraft.surface.layers[0]=material.layer;break;
      }
      state.terrainMaterialView=scenario=="cluster"?60u:scenario=="variant"?70u:scenario=="variant-maps"?100u:scenario=="cluster-source"?62u:80u;
      auto &cluster=state.terrainDraft.surface.layers[0].cluster;
      cluster.stochastic=true;cluster.selection.enabled=true;cluster.selection.map=cluster.variants[0].maps[0];
    }
    if(scenario.starts_with("geo") || scenario.starts_with("global")) {
      auto &geo=state.terrainDraft.surface.geo;
      resources::AssetGuid::parse("863f19b183b2443859f959a96f7dce32",geo.albedo.texture);
      resources::AssetGuid::parse("192aaea3237044f608d663c9884e0c70",geo.normal.texture);
      geo.mode=resources::TerrainGeoMode::AlbedoNormal;geo.rangeEnabled=geo.curveEnabled=true;
      geo.range={-10,5,55,80};
      resources::AnimationCurveKey key;key.id=3;key.time=.5f;key.value=.8f;
      key.broken=key.weightedIn=key.weightedOut=true;key.incoming=key.outgoing=resources::AnimationTangentMode::Free;
      key.inSlope=2;key.outSlope=-2;key.inWeight=.7f;key.outWeight=.6f;
      geo.curve.keys.insert(geo.curve.keys.begin()+1,key);state.terrainProfilePoint=1;
      if(scenario=="geo-curve-signed" || scenario=="geo-tangents-signed") {geo.curve.keys.front().time=-2;geo.curve.keys.back().time=3;geo.curve.keys.front().value=-.5f;geo.curve.keys.back().value=1.2f;}
      if(scenario=="geo-curve-empty") {geo.curve.keys.clear();state.terrainProfilePoint=0;}
      if(scenario=="geo-curve-single") {geo.curve.keys={key};geo.curve.keys[0].time=-.25f;geo.curve.keys[0].value=1.5f;state.terrainProfilePoint=0;}
      if(scenario=="geo-curve-off") geo.curveEnabled=false;
      if(scenario=="geo-off") geo.mode=resources::TerrainGeoMode::Off;
      state.terrainDraft.surface.layers[0].geoHeightFilterEnabled=true;
      state.terrainDraft.surface.layers[0].geoHeightThreshold=-.25f;
      for(u32 role=0;role<4;++role) {
        auto &global=state.terrainDraft.surface.globalMaps[role];global.enabled=true;
        global.map=state.terrainDraft.surface.layers[0].maps[0];
        if(role==1) {global.map=geo.normal;global.map.linear=true;}
        if(role==2) {global.smoothness.texture=global.ao.texture=global.metallic.texture=global.mask.texture=geo.albedo.texture;}
      }
      if(scenario=="global-empty") state.terrainDraft.surface.globalMaps[0]=resources::neutralTerrainGlobalMaps()[0];
      state.terrainMaterialView=scenario=="global-empty"?160:scenario=="geo-fontes"?121:(scenario=="geo" || scenario=="geo-off")?120:scenario=="geo-range"?124:scenario=="geo-layer"?123:(scenario=="geo-curve" || scenario=="geo-curve-signed" || scenario=="geo-curve-empty" || scenario=="geo-curve-single" || scenario=="geo-curve-off")?130:
        (scenario=="geo-tangents" || scenario=="geo-tangents-signed")?131:scenario=="geo-axis"?132:scenario=="geo-albedo-map"?140:scenario=="geo-normal-map"?141:scenario=="geo-source"?150:
        scenario=="global"?159:scenario=="global-tint"?160:scenario=="global-normal"?161:scenario=="global-saom"?162:scenario=="global-emission"?163:
        scenario=="global-saom-maps"?172:scenario=="global-normal-map"?171:180;
    }
    if(scenario.starts_with("snow-")) {
      if(scenario!="snow-off" && scenario!="snow-simple" && scenario!="snow-rich" && scenario!="snow-plane" && scenario!="snow-sources" && scenario!="snow-source-empty" && scenario!="snow-normal-map" && scenario!="snow-normal-rgb-map" && scenario!="snow-normal-rg-map" && scenario!="snow-mask" && scenario!="snow-noise" && scenario!="snow-distance" && scenario!="snow-lighting" && scenario!="snow-sss" && scenario!="snow-rim" && scenario!="snow-sparkle" && scenario!="snow-layer" && scenario!="snow-mapping" && scenario!="snow-height" && scenario!="snow-source" && scenario!="snow-normal-noise-source")return 2;
      auto &snow=state.terrainDraft.surface.snow;state.terrainDraft.height=0;
      if(scenario!="snow-off" && scenario!="snow-source-empty") {
        resources::AssetGuid diffuse,normal,noise,sparkle;
        resources::AssetGuid::parse("b822881b9ef3441f29c88bb6e0ef60cd",diffuse);resources::AssetGuid::parse("cc22f06eaf78d43f782ee617776c1640",normal);
        resources::AssetGuid::parse("3364d9aca19e4401dbdb0a9bcfcd70bc",noise);resources::AssetGuid::parse("fb84fc5f3e7e14ae7be5bd45efc3d3c4",sparkle);
        for(u32 role=0;role<5;++role)editor::terrainSnowMap(snow,role).texture=role<2?diffuse:normal;
        snow.normalNoiseMap.texture=noise;snow.sparkleNoiseMap.texture=sparkle;snow.mask.texture=sparkle;
        snow.mode=scenario=="snow-simple"?resources::TerrainSnowMode::Simple:resources::TerrainSnowMode::Rich;
        snow.normalNoiseEnabled=snow.distanceResampleEnabled=snow.maskEnabled=snow.sssEnabled=snow.rimEnabled=snow.sparkleEnabled=snow.perLayerEnabled=true;
      }
      if(scenario=="snow-normal-rgb-map" || scenario=="snow-normal-rg-map"){snow.normal=state.terrainDraft.surface.layers[0].maps[1];snow.normal.channel=scenario=="snow-normal-rgb-map"?resources::TerrainSurfaceChannel::NormalRGB:resources::TerrainSurfaceChannel::NormalRG;}
      if(scenario=="snow-plane"){snow.heightAngleRange[0]=state.terrainDraft.base-1;snow.heightAngleRange[1]=state.terrainDraft.base+1;}
      state.terrainMaterialView=(scenario=="snow-sources" || scenario=="snow-source-empty")?201:(scenario=="snow-normal-map" || scenario=="snow-normal-rgb-map" || scenario=="snow-normal-rg-map")?212:scenario=="snow-mask"?216:scenario=="snow-noise"?207:scenario=="snow-distance"?208:scenario=="snow-lighting"?203:scenario=="snow-sss"?245:scenario=="snow-rim"?246:scenario=="snow-sparkle"?209:scenario=="snow-layer"?204:scenario=="snow-mapping"?205:scenario=="snow-height"?206:scenario=="snow-source"?222:200;
    }
    if(scenario=="pbr-normal-source"){state.terrainMaterialView=190;state.terrainMapRole=1;}
    if(scenario=="geo-normal-source")state.terrainMaterialView=151;
    if(scenario=="global-normal-source")state.terrainMaterialView=181;
    if(scenario=="normal-noise-source")state.terrainMaterialView=20;
    if(scenario=="anti-normal-source")state.terrainMaterialView=39;
    if(scenario=="variant-normal-source"){state.terrainMaterialView=90;state.terrainMapRole=1;}
    if(scenario=="snow-normal-noise-source")state.terrainMaterialView=225;
    if(scenario=="quality"){state.terrainMaterialView=240;state.terrainDraft.surface.textureResidency={resources::TerrainTextureResidencyMode::Cap,1024};}
    if(scenario=="map-source")state.terrainMaterialView=190;
    if(scenario=="catalog-search")state.terrainCatalogQuery=catalog->materials.back().id;
    if(scenario=="catalog-empty")state.terrainCatalogQuery="sem_resultado_903714";
    if(scenario=="noise-search" || scenario=="global-search")state.terrainCatalogQuery=catalog->textures.back().identity.text();
    // Preview fixtures construct the state directly. Mirror a fresh picker
    // entry, while routed tests separately verify saved-context overrides.
    if(scenario.starts_with("fx-")) {
      state.terrainMaterialView=scenario=="fx-mode"?306:scenario=="fx-channel"?307:scenario=="fx-brush"?301:scenario=="fx-filter"?302:scenario=="fx-budget"?303:300;
      state.terrainFx.mode=scenario=="fx-paint"?1:0;
      if(scenario=="fx-budget")state.terrainFx.resolution=2048;
      if(scenario!="fx-empty" && scenario!="fx-budget") {
        resources::TerrainFxAsset fx;fx.enabled=true;fx.width=fx.height=64;const double size=state.terrainDraft.size;fx.domain={-size*.5,-size*.5,size,size};
        resources::TerrainFxStroke stroke;stroke.identity=1;stroke.gesture=1;stroke.target=.8;stroke.sampleSeconds=.2;stroke.radiusMeters=size*.25;fx.strokes.push_back(stroke);
        std::string error;std::shared_ptr<const editor::TerrainFxEditorBitmap> bitmap;
        if(!editor::TerrainFxEditorController::prepareBitmap(fx,{},nullptr,bitmap,error))return 4;
        state.terrainDraft.fx=std::move(fx);state.terrainFx.bitmap=std::move(bitmap);
      }
    }
    if(scenario=="fx-wetness" || scenario=="fx-wetness-empty" || scenario=="fx-wet-height" || scenario=="fx-wet-distance" || scenario=="fx-porosity" || scenario=="fx-puddle") {
      state.terrainMaterialView=scenario=="fx-wet-height"?311:scenario=="fx-wet-distance"?312:scenario=="fx-porosity"?313:scenario=="fx-puddle"?320:310;
      auto &flow=state.terrainDraft.surface.flow;
      if(scenario=="fx-wetness-empty"){state.terrainDraft.fx={};state.terrainFx.bitmap.reset();}
      else {flow.wetness=true;flow.puddles=scenario=="fx-puddle";flow.heightWetness=scenario=="fx-wet-height";flow.distanceWetness=scenario=="fx-wet-distance";flow.perTexPorosity=scenario=="fx-porosity";}
    }
    if(editor::terrainCatalogSearchView(state.terrainMaterialView))state.terrainCatalogRoleFilter=editor::terrainCatalogDefaultRole(state.terrainMaterialView,state.terrainMapRole);
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
    const bool compactModule=(panel.height<400 || editor::terrainFxView(state.terrainMaterialView)) && state.terrainMaterialView!=0;
    if(compactModule) {
      for(u32 action:(scenario.starts_with("fx-")?std::vector<u32>{61}:std::vector<u32>{61,62,63})) if(!reachable.contains(editor::widgetId(editor::terrainModuleAction(action)))) return 4;
    } else if (!reachable.contains(
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
    if (scenario == "layer") {
      const auto &layer=state.terrainDraft.surface.layers[state.terrainBrush.layer];
      // Catalog layers may use absolute cluster transforms or disable height
      // blending. Disabled controls intentionally do not receive routed hits.
      const bool scaleReachable=reachable.contains(base+64);
      const bool heightReachable=reachable.contains(base+76);
      if(scaleReachable==layer.cluster.enabled || heightReachable!=layer.heightBlend ||
         !reachable.contains(base+66)) {
        std::fprintf(stderr,"layer control gate mismatch: cluster=%d heightBlend=%d scale=%d height=%d\n",
          int(layer.cluster.enabled),int(layer.heightBlend),int(scaleReachable),int(heightReachable));
        return 4;
      }
    }
    if (scenario == "maps" && !reachable.contains(base + 96))
      return 4;
  }
  if(scenario=="catalog-search" || scenario=="catalog-empty" || scenario=="map-source" || scenario=="noise-search" || scenario=="global-search") {
    for(u32 action:{0u,2u,3u})if(!reachable.contains(editor::widgetId(editor::terrainCatalogAction(action)))) {
      std::fprintf(stderr,"search action %u unreachable in %s\n",action,std::string(scenario).c_str());return 4;
    }
    if(scenario=="catalog-empty" && reachable.contains(editor::widgetId(editor::EditorWidget::TerrainCatalogItemBase)))return 4;
  }
  if(scenario=="pbr-normal-source" || scenario=="geo-normal-source" || scenario=="global-normal-source" || scenario=="normal-noise-source" || scenario=="variant-normal-source" || scenario=="anti-normal-source" || scenario=="snow-normal-noise-source" || scenario=="snow-source") {
    if(state.terrainCatalogRoleFilter!=2)return 4;
    const u32 sourceAction=scenario=="pbr-normal-source"?editor::widgetId(editor::terrainCatalogAction(8)):
      (scenario=="geo-normal-source" || scenario=="global-normal-source")?editor::widgetId(editor::terrainModuleAction(32)):
      scenario=="variant-normal-source"?editor::widgetId(editor::terrainClusterAction(32)):
      (scenario=="snow-source" || scenario=="snow-normal-noise-source")?editor::widgetId(editor::terrainSnowAction(80)):editor::widgetId(editor::terrainNoiseAction(16));
    if(!reachable.contains(sourceAction)){std::fprintf(stderr,"normal picker first source unreachable in %s\n",std::string(scenario).c_str());return 4;}
  }
  const auto requireSnowField=[&](u32 field){const auto id=editor::terrainSnowNumberBase+field-256;if(!reachable.contains(id)){std::fprintf(stderr,"Snow field %u unreachable in %s\n",field,std::string(scenario).c_str());return false;}return true;};
  if(scenario=="snow-off" || scenario=="snow-simple" || scenario=="snow-rich" || scenario=="snow-plane")for(u32 field:{256u,257u,258u,259u})if(!requireSnowField(field))return 4;
  if(scenario=="snow-height")for(u32 field:{260u,261u,262u,265u,266u,267u,268u})if(!requireSnowField(field))return 4;
  if(scenario=="snow-mapping")for(u32 field:{263u,264u,279u,280u})if(!requireSnowField(field))return 4;
  if(scenario=="snow-noise")for(u32 field:{273u,274u})if(!requireSnowField(field))return 4;
  if(scenario=="snow-distance")for(u32 field:{275u,276u,277u,278u})if(!requireSnowField(field))return 4;
  if(scenario=="snow-layer")for(u32 field:{281u,282u})if(!requireSnowField(field))return 4;
  if(scenario=="snow-sss")for(u32 field:{283u,284u,285u,286u})if(!requireSnowField(field))return 4;
  if(scenario=="snow-rim")for(u32 field:{287u,288u,289u,290u})if(!requireSnowField(field))return 4;
  if(scenario=="snow-sparkle")for(u32 field=291;field<=300;++field)if(!requireSnowField(field))return 4;
  if(scenario=="snow-normal-map" || scenario=="snow-mask"){
    for(u32 field:{304u,307u,308u,311u,312u,315u,316u})if(!requireSnowField(field))return 4;
    for(u32 field:{305u,306u,309u,310u,313u,314u,317u,318u})if(reachable.contains(editor::terrainSnowNumberBase+field-256))return 4;
  }
  if(scenario=="snow-normal-rgb-map" || scenario=="snow-normal-rg-map"){
    for(u32 field:{304u,307u,308u,311u,312u,315u,316u})if(!requireSnowField(field))return 4;
    if(!reachable.contains(editor::widgetId(editor::terrainSnowAction(52))))return 4;
    for(u32 field:{309u,313u,317u})if(scenario=="snow-normal-rgb-map"?!requireSnowField(field):reachable.contains(editor::terrainSnowNumberBase+field-256))return 4;
  }
  if(scenario=="snow-source" && !reachable.contains(editor::widgetId(editor::terrainSnowAction(80))))return 4;
  if(scenario=="quality")for(u32 action:{101u,106u,107u,108u,109u,110u})if(!reachable.contains(editor::widgetId(editor::terrainSnowAction(action))))return 4;
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
  if(scenario=="geo")for(u32 field:{191u,192u,193u,194u,202u})if(!requireField(field))return 4;
  if(scenario=="geo-range")for(u32 field:{195u,196u,197u,198u})if(!requireField(field))return 4;
  if(scenario=="geo-layer")for(u32 field:{203u,204u,205u})if(!requireField(field))return 4;
  if(scenario=="geo-curve" || scenario=="geo-curve-signed" || scenario=="geo-curve-single")for(u32 field:{206u,207u,255u})if(!requireField(field))return 4;
  if(scenario=="geo-tangents" || scenario=="geo-tangents-signed")for(u32 field:{208u,209u,210u,211u,255u})if(!requireField(field))return 4;
  if(scenario=="geo-axis")for(u32 field:{199u,200u,201u})if(!requireField(field))return 4;
  if(scenario=="global-tint" || scenario=="global-normal" || scenario=="global-saom" || scenario=="global-emission")for(u32 field:{212u,213u,214u,215u,216u,217u,218u,219u,220u,221u})if(!requireField(field))return 4;
  if(scenario=="geo-albedo-map" || scenario=="geo-normal-map" || scenario=="global-normal-map")for(u32 field:{222u,225u,226u,229u,230u,233u,234u})if(!requireField(field))return 4;
  if(scenario=="global-saom-maps")for(u32 field:{222u,223u,224u})if(!requireField(field))return 4;
  if(scenario=="geo-curve-empty" && !reachable.contains(editor::widgetId(editor::terrainModuleAction(42)))) return 4;
  if(scenario=="geo-fontes")for(u32 action:{1u,2u,3u,4u,9u,10u,11u})if(!reachable.contains(editor::widgetId(editor::terrainModuleAction(action))))return 4;
  if(scenario.starts_with("fx-")) {
    if(!editor::terrainStaticFlowView(state.terrainMaterialView)){
      if(panel.height<400){if(!reachable.contains(editor::terrainFxActionBase+24) || !reachable.contains(editor::terrainFxActionBase+25))return 4;}
      else if(!reachable.contains(editor::terrainFxActionBase+1) || !reachable.contains(editor::terrainFxActionBase+2) || !reachable.contains(editor::terrainFxActionBase+3))return 4;
    }
    if(scenario=="fx-empty" || scenario=="fx-paint") {
      state.terrainScroll=0;render();const auto content=editor::terrainContentRect(state,panel);bool first=false;
      for(float y=content.y;y<content.bottom();y+=4)for(float x=content.x;x<content.right();x+=4)if(router.hitTest({x,y}).widgetId==editor::terrainSnowSliderBase+64+0x100)first=true;
      if(!first || state.terrainFx.canvas.y<content.y || state.terrainFx.canvas.bottom()>content.bottom())return 4;
    }
  }
  if(editor::terrainStaticFlowView(state.terrainMaterialView)){
    const auto view=state.terrainMaterialView;
    for(u32 field=336;field<=349;++field)if(editor::terrainStaticFlowNumberAvailable(state,field) && !reachable.contains(editor::terrainSnowNumberBase+field-256))return 4;
    if(scenario=="fx-wetness-empty" && reachable.contains(editor::terrainFxActionBase+110))return 4;
    if(view==310 || view==320){state.terrainScroll=0;render();const auto content=editor::terrainContentRect(state,panel);bool first=false;
      const auto slider=editor::terrainSnowSliderBase+(view==310?336:346)-256+0x100;
      for(float y=content.y;y<content.bottom();y+=4)for(float x=content.x;x<content.right();x+=4)if(router.hitTest({x,y}).widgetId==slider)first=true;
      if(!first)return 4;
    }
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
