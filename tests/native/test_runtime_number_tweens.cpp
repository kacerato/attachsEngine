#include "harness.h"
#include "runtime/scene_number_tweens.h"
#include "runtime/scene_lights.h"
#include "scene/light.h"
#include "scene/camera.h"
#include "editor/editor_scene_camera.h"
#include <chrono>
#include <cstdio>
using namespace ae;
using namespace ae::runtime;

AE_TEST(number_tween_real_light_camera_clock_pause_cancel_and_authoring_boundary) {
  SceneGraph graph;const auto id=graph.createEntity(graph.root(),ObjectKind::Folder,"Light");auto source=*graph.find(id);auto*light=static_cast<scene::Light*>(source.components.add(scene::Light::descriptor));light->intensity=0;const auto instance=light->instanceId();graph.applyEntityValues(id,source);
  const auto cameraId=graph.createEntity(graph.root(),ObjectKind::Folder,"Camera");auto cameraSource=*graph.find(cameraId);auto*camera=static_cast<scene::Camera*>(cameraSource.components.add(scene::Camera::descriptor));camera->verticalFov=60;const auto cameraInstance=camera->instanceId();graph.applyEntityValues(cameraId,cameraSource);
  GameWorld world;AE_EXPECT_TRUE(world.load(graph),"real world");SceneNumberTweens tweens;u64 track=0,duplicate=0,cameraTrack=0;
  AE_EXPECT_TRUE(tweens.create(world,{world.handle(id),instance},"intensity",8,1,0,true,track)==WorldStatus::Ok,"eligible native track");
  AE_EXPECT_TRUE(tweens.create(world,{world.handle(id),instance},"intensity",4,1,0,false,duplicate)==WorldStatus::PropertyAlreadyTweening&&duplicate==0,"duplicate rejected before write");
  AE_EXPECT_TRUE(tweens.create(world,{world.handle(cameraId),cameraInstance},"vertical_fov",100,1,0,false,cameraTrack)==WorldStatus::Ok,"perspective consumer");
  tweens.advance(world,0,.25);SceneNumberTweens::State state;tweens.command(world,track,0,state);AE_EXPECT_TRUE(state.value==2&&state.elapsed==.25,"unscaled advances while scaled clock frozen");
  tweens.command(world,track,1,state);tweens.advance(world,.25,.25);tweens.command(world,track,0,state);AE_EXPECT_TRUE(state.value==2&&state.paused,"pause preserves value and elapsed");
  tweens.command(world,track,2,state);tweens.advance(world,.25,.25);tweens.advance(world,.25,.25);tweens.advance(world,.25,.25);
  AE_EXPECT_TRUE(tweens.command(world,track,0,state)==WorldStatus::Ok&&state.status==SceneNumberTweens::Status::Completed&&state.value==8,"exact terminal endpoint");
  std::vector<renderer::SceneLight> lights;AE_EXPECT_TRUE(collectSceneLights(world.graph(),lights)&&lights.size()==1&&lights[0].intensity==scene::lightIntensityForShader(light->kind,light->unit,8,light->innerAngle,light->outerAngle),"actual light extraction consumes tweened value in authored units");
  const auto *liveCamera=static_cast<const scene::Camera*>(world.readComponent({world.handle(cameraId),cameraInstance}));AE_EXPECT_TRUE(liveCamera&&liveCamera->verticalFov==100,"scaled camera reaches endpoint through real property");
  AE_EXPECT_TRUE(editor::resolveSceneCamera(world.graph(),cameraId).verticalFov==100,"render camera resolver consumes interpolated lens");
  AE_EXPECT_TRUE(static_cast<const scene::Light*>(graph.find(id)->components.findInstance(instance))->intensity==0&&static_cast<const scene::Camera*>(graph.find(cameraId)->components.findInstance(cameraInstance))->verticalFov==60,"source authoring unchanged");
  AE_EXPECT_TRUE(tweens.create(world,{world.handle(id),instance},"intensity",0,1,0,false,duplicate)==WorldStatus::Ok,"completed track releases writer ownership");tweens.advance(world,.25,.25);tweens.command(world,duplicate,3,state);const auto cancelled=state.value;tweens.advance(world,.25,.25);tweens.command(world,duplicate,0,state);AE_EXPECT_TRUE(state.status==SceneNumberTweens::Status::Cancelled&&state.value==cancelled,"cancel preserves real field");
}
AE_TEST(number_tween_rgb_easing_orthographic_lens_and_dynamic_eligibility) {
  SceneGraph graph;const auto id=graph.createEntity(graph.root(),ObjectKind::Folder,"RGB");auto value=*graph.find(id);auto*light=static_cast<scene::Light*>(value.components.add(scene::Light::descriptor));const auto instance=light->instanceId();graph.applyEntityValues(id,value);
  const auto cameraId=graph.createEntity(graph.root(),ObjectKind::Folder,"Ortho");auto cameraValue=*graph.find(cameraId);auto*camera=static_cast<scene::Camera*>(cameraValue.components.add(scene::Camera::descriptor));camera->projection=scene::CameraProjection::Orthographic;const auto cameraInstance=camera->instanceId();graph.applyEntityValues(cameraId,cameraValue);
  GameWorld world;SceneNumberTweens tweens;AE_EXPECT_TRUE(world.load(graph),"real qualified components");u64 tracks[4]{},rejected=0;
  const char*properties[]{"color.r","color.g","color.b"};for(u32 i=0;i<3;++i)AE_EXPECT_TRUE(tweens.create(world,{world.handle(id),instance},properties[i],0,1,i+1,false,tracks[i])==WorldStatus::Ok,"independent RGB writers coexist");
  AE_EXPECT_TRUE(tweens.create(world,{world.handle(cameraId),cameraInstance},"vertical_fov",80,1,0,false,rejected)==WorldStatus::PropertyNotTweenable,"hidden perspective field rejected on orthographic camera");
  AE_EXPECT_TRUE(tweens.create(world,{world.handle(cameraId),cameraInstance},"orthographic_half_height",9,1,0,false,tracks[3])==WorldStatus::Ok,"actual orthographic lens eligible");
  AE_EXPECT_TRUE(world.setActive(world.handle(id),false)==WorldStatus::Ok&&tweens.advance(world,.25,.25),"inactive hierarchy freezes RGB");SceneNumberTweens::State snapshot;tweens.command(world,tracks[0],0,snapshot);AE_EXPECT_TRUE(snapshot.elapsed==0&&!snapshot.active,"inactive snapshot");world.setActive(world.handle(id),true);
  world.setProperty({world.handle(id),instance},"enabled",false);tweens.advance(world,.25,.25);tweens.command(world,tracks[0],0,snapshot);AE_EXPECT_TRUE(snapshot.elapsed==0&&!snapshot.enabled,"disabled light freezes RGB");world.setProperty({world.handle(id),instance},"enabled",true);
  tweens.advance(world,.25,.25);std::vector<renderer::SceneLight> lights;AE_EXPECT_TRUE(collectSceneLights(world.graph(),lights)&&lights.size()==1&&lights[0].color[0]==.84375f&&lights[0].color[1]==.9375f&&lights[0].color[2]==.5625f,"RGB extraction consumes distinct easing curves");
  AE_EXPECT_TRUE(editor::resolveSceneCamera(world.graph(),cameraId).orthographicHalfHeight==8,"orthographic render resolver consumes live extent");
  world.setProperty({world.handle(cameraId),cameraInstance},"projection",u32{0});tweens.advance(world,.25,.25);tweens.command(world,tracks[3],0,snapshot);AE_EXPECT_TRUE(snapshot.status==SceneNumberTweens::Status::Failed&&snapshot.failure==WorldStatus::PropertyNotTweenable,"projection change retires now-ineligible track");
}
AE_TEST(number_tween_eligibility_conflict_stale_session_bounded_storage_and_hot_path) {
  SceneGraph graph;const auto id=graph.createEntity(graph.root(),ObjectKind::Folder,"Light");auto value=*graph.find(id);auto*light=static_cast<scene::Light*>(value.components.add(scene::Light::descriptor));light->intensity=0;const auto instance=light->instanceId();graph.applyEntityValues(id,value);GameWorld world;AE_EXPECT_TRUE(world.load(graph),"world");SceneNumberTweens tweens;u64 track=0;
  AE_EXPECT_TRUE(tweens.create(world,{world.handle(id),instance},"range",2,1,0,false,track)==WorldStatus::PropertyNotTweenable,"unqualified field rejected");
  AE_EXPECT_TRUE(tweens.create(world,{world.handle(id),instance},"intensity",-1,1,0,false,track)==WorldStatus::Rejected,"domain validated before allocation or mutation");
  AE_EXPECT_TRUE(tweens.create(world,{world.handle(id),instance},"intensity",8,1,0,false,track)==WorldStatus::Ok,"track");tweens.advance(world,.25,.25);world.setProperty({world.handle(id),instance},"intensity",5.f);tweens.advance(world,.25,.25);SceneNumberTweens::State state;tweens.command(world,track,0,state);AE_EXPECT_TRUE(state.status==SceneNumberTweens::Status::Failed&&state.failure==WorldStatus::PropertyWrittenExternally,"external writer diagnosed without overwrite");scene::ComponentPropertyValue current;world.getProperty({world.handle(id),instance},"intensity",current);AE_EXPECT_TRUE(std::get<float>(current)==5,"external value retained");
  tweens.command(world,track,4,state);AE_EXPECT_TRUE(tweens.command(world,track,0,state)==WorldStatus::UnknownElement,"explicit release expires token");
  for(u32 i=0;i<SceneNumberTweens::MaximumTracks;++i){AE_EXPECT_TRUE(tweens.create(world,{world.handle(id),instance},"intensity",8,1,0,false,track)==WorldStatus::Ok,"bounded retained slot");tweens.command(world,track,3,state);}
  u64 rejected=0;AE_EXPECT_TRUE(tweens.create(world,{world.handle(id),instance},"intensity",8,1,0,false,rejected)==WorldStatus::LimitReached,"terminal states bounded too");tweens.command(world,track,4,state);AE_EXPECT_TRUE(tweens.create(world,{world.handle(id),instance},"intensity",8,100,0,false,track)==WorldStatus::Ok,"release restores capacity");
  const auto begin=std::chrono::steady_clock::now();for(u32 i=0;i<1000;++i)AE_EXPECT_TRUE(tweens.advance(world,.001,.001),"hot path");const double milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();std::printf("NUMBER_TWEEN_HOST: 1000 frames, 256 retained / one active, %.3f ms; not Android performance\n",milliseconds);
  world.removeComponent({world.handle(id),instance});world.flush();AE_EXPECT_TRUE(tweens.command(world,track,0,state)==WorldStatus::Ok&&state.status==SceneNumberTweens::Status::Failed&&state.failure==WorldStatus::ComponentMissing,"query catches removed target even before another frame");
  world.clear();AE_EXPECT_TRUE(tweens.command(world,track,0,state)==WorldStatus::NotRunning,"closed session");world.load(graph);AE_EXPECT_TRUE(tweens.command(world,track,0,state)==WorldStatus::ForeignWorld,"new session cannot reuse old token");
}
