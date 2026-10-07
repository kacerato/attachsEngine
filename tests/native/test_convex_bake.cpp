#include "harness.h"
#include "convex_bake_fixture.h"
#include "scene/dynamic_body_motor.h"
#include "editor/editor_component_visuals.h"
#include "editor/editor_archive.h"
#include <fstream>
using namespace ae;
AE_TEST(collider_owner_pointer_identity_history_archive_and_solver) {
  const auto read=[](const char *name){std::ifstream in(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui"/name,std::ios::binary);return std::vector<u8>(std::istreambuf_iterator<char>(in),{});};
  ui::UiFont font;ui::UiIconAtlas icons;
  AE_EXPECT_TRUE(font.load(read("astra-ui-font.aeuf"))&&icons.load(read("astra-ui-icons.aeui")),"actual UI assets");
  editor::EditorSession s;s.initialize(&font,&icons);s.setSurface({0,0,1280,720},{});auto &g=s.document();
  const auto bodyId=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Shared body");auto body=*g.find(bodyId);
  auto *physical=static_cast<scene::PhysicsBody*>(body.components.add(scene::PhysicsBody::descriptor));physical->motion=scene::BodyMotion::Static;
  const auto bodyInstance=physical->instanceId();g.applyEntityValues(bodyId,body);
  const auto branch=g.createEntity(bodyId,runtime::ObjectKind::Folder,"Branch");
  const auto a=g.createEntity(branch,runtime::ObjectKind::Folder,"Left"),b=g.createEntity(bodyId,runtime::ObjectKind::Folder,"Right");
  for(auto id:{a,b}) {auto object=*g.find(id);object.transform.position[0]=id==a?-1.5f:1.5f;
    auto *c=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));c->owner=bodyId;
    AE_EXPECT_TRUE(c->instanceId()==1&&g.applyEntityValues(id,object),"local UID repeats in different objects");}
  s.setSelection(bodyId);s.frameAll();const float eye[]{0,2,-8};s.setCameraPose(eye,0,.24f);s.update();
  AE_EXPECT_TRUE(test::tapConvexWidget(s,font,editor::EditorWidget::ToolSelect)&&test::tapConvexWidget(s,font,editor::EditorWidget::ComponentFoldBase),"enter through Body Inspector, without a root collider");
  AE_EXPECT_EQ(editor::colliderInspectionBody(g,bodyId,bodyInstance),bodyId,"Body starts linked-shape context");
  const auto original=editor::serializeEditorDocument(g,0);const auto undo=s.history().undoDepth();
  for(auto id:{a,b}) {float matrix[16];editor::editorWorldMatrix(g,id,matrix);const auto point=editor::projectWorldToScreen(s.view(),matrix+12);
    const auto hit=editor::pickInspectedCollider(g,s.selection(),s.screen().expandedNative,s.view(),nullptr,point.screen);
    AE_EXPECT_TRUE(hit.entity==id&&hit.instance==1&&hit.surface,"full object/component identity, across nested branch and siblings");
    s.handlePointer({94,ui::UiPointerPhase::Down,point.screen,0});s.handlePointer({94,ui::UiPointerPhase::Up,point.screen,.02});s.update();
    AE_EXPECT_TRUE(s.selection()==id&&s.screen().componentSelection==id&&s.screen().expandedNative==1,"actual pointer opens the component on its authoring object");
  }
  AE_EXPECT_TRUE(editor::serializeEditorDocument(g,0)==original&&s.history().undoDepth()==undo,"navigation never changes owner, transforms or archive/history");
  const auto visuals=editor::collectComponentVisuals(g,b,2,nullptr,0,0,0,1);u32 shapes=0,focused=0;
  for(const auto &v:visuals)if(v.icon==ui::UiIcon::ComponentCollider){++shapes;for(const auto &line:v.segments)if(line.emphasis)++focused;}
  AE_EXPECT_TRUE(shapes==2&&focused==12,"sibling shapes stay visible; only the selected object/UID is emphasized");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,font,editor::EditorWidget::ComponentEnableBase),"disable actual selected child from Inspector");
  AE_EXPECT_TRUE(!static_cast<const scene::Collider*>(g.find(b)->components.findInstance(1))->enabled&&static_cast<const scene::Collider*>(g.find(a)->components.findInstance(1))->enabled,"repeated local UID does not disable sibling");
  editor::EditorDocument reopened;AE_EXPECT_TRUE(editor::deserializeEditorDocument(editor::serializeEditorDocument(g,0),0,reopened),"native archive preserves child-owned shape edit");
  runtime::GameWorld world;runtime::ScenePhysics physics;runtime::QueryHit query;const float from[]{1.5f,3,0},delta[]{0,-6,0},left[]{-1.5f,3,0};
  AE_EXPECT_TRUE(world.load(reopened)&&physics.start(world)&&!physics.rayCast(from,delta,{},query)&&physics.rayCast(left,delta,{},query)&&query.object==bodyId,"actual Jolt compound removes only the selected child shape");physics.stop();
  AE_EXPECT_TRUE(s.history().undo(g)&&editor::serializeEditorDocument(g,0)==original,"undo restores exact authored compound");
  AE_EXPECT_TRUE(world.load(g)&&physics.start(world)&&physics.rayCast(from,delta,{},query)&&query.object==bodyId&&query.colliderInstance==1,"restored child collider participates in shared physical body");
}
AE_TEST(collider_owner_scope_barriers_visibility_and_rebind) {
  editor::EditorDocument g;const auto root=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Body");auto object=*g.find(root);
  object.components.add(scene::PhysicsBody::descriptor);g.applyEntityValues(root,object);
  const auto child=g.createEntity(root,runtime::ObjectKind::Folder,"Child");object=*g.find(child);
  auto *c=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));c->owner=root;g.applyEntityValues(child,object);
  const auto nested=g.createEntity(root,runtime::ObjectKind::Folder,"Independent");object=*g.find(nested);object.transform.position[2]=-2;
  object.components.add(scene::PhysicsBody::descriptor);c=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));const auto nestedCollider=c->instanceId();g.applyEntityValues(nested,object);
  const auto blocked=g.createEntity(nested,runtime::ObjectKind::Folder,"Invalid ancestor reference");object=*g.find(blocked);
  c=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));c->owner=root;g.applyEntityValues(blocked,object);
  const float eye[]{0,0,-10};editor::EditorViewport view;view.rect={20,30,800,400};view.frustum=renderer::buildPerspectiveFrustum(eye,0,0,2);
  const float zero[3]{};const auto point=editor::projectWorldToScreen(view,zero).screen;
  auto hit=editor::pickInspectedCollider(g,root,1,view,nullptr,point);
  AE_EXPECT_TRUE(hit.entity==child&&hit.instance==1,"nearest foreign body and invalid crossing never steal shared-body shape selection");
  const auto visuals=editor::collectComponentVisuals(g,root,2,nullptr,0,0,0,1);u32 count=0;
  for(const auto &v:visuals)if(v.icon==ui::UiIcon::ComponentCollider){++count;AE_EXPECT_EQ(v.entity,child,"only physically linked shapes enter overlay");}
  AE_EXPECT_EQ(count,1u,"subtree is not blanket collider ownership");
  const editor::EditorEntityId pickOff[]{child};
  AE_EXPECT_TRUE(!editor::pickInspectedCollider(g,root,1,view,nullptr,point,0,0,{},pickOff).entity,"pick locks apply to each linked authoring object");
  object=*g.find(child);object.layer=7;g.applyEntityValues(child,object);
  AE_EXPECT_TRUE(!editor::pickInspectedCollider(g,root,1,view,nullptr,point,1u<<7).entity&&!editor::pickInspectedCollider(g,root,1,view,nullptr,point,0,1u<<7).entity,"hidden and unpickable layer filtering across body descendants");
  object=*g.find(child);c=static_cast<scene::Collider*>(object.components.editInstance(1));c->owner=0;g.applyEntityValues(child,object);
  AE_EXPECT_TRUE(!editor::colliderInspectionBody(g,child,1)&&!editor::pickInspectedCollider(g,root,1,view,nullptr,point).entity,"owner zero means self, not implicit ancestor inheritance");
  AE_EXPECT_EQ(editor::colliderInspectionBody(g,nested,nestedCollider),nested,"independent body's collider retains its own context");
  AE_EXPECT_EQ(editor::pickInspectedCollider(g,child,1,view,nullptr,point).entity,child,"unbound draft remains editable locally without fabricating a body");
}
AE_TEST(collider_surface_primitives_depth_affine_clipping_and_identity) {
  const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  editor::EditorRay ray;ray.valid=true;ray.origin[2]=-5;ray.direction[2]=1;ray.maximumDistance=20;
  scene::Collider shape;shape.halfX=shape.halfY=shape.halfZ=shape.radius=shape.halfHeight=1;
  float depth;
  for(auto kind:{scene::ColliderShape::Box,scene::ColliderShape::Sphere,scene::ColliderShape::Capsule,scene::ColliderShape::Cylinder}) {
    shape.shape=kind;AE_EXPECT_TRUE(editor::intersectColliderPrimitive(shape,ray,identity,depth)&&std::abs(depth-4)<.0001f,"actual side surface for each primitive");
  }
  ray.origin[2]=0;ray.origin[1]=5;ray.direction[2]=0;ray.direction[1]=-1;
  shape.shape=scene::ColliderShape::Capsule;
  AE_EXPECT_TRUE(editor::intersectColliderPrimitive(shape,ray,identity,depth)&&std::abs(depth-3)<.0001f,"capsule hemispherical end");
  shape.shape=scene::ColliderShape::Cylinder;
  AE_EXPECT_TRUE(editor::intersectColliderPrimitive(shape,ray,identity,depth)&&std::abs(depth-4)<.0001f,"cylinder flat cap, not a capsule approximation");
  shape.shape=scene::ColliderShape::Capsule;ray.origin[1]=0;ray.direction[1]=1;
  AE_EXPECT_TRUE(editor::intersectColliderPrimitive(shape,ray,identity,depth)&&std::abs(depth-2)<.0001f,"inside capsule exits exposed hemisphere without selecting internal seam");
  shape.shape=scene::ColliderShape::Sphere;ray.origin[1]=0;ray.origin[2]=-5;ray.direction[1]=0;ray.direction[2]=1;ray.minimumDistance=4.5f;
  AE_EXPECT_TRUE(editor::intersectColliderPrimitive(shape,ray,identity,depth)&&std::abs(depth-6)<.0001f,"near plane clips entry and retains visible exit");
  ray.maximumDistance=5.5f;AE_EXPECT_TRUE(!editor::intersectColliderPrimitive(shape,ray,identity,depth),"no surface within clipped interval");
  ray.minimumDistance=0;ray.maximumDistance=20;
  float shear[]{2,0,0,0,.5f,1,0,0,0,0,.8f,0,0,0,0,1};ray.origin[2]=-4;
  AE_EXPECT_TRUE(editor::intersectColliderPrimitive(shape,ray,shear,depth)&&std::abs(depth-3.2f)<.0001f,"affine ray parameter remains world distance");
  ray.origin[0]=1.9f;ray.origin[1]=.9f;
  AE_EXPECT_TRUE(!editor::intersectColliderPrimitive(shape,ray,shear,depth),"ellipsoid/shear corner is not a bounds hit");
  shear[0]=0;AE_EXPECT_TRUE(!editor::intersectColliderPrimitive(shape,ray,shear,depth),"singular transform rejected");
  editor::EditorPickMesh triangles;
  AE_EXPECT_TRUE(triangles.build({{{-2,-2,0,2,-2,0,0,2,0}},{{-2,-2,2,2,-2,2,0,2,2}}}),"two actual mesh surfaces");
  const float origin[]{0,0,-5},direction[]{0,0,1};
  AE_EXPECT_TRUE(triangles.intersect(origin,direction,identity,depth,5.5f,7)&&depth==7,"BVH skips near-clipped face and includes exact far plane");
  AE_EXPECT_TRUE(!triangles.intersect(origin,direction,identity,depth,5.5f,6.5f),"BVH rejects every face outside interval");
  editor::EditorDocument g;const auto id=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Depth compound");auto object=*g.find(id);
  auto *rear=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));rear->centerZ=2;const auto rearId=rear->instanceId();
  auto *front=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));const auto frontId=front->instanceId();front->enabled=false;
  AE_EXPECT_TRUE(g.applyEntityValues(id,object),"real independently editable surfaces");
  const float eye[]{0,0,-10};editor::EditorViewport view;view.frustum=renderer::buildPerspectiveFrustum(eye,0,0,2);view.rect={30,40,800,400};
  const float center[3]{};const auto point=editor::projectWorldToScreen(view,center).screen;
  AE_EXPECT_TRUE(!editor::pickColliderContour(g,id,view,nullptr,point)&&editor::pickColliderSurface(g,id,view,nullptr,point)==frontId,"interior tap picks nearest surface, including disabled editable part");
  front=static_cast<scene::Collider*>(object.components.editInstance(frontId));front->centerZ=2;
  AE_EXPECT_TRUE(object.components.moveInstance(frontId,0)&&g.applyEntityValues(id,object)&&editor::pickColliderSurface(g,id,view,nullptr,point)==rearId,"coincident surface tie uses stable identity after reorder");
  AE_EXPECT_TRUE(!editor::pickColliderSurface(g,id,view,nullptr,{-1,-1}),"outside viewport is never selected");
}
AE_TEST(collider_contour_identity_projection_and_empty_space) {
  editor::EditorDocument g;auto parent=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Scaled parent");
  auto p=*g.find(parent);p.transform.scale[0]=1.4f;p.transform.scale[1]=.8f;p.transform.rotationDegrees[1]=23;
  AE_EXPECT_TRUE(g.applyEntityValues(parent,p),"real affine hierarchy");
  auto id=g.createEntity(parent,runtime::ObjectKind::Folder,"Compound");auto object=*g.find(id);
  auto *a=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));a->centerX=-1.5f;
  const auto first=a->instanceId();auto *b=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));b->centerX=1.5f;
  const auto second=b->instanceId();AE_EXPECT_TRUE(g.applyEntityValues(id,object),"independent colliders");
  const float eye[]{0,0,-10};editor::EditorViewport view;view.frustum=renderer::buildPerspectiveFrustum(eye,0,0,2);view.rect={30,40,800,400};
  const auto visuals=editor::collectComponentVisuals(g,id,2);
  for(const auto &v:visuals)if(v.instance==first||v.instance==second) {
    const auto &line=v.segments.front();ui::UiPoint x,y;
    AE_EXPECT_TRUE(editor::projectSegmentToScreen(view,line.a,line.b,x,y),"same clipped projection as drawing");
    AE_EXPECT_EQ(editor::pickColliderContour(g,id,view,nullptr,{(x.x+y.x)*.5f,(x.y+y.y)*.5f}),v.instance,"contour returns persistent component identity under nonuniform hierarchy");
  }
  const float zero[3]{};auto gap=editor::projectWorldToScreen(view,zero);
  AE_EXPECT_TRUE(!editor::pickColliderContour(g,id,view,nullptr,gap.screen)&&!editor::pickColliderContour(g,id,view,nullptr,{-1,-1}),"empty space and outside viewport have no proxy hit");
  object=*g.find(id);static_cast<scene::Collider*>(object.components.editInstance(first))->centerX=1.5f;
  AE_EXPECT_TRUE(object.components.moveInstance(second,0)&&g.applyEntityValues(id,object),"coincident colliders reordered without changing identity");
  for(const auto &v:visuals)if(v.instance==second) {
    ui::UiPoint x,y;const auto &line=v.segments.front();editor::projectSegmentToScreen(view,line.a,line.b,x,y);
    AE_EXPECT_EQ(editor::pickColliderContour(g,id,view,nullptr,{(x.x+y.x)*.5f,(x.y+y.y)*.5f}),first,"coincident contour tie uses identity, not collection order");
  }
  p=*g.find(parent);p.transform.position[2]=-20;g.applyEntityValues(parent,p);
  AE_EXPECT_TRUE(!editor::pickColliderContour(g,id,view,nullptr,gap.screen),"behind-camera contours cannot be selected");
  // A preserved unavailable record may carry a known type ID. Its descriptor
  // is not that class: never cast or execute it merely because the name matches.
  p.transform.position[2]=0;g.applyEntityValues(parent,p);
  std::ostringstream record;record<<1<<' '<<std::quoted(std::string(scene::Collider::descriptor.id))<<" 99 "<<std::quoted("unavailable payload");
  std::istringstream input(record.str());scene::Components unavailable;
  AE_EXPECT_TRUE(unavailable.read(input,{},scene::UnknownComponentPolicy::Preserve),"real parser preserves unavailable record");
  object=*g.find(id);object.components=std::move(unavailable);
  AE_EXPECT_TRUE(g.applyEntityValues(id,object)&&g.find(id)->components.at(0)->unresolved()&&!editor::pickColliderContour(g,id,view,nullptr,gap.screen),"unavailable Collider name never becomes an executable contour");
}
AE_TEST(collider_contour_pointer_inspector_history_archive_and_solver) {
  const auto read=[](const char *name){std::ifstream in(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui"/name,std::ios::binary);return std::vector<u8>(std::istreambuf_iterator<char>(in),{});};
  ui::UiFont font;ui::UiIconAtlas icons;
  AE_EXPECT_TRUE(font.load(read("astra-ui-font.aeuf"))&&icons.load(read("astra-ui-icons.aeui")),"actual editor UI assets");
  editor::EditorSession s;s.initialize(&font,&icons);s.setSurface({0,0,1280,720},{});
  auto &g=s.document();auto id=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Compound");auto object=*g.find(id);
  auto *body=static_cast<scene::PhysicsBody*>(object.components.add(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Static;
  auto *a=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));a->centerX=-1.5f;const auto first=a->instanceId();
  auto *b=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));b->centerX=1.5f;const auto second=b->instanceId();
  AE_EXPECT_TRUE(g.applyEntityValues(id,object),"authored compound");s.setSelection(id);s.frameAll();s.update();
  AE_EXPECT_TRUE(test::tapConvexWidget(s,font,editor::EditorWidget::ToolSelect)&&test::tapConvexWidget(s,font,static_cast<editor::EditorWidget>(editor::widgetId(editor::EditorWidget::ComponentFoldBase)+1)),"open first collider through actual Inspector");
  AE_EXPECT_EQ(s.screen().expandedNative,first,"initial focus");
  const auto original=editor::serializeEditorDocument(g,0);const auto depth=s.history().undoDepth();
  ui::UiInputRouter router;ui::UiDrawList list;list.begin(s.screen().surface,font.metrics(ui::UiFontWeight::Regular));editor::buildEditorScreen(s.screen(),ui::defaultTheme(),list,router);
  ui::UiPoint tap{};bool found=false;
  const auto visuals=editor::collectComponentVisuals(g,id,1,&s.mapScene());
  for(const auto &v:visuals)if(v.instance==second)for(const auto &line:v.segments) {
    ui::UiPoint x,y;if(!editor::projectSegmentToScreen(s.view(),line.a,line.b,x,y))continue;
    ui::UiPoint at{(x.x+y.x)*.5f,(x.y+y.y)*.5f};
    if(router.hitTest(at).target==ui::UiPointerTarget::Viewport && editor::pickColliderContour(g,id,s.view(),&s.mapScene(),at)==second){tap=at;found=true;break;}
  }
  AE_EXPECT_TRUE(found,"second part contour is reachable without a gizmo or UI intercept");
  s.handlePointer({91,ui::UiPointerPhase::Down,tap,0});s.handlePointer({91,ui::UiPointerPhase::Up,tap,.02});s.update();
  AE_EXPECT_TRUE(s.selection()==id&&s.screen().expandedNative==second&&s.history().undoDepth()==depth&&editor::serializeEditorDocument(g,0)==original,"touch changes only component focus, without moving meshes or authoring state");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,font,editor::EditorWidget::InspectorComponentPrevious)&&s.screen().expandedNative==first,"actual Inspector navigation back to first part");
  const float faceCenter[]{1.5f,0,0};const auto interior=editor::projectWorldToScreen(s.view(),faceCenter);
  AE_EXPECT_TRUE(interior.valid&&!editor::pickColliderContour(g,id,s.view(),&s.mapScene(),interior.screen)&&editor::pickColliderSurface(g,id,s.view(),&s.mapScene(),interior.screen)==second,"surface center is selectable away from every contour");
  s.handlePointer({92,ui::UiPointerPhase::Down,interior.screen,0});s.handlePointer({92,ui::UiPointerPhase::Up,interior.screen,.02});s.update();
  AE_EXPECT_TRUE(s.selection()==id&&s.screen().expandedNative==second&&s.history().undoDepth()==depth&&editor::serializeEditorDocument(g,0)==original,"real interior tap chooses surface without scene/history mutation");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,font,static_cast<editor::EditorWidget>(editor::widgetId(editor::EditorWidget::ComponentEnableBase)+2)),"edit selected instance through its real Inspector");
  AE_EXPECT_TRUE(!static_cast<const scene::Collider*>(g.find(id)->components.findInstance(second))->enabled&&static_cast<const scene::Collider*>(g.find(id)->components.findInstance(first))->enabled,"only touched collider is disabled");
  editor::EditorDocument reopened;AE_EXPECT_TRUE(editor::deserializeEditorDocument(editor::serializeEditorDocument(g,0),0,reopened),"real scene round-trip");
  runtime::GameWorld world;runtime::ScenePhysics physics;runtime::QueryHit hit;
  AE_EXPECT_TRUE(world.load(reopened)&&physics.start(world),"Jolt consumes selected-instance edit");
  const float origin[]{1.5f,3,0},delta[]{0,-6,0};AE_EXPECT_TRUE(!physics.rayCast(origin,delta,{},hit),"disabled part no longer participates in actual solver queries");physics.stop();
  AE_EXPECT_TRUE(s.history().undo(g)&&editor::serializeEditorDocument(g,0)==original,"one undo restores the edited instance");
  AE_EXPECT_TRUE(world.load(g)&&physics.start(world)&&physics.rayCast(origin,delta,{},hit)&&hit.colliderInstance==second,"restored contour identity is the actual runtime collider identity");
}
namespace {
struct Fixture {
  std::filesystem::path root=std::filesystem::absolute(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"build"/("convex-bake-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));
  test::ConvexCpuLibrary library;editor::EditorSession session;
  bool setup(){std::error_code ec;std::filesystem::create_directories(root,ec);return !ec&&session.setProjectDirectory(root.generic_string().c_str())&&library.connect(session);}
};
bool ray(runtime::ScenePhysics &physics,float x,float z,runtime::QueryHit &hit){const float p[]{x,3,z},d[]{0,-6,0};return physics.rayCast(p,d,{},hit);}
}
AE_TEST(collider_surface_mesh_hull_cavity_pose_resource_and_reopen) {
  Fixture f;AE_EXPECT_TRUE(f.setup(),"actual mesh resource project");auto &s=f.session;std::string error;
  const auto id=test::convexFixtureObject(s,error);AE_EXPECT_TRUE(id,error.c_str());auto &g=s.document();auto object=*g.find(id);
  while(object.components.remove(scene::Collider::descriptor)){}
  auto *c=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));c->shape=scene::ColliderShape::Mesh;c->convex=false;
  const auto instance=c->instanceId();AE_EXPECT_TRUE(g.applyEntityValues(id,object),"triangle mesh collider");
  editor::EditorRay cast;cast.valid=true;cast.origin[1]=3;cast.direction[1]=-1;cast.maximumDistance=10;
  float pose[16];editor::editorTransformMatrix(object.transform,pose);float depth;
  const auto slots=editor::visual_detail::meshColliderSlots(*c,object,s.mapScene());
  AE_EXPECT_TRUE(!slots.empty()&&!s.mapScene().intersectColliderMesh(slots,false,c->hullTolerance,cast,pose,depth),"triangle mesh U cavity remains empty, without bounds fallback");
  AE_EXPECT_TRUE(s.mapScene().intersectColliderMesh(slots,true,c->hullTolerance,cast,pose,depth),"cooked convex hull surface actually fills cavity");
  cast.origin[0]=1.5f;AE_EXPECT_TRUE(s.mapScene().intersectColliderMesh(slots,false,c->hullTolerance,cast,pose,depth),"real triangle wall hit");
  const auto wallDepth=depth;
  AE_EXPECT_TRUE(s.mapScene().intersectColliderMesh(slots,false,c->hullTolerance,cast,pose,depth)&&depth==wallDepth,"cached resource query stable");
  const u32 incomplete[]{slots.front(),0xffffffffu};
  AE_EXPECT_TRUE(!s.mapScene().intersectColliderMesh(incomplete,false,c->hullTolerance,cast,pose,depth),"missing secondary source refuses partial shape");
  c->convex=true;c->meshLocalPose=true;c->centerX=5;c->rotationY=23;
  AE_EXPECT_TRUE(g.applyEntityValues(id,object),"independent mesh-local pose");
  const float eye[]{5,4,-10};editor::EditorViewport view;view.frustum=renderer::buildPerspectiveFrustum(eye,0,.35f,2);view.rect={20,20,800,400};
  const float at[]{5,0,0};auto point=editor::projectWorldToScreen(view,at);
  AE_EXPECT_TRUE(point.valid&&editor::pickColliderSurface(g,id,view,&s.mapScene(),point.screen)==instance,"surface selection follows actual linked hull and local pose");
  editor::EditorDocument reopened;AE_EXPECT_TRUE(editor::deserializeEditorDocument(editor::serializeEditorDocument(g,0),0,reopened)&&editor::pickColliderSurface(reopened,id,view,&s.mapScene(),point.screen)==instance,"surface identity and pose survive native archive roundtrip");
  c->collisionMesh=resources::assetGuidFromSeed("unavailable collision source");AE_EXPECT_TRUE(g.applyEntityValues(id,object)&&!editor::pickColliderSurface(g,id,view,&s.mapScene(),point.screen),"missing linked resource does not use visual geometry instead");
  // Existing package replacement must invalidate the newly cached hull BVH too.
  auto retired=s.mapScene();
  AE_EXPECT_TRUE(retired.adoptPackage(reopened,{},{},{},{},{},0)&&!editor::pickColliderSurface(reopened,id,view,&retired,point.screen),"real package replacement invalidates hull pick geometry");
}
AE_TEST(convex_bake_cavity_resource_parts_undo_reopen_and_pose) {
  Fixture f;AE_EXPECT_TRUE(f.setup(),"host geometry project");auto &s=f.session;auto &g=s.document();std::string error;
  const auto id=test::convexFixtureObject(s,error);AE_EXPECT_TRUE(id,error.c_str());const auto original=*g.find(id);
  AE_EXPECT_TRUE(s.configureDynamicMotor(id,editor::EditorSession::MotorCollisionPolicy::ConvexMesh,error),error.c_str());
  test::ConvexMapGeometry geometry(s.mapScene());runtime::GameWorld world;runtime::ScenePhysics physics;runtime::QueryHit hit;
  AE_EXPECT_TRUE(world.load(g)&&physics.start(world,&geometry)&&ray(physics,0,0,hit),"one hull really fills U cavity");physics.stop();
  AE_EXPECT_TRUE(s.history().undo(g),"undo initial whole hull");
  const auto before=editor::serializeEditorDocument(g,0);const auto depth=s.history().undoDepth();
  resources::ConvexBakeSettings settings;settings.maximumParts=8;settings.voxelResolution=50000;
  AE_EXPECT_TRUE(s.beginMotorDecomposition(id,settings,error),error.c_str());
  AE_EXPECT_TRUE(test::waitConvexBake(s),s.motorDecompositionProgress().error.c_str());
  AE_EXPECT_TRUE(editor::serializeEditorDocument(g,0)==before&&s.history().undoDepth()==depth,"preview does not change scene or history");
  AE_EXPECT_TRUE(s.screen().motorBakePreview.size()>1&&s.screen().motorBakePreview.size()<=8,"true concavity decomposition within part budget");
  AE_EXPECT_TRUE(s.applyMotorDecomposition(error),error.c_str());
  const auto *object=g.find(id);std::ostringstream a,b;runtime::meshRenderer(original)->write(a);runtime::meshRenderer(*object)->write(b);
  AE_EXPECT_TRUE(a.str()==b.str()&&g.childrenOf(id).empty()&&object->id==original.id,"visual/material/identity/hierarchy unchanged");
  std::vector<u64> instances;for(usize i=0;i<object->components.size();++i)if(&object->components.at(i)->type()==&scene::Collider::descriptor){const auto &c=static_cast<const scene::Collider&>(*object->components.at(i));AE_EXPECT_TRUE(c.collisionMesh.valid()&&c.meshLocalPose&&c.convex,"independent resource and editable pose");instances.push_back(c.instanceId());}
  AE_EXPECT_TRUE(instances.size()>1&&s.history().undoDepth()==depth+1,"one undo for authored replacement");
  AE_EXPECT_TRUE(world.load(g)&&physics.start(world,&geometry)&&!ray(physics,0,0,hit)&&ray(physics,1.5f,0,hit),"cavity empty; real wall still collides");
  AE_EXPECT_TRUE(hit.colliderInstance&&std::find(instances.begin(),instances.end(),hit.colliderInstance)!=instances.end(),"query identifies actual part");const auto wall=hit.colliderInstance;physics.stop();
  auto edited=*g.find(id);auto *c=static_cast<scene::Collider*>(edited.components.editInstance(wall));c->enabled=false;
  AE_EXPECT_TRUE(g.applyEntityValues(id,edited)&&world.load(g)&&physics.start(world,&geometry)&&!ray(physics,1.5f,0,hit),"part disable changes physical collision");physics.stop();
  c->enabled=true;c->centerX=5;c->rotationY=23;edited.transform.scale[0]=2.7f;edited.transform.scale[2]=1.1f;
  AE_EXPECT_TRUE(g.applyEntityValues(id,edited)&&world.load(g)&&physics.start(world,&geometry),"local rotation under nonuniform scale uses full affine geometry");
  AE_EXPECT_TRUE(!ray(physics,4.05f,0,hit),"moved wall no longer collides at old location");physics.stop();
  const auto visuals=editor::collectComponentVisuals(g,id,1,&s.mapScene(),0,0,0,wall);bool moved=false;for(const auto &visual:visuals)for(const auto &line:visual.segments)moved|=line.a[0]>8;
  AE_EXPECT_TRUE(moved,"real collision outline includes local pose");
  // Restore original bake, then exercise save/load and asset re-opening.
  AE_EXPECT_TRUE(s.history().undo(g)&&!runtime::physicsBody(*g.find(id))&&s.history().redo(g),"undo/redo retains generated resources and stable instances");
  const auto archive=editor::serializeEditorDocument(g,0);editor::EditorDocument reloaded;AE_EXPECT_TRUE(editor::deserializeEditorDocument(archive,0,reloaded),"archive collider v8 roundtrip");
  Fixture reopened;AE_EXPECT_TRUE(reopened.library.connect(reopened.session)&&reopened.session.setProjectDirectory(f.root.generic_string().c_str()),"reopen project");
  resources::AssetRegistry registry;AE_EXPECT_TRUE(resources::AssetRegistry::deserialize(s.serializeAssets(),registry)&&reopened.session.loadAssets(s.serializeAssets()),"registry roundtrip");
  std::vector<editor::EditorSession::ReopenedSource> sources;resources::AssetGuid sourceGuid{},bakeGuid{};
  for(const auto &record:registry.records())if(record.type==resources::AssetType::Mesh){std::vector<u8> bytes;AE_EXPECT_TRUE(editor::EditorImportTransaction::read(f.root/editor::EditorImportTransaction::fromUtf8(record.path),bytes),"persistent resource file");resources::GltfImport model;AE_EXPECT_TRUE(resources::importGlb(bytes,{},{},model),"actual parser on saved resources");sources.push_back({std::move(model),record.contentHash,record.path});if(record.path.starts_with("Collision/")){bakeGuid=record.guid;AE_EXPECT_TRUE(!record.dependencies.empty(),"source dependency preserved");}else sourceGuid=record.guid;}
  AE_EXPECT_TRUE(sourceGuid.valid()&&bakeGuid.valid()&&registry.dependents(sourceGuid).size()==1,"baked resource prevents accidental source deletion");
  std::vector<editor::EditorSession::ModelImportReport> reports;AE_EXPECT_TRUE(reopened.session.reopenSources(sources,reports,error),error.c_str());
  resources::AssetRegistry reopenedRegistry;
  AE_EXPECT_TRUE(resources::AssetRegistry::deserialize(reopened.session.serializeAssets(),reopenedRegistry)&&reopenedRegistry.dependents(sourceGuid).size()==1,"source dependency survives resource reopening");
  reopened.session.document()=reloaded;test::ConvexMapGeometry reloadedGeometry(reopened.session.mapScene());
  AE_EXPECT_TRUE(world.load(reloaded)&&physics.start(world,&reloadedGeometry)&&!ray(physics,0,0,hit)&&ray(physics,1.5f,0,hit),"saved compound retains cavity in fresh physics world");physics.stop();
  auto moving=*reloaded.find(id);moving.transform.position[1]=2;reloaded.applyEntityValues(id,moving);
  const auto floor=reloaded.createEntity(reloaded.root(),runtime::ObjectKind::Folder,"Floor");auto ground=*reloaded.find(floor);ground.transform.position[1]=-.5f;
  auto *body=static_cast<scene::PhysicsBody*>(ground.components.add(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Static;
  auto *shape=static_cast<scene::Collider*>(ground.components.add(scene::Collider::descriptor));shape->halfX=shape->halfZ=30;shape->halfY=.5f;reloaded.applyEntityValues(floor,ground);
  AE_EXPECT_TRUE(world.load(reloaded)&&physics.start(world,&reloadedGeometry),"fresh compound and motor on floor");
  for(u32 i=0;i<180;++i)AE_EXPECT_TRUE(physics.advance(1./60,world),"compound fall and support lifecycle");
  runtime::ScenePhysics::DynamicMotorState state;AE_EXPECT_TRUE(physics.dynamicMotorState(id,state)&&state.grounded&&state.support==floor,"actual decomposed geometry grants motor support");
  AE_EXPECT_TRUE(physics.setDynamicMotorMove(id,1,0,0),"motor command independent of visual");for(u32 i=0;i<60;++i)AE_EXPECT_TRUE(physics.advance(1./60,world),"compound motor moves");
  float pose[16];AE_EXPECT_TRUE(runtime::worldMatrix(world.poseGraph(),id,pose)&&pose[12]>1,"decomposed body moves through actual solver");
  AE_EXPECT_TRUE(physics.jumpDynamicMotor(id)&&physics.advance(1./60,world),"jump through actual body motor");float velocity[3];AE_EXPECT_TRUE(physics.getBodyVelocity(id,velocity)&&velocity[1]>1,"jump affects compound velocity");
}
AE_TEST(convex_bake_cancel_stale_and_gpu_refusal_are_non_destructive) {
  Fixture f;AE_EXPECT_TRUE(f.setup(),"project");auto &s=f.session;std::string error;const auto id=test::convexFixtureObject(s,error);AE_EXPECT_TRUE(id,error.c_str());
  resources::ConvexBakeSettings settings;settings.maximumParts=8;settings.voxelResolution=50000;
  const auto before=editor::serializeEditorDocument(s.document(),0);const auto registry=s.serializeAssets();
  AE_EXPECT_TRUE(s.beginMotorDecomposition(id,settings,error),error.c_str());s.cancelMotorDecomposition();
  const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(30);while(s.motorDecompositionProgress().status==resources::ConvexBakeStatus::Running&&std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(10));
  s.update();AE_EXPECT_TRUE(!s.applyMotorDecomposition(error)&&before==editor::serializeEditorDocument(s.document(),0)&&registry==s.serializeAssets(),"cancel cannot publish or mutate scene");
  AE_EXPECT_TRUE(s.beginMotorDecomposition(id,settings,error)&&test::waitConvexBake(s),error.c_str());
  auto changed=*s.document().find(id);changed.transform.position[0]=1;s.document().applyEntityValues(id,changed);s.update();
  AE_EXPECT_TRUE(!s.applyMotorDecomposition(error)&&!s.screen().motorBakeReady,"stale revision discards preview");
  AE_EXPECT_TRUE(s.beginMotorDecomposition(id,settings,error)&&test::waitConvexBake(s),error.c_str());
  const auto changedBefore=editor::serializeEditorDocument(s.document(),0);f.library.refuse=true;
  AE_EXPECT_TRUE(!s.applyMotorDecomposition(error)&&changedBefore==editor::serializeEditorDocument(s.document(),0)&&registry==s.serializeAssets(),"renderer refusal rolls back resources and scene");
  AE_EXPECT_TRUE(!std::filesystem::exists(f.root/".astra/import-transaction/journal"),"journal recovered after publication refusal");
  Fixture invalid;AE_EXPECT_TRUE(invalid.setup(),"isolated authoring project");
  const auto target=test::convexFixtureObject(invalid.session,error);AE_EXPECT_TRUE(target,error.c_str());
  auto &graph=invalid.session.document();const auto bad=graph.createEntity(graph.root(),runtime::ObjectKind::Folder,"Missing collision source");auto malformed=*graph.find(bad);
  malformed.components.add(scene::PhysicsBody::descriptor);auto *missing=static_cast<scene::Collider*>(malformed.components.add(scene::Collider::descriptor));missing->shape=scene::ColliderShape::Mesh;missing->convex=true;
  AE_EXPECT_TRUE(graph.applyEntityValues(bad,malformed),"invalid physical draft can remain authored");
  const auto draft=editor::serializeEditorDocument(graph,0),assets=invalid.session.serializeAssets();
  AE_EXPECT_TRUE(invalid.session.beginMotorDecomposition(target,settings,error)&&test::waitConvexBake(invalid.session),"generation does not synchronously cook an unrelated broken scene");
  AE_EXPECT_TRUE(!invalid.session.applyMotorDecomposition(error)&&!error.empty()&&draft==editor::serializeEditorDocument(graph,0)&&assets==invalid.session.serializeAssets(),"Apply still rejects the invalid complete physics candidate before publication");
}
AE_TEST(convex_bake_hierarchy_selection_affine_instances_and_ownership) {
  Fixture f;AE_EXPECT_TRUE(f.setup(),"real resource project");auto &s=f.session;auto &g=s.document();std::string error;
  const auto id=test::convexFixtureHierarchy(s,error);AE_EXPECT_TRUE(id,error.c_str());
  resources::ConvexBakeSettings settings;settings.maximumParts=16;settings.voxelResolution=100000;
  AE_EXPECT_TRUE(!s.beginMotorDecomposition(id,settings,error),"empty root does not silently absorb children");
  auto options=s.motorDecompositionSources(id);AE_EXPECT_TRUE(options.size()==2&&options[0].error.empty()&&options[1].error.empty(),"both mesh instances are discoverable");
  const auto first=options[g.find(options[0].source.object)->name==std::string("RotatedPart")?0:1].source;
  const auto second=options[first==options[0].source?1:0].source;const auto branch=g.find(first.object)->parent;
  auto blocked=*g.find(branch);blocked.components.add(scene::PhysicsBody::descriptor);AE_EXPECT_TRUE(g.applyEntityValues(branch,blocked),"independent child authority");
  std::vector<editor::EditorSession::MotorBakeSource> chosen{first,second};
  AE_EXPECT_TRUE(!s.selectMotorDecompositionSources(id,chosen,error)&&s.selectedMotorDecompositionSources().empty(),"nested body cannot be absorbed; failed selection is atomic");
  blocked.components.remove(scene::PhysicsBody::descriptor);AE_EXPECT_TRUE(g.applyEntityValues(branch,blocked),"remove explicit temporary draft");
  const auto foreign=g.createEntity(g.root(),runtime::ObjectKind::Mesh,"OutsideHierarchy");auto unrelated=*g.find(foreign);unrelated.components=g.find(first.object)->components;unrelated.transform.position[0]=30;AE_EXPECT_TRUE(g.applyEntityValues(foreign,unrelated),"outside mesh");
  const editor::EditorSession::MotorBakeSource outside[]{ {foreign,0} };
  AE_EXPECT_TRUE(!s.selectMotorDecompositionSources(id,outside,error),"outside hierarchy rejected");
  AE_EXPECT_TRUE(s.selectMotorDecompositionSources(id,std::span(&first,1),error),error.c_str());
  const editor::EditorSession::MotorBakeSource duplicate[]{first,first};
  AE_EXPECT_TRUE(!s.selectMotorDecompositionSources(id,duplicate,error)&&s.selectedMotorDecompositionSources().size()==1,"duplicate selection cannot overwrite valid draft");
  const auto before=editor::serializeEditorDocument(g,0);const auto depth=s.history().undoDepth();
  AE_EXPECT_TRUE(s.beginMotorDecomposition(id,settings,error)&&test::waitConvexBake(s)&&s.applyMotorDecomposition(error),error.c_str());
  test::ConvexMapGeometry geometry(s.mapScene());runtime::GameWorld world;runtime::ScenePhysics physics;runtime::QueryHit hit;
  constexpr float radians=23*3.14159265359f/180;const float wallX=-4+1.3f*std::cos(radians)*1.5f,wallZ=-.8f*std::sin(radians)*1.5f;
  AE_EXPECT_TRUE(world.load(g)&&physics.start(world,&geometry)&&ray(physics,wallX,wallZ,hit),"child rotation beneath nonuniform scale cooks actual affine wall");
  AE_EXPECT_TRUE(!ray(physics,4-1.2f,0,hit),"excluded second instance has no generated collision");physics.stop();
  AE_EXPECT_TRUE(s.history().undo(g)&&editor::serializeEditorDocument(g,0)==before,"one Undo restores root and untouched hierarchy");
  AE_EXPECT_TRUE(s.selectMotorDecompositionSources(id,chosen,error)&&s.beginMotorDecomposition(id,settings,error)&&test::waitConvexBake(s)&&s.applyMotorDecomposition(error),error.c_str());
  AE_EXPECT_TRUE(s.history().undoDepth()==depth+1&&!runtime::meshRenderer(*g.find(id))&&g.find(first.object)->parent==branch&&g.find(second.object)->parent==id,"body is independent of render hierarchy");
  AE_EXPECT_TRUE(world.load(g)&&physics.start(world,&geometry)&&ray(physics,wallX,wallZ,hit)&&ray(physics,4-1.2f,0,hit),"same mesh GUID in distinct rotated/scaled instances is not deduplicated");
  AE_EXPECT_TRUE(!ray(physics,-4,0,hit)&&!ray(physics,4,0,hit),"both cavities remain empty");physics.stop();
  resources::AssetRegistry registry;AE_EXPECT_TRUE(resources::AssetRegistry::deserialize(s.serializeAssets(),registry),"published registry");bool provenance=false;
  for(const auto &record:registry.records())if(record.path.starts_with("Collision/")) {
    std::ifstream in(f.root/record.path,std::ios::binary);const std::string glb{std::istreambuf_iterator<char>(in),{}};
    provenance|=glb.find("\"objectId\":\""+std::to_string(first.object)+"\"")!=std::string::npos&&glb.find("\"objectId\":\""+std::to_string(second.object)+"\"")!=std::string::npos&&glb.find("relativeMatrix")!=std::string::npos;
  }
  AE_EXPECT_TRUE(provenance,"immutable resource records both real origins and local affine matrices");
  const auto archive=editor::serializeEditorDocument(g,0);editor::EditorDocument reopened;
  AE_EXPECT_TRUE(editor::deserializeEditorDocument(archive,0,reopened)&&world.load(reopened)&&physics.start(world,&geometry)&&ray(physics,wallX,wallZ,hit)&&ray(physics,2.8f,0,hit),"hierarchy and generated parts survive scene roundtrip");physics.stop();
}
AE_TEST(convex_bake_touching_hierarchy_solids_share_one_body) {
  Fixture f;AE_EXPECT_TRUE(f.setup(),"real touching assembly project");auto &s=f.session;auto &g=s.document();std::string error;
  const auto id=test::convexFixtureHierarchy(s,error);AE_EXPECT_TRUE(id,error.c_str());auto options=s.motorDecompositionSources(id);
  const auto first=options[0].source,second=options[1].source;AE_EXPECT_TRUE(g.reparent(first.object,id,0)&&g.reparent(second.object,id,1),"direct visual children");
  auto left=*g.find(first.object),right=*g.find(second.object);left.transform={};right.transform={};right.transform.position[0]=4;
  AE_EXPECT_TRUE(g.applyEntityValues(first.object,left)&&g.applyEntityValues(second.object,right),"closed solids touch along their outer walls");
  const editor::EditorSession::MotorBakeSource chosen[]{first,second};resources::ConvexBakeSettings settings;settings.maximumParts=16;settings.voxelResolution=50000;
  AE_EXPECT_TRUE(s.selectMotorDecompositionSources(id,chosen,error)&&s.beginMotorDecomposition(id,settings,error)&&test::waitConvexBake(s)&&s.applyMotorDecomposition(error),error.c_str());
  test::ConvexMapGeometry geometry(s.mapScene());runtime::GameWorld world;runtime::ScenePhysics physics;runtime::QueryHit hit;
  AE_EXPECT_TRUE(world.load(g)&&physics.start(world,&geometry)&&ray(physics,1.5f,0,hit)&&ray(physics,5.5f,0,hit),"touching objects become real compound shapes");
  AE_EXPECT_TRUE(!ray(physics,0,0,hit)&&!ray(physics,4,0,hit),"separate cavities remain open");physics.stop();
  AE_EXPECT_TRUE(s.history().undo(g),"return to authoring before overlap");right=*g.find(second.object);right.transform.position[0]=3;
  AE_EXPECT_TRUE(g.applyEntityValues(second.object,right)&&s.selectMotorDecompositionSources(id,chosen,error)&&s.beginMotorDecomposition(id,settings,error)&&test::waitConvexBake(s)&&s.applyMotorDecomposition(error),error.c_str());
  AE_EXPECT_TRUE(world.load(g)&&physics.start(world,&geometry)&&ray(physics,1.5f,0,hit)&&ray(physics,4.5f,0,hit),"overlapping sources retain their own solid identity");physics.stop();
}
AE_TEST(convex_bake_open_mesh_and_legacy_pose_are_explicit) {
  // A default must cover every root slot, even when a slot fails to resolve.
  // Explicit subset selection is different from silently losing geometry.
  {
    Fixture f;AE_EXPECT_TRUE(f.setup(),"missing-slot project");std::string error;auto &s=f.session;auto &g=s.document();
    const auto id=test::convexFixtureObject(s,error);AE_EXPECT_TRUE(id,error.c_str());auto values=*g.find(id);
    auto *render=runtime::editMeshRenderer(values);scene::MeshSubmesh missing;missing.asset={12345,67890};render->submeshes.push_back(missing);
    AE_EXPECT_TRUE(g.applyEntityValues(id,values),"unresolved visual resource can remain an authoring draft");
    resources::ConvexBakeSettings budget;budget.maximumParts=8;budget.voxelResolution=50000;
    AE_EXPECT_TRUE(!s.beginMotorDecomposition(id,budget,error)&&s.selectedMotorDecompositionSources().size()==2,"default generation refuses a missing slot instead of omitting it");
    const editor::EditorSession::MotorBakeSource subset[]{ {id,0} };
    AE_EXPECT_TRUE(s.selectMotorDecompositionSources(id,subset,error)&&s.beginMotorDecomposition(id,budget,error)&&test::waitConvexBake(s),"intentional subset generation is supported");
    s.cancelMotorDecomposition();
  }
  resources::ConvexBakeJob job;std::string error;resources::ConvexBakeSettings settings;
  AE_EXPECT_TRUE(job.start({0,0,0,1,0,0,0,1,0},settings,std::string(64,'a'),{},error),"worker accepts bounded input");
  const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(job.progress().status==resources::ConvexBakeStatus::Running&&std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(5));
  AE_EXPECT_TRUE(job.progress().status==resources::ConvexBakeStatus::Failed&&!job.result()&&!job.progress().error.empty(),"open geometry never silently becomes a box");
  // Fixture v7 imutável: o writer atual inclui material local (v9), portanto
  // truncar sua saída não produz um arquivo antigo.
  std::istringstream input("3 .5 .5 .5 .5 .5 8 0 0 0 0 0 .001 5 0 1 0 - 1 1");scene::Collider read;
  AE_EXPECT_TRUE(read.read(input,7)&&read.centerX==8&&!read.meshLocalPose,"v7 hidden pose preserved but remains inactive");
  settings.maximumParts=33;AE_EXPECT_TRUE(!resources::validConvexBakeSettings(settings),"component and mobile budget enforced");
}
