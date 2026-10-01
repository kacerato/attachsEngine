#include "editor/editor_creation_catalog.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include "harness.h"
#include "resources/curve3d.h"
#include "scene/path.h"
#include "editor/editor_session.h"
#include <sstream>
using namespace ae;
AE_TEST(curve3d_distance_closed_handles_degenerate_and_bounded_invalid_bake){
 resources::Curve3D c;c.points={ {1,{0,0,0},{},{1,2,0}}, {2,{4,0,0},{-1,2,0},{}} };
 resources::BakedCurve3D bake;AE_EXPECT_TRUE(bake.bake(c,.001),"adaptive Bezier bake");AE_EXPECT_TRUE(bake.length()>4&&bake.samples().size()>2,"handles affect length and subdivisions");resources::CurveVector3 pos{},tangent{};AE_EXPECT_TRUE(bake.sampleDistance(bake.length()*.5,pos,tangent)&&pos[1]>1,"distance sample follows curved geometry");AE_EXPECT_TRUE(std::abs(std::hypot(tangent[0],tangent[1],tangent[2])-1)<.001,"normalized tangent");
 const double oldLength=bake.length();auto invalid=c;invalid.points[1].id=1;AE_EXPECT_TRUE(!bake.bake(invalid)&&bake.length()==oldLength,"invalid bake preserves prior derived cache");
 c.points[0].out={};c.points[1].in={};c.closed=true;AE_EXPECT_TRUE(bake.bake(c)&&std::abs(bake.length()-8)<.001,"closed final segment contributes length");AE_EXPECT_TRUE(bake.sampleDistance(9,pos,tangent,true)&&std::abs(pos[0]-1)<.001,"wrapped distance");
 c.points[0].out={0,1,0};c.points[0].in={0,-1,0};AE_EXPECT_TRUE(bake.bake(c),"closed oriented handles");resources::CurveVector3 startTangent{},endTangent{};bake.sampleDistance(0,pos,startTangent);bake.sampleDistance(bake.length(),pos,endTangent);AE_EXPECT_TRUE(startTangent==endTangent&&startTangent[1]>.99f,"closed endpoints share authored seam tangent");c.points[0].out={};c.points[0].in={};
 c.points[1].position=c.points[0].position;AE_EXPECT_TRUE(bake.bake(c)&&bake.length()==0&&bake.sampleDistance(0,pos,tangent)&&tangent==resources::CurveVector3{},"degenerate curve is explicit zero direction");
}
AE_TEST(path_point_identity_history_reorder_and_document_roundtrip){
 editor::EditorSession session;auto&doc=session.document();const auto id=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"Path");auto value=*doc.find(id);auto*path=static_cast<scene::Path*>(value.components.add(scene::Path::descriptor));const auto instance=path->instanceId();AE_EXPECT_TRUE(session.history().applyValues(doc,id,value),"authoring component through history");
 u64 a=0,b=0;resources::CurvePoint3D point;point.position={0,0,0};AE_EXPECT_TRUE(session.insertPathPoint(id,instance,0,point,a),"insert first point");point.position={4,0,0};AE_EXPECT_TRUE(session.insertPathPoint(id,instance,0,point,b)&&a!=b,"allocate durable distinct point IDs");
 point.out={1,2,0};AE_EXPECT_TRUE(session.editPathPoint(id,instance,a,point),"edit by durable ID");AE_EXPECT_TRUE(session.movePathPoint(id,instance,b,a),"reorder by IDs");
 const auto read=[&](){return static_cast<const scene::Path*>(doc.find(id)->components.findInstance(instance));};AE_EXPECT_TRUE(read()->curve.points.front().id==b&&read()->point(a)->out[1]==2,"reorder preserves identity and handles");
 AE_EXPECT_TRUE(session.removePathPoint(id,instance,a)&&!read()->point(a),"remove point");AE_EXPECT_TRUE(session.history().undo(doc)&&read()->point(a),"undo restores same point identity");AE_EXPECT_TRUE(session.history().redo(doc)&&!read()->point(a),"redo removal");AE_EXPECT_TRUE(session.history().undo(doc),"restore point for archive");
 const auto archive=editor::serializeEditorDocument(doc,0);editor::EditorDocument reopened;AE_EXPECT_TRUE(editor::deserializeEditorDocument(archive,0,reopened),"Path payload survives scene save/reopen");const auto*p=static_cast<const scene::Path*>(reopened.find(id)->components.findInstance(instance));AE_EXPECT_TRUE(p&&p->curve.points.front().id==b&&p->point(a)->out[1]==2&&p->nextPointId>b,"order handles identity allocator persist");
 u64 refused=99;point.position[0]=std::numeric_limits<float>::infinity();AE_EXPECT_TRUE(!session.insertPathPoint(id,instance,0,point,refused)&&refused==0&&read()->curve.points.size()==2,"invalid edit is atomic");
}

namespace {
struct PathInputFixture {
 editor::EditorSession session;ui::UiFont font;ui::UiIconAtlas icons;
 PathInputFixture(){
  const auto root=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
  const auto read=[](const std::filesystem::path&path){std::ifstream in(path,std::ios::binary);return std::vector<u8>(std::istreambuf_iterator<char>(in),{});};
  const auto fontBytes=read(root/"assets/astra-visual/ui/astra-ui-font.aeuf"),iconBytes=read(root/"assets/astra-visual/ui/astra-ui-icons.aeui");
  AE_EXPECT_TRUE(font.load(fontBytes)&&icons.load(iconBytes),"real editor assets");session.initialize(&font,&icons);session.setSurface({0,0,1200,800},{});session.importMap({}, {},false);
 }
 runtime::ObjectId create(){u32 recipe=0;if(!editor::findCreationRecipe("path.curve",&recipe))return 0;const auto id=session.createRecipe(recipe,session.document().root());session.update();return id;}
 const scene::Path*path(runtime::ObjectId id){return static_cast<const scene::Path*>(session.document().find(id)->components.find(scene::Path::descriptor));}
 void tap(u32 widget){
  session.update();ui::UiInputRouter router;ui::UiDrawList list;list.begin(session.screen().surface,font.metrics(ui::UiFontWeight::Regular));editor::buildEditorScreen(session.screen(),ui::defaultTheme(),list,router);
  ui::UiPoint at{-1,-1};for(float y=2;y<session.screen().surface.height&&at.x<0;y+=4)for(float x=2;x<session.screen().surface.width;x+=4){const auto hit=router.hitTest({x,y});if(hit.target==ui::UiPointerTarget::Widget&&hit.widgetId==widget){at={x,y};break;}}
  AE_EXPECT_TRUE(at.x>=0,"Path input widget reachable in executed screen");if(at.x<0)return;session.handlePointer({77,ui::UiPointerPhase::Down,at,0});session.handlePointer({77,ui::UiPointerPhase::Up,at,.1});session.update();
 }
 u32 number(runtime::ObjectId id){const auto*o=session.document().find(id);u32 component=0;for(;component<o->components.size();++component)if(o->components.at(component)->type().id==scene::Path::descriptor.id)break;const auto*p=path(id);u32 slot=0;for(;slot<p->curve.points.size();++slot)if(p->curve.points[slot].id==session.screen().pathPointId)break;return editor::widgetId(editor::EditorWidget::ComponentSlotNumberBase)+component+(slot<<16);}
};
}
AE_TEST(path_input_recipe_seeds_durable_points_and_history_replays_whole_object){
 PathInputFixture f;const auto id=f.create();AE_EXPECT_TRUE(id!=0,"real composed recipe");const auto*p=f.path(id);AE_EXPECT_TRUE(p&&p->curve.points.size()==3&&f.session.screen().pathEditorOpen,"recipe creates three editable points");const auto points=p->curve.points;
 f.tap(editor::widgetId(editor::EditorWidget::Undo));AE_EXPECT_TRUE(!f.session.document().find(id),"router undo removes entire recipe object");f.tap(editor::widgetId(editor::EditorWidget::Redo));AE_EXPECT_TRUE(f.path(id)->curve.points==points,"router redo restores identities positions and handles");
}
AE_TEST(path_input_point_selection_numeric_rejects_stale_reorder_and_removal){
 PathInputFixture f;const auto id=f.create();AE_EXPECT_TRUE(id!=0,"real composed recipe");const auto instance=f.path(id)->instanceId();const auto first=f.path(id)->curve.points[0].id,second=f.path(id)->curve.points[1].id;
 f.tap(editor::widgetId(editor::EditorWidget::PathPointList));f.tap(editor::widgetId(editor::EditorWidget::PathPointSelectBase)+1);AE_EXPECT_TRUE(f.session.screen().pathPointId==second,"list resolves selected point identity");
 f.tap(f.number(id));auto edit=f.session.pendingTextEdit();AE_EXPECT_TRUE(edit.entity==id&&edit.componentInstance==instance&&edit.elementId==second,"keyboard captures object instance and durable point");const auto before=f.path(id)->point(second)->position;
 AE_EXPECT_TRUE(f.session.movePathPoint(id,instance,second,first),"external reorder while keyboard pending");AE_EXPECT_TRUE(!f.session.completeTextEdit(edit,"99",true)&&f.path(id)->point(second)->position==before,"stale revision rejects write without redirecting index");
 f.tap(f.number(id));edit=f.session.pendingTextEdit();AE_EXPECT_TRUE(edit.elementId==second,"selection remains same identity after reorder");AE_EXPECT_TRUE(f.session.removePathPoint(id,instance,second),"remove pending point");AE_EXPECT_TRUE(!f.session.completeTextEdit(edit,"99",true)&&!f.path(id)->point(second),"removed point cannot receive numeric result");
}
AE_TEST(path_input_locked_inspector_edits_exact_object_not_scene_selection){
 PathInputFixture f;const auto locked=f.create(),selected=f.create();AE_EXPECT_TRUE(locked&&selected,"real composed recipes");f.session.setSelection(selected);auto&state=const_cast<editor::EditorScreenState&>(f.session.screen());state.inspectorLocked=locked;f.session.update();state.pathEditorOpen=true;f.session.update();
 f.tap(f.number(locked));const auto edit=f.session.pendingTextEdit();AE_EXPECT_TRUE(edit.entity==locked&&edit.elementId==f.path(locked)->curve.points.front().id,"keyboard belongs to locked inspector");const float other=f.path(selected)->curve.points.front().position[0];AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"42",true),"commit exact inspected point");AE_EXPECT_TRUE(f.path(locked)->curve.points.front().position[0]==42&&f.path(selected)->curve.points.front().position[0]==other,"selection object remains unchanged");
}
AE_TEST(curve3d_authored_roll_transport_closed_seam_and_versioned_payload){
 resources::Curve3D curve;curve.points={{1,{0,0,0},{},{}},{2,{0,0,10},{},{},90}};
 resources::BakedCurve3D baked;resources::BakedCurve3D::Frame frame;
 AE_EXPECT_TRUE(baked.bake(curve)&&baked.sampleFrame(5,frame),"real baked frame");
 AE_EXPECT_TRUE(std::abs(frame.rollDegrees-45)<.001&&std::abs(frame.up[0]+std::sqrt(.5f))<.001&&std::abs(frame.right[1]-std::sqrt(.5f))<.001,"roll affects orthonormal basis");
 curve.up={1,0,0};AE_EXPECT_TRUE(baked.bake(curve)&&baked.sampleFrame(10,frame)&&frame.up[1]>.999,"authored up and roll both consumed");
 curve.up={0,0,0};const double length=baked.length();AE_EXPECT_TRUE(!baked.bake(curve)&&baked.length()==length,"zero up rejects atomically");
 curve.up={0,1,0};curve.points.push_back({3,{3,4,6},{},{},170});curve.closed=true;
 AE_EXPECT_TRUE(baked.bake(curve),"closed spatial curve");resources::BakedCurve3D::Frame first,last;
 AE_EXPECT_TRUE(baked.sampleFrame(0,first)&&baked.sampleFrame(baked.length(),last)&&first.up==last.up&&first.tangent==last.tangent&&first.rollDegrees==last.rollDegrees,"closed frames share exact authored seam");
 scene::Path path;path.curve=curve;path.nextPointId=4;std::stringstream payload;path.write(payload);scene::Path loaded;
 AE_EXPECT_TRUE(loaded.read(payload,2)&&loaded.curve.points==curve.points&&loaded.curve.up==curve.up,"v2 persists identities up and roll");
 std::stringstream legacy("0 3 2 1 0 0 0 0 0 0 0 0 0 2 0 0 10 0 0 0 0 0 0");scene::Path migrated;
 AE_EXPECT_TRUE(migrated.read(legacy,1)&&migrated.curve.up==(resources::CurveVector3{0,1,0})&&migrated.curve.points.back().rollDegrees==0&&migrated.nextPointId==3,"v1 migration preserves allocator and geometry with explicit defaults");
}
AE_TEST(path_input_orientation_roll_up_history_and_archive){
 PathInputFixture f;const auto id=f.create();AE_EXPECT_TRUE(id!=0,"real recipe");const auto instance=f.path(id)->instanceId(),point=f.session.screen().pathPointId;
 f.tap(editor::widgetId(editor::EditorWidget::PathPointOrientation));AE_EXPECT_TRUE(f.session.screen().pathOrientation,"orientation route reached by actual input");
 f.tap(f.number(id)+(9u<<8));auto edit=f.session.pendingTextEdit();AE_EXPECT_TRUE(edit.elementId==point&&edit.componentInstance==instance&&f.session.completeTextEdit(edit,"90",true),"roll edits stable point");
 AE_EXPECT_TRUE(f.path(id)->point(point)->rollDegrees==90,"roll stored by model");
 AE_EXPECT_TRUE(f.session.history().undo(f.session.document())&&f.path(id)->point(point)->rollDegrees==0,"one undo restores roll");
 AE_EXPECT_TRUE(f.session.history().redo(f.session.document()),"redo roll");
 const u32 component=f.number(id)&0xffu;
 f.tap(editor::widgetId(editor::EditorWidget::ComponentNumberBase)+component+(2u<<8));edit=f.session.pendingTextEdit();AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"1",true)&&f.path(id)->curve.up[2]==1,"global up edits through executed orientation surface");
 f.tap(editor::widgetId(editor::EditorWidget::ComponentNumberBase)+component+(1u<<8));edit=f.session.pendingTextEdit();AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"0",true)&&f.path(id)->curve.up[1]==0,"up remains valid after switching axis");
 f.tap(editor::widgetId(editor::EditorWidget::ComponentNumberBase)+component+(2u<<8));edit=f.session.pendingTextEdit();AE_EXPECT_TRUE(!f.session.completeTextEdit(edit,"0",true)&&f.path(id)->curve.up[2]==1,"zero up refused from real numeric editor");f.session.completeTextEdit(edit,"",false);
 const auto snapshot=editor::serializeEditorDocument(f.session.document(),0);editor::EditorDocument reopened;
 AE_EXPECT_TRUE(editor::deserializeEditorDocument(snapshot,0,reopened)&&static_cast<const scene::Path*>(reopened.find(id)->components.findInstance(instance))->point(point)->rollDegrees==90,"authored roll survives reopen");
 f.tap(editor::widgetId(editor::EditorWidget::PathPointTangents));AE_EXPECT_TRUE(f.session.screen().pathTangents&&!f.session.screen().pathOrientation,"geometry route remains available");
}
