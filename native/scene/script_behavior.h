#pragma once
#include "scene/components.h"
#include <array>
#include <cstdint>
#include <string>

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
inline bool validScriptPropertyValue(std::string_view type,std::string_view text) {
  if(text.size()>4096 || text.find('\0')!=std::string_view::npos) return false;
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
