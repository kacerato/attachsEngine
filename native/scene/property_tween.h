// Tween de propriedade: interpola uma propriedade numérica de um componente,
// neste objeto ou em outro, de forma persistente (autorada no Inspector).
//
// Só aceita propriedades que o próprio descritor marca como interpoláveis
// (ComponentNumber::tweenable): a mesma regra do NumberTween por script, que
// GameWorld::validateTweenNumber aplica em Play. O componente e a propriedade
// são gravados pelo id persistente do tipo e do PropertyId, não pela posição.
//
// Referência: Godot 4.5 Tween.tween_property
// https://docs.godotengine.org/en/4.5/classes/class_tween.html#class-tween-method-tween-property
#pragma once
#include "scene/components.h"
#include "scene/transform_tween.h"

#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <string>

namespace ae::scene {

class PropertyTween final : public ComponentValue {
public:
  bool enabled=true,autoplay=true,pingpong=false,relative=false,ignoreTimeScale=false;
  float duration=1,delay=0,destination=0;
  u32 easing=0,loops=1;
  u64 target=0;                  // zero: este objeto
  std::string componentType,property;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<PropertyTween>(*this);}
  static bool validId(const std::string &id) {
    if(id.size()>127) return false;
    for(unsigned char c:id) if(c<33||c==127||c=='"'||c=='\\') return false;
    return true;
  }
  bool valid() const override {
    return std::isfinite(duration) && duration>=.001f && duration<=36000 && std::isfinite(delay) && delay>=0 && delay<=36000 &&
           std::isfinite(destination) && destination>=-100000 && destination<=100000 && easing<=3 && loops<=100000 &&
           target<=std::numeric_limits<u32>::max() && validId(componentType) && validId(property);
  }
  void write(std::ostream &out) const override {
    out<<enabled<<' '<<autoplay<<' '<<pingpong<<' '<<relative<<' '<<ignoreTimeScale<<' '<<duration<<' '<<delay<<' '<<destination
       <<' '<<easing<<' '<<loops<<' '<<target<<' '<<std::quoted(componentType)<<' '<<std::quoted(property);
  }
  bool read(std::istream &in,u32 version) override {
    return version==1 && static_cast<bool>(in>>enabled>>autoplay>>pingpong>>relative>>ignoreTimeScale>>duration>>delay>>destination
                                              >>easing>>loops>>target>>std::quoted(componentType)>>std::quoted(property)) && valid();
  }
};
inline const PropertyTween &propertyTween(const ComponentValue &v) {return static_cast<const PropertyTween &>(v);}
inline PropertyTween &propertyTween(ComponentValue &v) {return static_cast<PropertyTween &>(v);}

inline constexpr std::array<ComponentNumber,3> propertyTweenNumbers{{
  {"Destino",-100000,100000,.1f,[](const ComponentValue &v)->const float &{return propertyTween(v).destination;},
   [](ComponentValue &v)->float *{return &propertyTween(v).destination;},"destination",
   {"Propriedade","","Valor final; com \"Destino relativo\", soma ao valor do início"}},
  {"Duração",.001f,36000,.1f,[](const ComponentValue &v)->const float &{return propertyTween(v).duration;},
   [](ComponentValue &v)->float *{return &propertyTween(v).duration;},"duration",{"Tempo","s"}},
  {"Espera",0,36000,.1f,[](const ComponentValue &v)->const float &{return propertyTween(v).delay;},
   [](ComponentValue &v)->float *{return &propertyTween(v).delay;},"delay",{"Tempo","s"}},
}};
inline constexpr std::array<ComponentBoolean,5> propertyTweenBooleans{{
  {"enabled","Ativo",[](const ComponentValue &v){return propertyTween(v).enabled;},[](ComponentValue &v,bool b){propertyTween(v).enabled=b;},{"Tempo"}},
  {"autoplay","Iniciar no Play",[](const ComponentValue &v){return propertyTween(v).autoplay;},[](ComponentValue &v,bool b){propertyTween(v).autoplay=b;},{"Tempo"}},
  {"pingpong","Ida e volta",[](const ComponentValue &v){return propertyTween(v).pingpong;},[](ComponentValue &v,bool b){propertyTween(v).pingpong=b;},{"Repetição"}},
  {"relative","Destino relativo",[](const ComponentValue &v){return propertyTween(v).relative;},[](ComponentValue &v,bool b){propertyTween(v).relative=b;},{"Propriedade"}},
  {"ignore_time_scale","Ignorar escala de tempo",[](const ComponentValue &v){return propertyTween(v).ignoreTimeScale;},[](ComponentValue &v,bool b){propertyTween(v).ignoreTimeScale=b;},{"Tempo"}},
}};
inline constexpr std::array<ComponentEnum,2> propertyTweenEnums{{
  {"easing","Curva",tweenEasings,[](const ComponentValue &v){return propertyTween(v).easing;},[](ComponentValue &v,u32 x){propertyTween(v).easing=x;},{"Tempo"}},
  {"loops","Repetição",tweenLoops,[](const ComponentValue &v){return propertyTween(v).loops;},[](ComponentValue &v,u32 x){propertyTween(v).loops=x;},{"Repetição"}},
}};
inline constexpr std::array<ComponentObjectReference,1> propertyTweenReferences{{
  {"target","Alvo","",ObjectReferenceScope::Any,"Este objeto",
   [](const ComponentValue &v){return propertyTween(v).target;},[](ComponentValue &v,u64 x){propertyTween(v).target=x;},
   {"Propriedade","","Objeto dono do componente animado; vazio usa este objeto"}},
}};
inline constexpr std::array<ComponentMethod,5> propertyTweenMethods{{
  {"restart","Reiniciar","Recomeça do valor atual da propriedade, conservando a pausa"},
  {"cancel","Cancelar","Interrompe sem voltar ao valor inicial"},
  {"pause","Pausar","Congela o progresso"},
  {"resume","Retomar","Continua o progresso pausado"},
  {"elapsed","Decorrido","Segundos desde o início, incluindo a espera",{},ComponentValueKind::Number},
}};
inline constexpr std::array<ComponentEvent,1> propertyTweenEvents{{
  {"completed","Concluiu","Emitido uma vez quando as repetições finitas terminam, depois do valor final"}
}};
inline const ComponentType PropertyTween::descriptor{
  "astra.tween.property",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<PropertyTween>();},
  propertyTweenNumbers,propertyTweenBooleans,propertyTweenEnums,nullptr,true,propertyTweenReferences,
  {},{},{},{},{},propertyTweenMethods,propertyTweenEvents};

// Etapa de sequência ou alvo de conexão: objeto com algum tween persistente.
inline bool hasPersistentTween(const Components &components) {
  return components.find(TransformTween::descriptor) || components.find(PropertyTween::descriptor);
}

} // namespace ae::scene
