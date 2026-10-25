#pragma once
#include "resources/material_asset.h"
#include <vector>

namespace ae::resources {
// Deliberately bounded reader for Unity's text Material asset, not a generic
// YAML loader or a shader translator. Unknown fields survive in the source.
struct UnityMaterialTexture {
  std::string property;
  AssetGuid unityGuid;
  u32 binding=0;
  scene::MaterialSampling sampling;
};
// G3a: auditoria por material. Completo: todo efeito do Standard tem
// consumidor Astra equivalente. Aproximado: converte, com diferença visível
// declarada em `approximations`. Sem conversão: `unsupported` recusa o material.
enum class UnityMaterialAudit : u8 { Complete, Approximated, Unsupported };
// G6a: o que um material de partícula do pacote passa ao Sistema de partículas
// que o usa, além do Material da Astra (imagem, cor e mistura): o brilho, a
// rolagem e o ruído vivem no componente (scene/particle_system.h, Material de
// partícula). Shaders Hovl Studio por GUID do .shader (RPG VFX Bundle 5.0;
// propriedades estudadas, nenhum código copiado).
enum class UnityParticleFamily : u8 { None, AlphaBlend, Additive, Distortion };
struct UnityParticleLook {
  UnityParticleFamily family = UnityParticleFamily::None;
  float emission = 1;
  float scroll[2]{}, noiseScroll[2]{};
  bool noise = false, centerGlow = false, soft = false;
  float opacity = 1;
  std::string shader;  // nome da família de origem, para o relatório
};
struct UnityMaterialImport {
  MaterialAsset material;
  std::vector<UnityMaterialTexture> textures;
  std::vector<std::string> unsupported;
  std::vector<std::string> approximations;
  UnityParticleLook particle;
  std::string diagnostic;
  bool supported() const {return diagnostic.empty() && unsupported.empty();}
  UnityMaterialAudit audit() const {
    return !supported()?UnityMaterialAudit::Unsupported:approximations.empty()?UnityMaterialAudit::Complete:UnityMaterialAudit::Approximated;
  }
};
// Prefixo das aproximações nas perdas conhecidas de um item da biblioteca: o
// relatório do pacote separa por ele o aproximado do sem conversão.
inline constexpr std::string_view UnityMaterialApproximationPrefix="Aproximação: ";
bool readUnityMaterial(std::string_view text,UnityMaterialImport &out);
} // namespace ae::resources
