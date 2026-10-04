#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
enum class AudioPlayback:u32 {Stopped,Playing,Paused};
enum class AudioDimension:u32 {Flat,Spatial};
enum class AudioRolloff:u32 {Linear,Inverse,Exponential};
class AudioSource final:public ComponentValue {
public:
  resources::AssetGuid clip{};u64 bus=0;

#include "scene/generated/audio_AudioSource_fields0.inc"

  AudioPlayback playback=AudioPlayback::Playing;AudioDimension dimension=AudioDimension::Flat;AudioRolloff rolloff=AudioRolloff::Inverse;

#include "scene/generated/audio_AudioSource_fields1.inc"

  static const ComponentType descriptor;
  const ComponentType &type() const override{return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override{return std::make_unique<AudioSource>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) if(!std::isfinite(p.read(*this)) || p.read(*this)<p.minimum || p.read(*this)>p.maximum) return false;
    return bus<=std::numeric_limits<u32>::max() && u32(playback)<=2 && u32(dimension)<=1 && u32(rolloff)<=2 && minDistance<maxDistance && coneInner<=coneOuter;
  }
  void write(std::ostream &o) const override {o<<(clip.valid()?clip.text():"-")<<' '<<bus<<' '<<enabled<<' '<<loop<<' '<<mute<<' '<<u32(playback)<<' '<<u32(dimension)<<' '<<u32(rolloff);for(const auto &p:descriptor.numbers)o<<' '<<p.read(*this);}
  bool read(std::istream &i,u32 v) override {std::string c;u32 a,b,d;if(v!=1 || !(i>>c>>bus>>enabled>>loop>>mute>>a>>b>>d))return false;if(c=="-")clip={};else if(!resources::AssetGuid::parse(c,clip))return false;playback=AudioPlayback(a);dimension=AudioDimension(b);rolloff=AudioRolloff(d);for(const auto &p:descriptor.numbers)if(!(i>>*p.write(*this)))return false;return valid();}
};
class AudioListener final:public ComponentValue {
public:

#include "scene/generated/audio_AudioListener_fields0.inc"

#include "scene/generated/audio_AudioListener_fields1.inc"

  static const ComponentType descriptor;
  const ComponentType &type() const override{return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override{return std::make_unique<AudioListener>(*this);}
  bool valid() const override{return std::isfinite(volume)&&volume>=0&&volume<=1&&std::isfinite(priority)&&priority>=0&&priority<=255&&std::floor(priority)==priority;}
  void write(std::ostream &o) const override{o<<enabled<<' '<<volume<<' '<<priority;}
  bool read(std::istream &i,u32 v) override{return v==1&&bool(i>>enabled>>volume>>priority)&&valid();}
};
class AudioBus final:public ComponentValue {
public:

#include "scene/generated/audio_AudioBus_fields0.inc"

#include "scene/generated/audio_AudioBus_fields1.inc"
u64 output=0;
  static const ComponentType descriptor;
  const ComponentType &type() const override{return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override{return std::make_unique<AudioBus>(*this);}
  bool valid() const override{return output<=std::numeric_limits<u32>::max()&&std::isfinite(volume)&&volume>=0&&volume<=1;}
  void write(std::ostream &o) const override{o<<enabled<<' '<<mute<<' '<<solo<<' '<<volume<<' '<<output;}
  bool read(std::istream &i,u32 v) override{return v==1&&bool(i>>enabled>>mute>>solo>>volume>>output)&&valid();}
};
inline bool audioSpatial(const ComponentValue &v){return static_cast<const AudioSource&>(v).dimension==AudioDimension::Spatial;}
inline bool audioFlat(const ComponentValue &v){return !audioSpatial(v);}

#include "scene/generated/audio_audioSourceNumbers.inc"
#include "scene/generated/audio_audioSourceBooleans.inc"
inline constexpr std::array<ComponentEnumOption,3> audioPlaybackOptions{{{0,"Parar"},{1,"Tocar"},{2,"Pausar"}}};
inline constexpr std::array<ComponentEnumOption,2> audioDimensionOptions{{{0,"2D / estéreo"},{1,"3D / espacial"}}};
inline constexpr std::array<ComponentEnumOption,3> audioRolloffOptions{{{0,"Linear"},{1,"Inverso"},{2,"Exponencial"}}};
#include "scene/generated/audio_audioSourceEnums.inc"
inline constexpr std::array<ComponentResourceBinding,1> audioSourceResources{{{"clip","Clipe WAV",resources::AssetType::AudioClip,[](const ComponentValue&)->u32{return 1;},[](const ComponentValue &v,u32){return static_cast<const AudioSource&>(v).clip;},[](ComponentValue &v,u32 slot,resources::AssetGuid a){if(slot)return false;static_cast<AudioSource&>(v).clip=a;return true;},{"Som"}}}};
inline constexpr std::array<ComponentObjectReference,1> audioSourceReferences{{{"bus","Bus","astra.audio.bus",ObjectReferenceScope::Any,"Master",[](const ComponentValue &v){return static_cast<const AudioSource&>(v).bus;},[](ComponentValue &v,u64 b){static_cast<AudioSource&>(v).bus=b;},{"Som"}}}};
#include "scene/generated/audio_audioListenerNumbers.inc"
#include "scene/generated/audio_audioListenerBooleans.inc"
#include "scene/generated/audio_audioBusNumbers.inc"
#include "scene/generated/audio_audioBusBooleans.inc"
inline constexpr std::array<ComponentObjectReference,1> audioBusReferences{{{"output","Saída","astra.audio.bus",ObjectReferenceScope::Other,"Master",[](const ComponentValue &v){return static_cast<const AudioBus&>(v).output;},[](ComponentValue &v,u64 b){static_cast<AudioBus&>(v).output=b;},{"Mixer"}}}};

inline const ComponentType AudioSource::descriptor{"astra.audio.source",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<AudioSource>();},audioSourceNumbers,audioSourceBooleans,audioSourceEnums,nullptr,false,audioSourceReferences,{},audioSourceResources};
inline const ComponentType AudioListener::descriptor{"astra.audio.listener",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<AudioListener>();},audioListenerNumbers,audioListenerBooleans};
inline const ComponentType AudioBus::descriptor{"astra.audio.bus",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<AudioBus>();},audioBusNumbers,audioBusBooleans,{},nullptr,false,audioBusReferences};
}
