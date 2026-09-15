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
