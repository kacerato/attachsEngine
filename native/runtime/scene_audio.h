#pragma once
#include "runtime/game_world.h"
#include "resources/audio_clip.h"
#include <functional>
#include <memory>
namespace ae::runtime {
// Áudio da cena em Play (miniaudio 0.11.23). Cada Bus de áudio é um nó DSP com
// ganho, cadeia de efeitos, envios e medidor; as fontes são vozes com
// espacialização própria (mistura espacial contínua), prioridade e vozes
// virtuais acima do limite; Snapshots transicionam propriedades do mixer.
class SceneAudio final {
public:
  enum class Output {Device,Offline};
  enum class Command : u32 {Play,Pause,Stop,Seek,Resume};
  enum class State {Stopped,Playing,Paused,MissingClip,MissingListener,InvalidBus,Limit,DeviceError,InvalidPose,Virtual};
  struct Diagnostic {ObjectId object=0;u64 instance=0;State state=State::Stopped;std::string message;double cursor=0;bool streaming=false;};
  // Medidor e estado de um bus: nível de saída (pico com queda de 20 dB/s), efeitos e envios.
  struct BusMeter {ObjectId bus=0;float peak=0,rms=0;u32 effects=0,sends=0;bool audible=true;std::string message;};
  using ClipLoader=std::function<std::shared_ptr<const resources::AudioClip>(resources::AssetGuid,std::string&)>;
  // Caminho absoluto de um WAV registrado, para streaming; falso com o motivo.
  using StreamResolver=std::function<bool(resources::AssetGuid,std::string &path,std::string &error)>;
  static constexpr u32 DefaultVoiceLimit=32,MaximumVoices=256;
  SceneAudio();~SceneAudio();
  SceneAudio(const SceneAudio&)=delete;SceneAudio &operator=(const SceneAudio&)=delete;
  void configure(ClipLoader loader,Output output=Output::Device);
  void configureStreams(StreamResolver resolver);
  bool setVoiceLimit(u32 voices);
  u32 voiceLimit() const;
  bool start(GameWorld &world);void stop();
  bool advance(GameWorld &world,double elapsed);
  WorldStatus command(GameWorld &world,const ComponentHandle &source,Command operation,double seconds=0);
  // Snapshot: seconds<0 usa a Transição do componente; zero aplica na hora.
  WorldStatus transitionSnapshot(GameWorld &world,const ComponentHandle &snapshot,double seconds=-1);
  void pause(bool value); // application/editor pause suspends output; preserves cursor
  bool renderOffline(std::span<float> stereo); // explicit offline output, never a device fallback
  const std::vector<Diagnostic> &diagnostics() const;
  const Diagnostic *diagnostic(ObjectId object,u64 instance) const;
  const BusMeter *busMeter(ObjectId bus) const;
  float compressorReduction(ObjectId bus,u64 instance) const; // dB reduzidos no último bloco
  bool isVirtual(ObjectId object,u64 instance) const;
  u32 realVoices() const;u32 virtualVoices() const;
  u32 activeTransitions() const;
  const std::string &error() const;
  bool deviceRunning() const;
  bool wantsDevice() const;
private:
  struct Data;std::unique_ptr<Data> data_;
};
}
