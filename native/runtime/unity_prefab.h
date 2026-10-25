#pragma once
#include "runtime/prefab.h"
#include "resources/unity_material_import.h"
#include "resources/unity_prefab_import.h"
#include <functional>

namespace ae::runtime {
struct UnityPrefabIdentity {i64 fileId=0;ObjectId object=0;};
struct UnityPrefabIdentityMap {
  ObjectId nextObject=2;
  std::vector<UnityPrefabIdentity> entries;
};
// Mesh reference resolves to its native submeshes, in source material order;
// material references resolve to exactly one shared material identity.
// G6a: Prefab (outro prefab do pacote, por GUID) e AudioClip (WAV) resolvem
// para uma identidade cada.
using UnityPrefabResolver=std::function<bool(const resources::UnityObjectReference &,resources::AssetType,
    std::vector<resources::AssetGuid> &,std::string &)>;
// G6a: dependências dos componentes de efeito. Uma que falta não recusa o
// prefab: o componente fica sem ela e a perda entra em `notes`.
struct UnityPrefabEffectServices {
  // O que o material de partícula do pacote passa ao componente (brilho,
  // rolagem, ruído); falso quando o material não é de partícula conhecido.
  std::function<bool(const resources::UnityObjectReference &,resources::UnityParticleLook &)> particleLook;
  // Malha do Render Mode Mesh: pode converter a fonte (FBX) na hora.
  UnityPrefabResolver particleMesh;
  std::vector<std::string> *notes=nullptr;
};
bool buildUnityPrefab(const resources::UnityPrefabImport &,resources::AssetGuid guid,const UnityPrefabResolver &,
    const UnityPrefabIdentityMap *previous,Prefab &out,UnityPrefabIdentityMap &identities,std::string &diagnostic,
    const UnityPrefabEffectServices *effects=nullptr);
} // namespace ae::runtime
