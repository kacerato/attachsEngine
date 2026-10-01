#include "harness.h"
#include "editor/editor_play_scene.h"
#include "scene/audio.h"
#include "scene/script_behavior.h"
using namespace ae;
namespace {
scene::ScriptSceneAccess audioAccess;
scene::ScriptRuntimeApi audioApi() {
  scene::ScriptRuntimeApi api;
  api.start=[](const u8*,int,const u8*,int,const scene::ScriptSceneAccess *access){audioAccess=*access;return access->available()?0:1;};
  api.update=[](float){return 0;};api.fixedUpdate=api.update;api.lateUpdate=api.update;
  api.stop=[]{};api.copyDiagnostics=[](u8*,int){return 0;};api.trigger=[](u64,u64,u32){return 0;};
  api.contact=[](u64,u64,u32,const float*){return 0;};api.timer=[](u64,u64,u32){return 0;};api.lifecycle=[](u32,u32){return 0;};
  return api;
}
}
AE_TEST(audio_script_observes_real_offline_voice_and_rejects_stale_lifetime) {
  editor::EditorDocument doc;
  const auto id=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"Audio");
  auto values=*doc.find(id);
  auto *source=static_cast<scene::AudioSource*>(values.components.add(scene::AudioSource::descriptor));
  source->clip=resources::assetGuidFromSeed("audioABI");
  source->dimension=scene::AudioDimension::Flat;source->playback=scene::AudioPlayback::Playing;
  const auto instance=source->instanceId();
  auto *script=static_cast<scene::ScriptBehavior*>(values.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="test.Audio";script->source="Audio.cs";
  AE_EXPECT_TRUE(doc.applyEntityValues(id,values),"authored source and script");
  auto clip=std::make_shared<resources::AudioClip>();clip->samples.assign(96000,.1f);
  editor::EditorMapScene resources;editor::EditorPlayScene play;
  play.configureAudio([clip](resources::AssetGuid,std::string&)->std::shared_ptr<const resources::AudioClip>{return clip;},runtime::SceneAudio::Output::Offline);
  play.setScriptRuntime(audioApi(),"/test");
  AE_EXPECT_TRUE(play.start(doc,resources),"real Play bridge");
  AE_EXPECT_TRUE(audioAccess.version==scene::ScriptSceneAccess{}.version&&audioAccess.available(),"complete ABI25");
  auto old=audioAccess;old.size=offsetof(scene::ScriptSceneAccess,audioSnapshot);
  AE_EXPECT_TRUE(!old.available(),"truncated ABI rejected before appended callback");
  const auto handle=play.world().handle(id);
  scene::ScriptAudioSnapshot snapshot;
  auto query=[&]{return audioAccess.audioSnapshot(audioAccess.context,id,handle.world,handle.generation,instance,&snapshot);};
  AE_EXPECT_EQ(audioAccess.audioSnapshot(audioAccess.context,id,handle.world+1,handle.generation,instance,&snapshot),0,"foreign world rejected");
  AE_EXPECT_TRUE(audioAccess.lastStatus(audioAccess.context)==u32(runtime::WorldStatus::ForeignWorld),"foreign world reports its own error");
  AE_EXPECT_EQ(audioAccess.audioSnapshot(audioAccess.context,id,handle.world,handle.generation+1,instance,&snapshot),0,"stale generation rejected");
  AE_EXPECT_TRUE(audioAccess.lastStatus(audioAccess.context)==u32(runtime::WorldStatus::StaleHandle),"stale generation reports its own error");
  std::vector<float> pcm(9600);
  AE_EXPECT_TRUE(play.audio().renderOffline(pcm)&&play.audio().advance(play.world(),0),"consume actual PCM then publish cursor");
  AE_EXPECT_EQ(query(),1,"read backend snapshot");
  AE_EXPECT_TRUE(snapshot.state==u32(runtime::SceneAudio::State::Playing)&&snapshot.cursor>.09&&snapshot.outputRunning==0,"Offline real DSP never claims device output");
  const auto cursor=snapshot.cursor;
  const runtime::ComponentHandle component{handle,instance};
  AE_EXPECT_TRUE(play.world().setProperty(component,"playback",u32(scene::AudioPlayback::Paused))==runtime::WorldStatus::Ok&&play.audio().advance(play.world(),0),"pause through same reflected setter");
  AE_EXPECT_TRUE(play.audio().renderOffline(pcm)&&play.audio().advance(play.world(),0)&&query()==1&&snapshot.state==u32(runtime::SceneAudio::State::Paused)&&snapshot.cursor==cursor,"paused backend cursor stable");
  AE_EXPECT_TRUE(play.world().removeComponent(component)==runtime::WorldStatus::Ok,"remove source");
  AE_EXPECT_EQ(query(),1,"queued removal keeps the source alive until the safe point");
  play.world().flush();
  AE_EXPECT_EQ(query(),0,"retained diagnostic cannot resurrect missing component");
  AE_EXPECT_TRUE(audioAccess.lastStatus(audioAccess.context)==u32(runtime::WorldStatus::ComponentMissing),"explicit component error");
  play.stop();
  AE_EXPECT_EQ(query(),0,"bridge Stop invalidates query");
  AE_EXPECT_TRUE(audioAccess.lastStatus(audioAccess.context)==u32(runtime::WorldStatus::NotRunning),"explicit stopped world error");
}
