#pragma once
#include "runtime/game_world.h"
#include "scene/ui_canvas.h"
#include "ui/gui_world.h"
#include "runtime/input_actions.h"
#include <functional>
#include <memory>

namespace ae::runtime {
class ScenePhysics;
ui::GuiCanvas uiCanvasPresentation(const scene::UiCanvas &canvas);
// Runtime host, usable without EditorSession/EditorPlayScene or Android.
// Immutable authored documents are shared; every lease owns its execution copy.
class SceneGui final {
public:
  using Loader=std::function<bool(resources::AssetGuid,ui::GuiDocument&,std::string&)>;
  using Occlusion=std::function<bool(ui::UiPoint,float)>;
  static constexpr u32 kMaximumInstances=64;
  struct Instance {
    u64 id=0;ComponentHandle owner{};resources::AssetGuid asset{};
    scene::UiCanvas config{};ui::GuiRuntime runtime{};ui::GuiWorldFrame frame{};
    float hostMatrix[16]{};ui::UiRect viewport{};bool enabled=false,prepared=false;
  };
  void configure(Loader loader){loader_=std::move(loader);}
  void reset();
  void invalidateResources();
  bool reconcile(GameWorld &world);
  void advance(GameWorld &world,double seconds);
  void prepare(GameWorld &world,const renderer::PerspectiveFrustum&,const ui::UiRect &viewport,const ui::UiRect &surface);
  void draw(ui::UiDrawList &out,const ui::UiFontMetrics &metrics);
  bool pointer(GameWorld &world,const ui::UiPointerEvent &event,const Occlusion &occluded={});
  void cancelPointers();
  void submitInput(GameWorld &,InputService &,const InputDeviceState &,double elapsed);
  void driveCharacters(GameWorld &,ScenePhysics &);
  const InputService *inputFor(ObjectId receiver) const;
  bool captures(u32 pointer,ui::UiPointerDevice device=ui::UiPointerDevice::Touch) const;
  void setImages(const ui::GuiImageAtlas *images);
  Instance *find(GameWorld &world,u64 instance);
  u64 instanceFor(GameWorld &world,ObjectId object,u64 component=0);
  std::span<const std::unique_ptr<Instance>> instances() const {return instances_;}
  const std::string &diagnostic() const {return diagnostic_.empty()?inputDiagnostic_:diagnostic_;}
  u64 revision() const {return revision_;}
private:
  Loader loader_{};
  std::vector<std::unique_ptr<Instance>> instances_;
  std::unordered_map<resources::AssetGuid,std::shared_ptr<const ui::GuiDocument>,resources::AssetGuidHash> documents_;
  std::unordered_map<resources::AssetGuid,std::string,resources::AssetGuidHash> failures_;
  const ui::GuiImageAtlas *images_=nullptr;
  ui::UiDrawList scratch_{};
  struct PointerRoute {u32 pointer;ui::UiPointerDevice device;u64 instance;};
  // A retired owner keeps the gesture consumed until release. It never routes
  // a held pointer to a replacement lease or to gameplay.
  std::vector<PointerRoute> routes_;
  u32 world_=0;u64 nextId_=1,graphRevision_=~u64{0},revision_=0;
  std::string diagnostic_,inputDiagnostic_;
  struct PendingJump {u64 instance=0;ui::GuiId node=0;u64 cancellation=0;};
  struct ReceiverInput {ObjectHandle receiver{},camera{};u32 space=0;std::vector<PendingJump> pendingJump;bool used=false;InputService input;InputDeviceState sample;};
  std::vector<ReceiverInput> receivers_;
};
}
