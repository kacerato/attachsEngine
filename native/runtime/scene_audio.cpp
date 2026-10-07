#include "runtime/scene_audio.h"
#include "runtime/audio_dsp.h"
#include "scene/audio.h"
#include "scene/audio_mixer.h"
#include "miniaudio.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <map>
#include <set>

namespace ae::runtime {
namespace {
using namespace audio_dsp;
constexpr double Rate=resources::AudioClip::SampleRate;
constexpr u32 MaximumSends=16,MaximumBusDepth=32;

// ---------------------------------------------------------------------------
// Nó DSP de um bus: entrada = soma das vozes e saídas que chegam; saída 0 vai
// ao bus de saída (ou ao endpoint); saídas 1..N são os envios, na ordem.
enum class EffectKind : u32 {Filter,Echo,Reverb,Compressor,Send};
struct EffectParams {bool enabled=true;u32 mode=0;float v[6]{};};
struct EffectState {
  EffectKind kind=EffectKind::Filter;u64 instance=0;u32 output=0;
  Biquad biquad;std::unique_ptr<Echo> echo;std::unique_ptr<Reverb> reverb;Compressor compressor;
  const std::atomic<float> *sidechain=nullptr;
  std::atomic<float> reduction{0};
};
struct BusNode;
struct NodeShell {ma_node_base base;BusNode *owner;};  // layout padrão: o miniaudio só vê `base`
struct BusNode {
  NodeShell shell{};ObjectId id=0;bool ready=false;
  std::vector<std::unique_ptr<EffectState>> chain;  // fixa depois de montada
  ma_spinlock lock=0;bool dirty=false;
  float pendingGain=1,activeGain=1;std::vector<EffectParams> pending,active;
  std::atomic<float> blockPeak{0},blockRms{0},level{0};
  float displayPeak=0,displayRms=0;
  ~BusNode() {if(ready) ma_node_uninit(&shell.base,nullptr);}
};
void processBus(ma_node *node,const float **in,ma_uint32 *,float **out,ma_uint32 *outCount) {
  auto &bus=*reinterpret_cast<NodeShell *>(node)->owner;
  const ma_uint32 frames=*outCount;
  ma_spinlock_lock(&bus.lock);
  if(bus.dirty) {bus.activeGain=bus.pendingGain;std::copy(bus.pending.begin(),bus.pending.end(),bus.active.begin());bus.dirty=false;}
  ma_spinlock_unlock(&bus.lock);
  float *main=out[0];
  if(in&&in[0]) for(ma_uint32 i=0;i<frames*2;++i) main[i]=in[0][i]*bus.activeGain;
  else std::memset(main,0,sizeof(float)*frames*2);
  const ma_uint32 outputs=ma_node_get_output_bus_count(node);
  for(ma_uint32 o=1;o<outputs;++o) std::memset(out[o],0,sizeof(float)*frames*2);
  for(usize i=0;i<bus.chain.size();++i) {
    auto &e=*bus.chain[i];const auto &p=bus.active[i];
    if(!p.enabled) {e.reduction.store(0,std::memory_order_relaxed);continue;}
    switch(e.kind) {
      case EffectKind::Filter: e.biquad.configure(static_cast<FilterMode>(p.mode),p.v[0],p.v[1],p.v[2]);e.biquad.process(main,frames);break;
      case EffectKind::Echo: e.echo->process(main,frames,p.v[0],p.v[1],p.v[2],p.v[3]);break;
      case EffectKind::Reverb: e.reverb->process(main,frames,p.v[0],p.v[1],p.v[2],p.v[3],p.v[4],p.v[5]);break;
      case EffectKind::Compressor: {
        float side=e.sidechain?e.sidechain->load(std::memory_order_relaxed):0;
        e.compressor.process(main,frames,p.v[0],p.v[1],p.v[2],p.v[3],p.v[4],p.v[5],e.sidechain?&side:nullptr);
        e.reduction.store(e.compressor.reductionDb,std::memory_order_relaxed);break;
      }
      case EffectKind::Send:
        if(e.output<outputs) for(ma_uint32 k=0;k<frames*2;++k) out[e.output][k]+=main[k]*p.v[0];
        break;
    }
  }
  float peak=0;double sum=0;
  for(ma_uint32 k=0;k<frames*2;++k) {peak=std::max(peak,std::abs(main[k]));sum+=double(main[k])*main[k];}
  const float rms=frames?float(std::sqrt(sum/(frames*2))):0;
  float held=bus.blockPeak.load(std::memory_order_relaxed);
  while(peak>held&&!bus.blockPeak.compare_exchange_weak(held,peak,std::memory_order_relaxed)) {}
  bus.blockRms.store(rms,std::memory_order_relaxed);bus.level.store(peak,std::memory_order_relaxed);
}
ma_node_vtable busVTable{processBus,nullptr,1,MA_NODE_BUS_COUNT_UNKNOWN,MA_NODE_FLAG_CONTINUOUS_PROCESSING|MA_NODE_FLAG_ALLOW_NULL_INPUT};

EffectKind effectKind(const scene::ComponentValue &c,bool &known) {
  known=true;const auto *t=&c.type();
  if(t==&scene::AudioFilter::descriptor) return EffectKind::Filter;
  if(t==&scene::AudioEcho::descriptor) return EffectKind::Echo;
  if(t==&scene::AudioReverb::descriptor) return EffectKind::Reverb;
  if(t==&scene::AudioCompressor::descriptor) return EffectKind::Compressor;
  if(t==&scene::AudioSend::descriptor) return EffectKind::Send;
  known=false;return EffectKind::Filter;
}
// Parâmetros ao vivo de um efeito, lidos do componente do mundo de Play.
EffectParams effectParams(const scene::ComponentValue &c) {
  EffectParams p;const auto *t=&c.type();
  if(t==&scene::AudioFilter::descriptor) {const auto &f=static_cast<const scene::AudioFilter&>(c);p.enabled=f.enabled;p.mode=static_cast<u32>(f.mode);p.v[0]=f.cutoff;p.v[1]=f.resonance;p.v[2]=f.gain;}
  else if(t==&scene::AudioEcho::descriptor) {const auto &e=static_cast<const scene::AudioEcho&>(c);p.enabled=e.enabled;p.v[0]=e.delay;p.v[1]=e.feedback;p.v[2]=e.wet;p.v[3]=e.dry;}
  else if(t==&scene::AudioReverb::descriptor) {const auto &r=static_cast<const scene::AudioReverb&>(c);p.enabled=r.enabled;p.v[0]=r.roomSize;p.v[1]=r.damping;p.v[2]=r.width;p.v[3]=r.predelay;p.v[4]=r.wet;p.v[5]=r.dry;}
  else if(t==&scene::AudioCompressor::descriptor) {const auto &k=static_cast<const scene::AudioCompressor&>(c);p.enabled=k.enabled;p.v[0]=k.threshold;p.v[1]=k.ratio;p.v[2]=k.attack;p.v[3]=k.release;p.v[4]=k.makeup;p.v[5]=k.mix;}
  else if(t==&scene::AudioSend::descriptor) {const auto &s=static_cast<const scene::AudioSend&>(c);p.enabled=s.enabled;p.v[0]=s.level;}
  return p;
}
} // namespace

// ---------------------------------------------------------------------------
struct SceneAudio::Data {
  struct Voice {
    ObjectId object=0;u64 instance=0;resources::AssetGuid asset{};bool streaming=false;
    std::shared_ptr<const resources::AudioClip> clip;
    ma_audio_buffer buffer{};ma_decoder decoder{};ma_sound sound{};
    bool bufferReady=false,decoderReady=false,soundReady=false,seen=false;
    u64 frames=0;
    scene::AudioPlayback command=scene::AudioPlayback::Stopped;
    bool virtualVoice=false,ended=false,positionReady=false;double virtualFrame=0;
    float position[3]{};
    const void *attached=nullptr;bool attachedValid=false;
    float loopStart=-1,loopEnd=-1;
    float priority=128,audibility=0,pitch=1;
    ma_data_source *source() {return streaming?static_cast<ma_data_source *>(&decoder):static_cast<ma_data_source *>(&buffer);}
    ~Voice() {if(soundReady) ma_sound_uninit(&sound);if(bufferReady) ma_audio_buffer_uninit(&buffer);if(decoderReady) ma_decoder_uninit(&decoder);}
  };
  struct Transition {ComponentHandle handle;std::string property;float from=0,to=0;double elapsed=0,duration=0;};
  ma_engine engine{};bool ready=false,paused=false,deviceAttempted=false;u32 world=0;
  ClipLoader loader;StreamResolver streams;Output output=Output::Device;std::string error;
  u32 voiceLimit=DefaultVoiceLimit;
  std::map<std::pair<ObjectId,u64>,std::unique_ptr<Voice>> voices;
  std::map<ObjectId,std::unique_ptr<BusNode>> buses;std::string graphSignature;bool graphReady=false;
  std::vector<Diagnostic> diagnostics;std::vector<BusMeter> meters;
  std::vector<Transition> transitions;bool startApplied=false;
  std::vector<ObjectId> ids;
  u64 revision=0;ObjectId listenerObject=0;u64 listenerInstance=0;
  float listenerPosition[3]{},listenerForward[3]{0,0,1},listenerUp[3]{0,1,0},listenerVelocity[3]{};bool listenerPositionReady=false;
  void clearAudio() {voices.clear();buses.clear();graphSignature.clear();graphReady=false;}
};

SceneAudio::SceneAudio():data_(std::make_unique<Data>()){}
SceneAudio::~SceneAudio(){stop();}
void SceneAudio::configure(ClipLoader loader,Output output){stop();data_->loader=std::move(loader);data_->output=output;}
void SceneAudio::configureStreams(StreamResolver resolver){data_->streams=std::move(resolver);}
bool SceneAudio::setVoiceLimit(u32 voices){if(voices<1||voices>MaximumVoices)return false;data_->voiceLimit=voices;return true;}
u32 SceneAudio::voiceLimit()const{return data_->voiceLimit;}
void SceneAudio::stop(){
  auto &d=*data_;d.clearAudio();if(d.ready)ma_engine_uninit(&d.engine);d.ready=false;d.deviceAttempted=false;d.world=0;d.revision=0;d.ids.clear();
  d.diagnostics.clear();d.meters.clear();d.transitions.clear();d.startApplied=false;d.error.clear();d.listenerPositionReady=false;d.paused=false;
}
bool SceneAudio::start(GameWorld &world){const bool paused=data_->paused;stop();data_->paused=paused;data_->world=world.worldId();return advance(world,0);}
const std::vector<SceneAudio::Diagnostic> &SceneAudio::diagnostics()const{return data_->diagnostics;}
const SceneAudio::Diagnostic *SceneAudio::diagnostic(ObjectId o,u64 i)const{for(const auto &v:data_->diagnostics)if(v.object==o&&v.instance==i)return &v;return nullptr;}
const SceneAudio::BusMeter *SceneAudio::busMeter(ObjectId bus)const{for(const auto &m:data_->meters)if(m.bus==bus)return &m;return nullptr;}
float SceneAudio::compressorReduction(ObjectId bus,u64 instance)const{
  const auto found=data_->buses.find(bus);if(found==data_->buses.end())return 0;
  for(const auto &e:found->second->chain)if(e->instance==instance)return e->reduction.load(std::memory_order_relaxed);
  return 0;
}
bool SceneAudio::isVirtual(ObjectId o,u64 i)const{const auto f=data_->voices.find({o,i});return f!=data_->voices.end()&&f->second->virtualVoice&&f->second->command==scene::AudioPlayback::Playing&&!f->second->ended;}
u32 SceneAudio::realVoices()const{u32 n=0;for(const auto &[k,v]:data_->voices){(void)k;n+=v->soundReady&&!v->virtualVoice&&ma_sound_is_playing(&v->sound);}return n;}
u32 SceneAudio::virtualVoices()const{u32 n=0;for(const auto &[k,v]:data_->voices){(void)k;n+=v->virtualVoice&&v->command==scene::AudioPlayback::Playing&&!v->ended;}return n;}
u32 SceneAudio::activeTransitions()const{return static_cast<u32>(data_->transitions.size());}
const std::string &SceneAudio::error()const{return data_->error;}
bool SceneAudio::deviceRunning()const{return data_->ready&&data_->output==Output::Device&&!data_->paused;}
bool SceneAudio::wantsDevice()const{
  const auto &d=*data_;
  if(!d.ready||d.output!=Output::Device) return false;
  for(const auto &[key,voice]:d.voices) {
    (void)key;
    if(voice->command==scene::AudioPlayback::Playing&&!voice->virtualVoice&&ma_sound_is_playing(&voice->sound)&&!ma_sound_at_end(&voice->sound)) return true;
  }
  return false;
}
void SceneAudio::pause(bool value){auto &d=*data_;if(d.paused==value)return;d.paused=value;if(d.ready&&d.output==Output::Device){const auto r=value?ma_engine_stop(&d.engine):ma_engine_start(&d.engine);if(r!=MA_SUCCESS)d.error="Falha ao suspender/retomar dispositivo de áudio";}}
bool SceneAudio::renderOffline(std::span<float> stereo){
  auto &d=*data_;
  if(d.output!=Output::Offline || !d.ready || stereo.size()%2)return false;
  // Sem fontes nem buses o grafo está vazio: silêncio explícito, sem ler o
  // endpoint nem expor amostras de um bloco anterior.
  if(d.paused||(d.voices.empty()&&d.buses.empty())){std::fill(stereo.begin(),stereo.end(),0);return true;}
  ma_uint64 read=0;
  return ma_engine_read_pcm_frames(&d.engine,stereo.data(),stereo.size()/2,&read)==MA_SUCCESS&&read==stereo.size()/2;
}

WorldStatus SceneAudio::command(GameWorld &world,const ComponentHandle &handle,Command operation,double seconds){
  auto &d=*data_;
  if(!world.running())return WorldStatus::NotRunning;
  const auto status=world.validate(handle.object);if(status!=WorldStatus::Ok)return status;
  if(world.componentTypeId(handle)!="astra.audio.source")return WorldStatus::ComponentMissing;
  if(u32(operation)>u32(Command::Resume)||!std::isfinite(seconds)||seconds<0 ||
     (operation!=Command::Seek&&seconds!=0))return WorldStatus::InvalidArgument;
  // Resolve current authoring changes, clip ownership and disabled/removed sources first.
  if(!advance(world,0))return WorldStatus::Rejected;
  const auto found=d.voices.find({handle.object.id,handle.instance});
  if(found==d.voices.end()){
    if(operation==Command::Stop||operation==Command::Pause)
      return world.setProperty(handle,"playback",u32(operation==Command::Stop?scene::AudioPlayback::Stopped:scene::AudioPlayback::Paused));
    return WorldStatus::ComponentUnavailable;
  }
  auto &voice=*found->second;auto &sound=voice.sound;
  if(operation==Command::Seek&&seconds>double(voice.frames)/Rate)
    return WorldStatus::InvalidArgument;
  // Resume is deliberately distinct from Play: it never restarts an ended/stopped voice.
  if(operation==Command::Resume&&voice.command!=scene::AudioPlayback::Paused)return WorldStatus::Ok;
  const auto playback=operation==Command::Pause?scene::AudioPlayback::Paused:
    operation==Command::Stop?scene::AudioPlayback::Stopped:
    operation==Command::Seek?voice.command:scene::AudioPlayback::Playing;
  const auto written=world.setProperty(handle,"playback",u32(playback));
  if(written!=WorldStatus::Ok)return written;
  ma_result result=MA_SUCCESS;
  if(operation==Command::Play||operation==Command::Stop){voice.ended=false;voice.virtualFrame=0;}
  if(operation==Command::Seek){voice.virtualFrame=seconds*Rate;voice.ended=false;}
  if(voice.virtualVoice){
    // Voz virtual: só o cursor acompanhado muda; o agendador decide se volta a tocar.
  }else if(operation==Command::Play||operation==Command::Stop){
    result=ma_sound_stop(&sound);
    if(result==MA_SUCCESS)result=ma_sound_seek_to_pcm_frame(&sound,0);
    if(result==MA_SUCCESS&&operation==Command::Play)result=ma_sound_start(&sound);
  }else if(operation==Command::Pause)result=ma_sound_stop(&sound);
  else if(operation==Command::Resume)result=ma_sound_start(&sound);
  else result=ma_sound_seek_to_pcm_frame(&sound,static_cast<ma_uint64>(seconds*Rate));
  voice.command=playback;
  // Query the backend's cursor (which includes its pending seek target). PCM is
  // consumed on the mixer thread; a successful command does not imply audibility.
  for(auto &diagnostic:d.diagnostics)if(diagnostic.object==handle.object.id&&diagnostic.instance==handle.instance){
    float cursor=0;ma_sound_get_cursor_in_seconds(&sound,&cursor);diagnostic.cursor=voice.virtualVoice?voice.virtualFrame/Rate:cursor;
    diagnostic.state=playback==scene::AudioPlayback::Paused?State::Paused:voice.virtualVoice&&playback==scene::AudioPlayback::Playing?State::Virtual:
      ma_sound_is_playing(&sound)&&!ma_sound_at_end(&sound)?State::Playing:State::Stopped;
    diagnostic.message="Comando aceito pelo backend; cursor pode incluir busca pendente";
    if(result!=MA_SUCCESS){diagnostic.state=State::DeviceError;diagnostic.message="Backend recusou comando de transporte de áudio";}
  }
  if(result!=MA_SUCCESS){d.error="Backend recusou comando de transporte de áudio";return WorldStatus::Rejected;}
  return WorldStatus::Ok;
}

// ---------------------------------------------------------------------------
// Snapshots: cada slot vira uma transição linear da propriedade de destino,
// em tempo real; uma nova transição sobre a mesma propriedade substitui a antiga.
WorldStatus SceneAudio::transitionSnapshot(GameWorld &world,const ComponentHandle &handle,double seconds){
  auto &d=*data_;
  if(!world.running())return WorldStatus::NotRunning;
  const auto status=world.validate(handle.object);if(status!=WorldStatus::Ok)return status;
  const auto *component=world.readComponent(handle);
  if(!component||&component->type()!=&scene::AudioSnapshot::descriptor)return WorldStatus::ComponentMissing;
  const auto &snapshot=static_cast<const scene::AudioSnapshot&>(*component);
  if(!snapshot.enabled)return WorldStatus::ComponentUnavailable;
  if(!std::isfinite(seconds)||seconds>3600)return WorldStatus::InvalidArgument;
  const double duration=seconds<0?snapshot.transition:seconds;
  for(const auto &slot:snapshot.slots){
    if(!slot.target)continue;
    const auto object=world.handle(slot.target);if(world.validate(object)!=WorldStatus::Ok)continue;
    const auto &target=scene::audioSnapshotTargets[static_cast<u32>(slot.parameter)];
    const auto destination=world.findComponent(object,target.component);
    const auto *value=world.readComponent(destination);if(!value)continue;
    const scene::ComponentNumber *number=nullptr;
    for(const auto &n:value->type().numbers)if(n.id==target.property)number=&n;
    if(!number)continue;
    const float from=number->read(*value),to=std::clamp(slot.value,number->minimum,number->maximum);
    std::erase_if(d.transitions,[&](const Data::Transition &t){return t.handle.object.id==destination.object.id&&t.handle.instance==destination.instance&&t.property==target.property;});
    if(duration<=0){world.setProperty(destination,target.property,to);continue;}
    d.transitions.push_back({destination,std::string(target.property),from,to,0,duration});
  }
  return WorldStatus::Ok;
}

bool SceneAudio::advance(GameWorld &world,double elapsed){
  auto &d=*data_;d.diagnostics.clear();
  if(!world.running()||!std::isfinite(elapsed)||elapsed<0||elapsed>.25){d.error="Mundo ou delta de áudio inválido";return false;}
  if(d.world!=world.worldId()){stop();d.world=world.worldId();}
  if(d.revision!=world.structuralRevision()){
    d.ids.clear();std::vector<ObjectId> all;world.graph().collectSubtree(world.graph().root(),all);
    for(auto id:all)if(const auto *o=world.graph().find(id))
      for(usize c=0;c<o->components.size();++c)if(std::string_view(o->components.at(c)->type().id).starts_with("astra.audio.")){d.ids.push_back(id);break;}
    d.revision=world.structuralRevision();
  }
  // Snapshots marcados para o início entram no primeiro quadro, sem transição.
  if(!d.startApplied){
    d.startApplied=true;
    for(auto id:d.ids)if(const auto *o=world.graph().find(id))if(const auto *c=o->components.find(scene::AudioSnapshot::descriptor))
      if(static_cast<const scene::AudioSnapshot&>(*c).applyAtStart&&world.activeInHierarchy(world.handle(id)))
        transitionSnapshot(world,{world.handle(id),c->instanceId()},0);
  }
  for(auto &t:d.transitions){
    t.elapsed+=elapsed;const double u=std::min(1.0,t.elapsed/t.duration);
    if(world.setProperty(t.handle,t.property,float(t.from+(t.to-t.from)*u))!=WorldStatus::Ok)t.elapsed=t.duration=-1;
  }
  std::erase_if(d.transitions,[](const Data::Transition &t){return t.duration<0||t.elapsed>=t.duration;});

  const SceneObject *listener=nullptr;const scene::AudioListener *listening=nullptr;
  bool solo=false,hasSources=false;
  for(auto id:d.ids)if(const auto *o=world.graph().find(id);o&&world.activeInHierarchy(world.handle(id))){
    if(const auto *c=o->components.find(scene::AudioListener::descriptor)){const auto *l=static_cast<const scene::AudioListener*>(c);if(l->enabled&&(!listening||l->priority>listening->priority)){listener=o;listening=l;}}
    if(const auto *c=o->components.find(scene::AudioBus::descriptor)){const auto &b=static_cast<const scene::AudioBus&>(*c);solo|=b.enabled&&b.solo;}
    if(const auto *c=o->components.find(scene::AudioSource::descriptor))hasSources|=static_cast<const scene::AudioSource*>(c)->enabled;
  }
  if(!hasSources){
    d.voices.clear();d.meters.clear();
    for(auto id:d.ids)if(const auto *o=world.graph().find(id))if(const auto *c=o->components.find(scene::AudioSource::descriptor))
      d.diagnostics.push_back({id,c->instanceId(),State::Stopped,"Inativo · cursor reiniciado",0});
    return true;
  }
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

  // ---- Grafo de buses ----------------------------------------------------
  struct BusInfo {ObjectId id=0;const scene::AudioBus *bus=nullptr;bool active=false;std::vector<const scene::ComponentValue*> chain;};
  std::map<ObjectId,BusInfo> busInfo;
  for(auto id:d.ids)if(const auto *o=world.graph().find(id))if(const auto *c=o->components.find(scene::AudioBus::descriptor)){
    BusInfo info{id,static_cast<const scene::AudioBus*>(c),world.activeInHierarchy(world.handle(id)),{}};
    for(usize k=0;k<o->components.size();++k){bool known=false;effectKind(*o->components.at(k),known);if(known)info.chain.push_back(o->components.at(k));}
    busInfo.emplace(id,std::move(info));
  }
  const auto reaches=[&](ObjectId from,ObjectId to,const std::map<ObjectId,std::vector<ObjectId>> &edges){
    std::vector<ObjectId> stack{from};std::set<ObjectId> seen;
    while(!stack.empty()){const auto n=stack.back();stack.pop_back();if(n==to)return true;if(!seen.insert(n).second)continue;
      if(const auto e=edges.find(n);e!=edges.end())for(auto m:e->second)stack.push_back(m);}
    return false;
  };
  // Saídas válidas (sem ciclo, destino existente) e envios aceitos sem formar ciclo.
  std::map<ObjectId,ObjectId> outputOf;std::map<ObjectId,std::vector<ObjectId>> edges;std::map<ObjectId,std::string> busIssue;
  for(auto &[id,info]:busInfo){
    ObjectId out=info.bus->output;
    if(out&&!busInfo.count(out)){busIssue[id]="Saída aponta para objeto sem Bus de áudio";out=0;}
    outputOf[id]=out;if(out)edges[id].push_back(out);
  }
  for(auto &[id,info]:busInfo)if(outputOf[id]&&reaches(outputOf[id],id,edges)){busIssue[id]="Saída forma ciclo";edges[id].clear();outputOf[id]=0;}
  std::map<std::pair<ObjectId,u64>,ObjectId> sendTarget;std::map<ObjectId,u32> sendCount;
  for(auto &[id,info]:busInfo)for(const auto *c:info.chain)if(&c->type()==&scene::AudioSend::descriptor){
    const auto target=static_cast<const scene::AudioSend&>(*c).target;
    if(!target||!busInfo.count(target)||target==id||reaches(target,id,edges)||sendCount[id]>=MaximumSends){
      if(target)busIssue[id]=target==id||(busInfo.count(target)&&reaches(target,id,edges))?"Envio recusado: formaria ciclo":"Envio sem Bus de áudio no destino";
      continue;
    }
    sendTarget[{id,c->instanceId()}]=target;edges[id].push_back(target);++sendCount[id];
  }
  std::string signature;
  for(auto &[id,info]:busInfo){
    signature+=std::to_string(id)+">"+std::to_string(outputOf[id])+"[";
    for(const auto *c:info.chain){
      signature+=std::string(c->type().id)+":"+std::to_string(c->instanceId());
      if(const auto t=sendTarget.find({id,c->instanceId()});t!=sendTarget.end())signature+="@"+std::to_string(t->second);
      if(&c->type()==&scene::AudioCompressor::descriptor)signature+="~"+std::to_string(static_cast<const scene::AudioCompressor&>(*c).sidechain);
      signature+=",";
    }
    signature+="]";
  }
  if(!d.graphReady||signature!=d.graphSignature){
    for(auto &[k,v]:d.voices){(void)k;if(v->soundReady)ma_node_detach_output_bus(&v->sound,0);v->attached=nullptr;v->attachedValid=false;}
    d.buses.clear();
    bool failed=false;
    for(auto &[id,info]:busInfo){
      auto node=std::make_unique<BusNode>();node->id=id;node->shell.owner=node.get();
      for(const auto *c:info.chain){
        bool known=false;auto e=std::make_unique<EffectState>();e->kind=effectKind(*c,known);e->instance=c->instanceId();
        if(e->kind==EffectKind::Send){if(!sendTarget.count({id,c->instanceId()}))continue;e->output=1;for(const auto &x:node->chain)e->output+=x->kind==EffectKind::Send;}
        if(e->kind==EffectKind::Echo)e->echo=std::make_unique<Echo>();
        if(e->kind==EffectKind::Reverb)e->reverb=std::make_unique<Reverb>();
        node->chain.push_back(std::move(e));
      }
      node->pending.resize(node->chain.size());node->active.resize(node->chain.size());
      const u32 outputs=1+sendCount[id];std::vector<ma_uint32> outChannels(outputs,2);const ma_uint32 inChannels[1]{2};
      auto config=ma_node_config_init();config.vtable=&busVTable;config.outputBusCount=outputs;config.pInputChannels=inChannels;config.pOutputChannels=outChannels.data();
      if(ma_node_init(ma_engine_get_node_graph(&d.engine),&config,nullptr,&node->shell.base)!=MA_SUCCESS){failed=true;break;}
      node->ready=true;d.buses.emplace(id,std::move(node));
    }
    for(auto &[id,node]:d.buses){
      for(auto &e:node->chain)if(e->kind==EffectKind::Compressor)
        for(const auto *c:busInfo[id].chain)if(c->instanceId()==e->instance){
          const auto side=static_cast<const scene::AudioCompressor&>(*c).sidechain;
          if(const auto s=d.buses.find(side);side&&s!=d.buses.end())e->sidechain=&s->second->level;
        }
      const auto out=outputOf[id];
      ma_node *destination=out&&d.buses.count(out)?static_cast<ma_node*>(&d.buses[out]->shell.base):ma_engine_get_endpoint(&d.engine);
      failed|=ma_node_attach_output_bus(&node->shell.base,0,destination,0)!=MA_SUCCESS;
      for(auto &e:node->chain)if(e->kind==EffectKind::Send){
        const auto target=sendTarget[{id,e->instance}];
        failed|=ma_node_attach_output_bus(&node->shell.base,e->output,&d.buses[target]->shell.base,0)!=MA_SUCCESS;
      }
    }
    if(failed){d.buses.clear();d.error="Não foi possível montar o grafo do mixer";}
    d.graphSignature=signature;d.graphReady=!failed;
  }
  // Solo: passam os buses solo, os que desembocam neles, os que estão no caminho
  // de saída deles e os que recebem envio de um bus que passa.
  std::set<ObjectId> soloPass;
  if(solo){
    for(auto &[id,info]:busInfo){
      bool through=false;ObjectId b=id;for(u32 k=0;b&&k<MaximumBusDepth;++k){const auto f=busInfo.find(b);if(f==busInfo.end())break;through|=f->second.bus->enabled&&f->second.bus->solo;b=outputOf[b];}
      if(through)soloPass.insert(id);
    }
    for(bool grew=true;grew;){
      grew=false;const auto before=soloPass.size();
      for(auto id:std::vector<ObjectId>(soloPass.begin(),soloPass.end())){
        for(ObjectId b=outputOf[id];b;b=outputOf[b])soloPass.insert(b);
        for(const auto &[key,target]:sendTarget)if(key.first==id)soloPass.insert(target);
      }
      grew=soloPass.size()!=before;
    }
  }
  // Parâmetros ao vivo de cada nó: o thread de áudio copia no próximo bloco.
  d.meters.clear();
  for(auto &[id,node]:d.buses){
    const auto &info=busInfo[id];const auto *o=world.graph().find(id);
    const bool audible=info.active&&info.bus->enabled&&!info.bus->mute&&(!solo||soloPass.count(id));
    std::vector<EffectParams> params(node->chain.size());
    for(usize i=0;i<node->chain.size();++i)if(const auto *c=o?o->components.findInstance(node->chain[i]->instance):nullptr)params[i]=effectParams(*c);
    ma_spinlock_lock(&node->lock);node->pendingGain=audible?info.bus->volume:0;std::copy(params.begin(),params.end(),node->pending.begin());node->dirty=true;ma_spinlock_unlock(&node->lock);
    // Medidor: pico máximo desde a última leitura, com queda de 20 dB/s.
    const float block=node->blockPeak.exchange(0,std::memory_order_relaxed);
    node->displayPeak=std::max(block,node->displayPeak*float(std::pow(10.0,-elapsed)));
    node->displayRms=node->blockRms.load(std::memory_order_relaxed);
    BusMeter meter{id,node->displayPeak,node->displayRms,0,0,audible,""};
    for(const auto &e:node->chain)(e->kind==EffectKind::Send?meter.sends:meter.effects)++;
    if(const auto issue=busIssue.find(id);issue!=busIssue.end())meter.message=issue->second;
    else meter.message=!audible?(info.bus->mute?"Silenciado":solo?"Fora do solo":"Desligado"):"";
    d.meters.push_back(std::move(meter));
  }

  // ---- Ouvinte -----------------------------------------------------------
  float listenerMatrix[16]{};
  const auto validDirection=[](const float *m,u32 column){const float length=std::hypot(m[column],m[column+1],m[column+2]);return std::isfinite(length)&&length>1e-6f;};
  if(listener && worldMatrix(world.graph(),listener->id,listenerMatrix)&&validDirection(listenerMatrix,8)&&validDirection(listenerMatrix,4)){
    if(d.listenerObject!=listener->id||d.listenerInstance!=listening->instanceId()) d.listenerPositionReady=false;
    d.listenerObject=listener->id;d.listenerInstance=listening->instanceId();
    for(u32 k=0;k<3;++k){
      d.listenerVelocity[k]=elapsed>0&&d.listenerPositionReady?float((listenerMatrix[12+k]-d.listenerPosition[k])/elapsed):0;
      d.listenerPosition[k]=listenerMatrix[12+k];d.listenerForward[k]=listenerMatrix[8+k];d.listenerUp[k]=listenerMatrix[4+k];
    }
    d.listenerPositionReady=true;
  }else {listener=nullptr;listening=nullptr;d.listenerPositionReady=false;}
  ma_engine_set_volume(&d.engine,listening?listening->volume:1);

  // ---- Vozes -------------------------------------------------------------
  for(auto &[key,v]:d.voices){(void)key;v->seen=false;}
  for(auto id:d.ids){
    const auto *o=world.graph().find(id);if(!o)continue;
    const auto *component=o->components.find(scene::AudioSource::descriptor);if(!component)continue;
    const auto &source=static_cast<const scene::AudioSource&>(*component);const auto key=std::make_pair(id,source.instanceId());
    Diagnostic diagnostic{id,source.instanceId(),State::Stopped,"Parado",0,source.loading==scene::AudioLoading::Stream};
    const auto issue=[&](State state,std::string message){diagnostic.state=state;diagnostic.message=std::move(message);d.voices.erase(key);d.diagnostics.push_back(diagnostic);};
    if(!source.enabled||!world.activeInHierarchy(world.handle(id))){issue(State::Stopped,"Inativo · cursor reiniciado");continue;}
    if(!source.clip.valid()){issue(State::MissingClip,"Atribua um clipe WAV");continue;}
    if(source.dimension==scene::AudioDimension::Spatial&&!listener){issue(State::MissingListener,"Áudio 3D requer listener ativo");continue;}
    // Ganho estimado do caminho de buses (para prioridade) e validade da rota.
    float routeGain=1;bool busValid=true;
    {std::set<ObjectId> path;ObjectId bus=source.bus;
     for(u32 depth=0;bus;++depth){
       if(depth>=MaximumBusDepth||!path.insert(bus).second){busValid=false;break;}
       const auto f=busInfo.find(bus);if(f==busInfo.end()){busValid=false;break;}
       const auto &b=*f->second.bus;
       if(!b.enabled||b.mute||!f->second.active)routeGain=0;else routeGain*=b.volume;
       if(busIssue.count(bus)&&busIssue[bus].starts_with("Saída")){busValid=false;break;}
       bus=b.output;
     }}
    if(!busValid){issue(State::InvalidBus,"Bus ausente, ciclo ou mais de 32 saídas");continue;}
    const bool streaming=source.loading==scene::AudioLoading::Stream;
    auto found=d.voices.find(key);
    if(found!=d.voices.end()&&(found->second->asset!=source.clip||found->second->streaming!=streaming)){d.voices.erase(found);found=d.voices.end();}
    if(found==d.voices.end()){
      if(d.voices.size()>=MaximumVoices){issue(State::Limit,"Limite de 256 vozes alocadas atingido");continue;}
      auto voice=std::make_unique<Data::Voice>();voice->object=id;voice->instance=source.instanceId();voice->asset=source.clip;voice->streaming=streaming;
      if(streaming){
        std::string path,failure;
        if(!d.streams||!d.streams(source.clip,path,failure)){issue(State::MissingClip,failure.empty()?"Streaming indisponível neste projeto":failure);continue;}
        auto config=ma_decoder_config_init(ma_format_f32,resources::AudioClip::Channels,resources::AudioClip::SampleRate);
        if(ma_decoder_init_file(path.c_str(),&config,&voice->decoder)!=MA_SUCCESS){issue(State::MissingClip,"Não foi possível abrir o WAV para streaming");continue;}
        voice->decoderReady=true;ma_uint64 length=0;ma_decoder_get_length_in_pcm_frames(&voice->decoder,&length);voice->frames=length;
      }else{
        std::string failure;auto clip=d.loader?d.loader(source.clip,failure):nullptr;
        if(!clip||!clip->valid()){issue(State::MissingClip,failure.empty()?"Clipe não carregado neste projeto":failure);continue;}
        voice->clip=std::move(clip);voice->frames=voice->clip->frames();
        auto config=ma_audio_buffer_config_init(ma_format_f32,resources::AudioClip::Channels,voice->clip->frames(),voice->clip->samples.data(),nullptr);config.sampleRate=resources::AudioClip::SampleRate;
        if(ma_audio_buffer_init(&config,&voice->buffer)!=MA_SUCCESS){issue(State::MissingClip,"Falha ao montar PCM");continue;}voice->bufferReady=true;
      }
      if(ma_sound_init_from_data_source(&d.engine,voice->source(),MA_SOUND_FLAG_NO_SPATIALIZATION,nullptr,&voice->sound)!=MA_SUCCESS){issue(State::DeviceError,"Falha ao criar voz de áudio");continue;}voice->soundReady=true;
      found=d.voices.emplace(key,std::move(voice)).first;
    }
    auto &voice=*found->second;voice.seen=true;auto &sound=voice.sound;
    // Rota: a voz entra no nó do seu bus (ou direto no endpoint, "Master").
    {const auto bus=source.bus&&d.buses.count(source.bus)?d.buses[source.bus].get():nullptr;
     const void *wanted=bus?static_cast<const void*>(bus):static_cast<const void*>(&d.engine);
     if(!voice.attachedValid||voice.attached!=wanted){
       ma_node_attach_output_bus(&sound,0,bus?static_cast<ma_node*>(&bus->shell.base):ma_engine_get_endpoint(&d.engine),0);
       voice.attached=wanted;voice.attachedValid=true;
     }}
    float matrix[16]{};
    if(!worldMatrix(world.graph(),id,matrix)||(source.dimension==scene::AudioDimension::Spatial&&!validDirection(matrix,8))){issue(State::InvalidPose,"Pose da fonte degenerada; escala deve preservar orientação");continue;}
    float velocity[3]{};
    for(u32 k=0;k<3;++k){velocity[k]=elapsed>0&&voice.positionReady?float((matrix[12+k]-voice.position[k])/elapsed):0;voice.position[k]=matrix[12+k];}
    voice.positionReady=true;
    float gain=source.volume,pan=source.pan,pitch=1;
    if(source.dimension==scene::AudioDimension::Spatial){
      SpatialInput in;
      std::copy_n(d.listenerPosition,3,in.listener);std::copy_n(d.listenerForward,3,in.listenerForward);std::copy_n(d.listenerUp,3,in.listenerUp);std::copy_n(d.listenerVelocity,3,in.listenerVelocity);
      std::copy_n(matrix+12,3,in.source);std::copy_n(matrix+8,3,in.sourceForward);std::copy_n(velocity,3,in.sourceVelocity);
      in.rolloff=static_cast<Rolloff>(source.rolloff);in.minDistance=source.minDistance;in.maxDistance=source.maxDistance;in.rolloffFactor=source.rolloffFactor;
      in.coneInner=source.coneInner;in.coneOuter=source.coneOuter;in.coneGain=source.coneGain;in.doppler=source.doppler;in.pan=source.pan;in.blend=source.spatialBlend;
      const auto out=spatialize(in);gain*=out.gain;pan=out.pan;pitch=out.pitch;
    }
    if(source.mute||(solo&&!source.bus))gain=0;
    voice.pitch=source.pitch*pitch;voice.priority=source.priority;voice.audibility=gain*routeGain*(listening?listening->volume:1);
    ma_sound_set_volume(&sound,gain);ma_sound_set_pitch(&sound,voice.pitch);
    ma_sound_set_pan_mode(&sound,source.dimension==scene::AudioDimension::Spatial?ma_pan_mode_pan:ma_pan_mode_balance);ma_sound_set_pan(&sound,pan);
    ma_sound_set_looping(&sound,source.loop);
    // Pontos de loop no próprio data source: o laço volta ao início escolhido.
    if(voice.loopStart!=source.loopStart||voice.loopEnd!=source.loopEnd){
      const u64 begin=std::min<u64>(static_cast<u64>(source.loopStart*Rate),voice.frames?voice.frames-1:0);
      const u64 end=source.loopEnd>0?std::clamp<u64>(static_cast<u64>(source.loopEnd*Rate),begin+1,voice.frames):~u64{0};
      ma_data_source_set_loop_point_in_pcm_frames(voice.source(),begin,end);
      voice.loopStart=source.loopStart;voice.loopEnd=source.loopEnd;
    }
    if(source.playback!=voice.command){
      if(source.playback==scene::AudioPlayback::Stopped){ma_sound_stop(&sound);ma_sound_seek_to_pcm_frame(&sound,0);voice.virtualFrame=0;voice.ended=false;}
      else if(source.playback==scene::AudioPlayback::Paused){if(!voice.virtualVoice)ma_sound_stop(&sound);}
      else voice.ended=false;
      voice.command=source.playback;
    }
    if(!voice.virtualVoice&&voice.command==scene::AudioPlayback::Playing&&ma_sound_at_end(&sound)&&!source.loop)voice.ended=true;
    // Voz virtual: o cursor segue o tempo real como se tocasse (Unity: voz virtual).
    if(voice.virtualVoice&&voice.command==scene::AudioPlayback::Playing&&!voice.ended){
      voice.virtualFrame+=elapsed*Rate*voice.pitch;
      if(source.loop){
        const double begin=std::min<double>(source.loopStart*Rate,double(voice.frames));
        const double end=source.loopEnd>0?std::min<double>(source.loopEnd*Rate,double(voice.frames)):double(voice.frames);
        if(end>begin&&voice.virtualFrame>=end)voice.virtualFrame=begin+std::fmod(voice.virtualFrame-begin,end-begin);
      }else if(voice.virtualFrame>=double(voice.frames)){voice.virtualFrame=double(voice.frames);voice.ended=true;}
    }
    d.diagnostics.push_back(std::move(diagnostic));
  }
  for(auto i=d.voices.begin();i!=d.voices.end();)if(!i->second->seen)i=d.voices.erase(i);else ++i;

  // ---- Prioridade: as N vozes mais importantes tocam, o resto fica virtual --
  std::vector<Data::Voice*> candidates;
  for(auto &[k,v]:d.voices){(void)k;if(v->command==scene::AudioPlayback::Playing&&!v->ended)candidates.push_back(v.get());}
  std::stable_sort(candidates.begin(),candidates.end(),[](const Data::Voice *a,const Data::Voice *b){
    if(a->priority!=b->priority)return a->priority<b->priority;
    if(a->audibility!=b->audibility)return a->audibility>b->audibility;
    return std::pair(a->object,a->instance)<std::pair(b->object,b->instance);});
  std::vector<const Data::Voice*> failures;
  for(usize i=0;i<candidates.size();++i){
    auto &v=*candidates[i];
    if(i<d.voiceLimit){
      if(v.virtualVoice){ma_sound_seek_to_pcm_frame(&v.sound,static_cast<ma_uint64>(v.virtualFrame));v.virtualVoice=false;}
      if(!ma_sound_is_playing(&v.sound)&&ma_sound_start(&v.sound)!=MA_SUCCESS)failures.push_back(&v);
    }else if(!v.virtualVoice){
      ma_uint64 cursor=0;ma_sound_get_cursor_in_pcm_frames(&v.sound,&cursor);v.virtualFrame=double(cursor);
      ma_sound_stop(&v.sound);v.virtualVoice=true;
    }
  }
  for(auto &diagnostic:d.diagnostics){
    const auto f=d.voices.find({diagnostic.object,diagnostic.instance});if(f==d.voices.end())continue;
    auto &v=*f->second;
    float cursor=0;ma_sound_get_cursor_in_seconds(&v.sound,&cursor);
    diagnostic.cursor=v.virtualVoice?v.virtualFrame/Rate:cursor;
    const bool failed=std::find(failures.begin(),failures.end(),&v)!=failures.end();
    if(failed){diagnostic.state=State::DeviceError;diagnostic.message="Dispositivo recusou reprodução";continue;}
    diagnostic.state=v.command==scene::AudioPlayback::Paused?State::Paused:
      v.virtualVoice&&v.command==scene::AudioPlayback::Playing&&!v.ended?State::Virtual:
      ma_sound_is_playing(&v.sound)&&!ma_sound_at_end(&v.sound)?State::Playing:State::Stopped;
    diagnostic.message=d.paused?"Saída suspensa · cursor preservado":diagnostic.state==State::Virtual?"Virtual · acima do limite de "+std::to_string(d.voiceLimit)+" vozes; o cursor continua":
      diagnostic.state==State::Paused?"Pausado":diagnostic.state==State::Playing?(d.output==Output::Offline?"Reproduzindo · saída offline explícita":"Reproduzindo · dispositivo"):"Parado / fim do clipe";
  }
  return true;
}
}
