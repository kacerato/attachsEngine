#pragma once
#include "resources/asset_registry.h"
#include "scene/components.h"
#include <cmath>
#include <iomanip>
#include <string>

namespace ae::scene {
// Vínculo de um objeto da cena com o nó da fonte importada que o originou
// (M08.2).
//
// Guarda a BASE: os valores que a fonte tinha na revisão com que este objeto
// foi reconciliado pela última vez. É ela que separa "o usuário mexeu" de "a
// fonte mudou": sem base, uma reimportação só pode escolher entre apagar a
// edição local e ignorar a fonte nova. Com base, cada campo tem três lados, e
// só o que o usuário não tocou recebe o valor novo.
//
// A base mora no próprio objeto, e não só no mapa da fonte, porque cena e
// registro são salvos em momentos diferentes. Uma cena gravada antes de uma
// reimportação continua sabendo contra o que foi comparada.
//
// `instance` agrupa os objetos de uma mesma instanciação; `root` marca o objeto
// que a representa (o grupo, ou a raiz única do arquivo). `primitive` >= 0 é o
// filho por primitiva que a Astra ainda cria para nós com várias malhas.
class ImportLink final : public ComponentValue {
public:
  resources::AssetGuid source, node, instance;
  i32 primitive = -1;
  u32 revision = 0;
  bool root = false;
  // A fonte não tem mais este nó e o objeto guardava dados locais: ficou na
  // cena, desligado da fonte, esperando o usuário decidir.
  bool orphan = false;
  // O usuário desvinculou: o objeto é independente. A marca fica, em vez de o
  // componente sumir, para que a adoção de objetos legados nunca religue o que
  // foi desligado de propósito.
  bool unlinked = false;
  std::string baseName;
  float basePosition[3]{0, 0, 0}, baseRotation[3]{0, 0, 0}, baseScale[3]{1, 1, 1};
  resources::AssetGuid baseParent, baseAsset;
  u32 basePrimitives = 0;

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<ImportLink>(*this); }
  bool valid() const override {
    if (!source.valid() || !instance.valid() || (!root && !node.valid()) || primitive < -1 || primitive > 65535 ||
        baseName.size() >= 256 || baseName.find('\0') != std::string::npos || basePrimitives > 65536)
      return false;
    for (u32 axis = 0; axis < 3; ++axis)
      if (!std::isfinite(basePosition[axis]) || !std::isfinite(baseRotation[axis]) || !std::isfinite(baseScale[axis]))
        return false;
    return true;
  }
  void write(std::ostream &out) const override {
    const auto guid = [](const resources::AssetGuid &value) { return value.valid() ? value.text() : std::string("-"); };
    out << guid(source) << ' ' << guid(node) << ' ' << guid(instance) << ' ' << primitive << ' ' << revision << ' '
        << root << ' ' << orphan << ' ' << std::quoted(baseName);
    for (float value : basePosition) out << ' ' << value;
    for (float value : baseRotation) out << ' ' << value;
    for (float value : baseScale) out << ' ' << value;
    out << ' ' << guid(baseParent) << ' ' << guid(baseAsset) << ' ' << basePrimitives << ' ' << unlinked;
  }
  bool read(std::istream &in, u32 version) override {
    if (version != 1) return false;
    std::string sourceText, nodeText, instanceText, parentText, assetText;
    if (!(in >> sourceText >> nodeText >> instanceText >> primitive >> revision >> root >> orphan >> std::quoted(baseName)))
      return false;
    for (float &value : basePosition) if (!(in >> value)) return false;
    for (float &value : baseRotation) if (!(in >> value)) return false;
    for (float &value : baseScale) if (!(in >> value)) return false;
    if (!(in >> parentText >> assetText >> basePrimitives >> unlinked)) return false;
    const auto parse = [](const std::string &text, resources::AssetGuid &out) {
      out = {};
      return text == "-" || resources::AssetGuid::parse(text, out);
    };
    return parse(sourceText, source) && parse(nodeText, node) && parse(instanceText, instance) &&
           parse(parentText, baseParent) && parse(assetText, baseAsset);
  }
};

inline const ComponentType ImportLink::descriptor{
    "astra.import.link", 1, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<ImportLink>(); }};

inline const ImportLink *importLink(const Components &components) {
  return static_cast<const ImportLink *>(components.find(ImportLink::descriptor));
}
inline ImportLink *editImportLink(Components &components) {
  return static_cast<ImportLink *>(components.edit(ImportLink::descriptor));
}
} // namespace ae::scene
