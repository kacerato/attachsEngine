#pragma once
#include "runtime/scene_graph.h"
#include "resources/asset_registry.h"

namespace ae::runtime {
// Recurso portátil sem dependência de EditorDocument, UI ou filesystem.
// O grafo contém uma raiz técnica (1) e exatamente uma subárvore instanciável.
// Ler/capturar só substitui o recurso quando toda a validação passou.
class Prefab final {
public:
  static constexpr usize MaximumBytes=32*1024*1024;
  using Registry=std::span<const scene::ComponentType *const>;
  resources::AssetGuid asset() const {return asset_;}
  ObjectId root() const {return root_;}
  const SceneGraph &graph() const {return graph_;}
  bool capture(const SceneGraph &source,ObjectId root,resources::AssetGuid asset,std::string &error);
  bool read(std::string_view text,Registry registry,std::string &error);
  std::string write() const;
  ObjectId instantiate(SceneGraph &destination,ObjectId parent,ObjectCloneMap &mapping,std::string &error) const;
private:
  SceneGraph graph_;
  ObjectId root_=0;
  resources::AssetGuid asset_;
};

// Canonical authoring snapshot used by the instance baseline. Stable component
// instance IDs are retained; scene-specific prefab ownership is excluded.
std::string serializePrefabObject(const SceneObject &object);
bool deserializePrefabObject(std::istream &in,Prefab::Registry registry,SceneObject &object);
} // namespace ae::runtime
