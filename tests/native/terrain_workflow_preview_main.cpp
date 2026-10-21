// Production full editor UI and actual terrain selection, software raster only.
#include "editor/editor_session.h"
#include "editor/editor_archive.h"
#include "editor/editor_scene_camera.h"
#include "scene/camera.h"
#include "editor/editor_screen.h"
#include "editor/editor_theme.h"
#include "editor/editor_terrain_fx_controller.h"
#include "editor/editor_terrain_modules_controls.h"
#include "editor/editor_terrain_snow_controls.h"
#include "resources/terrain_surface_catalog.h"
#include "renderer/authoring_geometry.h"
#include "scene/import_link.h"
#include "ui_software_raster.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
using namespace ae;
namespace {
bool check(bool ok,const char *message){if(!ok)std::fprintf(stderr,"FAIL: %s\n",message);return ok;}
std::vector<u8> read(const std::filesystem::path &path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
bool capturePpm(const std::filesystem::path &path,const test::UiSoftwareTarget &target){
 std::ofstream out(path,std::ios::binary);if(!out)return false;out<<"P6\n"<<target.width<<" "<<target.height<<"\n255\n";
 std::vector<u8> rgb(usize(target.width)*target.height*3);
 for(usize p=0;p<usize(target.width)*target.height;++p)for(u32 c=0;c<3;++c)rgb[p*3+c]=u8(std::clamp(target.pixels[p*4+c],0.f,1.f)*255+.5f);
 out.write(reinterpret_cast<const char *>(rgb.data()),std::streamsize(rgb.size()));return bool(out);
}
struct TemporaryProject {
 std::filesystem::path path;bool created=false;
 TemporaryProject(){path=std::filesystem::temp_directory_path()/("astra-terrain-full-ui-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));created=std::filesystem::create_directory(path);}
 ~TemporaryProject(){if(created && path.parent_path()==std::filesystem::temp_directory_path() && path.filename().string().starts_with("astra-terrain-full-ui-")){std::error_code ec;std::filesystem::remove_all(path,ec);}}
};
struct Scenario {const char *name;u32 mode;editor::EditorGizmoMode tool;bool navigation=false,pending=true;};
}
int main(int argc,char **argv){
 // Operates only on a caller-selected isolated QA scene, never the app project.
 if(argc==3 && std::string_view(argv[1])=="--prepare-player-qa") {
  editor::EditorDocument document;
  if(!check(editor::loadEditorDocument(argv[2],0,document),"load isolated exported QA document"))return 8;
  if(!check(!editor::resolveSceneCamera(document).entity,"QA source must not already contain a gameplay camera"))return 8;
  const auto id=document.createEntity(document.root(),runtime::ObjectKind::Camera,"QA Player Camera");
  auto object=*document.find(id);object.transform.position[1]=24;object.transform.position[2]=-36;object.transform.rotationDegrees[0]=33.6900675f;
  auto *camera=static_cast<scene::Camera*>(object.components.add(scene::Camera::descriptor));camera->nearPlane=.1f;camera->farPlane=1000;camera->priority=10;
  if(!check(document.applyEntityValues(id,object),"apply typed Camera and transform") ||
     !check(editor::saveEditorDocument(argv[2],document,0),"save QA through production scene archive"))return 8;
  editor::EditorDocument reopened;
  if(!check(editor::loadEditorDocument(argv[2],0,reopened),"reload serialized QA Camera") ||
     !check(editor::resolveSceneCamera(reopened).entity==id,"Player resolves its actual saved Camera"))return 8;
  std::printf("Player QA camera %u serialized and reopened. Only the selected local fixture changed.\n",id);return 0;
 }
 if(argc<2)return 2;
 const auto output=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(output);
 std::filesystem::current_path(AETHER_REPOSITORY_ROOT);
 const u32 width=argc>2?u32(std::atoi(argv[2])):655,height=argc>3?u32(std::atoi(argv[3])):300;
 if(!((width==655&&height==300)||(width==1280&&height==720)))return 2;
 auto fontBytes=read("assets/astra-visual/ui/astra-ui-font.aeuf"),iconBytes=read("assets/astra-visual/ui/astra-ui-icons.aeui");
 ui::UiFont font;ui::UiIconAtlas icons;
 if(!check(font.load(fontBytes)&&icons.load(iconBytes),"load actual font and icon atlas"))return 3;
 TemporaryProject project;if(!check(project.created,"create isolated temporary project"))return 3;
 editor::EditorSession session;
 std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
 renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials);
 if(!check(session.importMap(draws,materials,false,vertices,indices,0),"initialize real document/import scene"))return 3;
 session.setGeometryPublisher([&](std::span<const u8> v,std::span<const u32> i,std::span<const renderer::MapDrawRecord> d,std::span<const renderer::MapMaterialRecord> m,std::span<const renderer::SharedAuthoringTexture>,editor::EditorSession::PublishedGeometry &out){
  vertices.clear();indices.clear();draws.clear();materials.clear();renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials);
  const u32 vb=u32(vertices.size()/renderer::MapVertexStride),ib=u32(indices.size()),mb=u32(materials.size());vertices.insert(vertices.end(),v.begin(),v.end());indices.insert(indices.end(),i.begin(),i.end());materials.insert(materials.end(),m.begin(),m.end());
  for(auto draw:d){draw.vertexOffset+=vb;draw.firstIndex+=ib;draw.materialIndex+=mb;draw.lodGroupId=u32(draws.size());draws.push_back(draw);}out={draws,materials,vertices,indices};return true;
 });
 session.initialize(&font,&icons);session.setSurface({0,0,float(width),float(height)},{});
 if(argc>4 && std::string_view(argv[4])=="play-no-camera") {
  if(!check(session.setProjectDirectory(project.path.string().c_str()),"open isolated Play UI fixture"))return 7;
  session.advanceClock(0);
  if(!check(session.startPlay(),"start real Play without camera"))return 7;
  std::vector<renderer::MapDrawState> runtimeDraws;
  if(!check(session.extractPlayMap(runtimeDraws)&&!session.sceneCameraPose().entity,"actual headless runtime world"))return 7;
  session.update();test::UiSoftwareTarget image;image.resize(width,height,0,0,0);
  test::rasterizeUi(session.instances(),font,icons,image);
  if(!check(capturePpm(output/("play-no-camera-"+std::to_string(width)+"x"+std::to_string(height)+".ppm"),image),"write actual Play absence UI"))return 7;
  return 0;
 }
 if(!check(session.setProjectDirectory(project.path.string().c_str())&&session.openTerrain(),"open production terrain workspace"))return 3;
 resources::TerrainRecipe recipe;recipe.cells=8;recipe.controlCells=16;recipe.size=32;recipe.height=0;
 recipe.fx.enabled=true;recipe.fx.width=recipe.fx.height=64;recipe.fx.domain={-16,-16,32,32};recipe.fx.seedPixels.resize(64*64*4);
 for(u32 y=0;y<64;++y)for(u32 x=0;x<64;++x){auto at=(y*64+x)*4;recipe.fx.seedPixels[at]=u8(x*255/63);recipe.fx.seedPixels[at+1]=u8(y*255/63);}
 std::string error;if(!check(session.applyTerrain(recipe,error),error.c_str()))return 4;
 auto root=session.screen().terrainTarget;session.setSelection(root);session.update();
 const auto *entity=session.document().find(root);
 if(!check(entity&&scene::importLink(entity->components)&&session.selection()==root,"terrain root is published and really selected"))return 4;
 // Real catalog supplies an editable pending material draft. The published
 // terrain/root/FX are real; this fixture deliberately does not render 3D.
 auto catalog=std::make_shared<resources::TerrainSurfaceCatalog>();auto catalogBytes=read("assets/terrain/library/catalog.json");
 if(!check(resources::loadTerrainSurfaceCatalog(std::string_view(reinterpret_cast<const char *>(catalogBytes.data()),catalogBytes.size()),*catalog,error),error.c_str())||!check(!catalog->materials.empty(),"real material catalog is nonempty"))return 4;
 editor::EditorScreenState state=session.screen();state.document=&session.document();state.selection=root;state.selectionSet={root};state.terrainTarget=root;state.terrainOpen=true;state.terrainTab=2;state.terrainMaterialView=300;state.terrainScroll=0;state.terrainCatalog=catalog;state.terrainDraft=recipe;
 for(usize n=0;n<std::min(usize(4),catalog->materials.size());++n)state.terrainDraft.surface.layers.push_back(catalog->materials[n].layer);
 // Publish these actual catalog materials before capturing an Applied footer.
 // Texture data passes through the real importer/publisher seam; the output
 // remains a software UI raster and makes no 3D rendering claim.
 if(!check(session.applyTerrain(state.terrainDraft,error),error.c_str()))return 4;
 root=session.screen().terrainTarget;session.setSelection(root);session.update();
 entity=session.document().find(root);state.selection=root;state.selectionSet={root};state.terrainTarget=root;
 auto retained=std::make_shared<resources::TerrainBuild>();
 if(!check(resources::updateTerrain(state.terrainDraft,nullptr,*retained,error),error.c_str()))return 4;
 state.terrainPreviewBuild=retained;
 if(!check(editor::TerrainFxEditorController::prepareBitmap(recipe.fx,{},nullptr,state.terrainFx.bitmap,error),error.c_str()))return 4;
 state.terrainFx.recipePending=true;state.terrainFx.brush.radiusMeters=4;state.terrainFx.brush.target=.75;state.terrainFx.cursorU=.5;state.terrainFx.cursorV=.5;state.showComponentVisuals=false;state.showGrid=true;
 editor::EditorViewport view;state.view=&view;
 ui::UiDrawList list;ui::UiInputRouter router;
 const auto render=[&](){
  list.begin(state.surface,font.metrics(ui::UiFontWeight::Regular));router.beginFrame();auto layout=editor::buildEditorScreen(state,editor::editorTheme(),list,router);
  view.rect=layout.viewport;const float camera[3]{0,18,-28};view.frustum=renderer::buildPerspectiveFrustum(camera,0,std::atan2(18.f,28.f),view.rect.width/std::max(1.f,view.rect.height),{});
  list.begin(state.surface,font.metrics(ui::UiFontWeight::Regular));router.beginFrame();return editor::buildEditorScreen(state,editor::editorTheme(),list,router);
 };
 if(argc>4 && std::string_view(argv[4])=="viewport") {
  state.terrainOpen=false;state.terrainFx.cursorVisible=false;state.sceneEffectsMenu=true;
  for(u32 mode=0;mode<4;++mode) {
   state.viewportSettings.quality=static_cast<renderer::EditorViewportQuality>(std::min(mode,2u));
   state.viewportSettings.animatedMaterials=mode==2;state.qualityPanel=mode==3;
   state.sceneEffectsMenu=mode!=3;state.qualityDraft.upscalingFilter=renderer::UpscalingFilter::ArmAsr;
   state.qualityApplied=state.qualityDraft;state.qualityArmAsr=renderer::TemporalUpscalerAvailability::Available;
   state.qualityStats="Editor agora 960×540 · GPU pendente";render();
   std::vector<ui::UiInstance> instances;const auto built=ui::buildUiInstances(list,font,icons,65536,instances);
   if(!check(!built.dropped&&!built.missingGlyphRuns,"viewport UI complete"))return 6;
   test::UiSoftwareTarget image;image.resize(width,height,.098f,.11f,.13f);test::rasterizeUi(instances,font,icons,image);
   if(!check(capturePpm(output/("viewport-"+std::to_string(mode)+"-"+std::to_string(width)+"x"+std::to_string(height)+".ppm"),image),"write production viewport UI"))return 6;
  }
  return 0;
 }
 const bool searchOnly=argc>4 && std::string_view(argv[4])=="search";
 const bool searchRequested=searchOnly || (argc>4 && std::string_view(argv[4])=="all");
 const bool materialOnly=argc>4 && std::string_view(argv[4])=="material";
 const bool materialRequested=materialOnly || (argc>4 && std::string_view(argv[4])=="all");
 const Scenario scenarios[]{
  {"fx-edit",0,editor::EditorGizmoMode::Translate},{"fx-paint",1,editor::EditorGizmoMode::Translate},
  {"fx-transform-move",2,editor::EditorGizmoMode::Translate},{"fx-transform-rotate",2,editor::EditorGizmoMode::Rotate},{"fx-transform-scale",2,editor::EditorGizmoMode::Scale},
  {"fx-navigation",0,editor::EditorGizmoMode::Translate,true},
  {"fx-applied-paint",1,editor::EditorGizmoMode::Translate,false,false},
  {"fx-applied-navigation",0,editor::EditorGizmoMode::Translate,true,false}};
 std::ofstream report(output/("workflow-"+std::to_string(width)+"x"+std::to_string(height)+".tsv"));report<<"scenario\ttool_widgets\tgizmo_hit_pixels\tcanvas_hit_pixels\theader\tinstances\n";
 if(!materialOnly && !searchOnly)for(const auto &scenario:scenarios){
  state.terrainFx.mode=scenario.mode;state.tool=scenario.tool;state.terrainBrushArmed=false;state.terrainFx.recipePending=scenario.pending;state.terrainFx.cursorVisible=scenario.mode==1;state.navigation=scenario.navigation?editor::EditorNavigationMode::Pan:editor::EditorNavigationMode::Orbit;
  auto layout=render();std::set<u32> tools;u32 gizmoPixels=0,canvasPixels=0,cursorSegments=0;bool header=false,canvasDrawn=false,banner=false;
  const auto canvas=state.terrainFx.canvas;
  for(const auto &command:list.commands()){
   if(command.kind==ui::UiPrimitive::Text&&list.textOf(command).find("Molhado")!=std::string_view::npos)header=true;
   if(command.kind==ui::UiPrimitive::Text&&list.textOf(command).find(scenario.mode==1?"Pintar FX · toque":scenario.mode==2?"Transformar · terreno":"Editar FX · terreno")!=std::string_view::npos)banner=true;
   if(command.kind==ui::UiPrimitive::Line&&command.borderWidth==1.7f)++cursorSegments;
   if(command.kind==ui::UiPrimitive::Rect&&command.bounds.x==canvas.x&&command.bounds.y==canvas.y&&command.bounds.width==canvas.width&&command.bounds.height==canvas.height&&!command.clip.isEmpty())canvasDrawn=true;
  }
  for(float y=1;y<float(height);y+=2)for(float x=1;x<float(width);x+=2){const auto hit=router.hitTest({x,y});if(hit.target!=ui::UiPointerTarget::Widget)continue;
   const u32 id=hit.widgetId;if(id==editor::widgetId(editor::EditorWidget::ToolSelect)||id==editor::widgetId(editor::EditorWidget::ToolMove)||id==editor::widgetId(editor::EditorWidget::ToolRotate)||id==editor::widgetId(editor::EditorWidget::ToolScale))tools.insert(id);
   if(id>=editor::widgetId(editor::EditorWidget::GizmoAxisBase)&&id<editor::widgetId(editor::EditorWidget::GizmoAxisBase)+6)++gizmoPixels;
   if(id==editor::terrainFxActionBase+90)++canvasPixels;
  }
  if(!check(!layout.viewport.isEmpty()&&editor::isViewportValid(view),"production viewport rect and frustum valid")||!check(state.document->find(state.selection)==entity&&state.selection==root,"selected terrain root retained")||!check(header&&canvasDrawn&&!canvas.isEmpty(),"FX header and real bitmap canvas drawn"))return 5;
  if(!check(scenario.mode==2?tools.size()==4:tools.empty(),"ToolRail appears only in Transform")||!check(scenario.mode==2?gizmoPixels>0:gizmoPixels==0,"gizmo follows selected root only in Transform")||!check(scenario.mode==1?canvasPixels>0:canvasPixels==0,"canvas paints only in Paint"))return 5;
  if(!check(banner,"viewport banner matches actual FX mode")||!check(scenario.mode!=1||cursorSegments>=48,"Paint projects actual brush footprint over the retained terrain field"))return 5;
  if(!scenario.pending){bool footerPaint=false,footerNavigate=false,geometryPaint=false;
   for(float y=float(height)-96;y<float(height);y+=2)for(float x=editor::terrainPanelRect(state).x;x<float(width);x+=2){const auto hit=router.hitTest({x,y});footerPaint|=hit.widgetId==editor::terrainFxActionBase+2;footerNavigate|=hit.widgetId==editor::widgetId(editor::EditorWidget::TerrainNavigate);geometryPaint|=hit.widgetId==editor::widgetId(editor::EditorWidget::TerrainSculpt);}
   if(!check(footerPaint&&footerNavigate&&!geometryPaint,"actual Applied footer preserves exclusive FX paint/navigation"))return 5;
  }
  std::vector<ui::UiInstance> instances;const auto built=ui::buildUiInstances(list,font,icons,65536,instances);if(!check(!built.dropped&&!built.missingGlyphRuns,"UI instances complete"))return 6;
  test::UiSoftwareTarget image;image.resize(width,height,.098f,.11f,.13f);test::rasterizeUi(instances,font,icons,image);
  if(!check(capturePpm(output/(std::string(scenario.name)+"-"+std::to_string(width)+"x"+std::to_string(height)+".ppm"),image),"write full UI raster"))return 6;
  report<<scenario.name<<'\t'<<tools.size()<<'\t'<<gizmoPixels<<'\t'<<canvasPixels<<'\t'<<header<<'\t'<<built.emitted<<'\n';
 }
 const struct MaterialScenario {const char *name;u32 view,field,column;} materialScenarios[]{
  {"material-stream",330,368,1},{"material-lava",340,390,1},{"material-rain",350,429,0},{"material-spec-height",351,431,0}};
 const auto originalDraft=state.terrainDraft;
 if(materialRequested)for(const auto &scenario:materialScenarios){
  state.terrainDraft=originalDraft;state.terrainMaterialView=scenario.view;state.terrainScroll=0;state.terrainFx.mode=0;state.terrainFx.recipePending=true;state.terrainFx.cursorVisible=false;state.terrainBrushArmed=false;state.navigation=editor::EditorNavigationMode::Orbit;
  auto &flow=state.terrainDraft.surface.flow;
  if(scenario.view==351){state.terrainDraft.fx={};state.terrainFx.bitmap.reset();flow.specularFade=true;}
  else {
   for(u32 y=0;y<64;++y)for(u32 x=0;x<64;++x){const auto at=(y*64+x)*4;state.terrainDraft.fx.seedPixels[at+2]=u8(x*255/63);state.terrainDraft.fx.seedPixels[at+3]=u8(y*255/63);}
   const auto *source=scenario.view==350?editor::terrainRainPreset(*catalog):editor::terrainFlowPreset(*catalog,scenario.view==340,0);
   if(!check(source!=nullptr,"actual original material source present"))return 5;
   auto &map=editor::terrainFlowMap(state.terrainDraft.surface,scenario.view);map.texture=source->identity;map.channel=scenario.view==330?resources::TerrainSurfaceChannel::NormalRG:resources::TerrainSurfaceChannel::RGB;map.linear=true;
   if(scenario.view==330)flow.streams=true;else if(scenario.view==340)flow.lava=true;else{flow.puddles=true;flow.rainDrops=true;flow.rainIntensityScale[0]=.7f;}
   if(!check(editor::TerrainFxEditorController::prepareBitmap(state.terrainDraft.fx,{},nullptr,state.terrainFx.bitmap,error),error.c_str()))return 5;
  }
  auto materialBuild=std::make_shared<resources::TerrainBuild>();
  if(!check(resources::updateTerrain(state.terrainDraft,retained.get(),*materialBuild,error),error.c_str()))return 5;
  state.terrainPreviewBuild=std::move(materialBuild);
  const auto layout=render();bool back=false,first=false,pbr=false,tools=false;
  const auto fieldId=editor::terrainSnowSliderBase+scenario.field-256+scenario.column*0x100;
  for(float y=1;y<float(height);y+=2)for(float x=1;x<float(width);x+=2){const auto hit=router.hitTest({x,y});
   back=back || hit.widgetId==editor::widgetId(editor::terrainModuleAction(61));first=first || hit.widgetId==fieldId;
   pbr=pbr || hit.widgetId==editor::widgetId(editor::EditorWidget::TerrainMaterialLibrary) || hit.widgetId==editor::widgetId(editor::EditorWidget::TerrainLayerBase);
   tools=tools || hit.widgetId==editor::widgetId(editor::EditorWidget::ToolSelect) || hit.widgetId==editor::widgetId(editor::EditorWidget::ToolMove) || hit.widgetId==editor::widgetId(editor::EditorWidget::ToolRotate) || hit.widgetId==editor::widgetId(editor::EditorWidget::ToolScale) || (hit.widgetId>=editor::widgetId(editor::EditorWidget::GizmoAxisBase) && hit.widgetId<editor::widgetId(editor::EditorWidget::GizmoAxisBase)+6);
  }
  if(!check(back&&first&&!pbr&&!tools&&!layout.viewport.isEmpty(),"material context preserves Back and first effectful bar without unrelated PBR/gizmo"))return 5;
  if(!check(state.selection==root&&state.document->find(root)==entity,"material draft keeps actual published terrain selection"))return 5;
  std::vector<ui::UiInstance> instances;const auto built=ui::buildUiInstances(list,font,icons,65536,instances);if(!check(!built.dropped&&!built.missingGlyphRuns,"material UI instances complete"))return 6;
  test::UiSoftwareTarget image;image.resize(width,height,.098f,.11f,.13f);test::rasterizeUi(instances,font,icons,image);
  if(!check(capturePpm(output/(std::string(scenario.name)+"-"+std::to_string(width)+"x"+std::to_string(height)+".ppm"),image),"write pending material full UI raster"))return 6;
  report<<scenario.name<<'\t'<<0<<'\t'<<0<<'\t'<<0<<'\t'<<back<<'\t'<<built.emitted<<'\n';
 }

 if(searchRequested){
  const auto*source=editor::terrainFlowPreset(*catalog,false,0);if(!check(source&&source->id.size()==64,"real Stream source SHA64"))return 5;
  state.terrainTab=2;state.terrainMaterialView=380;state.terrainScroll=0;state.terrainCatalogPage=0;state.terrainCatalogRoleFilter=8;state.terrainCatalogQuery=source->id;state.terrainCatalogPackageFilter.clear();
  state.terrainDraft.surface.flow.streamNormal.texture=source->identity;state.terrainDraft.surface.flow.streamNormal.channel=resources::TerrainSurfaceChannel::NormalRG;state.terrainDraft.surface.flow.streamNormal.linear=true;
  state.terrainFx.mode=0;state.terrainFx.cursorVisible=false;state.editingTerrainCatalogSearch=true;state.platformTextInput=false;state.platformDraft=source->id;
  render();bool shaVisible=false;for(const auto&command:list.commands())if(command.kind==ui::UiPrimitive::Text&&list.textOf(command)==source->id&&!command.clip.isEmpty())shaVisible=true;
  const float modalWidth=std::min(560.f,state.surface.width-16),modalHeight=std::min(320.f,state.surface.height-16);
  const float x=state.surface.x+(state.surface.width-modalWidth)*.5f+8,y=state.surface.y+(state.surface.height-modalHeight)*.5f+8+40,w=modalWidth-16,h=modalHeight-16-40;
  const ui::UiPoint cancel{x+w*3.5f/5,y+h*.9f},apply{x+w*4.5f/5,y+h*.9f};
  if(!check(shaVisible&&router.hitTest(cancel).widgetId==editor::widgetId(editor::EditorWidget::NameCancel)&&router.hitTest(apply).widgetId==editor::widgetId(editor::EditorWidget::NameApply),"source SHA is drawn and actual Cancel/Apply centers hit above Terrain"))return 5;
  std::vector<ui::UiInstance>instances;const auto built=ui::buildUiInstances(list,font,icons,65536,instances);if(!check(!built.dropped&&!built.missingGlyphRuns,"search modal UI instances complete"))return 6;
  test::UiSoftwareTarget image;image.resize(width,height,.098f,.11f,.13f);test::rasterizeUi(instances,font,icons,image);
  if(!check(capturePpm(output/("stream-search-sha-"+std::to_string(width)+"x"+std::to_string(height)+".ppm"),image),"write actual full editor search modal"))return 6;
  report<<"stream-search-sha\t0\t0\t0\t1\t"<<built.emitted<<'\n';
 }
 std::printf("%ux%u: %u full production UI scenarios; real terrain selection; software UI raster only, no 3D render claim\n",width,height,searchOnly?1u:materialOnly?4u:materialRequested?13u:8u);return report?0:7;
}
