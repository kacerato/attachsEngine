#include "harness.h"
#include "runtime/scene_constraints.h"
#include "editor/editor_session.h"
#include "editor/editor_archive.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_component_visuals.h"
#include <sstream>
using namespace ae;
using namespace ae::runtime;
namespace {
template<class T> void attach(SceneGraph &g,ObjectId id,ObjectId source){auto value=*g.find(id);auto *c=static_cast<T*>(value.components.add(T::descriptor));c->target=source;g.applyEntityValues(id,value);}
bool near(float a,float b){return std::abs(a-b)<.001f;}
}
AE_TEST(constraints_parent_local_offset_rotation_without_scale_and_lookat_roll) {
 SceneGraph g;auto source=g.createEntity(g.root(),ObjectKind::Folder,"Source");auto owner=g.createEntity(g.root(),ObjectKind::Folder,"Follower");Transform p;p.position[0]=3;p.rotationDegrees[1]=90;p.scale[0]=4;g.setTransform(source,p);attach<scene::ParentConstraint>(g,owner,source);
 auto v=*g.find(owner);auto*c=static_cast<scene::ParentConstraint*>(v.components.edit(scene::ParentConstraint::descriptor));c->offset[2]=2;c->rotationOffset[2]=30;g.applyEntityValues(owner,v);
 std::stringstream payload;c->write(payload);scene::ParentConstraint restored;AE_EXPECT_TRUE(restored.read(payload,1)&&near(restored.rotationOffset[2],30),"parent payload preserves separate offsets");
 GameWorld w;AE_EXPECT_TRUE(w.load(g),"load parent");SceneConstraints runtime;runtime.advance(w);Transform out;w.worldTransform(w.handle(owner),out);AE_EXPECT_TRUE(near(out.position[0],5)&&near(out.scale[0],1),"rotated source-local offset does not inherit scale");
 for(u32 i=0;i<4;++i){runtime.advance(w);}w.worldTransform(w.handle(owner),out);AE_EXPECT_TRUE(near(out.position[0],5),"parent rest does not creep");
 SceneGraph look;source=look.createEntity(look.root(),ObjectKind::Folder,"Source");owner=look.createEntity(look.root(),ObjectKind::Folder,"Look");p=Transform{};p.position[2]=10;look.setTransform(source,p);attach<scene::LookAtConstraint>(look,owner,source);v=*look.find(owner);static_cast<scene::LookAtConstraint*>(v.components.edit(scene::LookAtConstraint::descriptor))->roll=45;look.applyEntityValues(owner,v);
 GameWorld lw;AE_EXPECT_TRUE(lw.load(look),"load lookat");runtime.reset();runtime.advance(lw);lw.worldTransform(lw.handle(owner),out);AE_EXPECT_TRUE(near(out.rotationDegrees[2],45),"lookat fixed +Z with roll");
}
AE_TEST(new_component_gizmos_use_offsets_local_destinations_and_unscaled_audio_ranges){
 using namespace ae::editor;EditorDocument doc;auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Gizmos");auto value=*doc.find(id);value.transform.position[0]=5;value.transform.scale[0]=value.transform.scale[1]=value.transform.scale[2]=3;
 auto*collider=static_cast<scene::Collider2D*>(value.components.add(scene::Collider2D::descriptor));collider->offsetX=1;
 auto*tween=static_cast<scene::TransformTween*>(value.components.add(scene::TransformTween::descriptor));tween->relative=true;tween->destination[0]=2;
 auto*audio=static_cast<scene::AudioSource*>(value.components.add(scene::AudioSource::descriptor));audio->dimension=scene::AudioDimension::Spatial;audio->maxDistance=4;
 auto*joint=static_cast<scene::Joint2D*>(value.components.add(scene::Joint2D::descriptor));joint->worldAnchor=true;joint->anchorAX=1;joint->anchorBX=10;
 AE_EXPECT_TRUE(doc.applyEntityValues(id,value),"real authoring");auto visuals=collectComponentVisuals(doc,id,1);
 bool shape=false,destination=false,range=false,anchors=false;
 for(const auto&v:visuals){if(v.icon==ui::UiIcon::PhysicsCollider2d)shape=v.segments.size()==4&&near(v.segments[0].a[0],6.5f);if(v.icon==ui::UiIcon::ComponentTweenTransform)destination=std::any_of(v.segments.begin(),v.segments.end(),[](const auto&s){return near(s.a[0],5)&&near(s.b[0],7);});if(v.icon==ui::UiIcon::AudioSource)range=std::any_of(v.segments.begin(),v.segments.end(),[](const auto&s){return near(s.a[0],9);});if(v.icon==ui::UiIcon::PhysicsJoint2d)anchors=std::any_of(v.segments.begin(),v.segments.end(),[](const auto&s){return near(s.a[0],8)&&near(s.b[0],10);});}
 AE_EXPECT_TRUE(shape&&destination&&range&&anchors,"geometry uses collider offset, local tween target, world audio metres and true joint anchors");
}
AE_TEST(constraints_weight_axes_dependencies_and_physics_authority) {
 SceneGraph graph;auto source=graph.createEntity(graph.root(),ObjectKind::Folder,"Source");auto first=graph.createEntity(graph.root(),ObjectKind::Folder,"First");auto second=graph.createEntity(graph.root(),ObjectKind::Folder,"Second");
 Transform pose;pose.position[0]=10;pose.position[1]=8;graph.setTransform(source,pose);
 attach<scene::PositionConstraint>(graph,first,source);attach<scene::PositionConstraint>(graph,second,first);
 auto authored=*graph.find(first);const auto *c=authored.components.find(scene::PositionConstraint::descriptor);
 auto copy=c->clone();auto &settings=static_cast<scene::PositionConstraint&>(*copy);settings.weight=.5f;settings.axis[1]=false;settings.offset[0]=2;authored.components.replaceInstance(c->instanceId(),settings);graph.applyEntityValues(first,authored);
 GameWorld world;AE_EXPECT_TRUE(world.load(graph),"load typed constraints");SceneConstraints runtime;
 AE_EXPECT_TRUE(runtime.advance(world),"evaluate dependency chain");Transform result;world.worldTransform(world.handle(first),result);
 AE_EXPECT_TRUE(near(result.position[0],6)&&near(result.position[1],0),"weight offset selected world axes");
 for(u32 frame=0;frame<10;++frame){runtime.advance(world);world.worldTransform(world.handle(first),result);AE_EXPECT_TRUE(near(result.position[0],6),"fractional weight does not creep");}
 pose.position[0]=14;world.setWorldTransform(world.handle(source),pose);runtime.advance(world);world.worldTransform(world.handle(first),result);AE_EXPECT_TRUE(near(result.position[0],8),"moving target blends from rest rather than last output");
 world.worldTransform(world.handle(second),result);AE_EXPECT_TRUE(near(result.position[0],8),"source evaluated first");
 world.setAuthority(first,TransformAuthority::PhysicsBody);pose.position[0]=20;world.setWorldTransform(world.handle(source),pose);runtime.advance(world);world.worldTransform(world.handle(first),result);AE_EXPECT_TRUE(near(result.position[0],8),"physics owns pose");
 // A moving unconstrained parent changes rest in world space, without treating
 // the previous constrained local output as new authoring input.
 SceneGraph parented;const auto parent=parented.createEntity(parented.root(),ObjectKind::Folder,"Pai");
 const auto target=parented.createEntity(parented.root(),ObjectKind::Folder,"Fonte");
 const auto child=parented.createEntity(parent,ObjectKind::Folder,"Filho");
 pose=Transform{};pose.position[0]=10;parented.setTransform(target,pose);attach<scene::PositionConstraint>(parented,child,target);
 auto childValue=*parented.find(child);static_cast<scene::PositionConstraint*>(childValue.components.edit(scene::PositionConstraint::descriptor))->weight=.5f;parented.applyEntityValues(child,childValue);
 GameWorld moving;AE_EXPECT_TRUE(moving.load(parented),"parented constraint");SceneConstraints evaluated;
 evaluated.advance(moving);pose=Transform{};pose.position[0]=4;moving.setWorldTransform(moving.handle(parent),pose);
 evaluated.advance(moving);moving.worldTransform(moving.handle(child),result);AE_EXPECT_TRUE(near(result.position[0],7),"world rest follows moving parent without output feedback");
 pose.position[0]=6;moving.setWorldTransform(moving.handle(parent),pose);evaluated.advance(moving);moving.worldTransform(moving.handle(child),result);
 AE_EXPECT_TRUE(near(result.position[0],8),"next parent motion remains stable");
}
AE_TEST(constraints_cycles_clone_and_serialization) {
 SceneGraph graph;auto group=graph.createEntity(graph.root(),ObjectKind::Folder,"Group");auto a=graph.createEntity(group,ObjectKind::Folder,"A");auto b=graph.createEntity(group,ObjectKind::Folder,"B");
 attach<scene::RotationConstraint>(graph,a,b);attach<scene::RotationConstraint>(graph,b,a);
 ObjectCloneMap mapping;graph.cloneSubtree(group,graph.root(),mapping);const auto *cloned=static_cast<const scene::RotationConstraint*>(graph.find(mapping[a])->components.find(scene::RotationConstraint::descriptor));AE_EXPECT_TRUE(cloned&&cloned->target==mapping[b],"clone remaps source through reflection");
 std::stringstream data;cloned->write(data);scene::RotationConstraint roundtrip;AE_EXPECT_TRUE(roundtrip.read(data,1)&&roundtrip.target==cloned->target,"typed versioned payload roundtrip");
 GameWorld world;AE_EXPECT_TRUE(world.load(graph),"cycle draft remains loadable");SceneConstraints runtime;AE_EXPECT_TRUE(runtime.advance(world)&&!runtime.diagnostics().empty(),"cycle diagnosed without stopping play");
 AE_EXPECT_TRUE(runtime.diagnostics().front().issue==SceneConstraints::Issue::Cycle,"explicit cycle diagnosis");
}
AE_TEST(constraints_aim_scale_and_degenerate_source) {
 SceneGraph graph;auto source=graph.createEntity(graph.root(),ObjectKind::Folder,"Source");auto owner=graph.createEntity(graph.root(),ObjectKind::Folder,"Owner");Transform pose;pose.position[0]=10;pose.scale[0]=3;graph.setTransform(source,pose);
 attach<scene::AimConstraint>(graph,owner,source);attach<scene::ScaleConstraint>(graph,owner,source);
 GameWorld world;AE_EXPECT_TRUE(world.load(graph),"load aim and scale");SceneConstraints runtime;runtime.advance(world);Transform result;world.worldTransform(world.handle(owner),result);float matrix[16];transformMatrix(result,matrix);
 AE_EXPECT_TRUE(near(result.scale[0],3)&&matrix[8]>.99f&&std::abs(matrix[10])<.001f,"+Z aims at world X and scale copies source");
 const float priorAngle=result.rotationDegrees[1];world.setWorldTransform(world.handle(source),Transform{});runtime.advance(world);AE_EXPECT_TRUE(!runtime.diagnostics().empty(),"coincident aim diagnosed");world.worldTransform(world.handle(owner),result);AE_EXPECT_TRUE(near(result.rotationDegrees[1],priorAngle),"invalid aim preserves prior pose");
}

AE_TEST(constraints_aim_all_local_axes) {
 for(u32 axis=0;axis<6;++axis){
  SceneGraph graph;auto source=graph.createEntity(graph.root(),ObjectKind::Folder,"Source");auto owner=graph.createEntity(graph.root(),ObjectKind::Folder,"Owner");Transform pose;pose.position[0]=4;pose.position[1]=2;pose.position[2]=7;graph.setTransform(source,pose);attach<scene::AimConstraint>(graph,owner,source);
  auto value=*graph.find(owner);const auto *c=value.components.find(scene::AimConstraint::descriptor);auto copy=c->clone();static_cast<scene::AimConstraint&>(*copy).aimAxis=axis;value.components.replaceInstance(c->instanceId(),*copy);graph.applyEntityValues(owner,value);
  GameWorld world;AE_EXPECT_TRUE(world.load(graph),"load selected aim axis");SceneConstraints runtime;runtime.advance(world);Transform result;world.worldTransform(world.handle(owner),result);float matrix[16];transformMatrix(result,matrix);const float length=std::sqrt(69.f);const float sign=axis<3?1.f:-1.f;
  AE_EXPECT_TRUE(near(matrix[(axis%3)*4]*sign,4/length)&&near(matrix[(axis%3)*4+1]*sign,2/length)&&near(matrix[(axis%3)*4+2]*sign,7/length),"selected signed local axis follows world direction");
 }
}

AE_TEST(constraints_editor_recipes_history_archive_and_play_use_real_sources) {
 using namespace ae::editor;
 EditorSession session;auto &document=session.document();
 const auto source=document.createEntity(document.root(),ObjectKind::Folder,"Fonte");
 Transform pose;pose.position[0]=5;pose.position[2]=4;pose.scale[0]=2;pose.rotationDegrees[1]=30;
 AE_EXPECT_TRUE(document.setTransform(source,pose),"source pose");
 const char *names[]{"constraint.position","constraint.rotation","constraint.scale","constraint.aim"};
 std::array<ObjectId,4> objects{};
 for(u32 k=0;k<4;++k) {
  session.setSelection(source);u32 recipe=0;AE_EXPECT_TRUE(findCreationRecipe(names[k],&recipe),"recipe exists");
  objects[k]=session.createRecipe(recipe,document.root());AE_EXPECT_TRUE(objects[k]!=0,"creation from selected source");
  const auto visuals=collectComponentVisuals(document,objects[k],1.5f);
  AE_EXPECT_TRUE(std::any_of(visuals.begin(),visuals.end(),[&](const auto &v){return v.entity==objects[k] && v.segments.size()==4;}),"axis frame and source link gizmo");
 }
 const auto saved=serializeEditorDocument(document,89);EditorDocument reopened;
 AE_EXPECT_TRUE(deserializeEditorDocument(saved,89,reopened),"all types save/reopen");
 AE_EXPECT_TRUE(session.history().undo(document) && !document.exists(objects.back()),"undo complete creation");
 AE_EXPECT_TRUE(session.history().redo(document) && document.exists(objects.back()),"redo restores reference");
 GameWorld world;AE_EXPECT_TRUE(world.load(reopened),"load authoring");SceneConstraints runtime;
 AE_EXPECT_TRUE(runtime.advance(world) && runtime.diagnostics().empty(),"four consumers in same scene");
 Transform output;world.worldTransform(world.handle(objects[0]),output);AE_EXPECT_TRUE(near(output.position[0],5),"position consumer");
 world.worldTransform(world.handle(objects[1]),output);AE_EXPECT_TRUE(near(output.rotationDegrees[1],30),"rotation consumer");
 world.worldTransform(world.handle(objects[2]),output);AE_EXPECT_TRUE(near(output.scale[0],2),"scale consumer");
 AE_EXPECT_TRUE(reopened.find(objects[0])->transform.position[0]!=5,"Play leaves authoring intact");
}
