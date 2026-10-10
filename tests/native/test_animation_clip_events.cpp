#include "harness.h"
#include "runtime/scene_animation.h"
#include "runtime/scene_animator_graph.h"
#include "runtime/scene_event_connections.h"
#include "scene/animation.h"
#include "scene/animator.h"
#include "resources/animation_clip_asset.h"
#include "resources/animation_clip_bake.h"
#include <sstream>
#include <cmath>
using namespace ae;
using namespace ae::resources;
namespace {
struct CueLibrary final : runtime::AnimationLibrary {
  runtime::SourceAnimations source;
  AssetGuid guid=assetGuidFromSeed("cue-clip");
  CueLibrary() {
    source.nodes={assetGuidFromSeed("cue-bone")};source.nodeNames={"Bone"};source.clipIds={guid};
    AnimationClip c;c.name="Mechanism";c.duration=1;
    AnimationChannel channel;channel.times={0,1};channel.values={0,0,0,1,0,0};c.channels={channel};
    c.cues={{1,.25f,AnimationCueKind::Event,"Latch",7,2.5},{2,.5f,AnimationCueKind::Marker,"Pose"},
            {3,.75f,AnimationCueKind::Event,"Sound",8,.125}};
    source.clips={c};
  }
  bool findClip(const AssetGuid &id,runtime::AnimationClipView &out) const override {
    if(id!=guid)return false;
    out={&source.clips[0],&source,"Mechanism"};return true;
  }
};
}
AE_TEST(animation_clip_cues_time_traversal_preservation_and_budget) {
  CueLibrary library;const auto &clip=library.source.clips[0];std::vector<u64> ids;
  const auto visit=[&](double from,double to,AnimationWrapMode mode,u32 budget=256){
    ids.clear();return visitAnimationCues(clip,from,to,mode,[&](const auto &c){ids.push_back(c.id);},budget);
  };
  AE_EXPECT_TRUE(!visit(0,.75,AnimationWrapMode::Loop)&&ids==std::vector<u64>({1,3}),"events ordered; marker never dispatched");
  AE_EXPECT_TRUE(!visit(.75,1.25,AnimationWrapMode::Loop)&&ids==std::vector<u64>({1}),"half-open boundary does not replay end of previous frame");
  AE_EXPECT_TRUE(!visit(.75,.25,AnimationWrapMode::Loop)&&ids==std::vector<u64>({1}),"reverse excludes origin and includes arrival");
  AE_EXPECT_TRUE(!visit(0,2,AnimationWrapMode::PingPong)&&ids==std::vector<u64>({1,3,3,1}),"both ping-pong legs");
  AE_EXPECT_TRUE(visit(0,1000000,AnimationWrapMode::Loop,3)==1999997&&ids==std::vector<u64>({1,3,1}),"analytical overflow with chronological bounded prefix");
  AE_EXPECT_TRUE(!visit(0,0,AnimationWrapMode::Loop)&&ids.empty(),"stationary seek produces no traversal");
  auto edges=clip;edges.cues={{4,0,AnimationCueKind::Event,"Start"},{5,1,AnimationCueKind::Event,"End"}};ids.clear();
  visitAnimationCues(edges,0,4,AnimationWrapMode::PingPong,[&](const auto &c){ids.push_back(c.id);});
  AE_EXPECT_TRUE(ids==std::vector<u64>({5,4,5,4}),"ping-pong endpoints emitted once");
  edges.cues[1].reverse=false;ids.clear();
  visitAnimationCues(edges,2,0,AnimationWrapMode::PingPong,[&](const auto &c){ids.push_back(c.id);});
  AE_EXPECT_TRUE(ids==std::vector<u64>({5,4}),"ping-pong endpoints use local arriving direction even with decreasing clock");
  AE_EXPECT_TRUE(visit(1000000,1000000.75,AnimationWrapMode::Loop)==0&&ids==std::vector<u64>({1,3}),"long-running graphs do not silently lose cues");
  const AnimationClipBinding binding{0,{},"","Bone"};AnimationClipAsset asset;std::string error;
  AE_EXPECT_TRUE(authorAnimationClip(clip,assetGuidFromSeed("cues-authored"),{},{},{},{&binding,1},asset,error),error.c_str());
  const auto original=asset.serialize();AnimationClipAsset restored;
  AE_EXPECT_TRUE(AnimationClipAsset::deserialize(original,restored,&error)&&restored.serialize()==original,"AECLIP 4 exact metadata and double payload round trip");
  const auto id=asset.cues[0].id;auto edited=asset.cues[0];edited.value=1.2345678901234567;u64 result=0;
  AE_EXPECT_TRUE(asset.putCue(edited,result,error)&&result==id,"editing preserves identity");
  AE_EXPECT_TRUE(asset.retime(2,error)&&asset.cue(id)->time==.5f&&asset.reverse(error)&&asset.cue(id)->time==1.5f,"global transforms include cues");
  AE_EXPECT_TRUE(asset.crop(.5f,1.5f,error)&&asset.cue(id)->time==1,"crop includes boundaries and relocates markers");
  auto invalid=asset.cues[0];invalid.time=-1;const auto before=asset.serialize();
  AE_EXPECT_TRUE(!asset.putCue(invalid,result,error)&&asset.serialize()==before,"invalid edits are atomic");
  AnimationBakeSettings settings;AnimationBakeReport report;AnimationClipAsset consolidated;
  AE_EXPECT_TRUE(consolidateAnimationClip(asset,assetGuidFromSeed("cues-consolidated"),"Consolidated",settings,consolidated,report,error)&&consolidated.cues.size()==asset.cues.size(),error.c_str());
  AE_EXPECT_EQ(consolidated.cues.back().value,asset.cues.back().value,"bake preserves precise event payloads");
  auto legacy=restored;legacy.cues.clear();auto text=legacy.serialize();text.replace(0,8,"AECLIP 3");text.resize(text.size()-2);
  AE_EXPECT_TRUE(AnimationClipAsset::deserialize(text,restored,&error)&&restored.cues.empty(),"legacy AECLIP 3 migrates with an empty cue list");
  scene::EventConnection connection;connection.event=30;connection.clipTag=16777215;std::ostringstream out;connection.write(out);
  scene::EventConnection reopened;std::istringstream in(out.str());AE_EXPECT_TRUE(reopened.read(in,2)&&reopened.clipTag==16777215,"largest exact connection code persists without float formatting loss");
  std::istringstream old("1 1 1 0 0 0 0 0");AE_EXPECT_TRUE(reopened.read(old,1)&&reopened.clipTag==-1,"legacy connection keeps any-code behavior");
}
AE_TEST(animation_clip_cues_deliver_to_scripts_and_filtered_scene_connections) {
  using namespace runtime;
  AE_EXPECT_TRUE(auditEventConnectionCatalog().empty(),"new runtime events are usable in the real connection catalog");
  for(bool graphMode:{false,true}) {
    CueLibrary library;SceneGraph scene;const auto owner=scene.createEntity(scene.root(),ObjectKind::Folder,"Machine");
    scene.createEntity(owner,ObjectKind::Folder,"Bone");const auto receiver=scene.createEntity(scene.root(),ObjectKind::Folder,"Lamp");
    auto *components=scene.editComponents(owner);u64 instance=0;
    if(graphMode) {
      auto *a=static_cast<scene::Animator*>(components->add(scene::Animator::descriptor));
      a->layers[0].states[0].motions={{library.guid}};instance=a->instanceId();
    } else {
      auto *a=static_cast<scene::Animation*>(components->add(scene::Animation::descriptor));
      a->clip=library.guid;a->playAutomatically=true;instance=a->instanceId();
    }
    auto *c=static_cast<scene::EventConnection*>(components->add(scene::EventConnection::descriptor));
    c->event=graphMode?32:30;c->action=3;c->receiver=receiver;c->clipTag=7;
    GameWorld world;AE_EXPECT_TRUE(world.load(scene),"load real playback world");
    ComponentEventQueue queue;queue.attach(ComponentEventQueue::Consumer::Scripts,true);queue.attach(ComponentEventQueue::Consumer::Connections,true);
    SceneEventConnections connections;ComponentOperationServices services;services.world=&world;
    SceneAnimator mixer;mixer.begin(world.poseGraph(),library);mixer.setEvents(&world,&queue);
    SceneAnimatorGraphs graphs;graphs.setEvents(&queue);graphs.setLibrary(&library);
    const auto advance=[&](float dt){world.beginFrame(dt);std::vector<SceneAnimator::ExternalSample> samples;
      graphs.advance(world,library,dt,dt,samples);mixer.setExternalSamples(std::move(samples));AE_EXPECT_TRUE(mixer.advance(dt,{}),"sample same clip in real evaluator");};
    advance(.8f);std::vector<ComponentEventRecord> received;
    queue.consume(ComponentEventQueue::Consumer::Scripts,[&](const auto &record,u64){if(record.type->events[record.event].id=="clip_event")received.push_back(record);});
    AE_EXPECT_TRUE(received.size()==2&&received[0].values[0].integer==7&&received[0].values[1].number==2.5&&received[0].instance==instance,"script consumer receives exact typed payload and instance");
    AE_EXPECT_TRUE(world.activeSelf(world.handle(receiver)),"actions deferred until connection processing");
    AE_EXPECT_TRUE(connections.process(services,queue)&&!world.activeSelf(world.handle(receiver))&&connections.deliveries()==1,"tag 8 does not undo tag 7's toggle");
    if(!graphMode) {
      AnimationStateView state;AE_EXPECT_TRUE(mixer.state(owner,instance,library.guid,state)==AnimationCommandStatus::Ok,"state read");
      state.time=.1f;AE_EXPECT_TRUE(mixer.setState(owner,instance,state)==AnimationCommandStatus::Ok,"seek without replay");advance(0);
      u32 count=0;queue.consume(ComponentEventQueue::Consumer::Scripts,[&](const auto &r,u64){if(r.type->events[r.event].id=="clip_event")++count;});
      AE_EXPECT_EQ(count,0u,"seek alone does not emit skipped events");
    }
    if(graphMode) {
      SceneAnimatorGraphs::LayerSettings settings;
      AE_EXPECT_TRUE(graphs.layerControl(world,owner,instance,0,1,0,{},settings)==WorldStatus::Ok,"mute real runtime layer");
    } else {
      AnimationStateView state;AE_EXPECT_TRUE(mixer.state(owner,instance,library.guid,state)==AnimationCommandStatus::Ok,"legacy state");
      state.weight=0;AE_EXPECT_TRUE(mixer.setState(owner,instance,state)==AnimationCommandStatus::Ok,"mute real runtime clip");
    }
    advance(1);u32 muted=0;
    queue.consume(ComponentEventQueue::Consumer::Scripts,[&](const auto &r,u64){if(r.type->events[r.event].id=="clip_event")++muted;});
    AE_EXPECT_EQ(muted,0u,"zero effective weight cannot trigger gameplay");
    if(graphMode) {
      SceneAnimatorGraphs::LayerSettings settings;
      AE_EXPECT_TRUE(graphs.layerControl(world,owner,instance,0,1,1,{},settings)==WorldStatus::Ok,"restore runtime layer");
    } else {
      AnimationStateView state;AE_EXPECT_TRUE(mixer.state(owner,instance,library.guid,state)==AnimationCommandStatus::Ok,"state before large step");
      state.weight=1;AE_EXPECT_TRUE(mixer.setState(owner,instance,state)==AnimationCommandStatus::Ok,"restore runtime clip");
    }
    advance(200);u32 delivered=0;i64 lost=0;
    queue.consume(ComponentEventQueue::Consumer::Scripts,[&](const auto &r,u64){
      const auto event=r.type->events[r.event].id;
      if(event=="clip_event")++delivered;else if(event=="clip_events_lost")lost+=r.values[0].integer;
    });
    AE_EXPECT_TRUE(delivered==256&&lost==144,"real evaluators deliver bounded events and expose the precise suppression diagnostic");
    mixer.reset();graphs.reset();graphs.setEvents(nullptr);
    AE_EXPECT_TRUE(!mixer.active(),"stop disconnects legacy animation state");
  }
}
