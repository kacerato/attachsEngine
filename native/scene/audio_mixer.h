// Mixer de áudio da Astra (bloco H, F061): efeitos, envios e snapshots.
//
// Efeitos e envios são componentes no objeto do Bus de áudio e formam a cadeia
// do bus na ordem dos componentes do objeto, depois do ganho do bus (como o
// Attenuation no topo de um grupo do AudioMixer da Unity). Um Envio copia o sinal
// naquele ponto da cadeia para outro bus. O DSP está em runtime/audio_dsp.h e o
// grafo em runtime/scene_audio.cpp.
//
// Referências estudadas:
// - Godot 4.5 Audio buses: https://docs.godotengine.org/en/4.5/tutorials/audio/audio_buses.html
// - Godot 4.5 AudioEffectFilter / Delay / Reverb / Compressor (classes AudioEffect*)
// - Unity 6000.0 AudioMixer: https://docs.unity3d.com/6000.0/Documentation/Manual/AudioMixer.html
// - Unity 6000.0 Snapshots: https://docs.unity3d.com/6000.0/Documentation/Manual/AudioMixerSnapshots.html
// Adaptações: o snapshot guarda até 8 parâmetros escolhidos (objeto, parâmetro,
// valor) em vez do estado inteiro do mixer; a transição é linear em tempo real.
#pragma once
#include "scene/audio.h"
#include <array>
#include <cmath>
#include <limits>

namespace ae::scene {

enum class AudioFilterMode : u32 {LowPass=0,HighPass=1,BandPass=2,Notch=3,Peak=4,LowShelf=5,HighShelf=6};

class AudioFilter final : public ComponentValue {
public:
  bool enabled=true;AudioFilterMode mode=AudioFilterMode::LowPass;
  float cutoff=5000,resonance=.707f,gain=0;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<AudioFilter>(*this);}
  bool valid() const override {
    return static_cast<u32>(mode)<=6 && std::isfinite(cutoff) && cutoff>=20 && cutoff<=20000 &&
           std::isfinite(resonance) && resonance>=.1f && resonance<=10 && std::isfinite(gain) && gain>=-24 && gain<=24;
  }
  void write(std::ostream &o) const override {o<<enabled<<' '<<static_cast<u32>(mode)<<' '<<cutoff<<' '<<resonance<<' '<<gain;}
  bool read(std::istream &i,u32 v) override {u32 m=0;if(v!=1||!(i>>enabled>>m>>cutoff>>resonance>>gain)) return false;mode=static_cast<AudioFilterMode>(m);return valid();}
};

class AudioEcho final : public ComponentValue {
public:
  bool enabled=true;float delay=300,feedback=.4f,wet=.5f,dry=1;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<AudioEcho>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {const float x=p.read(*this);if(!std::isfinite(x)||x<p.minimum||x>p.maximum) return false;}
    return true;
  }
  void write(std::ostream &o) const override {o<<enabled<<' '<<delay<<' '<<feedback<<' '<<wet<<' '<<dry;}
  bool read(std::istream &i,u32 v) override {return v==1&&bool(i>>enabled>>delay>>feedback>>wet>>dry)&&valid();}
};

class AudioReverb final : public ComponentValue {
public:
  bool enabled=true;float roomSize=.8f,damping=.5f,width=1,predelay=20,wet=.3f,dry=1;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<AudioReverb>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {const float x=p.read(*this);if(!std::isfinite(x)||x<p.minimum||x>p.maximum) return false;}
    return true;
  }
  void write(std::ostream &o) const override {o<<enabled<<' '<<roomSize<<' '<<damping<<' '<<width<<' '<<predelay<<' '<<wet<<' '<<dry;}
  bool read(std::istream &i,u32 v) override {return v==1&&bool(i>>enabled>>roomSize>>damping>>width>>predelay>>wet>>dry)&&valid();}
};

class AudioCompressor final : public ComponentValue {
public:
  bool enabled=true;float threshold=-20,ratio=4,attack=20,release=250,makeup=0,mix=1;
  u64 sidechain=0;  // zero: detecta o próprio sinal; senão, o nível medido de outro bus
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<AudioCompressor>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {const float x=p.read(*this);if(!std::isfinite(x)||x<p.minimum||x>p.maximum) return false;}
    return sidechain<=std::numeric_limits<u32>::max();
  }
  void write(std::ostream &o) const override {o<<enabled<<' '<<threshold<<' '<<ratio<<' '<<attack<<' '<<release<<' '<<makeup<<' '<<mix<<' '<<sidechain;}
  bool read(std::istream &i,u32 v) override {return v==1&&bool(i>>enabled>>threshold>>ratio>>attack>>release>>makeup>>mix>>sidechain)&&valid();}
};

class AudioSend final : public ComponentValue {
public:
  bool enabled=true;u64 target=0;float level=.5f;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<AudioSend>(*this);}
  bool valid() const override {return target<=std::numeric_limits<u32>::max()&&std::isfinite(level)&&level>=0&&level<=1;}
  void write(std::ostream &o) const override {o<<enabled<<' '<<target<<' '<<level;}
  bool read(std::istream &i,u32 v) override {return v==1&&bool(i>>enabled>>target>>level)&&valid();}
};

// Parâmetros que um snapshot pode levar; cada um aponta para (tipo, propriedade)
// do primeiro componente daquele tipo no objeto escolhido.
enum class AudioSnapshotParameter : u32 {BusGain=0,FilterCutoff=1,FilterResonance=2,FilterGain=3,EchoWet=4,ReverbWet=5,ReverbRoom=6,SendLevel=7,CompressorThreshold=8};
struct AudioSnapshotTarget {std::string_view component,property,unit;};
inline constexpr std::array<AudioSnapshotTarget,9> audioSnapshotTargets{{
  {"astra.audio.bus","volume",""},{"astra.audio.filter","cutoff","Hz"},{"astra.audio.filter","resonance",""},{"astra.audio.filter","gain","dB"},
  {"astra.audio.echo","wet",""},{"astra.audio.reverb","wet",""},{"astra.audio.reverb","room_size",""},{"astra.audio.send","level",""},
  {"astra.audio.compressor","threshold","dB"}}};
inline constexpr u32 kAudioSnapshotSlots=8;

class AudioSnapshot final : public ComponentValue {
public:
  struct Slot {u64 target=0;AudioSnapshotParameter parameter=AudioSnapshotParameter::BusGain;float value=1;};
  bool enabled=true,applyAtStart=false;float transition=1;
  std::array<Slot,kAudioSnapshotSlots> slots{};
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<AudioSnapshot>(*this);}
  bool valid() const override {
    if(!std::isfinite(transition)||transition<0||transition>60) return false;
    for(const auto &s:slots)
      if(s.target>std::numeric_limits<u32>::max()||static_cast<u32>(s.parameter)>8||!std::isfinite(s.value)||s.value<-100000||s.value>100000) return false;
    return true;
  }
  void write(std::ostream &o) const override {
    o<<enabled<<' '<<applyAtStart<<' '<<transition;
    for(const auto &s:slots) o<<' '<<s.target<<' '<<static_cast<u32>(s.parameter)<<' '<<s.value;
  }
  bool read(std::istream &i,u32 v) override {
    if(v!=1||!(i>>enabled>>applyAtStart>>transition)) return false;
    for(auto &s:slots) {u32 p=0;if(!(i>>s.target>>p>>s.value)) return false;s.parameter=static_cast<AudioSnapshotParameter>(p);}
    return valid();
  }
  u32 slotCount() const noexcept {u32 n=0;for(const auto &s:slots) n+=s.target!=0;return n;}
};

namespace audio_mixer_detail {
inline AudioFilter &filter(ComponentValue &v) {return static_cast<AudioFilter &>(v);}
inline const AudioFilter &filter(const ComponentValue &v) {return static_cast<const AudioFilter &>(v);}
inline bool filterGain(const ComponentValue &v) {const auto m=filter(v).mode;return m==AudioFilterMode::Peak||m==AudioFilterMode::LowShelf||m==AudioFilterMode::HighShelf;}
inline AudioSnapshot &snapshot(ComponentValue &v) {return static_cast<AudioSnapshot &>(v);}
inline const AudioSnapshot &snapshot(const ComponentValue &v) {return static_cast<const AudioSnapshot &>(v);}
template<u32 I> bool slotVisible(const ComponentValue &v) {return I==0||snapshot(v).slots[I-1].target!=0;}
template<u32 I> bool slotUsed(const ComponentValue &v) {return snapshot(v).slots[I].target!=0;}
} // namespace audio_mixer_detail

#define AE_AUDIO_NUMBER(TYPE,LABEL,MIN,MAX,STEP,MEMBER,ID,GROUP,UNIT,HELP,VISIBLE,TWEEN) \
  ComponentNumber{LABEL,MIN,MAX,STEP,[](const ComponentValue &v)->const float &{return static_cast<const TYPE &>(v).MEMBER;}, \
   [](ComponentValue &v)->float *{return &static_cast<TYPE &>(v).MEMBER;},ID,{GROUP,UNIT,HELP,VISIBLE},TWEEN}
#define AE_AUDIO_ENABLED(TYPE,GROUP,HELP) \
  ComponentBoolean{"enabled","Ativo",[](const ComponentValue &v){return static_cast<const TYPE &>(v).enabled;}, \
   [](ComponentValue &v,bool b){static_cast<TYPE &>(v).enabled=b;},{GROUP,"",HELP}}

inline constexpr std::array<ComponentEnumOption,7> audioFilterModeOptions{{
  {0,"Passa-baixa"},{1,"Passa-alta"},{2,"Passa-banda"},{3,"Rejeita-banda"},{4,"Pico"},{5,"Prateleira grave"},{6,"Prateleira aguda"}}};
inline constexpr std::array<ComponentEnum,1> audioFilterEnums{{
  {"mode","Tipo",audioFilterModeOptions,[](const ComponentValue &v){return static_cast<u32>(audio_mixer_detail::filter(v).mode);},
   [](ComponentValue &v,u32 x){audio_mixer_detail::filter(v).mode=static_cast<AudioFilterMode>(x);},
   {"Filtro","","Passa-baixa abafa (porta fechada, embaixo d'água); passa-alta afina (rádio, telefone)"}}}};
inline constexpr std::array<ComponentNumber,3> audioFilterNumbers{{
  AE_AUDIO_NUMBER(AudioFilter,"Frequência de corte",20,20000,10,cutoff,"cutoff","Filtro","Hz",nullptr,nullptr,true),
  AE_AUDIO_NUMBER(AudioFilter,"Ressonância",.1f,10,.01f,resonance,"resonance","Filtro","","Q do filtro; 0,707 é plano",nullptr,true),
  AE_AUDIO_NUMBER(AudioFilter,"Ganho",-24,24,.5f,gain,"gain","Filtro","dB","Reforço ou corte na faixa",audio_mixer_detail::filterGain,true),
}};
inline constexpr std::array<ComponentBoolean,1> audioFilterBooleans{{AE_AUDIO_ENABLED(AudioFilter,"Filtro","Desligado deixa o sinal passar sem alteração")}};

inline constexpr std::array<ComponentNumber,4> audioEchoNumbers{{
  AE_AUDIO_NUMBER(AudioEcho,"Atraso",1,5000,1,delay,"delay","Eco","ms","Intervalo entre as repetições",nullptr,false),
  AE_AUDIO_NUMBER(AudioEcho,"Realimentação",0,.95f,.01f,feedback,"feedback","Eco","","Quanto de cada repetição volta ao atraso",nullptr,true),
  AE_AUDIO_NUMBER(AudioEcho,"Mistura do eco",0,1,.01f,wet,"wet","Eco","",nullptr,nullptr,true),
  AE_AUDIO_NUMBER(AudioEcho,"Sinal original",0,1,.01f,dry,"dry","Eco","",nullptr,nullptr,true),
}};
inline constexpr std::array<ComponentBoolean,1> audioEchoBooleans{{AE_AUDIO_ENABLED(AudioEcho,"Eco","Desligado esvazia as repetições")}};

inline constexpr std::array<ComponentNumber,6> audioReverbNumbers{{
  AE_AUDIO_NUMBER(AudioReverb,"Tamanho da sala",0,1,.01f,roomSize,"room_size","Sala","","Maior deixa a cauda mais longa",nullptr,true),
  AE_AUDIO_NUMBER(AudioReverb,"Amortecimento",0,1,.01f,damping,"damping","Sala","","Absorção dos agudos pelas paredes",nullptr,true),
  AE_AUDIO_NUMBER(AudioReverb,"Largura",0,1,.01f,width,"width","Sala","","Abertura estéreo da reverberação",nullptr,false),
  AE_AUDIO_NUMBER(AudioReverb,"Pré-atraso",0,500,1,predelay,"predelay","Sala","ms","Tempo até as primeiras reflexões",nullptr,false),
  AE_AUDIO_NUMBER(AudioReverb,"Mistura da reverberação",0,1,.01f,wet,"wet","Mistura","",nullptr,nullptr,true),
  AE_AUDIO_NUMBER(AudioReverb,"Sinal original",0,1,.01f,dry,"dry","Mistura","","Zero em um bus de envio deixa só a reverberação",nullptr,true),
}};
inline constexpr std::array<ComponentBoolean,1> audioReverbBooleans{{AE_AUDIO_ENABLED(AudioReverb,"Sala","Desligado corta a reverberação")}};

inline constexpr std::array<ComponentNumber,6> audioCompressorNumbers{{
  AE_AUDIO_NUMBER(AudioCompressor,"Limiar",-60,0,.5f,threshold,"threshold","Compressão","dB","Acima deste nível o volume é reduzido",nullptr,true),
  AE_AUDIO_NUMBER(AudioCompressor,"Razão",1,48,.1f,ratio,"ratio","Compressão",": 1","4 reduz 4 dB acima do limiar para cada 1 dB que passa",nullptr,false),
  AE_AUDIO_NUMBER(AudioCompressor,"Ataque",.02f,250,.1f,attack,"attack","Compressão","ms",nullptr,nullptr,false),
  AE_AUDIO_NUMBER(AudioCompressor,"Liberação",1,2000,1,release,"release","Compressão","ms",nullptr,nullptr,false),
  AE_AUDIO_NUMBER(AudioCompressor,"Ganho de compensação",0,24,.5f,makeup,"makeup","Saída","dB",nullptr,nullptr,false),
  AE_AUDIO_NUMBER(AudioCompressor,"Mistura",0,1,.01f,mix,"mix","Saída","","Menor que 1 soma o sinal sem compressão (paralela)",nullptr,true),
}};
inline constexpr std::array<ComponentBoolean,1> audioCompressorBooleans{{AE_AUDIO_ENABLED(AudioCompressor,"Compressão","Desligado deixa o sinal passar sem alteração")}};
inline constexpr std::array<ComponentObjectReference,1> audioCompressorReferences{{
  {"sidechain","Sidechain","astra.audio.bus",ObjectReferenceScope::Other,"Próprio sinal",
   [](const ComponentValue &v){return static_cast<const AudioCompressor &>(v).sidechain;},[](ComponentValue &v,u64 x){static_cast<AudioCompressor &>(v).sidechain=x;},
   {"Compressão","","Comprime quando OUTRO bus soa: música abaixa quando há fala (ducking)"}}}};

inline constexpr std::array<ComponentNumber,1> audioSendNumbers{{
  AE_AUDIO_NUMBER(AudioSend,"Nível do envio",0,1,.01f,level,"level","Envio","","Fração do sinal deste ponto da cadeia que vai ao destino",nullptr,true)}};
inline constexpr std::array<ComponentBoolean,1> audioSendBooleans{{AE_AUDIO_ENABLED(AudioSend,"Envio","Desligado não envia nada")}};
inline constexpr std::array<ComponentObjectReference,1> audioSendReferences{{
  {"target","Destino","astra.audio.bus",ObjectReferenceScope::Other,"Nenhum",
   [](const ComponentValue &v){return static_cast<const AudioSend &>(v).target;},[](ComponentValue &v,u64 x){static_cast<AudioSend &>(v).target=x;},
   {"Envio","","Bus que recebe a cópia, por exemplo um bus só com Reverb"}}}};

// Snapshot: slots revelados um a um, como os passos da Sequência de tweens.
inline constexpr std::array<ComponentEnumOption,9> audioSnapshotParameterOptions{{
  {0,"Ganho do bus"},{1,"Corte do filtro"},{2,"Ressonância do filtro"},{3,"Ganho do filtro"},{4,"Mistura do eco"},
  {5,"Mistura da reverberação"},{6,"Tamanho da sala"},{7,"Nível do envio"},{8,"Limiar do compressor"}}};
template<u32 I> constexpr ComponentObjectReference audioSnapshotReference() {
  return {I==0?"slot_0_target":I==1?"slot_1_target":I==2?"slot_2_target":I==3?"slot_3_target":I==4?"slot_4_target":I==5?"slot_5_target":I==6?"slot_6_target":"slot_7_target",
          I==0?"Objeto 1":I==1?"Objeto 2":I==2?"Objeto 3":I==3?"Objeto 4":I==4?"Objeto 5":I==5?"Objeto 6":I==6?"Objeto 7":"Objeto 8",
          "",ObjectReferenceScope::Any,"Nenhum",
          [](const ComponentValue &v){return audio_mixer_detail::snapshot(v).slots[I].target;},
          [](ComponentValue &v,u64 x){audio_mixer_detail::snapshot(v).slots[I].target=x;},
          {"Valores","","Bus de áudio, ou o objeto do bus com o efeito",audio_mixer_detail::slotVisible<I>}};
}
template<u32 I> constexpr ComponentEnum audioSnapshotParameter() {
  return {I==0?"slot_0_parameter":I==1?"slot_1_parameter":I==2?"slot_2_parameter":I==3?"slot_3_parameter":I==4?"slot_4_parameter":I==5?"slot_5_parameter":I==6?"slot_6_parameter":"slot_7_parameter",
          "Parâmetro",audioSnapshotParameterOptions,
          [](const ComponentValue &v){return static_cast<u32>(audio_mixer_detail::snapshot(v).slots[I].parameter);},
          [](ComponentValue &v,u32 x){audio_mixer_detail::snapshot(v).slots[I].parameter=static_cast<AudioSnapshotParameter>(x);},
          {"Valores","","Usa o primeiro componente daquele tipo no objeto",audio_mixer_detail::slotUsed<I>}};
}
template<u32 I> constexpr ComponentNumber audioSnapshotValue() {
  return {"Valor",-100000,100000,.01f,
          [](const ComponentValue &v)->const float &{return audio_mixer_detail::snapshot(v).slots[I].value;},
          [](ComponentValue &v)->float *{return &audio_mixer_detail::snapshot(v).slots[I].value;},
          I==0?"slot_0_value":I==1?"slot_1_value":I==2?"slot_2_value":I==3?"slot_3_value":I==4?"slot_4_value":I==5?"slot_5_value":I==6?"slot_6_value":"slot_7_value",
          {"Valores","","Limitado à faixa da propriedade de destino",audio_mixer_detail::slotUsed<I>},false};
}
inline constexpr std::array<ComponentObjectReference,8> audioSnapshotReferences{{
  audioSnapshotReference<0>(),audioSnapshotReference<1>(),audioSnapshotReference<2>(),audioSnapshotReference<3>(),
  audioSnapshotReference<4>(),audioSnapshotReference<5>(),audioSnapshotReference<6>(),audioSnapshotReference<7>()}};
inline constexpr std::array<ComponentEnum,8> audioSnapshotEnums{{
  audioSnapshotParameter<0>(),audioSnapshotParameter<1>(),audioSnapshotParameter<2>(),audioSnapshotParameter<3>(),
  audioSnapshotParameter<4>(),audioSnapshotParameter<5>(),audioSnapshotParameter<6>(),audioSnapshotParameter<7>()}};
inline constexpr std::array<ComponentNumber,9> audioSnapshotNumbers{{
  AE_AUDIO_NUMBER(AudioSnapshot,"Transição",0,60,.05f,transition,"transition","Snapshot","s","Tempo real até os valores do snapshot",nullptr,false),
  audioSnapshotValue<0>(),audioSnapshotValue<1>(),audioSnapshotValue<2>(),audioSnapshotValue<3>(),
  audioSnapshotValue<4>(),audioSnapshotValue<5>(),audioSnapshotValue<6>(),audioSnapshotValue<7>()}};
inline constexpr std::array<ComponentBoolean,2> audioSnapshotBooleans{{
  AE_AUDIO_ENABLED(AudioSnapshot,"Snapshot","Desligado recusa transições"),
  {"apply_at_start","Aplicar ao iniciar",[](const ComponentValue &v){return audio_mixer_detail::snapshot(v).applyAtStart;},
   [](ComponentValue &v,bool b){audio_mixer_detail::snapshot(v).applyAtStart=b;},
   {"Snapshot","","Os valores entram no primeiro quadro do Play, sem transição"}}}};
inline constexpr std::array<ComponentParameter,1> audioSnapshotTransitionParameters{{{"seconds","Duração",ComponentValueKind::Number,"s"}}};
inline constexpr std::array<ComponentMethod,2> audioSnapshotMethods{{
  {"transition_to","Transicionar","Leva os valores atuais aos do snapshot no tempo dado; negativo usa a Transição do componente",audioSnapshotTransitionParameters},
  {"apply","Aplicar","Aplica os valores na hora, sem transição"},
}};

#undef AE_AUDIO_NUMBER
#undef AE_AUDIO_ENABLED

inline const ComponentType AudioFilter::descriptor{"astra.audio.filter",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<AudioFilter>();},
  audioFilterNumbers,audioFilterBooleans,audioFilterEnums,nullptr,true};
inline const ComponentType AudioEcho::descriptor{"astra.audio.echo",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<AudioEcho>();},
  audioEchoNumbers,audioEchoBooleans,{},nullptr,true};
inline const ComponentType AudioReverb::descriptor{"astra.audio.reverb",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<AudioReverb>();},
  audioReverbNumbers,audioReverbBooleans,{},nullptr,true};
inline constexpr std::array<ComponentMethod,1> audioCompressorMethods{{
  {"reduction_db","Redução (dB)","Quanto o compressor está abaixando agora",{},ComponentValueKind::Number}}};
inline const ComponentType AudioCompressor::descriptor{"astra.audio.compressor",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<AudioCompressor>();},
  audioCompressorNumbers,audioCompressorBooleans,{},nullptr,true,audioCompressorReferences,{},{},{},{},{},audioCompressorMethods};
inline const ComponentType AudioSend::descriptor{"astra.audio.send",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<AudioSend>();},
  audioSendNumbers,audioSendBooleans,{},nullptr,true,audioSendReferences};
inline const ComponentType AudioSnapshot::descriptor{"astra.audio.snapshot",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<AudioSnapshot>();},
  audioSnapshotNumbers,audioSnapshotBooleans,audioSnapshotEnums,nullptr,false,audioSnapshotReferences,{},{},{},{},{},audioSnapshotMethods};

} // namespace ae::scene
