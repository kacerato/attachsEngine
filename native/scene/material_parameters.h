#pragma once
#include <cstdint>
namespace ae::scene {
// Bindings de textura de um material, na ordem do pacote de mapa e do shader:
// cor base, normal, metálico/rugosidade, emissão.
inline constexpr std::uint32_t MaterialTextureCount=4;
// Valor RESOLVIDO de um binding que não foi trocado: vale a textura do material
// da fonte. `renderer::InvalidMapTexture` (0xFFFFFFFF) é "sem textura".
inline constexpr std::uint32_t MaterialTextureKeep=0xFFFFFFFEu;

// Authored scalar overrides. Resource textures and pipeline selection remain
// with the referenced material; this value has no renderer dependency.
//
// R4: `textures` carrega o índice da biblioteca PUBLICADA para cada binding. É
// estado de publicação, nunca persistido aqui: as identidades de textura moram
// no componente de malha (instância) e no MaterialAsset (compartilhado), e são
// resolvidas para índices a cada extração da cena.
struct MaterialParameters {
  bool enabled=false;
  float baseColor[3]{1,1,1};
  float roughness=.5f,metallic=0,normalScale=1,specular=1;
  float emission[3]{};
  float emissionStrength=1;
  std::uint32_t textures[MaterialTextureCount]{MaterialTextureKeep,MaterialTextureKeep,MaterialTextureKeep,MaterialTextureKeep};
  // Comparação por valor, não por bytes: a struct tem padding depois do bool, e
  // um memcmp acusaria diferença onde não existe nenhuma.
  friend bool operator==(const MaterialParameters &a,const MaterialParameters &b) {
    if(a.enabled!=b.enabled || a.roughness!=b.roughness || a.metallic!=b.metallic ||
       a.normalScale!=b.normalScale || a.specular!=b.specular || a.emissionStrength!=b.emissionStrength) return false;
    for(int i=0;i<3;++i) if(a.baseColor[i]!=b.baseColor[i] || a.emission[i]!=b.emission[i]) return false;
    for(std::uint32_t i=0;i<MaterialTextureCount;++i) if(a.textures[i]!=b.textures[i]) return false;
    return true;
  }
  friend bool operator!=(const MaterialParameters &a,const MaterialParameters &b) {return !(a==b);}
};
// Os valores escalares sem o estado de publicação das texturas: é o que se
// guarda num componente ou num MaterialAsset.
inline MaterialParameters withoutResolvedTextures(MaterialParameters value) {
  for(auto &texture:value.textures) texture=MaterialTextureKeep;
  return value;
}
}
