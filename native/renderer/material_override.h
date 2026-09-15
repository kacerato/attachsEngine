#pragma once
#include "renderer/map_package.h"
#include "scene/material_parameters.h"
#include <algorithm>
namespace ae::renderer {
// Scalar instance overrides preserve the shared textures and pipeline class.
using MaterialOverride=scene::MaterialParameters;
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
    if(set==scene::MaterialUvKeep || set>scene::MaterialUv1) continue;
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
  // Faces: uma face pede culling de trás; duas faces desliga.
  if(value.sides==scene::MaterialSidesSingle) result.flags=(result.flags&~MapMaterialDoubleSided)|MapMaterialCullBackFaces;
  else if(value.sides==scene::MaterialSidesDouble) result.flags=(result.flags&~MapMaterialCullBackFaces)|MapMaterialDoubleSided;
  if(!value.enabled) return result;
  for(u32 i=0;i<3;++i) {result.baseColorFactor[i]=value.baseColor[i];result.emissiveFactorAndStrength[i]=value.emission[i];}
  result.roughness=value.roughness;result.metallic=value.metallic;
  result.normalScale=value.normalScale;result.specular=value.specular;
  result.emissiveFactorAndStrength[3]=value.emissionStrength;return result;
}
}
