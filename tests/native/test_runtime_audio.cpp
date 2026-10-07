#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "scene/audio.h"
#include "harness.h"
#include <cmath>
#include <cstring>
#include <filesystem>
using namespace ae;
namespace {
std::vector<u8> wave(u32 frames=4800){
  std::vector<u8> bytes(44+frames*4,0);const auto put16=[&](usize p,u32 v){bytes[p]=u8(v);bytes[p+1]=u8(v>>8);};const auto put32=[&](usize p,u32 v){for(u32 k=0;k<4;++k)bytes[p+k]=u8(v>>(k*8));};
  std::memcpy(bytes.data(),"RIFF",4);put32(4,u32(bytes.size()-8));std::memcpy(bytes.data()+8,"WAVEfmt ",8);put32(16,16);put16(20,1);put16(22,2);put32(24,48000);put32(28,192000);put16(32,4);put16(34,16);std::memcpy(bytes.data()+36,"data",4);put32(40,frames*4);
  for(u32 k=0;k<frames*2;++k)put16(44+k*2,8192);
  return bytes;
}
float mean(std::span<const float> values){double s=0;for(float v:values)s+=std::abs(v);return float(s/values.size());}
template<class T>T &add(runtime::SceneGraph &graph,runtime::ObjectId id){return *static_cast<T*>(graph.editComponents(id)->add(T::descriptor));}
}
AE_TEST(audio_wave_decode_refuses_truncation_and_preserves_output_on_error){
  auto bytes=wave();resources::AudioClip clip;std::string error;
  AE_EXPECT_TRUE(resources::decodeWaveClip(bytes,clip,error),error.c_str());
  AE_EXPECT_EQ(clip.frames(),u64{4800},"PCM frames converted at48k");AE_EXPECT_TRUE(std::abs(clip.samples[0]-.25f)<.001f,"PCM16 real decode");
  auto broken=bytes;broken.pop_back();const auto old=clip.samples;
  AE_EXPECT_TRUE(!resources::decodeWaveClip(broken,clip,error)&&clip.samples==old,"truncation refuses without replacing liveclip");
  broken=bytes;broken[22]=3;AE_EXPECT_TRUE(!resources::decodeWaveClip(broken,clip,error),"unsupportedchannels refuse");
}
AE_TEST(audio_source_archive_clears_previous_clip_in_reused_component){
  scene::AudioSource empty,reused;
  reused.clip=resources::assetGuidFromSeed("previous-clip");
  std::ostringstream saved;empty.write(saved);std::istringstream input(saved.str());
  AE_EXPECT_TRUE(reused.read(input,scene::AudioSource::descriptor.version)&&!reused.clip.valid(),"empty persisted binding must replace previous GUID");
  std::ostringstream restored;reused.write(restored);
  AE_EXPECT_TRUE(restored.str()==saved.str(),"clear roundtrip must not resurrect previous resource");
}
AE_TEST(audio_listener_priority_pose_disable_and_removal_affect_real_output) {
  editor::EditorDocument doc;
  const auto muted=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"Muted listener");
  const auto audible=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"Selected listener");
  const auto emitter=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"Spatial emitter");
  auto &low=add<scene::AudioListener>(doc,muted);low.priority=1;low.volume=0;
  auto &high=add<scene::AudioListener>(doc,audible);high.priority=10;
  auto &source=add<scene::AudioSource>(doc,emitter);source.dimension=scene::AudioDimension::Spatial;source.loop=true;
  const auto guid=source.clip=resources::assetGuidFromSeed("listener-contract-wave");
  auto transform=doc.find(emitter)->transform;transform.position[2]=2;doc.setTransform(emitter,transform);
  auto clip=std::make_shared<resources::AudioClip>();std::string error;
  AE_EXPECT_TRUE(resources::decodeWaveClip(wave(48000),*clip,error),"real PCM decoded");
  runtime::GameWorld world;AE_EXPECT_TRUE(world.load(doc),"world");runtime::SceneAudio audio;
  audio.configure([&](resources::AssetGuid id,std::string&)->std::shared_ptr<const resources::AudioClip>{return id==guid?clip:nullptr;},runtime::SceneAudio::Output::Offline);
  AE_EXPECT_TRUE(audio.start(world),audio.error().c_str());std::vector<float> pcm(9600);
  const auto render=[&] {return audio.advance(world,0) && audio.renderOffline(pcm) && audio.renderOffline(pcm);};
  AE_EXPECT_TRUE(render() && mean(pcm)>.03f,"highest priority active listener produces spatial output");
  const float initial=mean(pcm);
  const auto chosen=world.findComponent(world.handle(audible),scene::AudioListener::descriptor.id);
  const auto silent=world.findComponent(world.handle(muted),scene::AudioListener::descriptor.id);
  AE_EXPECT_TRUE(world.setProperty(chosen,"enabled",false)==runtime::WorldStatus::Ok && render() && mean(pcm)<.001f,"disabled listener yields to the muted listener");
  AE_EXPECT_TRUE(world.setProperty(chosen,"enabled",true)==runtime::WorldStatus::Ok && world.setProperty(silent,"priority",20.f)==runtime::WorldStatus::Ok && render() && mean(pcm)<.001f,"priority mutation switches actual DSP receiver");
  AE_EXPECT_TRUE(world.destroyObject(world.handle(muted))==runtime::WorldStatus::Ok,"destroy winning listener");world.flush();
  AE_EXPECT_TRUE(render() && mean(pcm)>.03f,"destroyed listener relinquishes ownership");
  transform=world.graph().find(audible)->transform;transform.position[0]=90;
  AE_EXPECT_TRUE(world.setLocalTransform(world.handle(audible),transform)==runtime::WorldStatus::Ok && render() && mean(pcm)<initial*.1f,"world-space listener position changes attenuation");
  AE_EXPECT_TRUE(world.destroyObject(world.handle(audible))==runtime::WorldStatus::Ok,"destroy last listener");world.flush();
  const auto sourceHandle=world.findComponent(world.handle(emitter),scene::AudioSource::descriptor.id);
  AE_EXPECT_TRUE(audio.advance(world,0) && audio.diagnostic(emitter,sourceHandle.instance)->state==runtime::SceneAudio::State::MissingListener,"missing listener is diagnosed after lifecycle removal");
  audio.stop();
}
AE_TEST(audio_source_bus_pause_pitch_spatial_and_lifecycle_use_real_dsp){
  editor::EditorDocument doc;const auto master=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"Music");
  const auto bus=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"Nested");const auto sourceId=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"Source");
  add<scene::AudioBus>(doc,master).volume=.5f;auto &child=add<scene::AudioBus>(doc,bus);child.volume=.5f;child.output=master;
  auto &source=add<scene::AudioSource>(doc,sourceId);source.clip=resources::assetGuidFromSeed("testWave");source.bus=bus;source.loop=true;
  auto clip=std::make_shared<resources::AudioClip>();std::string error;AE_EXPECT_TRUE(resources::decodeWaveClip(wave(48000),*clip,error),"WAV fixture");
  runtime::GameWorld world;AE_EXPECT_TRUE(world.load(doc),"realGameWorld");runtime::SceneAudio audio;
  audio.configure([&](resources::AssetGuid guid,std::string&)->std::shared_ptr<const resources::AudioClip>{return guid==source.clip?clip:nullptr;},runtime::SceneAudio::Output::Offline);
  AE_EXPECT_TRUE(audio.start(world),audio.error().c_str());std::vector<float> pcm(9600);
  AE_EXPECT_TRUE(audio.renderOffline(pcm)&&mean(pcm)>.04f&&mean(pcm)<.07f,"realengine voice gain .25*.5*.5");
  AE_EXPECT_TRUE(audio.advance(world,0),"read observed cursor before output suspension");
  const auto initialCursor=audio.diagnostic(sourceId,source.instanceId())->cursor;
  audio.pause(true);
  AE_EXPECT_TRUE(audio.renderOffline(pcm)&&mean(pcm)==0&&audio.advance(world,0)&&audio.diagnostic(sourceId,source.instanceId())->cursor==initialCursor,"output suspension preserves cursor and produces silence");
  audio.pause(false);
  const auto sourceHandle=world.findComponent(world.handle(sourceId),scene::AudioSource::descriptor.id),busHandle=world.findComponent(world.handle(bus),scene::AudioBus::descriptor.id);
  AE_EXPECT_TRUE(world.setProperty(busHandle,"mute",true)==runtime::WorldStatus::Ok&&audio.advance(world,.1)&&audio.renderOffline(pcm)&&mean(pcm)<.001f,"busmute affects actualDSP");
  AE_EXPECT_TRUE(world.setProperty(busHandle,"mute",false)==runtime::WorldStatus::Ok,"unmute");
  AE_EXPECT_TRUE(world.setProperty(sourceHandle,"playback",u32(scene::AudioPlayback::Paused))==runtime::WorldStatus::Ok&&audio.advance(world,.1),"pause request");const auto before=audio.diagnostic(sourceId,sourceHandle.instance)->cursor;
  AE_EXPECT_TRUE(audio.renderOffline(pcm)&&mean(pcm)<.001f&&audio.advance(world,.1)&&std::abs(audio.diagnostic(sourceId,sourceHandle.instance)->cursor-before)<.0001,"pause keepscursor");
  AE_EXPECT_TRUE(world.setProperty(sourceHandle,"playback",u32(scene::AudioPlayback::Stopped))==runtime::WorldStatus::Ok&&audio.advance(world,0),"stoprewinds");
  AE_EXPECT_TRUE(world.setProperty(sourceHandle,"pitch",2.f)==runtime::WorldStatus::Ok&&world.setProperty(sourceHandle,"playback",u32(scene::AudioPlayback::Playing))==runtime::WorldStatus::Ok&&audio.advance(world,0)&&audio.renderOffline(pcm)&&audio.advance(world,.1),"pitchresampling");
  AE_EXPECT_TRUE(std::abs(audio.diagnostic(sourceId,sourceHandle.instance)->cursor-.2)<.02,"2xpitch advancesrealPCMcursor");
  AE_EXPECT_TRUE(world.setProperty(sourceHandle,"dimension",u32(scene::AudioDimension::Spatial))==runtime::WorldStatus::Ok&&audio.advance(world,0)&&audio.diagnostic(sourceId,sourceHandle.instance)->state==runtime::SceneAudio::State::MissingListener,"no spatiallistenerfake");
  runtime::WorldStatus status;
  const auto listenerHandle=world.addComponent(world.handle(bus),scene::AudioListener::descriptor.id,status);
  AE_EXPECT_TRUE(status==runtime::WorldStatus::Ok&&listenerHandle.instance!=0,"worldadd invalidates candidate cache");
  AE_EXPECT_TRUE(audio.advance(world,0)&&audio.renderOffline(pcm)&&mean(pcm)>.03f,"real spatialDSP withlistener");
  AE_EXPECT_TRUE(world.setProperty(sourceHandle,"loop",false)==runtime::WorldStatus::Ok&&audio.advance(world,0),"disable actual looping");
  for(u32 k=0;k<8;++k) AE_EXPECT_TRUE(audio.renderOffline(pcm),"consume beyond EOF");
  AE_EXPECT_TRUE(audio.advance(world,0)&&audio.diagnostic(sourceId,sourceHandle.instance)->state==runtime::SceneAudio::State::Stopped,"EOF reports observed stopped, not persistent playing request");
  AE_EXPECT_TRUE(audio.renderOffline(pcm)&&mean(pcm)<.001f,"EOF does not silently restart");
  AE_EXPECT_TRUE(world.destroyObject(world.handle(sourceId))==runtime::WorldStatus::Ok,"removeSource");world.flush();
  AE_EXPECT_TRUE(audio.advance(world,0),"removed voice reconciliation succeeds");
  std::fill(pcm.begin(),pcm.end(),1.f);
  AE_EXPECT_TRUE(audio.renderOffline(pcm),"ready empty source graph renders silence successfully");
  AE_EXPECT_TRUE(mean(pcm)==0,"removed voice clears the entire previously occupied output block");
  AE_EXPECT_TRUE(!audio.renderOffline(std::span<float>(pcm.data(),3)),"empty graph still rejects malformed stereo output");
  audio.stop();AE_EXPECT_TRUE(!audio.renderOffline(pcm),"Stop invalidatesaudiooutput");
}
AE_TEST(audio_project_import_binding_history_save_reopen_and_play_share_guid){
  namespace fs=std::filesystem;const auto root=fs::temp_directory_path()/"astra-audio-contract";std::error_code ec;fs::create_directories(root/"Audio",ec);
  AE_EXPECT_TRUE(!ec&&editor::EditorImportTransaction::write(root/"Audio/test.wav",wave()),"projectWAV");
  editor::EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(root.string().c_str()),"projectroot");session.setAudioOutput(runtime::SceneAudio::Output::Offline);
  resources::AssetGuid guid;std::string error;AE_EXPECT_TRUE(session.importWaveClip("Audio/test.wav",guid,error),error.c_str());
  const auto id=session.document().createEntity(session.document().root(),runtime::ObjectKind::Folder,"Source");auto value=*session.document().find(id);auto *source=static_cast<scene::AudioSource*>(value.components.add(scene::AudioSource::descriptor));source->clip=guid;
  AE_EXPECT_TRUE(session.history().applyValues(session.document(),id,value),"bind via history");
  const auto archive=editor::serializeEditorDocument(session.document(),0);editor::EditorDocument reopened;AE_EXPECT_TRUE(editor::deserializeEditorDocument(archive,0,reopened),"save/reopen");
  AE_EXPECT_TRUE(static_cast<const scene::AudioSource*>(reopened.find(id)->components.find(scene::AudioSource::descriptor))->clip==guid,"durablebinding");
  AE_EXPECT_TRUE(session.history().undo(session.document())&&!session.document().find(id)->components.find(scene::AudioSource::descriptor),"undobinding");AE_EXPECT_TRUE(session.history().redo(session.document()),"redobinding");
  runtime::GameWorld world;AE_EXPECT_TRUE(world.load(reopened),"reopenedWorld");runtime::SceneAudio audio;audio.configure([&](resources::AssetGuid asset,std::string &e){return session.loadAudioClip(asset,e);},runtime::SceneAudio::Output::Offline);
  AE_EXPECT_TRUE(audio.start(world),audio.error().c_str());std::vector<float> pcm(960);AE_EXPECT_TRUE(audio.renderOffline(pcm)&&mean(pcm)>.2f,"loadedprojectclip realDSP");audio.stop();
  editor::EditorSession again;AE_EXPECT_TRUE(again.setProjectDirectory(root.string().c_str()),"reopenproject");
  std::vector<u8> registry;
  AE_EXPECT_TRUE(editor::EditorImportTransaction::read(root/".astra/assets.astra",registry)&&again.loadAssets(std::string_view(reinterpret_cast<const char*>(registry.data()),registry.size())),"shell reloads registry from real project file");
  AE_EXPECT_TRUE(again.assets().find(guid)&&again.loadAudioClip(guid,error),"registryreload");
  fs::remove(root/"Audio/test.wav",ec);
}
