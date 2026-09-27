#pragma once
#include "scene/components.h"
#include "scene/script_gradient.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace ae::scene {
// Property identity and declared type survive source changes. Unmentioned
// fields retain the constructor default; only authored overrides are stored.
struct ScriptPropertyValue {
  std::string id,valueType,value;
};
// Campo que referencia um COMPONENTE (Unity: `public Rigidbody body;`). O tipo
// declarado é "component:<id do tipo>" e o valor "<objeto>:<instância>", com
// "0:0" para vazio. A instância é a identidade estável do componente dentro do
// objeto: dois colisores no mesmo objeto são dois valores diferentes.
inline std::string_view scriptComponentTypeId(std::string_view type) {
  constexpr std::string_view prefix="component:";
  return type.starts_with(prefix)?type.substr(prefix.size()):std::string_view{};
}
inline bool parseScriptComponentValue(std::string_view text,u64 &object,u64 &instance) {
  const auto colon=text.find(':');
  if(colon==std::string_view::npos || colon==0 || colon+1>=text.size()) return false;
  const auto number=[](std::string_view digits,u64 &out) {
    if(digits.empty() || digits.size()>20) return false;
    out=0;
    for(const char c:digits) {
      if(c<'0'||c>'9') return false;
      const u64 digit=static_cast<u64>(c-'0');
      if(out>(~0ull-digit)/10) return false;
      out=out*10+digit;
    }
    return true;
  };
  return number(text.substr(0,colon),object) && number(text.substr(colon+1),instance) && ((object==0)==(instance==0));
}
inline std::string scriptComponentValue(u64 object,u64 instance) {
  return std::to_string(object)+":"+std::to_string(instance);
}
// Cor (Unity: campo `Color` e [ColorUsage(showAlpha, hdr)]): tipo "color" com
// marcas opcionais ":hdr" e ":noalpha"; valor "r g b a" em RGB LINEAR. Sem hdr
// nenhum canal passa de 1; alfa sempre entre 0 e 1.
inline bool scriptColorType(std::string_view type,bool *hdr=nullptr,bool *alpha=nullptr) {
  if(!type.starts_with("color")) return false;
  std::string_view flags=type.substr(5);
  bool high=false,transparent=true;
  while(!flags.empty()) {
    if(flags.front()!=':') return false;
    flags.remove_prefix(1);
    const auto end=flags.find(':');
    const auto flag=flags.substr(0,end);
    if(flag=="hdr") high=true;
    else if(flag=="noalpha") transparent=false;
    else return false;
    flags=end==std::string_view::npos?std::string_view{}:flags.substr(end);
  }
  if(hdr) *hdr=high;
  if(alpha) *alpha=transparent;
  return true;
}
inline bool parseScriptColor(std::string_view text,float (&rgba)[4]) {
  std::istringstream in{std::string(text)};in.imbue(std::locale::classic());
  if(!(in>>rgba[0]>>rgba[1]>>rgba[2]>>rgba[3])) return false;
  in>>std::ws;
  if(!in.eof()) return false;
  for(const float v:rgba) if(!std::isfinite(v) || v<0) return false;
  return true;
}
inline std::string scriptColorValue(const float (&rgba)[4]) {
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<std::setprecision(9)<<rgba[0]<<' '<<rgba[1]<<' '<<rgba[2]<<' '<<rgba[3];
  return out.str();
}
// Lista (Unity: campo `float[]`/`List<Transform>`, Manual/InspectorArray): tipo
// "array:<tipo do elemento>", valor "<N>" seguido de N elementos entre aspas
// no formato do elemento — `3 "1.5" "2" "4"`. Lista de lista não existe.
inline constexpr u32 kScriptArrayMaximum=1024;
inline std::string_view scriptArrayElementType(std::string_view type) {
  constexpr std::string_view prefix="array:";
  return type.starts_with(prefix)?type.substr(prefix.size()):std::string_view{};
}
inline bool parseScriptArray(std::string_view text,std::vector<std::string> &out) {
  out.clear();
  std::istringstream in{std::string(text)};in.imbue(std::locale::classic());
  u32 count=0;
  if(!(in>>count) || count>kScriptArrayMaximum) return false;
  for(u32 i=0;i<count;++i) {std::string element;if(!(in>>std::quoted(element))) return false;out.push_back(std::move(element));}
  in>>std::ws;return in.eof();
}
inline std::string scriptArrayValue(const std::vector<std::string> &elements) {
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<elements.size();
  for(const auto &element:elements) out<<' '<<std::quoted(element);
  return out.str();
}
// Valor de um elemento novo quando a lista está vazia (sem anterior para copiar).
inline std::string scriptElementDefault(std::string_view type) {
  if(type=="bool") return "false";
  if(type=="float"||type=="int32"||type=="enum"||type=="object") return "0";
  if(type=="vector3") return "0 0 0";
  if(scriptColorType(type)) return "1 1 1 1";
  if(scriptGradientType(type)) return scriptGradientValue({});
  if(type.starts_with("component:")) return "0:0";
  return "";
}
// Objeto apontado por um campo "object" ou "component:…"; falso para os demais
// tipos. Duplicar, reimportar e editar em Play remapeiam por aqui.
inline bool scriptPropertyObject(const ScriptPropertyValue &p,u64 &object) {
  u64 instance=0;
  if(!scriptComponentTypeId(p.valueType).empty()) return parseScriptComponentValue(p.value,object,instance);
  if(p.valueType!="object") return false;
  object=0;
  for(const char c:p.value) {if(c<'0'||c>'9') return false;object=object*10+static_cast<u64>(c-'0');}
  return !p.value.empty();
}
inline void retargetScriptPropertyObject(ScriptPropertyValue &p,u64 object) {
  u64 previous=0,instance=0;
  if(!scriptComponentTypeId(p.valueType).empty()) {
    if(parseScriptComponentValue(p.value,previous,instance)) p.value=scriptComponentValue(object,object?instance:0);
  } else if(p.valueType=="object") p.value=std::to_string(object);
}
// Todos os objetos que o campo cita, inclusive os elementos de uma lista de
// referências. `remap` troca cada um pelo que a função devolver.
template<class Visit> void forEachScriptPropertyObject(const ScriptPropertyValue &p,Visit &&visit) {
  u64 object=0;
  if(scriptPropertyObject(p,object)) {visit(object);return;}
  const auto element=scriptArrayElementType(p.valueType);
  if(element.empty()) return;
  std::vector<std::string> items;
  if(!parseScriptArray(p.value,items)) return;
  for(const auto &item:items) {
    ScriptPropertyValue single{p.id,std::string(element),item};
    if(scriptPropertyObject(single,object)) visit(object);
  }
}
template<class Map> void remapScriptPropertyObjects(ScriptPropertyValue &p,Map &&map) {
  u64 object=0;
  if(scriptPropertyObject(p,object)) {const u64 target=map(object);if(target!=object) retargetScriptPropertyObject(p,target);return;}
  const auto element=scriptArrayElementType(p.valueType);
  if(element.empty()) return;
  std::vector<std::string> items;
  if(!parseScriptArray(p.value,items)) return;
  for(auto &item:items) {
    ScriptPropertyValue single{p.id,std::string(element),item};
    if(!scriptPropertyObject(single,object)) continue;
    const u64 target=map(object);
    if(target!=object) {retargetScriptPropertyObject(single,target);item=single.value;}
  }
  p.value=scriptArrayValue(items);
}
inline bool validScriptPropertyValue(std::string_view type,std::string_view text) {
  if(const auto element=scriptArrayElementType(type);!element.empty()) {
    if(!scriptArrayElementType(element).empty() || text.size()>256*1024) return false;
    std::vector<std::string> items;
    if(!parseScriptArray(text,items)) return false;
    for(const auto &item:items) if(!validScriptPropertyValue(element,item)) return false;
    return true;
  }
  if(text.size()>4096 || text.find('\0')!=std::string_view::npos) return false;
  if(bool hdr=false;scriptGradientType(type,&hdr)) {
    ScriptGradient gradient;
    return parseScriptGradient(text,gradient) && gradient.valid(hdr);
  }
  if(bool hdr=false;scriptColorType(type,&hdr)) {
    float rgba[4];
    if(!parseScriptColor(text,rgba) || rgba[3]>1) return false;
    for(u32 i=0;i<3;++i) if(rgba[i]>(hdr?65504.f:1.f)) return false;
    return true;
  }
  if(const auto component=scriptComponentTypeId(type);!component.empty()) {
    if(component.size()>256) return false;
    for(const char c:component)
      if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'||c=='/')) return false;
    u64 object=0,instance=0;
    return parseScriptComponentValue(text,object,instance);
  }
  if(type=="string" || type=="asset") return true;
  if(type=="bool") return text=="true" || text=="false";
  std::istringstream in{std::string(text)};in.imbue(std::locale::classic());
  if(type=="float") {float v=0;if(!(in>>v)||!std::isfinite(v)) return false;}
  else if(type=="int32" || type=="enum") {std::int32_t v=0;if(!(in>>v)) return false;}
  else if(type=="vector3") {float x=0,y=0,z=0;if(!(in>>x>>y>>z)||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)) return false;}
  else if(type=="object") {u64 id=0;if(text.empty() || text.front()=='-' || !(in>>id)) return false;}
  else return false;
  in>>std::ws;return in.eof();
}
struct ScriptBehavior final : ComponentValue {
  std::string scriptType,source;
  bool enabled=true;
  std::vector<ScriptPropertyValue> properties;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<ScriptBehavior>(*this);}
  bool valid() const override {
    if(scriptType.empty() || scriptType.size()>256 || source.size()>1024 || properties.size()>1024) return false;
    for(usize i=0;i<properties.size();++i) {
      const auto &p=properties[i];
      if(p.id.empty()||p.id.size()>256||!validScriptPropertyValue(p.valueType,p.value)) return false;
      for(usize j=0;j<i;++j) if(properties[j].id==p.id) return false;
    }
    return true;
  }
  bool setProperty(std::string_view id,std::string_view declaredType,std::string_view value) {
    if(id.empty()||id.size()>256||!validScriptPropertyValue(declaredType,value)) return false;
    for(auto &p:properties) if(p.id==id) {p.valueType=declaredType;p.value=value;return true;}
    if(properties.size()>=1024) return false;
    properties.push_back({std::string(id),std::string(declaredType),std::string(value)});return true;
  }
  void write(std::ostream &out) const override {
    out<<std::quoted(scriptType)<<' '<<std::quoted(source)<<' '<<enabled<<' '<<properties.size();
    for(const auto &p:properties) out<<' '<<std::quoted(p.id)<<' '<<std::quoted(p.valueType)<<' '<<std::quoted(p.value);
  }
  bool read(std::istream &in,u32 version) override {
    ScriptBehavior candidate;u32 count=0,state=0;
    if(version!=1 || !(in>>std::quoted(candidate.scriptType)>>std::quoted(candidate.source)>>state>>count) || state>1 || count>1024) return false;
    candidate.enabled=state!=0;
    for(u32 i=0;i<count;++i) {
      ScriptPropertyValue p;if(!(in>>std::quoted(p.id)>>std::quoted(p.valueType)>>std::quoted(p.value))) return false;
      candidate.properties.push_back(std::move(p));
    }
    if(!candidate.valid()) return false;
    scriptType=std::move(candidate.scriptType);source=std::move(candidate.source);
    enabled=candidate.enabled;properties=std::move(candidate.properties);return true;
  }
};
// As propriedades DO SCRIPT vêm da reflexão do tipo em C# e por isso não estão
// aqui. `enabled` é diferente: pertence ao componente, não ao código do autor, e
// precisa da mesma identidade persistente das demais — sem ela, desligar um
// comportamento seria possível pelo dedo no inspetor e impossível por API,
// preset ou animação, que é exatamente a divergência que o schema existe para
// impedir.
inline constexpr std::array<ComponentBoolean,1> scriptBehaviorBooleans{{
  {"enabled","Ativo",[](const ComponentValue &v){return static_cast<const ScriptBehavior&>(v).enabled;},
   [](ComponentValue &v,bool b){static_cast<ScriptBehavior&>(v).enabled=b;},{"Execução"}}
}};
inline const ComponentType ScriptBehavior::descriptor{
  "astra.script.behavior",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<ScriptBehavior>();},
  {},scriptBehaviorBooleans,{},nullptr,true
};
inline const ScriptBehavior *scriptBehavior(const ComponentValue *value) {
  return value && &value->type()==&ScriptBehavior::descriptor?static_cast<const ScriptBehavior *>(value):nullptr;
}
} // namespace ae::scene
