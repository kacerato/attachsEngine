#include "scene/collision_recipe.h"

namespace ae::scene {
namespace detail {
inline const auto collisionSourceNames = [] {
  std::array<std::string, 128> names;
  for (usize i = 0; i < names.size(); ++i)
    names[i] = "source_" + std::to_string(i);
  return names;
}();
template <usize I> ComponentObjectReference collisionSourceReference() {
  return {collisionSourceNames[I],
          "Fonte da colisão",
          "",
          ObjectReferenceScope::Any,
          "Fonte ausente",
          [](const ComponentValue &v) -> u64 {
            const auto &r = static_cast<const CollisionRecipe &>(v);
            return I < r.sources.size() ? r.sources[I].object : 0;
          },
          [](ComponentValue &v, u64 id) {
            auto &r = static_cast<CollisionRecipe &>(v);
            if (I < r.sources.size())
              r.sources[I].object = id;
          },
          {"Fontes", "",
           "Objeto/slot persistentes; regeneração é uma ação explícita",
           [](const ComponentValue &v) {
             return I < static_cast<const CollisionRecipe &>(v).sources.size();
           },
           [](const ComponentValue &) { return false; }}};
}
template <usize... I>
auto collisionSourceReferences(std::index_sequence<I...>) {
  return std::array<ComponentObjectReference, sizeof...(I)>{
      collisionSourceReference<I>()...};
}
inline const auto collisionRecipeReferences =
    collisionSourceReferences(std::make_index_sequence<128>{});
inline const std::array<ComponentResourceBinding, 2> collisionRecipeResources{
    {{"bake_source",
      "Revisão gerada",
      resources::AssetType::Mesh,
      [](const ComponentValue &v) -> u32 {
        return static_cast<const CollisionRecipe &>(v).bakeSource.valid() ? 1
                                                                          : 0;
      },
      [](const ComponentValue &v, u32) {
        return static_cast<const CollisionRecipe &>(v).bakeSource;
      },
      [](ComponentValue &v, u32 slot, resources::AssetGuid guid) {
        if (slot)
          return false;
        static_cast<CollisionRecipe &>(v).bakeSource = guid;
        return true;
      },
      {"Revisão", "", "Recurso imutável da geração", nullptr,
       [](const ComponentValue &) { return false; }}},
     {"baseline_mesh",
      "Malha base da parte",
      resources::AssetType::Mesh,
      [](const ComponentValue &v) -> u32 {
        return static_cast<u32>(
            static_cast<const CollisionRecipe &>(v).parts.size());
      },
      [](const ComponentValue &v, u32 slot) {
        return static_cast<const CollisionRecipe &>(v)
            .parts[slot]
            .baseline.collisionMesh;
      },
      [](ComponentValue &v, u32 slot, resources::AssetGuid guid) {
        auto &r = static_cast<CollisionRecipe &>(v);
        if (slot >= r.parts.size())
          return false;
        r.parts[slot].baseline.collisionMesh = guid;
        return true;
      },
      {"Revisão", "", "Base de comparação; o override pertence ao Colisor",
       nullptr, [](const ComponentValue &) { return false; }}}}};
} // namespace detail
const ComponentType CollisionRecipe::descriptor{
    "astra.physics.collision_recipe",
    2,
    []() -> std::unique_ptr<ComponentValue> {
      return std::make_unique<CollisionRecipe>();
    },
    {},
    {},
    {},
    nullptr,
    false,
    detail::collisionRecipeReferences,
    {},
    detail::collisionRecipeResources};
} // namespace ae::scene
