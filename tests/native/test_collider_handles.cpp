#include "harness.h"
#include "collider_occlusion_fixture.h"
#include "editor/editor_collider_handles.h"
#include "editor/editor_archive.h"
#include "ui_software_raster.h"
#include <fstream>
#include <sstream>
using namespace ae;
namespace {
using namespace editor;
std::vector<u8> uiBytes(const char *name) {
  std::ifstream in(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui"/name,std::ios::binary);
  return {std::istreambuf_iterator<char>(in),{}};
}
struct HandlesFixture {
  std::vector<u8> fontBytes,iconBytes; // Atlas pixel spans borrow these buffers.
  ui::UiFont font;ui::UiIconAtlas icons;test::ConvexCpuLibrary library;EditorSession session;
  EditorEntityId body=0,part=0,sibling=0;
  bool setup() {
    fontBytes=uiBytes("astra-ui-font.aeuf");iconBytes=uiBytes("astra-ui-icons.aeui");
    if(!font.load(fontBytes)||!icons.load(iconBytes))return false;
    const auto folder=std::filesystem::path(AETHER_REPOSITORY_ROOT)/"build"/("handles-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(folder);
    if(!session.setProjectDirectory(folder.generic_string().c_str())||!library.connect(session))return false;
    session.initialize(&font,&icons);session.setSurface({0,0,1280,720},{});
    auto &g=session.document();body=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Owner");auto value=*g.find(body);
    static_cast<scene::PhysicsBody*>(value.components.add(scene::PhysicsBody::descriptor))->motion=scene::BodyMotion::Static;
    if(!g.applyEntityValues(body,value))return false;
    for(u32 n=0;n<2;++n) {
      const auto id=g.createEntity(body,runtime::ObjectKind::Mesh,n?"Sibling":"Edited");value=*g.find(id);
      value.components.add(scene::Collider::descriptor);
      if(!runtime::configurePrimitive(value,scene::PrimitiveType::Cube,{1,session.mapScene().assetGuid(0),session.mapScene().materialForAsset(0)}))return false;
      while(value.components.remove(scene::PhysicsBody::descriptor)){}
      for(usize i=0;i<value.components.size();)if(&value.components.at(i)->type()==&scene::Collider::descriptor&&value.components.at(i)->instanceId()!=1)value.components.removeInstance(value.components.at(i)->instanceId());else ++i;
      static_cast<scene::Collider*>(value.components.editInstance(1))->owner=body;value.transform.position[0]=n?3.f:0.f;
      if(!g.applyEntityValues(id,value))return false;
      if(n)sibling=id;else part=id;
    }
    const float eye[]{0,0,-10};session.setCameraPose(eye,0,0);session.setSelection(part);session.update();
    return test::tapConvexWidget(session,font,EditorWidget::ComponentFoldBase);
  }
  bool tool(EditorWidget widget){return test::tapConvexWidget(session,font,widget);}
  bool handle(ColliderHandleKind kind,ColliderHandle &out) {
    session.update();return colliderHandleGeometry(session.document(),part,1,kind,session.view(),out);
  }
  bool pointer(u32 id,ui::UiPointerPhase phase,ui::UiPoint point,double time=0) {
    const bool consumed=session.handlePointer({id,phase,point,time});session.update();return consumed;
  }
  const scene::Collider &collider() {return *inspectedCollider(session.document(),part,1);}
};
bool near(float a,float b){return std::abs(a-b)<.002f;}
bool sameTransform(const runtime::Transform &a,const runtime::Transform &b) {
  return std::equal(a.position,a.position+3,b.position)&&std::equal(a.scale,a.scale+3,b.scale)&&
      std::equal(a.rotationDegrees,a.rotationDegrees+3,b.rotationDegrees);
}
std::string componentBytes(const scene::Components &components) {
  std::ostringstream out;for(usize i=0;i<components.size();++i){const auto *c=components.at(i);out<<c->type().id<<' '<<c->instanceId()<<' ';c->write(out);out<<'\n';}return out.str();
}
u32 hitWidget(HandlesFixture &f,ui::UiPoint point) {
  ui::UiInputRouter router;ui::UiDrawList list;list.begin(f.session.screen().surface,f.font.metrics(ui::UiFontWeight::Regular));
  buildEditorScreen(f.session.screen(),ui::defaultTheme(),list,router);return router.hitTest(point).widgetId;
}
}
AE_TEST(collider_handles_geometry_typed_shapes_local_pose_hierarchy_and_rotation) {
  EditorDocument g;const auto parent=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Parent");auto p=*g.find(parent);
  p.transform.scale[0]=2;p.transform.scale[1]=3;p.transform.scale[2]=.7f;p.transform.rotationDegrees[2]=20;g.applyEntityValues(parent,p);
  const auto id=g.createEntity(parent,runtime::ObjectKind::Folder,"Shape");auto e=*g.find(id);
  auto *c=static_cast<scene::Collider*>(e.components.add(scene::Collider::descriptor));c->centerX=.2f;c->rotationY=32;c->rotationZ=17;
  e.transform.rotationDegrees[1]=25;g.applyEntityValues(id,e);
  EditorViewport view;view.rect={20,30,900,600};const float eye[]{0,0,-12};view.frustum=renderer::buildPerspectiveFrustum(eye,.15f,.1f,1.5f);
  for(const auto shape:{scene::ColliderShape::Box,scene::ColliderShape::Sphere,scene::ColliderShape::Capsule,scene::ColliderShape::Cylinder}) {
    c=static_cast<scene::Collider*>(e.components.editInstance(1));c->shape=shape;g.applyEntityValues(id,e);
    ColliderHandle h;AE_EXPECT_TRUE(colliderHandleGeometry(g,id,1,ColliderHandleKind::SizeNegativeX,view,h),"all primitive dimensions use real local-to-world pose");
    auto changed=e;AE_EXPECT_TRUE(applyColliderHandleDelta(changed,h,h.unitsPerProperty*.3f),"negative-side grip expands outward in local units despite nonuniform hierarchy/shear");
    const auto *edited=static_cast<const scene::Collider*>(changed.components.findInstance(1));
    AE_EXPECT_TRUE(near(shape==scene::ColliderShape::Box?edited->halfX:edited->radius,.8f)&&near(edited->centerX,c->centerX)&&sameTransform(changed.transform,e.transform),"symmetric resizing preserves visual TRS and center");
    AE_EXPECT_TRUE(colliderHandleGeometry(g,id,1,ColliderHandleKind::SizePositiveY,view,h),"vertical endpoint");
    if(shape==scene::ColliderShape::Capsule||shape==scene::ColliderShape::Cylinder) {
      AE_EXPECT_TRUE(h.property=="half_height","capsule apex excludes hemisphere radius from authored height");
      changed=e;AE_EXPECT_TRUE(applyColliderHandleDelta(changed,h,h.unitsPerProperty*.2f),"height edits cylinder portion only");
      edited=static_cast<const scene::Collider*>(changed.components.findInstance(1));AE_EXPECT_TRUE(near(edited->halfHeight,c->halfHeight+.2f)&&near(edited->radius,c->radius),"radius remains independent");
    }
    changed=e;AE_EXPECT_TRUE(applyColliderHandleDelta(changed,h,-1e6f)&&changed.components.findInstance(1)->valid(),"clamped dimensions cannot become negative");
  }
  c=static_cast<scene::Collider*>(e.components.editInstance(1));c->shape=scene::ColliderShape::Mesh;c->meshLocalPose=false;g.applyEntityValues(id,e);ColliderHandle h;
  AE_EXPECT_TRUE(!colliderHandleGeometry(g,id,1,ColliderHandleKind::SizePositiveX,view,h)&&!colliderHandleGeometry(g,id,1,ColliderHandleKind::CenterX,view,h),"no fake mesh dimensions or ignored legacy pose");
  c->meshLocalPose=true;g.applyEntityValues(id,e);AE_EXPECT_TRUE(colliderHandleGeometry(g,id,1,ColliderHandleKind::CenterX,view,h),"opt-in mesh local pose reuses genuine property");
  auto changed=e;AE_EXPECT_TRUE(applyColliderHandleDelta(changed,h,h.unitsPerProperty*.4f)&&near(static_cast<const scene::Collider*>(changed.components.findInstance(1))->centerX,.6f),"center uses owner axes, independent of collider rotation");
  for(u32 axis=0;axis<3;++axis) {
    AE_EXPECT_TRUE(colliderHandleGeometry(g,id,1,static_cast<ColliderHandleKind>(9+axis),view,h),"Euler ring frame includes only preceding factors");
    // The authoring Transform currently rejects negative scale. Exercise the
    // ray mathematics on a reflected affine frame without bypassing that contract.
    for(u32 i=0;i<3;++i)h.pose[i]=-h.pose[i];
    AE_EXPECT_TRUE(renderer::buildNormalMatrix(h.pose,h.inverseTranspose),"reflected affine ring inverse");
    const float start=.7f,end=1.1f;float point[3],a,b;colliderRingPoint(h,start,point);auto pixel=projectWorldToScreen(view,point);
    // Some rings are viewed edge-on; skip unobservable geometry rather than amplifying it.
    if(colliderRingAngle(view,h,pixel.screen,a)) {
      colliderRingPoint(h,end,point);pixel=projectWorldToScreen(view,point);
      AE_EXPECT_TRUE(colliderRingAngle(view,h,pixel.screen,b)&&near(std::remainder(b-a,6.2831853f),end-start),"inverse affine ray handles mirrored nonuniform/sheared ring");
    }
  }
  renderer::PerspectiveVisibilitySettings ortho;ortho.projection=renderer::CameraProjection::Orthographic;ortho.orthographicHalfHeight=5;
  view.frustum=renderer::buildPerspectiveFrustum(eye,0,0,1.5f,ortho);
  AE_EXPECT_TRUE(colliderHandleGeometry(g,id,1,ColliderHandleKind::CenterX,view,h),"orthographic pose handle");
  float t;AE_EXPECT_TRUE(cameraHandleRayParameter(view,h,projectWorldToScreen(view,h.point).screen,t)&&near(t,0),"same point solves exactly in orthographic view");
  p.transform.scale[0]=0;g.applyEntityValues(parent,p);AE_EXPECT_TRUE(!colliderHandleGeometry(g,id,1,ColliderHandleKind::CenterX,view,h),"singular hierarchy never exposes an unstable grip");
}
AE_TEST(collider_handles_pointer_archive_single_undo_cancellation_and_jolt) {
  HandlesFixture f;AE_EXPECT_TRUE(f.setup()&&f.tool(EditorWidget::ToolScale),"real editor and shape context");auto &s=f.session;auto &g=s.document();
  s.history().clear();const auto original=serializeEditorDocument(g,0);const auto visual=g.find(f.part)->transform;const auto sibling=componentBytes(g.find(f.sibling)->components);
  ColliderHandle h;AE_EXPECT_TRUE(f.handle(ColliderHandleKind::SizePositiveX,h),"actual dimension grip");const auto pixel=projectWorldToScreen(s.view(),h.point).screen;
  AE_EXPECT_TRUE(hitWidget(f,pixel)==widgetId(EditorWidget::ColliderHandleBase),"drawn grip is the routed target");
  f.pointer(10,ui::UiPointerPhase::Down,pixel);
  for(float delta:{.2f,.4f,.6f}){float point[3];for(u32 i=0;i<3;++i)point[i]=h.point[i]+h.axis[i]*h.unitsPerProperty*delta;f.pointer(10,ui::UiPointerPhase::Move,projectWorldToScreen(s.view(),point).screen,.1);}
  float point[3];for(u32 i=0;i<3;++i)point[i]=h.point[i]+h.axis[i]*h.unitsPerProperty*.6f;
  f.pointer(10,ui::UiPointerPhase::Up,projectWorldToScreen(s.view(),point).screen,.2);
  AE_EXPECT_TRUE(near(f.collider().halfX,1.1f)&&s.history().undoDepth()==1&&sameTransform(g.find(f.part)->transform,visual)&&componentBytes(g.find(f.sibling)->components)==sibling,"captured moving grip edits one UID in one Undo, preserving visual and sibling");
  EditorDocument reopened;AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(g,0),0,reopened),"native archive roundtrip");
  runtime::GameWorld world;runtime::ScenePhysics physics;runtime::QueryHit hit;const float from[]{.9f,3,0},down[]{0,-6,0};
  AE_EXPECT_TRUE(world.load(reopened)&&physics.start(world)&&physics.rayCast(from,down,{},hit)&&hit.object==f.body&&hit.colliderObject==f.part&&hit.colliderInstance==1,"Jolt consumes actual enlarged child Collider with authoring identity");physics.stop();
  AE_EXPECT_TRUE(s.history().undo(g)&&serializeEditorDocument(g,0)==original,"one Undo restores exact complete archive");
  AE_EXPECT_TRUE(world.load(g)&&physics.start(world)&&!physics.rayCast(from,down,{},hit),"restored shape restores physics footprint");physics.stop();
  AE_EXPECT_TRUE(f.tool(EditorWidget::ToolMove)&&f.handle(ColliderHandleKind::CenterX,h),"local center tool");
  auto start=projectWorldToScreen(s.view(),h.point).screen;f.pointer(11,ui::UiPointerPhase::Down,start);
  for(u32 i=0;i<3;++i)point[i]=h.point[i]+h.axis[i]*h.unitsPerProperty*.4f;
  f.pointer(11,ui::UiPointerPhase::Move,projectWorldToScreen(s.view(),point).screen,.1);
  AE_EXPECT_TRUE(near(f.collider().centerX,.4f)&&sameTransform(g.find(f.part)->transform,visual),"center moves only physical local pose");
  f.pointer(99,ui::UiPointerPhase::Down,start);f.pointer(99,ui::UiPointerPhase::Up,start,.15);
  AE_EXPECT_TRUE(s.history().isOpen()&&s.selection()==f.part,"another finger cannot steal the gesture");
  f.pointer(11,ui::UiPointerPhase::Cancel,start,.2);
  AE_EXPECT_TRUE(serializeEditorDocument(g,0)==original&&s.history().undoDepth()==0&&s.history().redoDepth()==1,"cancel restores archive without consuming redo");
  f.pointer(12,ui::UiPointerPhase::Down,start);f.pointer(12,ui::UiPointerPhase::Move,projectWorldToScreen(s.view(),point).screen,.1);s.cancelPointers();s.update();
  AE_EXPECT_TRUE(serializeEditorDocument(g,0)==original&&!s.history().isOpen()&&s.screen().activeColliderHandle==0,"lifecycle cancellation releases capture and edit");
  AE_EXPECT_TRUE(f.tool(EditorWidget::ToolRotate)&&f.handle(ColliderHandleKind::RotationZ,h),"actual rotation ring");
  colliderRingPoint(h,2.95f,point);start=projectWorldToScreen(s.view(),point).screen;
  f.pointer(13,ui::UiPointerPhase::Down,start);
  for(float angle:{3.2f,3.4f}){colliderRingPoint(h,angle,point);f.pointer(13,ui::UiPointerPhase::Move,projectWorldToScreen(s.view(),point).screen,.1);}
  f.pointer(13,ui::UiPointerPhase::Up,projectWorldToScreen(s.view(),point).screen,.2);
  AE_EXPECT_TRUE(near(f.collider().rotationZ,.45f*57.2957795f)&&s.history().undoDepth()==1&&sameTransform(g.find(f.part)->transform,visual),"ring crosses atan2 seam without jump or rotating mesh");
  const float rotatedFrom[]{.6f,3,0};
  AE_EXPECT_TRUE(world.load(g)&&physics.start(world)&&physics.rayCast(rotatedFrom,down,{},hit)&&hit.colliderObject==f.part&&hit.colliderInstance==1,"Jolt footprint rotates independently of visual mesh");physics.stop();
  AE_EXPECT_TRUE(s.history().undo(g)&&serializeEditorDocument(g,0)==original,"rotation single Undo exact archive");
  AE_EXPECT_TRUE(f.tool(EditorWidget::ToolMove)&&f.handle(ColliderHandleKind::CenterX,h),"commit local center");
  start=projectWorldToScreen(s.view(),h.point).screen;f.pointer(14,ui::UiPointerPhase::Down,start);
  for(u32 i=0;i<3;++i)point[i]=h.point[i]+h.axis[i]*h.unitsPerProperty*2.f;
  f.pointer(14,ui::UiPointerPhase::Move,projectWorldToScreen(s.view(),point).screen,.1);f.pointer(14,ui::UiPointerPhase::Up,projectWorldToScreen(s.view(),point).screen,.2);
  AE_EXPECT_TRUE(near(f.collider().centerX,2)&&sameTransform(g.find(f.part)->transform,visual),"committed local pose without moving visual");
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(g,0),0,reopened)&&world.load(reopened)&&physics.start(world),"pose roundtrip into real solver");
  const float oldCenter[]{0,3,0},newCenter[]{2,3,0};
  AE_EXPECT_TRUE(!physics.rayCast(oldCenter,down,{},hit)&&physics.rayCast(newCenter,down,{},hit)&&hit.object==f.body&&hit.colliderObject==f.part,"Jolt consumes shifted physical center and stable owner");physics.stop();
  AE_EXPECT_TRUE(s.history().undo(g)&&serializeEditorDocument(g,0)==original,"center single Undo restores archive");
}
AE_TEST(collider_handles_occlusion_visibility_modes_and_compact_touch_targets) {
  HandlesFixture f;AE_EXPECT_TRUE(f.setup()&&f.tool(EditorWidget::ToolScale),"real focused UI");auto &s=f.session;
  const auto wall=test::addOcclusionWall(s,f.part);s.update();ColliderHandle h;AE_EXPECT_TRUE(f.handle(ColliderHandleKind::SizePositiveX,h),"potential grip");
  auto pixel=projectWorldToScreen(s.view(),h.point).screen;
  AE_EXPECT_TRUE(hitWidget(f,pixel)!=widgetId(EditorWidget::ColliderHandleBase),"occluded grip never intercepts foreground touch");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,f.font,static_cast<EditorWidget>(hierarchyEyeWidget(wall))),"hide foreground through actual Hierarchy");
  AE_EXPECT_TRUE(f.handle(ColliderHandleKind::SizePositiveX,h)&&hitWidget(f,projectWorldToScreen(s.view(),h.point).screen)==widgetId(EditorWidget::ColliderHandleBase),"eye releases handle without disabling physics");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,f.font,EditorWidget::ComponentVisualsToggle),"disable authoring overlays");
  s.update();AE_EXPECT_TRUE(hitWidget(f,projectWorldToScreen(s.view(),h.point).screen)!=widgetId(EditorWidget::ColliderHandleBase),"OFF leaves ordinary object tools, no invisible grips");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,f.font,EditorWidget::ComponentVisualsToggle)&&f.tool(EditorWidget::ToolSelect),"restore overlay/select mode");
  AE_EXPECT_TRUE(hitWidget(f,projectWorldToScreen(s.view(),h.point).screen)!=widgetId(EditorWidget::ColliderHandleBase),"Select remains shape selection, not accidental resize");
  AE_EXPECT_TRUE(f.tool(EditorWidget::ToolScale),"explicit scale edits dimensions");
  s.setSurface({0,0,800,400},{});s.update();test::UiSoftwareTarget image;image.resize(800,400,.06f,.07f,.09f);
  const auto &im=s.immediateGui();
  test::rasterizeUi(s.instances(),f.font,f.icons,image,im.atlas(),im.atlasWidth(),im.atlasHeight());
  std::ofstream capture(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"build/collider-handles-compact.ppm",std::ios::binary);capture<<"P6\n800 400\n255\n";
  for(usize i=0;i<image.pixels.size();i+=4)for(u32 c=0;c<3;++c)capture.put(static_cast<char>(std::clamp(image.pixels[i+c],0.f,1.f)*255+.5f));
  AE_EXPECT_TRUE(capture.good(),"capture actual compact native layout");
  ui::UiInputRouter router;ui::UiDrawList list;list.begin(s.screen().surface,f.font.metrics(ui::UiFontWeight::Regular));
  buildEditorScreen(s.screen(),ui::defaultTheme(),list,router);
  std::array<ui::UiRect,4> bounds{};std::array<bool,4> found{};
  const auto viewport=s.view().rect;
  for(float y=viewport.y;y<viewport.bottom();y+=1)for(float x=viewport.x;x<viewport.right();x+=1) {
    const auto id=router.hitTest({x,y}).widgetId;
    if(id<widgetId(EditorWidget::ColliderHandleBase)||id>=widgetId(EditorWidget::ColliderHandleBase)+4)continue;
    const auto n=id-widgetId(EditorWidget::ColliderHandleBase);
    if(!found[n]){bounds[n]={x,y,1,1};found[n]=true;}
    else {auto &b=bounds[n];b.width=std::max(b.width,x-b.x+1);b.height=std::max(b.height,y-b.y+1);}
  }
  for(u32 n=0;n<4;++n)AE_EXPECT_TRUE(found[n]&&bounds[n].width>=31&&bounds[n].height>=31,
      "each visible compact X/Y grip keeps a distinct generous touch target");
  pixel={bounds[0].x+bounds[0].width/2,bounds[0].y+bounds[0].height/2};
  const auto original=serializeEditorDocument(s.document(),0);const auto old=f.collider().halfX;
  f.pointer(44,ui::UiPointerPhase::Down,pixel);pixel.x+=24;
  f.pointer(44,ui::UiPointerPhase::Move,pixel,.1);f.pointer(44,ui::UiPointerPhase::Up,pixel,.2);
  AE_EXPECT_TRUE(f.collider().halfX>old&&near(f.collider().halfY,.5f),"displaced compact grip edits its own physical axis");
  AE_EXPECT_TRUE(s.history().undo(s.document())&&serializeEditorDocument(s.document(),0)==original,
      "compact gesture remains one reversible transaction");
}
