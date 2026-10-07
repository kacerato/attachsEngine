// Seletor do Tween de propriedade: lista o que o alvo deixa interpolar.
//
// A lista usa runtime::numberTweenable, a mesma regra que o Play aplica ao
// escrever; uma opção mostrada aqui é uma opção aceita em execução.
#pragma once
#include "runtime/game_world.h"
#include "scene/component_schema.h"

#include <string>
#include <vector>

namespace ae::editor {

struct TweenablePropertyOption {
  std::string type,property,componentName,propertyName,unit;
  float value=0;
};

inline std::vector<TweenablePropertyOption> tweenablePropertyOptions(const scene::Components &components) {
  std::vector<TweenablePropertyOption> options;
  for(usize i=0;i<components.size();++i) {
    const auto *value=components.at(i);
    const auto *schema=scene::findComponentSchema(value->type().id);
    if(!schema) continue;
    for(const auto &number:value->type().numbers) {
      if(!runtime::numberTweenable(*schema,number,*value)) continue;
      // Componentes repetidos: o tween resolve o primeiro do tipo.
      bool repeated=false;
      for(const auto &option:options) repeated|=option.type==value->type().id&&option.property==number.id;
      if(repeated) continue;
      options.push_back({std::string(value->type().id),std::string(number.id),schema->name,number.name,
                         std::string(number.presentation.unit),number.read(*value)});
    }
  }
  return options;
}

} // namespace ae::editor
