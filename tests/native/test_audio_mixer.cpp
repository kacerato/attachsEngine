#include "harness.h"
#include "runtime/audio_dsp.h"
#include "runtime/component_operations.h"
#include "runtime/scene_audio.h"
#include "runtime/scene_event_connections.h"
#include "scene/audio.h"
#include "scene/audio_mixer.h"
#include "scene/component_schema.h"
#include "scene/event_connection.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <vector>

using namespace ae;using namespace ae::runtime;namespace dsp=ae::runtime::audio_dsp;

namespace {
// Senoide estéreo de frequência e amplitude dadas.
std::vector<float> sine(float hz,u32 frames,float amplitude=.5f) {
  std::vector<float> s(frames*2);
  for(u32 i=0;i<frames;++i) s[i*2]=s[i*2+1]=amplitude*std::sin(2*dsp::Pi*hz*float(i)/dsp::SampleRate);
  return s;
}
float rms(std::span<const float> v,usize skip=0) {double s=0;usize n=0;for(usize i=skip;i<v.size();++i){s+=double(v[i])*v[i];++n;}return n?float(std::sqrt(s/n)):0;}
float peak(std::span<const float> v) {float p=0;for(float x:v)p=std::max(p,std::abs(x));return p;}
std::shared_ptr<resources::AudioClip> clipOf(std::vector<float> samples) {auto c=std::make_shared<resources::AudioClip>();c->samples=std::move(samples);return c;}
// Clipe com um degrau por segundo: o valor revela a posição tocada.
std::shared_ptr<resources::AudioClip> rampClip(u32 seconds) {
  std::vector<float> s(seconds*48000*2);
  for(u32 i=0;i<seconds*48000;++i) s[i*2]=s[i*2+1]=.1f*float(i/48000+1);
  return clipOf(std::move(s));
}
struct Scene {
  SceneGraph g;std::vector<std::pair<resources::AssetGuid,std::shared_ptr<resources::AudioClip>>> clips;
  ObjectId object(const char *name,float x=0,float y=0,float z=0,ObjectId parent=0) {
    const auto id=g.createEntity(parent?parent:g.root(),ObjectKind::Folder,name);auto v=*g.find(id);
    v.transform.position[0]=x;v.transform.position[1]=y;v.transform.position[2]=z;g.applyEntityValues(id,v);return id;
  }
  template<class T> T &add(ObjectId id) {return *static_cast<T*>(g.editComponents(id)->add(T::descriptor));}
  scene::AudioSource &source(ObjectId id,std::shared_ptr<resources::AudioClip> clip,const char *seed) {
    auto &s=add<scene::AudioSource>(id);s.clip=resources::assetGuidFromSeed(seed);s.loop=true;clips.emplace_back(s.clip,std::move(clip));return s;
  }
  void configure(SceneAudio &audio) {
    audio.configure([this](resources::AssetGuid id,std::string&)->std::shared_ptr<const resources::AudioClip>{for(const auto &[g,c]:clips) if(g==id) return c;return nullptr;},SceneAudio::Output::Offline);
  }
};
std::vector<float> render(SceneAudio &audio,GameWorld &world,u32 frames,double elapsed=0) {
  std::vector<float> out(frames*2);
  if(!audio.advance(world,elapsed)||!audio.renderOffline(out)) out.assign(frames*2,-99.f);
  return out;
}
}

AE_TEST(audio_dsp_filters_echo_reverb_and_compressor_change_real_signal) {
  // Passa-baixa a 500 Hz: 5 kHz cai mais de 20 dB; 100 Hz passa quase inteiro.
  for(const auto &[hz,low]:{std::pair{5000.f,true},std::pair{100.f,false}}) {
    auto s=sine(hz,9600);dsp::Biquad f;f.configure(dsp::FilterMode::LowPass,500,.707f,0);f.process(s.data(),9600);
    const float ratio=rms(s,4800)/(.5f/std::sqrt(2.f));
    AE_EXPECT_TRUE(low?ratio<.1f:ratio>.9f,low?"passa-baixa corta agudo":"passa-baixa deixa grave");
  }
  {auto s=sine(100,9600);dsp::Biquad f;f.configure(dsp::FilterMode::HighPass,2000,.707f,0);f.process(s.data(),9600);
   AE_EXPECT_TRUE(rms(s,4800)<.01f,"passa-alta corta grave");}
  {auto s=sine(1000,9600);dsp::Biquad f;f.configure(dsp::FilterMode::Peak,1000,1,12);f.process(s.data(),9600);
   AE_EXPECT_TRUE(std::abs(dsp::gainToDb(rms(s,4800)/(.5f/std::sqrt(2.f)))-12)<.5f,"pico de +12 dB na frequência central");}
  // Eco: um impulso volta 100 ms depois com o nível da mistura.
  {std::vector<float> s(9600*2,0.f);s[0]=s[1]=1;dsp::Echo e;e.process(s.data(),9600,100,0,.5f,1);
   AE_EXPECT_TRUE(s[0]==1&&std::abs(s[4800*2]-.5f)<1e-6f&&std::abs(s[4801*2])<1e-6f,"repetição exata a 4800 quadros");}
  // Reverb: o impulso ganha cauda longa; com mistura zero nada muda.
  {std::vector<float> s(48000*2,0.f);s[0]=s[1]=1;dsp::Reverb r;r.process(s.data(),48000,.9f,.3f,1,0,1,0);
   std::vector<float> tail(s.begin()+24000*2,s.end());
   AE_EXPECT_TRUE(rms(tail)>1e-4f,"cauda audível meio segundo depois");}
  {auto s=sine(440,4800);const auto before=s;dsp::Reverb r;r.process(s.data(),4800,.9f,.3f,1,0,0,1);
   AE_EXPECT_TRUE(s==before,"sem mistura de reverberação o sinal passa intacto");}
  // Compressor 4:1 com limiar −20 dB: senoide de 0 dBFS de pico perde ~15 dB.
  {auto s=sine(1000,48000,1);dsp::Compressor c;c.process(s.data(),48000,-20,4,1,100,0,1,nullptr);
   AE_EXPECT_TRUE(c.reductionDb>13&&c.reductionDb<16&&peak(std::span<const float>(s).subspan(40000*2))<.2f,"redução de ~15 dB acima do limiar");}
  // Sidechain: detector é o nível de outro bus, não o sinal próprio.
  {auto s=sine(1000,4800,.05f);dsp::Compressor c;const float loud=1;c.process(s.data(),4800,-30,10,1,100,0,1,&loud);
   AE_EXPECT_TRUE(c.reductionDb>20,"o próprio sinal baixo é comprimido pelo sidechain alto");}
}

AE_TEST(audio_spatialize_blend_rolloff_cone_pan_and_doppler) {
  dsp::SpatialInput in;in.source[0]=10;
  auto out=dsp::spatialize(in);
  AE_EXPECT_TRUE(std::abs(out.gain-.1f)<1e-4f&&std::abs(out.pan-1)<1e-4f,"inverso a 10 m: 1/10; fonte à direita: pan +1");
  in.blend=.5f;out=dsp::spatialize(in);
  AE_EXPECT_TRUE(std::abs(out.gain-.55f)<1e-4f&&std::abs(out.pan-.5f)<1e-4f,"mistura 0,5: metade 2D, metade 3D");
  in.blend=0;in.pan=-.3f;out=dsp::spatialize(in);
  AE_EXPECT_TRUE(out.gain==1&&std::abs(out.pan+.3f)<1e-6f&&out.pitch==1,"mistura 0 é 2D puro");
  in.blend=1;in.pan=0;in.rolloff=dsp::Rolloff::Linear;in.maxDistance=20;out=dsp::spatialize(in);
  AE_EXPECT_TRUE(std::abs(out.gain-(1-9.f/19))<1e-4f,"linear entre mínima e máxima");
  in.rolloff=dsp::Rolloff::Inverse;in.coneInner=60;in.coneOuter=120;in.coneGain=.25f;  // frente da fonte em +Z, ouvinte a −X
  out=dsp::spatialize(in);AE_EXPECT_TRUE(std::abs(out.gain-.025f)<1e-4f,"ouvinte fora do cone externo recebe o ganho do cone");
  in.coneInner=in.coneOuter=360;in.source[0]=0;in.source[2]=50;in.sourceVelocity[2]=-34.33f;
  out=dsp::spatialize(in);AE_EXPECT_TRUE(std::abs(out.pitch-343.3f/(343.3f-34.33f))<1e-3f,"fonte se aproximando a 10% da velocidade do som sobe o tom");
  in.sourceVelocity[2]=34.33f;out=dsp::spatialize(in);AE_EXPECT_TRUE(out.pitch<1,"afastando desce o tom");
}

AE_TEST(audio_source_v2_migrates_v1_and_validates_loop_points) {
  scene::AudioSource v1;v1.dimension=scene::AudioDimension::Spatial;v1.volume=.7f;v1.doppler=2;
  std::ostringstream old;old<<"- 0 1 1 0 1 1 1";for(usize n=0;n<scene::audioSourceNumbersV1;++n) old<<' '<<scene::AudioSource::descriptor.numbers[n].read(v1);
  scene::AudioSource read;std::istringstream in(old.str());
  AE_EXPECT_TRUE(read.read(in,1)&&read.volume==.7f&&read.doppler==2&&read.spatialBlend==1&&read.priority==128&&read.loading==scene::AudioLoading::Memory,"v1 lida com padrões novos");
  read.loading=scene::AudioLoading::Stream;read.loopStart=2;read.loopEnd=5;read.priority=10;
  std::ostringstream now;read.write(now);scene::AudioSource back;std::istringstream again(now.str());
  AE_EXPECT_TRUE(back.read(again,2)&&back.loading==scene::AudioLoading::Stream&&back.loopStart==2&&back.loopEnd==5&&back.priority==10,"v2 relida");
  back.loopEnd=1;AE_EXPECT_TRUE(!back.valid(),"fim do loop antes do início recusado");
  back.loopEnd=0;back.priority=1.5f;AE_EXPECT_TRUE(!back.valid(),"prioridade inteira");
}

AE_TEST(audio_priority_keeps_the_most_important_voices_and_virtual_cursor_advances) {
  Scene s;
  for(u32 i=0;i<4;++i) {const auto id=s.object(("Voz "+std::to_string(i)).c_str());auto &src=s.source(id,rampClip(4),("ramp"+std::to_string(i)).c_str());src.priority=float(100+i);src.volume=.25f;}
  GameWorld w;AE_EXPECT_TRUE(w.load(s.g),"mundo");SceneAudio audio;s.configure(audio);AE_EXPECT_TRUE(audio.setVoiceLimit(2),"limite de 2 vozes");
  AE_EXPECT_TRUE(audio.start(w),audio.error().c_str());
  auto pcm=render(audio,w,4800);
  AE_EXPECT_TRUE(audio.realVoices()==2&&audio.virtualVoices()==2,"duas reais, duas virtuais");
  AE_EXPECT_TRUE(std::abs(pcm[100]-.05f)<1e-4f,"só as duas de prioridade 100 e 101 soam (2 × 0,25 × 0,1)");
  ObjectId lowest=0;u64 lowestInstance=0;
  for(const auto &d:audio.diagnostics()) if(d.state==SceneAudio::State::Virtual&&(!lowest||d.object>lowest)) {lowest=d.object;lowestInstance=d.instance;}
  AE_EXPECT_TRUE(lowest&&audio.isVirtual(lowest,lowestInstance),"diagnóstico Virtual");
  // 1,5 s de tempo real: a voz virtual avança o cursor sem tocar.
  for(u32 k=0;k<6;++k) render(audio,w,12000,.25);
  AE_EXPECT_TRUE(std::abs(audio.diagnostic(lowest,lowestInstance)->cursor-1.5)<.01,"cursor virtual segue o tempo");
  // Promovida a prioridade 0: entra real no ponto acompanhado (degrau do 2º segundo).
  const auto h=w.findComponent(w.handle(lowest),"astra.audio.source");
  AE_EXPECT_TRUE(w.setProperty(h,"priority",0.f)==WorldStatus::Ok,"prioridade em Play");
  pcm=render(audio,w,480);
  AE_EXPECT_TRUE(!audio.isVirtual(lowest,lowestInstance)&&audio.realVoices()==2,"virou real e outra saiu");
  // As duas reais estão no 2º segundo (degrau .2): 2 × .25 × .2. Do começo seria .075.
  AE_EXPECT_TRUE(std::abs(pcm[200]-.1f)<2e-3f,"a voz promovida toca do cursor acompanhado, não do começo");
  ComponentOperationServices services;services.world=&w;services.audio=&audio;scene::ComponentOperationValue result;
  AE_EXPECT_TRUE(invokeComponentMethod(services,h,"is_virtual",{},result)==WorldStatus::Ok&&result.boolean==0,"is_virtual pela ABI");
}

AE_TEST(audio_loop_points_wrap_inside_the_region_after_the_intro) {
  Scene s;const auto id=s.object("Música");auto &src=s.source(id,rampClip(4),"loop-ramp");src.loopStart=1;src.loopEnd=2;
  GameWorld w;w.load(s.g);SceneAudio audio;s.configure(audio);AE_EXPECT_TRUE(audio.start(w),"start");
  std::vector<float> all;
  for(u32 k=0;k<30;++k) {auto pcm=render(audio,w,4800);all.insert(all.end(),pcm.begin(),pcm.end());}  // 3 s
  AE_EXPECT_TRUE(std::abs(all[100]-.1f)<1e-5f,"introdução toca o 1º segundo");
  AE_EXPECT_TRUE(std::abs(all[(48000+2400)*2]-.2f)<1e-5f,"depois o 2º segundo");
  AE_EXPECT_TRUE(std::abs(all[(96000+2400)*2]-.2f)<1e-5f,"e volta ao início do loop, nunca ao 3º segundo");
}

AE_TEST(audio_stream_loading_reads_a_wav_file_from_disk) {
  namespace fs=std::filesystem;const auto path=fs::temp_directory_path()/"astra-stream-test.wav";
  {const u32 frames=96000;std::ofstream out(path,std::ios::binary);
   const auto put32=[&](u32 v){out.put(char(v));out.put(char(v>>8));out.put(char(v>>16));out.put(char(v>>24));};
   const auto put16=[&](u32 v){out.put(char(v));out.put(char(v>>8));};
   out.write("RIFF",4);put32(36+frames*4);out.write("WAVEfmt ",8);put32(16);put16(1);put16(2);put32(48000);put32(192000);put16(4);put16(16);out.write("data",4);put32(frames*4);
   for(u32 i=0;i<frames*2;++i) put16(8192);}
  u64 frames=0;std::string error;
  AE_EXPECT_TRUE(resources::inspectWaveStream(path.string(),frames,error)&&frames==96000,"inspeção sem decodificar tudo");
  Scene s;const auto id=s.object("Fala");auto &src=s.add<scene::AudioSource>(id);src.clip=resources::assetGuidFromSeed("stream");src.loading=scene::AudioLoading::Stream;
  GameWorld w;w.load(s.g);SceneAudio audio;s.configure(audio);
  audio.configureStreams([&](resources::AssetGuid g,std::string &p,std::string &e){if(g!=src.clip){e="desconhecido";return false;}p=path.string();return true;});
  AE_EXPECT_TRUE(audio.start(w),audio.error().c_str());
  auto pcm=render(audio,w,4800);
  AE_EXPECT_TRUE(std::abs(pcm[400]-.25f)<1e-3f,"amostras lidas do arquivo em blocos");
  AE_EXPECT_TRUE(audio.diagnostic(id,src.instanceId())->streaming,"diagnóstico marca streaming");
  audio.stop();std::error_code ec;fs::remove(path,ec);
}

AE_TEST(audio_bus_graph_runs_effects_in_component_order_and_sends_to_reverb_bus) {
  Scene s;
  const auto music=s.object("Música"),fx=s.object("Reverb"),src=s.object("Fonte");
  auto &m=s.add<scene::AudioBus>(music);m.volume=1;
  auto &filter=s.add<scene::AudioFilter>(music);filter.mode=scene::AudioFilterMode::LowPass;filter.cutoff=400;
  auto &r=s.add<scene::AudioBus>(fx);(void)r;
  auto &reverb=s.add<scene::AudioReverb>(fx);reverb.dry=0;reverb.wet=1;reverb.roomSize=.3f;
  auto &send=s.add<scene::AudioSend>(music);send.target=fx;send.level=1;
  auto &source=s.source(src,clipOf(sine(5000,48000)),"tone");source.bus=music;
  GameWorld w;w.load(s.g);SceneAudio audio;s.configure(audio);AE_EXPECT_TRUE(audio.start(w),audio.error().c_str());
  render(audio,w,4800);auto pcm=render(audio,w,9600);
  const auto *meter=audio.busMeter(music);
  AE_EXPECT_TRUE(meter&&meter->effects==1&&meter->sends==1&&meter->peak>0,"medidor do bus com 1 efeito e 1 envio");
  const float filtered=rms(pcm);
  AE_EXPECT_TRUE(filtered>0&&filtered<.05f,"5 kHz passa pelo passa-baixa de 400 Hz do bus");
  // Tirar o filtro: o tom aparece inteiro; a reverberação do envio também soma.
  const auto filterHandle=w.findComponent(w.handle(music),"astra.audio.filter");
  AE_EXPECT_TRUE(w.setProperty(filterHandle,"enabled",false)==WorldStatus::Ok,"desliga o filtro");
  render(audio,w,4800);pcm=render(audio,w,9600);
  AE_EXPECT_TRUE(rms(pcm)>.3f,"sem filtro o tom e a reverberação chegam à saída");
  AE_EXPECT_TRUE(audio.busMeter(fx)->peak>0,"o bus de reverberação recebe o envio");
  // Envio desligado: o bus de reverberação esvazia (só cauda decrescente).
  const auto sendHandle=w.findComponent(w.handle(music),"astra.audio.send");
  w.setProperty(sendHandle,"level",0.f);
  for(u32 k=0;k<20;++k) render(audio,w,4800,.1);
  AE_EXPECT_TRUE(audio.busMeter(fx)->rms<1e-3f,"sem envio, nada novo entra na reverberação (o pico exibido cai 20 dB/s)");
}

AE_TEST(audio_bus_solo_mute_cycles_and_sidechain_ducking) {
  Scene s;
  const auto music=s.object("Música"),voice=s.object("Voz"),a=s.object("Fonte música"),b=s.object("Fonte voz");
  s.add<scene::AudioBus>(music);s.add<scene::AudioBus>(voice);
  auto &duck=s.add<scene::AudioCompressor>(music);duck.sidechain=voice;duck.threshold=-30;duck.ratio=20;duck.attack=1;duck.release=50;
  s.source(a,clipOf(sine(200,48000,.5f)),"music").bus=music;
  auto &talk=s.source(b,clipOf(sine(1000,48000,.5f)),"talk");talk.bus=voice;talk.mute=true;
  GameWorld w;w.load(s.g);SceneAudio audio;s.configure(audio);AE_EXPECT_TRUE(audio.start(w),"start");
  for(u32 k=0;k<4;++k) render(audio,w,4800,.1);
  const float before=audio.busMeter(music)->peak;
  AE_EXPECT_TRUE(before>.4f&&audio.compressorReduction(music,duck.instanceId())<.5f,"sem fala, música cheia");
  w.setProperty(w.findComponent(w.handle(b),"astra.audio.source"),"mute",false);
  for(u32 k=0;k<6;++k) render(audio,w,4800,.1);
  AE_EXPECT_TRUE(audio.compressorReduction(music,duck.instanceId())>10,"a voz no outro bus comprime a música (ducking)");
  // Solo na voz: a música cala.
  w.setProperty(w.findComponent(w.handle(voice),"astra.audio.bus"),"solo",true);
  for(u32 k=0;k<8;++k) render(audio,w,4800,.25);
  AE_EXPECT_TRUE(audio.busMeter(music)->peak<.01f&&!audio.busMeter(music)->audible,"fora do solo");
  // Envio para si mesmo via ciclo é recusado com motivo.
  Scene c;const auto x=c.object("X"),y=c.object("Y"),src=c.object("Fonte");
  c.add<scene::AudioBus>(x).output=y;c.add<scene::AudioBus>(y);c.add<scene::AudioSend>(y).target=x;c.source(src,clipOf(sine(300,4800)),"c").bus=x;
  GameWorld cw;cw.load(c.g);SceneAudio ca;c.configure(ca);AE_EXPECT_TRUE(ca.start(cw),"start ciclo");
  render(ca,cw,4800);
  AE_EXPECT_TRUE(ca.busMeter(y)->sends==0&&ca.busMeter(y)->message.find("ciclo")!=std::string::npos,"envio em ciclo recusado e explicado");
}

AE_TEST(audio_snapshot_transitions_mixer_properties_in_real_time) {
  Scene s;const auto music=s.object("Música"),snap=s.object("Pausa"),src=s.object("Fonte");
  s.add<scene::AudioBus>(music).volume=1;auto &lp=s.add<scene::AudioFilter>(music);lp.cutoff=20000;
  auto &snapshot=s.add<scene::AudioSnapshot>(snap);snapshot.transition=1;
  snapshot.slots[0]={music,scene::AudioSnapshotParameter::BusGain,.2f};
  snapshot.slots[1]={music,scene::AudioSnapshotParameter::FilterCutoff,500};
  snapshot.slots[2]={music,scene::AudioSnapshotParameter::ReverbWet,1};  // sem reverb no objeto: ignorado
  s.source(src,clipOf(sine(300,48000)),"s").bus=music;
  GameWorld w;w.load(s.g);SceneAudio audio;s.configure(audio);AE_EXPECT_TRUE(audio.start(w),"start");
  ComponentOperationServices services;services.world=&w;services.audio=&audio;scene::ComponentOperationValue none;
  const auto handle=w.findComponent(w.handle(snap),"astra.audio.snapshot");
  scene::ComponentOperationValue seconds=scene::ComponentOperationValue::makeNumber(-1);
  AE_EXPECT_TRUE(invokeComponentMethod(services,handle,"transition_to",std::span(&seconds,1),none)==WorldStatus::Ok,"Transicionar");
  AE_EXPECT_TRUE(audio.activeTransitions()==2,"dois slots com destino válido");
  render(audio,w,480,.25);render(audio,w,480,.25);
  // Lidos de novo a cada vez: a escrita no mundo pode substituir o valor.
  const auto bus=[&]{return static_cast<const scene::AudioBus*>(w.readComponent(w.findComponent(w.handle(music),"astra.audio.bus")))->volume;};
  const auto cutoff=[&]{return static_cast<const scene::AudioFilter*>(w.readComponent(w.findComponent(w.handle(music),"astra.audio.filter")))->cutoff;};
  AE_EXPECT_TRUE(std::abs(bus()-.6f)<1e-4f&&std::abs(cutoff()-10250)<1,"metade do caminho em 0,5 s");
  render(audio,w,480,.25);render(audio,w,480,.25);
  AE_EXPECT_TRUE(std::abs(bus()-.2f)<1e-5f&&cutoff()==500&&audio.activeTransitions()==0,"valores finais");
  AE_EXPECT_TRUE(static_cast<const scene::AudioBus*>(s.g.find(music)->components.find(scene::AudioBus::descriptor))->volume==1,"autoria preservada");
  // Aplicar ao iniciar: entra no primeiro quadro.
  snapshot.applyAtStart=true;
  GameWorld w2;w2.load(s.g);SceneAudio a2;s.configure(a2);AE_EXPECT_TRUE(a2.start(w2),"start 2");
  AE_EXPECT_TRUE(static_cast<const scene::AudioBus*>(w2.readComponent(w2.findComponent(w2.handle(music),"astra.audio.bus")))->volume==.2f,"aplicado sem transição no início");
  ComponentOperationServices none2;none2.world=&w2;
  AE_EXPECT_TRUE(invokeComponentMethod(none2,w2.findComponent(w2.handle(snap),"astra.audio.snapshot"),"apply",{},none)==WorldStatus::NotRunning,"sem serviço de áudio: recusa explícita");
}

AE_TEST(audio_mixer_schema_requirements_and_connection_identity) {
  scene::Components bare;
  AE_EXPECT_TRUE(!scene::planComponentAddition(bare,"astra.audio.reverb").ready||scene::planComponentAddition(bare,"astra.audio.reverb").candidate.find(scene::AudioBus::descriptor),
                 "efeito exige Bus de áudio");
  auto plan=scene::planComponentAddition(bare,"astra.audio.bus");auto bus=std::move(plan.candidate);
  for(const char *t:{"astra.audio.filter","astra.audio.filter","astra.audio.send"}) {
    auto p=scene::planComponentAddition(bus,t);AE_EXPECT_TRUE(p.ready,"efeitos repetíveis no mesmo bus");bus=std::move(p.candidate);
  }
  AE_EXPECT_TRUE(runtime::auditEventConnectionCatalog().empty(),"métodos de snapshot têm identidade de conexão");
  AE_EXPECT_TRUE(scene::eventConnectionTakesNumber(28)&&!scene::eventConnectionTakesNumber(29),"transicionar leva a duração");
}
