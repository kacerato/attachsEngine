#include "harness.h"
#include "runtime/scene_tween_sequences.h"
#include <sstream>
#include <vector>
using namespace ae;using namespace ae::runtime;

namespace {
struct SequenceScene {
  SceneGraph graph;
  ObjectId a=0,b=0,c=0,plain=0,endless=0,sequencer=0,broken=0,looping=0;
  u64 tweenA=0,sequenceInstance=0,brokenInstance=0,loopingInstance=0;
  ObjectId tweened(const char *name,bool autoplay,u32 loops,u64 *instance=nullptr) {
    const auto id=graph.createEntity(graph.root(),ObjectKind::Folder,name);auto value=*graph.find(id);
    auto *t=static_cast<scene::TransformTween*>(value.components.add(scene::TransformTween::descriptor));
    t->duration=.5f;t->autoplay=autoplay;t->loops=loops;t->destination[0]=1;if(instance)*instance=t->instanceId();
    graph.applyEntityValues(id,value);return id;
  }
  ObjectId sequence(const char *name,std::initializer_list<scene::TweenSequence::Step> steps,u32 loops,u64 &instance) {
    const auto id=graph.createEntity(graph.root(),ObjectKind::Folder,name);auto value=*graph.find(id);
    auto *s=static_cast<scene::TweenSequence*>(value.components.add(scene::TweenSequence::descriptor));
    u32 i=0;for(const auto &step:steps) s->steps[i++]=step;s->loops=loops;instance=s->instanceId();
    graph.applyEntityValues(id,value);return id;
  }
  SequenceScene() {
    a=tweened("A",true,1,&tweenA);b=tweened("B",false,1);c=tweened("C",false,1);endless=tweened("Sem fim",false,0);
    plain=graph.createEntity(graph.root(),ObjectKind::Folder,"Sem tween");
    sequencer=sequence("Sequência",{{a,false,0},{b,false,.25f},{c,true,0}},2,sequenceInstance);
    broken=sequence("Quebrada",{{plain,false,0}},1,brokenInstance);
    looping=sequence("Infinita",{{endless,false,0}},1,loopingInstance);
  }
};
float x(GameWorld &w,ObjectId id) {Transform t;w.localTransform(w.handle(id),t);return t.position[0];}
}

AE_TEST(tween_sequence_runs_steps_in_order_parallel_with_interval_loops_and_events) {
  scene::TweenSequence authored;authored.steps[0]={7,false,.5f};authored.steps[2]={9,true,0};authored.loops=3;
  std::stringstream payload;authored.write(payload);scene::TweenSequence copy;
  AE_EXPECT_TRUE(copy.read(payload,1)&&copy.steps[2].join&&copy.steps[0].interval==.5f&&copy.loops==3&&copy.stepCount()==2,"versioned sequence round-trips");
  authored.steps[1].interval=-1;AE_EXPECT_TRUE(!authored.valid(),"negative interval rejected");

  SequenceScene scene;GameWorld w;AE_EXPECT_TRUE(w.load(scene.graph),"real world");
  SceneTweens tweens;SceneTweenSequences sequences;ComponentEventQueue events;
  events.attach(ComponentEventQueue::Consumer::Scripts,true);sequences.setEvents(&events);tweens.setEvents(&events);
  const auto frame=[&](double dt){AE_EXPECT_TRUE(sequences.advance(w,tweens,dt,dt)&&tweens.advance(w,dt,dt),"frame advances");};
  frame(0);
  AE_EXPECT_TRUE(sequences.state(scene.sequencer,scene.sequenceInstance)->status==SceneTweenSequences::Status::Running,"step 1 started");
  frame(.25);frame(.25);
  AE_EXPECT_TRUE(std::abs(x(w,scene.a)-1)<1e-4f&&x(w,scene.b)==0&&x(w,scene.c)==0,"later steps wait; their own tweens never ran");
  frame(.1);frame(.1);frame(.1);
  AE_EXPECT_TRUE(x(w,scene.b)==0,"interval holds step 2");
  frame(.1);
  AE_EXPECT_TRUE(x(w,scene.b)>0&&std::abs(x(w,scene.b)-x(w,scene.c))<1e-5f,"step 3 joins step 2 in the same frame");
  for(int i=0;i<40;++i) frame(.1);
  const auto *done=sequences.state(scene.sequencer,scene.sequenceInstance);
  AE_EXPECT_TRUE(done->status==SceneTweenSequences::Status::Completed&&done->loopsDone==2,"two passes complete");
  std::vector<i64> steps;u32 completed=0;
  events.consume(ComponentEventQueue::Consumer::Scripts,[&](const ComponentEventRecord &r,u64){
    if(r.type!=&scene::TweenSequence::descriptor) return;
    if(r.type->events[r.event].id=="step_started") steps.push_back(r.values[0].integer); else ++completed;});
  AE_EXPECT_TRUE((steps==std::vector<i64>{1,2,3,1,2,3}),"step_started carries each step number per pass");
  AE_EXPECT_EQ(completed,u32{1},"completed is emitted once after the finite loops");

  const auto *broken=sequences.state(scene.broken,scene.brokenInstance);
  AE_EXPECT_TRUE(broken->status==SceneTweenSequences::Status::Failed&&broken->failure.find("sem Transform Tween")!=std::string::npos,"missing tween fails with reason");
  const auto *endless=sequences.state(scene.looping,scene.loopingInstance);
  AE_EXPECT_TRUE(endless->status==SceneTweenSequences::Status::Failed&&endless->failure.find("infinita")!=std::string::npos,"infinite member tween is refused, not waited forever");
}

AE_TEST(tween_sequence_methods_play_pause_resume_cancel_and_step_through_operations) {
  SequenceScene scene;GameWorld w;AE_EXPECT_TRUE(w.load(scene.graph),"real world");
  SceneTweens tweens;SceneTweenSequences sequences;
  ComponentOperationServices services{&w,nullptr,&tweens,nullptr,nullptr,&sequences};
  const ComponentHandle handle{w.handle(scene.sequencer),scene.sequenceInstance};
  scene::ComponentOperationValue result;
  const auto call=[&](const char *method){return invokeComponentMethod(services,handle,method,{},result);};
  const auto frame=[&](double dt){sequences.advance(w,tweens,dt,dt);tweens.advance(w,dt,dt);};
  frame(0);frame(.1);
  AE_EXPECT_TRUE(call("step")==WorldStatus::Ok&&result.integer==1,"step reports the running step");
  AE_EXPECT_TRUE(call("pause")==WorldStatus::Ok,"pause");
  const float paused=x(w,scene.a);frame(.2);
  AE_EXPECT_TRUE(x(w,scene.a)==paused&&tweens.state(scene.a,scene.tweenA)->paused,"pause reaches the member tween");
  AE_EXPECT_TRUE(call("resume")==WorldStatus::Ok,"resume");frame(.1);
  AE_EXPECT_TRUE(x(w,scene.a)>paused,"resume continues the member");
  AE_EXPECT_TRUE(call("cancel")==WorldStatus::Ok,"cancel");frame(.1);
  AE_EXPECT_TRUE(sequences.state(scene.sequencer,scene.sequenceInstance)->status==SceneTweenSequences::Status::Cancelled&&
                 tweens.state(scene.a,scene.tweenA)->status==SceneTweens::Status::Cancelled,"cancel stops sequence and member once");
  AE_EXPECT_TRUE(call("step")==WorldStatus::Ok&&result.integer==0,"cancelled sequence reports no step");
  AE_EXPECT_TRUE(call("play")==WorldStatus::Ok,"play restarts");frame(0);
  AE_EXPECT_TRUE(call("step")==WorldStatus::Ok&&result.integer==1,"play restarts from step 1");
  ComponentOperationServices missing{&w,nullptr,&tweens};
  AE_EXPECT_TRUE(invokeComponentMethod(missing,handle,"play",{},result)==WorldStatus::NotRunning,"host without the evaluator refuses explicitly");
}
