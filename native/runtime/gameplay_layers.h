// As camadas de gameplay do projeto.
//
// São um conceito do JOGO, não do backend: "Personagem", "Cenário", "Gatilho".
// As camadas amplas do Jolt (estático contra móvel) continuam existindo por
// baixo e continuam decidindo o que a broadphase precisa parear; estas decidem
// o que o projeto QUER que colida, e a matriz vale no solver — um par proibido
// nunca gera contato, em vez de gerar e ser descartado depois.
//
// Limite: 32 camadas, porque a linha da matriz é uma máscara de 32 bits e é
// isso que a fronteira nativa aceita. O objeto guarda sua camada no campo
// `layer` que a cena já persiste, então um projeto antigo abre com todo mundo
// na camada 0 e nada muda de comportamento.
#pragma once
#include "core/base.h"

#include <array>
#include <iomanip>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>

namespace ae::runtime {

class GameplayLayers final {
public:
  static constexpr u32 kCount = 32;
  static constexpr u32 kNameCapacity = 32;

  GameplayLayers() { reset(); }

  void reset() {
    for (auto &name : names_) name.clear();
    names_[0] = "Padrão";
    // Tudo interage por padrão: a configuração é uma restrição que o usuário
    // adiciona, não uma permissão que ele precisa lembrar de dar.
    for (auto &row : matrix_) row = 0xffffffffu;
  }

  std::string_view name(u32 layer) const {
    return layer < kCount ? std::string_view(names_[layer]) : std::string_view{};
  }
  bool named(u32 layer) const { return layer < kCount && !names_[layer].empty(); }
  // Nome vazio apaga a camada; ela continua existindo como índice, mas some da
  // interface. Nome duplicado é recusado: duas camadas com o mesmo rótulo
  // tornariam a matriz impossível de ler.
  bool setName(u32 layer, std::string_view value) {
    if (layer >= kCount || value.size() > kNameCapacity) return false;
    if (value.find('"') != std::string_view::npos || value.find('\n') != std::string_view::npos) return false;
    for (u32 other = 0; other < kCount; ++other)
      if (other != layer && !value.empty() && names_[other] == value) return false;
    if (layer == 0 && value.empty()) return false;  // a camada padrão sempre existe
    names_[layer] = std::string(value);
    return true;
  }

  bool interacts(u32 a, u32 b) const {
    return a < kCount && b < kCount && (matrix_[a] & (1u << b)) != 0;
  }
  // Recíproco por construção: o Jolt consulta o par uma vez só, em ordem não
  // especificada, então uma matriz assimétrica faria a colisão depender da
  // ordem de criação dos corpos.
  bool setInteraction(u32 a, u32 b, bool value) {
    if (a >= kCount || b >= kCount) return false;
    if (value) { matrix_[a] |= 1u << b; matrix_[b] |= 1u << a; }
    else { matrix_[a] &= ~(1u << b); matrix_[b] &= ~(1u << a); }
    return true;
  }
  const u32 *matrix() const noexcept { return matrix_.data(); }
  // Máscara das camadas que interagem com `layer`; é o filtro natural de uma
  // consulta feita "do ponto de vista" de um objeto.
  u32 maskFor(u32 layer) const noexcept { return layer < kCount ? matrix_[layer] : 0xffffffffu; }

  u32 definedCount() const noexcept {
    u32 highest = 1;
    for (u32 layer = 0; layer < kCount; ++layer) if (!names_[layer].empty()) highest = layer + 1;
    return highest;
  }
  bool isDefault() const {
    GameplayLayers defaults;
    return *this == defaults;
  }
  friend bool operator==(const GameplayLayers &a, const GameplayLayers &b) {
    return a.names_ == b.names_ && a.matrix_ == b.matrix_;
  }

  void write(std::ostream &out) const {
    const u32 count = definedCount();
    out << ' ' << count;
    for (u32 layer = 0; layer < count; ++layer) out << ' ' << std::quoted(names_[layer]) << ' ' << matrix_[layer];
  }
  bool read(std::istream &in) {
    u32 count = 0;
    if (!(in >> count) || count == 0 || count > kCount) return false;
    GameplayLayers candidate;
    for (u32 layer = 0; layer < count; ++layer) {
      std::string value;
      u32 mask = 0;
      if (!(in >> std::quoted(value) >> mask)) return false;
      if (value.size() > kNameCapacity) return false;
      candidate.names_[layer] = std::move(value);
      candidate.matrix_[layer] = mask;
    }
    if (candidate.names_[0].empty()) return false;
    // Reciprocidade conferida na leitura: um arquivo editado à mão não pode
    // instalar uma matriz que o backend recusaria mais tarde, em silêncio.
    for (u32 a = 0; a < kCount; ++a)
      for (u32 b = 0; b < kCount; ++b)
        if (((candidate.matrix_[a] >> b) & 1u) != ((candidate.matrix_[b] >> a) & 1u)) return false;
    *this = std::move(candidate);
    return true;
  }

private:
  std::array<std::string, kCount> names_{};
  std::array<u32, kCount> matrix_{};
};

} // namespace ae::runtime
