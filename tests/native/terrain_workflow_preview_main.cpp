// Production full editor UI and actual terrain selection, software raster only.
#include "editor/editor_session.h"
#include "editor/editor_screen.h"
#include "editor/editor_theme.h"
#include "editor/editor_terrain_fx_controller.h"
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
 const Scenario scenarios[]{
  {"fx-edit",0,editor::EditorGizmoMode::Translate},{"fx-paint",1,editor::EditorGizmoMode::Translate},
  {"fx-transform-move",2,editor::EditorGizmoMode::Translate},{"fx-transform-rotate",2,editor::EditorGizmoMode::Rotate},{"fx-transform-scale",2,editor::EditorGizmoMode::Scale},
  {"fx-navigation",0,editor::EditorGizmoMode::Translate,true},
  {"fx-applied-paint",1,editor::EditorGizmoMode::Translate,false,false},
  {"fx-applied-navigation",0,editor::EditorGizmoMode::Translate,true,false}};
 std::ofstream report(output/("workflow-"+std::to_string(width)+"x"+std::to_string(height)+".tsv"));report<<"scenario\ttool_widgets\tgizmo_hit_pixels\tcanvas_hit_pixels\theader\tinstances\n";
 for(const auto &scenario:scenarios){
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
 std::printf("%ux%u: eight full production UI scenarios; real terrain selection/material publication; software UI raster only, no 3D render claim\n",width,height);return report?0:7;
}
