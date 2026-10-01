#include "harness.h"
#include "runtime/scene_paths.h"
#include "scene/path.h"
#include "scene/path_follow.h"
#include "scene/camera_follow.h"
#include "runtime/scene_physics2d.h"
#include "scene/physics2d_components.h"
#include "editor/editor_component_visuals.h"
#include <sstream>
#include <functional>
using namespace ae;using namespace ae::runtime;
namespace {
ObjectId line(SceneGraph&g,float x,float y,float z){const auto id=g.createEntity(g.root(),ObjectKind::Folder,"Path");auto object=*g.find(id);auto *path=static_cast<scene::Path*>(object.components.add(scene::Path::descriptor));resources::CurvePoint3D point;u64 pid=0;path->insertPoint(0,point,pid);point.position={x,y,z};path->insertPoint(0,point,pid);g.applyEntityValues(id,object);return id;}
ObjectId follower(SceneGraph&g,ObjectId path){const auto id=g.createEntity(g.root(),ObjectKind::Folder,"Follow");auto object=*g.find(id);auto*c=static_cast<scene::PathFollow*>(object.components.add(scene::PathFollow::descriptor));c->target=path;g.applyEntityValues(id,object);return id;}
void configure(GameWorld&w,ObjectId id,const std::function<void(scene::PathFollow&)>&edit){auto object=*w.graph().find(id);auto *old=object.components.find(scene::PathFollow::descriptor);auto copy=old->clone();edit(static_cast<scene::PathFollow&>(*copy));object.components.replaceInstance(old->instanceId(),*copy);w.poseGraph().applyEntityValues(id,object);}
bool near(float a,float b){return std::abs(a-b)<.03f;}
}
AE_TEST(paths_world_distance_orientation_offsets_scale_and_live_geometry){
 SceneGraph graph;const auto path=line(graph,10,0,0),owner=follower(graph,path);auto object=*graph.find(path);object.transform.scale[0]=2;object.transform.position[2]=5;graph.applyEntityValues(path,object);
 GameWorld world;AE_EXPECT_TRUE(world.load(graph),"typed path scene");ScenePaths paths;
 configure(world,owner,[](auto&c){c.speed=2;c.offset[0]=1;c.offset[1]=2;});
 AE_EXPECT_TRUE(paths.advance(world,1)&&paths.diagnostics().empty(),"world curve evaluator");Transform pose;world.worldTransform(world.handle(owner),pose);
 AE_EXPECT_TRUE(near(pose.position[0],2)&&near(pose.position[1],2)&&near(pose.position[2],4),"distance world speed and tangent frame offsets");float matrix[16];transformMatrix(pose,matrix);AE_EXPECT_TRUE(matrix[8]>.99f,"+Z oriented world X");double length=0;AE_EXPECT_TRUE(paths.length(world,path,length)&&std::abs(length-20)<.001,"world scale included in length");
 object=*world.graph().find(path);object.transform.scale[0]=3;world.poseGraph().applyEntityValues(path,object);paths.advance(world,1);world.worldTransform(world.handle(owner),pose);AE_EXPECT_TRUE(near(pose.position[0],4),"scale edit preserves world speed and current distance");
 object=*world.graph().find(path);auto *old=object.components.find(scene::Path::descriptor);const auto pathInstance=old->instanceId();auto copy=old->clone();auto &changed=static_cast<scene::Path&>(*copy);auto point=changed.curve.points.back();point.position[0]=20;changed.editPoint(point.id,point);object.components.replaceInstance(pathInstance,changed);world.poseGraph().applyEntityValues(path,object);AE_EXPECT_TRUE(paths.length(world,path,length)&&std::abs(length-60)<.001,"point edit invalidates cached world bake");
 const auto validCurve=changed.curve;
 changed.curve.points.clear();object.components.replaceInstance(pathInstance,changed);world.poseGraph().applyEntityValues(path,object);
 AE_EXPECT_TRUE(paths.advance(world,1)&&!paths.diagnostics().empty()&&paths.diagnostics()[0].issue==ScenePaths::Issue::InvalidCurve,"invalid live geometry holds follower with diagnostic");
 AE_EXPECT_TRUE(!paths.length(world,path,length),"invalid geometry never returns old cached length");
 changed.curve=validCurve;object.components.replaceInstance(pathInstance,changed);world.poseGraph().applyEntityValues(path,object);
 AE_EXPECT_TRUE(paths.advance(world,1)&&paths.diagnostics().empty(),"corrected geometry retries and recovers");
 world.worldTransform(world.handle(owner),pose);AE_EXPECT_TRUE(near(pose.position[0],6),"failed frame does not consume progress and corrected frame resumes");
}
AE_TEST(paths_duration_loop_reverse_pause_restart_and_world_lifetime){
 SceneGraph graph;const auto path=line(graph,10,0,0),owner=follower(graph,path);GameWorld world;AE_EXPECT_TRUE(world.load(graph),"path scene");ScenePaths paths;
 AE_EXPECT_TRUE(paths.stop(world,owner),"Stop valid before first advance");paths.advance(world,1);double distance;paths.progress(world,owner,distance);AE_EXPECT_TRUE(distance==0,"stopped initial pose samples without advance");
 configure(world,owner,[](auto&c){c.mode=scene::PathFollowMode::Duration;c.duration=2;c.loop=true;});AE_EXPECT_TRUE(paths.restart(world,owner),"restart before any script step");paths.advance(world,2.5);paths.progress(world,owner,distance);AE_EXPECT_TRUE(std::abs(distance-2.5)<.01,"duration and wrap world length");
 world.setActive(world.handle(owner),false);paths.advance(world,2);world.setActive(world.handle(owner),true);paths.advance(world,.1);paths.progress(world,owner,distance);AE_EXPECT_TRUE(std::abs(distance-3)<.01,"inactive freezes and reentry resumes");
 configure(world,owner,[](auto&c){c.backwards=true;c.progressDistance=8;c.loop=false;});paths.advance(world,1);paths.progress(world,owner,distance);AE_EXPECT_TRUE(std::abs(distance-8)<.01,"initial distance edit publishes without consuming dt");paths.advance(world,1);paths.progress(world,owner,distance);AE_EXPECT_TRUE(std::abs(distance-3)<.01,"reverse subtracts duration distance");paths.stop(world,owner);paths.advance(world,1);paths.progress(world,owner,distance);AE_EXPECT_TRUE(std::abs(distance-3)<.01,"stop freezes live progress");
 const auto handle=world.handle(owner);world.destroyObject(handle);world.flush();paths.advance(world,0);AE_EXPECT_TRUE(!paths.progress(world,owner,distance),"removed follower cache retired");world.clear();AE_EXPECT_TRUE(world.load(graph),"fresh world same ids");paths.advance(world,1);paths.progress(world,owner,distance);AE_EXPECT_TRUE(std::abs(distance-1)<.01,"new world resets runtime state");
}
AE_TEST(paths_reference_remap_persistence_conflicts_cycles_and_vertical_diagnostics){
 SceneGraph graph;const auto path=line(graph,0,10,0),owner=follower(graph,path);GameWorld world;AE_EXPECT_TRUE(world.load(graph),"vertical curve");ScenePaths paths;paths.advance(world,1);AE_EXPECT_TRUE(paths.diagnostics().empty(),"transport frame resolves parallel initial up with documented axis convention");
 configure(world,owner,[](auto&c){c.orient=false;});paths.advance(world,1);Transform pose;world.worldTransform(world.handle(owner),pose);AE_EXPECT_TRUE(near(pose.position[1],2),"rotation disabled still follows transported frame");
 auto object=*world.graph().find(owner);auto *camera=static_cast<scene::CameraFollow*>(object.components.add(scene::CameraFollow::descriptor));camera->target=path;world.poseGraph().applyEntityValues(owner,object);paths.advance(world,1);AE_EXPECT_TRUE(paths.diagnostics()[0].issue==ScenePaths::Issue::CompetingWriter,"camera writer conflict");
 object=*graph.find(owner);ObjectCloneMap map{{path,72},{owner,73}};AE_EXPECT_TRUE(remapObjectReferences(object,map),"reference follows clone");const auto *c=static_cast<const scene::PathFollow*>(object.components.find(scene::PathFollow::descriptor));std::stringstream payload;c->write(payload);scene::PathFollow restored;AE_EXPECT_TRUE(restored.read(payload,1)&&restored.target==72,"typed authoring roundtrip");
 SceneGraph cyclic;const auto parent=cyclic.createEntity(cyclic.root(),ObjectKind::Folder,"Owner");auto target=line(cyclic,10,0,0);AE_EXPECT_TRUE(cyclic.reparent(target,parent,0),"path child of follower");auto value=*cyclic.find(parent);auto *follow=static_cast<scene::PathFollow*>(value.components.add(scene::PathFollow::descriptor));follow->target=target;cyclic.applyEntityValues(parent,value);world.clear();AE_EXPECT_TRUE(world.load(cyclic),"cycle draft load");paths.advance(world,1);AE_EXPECT_TRUE(paths.diagnostics()[0].issue==ScenePaths::Issue::Cycle,"dependency cycle never writes pose");
 SceneGraph physical;const auto track=line(physical,10,0,0),ancestor=follower(physical,track);
 const auto child=physical.createEntity(ancestor,ObjectKind::Folder,"Simulated child");
 auto body=*physical.find(child);body.components.add(scene::Collider2D::descriptor);body.components.add(scene::Body2D::descriptor);physical.applyEntityValues(child,body);
 world.clear();AE_EXPECT_TRUE(world.load(physical),"follower parent of physical body");ScenePhysics2D physics;
 AE_EXPECT_TRUE(physics.start(world),"real backend acquires child authority");paths.advance(world,1);
 AE_EXPECT_TRUE(!paths.diagnostics().empty()&&paths.diagnostics()[0].issue==ScenePaths::Issue::Authority,"subtree physics refusal identified as Authority");physics.stop(&world);
}

#include "editor/editor_archive.h"
#include "editor/editor_play_scene.h"
AE_TEST(paths_live_roll_up_frame_offsets_and_pose_are_same_consumer){
 SceneGraph graph;const auto path=line(graph,0,0,10),owner=follower(graph,path);auto authored=*graph.find(path);auto*c=static_cast<scene::Path*>(authored.components.edit(scene::Path::descriptor));c->curve.points.back().rollDegrees=90;graph.applyEntityValues(path,authored);
 GameWorld world;AE_EXPECT_TRUE(world.load(graph),"authored tilted path");ScenePaths paths;configure(world,owner,[](auto&v){v.progressDistance=5;v.autoplay=false;v.offset[0]=1;});
 AE_EXPECT_TRUE(paths.advance(world,0)&&paths.diagnostics().empty(),"frame controls actual follower");Transform pose;world.worldTransform(world.handle(owner),pose);float matrix[16];transformMatrix(pose,matrix);
 AE_EXPECT_TRUE(near(pose.position[0],std::sqrt(.5f))&&near(pose.position[1],std::sqrt(.5f))&&near(pose.position[2],5)&&near(matrix[4],-std::sqrt(.5f)),"roll changes orientation and lateral offset");
 const auto handle=world.findComponent(world.handle(path),scene::Path::descriptor.id);
 AE_EXPECT_TRUE(world.setSlotProperty(handle,"point_roll",0,90.f)==WorldStatus::Ok&&paths.advance(world,0),"live roll edit invalidates frame bake");world.worldTransform(world.handle(owner),pose);AE_EXPECT_TRUE(near(pose.position[0],0)&&near(pose.position[1],1),"live geometry cache never returns old roll");
 const float up[3]{1,0,0},zero[3]{};AE_EXPECT_TRUE(world.setTriple(handle,"up",up)==WorldStatus::Ok&&paths.advance(world,0),"atomic local up mutation");world.worldTransform(world.handle(owner),pose);AE_EXPECT_TRUE(near(pose.position[0],1)&&near(pose.position[1],0),"up changes actual frame offset");
 AE_EXPECT_TRUE(world.setTriple(handle,"up",zero)!=WorldStatus::Ok,"zero up refused");resources::BakedCurve3D::Frame frame;
 AE_EXPECT_TRUE(paths.sampleFrame(world,path,5,frame)&&frame.up[1]>.99,"refused mutation preserves prior model/cache");
 configure(world,owner,[](auto&v){v.backwards=true;});paths.advance(world,0);world.worldTransform(world.handle(owner),pose);transformMatrix(pose,matrix);AE_EXPECT_TRUE(near(pose.position[0],-1)&&matrix[10]<-.99,"reverse keeps authored up and reverses tangent/right");
 auto scaled=*world.graph().find(path);scaled.transform.scale[0]=2;scaled.transform.scale[1]=3;scaled.transform.scale[2]=4;scaled.transform.rotationDegrees[2]=25;world.poseGraph().applyEntityValues(path,scaled);
 double length=0;AE_EXPECT_TRUE(paths.length(world,path,length)&&paths.sampleFrame(world,path,length*.5,frame),"world frame under rotated nonuniform scale");
 const auto visuals=editor::collectComponentVisuals(world.graph(),path,1.f);bool matched=false;
 for(const auto&visual:visuals)if(visual.entity==path)for(const auto&segment:visual.segments){bool same=true;for(u32 a=0;a<3;++a)same=same&&std::abs(segment.a[a]-frame.position[a])<.001&&std::abs(segment.b[a]-frame.position[a]-.28f*frame.up[a])<.001;if(same)matched=true;}
 AE_EXPECT_TRUE(matched,"executed viewport orientation marker equals runtime frame under nonuniform scale");
}
AE_TEST(paths_editor_archive_duration_inactive_stop_restart_preserves_authoring){
 using namespace ae::editor;
 EditorDocument document;
 const auto path=line(document,10,0,0),owner=follower(document,path);
 auto object=*document.find(owner);
 auto *old=object.components.find(scene::PathFollow::descriptor);
 auto copy=old->clone();
 auto &follow=static_cast<scene::PathFollow&>(*copy);
 follow.mode=scene::PathFollowMode::Duration;follow.duration=2;
 object.components.replaceInstance(old->instanceId(),follow);
 document.applyEntityValues(owner,object);
 const auto saved=serializeEditorDocument(document,91);
 EditorDocument reopened;
 AE_EXPECT_TRUE(deserializeEditorDocument(saved,91,reopened),"archive preserves duration and path identities");
 EditorMapScene resources;EditorPlayScene play;
 AE_EXPECT_TRUE(play.start(reopened,resources),"integrated path world starts");
 double distance=0;
 AE_EXPECT_TRUE(play.paths().progress(play.world(),owner,distance)&&distance==0,"progress readable before first frame");
 for(u32 frame=0;frame<10;++frame){AE_EXPECT_TRUE(play.advance(.1),"shared lifecycle advances duration");}
 Transform pose;play.world().worldTransform(play.world().handle(owner),pose);
 AE_EXPECT_TRUE(near(pose.position[0],5),"duration moves halfway through path");
 play.world().setActive(play.world().handle(owner),false);
 for(u32 frame=0;frame<3;++frame){AE_EXPECT_TRUE(play.advance(.1),"inactive lifecycle remains valid");}
 play.paths().progress(play.world(),owner,distance);
 AE_EXPECT_TRUE(std::abs(distance-5)<.03,"inactive freezes live time");
 play.world().setActive(play.world().handle(owner),true);
 AE_EXPECT_TRUE(play.paths().stop(play.world(),owner)&&play.advance(.1),"Stop holds active follower");
 bool playing=true;
 AE_EXPECT_TRUE(play.paths().playing(play.world(),owner,playing)&&!playing,"status reflects real Stop");
 play.paths().progress(play.world(),owner,distance);
 AE_EXPECT_TRUE(std::abs(distance-5)<.03,"Stop retains progress");
 AE_EXPECT_TRUE(play.paths().restart(play.world(),owner)&&play.advance(0),"Restart publishes initial sample");
 play.world().worldTransform(play.world().handle(owner),pose);
 AE_EXPECT_TRUE(near(pose.position[0],0)&&play.paths().playing(play.world(),owner,playing)&&playing,"Restart returns to initial distance and resumes real state");
 AE_EXPECT_EQ(serializeEditorDocument(reopened,91),saved,"runtime time pose and active edits leave authoring unchanged");
 play.stop();
 AE_EXPECT_TRUE(!play.world().running()&&play.start(reopened,resources),"Stop destroys session and reopen starts fresh");
 play.paths().progress(play.world(),owner,distance);
 AE_EXPECT_TRUE(distance==0,"fresh session initial progress");
 play.stop();
}
