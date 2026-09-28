#pragma once
#include "scene/components.h"

namespace ae::scene {
// sourceObject é identidade LOCAL ao recurso. instanceRoot pertence à cena e
// participa do mesmo remapeamento das referências de gameplay ao duplicar.
// A base acompanha a cena: salvar a fonte não apaga a comparação de uma cena
// que ainda contém a revisão anterior. O payload não contém este componente.
class PrefabLink final : public ComponentValue {
public:
  resources::AssetGuid asset;
  u32 sourceObject=0;
  u64 instanceRoot=0;
  std::string base;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<PrefabLink>(*this);}
  bool valid() const override {
    return asset.valid() && sourceObject>1 && sourceObject<=65536 && instanceRoot &&
      instanceRoot<=std::numeric_limits<u32>::max() && !base.empty() && base.size()<=256*1024;
  }
  void write(std::ostream &out) const override {
    out<<asset.text()<<' '<<sourceObject<<' '<<instanceRoot<<' '<<std::quoted(base);
  }
  bool read(std::istream &in,u32 version) override {
    std::string id;
    return version==1 && bool(in>>id>>sourceObject>>instanceRoot>>std::quoted(base)) &&
      resources::AssetGuid::parse(id,asset) && valid();
  }
};
inline const ComponentObjectReference prefabRootReference[]{
  {"instanceRoot","Raiz da instância",{},ObjectReferenceScope::SelfOrAncestor,"Nenhuma",
    [](const ComponentValue &v)->u64 {return static_cast<const PrefabLink&>(v).instanceRoot;},
    [](ComponentValue &v,u64 id) {static_cast<PrefabLink&>(v).instanceRoot=id;}}
};
inline const ComponentResourceBinding prefabSourceBinding[]{
  {"source","Fonte",resources::AssetType::Prefab,
    [](const ComponentValue &)->u32 {return 1;},
    [](const ComponentValue &v,u32)->resources::AssetGuid {return static_cast<const PrefabLink&>(v).asset;},
    nullptr}
};
inline const ComponentType PrefabLink::descriptor{
  "astra.prefab.link",1,[]()->std::unique_ptr<ComponentValue> {return std::make_unique<PrefabLink>();},
  {},{},{},nullptr,false,prefabRootReference,{},prefabSourceBinding
};
inline const PrefabLink *prefabLink(const Components &values) {
  return static_cast<const PrefabLink*>(values.find(PrefabLink::descriptor));
}
} // namespace ae::scene
