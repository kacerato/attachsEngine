#include "runtime/scene_gui.h"
#include "runtime/scene_physics.h"
#include "scene/character.h"
#include "scene/camera.h"
#include "scene/virtual_camera.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ae::runtime {
const InputService *SceneGui::inputFor(ObjectId receiver) const {for(const auto &r:receivers_)if(r.receiver.id==receiver)return &r.input;return nullptr;}
void SceneGui::submitInput(GameWorld &world,InputService &global,const InputDeviceState &hardware,double elapsed) {
  reconcile(world);inputDiagnostic_.clear();InputDeviceState merged=hardware;
  const u32 touchSources=(1u<<u32(InputSource::TouchMove))|(1u<<u32(InputSource::TouchLook))|(1u<<u32(InputSource::TouchButton));
  if(!global.gameplayFocus()||global.captureStatus()==InputCaptureStatus::Waiting||(hardware.canceledDeviceGroups&InputTouch)||(hardware.canceledSources&touchSources))cancelPointers();
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
      if(sample==&merged&&pulses&&node->control.action==global.map().jumpAction())if(const auto *state=runtime.controlState(node->id))
        for(u32 count=0;count<pulses&&rootPendingJump_.size()<32;++count)rootPendingJump_.push_back({i.id,node->id,state->cancellation});
      sample->virtualActions.push_back({i.id,node->id,node->control.action,kind,vector.x,vector.y,pulses});
    }
  }
  auto &touch=sourceSamples_[0];auto &keyboard=sourceSamples_[1];auto &gamepad=sourceSamples_[2];
  keyboard=hardware;gamepad=hardware;touch=merged;
  keyboard.virtualActions.clear();keyboard.moveX=keyboard.moveY=keyboard.lookX=keyboard.lookY=0;keyboard.touchButtons=0;keyboard.gamepadButtons.clear();keyboard.gamepadAxes={};
  gamepad.virtualActions.clear();gamepad.moveX=gamepad.moveY=gamepad.lookX=gamepad.lookY=0;gamepad.touchButtons=0;gamepad.keys.clear();gamepad.mouseButtons=0;gamepad.mouseAxes={};
  touch.keys.clear();touch.gamepadButtons.clear();touch.gamepadAxes={};touch.mouseButtons=0;touch.mouseAxes={};
  // A composite action may bind all device groups. Cancellation belongs to
  // its origin; disconnecting a pad must not cancel the keyboard channel.
  keyboard.canceledDeviceGroups&=InputKeyboardMouse;keyboard.canceledSources&=(1u<<u32(InputSource::Key))|(1u<<u32(InputSource::MouseButton))|(1u<<u32(InputSource::MouseAxis));
  gamepad.canceledDeviceGroups&=InputGamepad;gamepad.canceledSources&=(1u<<u32(InputSource::GamepadAxis))|(1u<<u32(InputSource::GamepadButton));
  touch.canceledDeviceGroups&=InputTouch;touch.canceledSources&=(1u<<u32(InputSource::TouchMove))|(1u<<u32(InputSource::TouchLook))|(1u<<u32(InputSource::TouchButton));
  const InputDeviceState *origins[]{&touch,&keyboard,&gamepad};
  rootSample_=touch;rootSample_.virtualActions.clear();rootControlInput_.copyPolicyFrom(global);rootControlInput_.submit(rootSample_,elapsed);
  rootHardwareJump_|=rootControlInput_.justPressed(rootControlInput_.map().jumpAction());
  for(u32 n=0;n<3;++n){sourceInputs_[n].copyPolicyFrom(global);sourceInputs_[n].submit(*origins[n],elapsed);}
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
    const auto *view=world.find(r.camera);
    const auto *brain=view?view->components.find(scene::CameraBrain::descriptor):nullptr;
    if((look[0]!=0||look[1]!=0)&&!(brain&&static_cast<const scene::CameraBrain&>(*brain).enabled)&&world.applyCameraLook(r.camera,look[0],look[1])!=WorldStatus::Ok)inputDiagnostic_="Camera de entrada nao aceita CameraLook";
  }
}
void SceneGui::driveDefaultControl(GameWorld &world,ScenePhysics &physics,ObjectId target,float yaw) {
  if(!target||!world.activeInHierarchy(world.handle(target))){rootPendingJump_.clear();rootHardwareJump_=false;return;}
  auto &sample=rootStepSample_;sample=rootSample_;
  for(auto &instance:instances_)if(instance->enabled&&!instance->config.inputReceiver&&find(world,instance->id)) {
    auto &runtime=instance->runtime;runtime.layout(runtime.viewport());
    for(const auto &control:runtime.controls())if(const auto *node=runtime.document().find(control.node);node&&node->kind==ui::GuiKind::Joystick)
      sample.virtualActions.push_back({instance->id,node->id,node->control.action,ActionKind::Axis2D,control.value.x,control.value.y,0});
  }
  rootControlInput_.submit(sample);float move[2]{};rootControlInput_.axis2(rootControlInput_.map().moveAction(),move);
  for(auto &axis:move)axis=std::clamp(axis,-1.f,1.f);
  std::erase_if(rootPendingJump_,[&](const auto &pending){auto *instance=find(world,pending.instance);if(!instance||!instance->enabled||instance->config.inputReceiver)return true;
    const auto *node=instance->runtime.document().find(pending.node);const auto *state=instance->runtime.controlState(pending.node);
    return !node||!state||state->cancellation!=pending.cancellation||node->control.action!=rootControlInput_.map().jumpAction();});
  const bool jump=rootHardwareJump_||!rootPendingJump_.empty();
  physics.submitMotorControl(target,MotorControlSource::Ui,move[0],move[1],yaw,jump);
  rootHardwareJump_=false;if(!rootPendingJump_.empty())rootPendingJump_.erase(rootPendingJump_.begin());
}
void SceneGui::driveCharacters(GameWorld &world,ScenePhysics &physics) {
  for(auto &r:receivers_) {
    if(world.validate(r.receiver)!=WorldStatus::Ok)continue;
    // Re-read live UI after script mutations/pointer cancellation. A deleted or
    // retargeted canvas must not leave a vector on its previous recipient.
    r.sample.virtualActions.clear();
    for(auto &instance:instances_)if(instance->enabled&&find(world,instance->id)&&instance->config.inputReceiver==r.receiver.id&&instance->config.movementSpace==r.space&&instance->config.inputCamera==r.camera.id) {
      auto &runtime=instance->runtime;runtime.layout(runtime.viewport());
      for(const auto &control:runtime.controls())if(const auto *node=runtime.document().find(control.node);node&&node->kind==ui::GuiKind::Joystick)
        r.sample.virtualActions.push_back({instance->id,node->id,node->control.action,ActionKind::Axis2D,control.value.x,control.value.y,0});
    }
    r.controlInput.copyPolicyFrom(r.input);r.controlInput.submit(r.sample);
    float move[2]{};r.controlInput.axis2(r.input.map().moveAction(),move);float yaw=0;
    for(auto &axis:move)axis=std::clamp(axis,-1.f,1.f);
    const auto reference=r.space==2?r.camera:r.receiver;
    if(r.space) {
      float matrix[16];
      if(world.validate(reference)!=WorldStatus::Ok||!worldMatrix(world.poseGraph(),reference.id,matrix)){move[0]=move[1]=0;r.pendingJump.clear();inputDiagnostic_="Referencial de movimento UI invalido";}
      else yaw=std::atan2(matrix[8],matrix[10]);
    }
    const auto *entity=world.find(r.receiver);const bool dynamic=entity&&entity->components.find(scene::DynamicBodyMotor::descriptor);
    if(!physics.submitMotorControl(r.receiver.id,MotorControlSource::Ui,move[0],move[1],yaw)){r.pendingJump.clear();if(r.input.gameplayFocus())inputDiagnostic_="Motor do receptor UI não disponível";continue;}
    std::erase_if(r.pendingJump,[&](const auto &pending) {
      auto *instance=find(world,pending.instance);if(!instance||!instance->enabled||instance->config.inputReceiver!=r.receiver.id)return true;
      instance->runtime.layout(instance->runtime.viewport());const auto *node=instance->runtime.document().find(pending.node);const auto *state=instance->runtime.controlState(pending.node);
      return !node||!state||state->cancellation!=pending.cancellation||node->control.action!=r.input.map().jumpAction();
    });
    if(!r.pendingJump.empty()){if(dynamic)physics.jumpDynamicMotor(r.receiver.id,MotorControlSource::Ui);else physics.jumpCharacter(r.receiver.id,&world,MotorControlSource::Ui);r.pendingJump.erase(r.pendingJump.begin());}
  }
  std::erase_if(receivers_,[&](const auto &r){return world.validate(r.receiver)!=WorldStatus::Ok||(!r.used&&r.pendingJump.empty());});
}
}
