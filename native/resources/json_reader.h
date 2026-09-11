#pragma once
#include "core/base.h"
#include <string>
#include <string_view>
#include <vector>

namespace ae::resources {

// Leitor de JSON para o caminho de IMPORTAÇÃO, não para o caminho de quadro.
//
// A regra da engine — "o runtime nunca interpreta JSON" — vale para o que roda
// em todo quadro: geometria e texturas chegam cozidas em formato binário. Uma
// importação é outra coisa: acontece uma vez, por ação explícita do usuário, e o
// arquivo que ele escolheu é um glTF, cujo cabeçalho é JSON por definição do
// formato. Interpretar aqui é o que permite importar no aparelho em vez de
// exigir um cozimento no computador.
//
// Falha fechada em tudo: profundidade, tamanho, número mal formado, string sem
// fechamento, lixo depois do valor raiz. Um documento parcialmente lido nunca é
// entregue.
class JsonDocument final {
public:
  enum class Kind : u8 { Null, Boolean, Number, String, Array, Object };
  struct Node {
    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0;
    // Índice no pool de texto: chave (em membro de objeto) e valor de string.
    u32 key = 0, text = 0;
    u32 childBegin = 0, childCount = 0;
  };
  static constexpr u32 MaximumDepth = 64;
  static constexpr u32 MaximumNodes = 1u << 21;

  static bool parse(std::string_view text, JsonDocument &out);

  const Node *root() const noexcept { return nodes_.empty() ? nullptr : &nodes_[0]; }
  const Node *child(const Node &parent, u32 index) const noexcept {
    return index < parent.childCount ? &nodes_[children_[parent.childBegin + index]] : nullptr;
  }
  std::string_view keyOf(const Node &node) const noexcept { return strings_[node.key]; }
  std::string_view textOf(const Node &node) const noexcept { return strings_[node.text]; }

  // Acesso por nome em objeto. Linear de propósito: os objetos de um glTF têm
  // poucas chaves, e um índice custaria mais memória do que a busca custa tempo.
  const Node *member(const Node &object, std::string_view name) const noexcept {
    if (object.kind != Kind::Object) return nullptr;
    for (u32 i = 0; i < object.childCount; ++i) {
      const auto &value = nodes_[children_[object.childBegin + i]];
      if (strings_[value.key] == name) return &value;
    }
    return nullptr;
  }
  // Conveniências que devolvem o padrão quando o campo não existe ou tem outro
  // tipo. Um glTF válido pode simplesmente omitir quase tudo.
  double number(const Node &object, std::string_view name, double fallback) const noexcept {
    const auto *value = member(object, name);
    return value && value->kind == Kind::Number ? value->number : fallback;
  }
  bool boolean(const Node &object, std::string_view name, bool fallback) const noexcept {
    const auto *value = member(object, name);
    return value && value->kind == Kind::Boolean ? value->boolean : fallback;
  }
  std::string_view string(const Node &object, std::string_view name) const noexcept {
    const auto *value = member(object, name);
    return value && value->kind == Kind::String ? strings_[value->text] : std::string_view{};
  }
  // Índice não negativo e inteiro, como todo índice de glTF. `-1` quando o campo
  // não existe: o formato usa ausência, nunca um índice negativo.
  i64 index(const Node &object, std::string_view name) const noexcept {
    const auto *value = member(object, name);
    if (!value || value->kind != Kind::Number) return -1;
    const auto whole = static_cast<i64>(value->number);
    return static_cast<double>(whole) == value->number && whole >= 0 ? whole : -1;
  }

private:
  struct Parser;
  std::vector<Node> nodes_;
  std::vector<u32> children_;
  std::vector<std::string> strings_;
};

} // namespace ae::resources
