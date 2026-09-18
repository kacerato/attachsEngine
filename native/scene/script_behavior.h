#pragma once
#include "scene/components.h"
#include <array>
#include <cstdint>

namespace ae::scene {
// Property identity and declared type survive source changes. Unmentioned
// fields retain the constructor default; only authored overrides are stored.
struct ScriptPropertyValue {
  std::string id,valueType,value;
};
inline bool validScriptPropertyValue(std::string_view type,std::string_view text) {
  if(text.size()>4096 || text.find('\0')!=std::string_view::npos) return false;
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
