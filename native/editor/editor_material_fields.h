#pragma once
#include "resources/material_asset.h"
#include <algorithm>
#include <array>
#include <span>
#include <string_view>

namespace ae::editor {
// PK-9: Inspector do Material do projeto como o da Unity 6/URP
// (https://docs.unity3d.com/6000.0/Documentation/Manual/urp/lit-shader.html):
// um Shader no topo e as seções Surface Options, Surface Inputs, Detail
// Inputs e Advanced Options, cada campo só quando o shader o lê. A mesma
// tabela serve à tela, às ações, ao histórico, à multiedição e aos testes:
// um campo, uma definição. Os canais da Astra (R4/G3a) ficam numa seção
// própria no fim, porque a Unity não os tem.
enum class MaterialSection : u8 {Shader,SurfaceOptions,SurfaceInputs,DetailInputs,ParticleOptions,AdvancedOptions,Count};
inline constexpr std::array<std::string_view,static_cast<usize>(MaterialSection::Count)> materialSectionTitles{
  "Shader","Opções de superfície","Entradas de superfície","Detalhe","Partícula","Avançado"};
// Segment: botões lado a lado (2-4 opções). Choice: abre a lista. Toggle:
// interruptor. Slider: barra que segue o dedo, valor tocável. Texture: abre o
// seletor no binding. Color: cor linear (r g b). Pair: dois números (x y).
// Stepper: inteiro com - e +.
enum class MaterialFieldKind : u8 {Segment,Choice,Toggle,Slider,Texture,Color,Pair,Stepper};
// Binding do seletor de textura: 0..3 os do pacote, 4 a oclusão própria,
// 5..9 as extras (altura, máscara de detalhe, detalhe de cor, normal de
// detalhe, clear coat).
struct MaterialFieldSpec {
  std::string_view key;   // "pk9.<nome>": histórico, multiedição e testes
  std::string_view label; // rótulo pt-BR
  std::string_view unity; // nome no Inspector da Unity (a busca acha pelos dois)
  MaterialSection section;
  MaterialFieldKind kind;
  std::span<const std::string_view> options{};
  float minimum=0,maximum=1;
  u32 binding=0;
  bool (*visible)(const resources::MaterialAsset &)=nullptr;
  void (*read)(const resources::MaterialAsset &,float out[3])=nullptr;
  bool (*write)(resources::MaterialAsset &,const float in[3])=nullptr;
};

namespace material_fields {
using resources::MaterialAsset;
inline constexpr std::array<std::string_view,6> shaderOptions{"Lit","Complex Lit","Simple Lit","Unlit","Particles Lit","Particles Unlit"};
inline constexpr std::array<std::string_view,2> workflowOptions{"Metálico","Especular"};
inline constexpr std::array<std::string_view,2> surfaceOptions{"Opaco","Transparente"};
inline constexpr std::array<std::string_view,4> blendOptions{"Alfa","Premultiplicado","Aditivo","Multiplicar"};
inline constexpr std::array<std::string_view,3> faceOptions{"Frente","Trás","Ambas"};
inline constexpr std::array<std::string_view,3> smoothnessOptions{"Mapa (A)","Cor base (A)","glTF"};
inline constexpr std::array<std::string_view,2> uvOptions{"UV 0","UV 1"};
inline constexpr std::array<std::string_view,6> colorModeOptions{"Multiplicar","Somar","Subtrair","Sobrepor","Cor","Diferença"};
inline bool any(const MaterialAsset &) {return true;}
inline bool lit(const MaterialAsset &m) {return scene::materialShaderHasLighting(m.shading.shader);}
inline bool detail(const MaterialAsset &m) {return scene::materialShaderHasSurfaceDetail(m.shading.shader);}
inline bool particle(const MaterialAsset &m) {return scene::materialShaderIsParticle(m.shading.shader);}
inline bool transparent(const MaterialAsset &m) {return m.surface.alphaMode==scene::MaterialAlphaBlend;}
inline bool clipping(const MaterialAsset &m) {
  return m.surface.alphaMode==scene::MaterialAlphaMask || (transparent(m) && m.shading.transparentClip);
}
inline bool specularWorkflow(const MaterialAsset &m) {
  return m.shading.shader==scene::MaterialShaderSimpleLit || (detail(m) && m.shading.workflow==scene::MaterialWorkflowSpecular);
}
inline bool metallicWorkflow(const MaterialAsset &m) {return lit(m) && !specularWorkflow(m);}
inline bool coat(const MaterialAsset &m) {return m.shading.shader==scene::MaterialShaderComplexLit;}
inline bool coatOn(const MaterialAsset &m) {return coat(m) && m.shading.clearCoat;}
inline bool unit(float v,float lo,float hi) {return v>=lo && v<=hi;}
inline u8 option(float value,u32 count) {return static_cast<u8>(std::clamp(static_cast<int>(value+.5f),0,static_cast<int>(count)-1));}
} // namespace material_fields

#define AE_READ(...) [](const resources::MaterialAsset &m,float out[3]) {(void)m;__VA_ARGS__;}
#define AE_WRITE(...) [](resources::MaterialAsset &m,const float in[3])->bool {(void)m;(void)in;__VA_ARGS__;}
inline constexpr std::array<MaterialFieldSpec,45> materialFieldSpecs{{
  {"pk9.shader","Shader","Shader",MaterialSection::Shader,MaterialFieldKind::Choice,material_fields::shaderOptions,0,5,0,material_fields::any,
   AE_READ(out[0]=m.shading.shader),AE_WRITE(m.shading.shader=material_fields::option(in[0],scene::MaterialShaderCount);return true)},
  // Surface Options.
  {"pk9.workflow","Fluxo","Workflow Mode",MaterialSection::SurfaceOptions,MaterialFieldKind::Segment,material_fields::workflowOptions,0,1,0,
   material_fields::detail,AE_READ(out[0]=m.shading.workflow),AE_WRITE(m.shading.workflow=material_fields::option(in[0],2);return true)},
  {"pk9.surface","Tipo de superfície","Surface Type",MaterialSection::SurfaceOptions,MaterialFieldKind::Segment,material_fields::surfaceOptions,0,1,0,
   material_fields::any,AE_READ(out[0]=material_fields::transparent(m)?1.f:0.f),
   AE_WRITE(const bool clip=material_fields::clipping(m);
            m.surface.alphaMode=in[0]>.5f?scene::MaterialAlphaBlend:clip?scene::MaterialAlphaMask:scene::MaterialAlphaOpaque;
            if(in[0]>.5f) m.shading.transparentClip=clip;return true)},
  {"pk9.blend","Mistura","Blending Mode",MaterialSection::SurfaceOptions,MaterialFieldKind::Choice,material_fields::blendOptions,0,3,0,
   material_fields::transparent,AE_READ(out[0]=m.shading.blend),AE_WRITE(m.shading.blend=material_fields::option(in[0],4);return true)},
  {"pk9.preserve_specular","Preservar brilho","Preserve Specular Lighting",MaterialSection::SurfaceOptions,MaterialFieldKind::Toggle,{},0,1,0,
   [](const resources::MaterialAsset &m) {
     return material_fields::transparent(m) && material_fields::lit(m) &&
            (m.shading.blend==scene::MaterialBlendAlpha || m.shading.blend==scene::MaterialBlendAdditive);},
   AE_READ(out[0]=m.shading.preserveSpecular?1.f:0.f),AE_WRITE(m.shading.preserveSpecular=in[0]>.5f;return true)},
  {"pk9.render_face","Faces","Render Face",MaterialSection::SurfaceOptions,MaterialFieldKind::Segment,material_fields::faceOptions,0,2,0,
   material_fields::any,
   AE_READ(out[0]=m.surface.sides==scene::MaterialSidesBack?1.f:m.surface.sides==scene::MaterialSidesDouble?2.f:
                  m.surface.sides==scene::MaterialSidesSingle?0.f:-1.f),
   AE_WRITE(static constexpr std::uint8_t sides[]{scene::MaterialSidesSingle,scene::MaterialSidesBack,scene::MaterialSidesDouble};
            m.surface.sides=sides[material_fields::option(in[0],3)];return true)},
  {"pk9.alpha_clip","Recorte alfa","Alpha Clipping",MaterialSection::SurfaceOptions,MaterialFieldKind::Toggle,{},0,1,0,material_fields::any,
   AE_READ(out[0]=material_fields::clipping(m)?1.f:0.f),
   AE_WRITE(if(material_fields::transparent(m)) m.shading.transparentClip=in[0]>.5f;
            else m.surface.alphaMode=in[0]>.5f?scene::MaterialAlphaMask:scene::MaterialAlphaOpaque;
            return true)},
  {"pk9.threshold","Limiar","Threshold",MaterialSection::SurfaceOptions,MaterialFieldKind::Slider,{},0,1,0,material_fields::clipping,
   AE_READ(out[0]=m.surface.alphaCutoff),AE_WRITE(if(!material_fields::unit(in[0],0,1)) return false;m.surface.alphaCutoff=in[0];return true)},
  // Surface Inputs.
  {"pk9.base_map","Mapa base","Base Map",MaterialSection::SurfaceInputs,MaterialFieldKind::Texture,{},0,1,0,material_fields::any},
  {"pk9.base_color","Cor base","Base Color",MaterialSection::SurfaceInputs,MaterialFieldKind::Color,{},0,1,0,material_fields::any,
   AE_READ(std::copy_n(m.values.baseColor,3,out)),
   AE_WRITE(for(int i=0;i<3;++i) if(!material_fields::unit(in[i],0,1)) return false;std::copy_n(in,3,m.values.baseColor);return true)},
  {"pk9.metallic_map","Mapa metálico","Metallic Map",MaterialSection::SurfaceInputs,MaterialFieldKind::Texture,{},0,1,2,
   material_fields::metallicWorkflow},
  {"pk9.specular_map","Mapa especular","Specular Map",MaterialSection::SurfaceInputs,MaterialFieldKind::Texture,{},0,1,2,
   material_fields::specularWorkflow},
  {"pk9.metallic","Metálico","Metallic",MaterialSection::SurfaceInputs,MaterialFieldKind::Slider,{},0,1,0,material_fields::metallicWorkflow,
   AE_READ(out[0]=m.values.metallic),AE_WRITE(if(!material_fields::unit(in[0],0,1)) return false;m.values.metallic=in[0];return true)},
  {"pk9.specular_color","Cor especular","Specular Color",MaterialSection::SurfaceInputs,MaterialFieldKind::Color,{},0,1,0,
   material_fields::specularWorkflow,AE_READ(std::copy_n(m.shading.specularColor,3,out)),
   AE_WRITE(for(int i=0;i<3;++i) if(!material_fields::unit(in[i],0,1)) return false;std::copy_n(in,3,m.shading.specularColor);return true)},
  // T-02: o arquivo guarda rugosidade; o Inspector e a API mostram Smoothness.
  {"pk9.smoothness","Suavidade","Smoothness",MaterialSection::SurfaceInputs,MaterialFieldKind::Slider,{},0,1,0,material_fields::lit,
   AE_READ(out[0]=1-m.values.roughness),AE_WRITE(if(!material_fields::unit(in[0],0,1)) return false;m.values.roughness=1-in[0];return true)},
  {"pk9.smoothness_source","Origem da suavidade","Source",MaterialSection::SurfaceInputs,MaterialFieldKind::Segment,
   material_fields::smoothnessOptions,0,2,0,material_fields::lit,
   AE_READ(out[0]=m.channels.roughnessSource==scene::MaterialRoughnessSmoothnessMap?0.f:
                  m.channels.roughnessSource==scene::MaterialRoughnessSmoothnessBaseAlpha?1.f:2.f),
   AE_WRITE(switch(material_fields::option(in[0],3)) {
     case 0: m.channels.roughnessSource=scene::MaterialRoughnessSmoothnessMap;m.channels.roughness=scene::MaterialChannelA;break;
     case 1: m.channels.roughnessSource=scene::MaterialRoughnessSmoothnessBaseAlpha;break;
     default: m.channels.roughnessSource=scene::MaterialRoughnessKeep;
              if(m.channels.roughness==scene::MaterialChannelA) m.channels.roughness=scene::MaterialChannelKeep;break;
   } return true)},
  {"pk9.normal_map","Mapa normal","Normal Map",MaterialSection::SurfaceInputs,MaterialFieldKind::Texture,{},0,1,1,material_fields::lit},
  {"pk9.normal_scale","Força do normal","Normal Scale",MaterialSection::SurfaceInputs,MaterialFieldKind::Slider,{},0,2,0,material_fields::lit,
   AE_READ(out[0]=m.values.normalScale),AE_WRITE(if(!material_fields::unit(in[0],0,16)) return false;m.values.normalScale=in[0];return true)},
  {"pk9.height_map","Mapa de altura","Height Map",MaterialSection::SurfaceInputs,MaterialFieldKind::Texture,{},0,1,
   scene::MaterialExtraTextureBindingBase+scene::MaterialExtraHeight,material_fields::detail},
  {"pk9.height_scale","Altura","Height Scale",MaterialSection::SurfaceInputs,MaterialFieldKind::Slider,{},0,.1f,0,
   [](const resources::MaterialAsset &m) {return material_fields::detail(m) && m.extraTextures[scene::MaterialExtraHeight].valid();},
   AE_READ(out[0]=m.shading.heightScale),AE_WRITE(if(!material_fields::unit(in[0],0,.1f)) return false;m.shading.heightScale=in[0];return true)},
  {"pk9.occlusion_map","Mapa de oclusão","Occlusion Map",MaterialSection::SurfaceInputs,MaterialFieldKind::Texture,{},0,1,
   scene::MaterialOcclusionTextureBinding,material_fields::lit},
  {"pk9.occlusion_strength","Força da oclusão","Occlusion Strength",MaterialSection::SurfaceInputs,MaterialFieldKind::Slider,{},0,1,0,
   material_fields::lit,AE_READ(out[0]=m.channels.occlusionStrength<0?1.f:m.channels.occlusionStrength),
   AE_WRITE(if(!material_fields::unit(in[0],0,1)) return false;m.channels.occlusionStrength=in[0];return true)},
  {"pk9.clear_coat","Verniz","Clear Coat",MaterialSection::SurfaceInputs,MaterialFieldKind::Toggle,{},0,1,0,material_fields::coat,
   AE_READ(out[0]=m.shading.clearCoat?1.f:0.f),AE_WRITE(m.shading.clearCoat=in[0]>.5f;return true)},
  {"pk9.clear_coat_map","Mapa do verniz","Clear Coat Mask Map",MaterialSection::SurfaceInputs,MaterialFieldKind::Texture,{},0,1,
   scene::MaterialExtraTextureBindingBase+scene::MaterialExtraClearCoat,material_fields::coatOn},
  {"pk9.clear_coat_mask","Máscara do verniz","Mask",MaterialSection::SurfaceInputs,MaterialFieldKind::Slider,{},0,1,0,material_fields::coatOn,
   AE_READ(out[0]=m.shading.clearCoatMask),AE_WRITE(if(!material_fields::unit(in[0],0,1)) return false;m.shading.clearCoatMask=in[0];return true)},
  {"pk9.clear_coat_smoothness","Suavidade do verniz","Clear Coat Smoothness",MaterialSection::SurfaceInputs,MaterialFieldKind::Slider,{},0,1,0,
   material_fields::coatOn,AE_READ(out[0]=m.shading.clearCoatSmoothness),
   AE_WRITE(if(!material_fields::unit(in[0],0,1)) return false;m.shading.clearCoatSmoothness=in[0];return true)},
  {"pk9.emission_map","Mapa de emissão","Emission Map",MaterialSection::SurfaceInputs,MaterialFieldKind::Texture,{},0,1,3,material_fields::any},
  {"pk9.emission_color","Cor de emissão","Emission Color",MaterialSection::SurfaceInputs,MaterialFieldKind::Color,{},0,1,0,material_fields::any,
   AE_READ(std::copy_n(m.values.emission,3,out)),
   AE_WRITE(for(int i=0;i<3;++i) if(!material_fields::unit(in[i],0,1)) return false;std::copy_n(in,3,m.values.emission);return true)},
  {"pk9.emission_intensity","Intensidade da emissão","Emission Intensity",MaterialSection::SurfaceInputs,MaterialFieldKind::Slider,{},0,20,0,
   material_fields::any,AE_READ(out[0]=m.values.emissionStrength),
   AE_WRITE(if(!material_fields::unit(in[0],0,10000)) return false;m.values.emissionStrength=in[0];return true)},
  // Tiling/Offset do Base Map valem para todos os mapas (o _BaseMap_ST da URP).
  {"pk9.tiling","Repetição","Tiling",MaterialSection::SurfaceInputs,MaterialFieldKind::Pair,{},.01f,100,0,material_fields::any,
   AE_READ(out[0]=m.sampling[0].scale[0];out[1]=m.sampling[0].scale[1]),
   AE_WRITE(if(!material_fields::unit(in[0],.01f,100) || !material_fields::unit(in[1],.01f,100)) return false;
            for(auto &s:m.sampling) {s.scale[0]=in[0];s.scale[1]=in[1];} return true)},
  {"pk9.offset","Deslocamento","Offset",MaterialSection::SurfaceInputs,MaterialFieldKind::Pair,{},-100,100,0,material_fields::any,
   AE_READ(out[0]=m.sampling[0].offset[0];out[1]=m.sampling[0].offset[1]),
   AE_WRITE(if(!material_fields::unit(in[0],-100,100) || !material_fields::unit(in[1],-100,100)) return false;
            for(auto &s:m.sampling) {s.offset[0]=in[0];s.offset[1]=in[1];} return true)},
  // Detail Inputs.
  {"pk9.detail_mask","Máscara","Mask",MaterialSection::DetailInputs,MaterialFieldKind::Texture,{},0,1,
   scene::MaterialExtraTextureBindingBase+scene::MaterialExtraDetailMask,material_fields::detail},
  {"pk9.detail_albedo_map","Mapa base de detalhe","Detail Base Map",MaterialSection::DetailInputs,MaterialFieldKind::Texture,{},0,1,
   scene::MaterialExtraTextureBindingBase+scene::MaterialExtraDetailAlbedo,material_fields::detail},
  {"pk9.detail_albedo_scale","Visibilidade","Detail Base Map scale",MaterialSection::DetailInputs,MaterialFieldKind::Slider,{},0,2,0,
   material_fields::detail,AE_READ(out[0]=m.shading.detailAlbedoScale),
   AE_WRITE(if(!material_fields::unit(in[0],0,2)) return false;m.shading.detailAlbedoScale=in[0];return true)},
  {"pk9.detail_normal_map","Normal de detalhe","Detail Normal Map",MaterialSection::DetailInputs,MaterialFieldKind::Texture,{},0,1,
   scene::MaterialExtraTextureBindingBase+scene::MaterialExtraDetailNormal,material_fields::detail},
  {"pk9.detail_normal_scale","Força do detalhe","Detail Normal Scale",MaterialSection::DetailInputs,MaterialFieldKind::Slider,{},0,2,0,
   material_fields::detail,AE_READ(out[0]=m.shading.detailNormalScale),
   AE_WRITE(if(!material_fields::unit(in[0],0,2)) return false;m.shading.detailNormalScale=in[0];return true)},
  {"pk9.detail_tiling","Repetição do detalhe","Detail Tiling",MaterialSection::DetailInputs,MaterialFieldKind::Pair,{},.01f,100,0,
   material_fields::detail,AE_READ(out[0]=m.shading.detailSampling.scale[0];out[1]=m.shading.detailSampling.scale[1]),
   AE_WRITE(if(!material_fields::unit(in[0],.01f,100) || !material_fields::unit(in[1],.01f,100)) return false;
            m.shading.detailSampling.scale[0]=in[0];m.shading.detailSampling.scale[1]=in[1];return true)},
  {"pk9.detail_offset","Deslocamento do detalhe","Detail Offset",MaterialSection::DetailInputs,MaterialFieldKind::Pair,{},-100,100,0,
   material_fields::detail,AE_READ(out[0]=m.shading.detailSampling.offset[0];out[1]=m.shading.detailSampling.offset[1]),
   AE_WRITE(if(!material_fields::unit(in[0],-100,100) || !material_fields::unit(in[1],-100,100)) return false;
            m.shading.detailSampling.offset[0]=in[0];m.shading.detailSampling.offset[1]=in[1];return true)},
  {"pk9.detail_uv","UV do detalhe","UV Set",MaterialSection::DetailInputs,MaterialFieldKind::Segment,material_fields::uvOptions,0,1,0,
   material_fields::detail,AE_READ(out[0]=m.shading.detailSampling.uvSet==scene::MaterialUv1?1.f:0.f),
   AE_WRITE(m.shading.detailSampling.uvSet=in[0]>.5f?scene::MaterialUv1:scene::MaterialUv0;return true)},
  // Particles Lit/Unlit.
  {"pk9.color_mode","Modo de cor","Color Mode",MaterialSection::ParticleOptions,MaterialFieldKind::Choice,material_fields::colorModeOptions,0,5,0,
   material_fields::particle,AE_READ(out[0]=m.shading.particleColorMode),
   AE_WRITE(m.shading.particleColorMode=material_fields::option(in[0],6);return true)},
  {"pk9.camera_fading","Sumir perto da câmera","Camera Fading",MaterialSection::ParticleOptions,MaterialFieldKind::Toggle,{},0,1,0,
   material_fields::particle,AE_READ(out[0]=m.shading.cameraFading?1.f:0.f),AE_WRITE(m.shading.cameraFading=in[0]>.5f;return true)},
  {"pk9.camera_fade_distance","Distâncias (de, até)","Near / Far",MaterialSection::ParticleOptions,MaterialFieldKind::Pair,{},0,1000,0,
   [](const resources::MaterialAsset &m) {return material_fields::particle(m) && m.shading.cameraFading;},
   AE_READ(out[0]=m.shading.cameraFadeNear;out[1]=m.shading.cameraFadeFar),
   AE_WRITE(if(!material_fields::unit(in[0],0,1000) || !material_fields::unit(in[1],in[0],1000)) return false;
            m.shading.cameraFadeNear=in[0];m.shading.cameraFadeFar=in[1];return true)},
  // Advanced Options.
  {"pk9.specular_highlights","Brilho das luzes","Specular Highlights",MaterialSection::AdvancedOptions,MaterialFieldKind::Toggle,{},0,1,0,
   material_fields::lit,AE_READ(out[0]=m.shading.specularHighlights?1.f:0.f),AE_WRITE(m.shading.specularHighlights=in[0]>.5f;return true)},
  {"pk9.environment_reflections","Reflexos do ambiente","Environment Reflections",MaterialSection::AdvancedOptions,MaterialFieldKind::Toggle,{},0,1,0,
   [](const resources::MaterialAsset &m) {return material_fields::lit(m) && m.shading.shader!=scene::MaterialShaderSimpleLit;},
   AE_READ(out[0]=m.shading.environmentReflections?1.f:0.f),AE_WRITE(m.shading.environmentReflections=in[0]>.5f;return true)},
  {"pk9.sorting_priority","Prioridade de ordenação","Sorting Priority",MaterialSection::AdvancedOptions,MaterialFieldKind::Stepper,{},-50,50,0,
   material_fields::any,AE_READ(out[0]=m.shading.sortingPriority),
   AE_WRITE(if(!material_fields::unit(in[0],-50,50)) return false;m.shading.sortingPriority=static_cast<std::int8_t>(in[0]);return true)},
}};
#undef AE_READ
#undef AE_WRITE

inline const MaterialFieldSpec *findMaterialField(std::string_view key) {
  for(const auto &field:materialFieldSpecs) if(field.key==key) return &field;
  return nullptr;
}
inline u32 materialFieldIndex(const MaterialFieldSpec &field) {return static_cast<u32>(&field-materialFieldSpecs.data());}
// Aplica um valor validando o recurso inteiro: o campo pode recusar e o
// material resultante precisa continuar dentro do contrato.
inline bool applyMaterialField(const MaterialFieldSpec &field,resources::MaterialAsset &material,const float value[3]) {
  if(!field.write) return false;
  auto candidate=material;
  if(!field.write(candidate,value) || !candidate.valid()) return false;
  material=std::move(candidate);
  return true;
}
} // namespace ae::editor
