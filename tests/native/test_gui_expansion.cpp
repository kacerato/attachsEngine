#include "harness.h"
#include "ui/gui_document.h"
#include "ui/gui_images.h"
#include "ui/gui_world.h"
#include "ui/ui_instance_builder.h"
#include "editor/editor_view.h"
#include "renderer/primitive_geometry.h"
#include <sstream>
#include <cmath>
using namespace ae;using namespace ae::ui;
AE_TEST(gui_joystick_modes_curve_return_and_independent_image_sources) {
  GuiDocument d;const auto id=d.create(GuiKind::Joystick);auto n=*d.find(id);std::string error;n.offsets={0,0,200,200};n.control.deadzone=0;n.control.exponent=2;n.control.mode=GuiStickMode::Floating;n.control.baseImage="UI/base.png";n.control.knobImage="UI/knob.png";AE_EXPECT_TRUE(d.update(n,error),"editable floating joystick");
  GuiImageAtlas images;u32 loads=0;images.reconcile(d,1,[&](std::string_view,std::vector<u8>&pixels,u32 &w,u32 &h,std::string &){++loads;w=h=16;pixels.assign(16*16*4,255);return true;});
  AE_EXPECT_TRUE(loads==2&&images.find(n.control.baseImage)&&images.find(n.control.knobImage),"base and knob use actual project image atlas entries");
  GuiRuntime r;r.setImages(&images);r.load(d);r.layout({0,0,800,600});r.pointer({1,UiPointerPhase::Down,{60,60}});
  AE_EXPECT_TRUE(std::abs(r.controlState(id)->value.x)<.001f&&std::abs(r.controlState(id)->origin.x-.3f)<.001f,"floating origin is the initial touch");
  r.pointer({1,UiPointerPhase::Move,{100,60}});AE_EXPECT_TRUE(std::abs(r.controlState(id)->value.x-.25f)<.001f,"authored exponent transforms half displacement");
  UiDrawList list;list.begin({0,0,800,600},{});r.draw(list);u32 draws=0;for(const auto &c:list.commands())if(c.image==kUiGuiImage)++draws;AE_EXPECT_EQ(draws,2u,"both authored image resources reach draw commands");
  r.pointer({1,UiPointerPhase::Up,{100,60}});r.advance(.06);AE_EXPECT_TRUE(r.controlState(id)->value.x==0&&r.controlState(id)->knob.x>0&&r.controlState(id)->knob.x<.5f,"action stops immediately while visual returns separately");r.advance(.2);AE_EXPECT_TRUE(r.controlState(id)->knob.x==0,"return completes");
  n.control.mode=GuiStickMode::Dynamic;n.control.exponent=1;d.update(n,error);r.load(d);r.layout({0,0,800,600});r.pointer({1,UiPointerPhase::Down,{60,60}});r.pointer({1,UiPointerPhase::Move,{300,60}});
  AE_EXPECT_TRUE(std::abs(r.controlState(id)->origin.x-1.1f)<.001f&&r.controlState(id)->value.x>.99f,"dynamic center follows beyond the input radius");
  r.pointer({1,UiPointerPhase::Move,{260,60}});AE_EXPECT_TRUE(std::abs(r.controlState(id)->value.x-.5f)<.001f,"movement is measured from updated origin");
  n.control.mode=GuiStickMode::Fixed;n.control.gate=GuiStickGate::Square;n.control.axis=GuiStickAxis::Horizontal;d.update(n,error);r.load(d);r.layout({0,0,800,600});r.pointer({1,UiPointerPhase::Down,{180,20}});
  AE_EXPECT_TRUE(r.controlState(id)->value.x>.99f&&r.controlState(id)->value.y==0,"fixed center and horizontal restriction affect actual signal");
  n.control.showBase=n.control.showKnob=false;d.update(n,error);r.load(d);r.layout({0,0,800,600});list.begin({0,0,800,600},{});r.draw(list);AE_EXPECT_TRUE(list.commands().empty()&&r.pointer({1,UiPointerPhase::Down,{180,20}}),"fully unpainted joystick remains a functional authored input region");
}
AE_TEST(gui_automatic_nested_layout_reflows_resize_hide_order_and_roundtrip) {
  GuiDocument d;std::string error;const auto root=d.create(GuiKind::VBox),row=d.create(GuiKind::HBox,root),a=d.create(GuiKind::Button,row),b=d.create(GuiKind::Button,row),grid=d.create(GuiKind::Grid,root);
  auto n=*d.find(root);n.anchorMax={1,1};n.offsets={0,0,0,0};n.sizing.padding=UiInsets::all(10);d.update(n,error);
  for(auto id:{row,grid}){n=*d.find(id);n.sizing.preferred={0,100};n.sizing.flexible={0,1};d.update(n,error);}
  for(auto id:{a,b}){n=*d.find(id);n.sizing.minimum={20,20};n.sizing.preferred={80,40};n.sizing.flexible={1,0};d.update(n,error);}
  const auto ga=d.create(GuiKind::Button,grid),gb=d.create(GuiKind::Button,grid),gc=d.create(GuiKind::Button,grid);
  GuiRuntime r;r.load(d);r.layout({0,0,400,300});
  AE_EXPECT_TRUE(r.placement(a)->bounds.right()<r.placement(b)->bounds.x && r.placement(ga)->bounds.x<r.placement(gb)->bounds.x && r.placement(gc)->bounds.y>r.placement(ga)->bounds.y,"nested row/grid have measured non-overlapping cells");
  const float width=r.placement(a)->bounds.width;r.layout({0,0,800,300});AE_EXPECT_TRUE(r.placement(a)->bounds.width>width,"flex shares resized available space");
  r.setVisible(b,false);r.layout({0,0,800,300});AE_EXPECT_TRUE(!r.placement(b)&&r.placement(a)->bounds.width>width*2,"hidden child leaves no gap");
  AE_EXPECT_TRUE(d.reorder(b,-1),"sibling order editable");r.load(d);r.layout({0,0,400,300});AE_EXPECT_TRUE(r.placement(b)->bounds.x<r.placement(a)->bounds.x,"reorder changes actual layout");
  std::ostringstream out;d.write(out);GuiDocument copy;std::istringstream input(out.str());AE_EXPECT_TRUE(copy.read(input,error)&&copy.find(grid)->sizing.columns==2,"AEUI2 preserves nested sizing");
  std::istringstream legacy("AEUI 1 0 1\n");AE_EXPECT_TRUE(copy.read(legacy,error)&&copy.canvas().mode==GuiCanvasMode::Screen,"v1 migrates without fabricated properties");
}
AE_TEST(gui_image_atlas_fit_cover_error_reload_and_resource_lifetime) {
  GuiDocument d;std::string error;const auto id=d.create(GuiKind::Image);auto n=*d.find(id);n.image="UI/banner.png";n.offsets={0,0,100,100};d.update(n,error);
  GuiImageAtlas atlas;u32 loads=0;auto loader=[&](std::string_view,std::vector<u8>&p,u32&w,u32&h,std::string&){++loads;w=100;h=50;p.assign(w*h*4,255);return true;};
  atlas.reconcile(d,1,loader);atlas.reconcile(d,1,loader);AE_EXPECT_EQ(loads,1u,"unchanged sources decode once");
  GuiRuntime r;r.setImages(&atlas);r.load(d);r.layout({0,0,200,200});UiDrawList list;list.begin({0,0,200,200},{});r.draw(list);
  const UiDrawCommand *image=nullptr;for(const auto &c:list.commands())if(c.image==kUiGuiImage)image=&c;
  AE_EXPECT_TRUE(image && image->bounds.height==50 && image->bounds.y==25,"Contain preserves aspect and centers");
  UiFont font;UiIconAtlas icons;std::vector<UiInstance> instances;buildUiInstances(list,font,icons,100,instances);bool emitted=false;for(const auto &instance:instances)if(static_cast<u32>(instance.params[2])==7)emitted=instance.bounds[2]==100&&instance.bounds[3]==50;
  AE_EXPECT_TRUE(emitted,"image rectangle and UV reach the real GPU instance builder");
  n.imageFit=GuiImageFit::Cover;d.update(n,error);r.load(d);r.layout({0,0,200,200});list.begin({0,0,200,200},{});r.draw(list);image=nullptr;for(const auto &c:list.commands())if(c.image==kUiGuiImage)image=&c;
  AE_EXPECT_TRUE(image && image->bounds.width==100 && image->atlas.width==50,"Cover crops source UV without distortion");
  atlas.reconcile(d,2,[](std::string_view,std::vector<u8>&,u32&,u32&,std::string&e){e="missing";return false;});AE_EXPECT_TRUE(atlas.find(n.image)->error=="missing" && atlas.find(n.image)->texels.isEmpty(),"removed file invalidates previous mapping with visible diagnostic");
  n.image="../external.png";AE_EXPECT_TRUE(!d.update(n,error),"path traversal cannot persist");atlas.clear();AE_EXPECT_TRUE(!atlas.find("UI/banner.png")&&atlas.pixels().empty(),"project unload releases resource pixels");
}
AE_TEST(gui_world_perspective_orthographic_ray_inverse_and_draw_contract) {
  GuiCanvas c;c.mode=GuiCanvasMode::World;c.position[1]=0;c.position[2]=5;c.rotation[1]=25;c.resolution={400,200};c.unitsPerPixel=.01f;
  const float origin[3]{};auto camera=renderer::buildPerspectiveFrustum(origin,0,0,1.5f);GuiWorldFrame frame;
  AE_EXPECT_TRUE(frame.configure(c,camera,{20,40,600,400},{0,0,640,480}),"world uses existing camera projection");
  for(const UiPoint local:{UiPoint{50,50},UiPoint{350,150}}){UiPoint screen,recovered;float depth,distance;AE_EXPECT_TRUE(frame.project(local,screen,depth)&&frame.map(screen,recovered,distance)&&std::abs(local.x-recovered.x)<.001f&&std::abs(local.y-recovered.y)<.001f,"rotated perspective plane maps pixel back to same control");}
  GuiDocument d;std::string error;d.setCanvas(c,error);d.create(GuiKind::Button);GuiRuntime r;r.load(d);r.layout({0,0,400,200});UiDrawList list;list.begin({0,0,400,200},{});r.draw(list);list.projectRange(0,frame.projection(),true,{20,40,600,400});
  UiFont font;UiIconAtlas icons;std::vector<UiInstance> instances;buildUiInstances(list,font,icons,100,instances);AE_EXPECT_TRUE(!instances.empty()&&instances.front().colors[3]==3 && instances.front().projection[11]>0 && instances.front().worldClip[1]==40,"real renderer receives homogeneous projection and scene-depth flag");
  c.position[2]=-5;frame.configure(c,camera,{0,0,600,400},{0,0,600,400});UiPoint pixel;float depth;AE_EXPECT_TRUE(!frame.project({200,100},pixel,depth),"behind-camera plane is rejected");
  c.position[2]=5;camera.projection=renderer::CameraProjection::Orthographic;camera.orthographicHalfWidth=3;camera.orthographicHalfHeight=2;frame.configure(c,camera,{0,0,600,400},{0,0,600,400});UiPoint recovered;float distance;AE_EXPECT_TRUE(frame.project({150,80},pixel,depth)&&frame.map(pixel,recovered,distance)&&std::abs(recovered.x-150)<.001f,"orthographic ray origin is honored");
  std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendPrimitiveGeometry(scene::PrimitiveType::Cube,renderer::MapVertexStride,vertices,indices,draws,materials),"use real engine cube geometry for occlusion");
  std::vector<editor::EditorPickMesh::Triangle> triangles(indices.size()/3);
  for(usize t=0;t<triangles.size();++t)for(u32 p=0;p<3;++p)std::memcpy(triangles[t].data()+p*3,vertices.data()+indices[t*3+p]*renderer::MapVertexStride,3*sizeof(float));
  auto mesh=std::make_shared<editor::EditorPickMesh>();AE_EXPECT_TRUE(mesh->build(std::move(triangles)),"actual picking mesh");
  editor::EditorPickCandidate cube;cube.id=1;cube.selectable=true;cube.mesh=mesh;cube.center[0]=cube.center[1]=1;cube.center[2]=-1;cube.radius=draws.front().boundsRadius*1.1f;
  cube.model[0]=1.1f;cube.model[5]=.8f;cube.model[10]=.35f;cube.model[15]=1;cube.model[12]=cube.model[13]=1;cube.model[14]=-1;
  const float eye[3]{0,0,-6};camera=renderer::buildPerspectiveFrustum(eye,0,0,2.35f);c={};c.mode=GuiCanvasMode::World;c.position[1]=0;c.rotation[1]=20;c.unitsPerPixel=.006f;
  editor::EditorViewport view{camera,{}, {0,73,2772,1207}};frame.configure(c,camera,view.rect,{0,0,2772,1280});
  frame.project({600,130},pixel,depth);frame.map(pixel,recovered,distance);const auto blocked=editor::pickNearest(std::span{&cube,1},editor::screenPointToRay(view,pixel));
  AE_EXPECT_TRUE(blocked.hit&&blocked.distance<distance,"cube covers the button before the world plane");
  frame.project({650,470},pixel,depth);const auto free=editor::pickNearest(std::span{&cube,1},editor::screenPointToRay(view,pixel));
  AE_EXPECT_TRUE(!free.hit,"uncovered control remains interactive with the same ray");
}
