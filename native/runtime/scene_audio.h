#pragma once
#include "runtime/game_world.h"
#include "resources/audio_clip.h"
#include <functional>
#include <memory>
namespace ae::runtime {
class SceneAudio final {
public:
  enum class Output {Device,Offline};
  enum class Command : u32 {Play,Pause,Stop,Seek,Resume};
  enum class State {Stopped,Playing,Paused,MissingClip,MissingListener,InvalidBus,Limit,DeviceError,InvalidPose};
  struct Diagnostic {ObjectId object=0;u64 instance=0;State state=State::Stopped;std::string message;double cursor=0;};
  using ClipLoader=std::function<std::shared_ptr<const resources::AudioClip>(resources::AssetGuid,std::string&)>;
  SceneAudio();~SceneAudio();
  SceneAudio(const SceneAudio&)=delete;SceneAudio &operator=(const SceneAudio&)=delete;
  void configure(ClipLoader loader,Output output=Output::Device);
  bool start(GameWorld &world);void stop();
  bool advance(GameWorld &world,double elapsed);
  WorldStatus command(GameWorld &world,const ComponentHandle &source,Command operation,double seconds=0);
  void pause(bool value); // application/editor pause suspends output; preserves cursor
  bool renderOffline(std::span<float> stereo); // explicit offline output, never a device fallback
  const std::vector<Diagnostic> &diagnostics() const;
  const Diagnostic *diagnostic(ObjectId object,u64 instance) const;
  const std::string &error() const;
  bool deviceRunning() const;
  bool wantsDevice() const;
private:
  struct Data;std::unique_ptr<Data> data_;
};
}
