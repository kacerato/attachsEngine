#include "runtime/scene_gui.h"
#include "runtime/scene_physics.h"
#include "scene/character.h"
#include "scene/camera.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ae::runtime {
const InputService *SceneGui::inputFor(ObjectId receiver) const {for(const auto &r:receivers_)if(r.receiver.id==receiver)return &r.input;return nullptr;}
void SceneGui::submitInput(GameWorld &world,InputService &global,const InputDeviceState &hardware,double elapsed) {
  reconcile(world);inputDiagnostic_.clear();InputDeviceState merged=hardware;
  if(!global.gameplayFocus()||global.captureStatus()==InputCaptureStatus::Waiting)cancelPointers();
  for(auto &r:receivers_){r.used=false;r.sample.virtualActions.clear();r.input.copyPolicyFrom(global);}
  for(auto &instance:instances_) {
    auto &i=*instance;if(!i.enabled||!find(world,i.id))continue;
    auto &runtime=i.runtime;runtime.layout(runtime.document().canvas().mode==ui::GuiCanvasMode::Screen?i.viewport:ui::UiRect{0,0,i.config.resolution[0],i.config.resolution[1]});
    InputDeviceState *sample=&merged;
    if(i.config.inputReceiver) {
      if(i.config.inputReceiver>std::numeric_limits<ObjectId>::max()||i.config.inputCamera>std::numeric_limits<ObjectId>::max()) {inputDiagnostic_="Referencia de entrada UI fora do mundo";runtime.cancelPointers();continue;}
      const auto handle=world.handle(static_cast<ObjectId>(i.config.inputReceiver));const auto *entity=world.find(handle);
      if(!entity||!world.activeInHierarchy(handle)||!scene::uiInputReceiverAccepts(entity->components)) {inputDiagnostic_="Receptor UI ausente, inativo ou sem Character/Motor dinâmico compatível: "+std::to_string(i.config.inputReceiver);runtime.cancelPointers();continue;}
      if(const auto *motor=entity->components.find(scene::DynamicBodyMotor::descriptor);motor&&!static_cast<const scene::DynamicBodyMotor&>(*motor).enabled){inputDiagnostic_="Motor dinâmico do receptor UI desabilitado";runtime.cancelPointers();continue;}
      const auto camera=world.handle(static_cast<ObjectId>(i.config.inputCamera));
      if(i.config.movementSpace==2&&(!world.find(camera)||!world.find(camera)->components.find(scene::Camera::descriptor))) {inputDiagnostic_="Canvas requer camera de entrada valida";runtime.cancelPointers();continue;}
      auto at=std::find_if(receivers_.begin(),receivers_.end(),[&](const auto &r){return r.receiver.id==handle.id;});
      if(at==receivers_.end()) {
        if(receivers_.size()>=32){inputDiagnostic_="Limite de 32 receptores de entrada UI excedido";runtime.cancelPointers();continue;}
        receivers_.emplace_back();at=std::prev(receivers_.end());at->receiver=handle;at->input.copyPolicyFrom(global);
      }
      if(at->receiver!=handle){at->input.reset();at->pendingJump.clear();at->receiver=handle;runtime.cancelPointers();}
      if(at->used&&(at->camera!=camera||at->space!=i.config.movementSpace)){inputDiagnostic_="Canvas do mesmo receptor usam cameras/espacos incompatíveis";runtime.cancelPointers();continue;}
      at->camera=camera;at->space=i.config.movementSpace;at->used=true;sample=&at->sample;
    }
    for(const auto &control:runtime.controls()) {
      const auto *node=runtime.document().find(control.node);if(!node)continue;
      const auto *action=global.map().find(node->control.action);
      const auto kind=node->kind==ui::GuiKind::ActionButton?ActionKind::Button:ActionKind::Axis2D;
      if(!action||action->kind!=kind||(kind==ActionKind::Button&&action->interaction!=InputInteraction::Press)) {
        inputDiagnostic_="Vinculo UI invalido: "+node->name+" -> "+node->control.action+" (botao exige Press; vetores exigem Axis2D)";
        runtime.takeControlPresses(node->id);if(node->kind==ui::GuiKind::LookArea)runtime.takeLookDelta(node->id);continue;
      }
      auto vector=control.value;
      if(node->kind==ui::GuiKind::LookArea)vector=runtime.takeLookDelta(node->id);
      const auto pulses=runtime.takeControlPresses(node->id);
      sample->virtualActions.push_back({i.id,node->id,node->control.action,kind,vector.x,vector.y,pulses});
    }
  }
  global.submit(merged,elapsed);
  for(auto &r:receivers_) {
    r.input.submit(r.sample,elapsed);
    if(!r.input.gameplayFocus()||!r.used){r.pendingJump.clear();continue;}
    if(r.input.justPressed(r.input.map().jumpAction()))for(const auto &source:r.sample.virtualActions)if(source.action==r.input.map().jumpAction()&&source.pressCount) {
      auto *instance=find(world,source.instance);const auto *state=instance?instance->runtime.controlState(source.node):nullptr;if(!state)continue;
      for(u32 count=0;count<source.pressCount;++count) {
        if(r.pendingJump.size()==32){inputDiagnostic_="Buffer de salto UI cheio (32 eventos)";break;}
        r.pendingJump.push_back({source.instance,source.node,state->cancellation});
      }
    }
    float look[2]{};r.input.axis2(r.input.map().lookAction(),look);
    if((look[0]!=0||look[1]!=0)&&world.applyCameraLook(r.camera,look[0],look[1])!=WorldStatus::Ok)inputDiagnostic_="Camera de entrada nao aceita CameraLook";
  }
}
void SceneGui::driveCharacters(GameWorld &world,ScenePhysics &physics) {
  for(auto &r:receivers_) {
    if(world.validate(r.receiver)!=WorldStatus::Ok)continue;
    float move[2]{};r.input.axis2(r.input.map().moveAction(),move);float yaw=0;
    const auto reference=r.space==2?r.camera:r.receiver;
    if(r.space) {
      float matrix[16];
      if(world.validate(reference)!=WorldStatus::Ok||!worldMatrix(world.poseGraph(),reference.id,matrix)){move[0]=move[1]=0;r.pendingJump.clear();inputDiagnostic_="Referencial de movimento UI invalido";}
      else yaw=std::atan2(matrix[8],matrix[10]);
    }
    const auto *entity=world.find(r.receiver);const bool dynamic=entity&&entity->components.find(scene::DynamicBodyMotor::descriptor);
    if(!(dynamic?physics.setDynamicMotorMove(r.receiver.id,move[0],move[1],yaw):physics.setCharacterMove(r.receiver.id,move[0],move[1],yaw))){inputDiagnostic_="Motor do receptor UI nao disponivel";continue;}
    std::erase_if(r.pendingJump,[&](const auto &pending) {
      auto *instance=find(world,pending.instance);if(!instance||!instance->enabled||instance->config.inputReceiver!=r.receiver.id)return true;
      instance->runtime.layout(instance->runtime.viewport());const auto *node=instance->runtime.document().find(pending.node);const auto *state=instance->runtime.controlState(pending.node);
      return !node||!state||state->cancellation!=pending.cancellation||node->control.action!=r.input.map().jumpAction();
    });
    if(!r.pendingJump.empty()){if(dynamic)physics.jumpDynamicMotor(r.receiver.id);else physics.jumpCharacter(r.receiver.id,&world);r.pendingJump.erase(r.pendingJump.begin());}
  }
  std::erase_if(receivers_,[&](const auto &r){return world.validate(r.receiver)!=WorldStatus::Ok||(!r.used&&r.pendingJump.empty());});
}
}
