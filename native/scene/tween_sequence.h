// Sequência de tweens: encadeia Transform Tweens de outros objetos em etapas.
//
// Cada etapa aponta um objeto que tem Transform Tween; a etapa começa quando a
// anterior termina, ou junto com ela ("Junto da anterior"), depois do próprio
// intervalo. Enquanto a sequência roda, ela é dona dos tweens das etapas:
// reinicia, pausa e cancela os mesmos estados do avaliador de Play
// (runtime/scene_tweens.h), sem segundo interpolador.
//
// Referências: Godot 4.5 Tween (chain/parallel/set_loops)
// https://docs.godotengine.org/en/4.5/classes/class_tween.html
// A Unity 6000.0 não tem sequência nativa; o princípio Append/Join/Insert vem
// do mesmo modelo de etapas encadeadas e paralelas.
#pragma once
#include "scene/components.h"
#include "scene/transform_tween.h"

#include <array>
#include <cmath>
#include <limits>

namespace ae::scene {

inline constexpr u32 kTweenSequenceSteps=8;

class TweenSequence final : public ComponentValue {
public:
  struct Step {u64 target=0;bool join=false;float interval=0;};
  bool enabled=true,autoplay=true,ignoreTimeScale=false;
  u32 loops=1;
  std::array<Step,kTweenSequenceSteps> steps{};
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<TweenSequence>(*this);}
  bool valid() const override {
    if(loops>100000) return false;
    for(const auto &s:steps)
      if(s.target>std::numeric_limits<u32>::max() || !std::isfinite(s.interval) || s.interval<0 || s.interval>3600) return false;
    return true;
  }
  void write(std::ostream &out) const override {
    out<<enabled<<' '<<autoplay<<' '<<ignoreTimeScale<<' '<<loops;
    for(const auto &s:steps) out<<' '<<s.target<<' '<<s.join<<' '<<s.interval;
  }
  bool read(std::istream &in,u32 version) override {
    if(version!=1 || !(in>>enabled>>autoplay>>ignoreTimeScale>>loops)) return false;
    for(auto &s:steps) if(!(in>>s.target>>s.join>>s.interval)) return false;
    return valid();
  }
  // Etapas autoradas, na ordem; referências vazias são puladas.
  u32 stepCount() const noexcept {u32 n=0;for(const auto &s:steps) n+=s.target!=0;return n;}
};
inline const TweenSequence &tweenSequence(const ComponentValue &v) {return static_cast<const TweenSequence &>(v);}
inline TweenSequence &tweenSequence(ComponentValue &v) {return static_cast<TweenSequence &>(v);}

namespace tween_sequence_detail {
// Etapa I aparece quando é a primeira ou quando a anterior já aponta um objeto.
template<u32 I> bool stepVisible(const ComponentValue &v) {return I==0 || tweenSequence(v).steps[I-1].target!=0;}
template<u32 I> bool stepUsed(const ComponentValue &v) {return tweenSequence(v).steps[I].target!=0;}
template<u32 I> bool joinVisible(const ComponentValue &v) {return I>0 && tweenSequence(v).steps[I].target!=0;}
template<u32 I> ComponentObjectReference reference(const char *name) {
  return {I==0?"step_0":I==1?"step_1":I==2?"step_2":I==3?"step_3":I==4?"step_4":I==5?"step_5":I==6?"step_6":"step_7",name,
          "astra.tween.transform",ObjectReferenceScope::Any,"Nenhum",
          [](const ComponentValue &v){return tweenSequence(v).steps[I].target;},
          [](ComponentValue &v,u64 x){tweenSequence(v).steps[I].target=x;},
          {"Etapas","","Objeto com Transform Tween; a sequência reinicia esse tween quando a etapa começa",stepVisible<I>}};
}
template<u32 I> ComponentBoolean join() {
  return {I==1?"join_1":I==2?"join_2":I==3?"join_3":I==4?"join_4":I==5?"join_5":I==6?"join_6":I==7?"join_7":"join_0",
          I==1?"Etapa 2 junto da anterior":I==2?"Etapa 3 junto da anterior":I==3?"Etapa 4 junto da anterior":I==4?"Etapa 5 junto da anterior":
          I==5?"Etapa 6 junto da anterior":I==6?"Etapa 7 junto da anterior":I==7?"Etapa 8 junto da anterior":"Etapa 1 junto da anterior",
          [](const ComponentValue &v){return tweenSequence(v).steps[I].join;},
          [](ComponentValue &v,bool b){tweenSequence(v).steps[I].join=b;},
          {"Etapas","","Começa com a etapa anterior em vez de esperar ela terminar",joinVisible<I>}};
}
template<u32 I> ComponentNumber interval(const char *name) {
  return {name,0,3600,.05f,
          [](const ComponentValue &v)->const float &{return tweenSequence(v).steps[I].interval;},
          [](ComponentValue &v)->float *{return &tweenSequence(v).steps[I].interval;},
          I==0?"interval_0":I==1?"interval_1":I==2?"interval_2":I==3?"interval_3":I==4?"interval_4":I==5?"interval_5":I==6?"interval_6":"interval_7",
          {"Etapas","s","Espera antes de a etapa começar, somada à espera do próprio tween",stepUsed<I>}};
}
} // namespace tween_sequence_detail

inline const std::array<ComponentObjectReference,kTweenSequenceSteps> tweenSequenceReferences{
  tween_sequence_detail::reference<0>("Etapa 1"),tween_sequence_detail::reference<1>("Etapa 2"),
  tween_sequence_detail::reference<2>("Etapa 3"),tween_sequence_detail::reference<3>("Etapa 4"),
  tween_sequence_detail::reference<4>("Etapa 5"),tween_sequence_detail::reference<5>("Etapa 6"),
  tween_sequence_detail::reference<6>("Etapa 7"),tween_sequence_detail::reference<7>("Etapa 8")};
inline const std::array<ComponentNumber,kTweenSequenceSteps> tweenSequenceNumbers{
  tween_sequence_detail::interval<0>("Intervalo da etapa 1"),tween_sequence_detail::interval<1>("Intervalo da etapa 2"),
  tween_sequence_detail::interval<2>("Intervalo da etapa 3"),tween_sequence_detail::interval<3>("Intervalo da etapa 4"),
  tween_sequence_detail::interval<4>("Intervalo da etapa 5"),tween_sequence_detail::interval<5>("Intervalo da etapa 6"),
  tween_sequence_detail::interval<6>("Intervalo da etapa 7"),tween_sequence_detail::interval<7>("Intervalo da etapa 8")};
inline const std::array<ComponentBoolean,10> tweenSequenceBooleans{
  ComponentBoolean{"enabled","Ativa",[](const ComponentValue &v){return tweenSequence(v).enabled;},[](ComponentValue &v,bool b){tweenSequence(v).enabled=b;},
   {"Execução","","Desligada não reinicia etapas; a configuração é preservada"}},
  ComponentBoolean{"autoplay","Iniciar no Play",[](const ComponentValue &v){return tweenSequence(v).autoplay;},[](ComponentValue &v,bool b){tweenSequence(v).autoplay=b;},
   {"Execução","","Sem isto a sequência espera o método Tocar (script ou Conexão de evento)"}},
  ComponentBoolean{"ignore_time_scale","Ignorar escala de tempo",[](const ComponentValue &v){return tweenSequence(v).ignoreTimeScale;},[](ComponentValue &v,bool b){tweenSequence(v).ignoreTimeScale=b;},
   {"Execução","","Vale para os intervalos da sequência; cada tween segue a própria opção"}},
  tween_sequence_detail::join<1>(),tween_sequence_detail::join<2>(),tween_sequence_detail::join<3>(),tween_sequence_detail::join<4>(),
  tween_sequence_detail::join<5>(),tween_sequence_detail::join<6>(),tween_sequence_detail::join<7>()};
inline constexpr std::array<ComponentEnum,1> tweenSequenceEnums{{
  {"loops","Repetição",tweenLoops,[](const ComponentValue &v){return tweenSequence(v).loops;},[](ComponentValue &v,u32 x){tweenSequence(v).loops=x;},
   {"Execução","","Quantas vezes a lista inteira de etapas é percorrida"}},
}};
inline constexpr std::array<ComponentMethod,5> tweenSequenceMethods{{
  {"play","Tocar","Recomeça da etapa 1, cancelando os tweens das etapas em andamento"},
  {"cancel","Cancelar","Interrompe a sequência e os tweens da etapa em andamento, sem voltar poses"},
  {"pause","Pausar","Congela intervalo e tweens da etapa em andamento"},
  {"resume","Retomar","Continua de onde pausou"},
  {"step","Etapa atual","Número da etapa em andamento (1 a 8); zero parada",{},ComponentValueKind::Integer},
}};
inline constexpr std::array<ComponentParameter,1> tweenSequenceStepPayload{{{"step","Etapa",ComponentValueKind::Integer}}};
inline constexpr std::array<ComponentEvent,2> tweenSequenceEvents{{
  {"step_started","Etapa começou","Emitido quando cada etapa reinicia o seu tween; carrega o número da etapa",tweenSequenceStepPayload},
  {"completed","Concluiu","Emitido uma vez quando as repetições finitas terminam"},
}};
inline const ComponentType TweenSequence::descriptor{
  "astra.tween.sequence",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<TweenSequence>();},
  tweenSequenceNumbers,tweenSequenceBooleans,tweenSequenceEnums,nullptr,false,tweenSequenceReferences,
  {},{},{},{},{},tweenSequenceMethods,tweenSequenceEvents};

} // namespace ae::scene
