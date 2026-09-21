#pragma once
// Número com vírgula decimal, sem depender de locale.
//
// O editor mostra medidas em vários lugares (importação, densidade de texel,
// tamanho na cena) e todas precisam sair iguais. Duas implementações de
// arredondamento produziriam "1,05" numa tela e "1,1" na outra para o mesmo
// valor, e o autor leria isso como diferença do modelo.
#include "core/base.h"

#include <cmath>
#include <string>

namespace ae::editor {

inline std::string decimalText(double value, int digits) {
  const long long factor = digits == 0 ? 1 : digits == 1 ? 10 : digits == 2 ? 100 : 1000;
  const long long scaled = std::llround(std::fabs(value) * static_cast<double>(factor));
  std::string text = std::to_string(scaled / factor);
  if (digits > 0) {
    std::string fraction = std::to_string(scaled % factor);
    fraction.insert(0, static_cast<usize>(digits) - fraction.size(), '0');
    text += "," + fraction;
  }
  return (value < 0 && scaled ? "-" : "") + text;
}

} // namespace ae::editor
