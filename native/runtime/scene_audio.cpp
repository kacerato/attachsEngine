#include "runtime/scene_audio.h"
#include "scene/audio.h"
#include "miniaudio.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace ae::runtime {
struct SceneAudio::Data {
  struct Voice {
    ObjectId object=0;u64 instance=0;resources::AssetGuid asset{};
    std::shared_ptr<const resources::AudioClip> clip;
    ma_audio_buffer buffer{};ma_sound sound{};bool bufferReady=false,soundReady=false,seen=false;
    scene::AudioPlayback command=scene::AudioPlayback::Stopped;
    bool everStarted=false,suspended=false,positionReady=false;
    float position[3]{};
    ~Voice(){if(soundReady)ma_sound_uninit(&sound);if(bufferReady)ma_audio_buffer_uninit(&buffer);}
  };
  ma_engine engine{};bool ready=false,paused=false,deviceAttempted=false;u32 world=0;
  ClipLoader loader;Output output=Output::Device;std::string error;
  std::map<std::pair<ObjectId,u64>,std::unique_ptr<Voice>> voices;
  std::vector<Diagnostic> diagnostics;
  std::vector<ObjectId> ids;
  u64 revision=0;ObjectId listenerObject=0;u64 listenerInstance=0;float listenerPosition[3]{};bool listenerPositionReady=false;
};
SceneAudio::SceneAudio():data_(std::make_unique<Data>()){}
SceneAudio::~SceneAudio(){stop();}
void SceneAudio::configure(ClipLoader loader,Output output){stop();data_->loader=std::move(loader);data_->output=output;}
void SceneAudio::stop(){auto &d=*data_;d.voices.clear();if(d.ready)ma_engine_uninit(&d.engine);d.ready=false;d.deviceAttempted=false;d.world=0;d.revision=0;d.ids.clear();d.diagnostics.clear();d.error.clear();d.listenerPositionReady=false;d.paused=false;}
bool SceneAudio::start(GameWorld &world){const bool paused=data_->paused;stop();data_->paused=paused;data_->world=world.worldId();return advance(world,0);}
const std::vector<SceneAudio::Diagnostic> &SceneAudio::diagnostics()const{return data_->diagnostics;}
const SceneAudio::Diagnostic *SceneAudio::diagnostic(ObjectId o,u64 i)const{for(const auto &v:data_->diagnostics)if(v.object==o&&v.instance==i)return &v;return nullptr;}
const std::string &SceneAudio::error()const{return data_->error;}
bool SceneAudio::deviceRunning()const{return data_->ready&&data_->output==Output::Device&&!data_->paused;}
bool SceneAudio::wantsDevice()const{
  const auto &d=*data_;
  if(!d.ready||d.output!=Output::Device) return false;
  for(const auto &[key,voice]:d.voices) {
    (void)key;
    if(voice->command==scene::AudioPlayback::Playing&&ma_sound_is_playing(&voice->sound)&&!ma_sound_at_end(&voice->sound)) return true;
  }
  return false;
}
void SceneAudio::pause(bool value){auto &d=*data_;if(d.paused==value)return;d.paused=value;if(d.ready&&d.output==Output::Device){const auto r=value?ma_engine_stop(&d.engine):ma_engine_start(&d.engine);if(r!=MA_SUCCESS)d.error="Falha ao suspender/retomar dispositivo de áudio";}}
bool SceneAudio::renderOffline(std::span<float> stereo){
  auto &d=*data_;
  if(d.output!=Output::Offline || !d.ready || stereo.size()%2)return false;
  // An initialized output with no attached sources is valid silence. Avoid
  // reading the empty graph or exposing retained samples from an earlier block.
  // Readiness checks stay above this branch: Stop/device errors are not hidden.
  if(d.paused||d.voices.empty()){std::fill(stereo.begin(),stereo.end(),0);return true;}
  ma_uint64 read=0;
  return ma_engine_read_pcm_frames(&d.engine,stereo.data(),stereo.size()/2,&read)==MA_SUCCESS&&read==stereo.size()/2;
}

bool SceneAudio::advance(GameWorld &world,double elapsed){
  auto &d=*data_;d.diagnostics.clear();
  if(!world.running()||!std::isfinite(elapsed)||elapsed<0||elapsed>.25){d.error="Mundo ou delta de áudio inválido";return false;}
  if(d.world!=world.worldId()){stop();d.world=world.worldId();}
  if(d.revision!=world.structuralRevision()){
    d.ids.clear();std::vector<ObjectId> all;world.graph().collectSubtree(world.graph().root(),all);
    for(auto id:all)if(const auto *o=world.graph().find(id))if(o->components.find(scene::AudioSource::descriptor)||o->components.find(scene::AudioListener::descriptor)||o->components.find(scene::AudioBus::descriptor))d.ids.push_back(id);
    d.revision=world.structuralRevision();
  }
  const SceneObject *listener=nullptr;const scene::AudioListener *listening=nullptr;
  bool solo=false,hasSources=false;
  for(auto id:d.ids)if(const auto *o=world.graph().find(id);o&&world.activeInHierarchy(world.handle(id))){
    if(const auto *c=o->components.find(scene::AudioListener::descriptor)){const auto *l=static_cast<const scene::AudioListener*>(c);if(l->enabled&&(!listening||l->priority>listening->priority)){listener=o;listening=l;}}
    if(const auto *c=o->components.find(scene::AudioBus::descriptor)){const auto &b=static_cast<const scene::AudioBus&>(*c);solo|=b.enabled&&b.solo;}
    if(const auto *c=o->components.find(scene::AudioSource::descriptor))hasSources|=static_cast<const scene::AudioSource*>(c)->enabled;
  }
  if(!hasSources){d.voices.clear();return true;}
  if(!d.ready){
    bool needsOutput=false;
    for(auto id:d.ids)if(const auto *o=world.graph().find(id);o&&world.activeInHierarchy(world.handle(id)))
      if(const auto *c=o->components.find(scene::AudioSource::descriptor)) {
        const auto &s=static_cast<const scene::AudioSource&>(*c);
        needsOutput|=s.enabled&&s.clip.valid()&&(s.dimension==scene::AudioDimension::Flat||listener);
      }
    if(!needsOutput) {
      for(auto id:d.ids)if(const auto *o=world.graph().find(id))if(const auto *c=o->components.find(scene::AudioSource::descriptor)) {
        const auto &s=static_cast<const scene::AudioSource&>(*c);
        const bool active=s.enabled&&world.activeInHierarchy(world.handle(id));
        const auto state=!active?State::Stopped:!s.clip.valid()?State::MissingClip:State::MissingListener;
        d.diagnostics.push_back({id,c->instanceId(),state,!active?"Inativo":!s.clip.valid()?"Atribua um clipe WAV":"Áudio 3D requer listener ativo",0});
      }
      return true;
    }
    auto config=ma_engine_config_init();config.channels=2;config.sampleRate=resources::AudioClip::SampleRate;config.listenerCount=1;
    config.noDevice=d.output==Output::Offline;config.noAutoStart=d.paused;
    const bool failed=d.deviceAttempted || ma_engine_init(&config,&d.engine)!=MA_SUCCESS;
    d.deviceAttempted=true;
    if(failed){
      d.error="Dispositivo de áudio indisponível; saída não foi simulada";
      for(auto id:d.ids)if(const auto *o=world.graph().find(id))if(const auto *c=o->components.find(scene::AudioSource::descriptor))d.diagnostics.push_back({id,c->instanceId(),State::DeviceError,d.error,0});
      return true; // Visual/gameplay survives; audio availability remains explicit.
    }
    d.ready=true;d.error.clear();
  }
  float listenerMatrix[16]{};
  const auto validDirection=[](const float *m,u32 column){const float length=std::hypot(m[column],m[column+1],m[column+2]);return std::isfinite(length)&&length>1e-6f;};
  if(listener && worldMatrix(world.graph(),listener->id,listenerMatrix)&&validDirection(listenerMatrix,8)&&validDirection(listenerMatrix,4)){
    if(d.listenerObject!=listener->id||d.listenerInstance!=listening->instanceId()) d.listenerPositionReady=false;
    d.listenerObject=listener->id;d.listenerInstance=listening->instanceId();
    const auto unit=[](float x,float y,float z){const float l=std::sqrt(x*x+y*y+z*z);return std::array<float,3>{x/l,y/l,z/l};};
    const auto forward=unit(listenerMatrix[8],listenerMatrix[9],listenerMatrix[10]),up=unit(listenerMatrix[4],listenerMatrix[5],listenerMatrix[6]);
    ma_engine_listener_set_position(&d.engine,0,listenerMatrix[12],listenerMatrix[13],listenerMatrix[14]);
    ma_engine_listener_set_direction(&d.engine,0,forward[0],forward[1],forward[2]);ma_engine_listener_set_world_up(&d.engine,0,up[0],up[1],up[2]);
    ma_engine_listener_set_velocity(&d.engine,0,elapsed>0&&d.listenerPositionReady?float((listenerMatrix[12]-d.listenerPosition[0])/elapsed):0,elapsed>0&&d.listenerPositionReady?float((listenerMatrix[13]-d.listenerPosition[1])/elapsed):0,elapsed>0&&d.listenerPositionReady?float((listenerMatrix[14]-d.listenerPosition[2])/elapsed):0);
    std::copy_n(listenerMatrix+12,3,d.listenerPosition);d.listenerPositionReady=true;
  }else {listener=nullptr;listening=nullptr;d.listenerPositionReady=false;}
  for(auto &[key,v]:d.voices){(void)key;v->seen=false;}
  for(auto id:d.ids){
    const auto *o=world.graph().find(id);if(!o)continue;
    const auto *component=o->components.find(scene::AudioSource::descriptor);if(!component)continue;
    const auto &source=static_cast<const scene::AudioSource&>(*component);const auto key=std::make_pair(id,source.instanceId());
    Diagnostic diagnostic{id,source.instanceId(),State::Stopped,"Parado",0};
    const auto issue=[&](State state,std::string message){diagnostic.state=state;diagnostic.message=std::move(message);d.voices.erase(key);d.diagnostics.push_back(diagnostic);};
    if(!source.enabled||!world.activeInHierarchy(world.handle(id))){issue(State::Stopped,"Inativo · cursor reiniciado");continue;}
    if(!source.clip.valid()){issue(State::MissingClip,"Atribua um clipe WAV");continue;}
    if(source.dimension==scene::AudioDimension::Spatial&&!listener){issue(State::MissingListener,"Áudio 3D requer listener ativo");continue;}
    float gain=listening?listening->volume:1;bool soloPath=false,busValid=true;
    std::set<ObjectId> path;ObjectId bus=source.bus;
    for(u32 depth=0;bus;++depth){
      if(depth>=32||!path.insert(bus).second){busValid=false;break;}
      const auto *node=world.graph().find(bus);const auto *c=node?node->components.find(scene::AudioBus::descriptor):nullptr;
      if(!c){busValid=false;break;}
      const auto &b=static_cast<const scene::AudioBus&>(*c);soloPath|=b.enabled&&b.solo;
      if(!b.enabled||b.mute||!world.activeInHierarchy(world.handle(bus)))gain=0;else gain*=b.volume;
      bus=b.output;
    }
    if(!busValid){issue(State::InvalidBus,"Bus ausente, ciclo ou mais de 32 saídas");continue;}
    if(solo&&!soloPath)gain=0;
    if(source.mute)gain=0;
    auto found=d.voices.find(key);
    if(found!=d.voices.end()&&found->second->asset!=source.clip){d.voices.erase(found);found=d.voices.end();}
    if(found==d.voices.end()){
      if(d.voices.size()>=64){issue(State::Limit,"Limite de 64 vozes atingido");continue;}
      std::string failure;auto clip=d.loader?d.loader(source.clip,failure):nullptr;
      if(!clip||!clip->valid()){issue(State::MissingClip,failure.empty()?"Clipe não carregado neste projeto":failure);continue;}
      auto voice=std::make_unique<Data::Voice>();voice->object=id;voice->instance=source.instanceId();voice->asset=source.clip;voice->clip=std::move(clip);
      auto config=ma_audio_buffer_config_init(ma_format_f32,resources::AudioClip::Channels,voice->clip->frames(),voice->clip->samples.data(),nullptr);config.sampleRate=resources::AudioClip::SampleRate;
      if(ma_audio_buffer_init(&config,&voice->buffer)!=MA_SUCCESS){issue(State::MissingClip,"Falha ao montar PCM");continue;}voice->bufferReady=true;
      if(ma_sound_init_from_data_source(&d.engine,&voice->buffer,0,nullptr,&voice->sound)!=MA_SUCCESS){issue(State::DeviceError,"Falha ao criar voz de áudio");continue;}voice->soundReady=true;
      found=d.voices.emplace(key,std::move(voice)).first;
    }
    auto &voice=*found->second;voice.seen=true;auto &sound=voice.sound;
    ma_sound_set_volume(&sound,gain*source.volume);ma_sound_set_pitch(&sound,source.pitch);ma_sound_set_pan(&sound,source.dimension==scene::AudioDimension::Flat?source.pan:0);
    ma_sound_set_looping(&sound,source.loop);ma_sound_set_spatialization_enabled(&sound,source.dimension==scene::AudioDimension::Spatial);
    ma_sound_set_pinned_listener_index(&sound,0);
    ma_sound_set_attenuation_model(&sound,source.rolloff==scene::AudioRolloff::Linear?ma_attenuation_model_linear:source.rolloff==scene::AudioRolloff::Inverse?ma_attenuation_model_inverse:ma_attenuation_model_exponential);
    ma_sound_set_min_distance(&sound,source.minDistance);ma_sound_set_max_distance(&sound,source.maxDistance);ma_sound_set_rolloff(&sound,source.rolloffFactor);
    ma_sound_set_cone(&sound,source.coneInner*.017453292519943295f,source.coneOuter*.017453292519943295f,source.coneGain);ma_sound_set_doppler_factor(&sound,source.doppler);
    float matrix[16]{};
    if(!worldMatrix(world.graph(),id,matrix)||(source.dimension==scene::AudioDimension::Spatial&&!validDirection(matrix,8))){issue(State::InvalidPose,"Pose da fonte degenerada; escala deve preservar orientação");continue;}
    ma_sound_set_position(&sound,matrix[12],matrix[13],matrix[14]);
    const float length=std::sqrt(matrix[8]*matrix[8]+matrix[9]*matrix[9]+matrix[10]*matrix[10]);
    if(length>1e-6f) ma_sound_set_direction(&sound,matrix[8]/length,matrix[9]/length,matrix[10]/length);
    else ma_sound_set_direction(&sound,0,0,1); // flat audio has no orientation requirement
    ma_sound_set_velocity(&sound,elapsed>0&&voice.positionReady?float((matrix[12]-voice.position[0])/elapsed):0,elapsed>0&&voice.positionReady?float((matrix[13]-voice.position[1])/elapsed):0,elapsed>0&&voice.positionReady?float((matrix[14]-voice.position[2])/elapsed):0);
    std::copy_n(matrix+12,3,voice.position);voice.positionReady=true;
    if(source.playback!=voice.command || !voice.everStarted){
      if(source.playback==scene::AudioPlayback::Stopped){ma_sound_stop(&sound);ma_sound_seek_to_pcm_frame(&sound,0);}
      else if(source.playback==scene::AudioPlayback::Paused)ma_sound_stop(&sound);
      else if(ma_sound_start(&sound)!=MA_SUCCESS){issue(State::DeviceError,"Dispositivo recusou reprodução");continue;}
      voice.command=source.playback;voice.everStarted=true;
    }
    float cursor=0;ma_sound_get_cursor_in_seconds(&sound,&cursor);diagnostic.cursor=cursor;
    diagnostic.state=source.playback==scene::AudioPlayback::Paused?State::Paused:ma_sound_is_playing(&sound)&&!ma_sound_at_end(&sound)?State::Playing:State::Stopped;
    diagnostic.message=d.paused?"Saída suspensa · cursor preservado":diagnostic.state==State::Paused?"Pausado":diagnostic.state==State::Playing?(d.output==Output::Offline?"Reproduzindo · saída offline explícita":"Reproduzindo · dispositivo"):"Parado / fim do clipe";
    d.diagnostics.push_back(std::move(diagnostic));
  }
  for(auto i=d.voices.begin();i!=d.voices.end();)if(!i->second->seen)i=d.voices.erase(i);else ++i;
  return true;
}
}
