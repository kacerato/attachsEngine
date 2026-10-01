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
  bool enabled=true,loop=false,mute=false;
  AudioPlayback playback=AudioPlayback::Playing;AudioDimension dimension=AudioDimension::Flat;AudioRolloff rolloff=AudioRolloff::Inverse;
  float volume=1,pitch=1,pan=0,minDistance=1,maxDistance=100,rolloffFactor=1,coneInner=360,coneOuter=360,coneGain=0,doppler=1;
  static const ComponentType descriptor;
  const ComponentType &type() const override{return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override{return std::make_unique<AudioSource>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) if(!std::isfinite(p.read(*this)) || p.read(*this)<p.minimum || p.read(*this)>p.maximum) return false;
    return bus<=std::numeric_limits<u32>::max() && u32(playback)<=2 && u32(dimension)<=1 && u32(rolloff)<=2 && minDistance<maxDistance && coneInner<=coneOuter;
  }
  void write(std::ostream &o) const override {o<<(clip.valid()?clip.text():"-")<<' '<<bus<<' '<<enabled<<' '<<loop<<' '<<mute<<' '<<u32(playback)<<' '<<u32(dimension)<<' '<<u32(rolloff);for(const auto &p:descriptor.numbers)o<<' '<<p.read(*this);}
  bool read(std::istream &i,u32 v) override {std::string c;u32 a,b,d;if(v!=1 || !(i>>c>>bus>>enabled>>loop>>mute>>a>>b>>d))return false;if(c!="-"&&!resources::AssetGuid::parse(c,clip))return false;playback=AudioPlayback(a);dimension=AudioDimension(b);rolloff=AudioRolloff(d);for(const auto &p:descriptor.numbers)if(!(i>>*p.write(*this)))return false;return valid();}
};
class AudioListener final:public ComponentValue {
public:
  bool enabled=true;float volume=1,priority=0;
  static const ComponentType descriptor;
  const ComponentType &type() const override{return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override{return std::make_unique<AudioListener>(*this);}
  bool valid() const override{return std::isfinite(volume)&&volume>=0&&volume<=1&&std::isfinite(priority)&&priority>=0&&priority<=255&&std::floor(priority)==priority;}
  void write(std::ostream &o) const override{o<<enabled<<' '<<volume<<' '<<priority;}
  bool read(std::istream &i,u32 v) override{return v==1&&bool(i>>enabled>>volume>>priority)&&valid();}
};
class AudioBus final:public ComponentValue {
public:
  bool enabled=true,mute=false,solo=false;float volume=1;u64 output=0;
  static const ComponentType descriptor;
  const ComponentType &type() const override{return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override{return std::make_unique<AudioBus>(*this);}
  bool valid() const override{return output<=std::numeric_limits<u32>::max()&&std::isfinite(volume)&&volume>=0&&volume<=1;}
  void write(std::ostream &o) const override{o<<enabled<<' '<<mute<<' '<<solo<<' '<<volume<<' '<<output;}
  bool read(std::istream &i,u32 v) override{return v==1&&bool(i>>enabled>>mute>>solo>>volume>>output)&&valid();}
};
inline bool audioSpatial(const ComponentValue &v){return static_cast<const AudioSource&>(v).dimension==AudioDimension::Spatial;}
inline bool audioFlat(const ComponentValue &v){return !audioSpatial(v);}
#define AE_AUDIO_NUMBER(T,id,label,member,minimum,maximum,step,group,unit,visible) {label,minimum,maximum,step,[](const ComponentValue &v)->const float&{return static_cast<const T&>(v).member;},[](ComponentValue &v)->float*{return &static_cast<T&>(v).member;},id,{group,unit,nullptr,visible}}
#define AE_AUDIO_BOOL(T,id,label,member,group) {id,label,[](const ComponentValue &v){return static_cast<const T&>(v).member;},[](ComponentValue &v,bool b){static_cast<T&>(v).member=b;},{group}}
inline constexpr std::array<ComponentNumber,10> audioSourceNumbers{{
 AE_AUDIO_NUMBER(AudioSource,"volume","Volume",volume,0,1,.01f,"Som","",nullptr),
 AE_AUDIO_NUMBER(AudioSource,"pitch","Velocidade / pitch",pitch,.1f,4,.01f,"Reprodução","×",nullptr),
 AE_AUDIO_NUMBER(AudioSource,"pan","Pan estéreo",pan,-1,1,.01f,"Espaço","",audioFlat),
 AE_AUDIO_NUMBER(AudioSource,"min_distance","Distância mínima",minDistance,.01f,100000,.1f,"Espaço","m",audioSpatial),
 AE_AUDIO_NUMBER(AudioSource,"max_distance","Distância máxima",maxDistance,.02f,100001,.1f,"Espaço","m",audioSpatial),
 AE_AUDIO_NUMBER(AudioSource,"rolloff_factor","Decaimento",rolloffFactor,0,10,.1f,"Espaço","",audioSpatial),
 AE_AUDIO_NUMBER(AudioSource,"cone_inner","Cone interno",coneInner,0,360,1,"Emissão","°",audioSpatial),
 AE_AUDIO_NUMBER(AudioSource,"cone_outer","Cone externo",coneOuter,0,360,1,"Emissão","°",audioSpatial),
 AE_AUDIO_NUMBER(AudioSource,"cone_gain","Ganho fora do cone",coneGain,0,1,.01f,"Emissão","",audioSpatial),
 AE_AUDIO_NUMBER(AudioSource,"doppler","Doppler",doppler,0,4,.1f,"Emissão","×",audioSpatial)
}};
inline constexpr std::array<ComponentBoolean,3> audioSourceBooleans{{AE_AUDIO_BOOL(AudioSource,"enabled","Ativo",enabled,"Som"),AE_AUDIO_BOOL(AudioSource,"mute","Silenciar",mute,"Som"),AE_AUDIO_BOOL(AudioSource,"loop","Repetir",loop,"Reprodução")}};
inline constexpr std::array<ComponentEnumOption,3> audioPlaybackOptions{{{0,"Parar"},{1,"Tocar"},{2,"Pausar"}}};
inline constexpr std::array<ComponentEnumOption,2> audioDimensionOptions{{{0,"2D / estéreo"},{1,"3D / espacial"}}};
inline constexpr std::array<ComponentEnumOption,3> audioRolloffOptions{{{0,"Linear"},{1,"Inverso"},{2,"Exponencial"}}};
inline constexpr std::array<ComponentEnum,3> audioSourceEnums{{
 {"playback","Pedido",audioPlaybackOptions,[](const ComponentValue &v){return u32(static_cast<const AudioSource&>(v).playback);},[](ComponentValue &v,u32 x){static_cast<AudioSource&>(v).playback=AudioPlayback(x);},{"Reprodução","","Pedido persistido; consulte o estado real na nota do componente."}},
 {"dimension","Dimensão",audioDimensionOptions,[](const ComponentValue &v){return u32(static_cast<const AudioSource&>(v).dimension);},[](ComponentValue &v,u32 x){static_cast<AudioSource&>(v).dimension=AudioDimension(x);},{"Espaço"}},
 {"rolloff","Atenuação",audioRolloffOptions,[](const ComponentValue &v){return u32(static_cast<const AudioSource&>(v).rolloff);},[](ComponentValue &v,u32 x){static_cast<AudioSource&>(v).rolloff=AudioRolloff(x);},{"Espaço","",nullptr,audioSpatial}}
}};
inline constexpr std::array<ComponentResourceBinding,1> audioSourceResources{{{"clip","Clipe WAV",resources::AssetType::AudioClip,[](const ComponentValue&)->u32{return 1;},[](const ComponentValue &v,u32){return static_cast<const AudioSource&>(v).clip;},[](ComponentValue &v,u32 slot,resources::AssetGuid a){if(slot)return false;static_cast<AudioSource&>(v).clip=a;return true;},{"Som"}}}};
inline constexpr std::array<ComponentObjectReference,1> audioSourceReferences{{{"bus","Bus","astra.audio.bus",ObjectReferenceScope::Any,"Master",[](const ComponentValue &v){return static_cast<const AudioSource&>(v).bus;},[](ComponentValue &v,u64 b){static_cast<AudioSource&>(v).bus=b;},{"Som"}}}};
inline constexpr std::array<ComponentNumber,2> audioListenerNumbers{{AE_AUDIO_NUMBER(AudioListener,"volume","Volume global",volume,0,1,.01f,"Escuta","",nullptr),AE_AUDIO_NUMBER(AudioListener,"priority","Prioridade",priority,0,255,1,"Escuta","",nullptr)}};
inline constexpr std::array<ComponentBoolean,1> audioListenerBooleans{{AE_AUDIO_BOOL(AudioListener,"enabled","Ativo",enabled,"Escuta")}};
inline constexpr std::array<ComponentNumber,1> audioBusNumbers{{AE_AUDIO_NUMBER(AudioBus,"volume","Ganho",volume,0,1,.01f,"Mixer","",nullptr)}};
inline constexpr std::array<ComponentBoolean,3> audioBusBooleans{{AE_AUDIO_BOOL(AudioBus,"enabled","Ativo",enabled,"Mixer"),AE_AUDIO_BOOL(AudioBus,"mute","Silenciar",mute,"Mixer"),AE_AUDIO_BOOL(AudioBus,"solo","Solo",solo,"Mixer")}};
inline constexpr std::array<ComponentObjectReference,1> audioBusReferences{{{"output","Saída","astra.audio.bus",ObjectReferenceScope::Other,"Master",[](const ComponentValue &v){return static_cast<const AudioBus&>(v).output;},[](ComponentValue &v,u64 b){static_cast<AudioBus&>(v).output=b;},{"Mixer"}}}};
#undef AE_AUDIO_NUMBER
#undef AE_AUDIO_BOOL
inline const ComponentType AudioSource::descriptor{"astra.audio.source",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<AudioSource>();},audioSourceNumbers,audioSourceBooleans,audioSourceEnums,nullptr,false,audioSourceReferences,{},audioSourceResources};
inline const ComponentType AudioListener::descriptor{"astra.audio.listener",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<AudioListener>();},audioListenerNumbers,audioListenerBooleans};
inline const ComponentType AudioBus::descriptor{"astra.audio.bus",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<AudioBus>();},audioBusNumbers,audioBusBooleans,{},nullptr,false,audioBusReferences};
}
