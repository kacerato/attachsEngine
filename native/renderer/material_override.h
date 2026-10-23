#pragma once
#include "renderer/map_package.h"
#include "scene/material_parameters.h"
#include <algorithm>
namespace ae::renderer {
// Scalar instance overrides preserve the shared textures and pipeline class.
using MaterialOverride=scene::MaterialParameters;
using MaterialShading=scene::MaterialShading;
inline MaterialOverride materialOverrideFrom(const MapMaterialRecord &source) {
  MaterialOverride result;
  for(u32 i=0;i<3;++i) {result.baseColor[i]=source.baseColorFactor[i];result.emission[i]=source.emissiveFactorAndStrength[i];}
  result.roughness=source.roughness;result.metallic=source.metallic;
  result.normalScale=source.normalScale;result.specular=source.specular;
  result.emissionStrength=source.emissiveFactorAndStrength[3];return result;
}
// R4: bindings de textura, modo de alfa e faces trocados valem mesmo sem
// substituição escalar (`enabled`). `textureBase` é onde a biblioteca importada
// começa na lista de texturas do renderer, o mesmo deslocamento aplicado aos
// materiais importados. Trocar ou tirar normal, metálico/rugosidade ou emissão
// atualiza a flag do binding: é ela que escolhe a variante de pipeline.
// R4: uma entrada da tabela de transformações de UV que o shader lê
// (`material_uv_transform.glsl`): duas linhas vec4 por binding, (a b c 0) e
// (d e f 0), identidade onde o binding não transforma.
inline constexpr u32 MaterialUvTransformFloats=scene::MaterialTextureCount*8;
inline void materialUvTransformEntry(const MaterialOverride &value,float out[MaterialUvTransformFloats]) {
  static constexpr float identity[6]{1,0,0,0,1,0};
  for(u32 binding=0;binding<scene::MaterialTextureCount;++binding) {
    const float *rows=((value.uvTransformMask>>binding)&1u)?value.uvTransforms[binding]:identity;
    float *entry=out+binding*8;
    entry[0]=rows[0];entry[1]=rows[1];entry[2]=rows[2];entry[3]=0;
    entry[4]=rows[3];entry[5]=rows[4];entry[6]=rows[5];entry[7]=0;
  }
}
// R4: entrada completa da extensão de material por desenho: as oito linhas de
// transformação de UV acima e mais seis linhas, já com o padrão da fonte
// resolvido (o shader não conhece "herdar"):
//   linha 8:  força da oclusão, origem (0 nenhuma, 1 metal/rugosidade, 2 textura
//             própria), canal da oclusão (0..3), índice bindless da textura
//   linha 9:  canal da rugosidade, canal do metal, origem do alfa (0 cor base,
//             1 opaco, 2 luminância), inversão Y do normal (0/1)
//   linha 10: dado isolado na prévia (MaterialIsolate*), origem da rugosidade
//             (0 canal como rugosidade, 1 canal como suavidade, 2 alfa da cor
//             base como suavidade)
//   linha 11: lightmap ativo, índice bindless, intensidade, reservado
//   linha 12: lightmap escala UV1 xy e deslocamento zw
// PK-9 (shader do Material, só com MapMaterialShadingExtension):
//   linha 13: shader (MaterialShader*), workflow (0 metálico, 1 especular),
//             saída da mistura (MaterialBlendOutput*), bits (1 Specular
//             Highlights, 2 Environment Reflections, 4 Preserve Specular,
//             8 recorte no transparente, 16 esmaecer perto da câmera, 32 clear coat)
//   linha 14: cor especular linear rgb, corte do recorte no transparente
//   linha 15: altura (índice bindless ou -1), amplitude da altura, máscara de
//             detalhe (índice ou -1), detalhe de cor (índice ou -1)
//   linha 16: normal de detalhe (índice ou -1), visibilidade do detalhe de cor,
//             força do normal de detalhe, conjunto de UV do detalhe (0/1)
//   linhas 17-18: transformação de UV do detalhe (a b c 0) (d e f 0)
//   linha 19: máscara do clear coat, suavidade do clear coat, mapa (índice ou -1), 0
//   linha 20: modo de cor da partícula, esmaecer de (m), esmaecer até (m), 0
inline constexpr u32 MaterialExtensionRows=21;
inline constexpr u32 MaterialExtensionFloats=MaterialExtensionRows*4;
inline constexpr u32 MaterialExtensionNoTexture=0xFFFFFFFFu;
// Como a cor do transparente vai ao framebuffer. Reto: (cor, a) com SRC_ALPHA.
// Premultiplicado: (difuso·a + especular, a) com ONE (Premultiply e Alpha com
// Preserve Specular). Aditivo: ONE/ONE com a cor já multiplicada pelo alfa
// (o especular inteiro com Preserve Specular). Multiplicar: DST_COLOR/ZERO
// com mix(1, cor, a). É também o índice da pipeline transparente.
inline constexpr u32 MaterialBlendOutputStraight=0,MaterialBlendOutputPremultiplied=1,MaterialBlendOutputAdditive=2,
                     MaterialBlendOutputMultiply=3,MaterialBlendOutputCount=4;
inline u32 materialBlendOutput(const scene::MaterialShading &shading) {
  switch(shading.blend) {
  case scene::MaterialBlendPremultiply: return MaterialBlendOutputPremultiplied;
  case scene::MaterialBlendAdditive: return MaterialBlendOutputAdditive;
  case scene::MaterialBlendMultiply: return MaterialBlendOutputMultiply;
  default: return shading.preserveSpecular?MaterialBlendOutputPremultiplied:MaterialBlendOutputStraight;
  }
}
inline bool materialExtraTexturePresent(u32 resolved) {return resolved!=scene::MaterialTextureKeep && resolved!=InvalidMapTexture;}
// PK-9: o shader ou as opções saem do Lit padrão? `transparent` é o modo
// efetivo: mistura, Preserve Specular e recorte só existem no transparente.
inline bool materialShadingExtended(const MaterialOverride &value,bool transparent) {
  const auto &s=value.shading;
  if(s.shader!=scene::MaterialShaderLit || s.workflow!=scene::MaterialWorkflowMetallic || !s.specularHighlights || !s.environmentReflections)
    return true;
  for(u32 extra=0;extra<scene::MaterialExtraTextureCount;++extra)
    if(materialExtraTexturePresent(value.extraTextures[extra])) return true;
  return transparent && (materialBlendOutput(s)!=MaterialBlendOutputStraight || s.transparentClip);
}
inline bool needsMaterialExtension(const MaterialOverride &value) {
  return (value.lightmapTexture!=scene::MaterialTextureKeep && value.lightmapTexture!=InvalidMapTexture) || value.uvTransformMask!=0 || value.channels.overrides() || value.isolate!=scene::MaterialIsolateNone ||
         (value.occlusionTexture!=scene::MaterialTextureKeep && value.occlusionTexture!=InvalidMapTexture);
}
// `effectiveFlags` são as flags do material depois de applyMaterialOverride;
// `occlusionSlot` é o índice bindless da textura de oclusão, ou NoTexture.
inline void materialExtensionEntry(const MaterialOverride &value,u32 effectiveFlags,u32 occlusionSlot,
                                   float out[MaterialExtensionFloats],u32 lightmapSlot=MaterialExtensionNoTexture,
                                   const u32 *extraSlots=nullptr) {
  materialUvTransformEntry(value,out);
  const auto &channels=value.channels;
  const auto channel=[](std::uint8_t chosen,u32 fallback) {return static_cast<float>(chosen?chosen-1u:fallback);};
  u32 source=channels.occlusionSource;
  if(source==scene::MaterialOcclusionKeep)
    source=(effectiveFlags&MapMaterialOcclusionInMetallicRoughness)?scene::MaterialOcclusionPacked:scene::MaterialOcclusionNone;
  if(source==scene::MaterialOcclusionTexture && occlusionSlot==MaterialExtensionNoTexture) source=scene::MaterialOcclusionNone;
  float *row=out+32;
  row[0]=channels.occlusionStrength<0?1.0f:channels.occlusionStrength;
  row[1]=static_cast<float>(source-1u);
  row[2]=channel(channels.occlusion,0);
  row[3]=source==scene::MaterialOcclusionTexture?static_cast<float>(occlusionSlot):0.0f;
  row+=4;
  row[0]=channel(channels.roughness,1);
  row[1]=channel(channels.metallic,2);
  row[2]=static_cast<float>(channels.alphaSource==scene::MaterialAlphaSourceKeep?0u:channels.alphaSource-1u);
  row[3]=channels.normalFlipY==scene::MaterialToggleOn?1.0f:0.0f;
  row+=4;
  row[0]=static_cast<float>(value.isolate);
  row[1]=static_cast<float>(channels.roughnessSource==scene::MaterialRoughnessKeep?0u:channels.roughnessSource-1u);
  row[2]=row[3]=0;
  row+=4;
  row[0]=lightmapSlot==MaterialExtensionNoTexture?0.0f:1.0f;
  row[1]=lightmapSlot==MaterialExtensionNoTexture?0.0f:static_cast<float>(lightmapSlot);
  row[2]=value.lightmapIntensity;row[3]=0;
  row+=4;
  for(unsigned i=0;i<4;++i) row[i]=value.lightmapScaleOffset[i];
  row+=4;
  // PK-9. Sem a flag de extensão o shader não lê daqui: tudo zero.
  for(u32 i=0;i<(MaterialExtensionRows-13)*4;++i) row[i]=0;
  if(!(effectiveFlags&MapMaterialShadingExtension)) return;
  const auto &s=value.shading;
  const auto slot=[&](u32 extra) {
    const u32 resolved=extraSlots?extraSlots[extra]:MaterialExtensionNoTexture;
    return resolved==MaterialExtensionNoTexture?-1.0f:static_cast<float>(resolved);
  };
  const bool transparent=(effectiveFlags&MapMaterialBlend)!=0;
  const bool coat=s.shader==scene::MaterialShaderComplexLit && s.clearCoat;
  const bool detailed=scene::materialShaderHasSurfaceDetail(s.shader);
  row[0]=static_cast<float>(s.shader);
  row[1]=static_cast<float>(s.workflow);
  row[2]=static_cast<float>(transparent?materialBlendOutput(s):MaterialBlendOutputStraight);
  row[3]=static_cast<float>((s.specularHighlights?1u:0u)|(s.environmentReflections?2u:0u)|(s.preserveSpecular?4u:0u)|
                            (transparent&&s.transparentClip?8u:0u)|(scene::materialShaderIsParticle(s.shader)&&s.cameraFading?16u:0u)|
                            (coat?32u:0u));
  row+=4;
  row[0]=s.specularColor[0];row[1]=s.specularColor[1];row[2]=s.specularColor[2];
  row[3]=std::clamp(value.alphaCutoff,0.0f,1.0f);
  row+=4;
  row[0]=detailed?slot(scene::MaterialExtraHeight):-1.0f;row[1]=s.heightScale;
  row[2]=detailed?slot(scene::MaterialExtraDetailMask):-1.0f;row[3]=detailed?slot(scene::MaterialExtraDetailAlbedo):-1.0f;
  row+=4;
  row[0]=detailed?slot(scene::MaterialExtraDetailNormal):-1.0f;row[1]=s.detailAlbedoScale;row[2]=s.detailNormalScale;
  row[3]=s.detailSampling.uvSet==scene::MaterialUv1?1.0f:0.0f;
  row+=4;
  float detailRows[6];
  scene::materialUvTransformRows(s.detailSampling,detailRows);
  row[0]=detailRows[0];row[1]=detailRows[1];row[2]=detailRows[2];row[3]=0;
  row[4]=detailRows[3];row[5]=detailRows[4];row[6]=detailRows[5];row[7]=0;
  row+=8;
  row[0]=s.clearCoatMask;row[1]=s.clearCoatSmoothness;row[2]=coat?slot(scene::MaterialExtraClearCoat):-1.0f;row[3]=0;
  row+=4;
  row[0]=static_cast<float>(s.particleColorMode);row[1]=s.cameraFadeNear;row[2]=s.cameraFadeFar;row[3]=0;
}
// Culling da rasterização pelas flags efetivas: 0 nenhum, 1 a face de trás
// (uma face com ordem provada), 2 a da frente (Render Face "Back").
inline u32 materialCullMode(u32 flags) {
  if(flags&MapMaterialDoubleSided) return 0;
  if(flags&MapMaterialCullFrontFaces) return 2;
  return (flags&MapMaterialCullBackFaces)?1u:0u;
}
inline MapMaterialRecord applyMaterialOverride(const MapMaterialRecord &source,const MaterialOverride &value,u32 textureBase=0) {
  auto result=source;
  static constexpr u32 bindingFlags[scene::MaterialTextureCount]{0,MapMaterialNormalMap,MapMaterialMetallicRoughnessMap,MapMaterialEmissiveMap};
  for(u32 slot=0;slot<scene::MaterialTextureCount;++slot) {
    const u32 texture=value.textures[slot];
    if(texture==scene::MaterialTextureKeep) continue;
    result.textureIndices[slot]=texture==InvalidMapTexture?InvalidMapTexture:texture+textureBase;
    if(bindingFlags[slot]) {
      if(texture==InvalidMapTexture) result.flags&=~bindingFlags[slot];
      else result.flags|=bindingFlags[slot];
    }
    // A oclusão da fonte vive no canal R do mapa metálico/rugosidade dela; outra
    // textura nesse binding não carrega essa promessa.
    if(slot==2) result.flags&=~MapMaterialOcclusionInMetallicRoughness;
  }
  // Conjunto de UV por binding: dois bits por slot, o mesmo campo que o shader lê.
  for(u32 slot=0;slot<scene::MaterialTextureCount;++slot) {
    const u32 set=value.uvSets[slot];
    if(set==scene::MaterialUvKeep || set>scene::MaterialUvWorld) continue;
    result.textureCoordinates=(result.textureCoordinates&~(3u<<(slot*2)))|((set-1)<<(slot*2));
  }
  // Modo de alfa decide a fila (sólido, recorte, transparente) e o corte.
  switch(value.alphaMode) {
  case scene::MaterialAlphaOpaque: result.flags&=~(MapMaterialBlend|MapMaterialAlphaMask); break;
  case scene::MaterialAlphaMask:
    result.flags=(result.flags&~MapMaterialBlend)|MapMaterialAlphaMask;
    result.alphaCutoff=std::clamp(value.alphaCutoff,0.0f,1.0f);
    break;
  case scene::MaterialAlphaBlend: result.flags=(result.flags&~MapMaterialAlphaMask)|MapMaterialBlend; break;
  default: break;
  }
  // Faces: uma face pede culling de trás; duas faces desliga; só a de trás
  // (Render Face Back) descarta a da frente.
  if(value.sides==scene::MaterialSidesSingle)
    result.flags=(result.flags&~(MapMaterialDoubleSided|MapMaterialCullFrontFaces))|MapMaterialCullBackFaces;
  else if(value.sides==scene::MaterialSidesDouble)
    result.flags=(result.flags&~(MapMaterialCullBackFaces|MapMaterialCullFrontFaces))|MapMaterialDoubleSided;
  else if(value.sides==scene::MaterialSidesBack)
    result.flags=(result.flags&~(MapMaterialCullBackFaces|MapMaterialDoubleSided))|MapMaterialCullFrontFaces;
  if(materialShadingExtended(value,(result.flags&MapMaterialBlend)!=0)) result.flags|=MapMaterialShadingExtension;
  else result.flags&=~MapMaterialShadingExtension;
  if(!value.enabled) return result;
  for(u32 i=0;i<3;++i) {result.baseColorFactor[i]=value.baseColor[i];result.emissiveFactorAndStrength[i]=value.emission[i];}
  result.roughness=value.roughness;result.metallic=value.metallic;
  result.normalScale=value.normalScale;result.specular=value.specular;
  result.emissiveFactorAndStrength[3]=value.emissionStrength;return result;
}
}
