#include "harness.h"
#include "collider_occlusion_fixture.h"
#include "editor/editor_component_visuals.h"
#include "editor/editor_archive.h"
#include <fstream>
using namespace ae;
AE_TEST(collider_occlusion_real_triangles_clipping_identity_and_lazy_geometry) {
  const float eye[]{0,0,-10};editor::EditorViewport view;view.rect={20,30,800,400};
  view.frustum=renderer::buildPerspectiveFrustum(eye,0,0,2);
  const float zero[3]{};const auto point=editor::projectWorldToScreen(view,zero).screen;
  auto mesh=std::make_shared<editor::EditorPickMesh>();
  AE_EXPECT_TRUE(mesh->build({{{-2,-2,-2,2,-2,-2,0,2,-2}}}),"real foreground triangle");
  std::vector<editor::EditorPickCandidate> candidates(3);
  for(u32 i=0;i<3;++i){candidates[i].id=i+2;candidates[i].radius=4;}
  candidates[0].mesh=mesh;u32 missing=0,offRay=0;
  candidates[1].resolve=[&](){++missing;return std::shared_ptr<const editor::EditorPickMesh>{};};
  candidates[2].center[0]=100;candidates[2].resolve=[&](){++offRay;return mesh;};
  const auto ray=editor::screenPointToRay(view,point);
  AE_EXPECT_TRUE(editor::colliderPointOccluded(candidates,view,point,9,9.5f),"foreground real surface occludes");
  AE_EXPECT_TRUE(!editor::colliderPointOccluded(candidates,view,point,2,9.5f),"own visual ignored; unavailable CPU geometry never invents obstacle");
  AE_EXPECT_TRUE(missing>0&&offRay==0,"only on-ray resources resolve, preserving lazy BVH");
  candidates[1].resolve={};
  AE_EXPECT_TRUE(!editor::pickNearest(candidates,ray,2,true).hit,"legacy bounds-only candidate cannot occlude");
  candidates[0].model[0]=-1.4f;candidates[0].model[4]=.4f;candidates[0].model[5]=.8f;
  AE_EXPECT_TRUE(editor::colliderPointOccluded(candidates,view,point,9,9.5f),"negative scale and shear retain real depth");
  candidates[0].model[0]=0;
  AE_EXPECT_TRUE(!editor::colliderPointOccluded(candidates,view,point,9,9.5f),"singular mesh is not a fake obstacle");
  candidates[0].model[0]=1;candidates[0].model[4]=0;candidates[0].model[5]=1;
  auto clipped=ray;clipped.minimumDistance=8.1f;
  AE_EXPECT_TRUE(!editor::pickNearest(candidates,clipped,0,true).hit,"near clipping excludes foreground surface");
  clipped=ray;clipped.maximumDistance=7.9f;
  AE_EXPECT_TRUE(!editor::pickNearest(candidates,clipped,0,true).hit,"far clipping excludes foreground surface");
  AE_EXPECT_TRUE(!editor::colliderPointOccluded(candidates,view,point,9,8.f),"coincident visual/collider surface stays editable");
}
AE_TEST(collider_occlusion_contour_perspective_world_depth_and_visible_portions) {
  const float eye[]{0,0,-10};editor::EditorViewport view;view.rect={20,30,800,400};
  view.frustum=renderer::buildPerspectiveFrustum(eye,0,0,2);
  editor::ComponentVisualSegment line{{-2,0,-8},{2,0,2}};
  ui::UiPoint a,b;AE_EXPECT_TRUE(editor::projectSegmentToScreen(view,line.a,line.b,a,b),"perspective segment");
  const ui::UiPoint midpoint{(a.x+b.x)*.5f,(a.y+b.y)*.5f};float depth;
  AE_EXPECT_TRUE(editor::colliderContourWorldDepth(view,line,midpoint,depth),"recover actual world depth");
  const auto r=editor::screenPointToRay(view,midpoint);
  const double u=2./14.;const double expected=(2+10*u)/r.direction[2];
  AE_EXPECT_TRUE(std::abs(depth-expected)<.0002,"screen midpoint is not affine world midpoint");
  editor::EditorDocument g;const auto id=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Contour");auto e=*g.find(id);
  auto *c=static_cast<scene::Collider*>(e.components.add(scene::Collider::descriptor));c->halfX=c->halfY=c->halfZ=1;g.applyEntityValues(id,e);
  auto blocker=std::make_shared<editor::EditorPickMesh>();
  AE_EXPECT_TRUE(blocker->build({{{-4,-4,-2,0,-4,-2,0,4,-2}},{{-4,-4,-2,0,4,-2,-4,4,-2}}}),"left half is blocked by triangles");
  editor::EditorPickCandidate occluder;occluder.id=20;occluder.radius=8;occluder.mesh=blocker;
  const std::span<const editor::EditorPickCandidate> occluders(&occluder,1);
  const float left[]{-1,0,-1},right[]{1,0,-1};
  AE_EXPECT_TRUE(editor::pickColliderContour(g,id,view,nullptr,editor::projectWorldToScreen(view,left).screen)==1,"diagnostic contour exists behind wall");
  AE_EXPECT_TRUE(!editor::pickColliderContour(g,id,view,nullptr,editor::projectWorldToScreen(view,left).screen,8,0,nullptr,occluders),"hidden contour cannot bypass surface occlusion");
  AE_EXPECT_TRUE(editor::pickColliderContour(g,id,view,nullptr,editor::projectWorldToScreen(view,right).screen,8,0,nullptr,occluders)==1,"other visible portion remains selectable");
  renderer::PerspectiveVisibilitySettings orthographic;orthographic.projection=renderer::CameraProjection::Orthographic;orthographic.orthographicHalfHeight=3;
  view.frustum=renderer::buildPerspectiveFrustum(eye,0,0,2,orthographic);
  editor::ComponentVisualSegment ortho{{-2,0,-8},{2,0,2}};editor::projectSegmentToScreen(view,ortho.a,ortho.b,a,b);
  AE_EXPECT_TRUE(editor::colliderContourWorldDepth(view,ortho,{(a.x+b.x)*.5f,(a.y+b.y)*.5f},depth)&&std::abs(depth-7)<.0002,"orthographic depth uses its shifted ray origin");
  renderer::PerspectiveVisibilitySettings clipped;clipped.nearPlane=1;clipped.farPlane=6;
  view.frustum=renderer::buildPerspectiveFrustum(eye,0,0,2,clipped);
  line={{-1,0,-9.5f},{1,0,0}};
  AE_EXPECT_TRUE(editor::projectSegmentToScreen(view,line.a,line.b,a,b)&&
      editor::colliderContourWorldDepth(view,line,a,depth),"near/far clipping recovers real segment point");
  auto ray=editor::screenPointToRay(view,a);AE_EXPECT_TRUE(std::abs(depth*ray.direction[2]-1)<.0002,"near-clipped endpoint");
  AE_EXPECT_TRUE(editor::colliderContourWorldDepth(view,line,b,depth),"far-clipped endpoint");ray=editor::screenPointToRay(view,b);
  AE_EXPECT_TRUE(std::abs(depth*ray.direction[2]-6)<.0002,"perspective far plane matches surface query interval");
}
AE_TEST(collider_occlusion_pointer_visibility_locks_archive_undo_solver_and_hole) {
  const auto read=[](const char *name){std::ifstream in(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui"/name,std::ios::binary);return std::vector<u8>(std::istreambuf_iterator<char>(in),{});};
  ui::UiFont font;ui::UiIconAtlas icons;AE_EXPECT_TRUE(font.load(read("astra-ui-font.aeuf"))&&icons.load(read("astra-ui-icons.aeui")),"real UI");
  const auto folder=std::filesystem::path(AETHER_REPOSITORY_ROOT)/"build"/("occlusion-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(folder);test::ConvexCpuLibrary library;editor::EditorSession s;
  AE_EXPECT_TRUE(s.setProjectDirectory(folder.generic_string().c_str()),"actual project");
  AE_EXPECT_TRUE(library.connect(s),"actual registered resource publisher");s.initialize(&font,&icons);s.setSurface({0,0,1280,720},{});
  auto &g=s.document();const auto body=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Body");auto value=*g.find(body);
  auto *physical=static_cast<scene::PhysicsBody*>(value.components.add(scene::PhysicsBody::descriptor));physical->motion=scene::BodyMotion::Static;g.applyEntityValues(body,value);
  const auto part=g.createEntity(body,runtime::ObjectKind::Mesh,"Part");value=*g.find(part);value.components.add(scene::Collider::descriptor);
  AE_EXPECT_TRUE(runtime::configurePrimitive(value,scene::PrimitiveType::Cube,{1,s.mapScene().assetGuid(0),s.mapScene().materialForAsset(0)}),"real primitive geometry");
  while(value.components.remove(scene::PhysicsBody::descriptor)){}
  for(usize n=0;n<value.components.size();)if(&value.components.at(n)->type()==&scene::Collider::descriptor&&value.components.at(n)->instanceId()!=1)value.components.removeInstance(value.components.at(n)->instanceId());else ++n;
  static_cast<scene::Collider*>(value.components.editInstance(1))->owner=body;AE_EXPECT_TRUE(g.applyEntityValues(part,value),"part published to actual document");
  const auto wall=test::addOcclusionWall(s,part);AE_EXPECT_TRUE(wall,"visible unrelated wall without physics");
  const float eye[]{0,0,-10};s.setCameraPose(eye,0,0);s.update();
  editor::EditorPickCandidate checked;checked.id=wall;
  if(!s.mapScene().bounds(g,wall,checked.center,checked.radius)){const auto *r=editor::meshRenderer(*g.find(wall));std::fprintf(stderr,"wall renderer=%d mesh=%u asset=%s mapped=%u components=%zu mapDraws=%zu\n",r!=nullptr,r?r->mesh:0,r?r->asset.text().c_str():"none",r?s.mapScene().assetSlot(r->asset):0,size_t(g.find(wall)->components.size()),library.draws.size());}
  AE_EXPECT_TRUE(s.mapScene().bounds(g,wall,checked.center,checked.radius)&&s.mapScene().pickGeometry(g,wall,checked),"wall has published triangles before pointer validation");
  const auto focus=[&](){s.setSelection(body);s.update();return test::tapConvexWidget(s,font,editor::EditorWidget::ToolSelect)&&
    (s.screen().expandedNative==1||test::tapConvexWidget(s,font,editor::EditorWidget::ComponentFoldBase));};
  const auto tap=[&](){const float center[3]{};const auto point=editor::projectWorldToScreen(s.view(),center).screen;
    s.handlePointer({96,ui::UiPointerPhase::Down,point,0});s.handlePointer({96,ui::UiPointerPhase::Up,point,.02});s.update();};
  AE_EXPECT_TRUE(focus(),"Body context");const auto original=editor::serializeEditorDocument(g,0);s.history().clear();tap();
  AE_EXPECT_TRUE(s.selection()==wall&&s.history().undoDepth()==0&&editor::serializeEditorDocument(g,0)==original,"visible wall wins over hidden linked surface; navigation changes no authored bytes");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,font,static_cast<editor::EditorWidget>(editor::hierarchyPickWidget(wall)))&&focus(),"lock wall through actual Hierarchy");tap();
  AE_EXPECT_TRUE(s.selection()==body&&s.screen().status.find("travado")!=std::string::npos,"visible locked wall blocks without stealing selection or click-through");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,font,static_cast<editor::EditorWidget>(editor::hierarchyEyeWidget(wall)))&&focus(),"hide wall through real eye control");tap();
  AE_EXPECT_TRUE(s.selection()==part&&s.screen().expandedNative==1&&editor::serializeEditorDocument(g,0)==original&&s.history().undoDepth()==0,"hidden wall releases part despite its own visual mesh; eye/lock are editor state");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,font,editor::EditorWidget::ComponentEnableBase),"actual selected Collider Inspector");
  editor::EditorDocument reopened;AE_EXPECT_TRUE(editor::deserializeEditorDocument(editor::serializeEditorDocument(g,0),0,reopened),"native archive");
  runtime::GameWorld world;runtime::ScenePhysics physics;runtime::QueryHit hit;const float from[]{0,3,0},down[]{0,-6,0};
  AE_EXPECT_TRUE(world.load(reopened)&&physics.start(world)&&!physics.rayCast(from,down,{},hit),"Jolt consumes edited linked Collider only, no fake wall physics");physics.stop();
  AE_EXPECT_TRUE(s.history().undo(g)&&editor::serializeEditorDocument(g,0)==original,"one undo restores actual shape");
  AE_EXPECT_TRUE(world.load(g)&&physics.start(world)&&physics.rayCast(from,down,{},hit)&&hit.object==body&&hit.colliderObject==part&&hit.colliderInstance==1,"restored runtime identity");physics.stop();
  AE_EXPECT_TRUE(test::tapConvexWidget(s,font,static_cast<editor::EditorWidget>(editor::hierarchyEyeWidget(wall))),"show wall again");
  value=*g.find(wall);value.layer=7;g.applyEntityValues(wall,value);
  AE_EXPECT_TRUE(test::tapConvexWidget(s,font,editor::EditorWidget::SceneLayersOpen)&&
      test::tapConvexWidget(s,font,static_cast<editor::EditorWidget>(editor::widgetId(editor::EditorWidget::SceneLayerVisibleBase)+7))&&
      test::tapConvexWidget(s,font,editor::EditorWidget::SceneLayersClose)&&focus(),"hidden layer through actual layer controls");
  tap();AE_EXPECT_EQ(s.selection(),part,"hidden layer never occludes");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,font,editor::EditorWidget::SceneLayersOpen)&&test::tapConvexWidget(s,font,editor::EditorWidget::SceneLayersShowAll)&&test::tapConvexWidget(s,font,editor::EditorWidget::SceneLayersClose),"restore layer visibility");
  value.visible=false;g.applyEntityValues(wall,value);AE_EXPECT_TRUE(focus(),"invisible object");tap();AE_EXPECT_EQ(s.selection(),part,"invisible renderer never occludes");
  value.visible=true;auto *render=editor::editMeshRenderer(value);render->enabled=false;g.applyEntityValues(wall,value);
  AE_EXPECT_TRUE(focus(),"disabled renderer");tap();AE_EXPECT_EQ(s.selection(),part,"disabled renderer never occludes");
  g.destroyEntity(wall);
  std::string error;const auto hole=test::convexFixtureObject(s,error);AE_EXPECT_TRUE(hole,error.c_str());value=*g.find(hole);
  value.transform.rotationDegrees[0]=90;value.transform.position[2]=-2;g.applyEntityValues(hole,value);
  AE_EXPECT_TRUE(focus(),"imported concave visual");tap();AE_EXPECT_EQ(s.selection(),part,"hole in imported triangles allows picking; bounds do not block");
  value.transform.position[0]=-1.5f;g.applyEntityValues(hole,value);
  AE_EXPECT_TRUE(focus(),"solid arm over part");tap();AE_EXPECT_EQ(s.selection(),hole,"same imported mesh blocks on its actual solid arm");
}
