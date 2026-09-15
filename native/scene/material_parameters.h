#pragma once
#include <cmath>
#include <cstdint>
namespace ae::scene {
// Bindings de textura de um material, na ordem do pacote de mapa e do shader:
// cor base, normal, metálico/rugosidade, emissão.
inline constexpr std::uint32_t MaterialTextureCount=4;
// Valor RESOLVIDO de um binding que não foi trocado: vale a textura do material
// da fonte. `renderer::InvalidMapTexture` (0xFFFFFFFF) é "sem textura".
inline constexpr std::uint32_t MaterialTextureKeep=0xFFFFFFFEu;

// R4: modo de alfa e faces. `Keep` herda (do material compartilhado, senão da fonte).
inline constexpr std::uint8_t MaterialAlphaKeep=0,MaterialAlphaOpaque=1,MaterialAlphaMask=2,MaterialAlphaBlend=3;
inline constexpr std::uint8_t MaterialSidesKeep=0,MaterialSidesSingle=1,MaterialSidesDouble=2;

// Superfície trocada num slot ou num MaterialAsset: modo de alfa, corte (só vale
// no modo recorte) e faces. É dado persistido, por identidade de valor.
struct MaterialSurface {
  std::uint8_t alphaMode=MaterialAlphaKeep;
  std::uint8_t sides=MaterialSidesKeep;
  float alphaCutoff=.5f;
  bool overrides() const noexcept {return alphaMode!=MaterialAlphaKeep || sides!=MaterialSidesKeep;}
  friend bool operator==(const MaterialSurface &a,const MaterialSurface &b) {
    return a.alphaMode==b.alphaMode && a.sides==b.sides && a.alphaCutoff==b.alphaCutoff;
  }
};
inline bool validMaterialSurface(const MaterialSurface &surface) {
  return surface.alphaMode<=MaterialAlphaBlend && surface.sides<=MaterialSidesDouble &&
         std::isfinite(surface.alphaCutoff) && surface.alphaCutoff>=0 && surface.alphaCutoff<=1;
}

// R4: amostragem de um binding. `Keep` herda (do material compartilhado, senão da
// fonte). O conjunto de UV vale para qualquer textura do binding; repetição e
// filtro são o sampler da textura publicada e só valem para textura do PROJETO
// (a da fonte já sobe com o sampler que o arquivo pediu).
inline constexpr std::uint8_t MaterialUvKeep=0,MaterialUv0=1,MaterialUv1=2;
inline constexpr std::uint8_t MaterialWrapKeep=0,MaterialWrapRepeat=1,MaterialWrapClamp=2,MaterialWrapMirror=3;
inline constexpr std::uint8_t MaterialFilterKeep=0,MaterialFilterLinear=1,MaterialFilterNearest=2;
struct MaterialSampling {
  std::uint8_t uvSet=MaterialUvKeep;
  std::uint8_t wrap=MaterialWrapKeep;
  std::uint8_t filter=MaterialFilterKeep;
  bool overrides() const noexcept {return uvSet!=MaterialUvKeep || wrap!=MaterialWrapKeep || filter!=MaterialFilterKeep;}
  friend bool operator==(const MaterialSampling &a,const MaterialSampling &b) {
    return a.uvSet==b.uvSet && a.wrap==b.wrap && a.filter==b.filter;
  }
};
inline bool validMaterialSampling(const MaterialSampling &sampling) {
  return sampling.uvSet<=MaterialUv1 && sampling.wrap<=MaterialWrapMirror && sampling.filter<=MaterialFilterNearest;
}

// Authored scalar overrides. Resource textures and pipeline selection remain
// with the referenced material; this value has no renderer dependency.
//
// R4: `textures`, `alphaMode`, `sides` e `alphaCutoff` carregam o estado
// RESOLVIDO para publicação (índice da biblioteca publicada, modo efetivo). Nunca
// são persistidos aqui: as identidades e escolhas moram no componente de malha
// (instância) e no MaterialAsset (compartilhado), e são resolvidas a cada extração.
struct MaterialParameters {
  bool enabled=false;
  float baseColor[3]{1,1,1};
  float roughness=.5f,metallic=0,normalScale=1,specular=1;
  float emission[3]{};
  float emissionStrength=1;
  std::uint32_t textures[MaterialTextureCount]{MaterialTextureKeep,MaterialTextureKeep,MaterialTextureKeep,MaterialTextureKeep};
  std::uint8_t alphaMode=MaterialAlphaKeep,sides=MaterialSidesKeep;
  float alphaCutoff=.5f;
  // Conjunto de UV resolvido por binding (MaterialUvKeep, MaterialUv0, MaterialUv1).
  std::uint8_t uvSets[MaterialTextureCount]{};
  // Comparação por valor, não por bytes: a struct tem padding depois do bool, e
  // um memcmp acusaria diferença onde não existe nenhuma.
  friend bool operator==(const MaterialParameters &a,const MaterialParameters &b) {
    if(a.enabled!=b.enabled || a.roughness!=b.roughness || a.metallic!=b.metallic ||
       a.normalScale!=b.normalScale || a.specular!=b.specular || a.emissionStrength!=b.emissionStrength) return false;
    for(int i=0;i<3;++i) if(a.baseColor[i]!=b.baseColor[i] || a.emission[i]!=b.emission[i]) return false;
    for(std::uint32_t i=0;i<MaterialTextureCount;++i) if(a.textures[i]!=b.textures[i] || a.uvSets[i]!=b.uvSets[i]) return false;
    return a.alphaMode==b.alphaMode && a.sides==b.sides && a.alphaCutoff==b.alphaCutoff;
  }
  friend bool operator!=(const MaterialParameters &a,const MaterialParameters &b) {return !(a==b);}
};
// Os valores escalares sem o estado resolvido de publicação (texturas, alfa,
// faces): é o que se guarda num componente ou num MaterialAsset.
inline MaterialParameters withoutResolvedTextures(MaterialParameters value) {
  for(auto &texture:value.textures) texture=MaterialTextureKeep;
  for(auto &set:value.uvSets) set=MaterialUvKeep;
  value.alphaMode=MaterialAlphaKeep;value.sides=MaterialSidesKeep;value.alphaCutoff=.5f;
  return value;
}
}
