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
// R4: a textura de oclusão própria entra nos mesmos caminhos de seleção e
// publicação como um binding extra, depois dos quatro do pacote. Ela usa o
// conjunto de UV e a amostragem do mapa metal/rugosidade.
inline constexpr std::uint32_t MaterialOcclusionTextureBinding=MaterialTextureCount;

// R4: modo de alfa e faces. `Keep` herda (do material compartilhado, senão da fonte).
inline constexpr std::uint8_t MaterialAlphaKeep=0,MaterialAlphaOpaque=1,MaterialAlphaMask=2,MaterialAlphaBlend=3;
// PK-9: Render Face da Unity é Front (uma face), Back (só a de trás) e Both.
inline constexpr std::uint8_t MaterialSidesKeep=0,MaterialSidesSingle=1,MaterialSidesDouble=2,MaterialSidesBack=3;

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
  return surface.alphaMode<=MaterialAlphaBlend && surface.sides<=MaterialSidesBack &&
         std::isfinite(surface.alphaCutoff) && surface.alphaCutoff>=0 && surface.alphaCutoff<=1;
}

// R4: canais do mapa metal/rugosidade e da oclusão, convenção do normal e origem
// do alfa (T04/T06/T20/T21). `Keep` herda do material compartilhado, senão da fonte.
inline constexpr std::uint8_t MaterialChannelKeep=0,MaterialChannelR=1,MaterialChannelG=2,MaterialChannelB=3,MaterialChannelA=4;
inline constexpr std::uint8_t MaterialOcclusionKeep=0,MaterialOcclusionNone=1,MaterialOcclusionPacked=2,MaterialOcclusionTexture=3;
inline constexpr std::uint8_t MaterialToggleKeep=0,MaterialToggleOff=1,MaterialToggleOn=2;
inline constexpr std::uint8_t MaterialAlphaSourceKeep=0,MaterialAlphaSourceBase=1,MaterialAlphaSourceOpaque=2,MaterialAlphaSourceLuminance=3;
// G3a: de onde vem a rugosidade. `Map` lê o canal escolhido como rugosidade
// (glTF). As duas de suavidade seguem o Standard da Unity (smoothness = 1 -
// rugosidade): o canal do mapa metal/rugosidade ou o alfa da textura de cor
// base. Na suavidade, o fator escalar continua sendo rugosidade e vale como
// 1 - escala da suavidade: rugosidade = 1 - (1 - fator) * canal. Sem textura o
// canal vale 1 e a rugosidade é o próprio fator nos três modos.
inline constexpr std::uint8_t MaterialRoughnessKeep=0,MaterialRoughnessMap=1,MaterialRoughnessSmoothnessMap=2,
                              MaterialRoughnessSmoothnessBaseAlpha=3;
// Isolar na prévia do editor: transitório, nunca persistido nem usado em Play.
inline constexpr std::uint8_t MaterialIsolateNone=0,MaterialIsolateOcclusion=1,MaterialIsolateRoughness=2,MaterialIsolateMetallic=3,
                              MaterialIsolateAlpha=4,MaterialIsolateNormal=5;
struct MaterialChannels {
  std::uint8_t roughness=MaterialChannelKeep,metallic=MaterialChannelKeep,occlusion=MaterialChannelKeep;
  std::uint8_t occlusionSource=MaterialOcclusionKeep;
  float occlusionStrength=-1; // -1 herda; senão 0..1
  std::uint8_t normalFlipY=MaterialToggleKeep;
  std::uint8_t alphaSource=MaterialAlphaSourceKeep;
  std::uint8_t roughnessSource=MaterialRoughnessKeep;
  bool overrides() const noexcept {
    return roughness || metallic || occlusion || occlusionSource || occlusionStrength>=0 || normalFlipY || alphaSource || roughnessSource;
  }
  friend bool operator==(const MaterialChannels &a,const MaterialChannels &b) {
    return a.roughness==b.roughness && a.metallic==b.metallic && a.occlusion==b.occlusion && a.occlusionSource==b.occlusionSource &&
           a.occlusionStrength==b.occlusionStrength && a.normalFlipY==b.normalFlipY && a.alphaSource==b.alphaSource &&
           a.roughnessSource==b.roughnessSource;
  }
};
inline bool validMaterialChannels(const MaterialChannels &channels) {
  return channels.roughness<=MaterialChannelA && channels.metallic<=MaterialChannelA && channels.occlusion<=MaterialChannelA &&
         channels.occlusionSource<=MaterialOcclusionTexture && channels.normalFlipY<=MaterialToggleOn &&
         channels.alphaSource<=MaterialAlphaSourceLuminance && channels.roughnessSource<=MaterialRoughnessSmoothnessBaseAlpha &&
         std::isfinite(channels.occlusionStrength) &&
         (channels.occlusionStrength==-1 || (channels.occlusionStrength>=0 && channels.occlusionStrength<=1));
}

// R4: amostragem de um binding. `Keep` herda (do material compartilhado, senão da
// fonte). O conjunto de UV vale para qualquer textura do binding; repetição e
// filtro são o sampler da textura publicada e só valem para textura do PROJETO
// (a da fonte já sobe com o sampler que o arquivo pediu).
// Mundo projeta a textura em metros sobre o eixo dominante da superfície.
// Serve para arquitetura escalada; modelos com UV autoral continuam em Keep.
inline constexpr std::uint8_t MaterialUvKeep=0,MaterialUv0=1,MaterialUv1=2,MaterialUvWorld=3;
inline constexpr std::uint8_t MaterialWrapKeep=0,MaterialWrapRepeat=1,MaterialWrapClamp=2,MaterialWrapMirror=3;
inline constexpr std::uint8_t MaterialFilterKeep=0,MaterialFilterLinear=1,MaterialFilterNearest=2;
struct MaterialSampling {
  std::uint8_t uvSet=MaterialUvKeep;
  std::uint8_t wrap=MaterialWrapKeep;
  std::uint8_t filter=MaterialFilterKeep;
  // Transformação de UV do binding (KHR_texture_transform): deslocamento, escala e
  // rotação em graus, aplicada como T·R·S. A identidade herda. Diferente de
  // repetição e filtro, vale para qualquer textura: é o shader que a aplica.
  float offset[2]{0,0};
  float scale[2]{1,1};
  float rotation=0;
  bool transformed() const noexcept {
    return offset[0]!=0 || offset[1]!=0 || scale[0]!=1 || scale[1]!=1 || rotation!=0;
  }
  bool overrides() const noexcept {
    return uvSet!=MaterialUvKeep || wrap!=MaterialWrapKeep || filter!=MaterialFilterKeep || transformed();
  }
  friend bool operator==(const MaterialSampling &a,const MaterialSampling &b) {
    return a.uvSet==b.uvSet && a.wrap==b.wrap && a.filter==b.filter && a.offset[0]==b.offset[0] && a.offset[1]==b.offset[1] &&
           a.scale[0]==b.scale[0] && a.scale[1]==b.scale[1] && a.rotation==b.rotation;
  }
};
inline bool validMaterialSampling(const MaterialSampling &sampling) {
  const auto within=[](float value,float limit) {return std::isfinite(value) && value>=-limit && value<=limit;};
  return sampling.uvSet<=MaterialUvWorld && sampling.wrap<=MaterialWrapMirror && sampling.filter<=MaterialFilterNearest &&
         within(sampling.offset[0],100) && within(sampling.offset[1],100) && within(sampling.rotation,360) &&
         within(sampling.scale[0],100) && within(sampling.scale[1],100) && sampling.scale[0]>=.01f && sampling.scale[1]>=.01f;
}
// Linhas 2x3 da transformação (a b c / d e f): uv' = (a·u + b·v + c, d·u + e·v + f).
inline void materialUvTransformRows(const MaterialSampling &sampling,float out[6]) {
  const double radians=static_cast<double>(sampling.rotation)*3.14159265358979323846/180.0;
  const float c=static_cast<float>(std::cos(radians)),s=static_cast<float>(std::sin(radians));
  out[0]=c*sampling.scale[0];out[1]=s*sampling.scale[1];out[2]=sampling.offset[0];
  out[3]=-s*sampling.scale[0];out[4]=c*sampling.scale[1];out[5]=sampling.offset[1];
}

// PK-9: o Shader do Material da Unity 6/URP e as opções que dependem dele.
// Mora só no MaterialAsset (como na Unity: keyword e shader são do Material;
// a instância troca escalares, não o shader). O padrão (Lit, Metallic, Alpha,
// sem detalhe, altura nem clear coat) é exatamente o material de antes.
//
//  - Lit: PBR metal/suavidade (o atual). Complex Lit: Lit + Clear Coat.
//  - Simple Lit: Blinn-Phong normalizado (difuso Lambert, brilho com cor
//    especular e expoente da suavidade), sem reflexo do ambiente.
//  - Unlit: cor base × textura (× cor do vértice), sem luz.
//  - Particles Lit/Unlit: Lit/Unlit mais o modo de cor do vértice e o
//    esmaecimento perto da câmera; é o Material que o PK-10 liga às partículas.
inline constexpr std::uint8_t MaterialShaderLit=0,MaterialShaderComplexLit=1,MaterialShaderSimpleLit=2,MaterialShaderUnlit=3,
                              MaterialShaderParticlesLit=4,MaterialShaderParticlesUnlit=5,MaterialShaderCount=6;
// Workflow Mode: Metallic usa o fator/mapa metálico; Specular usa a cor
// especular (e o RGB do binding 2 como Specular Map, A como suavidade).
inline constexpr std::uint8_t MaterialWorkflowMetallic=0,MaterialWorkflowSpecular=1;
// Blending Mode do Surface Type Transparent.
inline constexpr std::uint8_t MaterialBlendAlpha=0,MaterialBlendPremultiply=1,MaterialBlendAdditive=2,MaterialBlendMultiply=3;
// Color Mode dos shaders de partícula: como a cor do vértice entra na cor base.
inline constexpr std::uint8_t MaterialParticleColorMultiply=0,MaterialParticleColorAdditive=1,MaterialParticleColorSubtract=2,
                              MaterialParticleColorOverlay=3,MaterialParticleColorColor=4,MaterialParticleColorDifference=5;
// Texturas além dos bindings do pacote, por identidade, na ordem da extensão.
inline constexpr std::uint32_t MaterialExtraHeight=0,MaterialExtraDetailMask=1,MaterialExtraDetailAlbedo=2,MaterialExtraDetailNormal=3,
                               MaterialExtraClearCoat=4,MaterialExtraTextureCount=5;
// Classe de publicação de cada textura extra (o binding cuja cor/dado ela
// segue): altura, máscara e clear coat são dado; detalhe de cor é cor; o
// normal de detalhe sobe como normal.
// No Inspector e no seletor de textura as extras seguem a oclusão própria.
inline constexpr std::uint32_t MaterialExtraTextureBindingBase=MaterialOcclusionTextureBinding+1;
inline constexpr std::uint32_t materialExtraPublicationBinding(std::uint32_t extra) {
  return extra==MaterialExtraDetailAlbedo?0u:extra==MaterialExtraDetailNormal?1u:2u;
}
inline constexpr bool materialShaderHasLighting(std::uint8_t shader) {
  return shader!=MaterialShaderUnlit && shader!=MaterialShaderParticlesUnlit;
}
inline constexpr bool materialShaderIsParticle(std::uint8_t shader) {
  return shader==MaterialShaderParticlesLit || shader==MaterialShaderParticlesUnlit;
}
// Lit e Complex Lit leem detalhe, altura e o workflow; Particles Lit e Simple
// Lit, só o workflow especular; Unlit nada disso (URP 17).
inline constexpr bool materialShaderHasSurfaceDetail(std::uint8_t shader) {
  return shader==MaterialShaderLit || shader==MaterialShaderComplexLit;
}
struct MaterialShading {
  std::uint8_t shader=MaterialShaderLit;
  std::uint8_t workflow=MaterialWorkflowMetallic;
  std::uint8_t blend=MaterialBlendAlpha;
  // Preserve Specular Lighting (Transparent com Alpha ou Additive): o brilho
  // não é multiplicado pelo alfa.
  bool preserveSpecular=true;
  // Alpha Clipping também no Transparent (no Opaque o recorte é o modo Mask).
  bool transparentClip=false;
  // Advanced Options: tirar o brilho das luzes diretas e o reflexo do ambiente.
  bool specularHighlights=true,environmentReflections=true;
  // Sorting Priority (-50..50): fila Transparent 3000 + prioridade, depois distância.
  std::int8_t sortingPriority=0;
  std::uint8_t particleColorMode=MaterialParticleColorMultiply;
  bool cameraFading=false;
  float cameraFadeNear=1,cameraFadeFar=2;
  // Specular Color (linear) do workflow Specular.
  float specularColor[3]{.2f,.2f,.2f};
  // Height Map: amplitude da paralaxe (URP: 0..0,1, padrão 0,005), canal G.
  float heightScale=.005f;
  // Detail Inputs: Base Map (×2, visibilidade 0..2) e Normal Map (0..2), com
  // tiling/offset e conjunto de UV próprios; a máscara usa o canal A e o UV
  // da cor base, como na Unity.
  float detailAlbedoScale=1,detailNormalScale=1;
  MaterialSampling detailSampling{MaterialUv0};
  // Clear Coat (Complex Lit): máscara e suavidade; o mapa (R máscara, G
  // suavidade) multiplica os dois.
  bool clearCoat=false;
  float clearCoatMask=1,clearCoatSmoothness=.95f;
  friend bool operator==(const MaterialShading &a,const MaterialShading &b) {
    return a.shader==b.shader && a.workflow==b.workflow && a.blend==b.blend && a.preserveSpecular==b.preserveSpecular &&
           a.transparentClip==b.transparentClip && a.specularHighlights==b.specularHighlights &&
           a.environmentReflections==b.environmentReflections && a.sortingPriority==b.sortingPriority &&
           a.particleColorMode==b.particleColorMode && a.cameraFading==b.cameraFading && a.cameraFadeNear==b.cameraFadeNear &&
           a.cameraFadeFar==b.cameraFadeFar && a.specularColor[0]==b.specularColor[0] && a.specularColor[1]==b.specularColor[1] &&
           a.specularColor[2]==b.specularColor[2] && a.heightScale==b.heightScale && a.detailAlbedoScale==b.detailAlbedoScale &&
           a.detailNormalScale==b.detailNormalScale && a.detailSampling==b.detailSampling && a.clearCoat==b.clearCoat &&
           a.clearCoatMask==b.clearCoatMask && a.clearCoatSmoothness==b.clearCoatSmoothness;
  }
};
inline bool validMaterialShading(const MaterialShading &value) {
  const auto unit=[](float v,float maximum) {return std::isfinite(v) && v>=0 && v<=maximum;};
  return value.shader<MaterialShaderCount && value.workflow<=MaterialWorkflowSpecular && value.blend<=MaterialBlendMultiply &&
         value.sortingPriority>=-50 && value.sortingPriority<=50 && value.particleColorMode<=MaterialParticleColorDifference &&
         unit(value.cameraFadeNear,1000) && unit(value.cameraFadeFar,1000) && value.cameraFadeFar>=value.cameraFadeNear &&
         unit(value.specularColor[0],1) && unit(value.specularColor[1],1) && unit(value.specularColor[2],1) &&
         unit(value.heightScale,.1f) && unit(value.detailAlbedoScale,2) && unit(value.detailNormalScale,2) &&
         validMaterialSampling(value.detailSampling) && value.detailSampling.uvSet!=MaterialUvKeep && value.detailSampling.uvSet!=MaterialUvWorld &&
         unit(value.clearCoatMask,1) && unit(value.clearCoatSmoothness,1);
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
  // Transformação de UV resolvida: bit N ligado quando o binding N a tem, com as
  // linhas 2x3 em uvTransforms[N].
  std::uint8_t uvTransformMask=0;
  float uvTransforms[MaterialTextureCount][6]{};
  // Canais, oclusão e alfa resolvidos (Keep onde a fonte decide), textura de
  // oclusão resolvida como as dos bindings e o dado isolado na prévia.
  MaterialChannels channels{};
  std::uint32_t occlusionTexture=MaterialTextureKeep;
  std::uint8_t isolate=MaterialIsolateNone;
  std::uint32_t lightmapTexture=MaterialTextureKeep;
  float lightmapScaleOffset[4]{1,1,0,0};
  float lightmapIntensity=1;
  // PK-9: shader e opções do MaterialAsset, e as texturas extras resolvidas
  // como as dos bindings (índice publicado; Keep/InvalidMapTexture = nenhuma).
  MaterialShading shading{};
  std::uint32_t extraTextures[MaterialExtraTextureCount]{MaterialTextureKeep,MaterialTextureKeep,MaterialTextureKeep,MaterialTextureKeep,
                                                         MaterialTextureKeep};

  // Comparação por valor, não por bytes: a struct tem padding depois do bool, e
  // um memcmp acusaria diferença onde não existe nenhuma.
  friend bool operator==(const MaterialParameters &a,const MaterialParameters &b) {
    if(a.enabled!=b.enabled || a.roughness!=b.roughness || a.metallic!=b.metallic ||
       a.normalScale!=b.normalScale || a.specular!=b.specular || a.emissionStrength!=b.emissionStrength) return false;
    for(int i=0;i<3;++i) if(a.baseColor[i]!=b.baseColor[i] || a.emission[i]!=b.emission[i]) return false;
    for(std::uint32_t i=0;i<MaterialTextureCount;++i) if(a.textures[i]!=b.textures[i] || a.uvSets[i]!=b.uvSets[i]) return false;
    if(a.uvTransformMask!=b.uvTransformMask) return false;
    for(std::uint32_t i=0;i<MaterialTextureCount;++i)
      if((a.uvTransformMask>>i)&1u)
        for(int k=0;k<6;++k) if(a.uvTransforms[i][k]!=b.uvTransforms[i][k]) return false;
    if(a.lightmapTexture!=b.lightmapTexture || a.lightmapIntensity!=b.lightmapIntensity) return false;
    for(unsigned i=0;i<4;++i) if(a.lightmapScaleOffset[i]!=b.lightmapScaleOffset[i]) return false;
    if(!(a.channels==b.channels) || a.occlusionTexture!=b.occlusionTexture || a.isolate!=b.isolate) return false;
    if(!(a.shading==b.shading)) return false;
    for(std::uint32_t i=0;i<MaterialExtraTextureCount;++i) if(a.extraTextures[i]!=b.extraTextures[i]) return false;
    return a.alphaMode==b.alphaMode && a.sides==b.sides && a.alphaCutoff==b.alphaCutoff;
  }
  friend bool operator!=(const MaterialParameters &a,const MaterialParameters &b) {return !(a==b);}
};
// Os valores escalares sem o estado resolvido de publicação (texturas, alfa,
// faces): é o que se guarda num componente ou num MaterialAsset.
inline MaterialParameters withoutResolvedTextures(MaterialParameters value) {
  for(auto &texture:value.textures) texture=MaterialTextureKeep;
  for(auto &set:value.uvSets) set=MaterialUvKeep;
  value.uvTransformMask=0;
  for(auto &rows:value.uvTransforms) for(auto &entry:rows) entry=0;
  value.lightmapTexture=MaterialTextureKeep;value.lightmapIntensity=1;
  value.lightmapScaleOffset[0]=value.lightmapScaleOffset[1]=1;value.lightmapScaleOffset[2]=value.lightmapScaleOffset[3]=0;
  value.channels={};value.occlusionTexture=MaterialTextureKeep;value.isolate=MaterialIsolateNone;
  value.alphaMode=MaterialAlphaKeep;value.sides=MaterialSidesKeep;value.alphaCutoff=.5f;
  value.shading={};for(auto &texture:value.extraTextures) texture=MaterialTextureKeep;
  return value;
}
}
