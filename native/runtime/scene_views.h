#pragma once
// Vistas salvas da cena: enquadramentos com nome, guardados COM o documento.
//
// Comparar uma mudança visual exige repetir o mesmo enquadramento. Reposicionar
// a câmera à mão entre duas medições muda o que está sendo comparado — o
// resultado passa a incluir a diferença de ângulo. A Unity não traz isso de
// fábrica (a prática é extensão de editor: "Set Bookmark" / "Move to Bookmark"
// numa sobreposição da Scene view); aqui a vista é dado autoral da cena, viaja
// no arquivo e vale para qualquer projeto.
//
// O que uma vista guarda é a câmera de ÓRBITA do editor (alvo, distância e
// ângulos) mais a lente, e não a posição derivada: guardar posição e alvo
// separados deixaria os dois divergirem, e voltar à vista passaria a orbitar em
// torno de um ponto que não é o que está na tela.
#include "core/base.h"

#include <algorithm>
#include <iomanip>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace ae::runtime {

struct SceneView final {
  std::string name;
  float target[3]{0, 0, 0};
  float distance = 12;
  float yaw = 0;
  float pitch = 0;
  // Campo de visão vertical em radianos, o mesmo vocabulário da câmera de cena
  // da Unity (Field of View). Zero significa "a lente atual do editor".
  float verticalFov = 0;
  friend bool operator==(const SceneView &a, const SceneView &b) {
    return a.name == b.name && a.distance == b.distance && a.yaw == b.yaw && a.pitch == b.pitch &&
           a.verticalFov == b.verticalFov && std::equal(a.target, a.target + 3, b.target);
  }
};

class SceneViews final {
public:
  static constexpr u32 kMaximum = 64;
  static constexpr u32 kNameCapacity = 48;

  const std::vector<SceneView> &all() const noexcept { return views_; }
  u32 count() const noexcept { return static_cast<u32>(views_.size()); }
  const SceneView *find(std::string_view name) const {
    for (const auto &view : views_) if (view.name == name) return &view;
    return nullptr;
  }
  const SceneView *at(u32 index) const { return index < views_.size() ? &views_[index] : nullptr; }

  // Nome vazio, repetido ou com aspas/quebra de linha é recusado: a lista é
  // endereçada pelo nome e o arquivo guarda o nome entre aspas.
  static bool validName(std::string_view name) {
    return !name.empty() && name.size() <= kNameCapacity && name.find('"') == std::string_view::npos &&
           name.find('\n') == std::string_view::npos;
  }
  bool add(const SceneView &view) {
    if (!validName(view.name) || !valid(view) || views_.size() >= kMaximum || find(view.name)) return false;
    views_.push_back(view);
    return true;
  }
  // Grava por cima da vista de mesmo nome: é o "atualizar com a vista atual".
  bool replace(const SceneView &view) {
    if (!validName(view.name) || !valid(view)) return false;
    for (auto &existing : views_)
      if (existing.name == view.name) { existing = view; return true; }
    return add(view);
  }
  bool rename(u32 index, std::string_view name) {
    if (index >= views_.size() || !validName(name)) return false;
    for (u32 other = 0; other < views_.size(); ++other)
      if (other != index && views_[other].name == name) return false;
    views_[index].name = std::string(name);
    return true;
  }
  bool remove(u32 index) {
    if (index >= views_.size()) return false;
    views_.erase(views_.begin() + index);
    return true;
  }
  void clear() { views_.clear(); }
  bool empty() const noexcept { return views_.empty(); }

  friend bool operator==(const SceneViews &a, const SceneViews &b) { return a.views_ == b.views_; }

  void write(std::ostream &out) const {
    out << ' ' << views_.size();
    for (const auto &view : views_) {
      out << ' ' << std::quoted(view.name);
      for (const float axis : view.target) out << ' ' << axis;
      out << ' ' << view.distance << ' ' << view.yaw << ' ' << view.pitch << ' ' << view.verticalFov;
    }
  }
  bool read(std::istream &in) {
    u32 count = 0;
    if (!(in >> count) || count > kMaximum) return false;
    SceneViews candidate;
    for (u32 i = 0; i < count; ++i) {
      SceneView view;
      if (!(in >> std::quoted(view.name))) return false;
      for (float &axis : view.target) if (!(in >> axis)) return false;
      if (!(in >> view.distance >> view.yaw >> view.pitch >> view.verticalFov)) return false;
      // Vista ilegível recusa o arquivo inteiro: aceitar o resto perderia um
      // enquadramento em silêncio, e é justamente ele que dá a comparação.
      if (!candidate.add(view)) return false;
    }
    *this = std::move(candidate);
    return true;
  }

private:
  static bool valid(const SceneView &view) {
    const auto finite = [](float value) { return value == value && value > -1e30f && value < 1e30f; };
    return finite(view.target[0]) && finite(view.target[1]) && finite(view.target[2]) && finite(view.distance) &&
           view.distance > 0 && finite(view.yaw) && finite(view.pitch) && finite(view.verticalFov) &&
           view.verticalFov >= 0 && view.verticalFov < 3.14159f;
  }

  std::vector<SceneView> views_;
};

} // namespace ae::runtime
