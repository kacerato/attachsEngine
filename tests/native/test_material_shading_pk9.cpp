#include "harness.h"
#include "editor/editor_material_fields.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_screen.h"
#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "renderer/material_override.h"
#include "renderer/primitive_geometry.h"
#include "resources/unity_material_import.h"
#include "runtime/primitive_object.h"
#include "ui_software_raster.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>

// PK-9 (auditoria full Unity, T-01..T-05): shader do Material, vocabulário
// Smoothness, altura/detalhe/clear coat/especular, superfície transparente e o
// importador. Cada teste prova um consumidor real: o arquivo, as flags e a
// extensão que o shader lê, o leitor Unity e o Inspector pelo toque.
using namespace ae;
using namespace ae::editor;
namespace {
resources::MaterialAsset sample() {
  resources::MaterialAsset material;
  material.guid=resources::assetGuidFromSeed("pk9-material");material.name="Verniz";
  material.values.enabled=true;
  auto &s=material.shading;
  s.shader=scene::MaterialShaderComplexLit;s.workflow=scene::MaterialWorkflowSpecular;s.blend=scene::MaterialBlendAdditive;
  s.preserveSpecular=false;s.transparentClip=true;s.specularHighlights=false;s.environmentReflections=false;s.sortingPriority=-7;
  s.particleColorMode=scene::MaterialParticleColorOverlay;s.cameraFading=true;s.cameraFadeNear=.5f;s.cameraFadeFar=3;
  s.specularColor[0]=.1f;s.specularColor[1]=.2f;s.specularColor[2]=.3f;s.heightScale=.04f;s.detailAlbedoScale=1.5f;s.detailNormalScale=.75f;
  s.detailSampling.uvSet=scene::MaterialUv1;s.detailSampling.scale[0]=8;s.detailSampling.scale[1]=6;s.detailSampling.offset[0]=.25f;
  s.clearCoat=true;s.clearCoatMask=.8f;s.clearCoatSmoothness=.9f;
  material.extraTextures[scene::MaterialExtraHeight]=resources::assetGuidFromSeed("height");
  material.extraTextures[scene::MaterialExtraDetailNormal]=resources::assetGuidFromSeed("detail-normal");
  material.surface.alphaMode=scene::MaterialAlphaBlend;material.surface.sides=scene::MaterialSidesBack;
  return material;
}
std::string dropTokens(std::string text,int count) {
  while(!text.empty()&&(text.back()=='\n'||text.back()=='\r')) text.pop_back();
  for(int i=0;i<count;++i) text.erase(text.rfind(' '));
  return text+"\n";
}
std::string unity(const std::string &shader,const std::string &properties,const std::string &keywords) {
  return "%YAML 1.1\n--- !u!21 &2100000\nMaterial:\n  m_Name: Teste\n  m_Shader: "+shader+"\n  m_ValidKeywords:\n"+keywords+
         "  m_SavedProperties:\n"+properties;
}
const std::string urpLit="{fileID: 4800000, guid: 933532a4fcc9baf4fa0491de14d08ed7, type: 3}";
const std::string standard="{fileID: 46, guid: 0000000000000000f000000000000000, type: 0}";
std::string texture(const char *name,const char *guid,float scale=1) {
  char text[256];
  std::snprintf(text,sizeof(text),"    - %s:\n        m_Texture: {fileID: 2800000, guid: %s, type: 3}\n        m_Scale: {x: %g, y: %g}\n"
                "        m_Offset: {x: 0, y: 0}\n",name,guid,static_cast<double>(scale),static_cast<double>(scale));
  return text;
}
struct Project {
  std::filesystem::path path=std::filesystem::temp_directory_path()/
    ("astra-pk9-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Project() {std::filesystem::create_directories(path);}
  ~Project() {std::error_code error;std::filesystem::remove_all(path,error);}
};
bool captureUi(const std::string &path,u32 width,u32 height,ui::UiDrawList &list,ui::UiFont &font,ui::UiIconAtlas &icons) {
  std::vector<ui::UiInstance> instances;const auto result=ui::buildUiInstances(list,font,icons,32768,instances);
  if(result.dropped||result.missingGlyphRuns) return false;
  test::UiSoftwareTarget target;target.resize(width,height,.12f,.13f,.14f);test::rasterizeUi(instances,font,icons,target);
  std::FILE *output=std::fopen(path.c_str(),"wb");if(!output) return false;
  std::fprintf(output,"P6\n%u %u\n255\n",width,height);
  for(usize i=0;i<target.pixels.size();i+=4) for(u32 c=0;c<3;++c)
    std::fputc(static_cast<u8>(std::clamp(target.pixels[i+c],0.0f,1.0f)*255+.5f),output);
  return std::fclose(output)==0;
}
} // namespace

AE_TEST(pk9_material_asset_v8_round_trip_and_migration) {
  const auto material=sample();
  AE_EXPECT_TRUE(material.valid(),"sample valid");
  const auto text=material.serialize();
  AE_EXPECT_TRUE(text.starts_with("ASTRA_MATERIAL 8 "),"current version");
  resources::MaterialAsset back;
  AE_EXPECT_TRUE(resources::MaterialAsset::deserialize(text,back),"v8 reads");
  AE_EXPECT_TRUE(back.shading==material.shading && back.extraTextures==material.extraTextures && back.surface==material.surface,"every PK-9 field survives");
  AE_EXPECT_EQ(back.serialize(),text,"stable text");
  // v7: sem os 32 números do PK-9. Lit, Metallic e o transparente antigo (reto).
  auto old=dropTokens(text,32);old.replace(0,16,"ASTRA_MATERIAL 7");
  AE_EXPECT_TRUE(resources::MaterialAsset::deserialize(old,back),"v7 reads");
  AE_EXPECT_EQ(back.shading.shader,scene::MaterialShaderLit,"v7 is Lit");
  AE_EXPECT_TRUE(!back.shading.preserveSpecular,"v7 transparent keeps straight alpha");
  AE_EXPECT_TRUE(!back.extraTextures[0].valid(),"v7 has no height map");
  // Valores fora do contrato e "none" numa extra (o recurso não herda) recusam.
  auto bad=material;bad.shading.heightScale=.5f;AE_EXPECT_TRUE(!bad.valid(),"height above URP range refused");
  bad=material;bad.shading.sortingPriority=60;AE_EXPECT_TRUE(!bad.valid(),"priority above 50 refused");
  bad=material;bad.shading.cameraFadeFar=.1f;AE_EXPECT_TRUE(!bad.valid(),"fade far before near refused");
  bad=material;bad.extraTextures[1]=scene::MaterialTextureNone;AE_EXPECT_TRUE(!bad.valid(),"none extra refused");
  auto truncated=dropTokens(text,1);
  AE_EXPECT_TRUE(!resources::MaterialAsset::deserialize(truncated,back),"truncated v8 refused");
}

AE_TEST(pk9_override_flags_and_extension_rows_reach_the_shader) {
  renderer::MapMaterialRecord source{};source.flags=renderer::MapMaterialCullBackFaces;source.alphaCutoff=.5f;
  scene::MaterialParameters lit;
  AE_EXPECT_TRUE(!(renderer::applyMaterialOverride(source,lit).flags&renderer::MapMaterialShadingExtension),"default Lit stays on specialized pipelines");
  // Render Face Back: culling da frente.
  auto back=lit;back.sides=scene::MaterialSidesBack;
  const auto backFlags=renderer::applyMaterialOverride(source,back).flags;
  AE_EXPECT_TRUE((backFlags&renderer::MapMaterialCullFrontFaces)&&!(backFlags&renderer::MapMaterialCullBackFaces),"back face culls the front");
  AE_EXPECT_EQ(renderer::materialCullMode(backFlags),2u,"front culling mode");
  auto both=lit;both.sides=scene::MaterialSidesDouble;
  AE_EXPECT_EQ(renderer::materialCullMode(renderer::applyMaterialOverride(source,both).flags),0u,"both faces");
  AE_EXPECT_EQ(renderer::materialCullMode(source.flags),1u,"single face");
  // Transparente com Preserve Specular (padrão da Unity) vira premultiplicado.
  auto glass=lit;glass.alphaMode=scene::MaterialAlphaBlend;
  AE_EXPECT_TRUE(renderer::applyMaterialOverride(source,glass).flags&renderer::MapMaterialShadingExtension,"preserve specular needs the general pipeline");
  AE_EXPECT_EQ(renderer::materialBlendOutput(glass.shading),renderer::MaterialBlendOutputPremultiplied,"alpha + preserve = premultiplied output");
  glass.shading.preserveSpecular=false;
  AE_EXPECT_TRUE(!(renderer::applyMaterialOverride(source,glass).flags&renderer::MapMaterialShadingExtension),"straight alpha keeps the old path");
  glass.shading.blend=scene::MaterialBlendMultiply;
  AE_EXPECT_EQ(renderer::materialBlendOutput(glass.shading),renderer::MaterialBlendOutputMultiply,"multiply output");
  glass.shading.blend=scene::MaterialBlendAdditive;
  AE_EXPECT_EQ(renderer::materialBlendOutput(glass.shading),renderer::MaterialBlendOutputAdditive,"additive output");
  // Mistura e recorte transparente não valem no opaco.
  auto opaque=glass;opaque.alphaMode=scene::MaterialAlphaOpaque;opaque.shading.transparentClip=true;
  AE_EXPECT_TRUE(!(renderer::applyMaterialOverride(source,opaque).flags&renderer::MapMaterialShadingExtension),"blend options ignored when opaque");
  // Linhas 13..20 da extensão.
  scene::MaterialParameters value;value.shading=sample().shading;value.alphaMode=scene::MaterialAlphaBlend;value.alphaCutoff=.3f;
  value.extraTextures[scene::MaterialExtraHeight]=4;value.extraTextures[scene::MaterialExtraDetailNormal]=5;
  const auto flags=renderer::applyMaterialOverride(source,value).flags;
  AE_EXPECT_TRUE(flags&renderer::MapMaterialShadingExtension,"complex lit extended");
  float entry[renderer::MaterialExtensionFloats]{};
  const u32 slots[scene::MaterialExtraTextureCount]{40,renderer::MaterialExtensionNoTexture,renderer::MaterialExtensionNoTexture,41,42};
  renderer::materialExtensionEntry(value,flags,renderer::MaterialExtensionNoTexture,entry,renderer::MaterialExtensionNoTexture,slots);
  const auto row=[&](u32 r,u32 c) {return entry[r*4+c];};
  AE_EXPECT_EQ(row(13,0),static_cast<float>(scene::MaterialShaderComplexLit),"row 13.x shader");
  AE_EXPECT_EQ(row(13,1),1.f,"row 13.y specular workflow");
  AE_EXPECT_EQ(row(13,2),static_cast<float>(renderer::MaterialBlendOutputAdditive),"row 13.z additive");
  AE_EXPECT_EQ(row(13,3),static_cast<float>(8u|16u*0u|32u),"row 13.w: clip + clear coat; highlights, reflections, preserve off; no particle fade");
  AE_EXPECT_TRUE(std::abs(row(14,1)-.2f)<1e-6f && std::abs(row(14,3)-.3f)<1e-6f,"row 14 specular colour and cutoff");
  AE_EXPECT_EQ(row(15,0),40.f,"row 15.x height slot");AE_EXPECT_TRUE(std::abs(row(15,1)-.04f)<1e-6f,"row 15.y amplitude");
  AE_EXPECT_EQ(row(15,2),-1.f,"no detail mask");AE_EXPECT_EQ(row(16,0),41.f,"detail normal slot");AE_EXPECT_EQ(row(16,3),1.f,"detail uv1");
  AE_EXPECT_EQ(row(17,0),8.f,"detail tiling u");AE_EXPECT_EQ(row(18,1),6.f,"detail tiling v");AE_EXPECT_EQ(row(17,2),.25f,"detail offset u");
  AE_EXPECT_EQ(row(19,2),42.f,"clear coat map slot");AE_EXPECT_TRUE(std::abs(row(19,0)-.8f)<1e-6f,"clear coat mask");
  // Unlit não lê altura nem detalhe, mesmo com textura no recurso.
  value.shading.shader=scene::MaterialShaderUnlit;
  renderer::materialExtensionEntry(value,flags,renderer::MaterialExtensionNoTexture,entry,renderer::MaterialExtensionNoTexture,slots);
  AE_EXPECT_EQ(row(15,0),-1.f,"unlit ignores height");AE_EXPECT_EQ(row(19,2),-1.f,"unlit has no clear coat");
}

// No POCO o Multiply clareava: o shader lia as flags num uint mediump, a Adreno
// guardava 16 bits e o bit 20 (extensão PK-9) sumia. O host não reproduz a
// perda, então o contrato fica no texto do shader: toda flag acima do bit 15
// só pode ser lida por um caminho highp.
AE_TEST(pk9_material_flags_above_bit_15_are_read_in_highp) {
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"native/rhi/shaders/dirt_road_shading.glsl",bytes),"shader source");
  const std::string source(bytes.begin(),bytes.end());
  AE_EXPECT_TRUE(source.find("precision mediump int;")!=std::string::npos,"the file still defaults ints to mediump");
  AE_EXPECT_TRUE(renderer::MapMaterialShadingExtension>0xffffu,"the PK-9 flag lives above bit 15");
  AE_EXPECT_TRUE(source.find("const uint MATERIAL_SHADING_EXTENSION="+std::to_string(renderer::MapMaterialShadingExtension)+"u")!=std::string::npos,
                 "shader constant matches the CPU flag");
  AE_EXPECT_TRUE(source.find("bool hasMaterialFeature(highp uint flags,highp uint feature)")!=std::string::npos,"feature test is highp");
  AE_EXPECT_TRUE(source.find("highp uint flags=frame.materialFlags.x;")!=std::string::npos,"flags are loaded in highp");
  AE_EXPECT_TRUE(source.find("  uint flags=frame.materialFlags.x;")==std::string::npos,"no mediump copy of the flags");
}

AE_TEST(pk9_inspector_fields_follow_the_shader_like_urp) {
  auto material=sample();
  const auto visible=[&](std::string_view key) {const auto *field=findMaterialField(key);return field && field->visible(material);};
  material.shading.shader=scene::MaterialShaderLit;material.shading.workflow=scene::MaterialWorkflowMetallic;
  AE_EXPECT_TRUE(visible("pk9.metallic")&&!visible("pk9.specular_color")&&visible("pk9.detail_albedo_map")&&visible("pk9.height_scale"),"Lit metallic");
  AE_EXPECT_TRUE(!visible("pk9.clear_coat"),"clear coat is Complex Lit only");
  material.shading.workflow=scene::MaterialWorkflowSpecular;
  AE_EXPECT_TRUE(!visible("pk9.metallic")&&visible("pk9.specular_color")&&visible("pk9.specular_map"),"Lit specular");
  material.shading.shader=scene::MaterialShaderSimpleLit;
  AE_EXPECT_TRUE(visible("pk9.specular_color")&&!visible("pk9.detail_mask")&&!visible("pk9.workflow")&&!visible("pk9.environment_reflections"),"Simple Lit");
  material.shading.shader=scene::MaterialShaderUnlit;
  AE_EXPECT_TRUE(!visible("pk9.smoothness")&&!visible("pk9.normal_map")&&visible("pk9.base_map")&&visible("pk9.surface"),"Unlit");
  material.shading.shader=scene::MaterialShaderParticlesUnlit;
  AE_EXPECT_TRUE(visible("pk9.color_mode")&&visible("pk9.camera_fade_distance"),"Particles");
  material.shading.shader=scene::MaterialShaderComplexLit;
  AE_EXPECT_TRUE(visible("pk9.clear_coat")&&visible("pk9.clear_coat_mask"),"Complex Lit");
  // T-02: o Inspector mostra suavidade; o arquivo guarda rugosidade.
  const float half[3]{.8f,0,0};
  AE_EXPECT_TRUE(applyMaterialField(*findMaterialField("pk9.smoothness"),material,half) && std::abs(material.values.roughness-.2f)<1e-6f,"smoothness 0.8 = roughness 0.2");
  const float albedo[3]{1,0,0};
  AE_EXPECT_TRUE(applyMaterialField(*findMaterialField("pk9.smoothness_source"),material,albedo)&&
                 material.channels.roughnessSource==scene::MaterialRoughnessSmoothnessBaseAlpha,"source albedo alpha");
  const float metallicAlpha[3]{0,0,0};
  AE_EXPECT_TRUE(applyMaterialField(*findMaterialField("pk9.smoothness_source"),material,metallicAlpha)&&
                 material.channels.roughnessSource==scene::MaterialRoughnessSmoothnessMap&&material.channels.roughness==scene::MaterialChannelA,
                 "source metallic alpha reads A as smoothness");
  // Superfície: opaco com recorte é Mask; transparente com recorte guarda o recorte à parte.
  material.surface.alphaMode=scene::MaterialAlphaOpaque;material.shading.transparentClip=false;
  const float on[3]{1,0,0},off[3]{0,0,0};
  AE_EXPECT_TRUE(applyMaterialField(*findMaterialField("pk9.alpha_clip"),material,on)&&material.surface.alphaMode==scene::MaterialAlphaMask,"opaque clip = mask");
  AE_EXPECT_TRUE(applyMaterialField(*findMaterialField("pk9.surface"),material,on)&&material.surface.alphaMode==scene::MaterialAlphaBlend&&
                 material.shading.transparentClip,"transparent keeps clipping");
  AE_EXPECT_TRUE(applyMaterialField(*findMaterialField("pk9.surface"),material,off)&&material.surface.alphaMode==scene::MaterialAlphaMask,"back to opaque clip");
  const float tiling[3]{3,4,0};
  AE_EXPECT_TRUE(applyMaterialField(*findMaterialField("pk9.tiling"),material,tiling)&&material.sampling[0].scale[0]==3&&material.sampling[3].scale[1]==4,
                 "Tiling applies to every map (_BaseMap_ST)");
  const float tooMuch[3]{2,0,0};
  AE_EXPECT_TRUE(!applyMaterialField(*findMaterialField("pk9.height_scale"),material,tooMuch),"out of range refused without change");
  // Todo campo com leitura escreve de volta o próprio valor e continua válido.
  for(const auto &field:materialFieldSpecs) {
    if(!field.read||!field.write) continue;
    float value[3]{};field.read(material,value);
    if(field.kind==MaterialFieldKind::Segment&&value[0]<0) continue;
    auto copy=material;
    AE_EXPECT_TRUE(applyMaterialField(field,copy,value),std::string(field.key).c_str());
  }
}

AE_TEST(pk9_unity_import_accepts_parallax_detail_premultiply_and_urp) {
  resources::UnityMaterialImport result;
  // Standard: Transparent (premultiplicado) + paralaxe + detalhe ×2 no UV1.
  const auto props="    m_TexEnvs:\n"+texture("_MainTex","11111111111111111111111111111111",2)+
    texture("_ParallaxMap","22222222222222222222222222222222")+texture("_DetailMask","33333333333333333333333333333333")+
    texture("_DetailAlbedoMap","44444444444444444444444444444444",8)+texture("_DetailNormalMap","55555555555555555555555555555555",8)+
    "    m_Floats:\n    - _Mode: 3\n    - _SrcBlend: 1\n    - _DstBlend: 10\n    - _ZWrite: 0\n    - _Parallax: 0.03\n"
    "    - _DetailNormalMapScale: 0.6\n    - _UVSec: 1\n";
  AE_EXPECT_TRUE(resources::readUnityMaterial(unity(standard,props,"  - _ALPHAPREMULTIPLY_ON\n  - _PARALLAXMAP\n  - _DETAIL_MULX2\n"),result),result.diagnostic.c_str());
  AE_EXPECT_TRUE(result.supported(),result.unsupported.empty()?"supported":result.unsupported.front().c_str());
  const auto &s=result.material.shading;
  AE_EXPECT_EQ(result.material.surface.alphaMode,scene::MaterialAlphaBlend,"premultiplied is transparent");
  AE_EXPECT_TRUE(s.blend==scene::MaterialBlendAlpha&&s.preserveSpecular,"premultiplied = alpha with preserve specular");
  AE_EXPECT_TRUE(std::abs(s.heightScale-.03f)<1e-6f,"_Parallax");
  AE_EXPECT_TRUE(std::abs(s.detailNormalScale-.6f)<1e-6f&&s.detailSampling.uvSet==scene::MaterialUv1&&s.detailSampling.scale[0]==8,"detail scale, UV set and tiling");
  u32 extras=0;
  for(const auto &t:result.textures) if(t.binding>=scene::MaterialExtraTextureBindingBase) ++extras;
  AE_EXPECT_EQ(extras,4u,"height, mask, detail albedo and normal are dependencies");
  // URP Lit: transparente aditivo, só a face de trás, recorte, prioridade e especular.
  const auto urpProps="    m_TexEnvs:\n"+texture("_BaseMap","11111111111111111111111111111111")+texture("_SpecGlossMap","22222222222222222222222222222222")+
    "    m_Floats:\n    - _Surface: 1\n    - _Blend: 2\n    - _Cull: 1\n    - _AlphaClip: 1\n    - _Cutoff: 0.4\n    - _QueueOffset: -3\n"
    "    - _WorkflowMode: 0\n    - _Smoothness: 0.7\n    - _BlendModePreserveSpecular: 0\n    - _EnvironmentReflections: 0\n"
    "    m_Colors:\n    - _BaseColor: {r: 0.5, g: 0.6, b: 0.7, a: 1}\n    - _SpecColor: {r: 0.3, g: 0.3, b: 0.3, a: 1}\n";
  AE_EXPECT_TRUE(resources::readUnityMaterial(unity(urpLit,urpProps,"  - _SURFACE_TYPE_TRANSPARENT\n  - _ALPHATEST_ON\n  - _SPECULAR_SETUP\n"
                                                    "  - _METALLICSPECGLOSSMAP\n  - _ENVIRONMENTREFLECTIONS_OFF\n"),result)&&result.supported(),
                 result.unsupported.empty()?result.diagnostic.c_str():result.unsupported.front().c_str());
  const auto &u=result.material;
  AE_EXPECT_EQ(u.shading.shader,scene::MaterialShaderLit,"URP Lit");
  AE_EXPECT_EQ(u.shading.workflow,scene::MaterialWorkflowSpecular,"specular workflow");
  AE_EXPECT_TRUE(u.surface.alphaMode==scene::MaterialAlphaBlend&&u.shading.blend==scene::MaterialBlendAdditive&&!u.shading.preserveSpecular,"additive");
  AE_EXPECT_TRUE(u.shading.transparentClip&&std::abs(u.surface.alphaCutoff-.4f)<1e-6f,"transparent clipping");
  AE_EXPECT_EQ(u.surface.sides,scene::MaterialSidesBack,"_Cull 1 renders the back face");
  AE_EXPECT_EQ(static_cast<int>(u.shading.sortingPriority),-3,"_QueueOffset");
  AE_EXPECT_TRUE(!u.shading.environmentReflections&&u.shading.specularHighlights,"advanced options");
  AE_EXPECT_TRUE(u.channels.roughnessSource==scene::MaterialRoughnessSmoothnessMap&&std::abs(u.values.roughness-.3f)<1e-6f,"smoothness from map alpha × _Smoothness");
  AE_EXPECT_TRUE(std::abs(u.values.baseColor[1]-.6f)<1e-6f,"_BaseColor");
  bool specularMap=false;for(const auto &t:result.textures) if(t.property=="_SpecGlossMap") specularMap=t.binding==2;
  AE_EXPECT_TRUE(specularMap,"Specular Map on binding 2");
  // Simple Lit, Unlit e Complex Lit (keyword _CLEARCOAT).
  const std::string simpleLit="{fileID: 4800000, guid: 8d2bb70cbf9db8d4da26e15b26e74248, type: 3}";
  const std::string unlit="{fileID: 4800000, guid: 650dd9526735d5b46b79224bc6e94025, type: 3}";
  AE_EXPECT_TRUE(resources::readUnityMaterial(unity(simpleLit,"    m_Colors:\n    - _SpecColor: {r: 0.5, g: 0.4, b: 0.3, a: 1}\n","  - _SPECULAR_COLOR\n"),result)&&
                 result.supported()&&result.material.shading.shader==scene::MaterialShaderSimpleLit&&result.material.shading.specularHighlights&&
                 std::abs(result.material.shading.specularColor[0]-.5f)<1e-6f,"Simple Lit with specular colour");
  AE_EXPECT_TRUE(resources::readUnityMaterial(unity(unlit,"",""),result)&&result.supported()&&result.material.shading.shader==scene::MaterialShaderUnlit,"Unlit");
  AE_EXPECT_TRUE(resources::readUnityMaterial(unity(urpLit,"    m_Floats:\n    - _ClearCoatMask: 0.6\n    - _ClearCoatSmoothness: 0.8\n","  - _CLEARCOAT\n"),result)&&
                 result.supported()&&result.material.shading.shader==scene::MaterialShaderComplexLit&&result.material.shading.clearCoat&&
                 std::abs(result.material.shading.clearCoatMask-.6f)<1e-6f,"Complex Lit clear coat");
  // Incoerência entre o Inspector e a keyword continua recusada.
  AE_EXPECT_TRUE(resources::readUnityMaterial(unity(urpLit,"    m_Floats:\n    - _Surface: 1\n",""),result)&&!result.supported(),"_Surface without keyword refused");
}

AE_TEST(pk9_material_inspector_touch_edits_the_project_material) {
  Project project;
  std::vector<u8> fontBytes,iconBytes;ui::UiFont font;ui::UiIconAtlas icons;
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-font.aeuf",fontBytes)&&font.load(fontBytes),"font");
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-icons.aeui",iconBytes)&&icons.load(iconBytes),"icons");
  EditorSession session;session.initialize(&font,&icons);session.setSurface({0,0,1200,800},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"project");
  EditorImportTransaction::writeText(project.path/"Tinta.mat",unity(standard,"    m_Floats:\n    - _Glossiness: 0.5\n",""));
  std::string diagnostic;
  const auto guid=session.importUnityMaterial("Tinta.mat",diagnostic);
  AE_EXPECT_TRUE(guid.valid(),diagnostic.c_str());
  AE_EXPECT_TRUE(session.openMaterialInspector(guid),"inspector");
  session.update();
  const auto frame=[&](ui::UiDrawList &list,ui::UiInputRouter &router) {
    list.begin(session.screen().surface,font.metrics(ui::UiFontWeight::Regular));router.beginFrame();
    buildEditorScreen(session.screen(),ui::defaultTheme(),list,router);
  };
  const auto find=[&](ui::UiDrawList &list,std::string_view text) {
    ui::UiRect found{};
    for(const auto &command:list.commands()) if(command.kind==ui::UiPrimitive::Text&&list.textOf(command)==text) found=command.bounds;
    return found;
  };
  const auto findShader=[&](ui::UiDrawList &list,std::string_view shader) {
    auto rect=find(list,"Universal Render Pipeline/"+std::string(shader));
    return rect.isEmpty()?find(list,"URP/"+std::string(shader)):rect;
  };
  const auto tap=[&](ui::UiRect rect,u32 id,double time) {
    const ui::UiPoint point{rect.x+rect.width*.5f,rect.y+rect.height*.5f};
    session.handlePointer({id,ui::UiPointerPhase::Down,point,time});session.handlePointer({id,ui::UiPointerPhase::Up,point,time+.05});session.update();
  };
  ui::UiDrawList list;ui::UiInputRouter router;frame(list,router);
  AE_EXPECT_TRUE(!findShader(list,"Lit").isEmpty(),"shader card shows URP Lit");
  AE_EXPECT_TRUE(!find(list,"Suavidade").isEmpty()&&!find(list,"Smoothness").isEmpty(),"bilingual Smoothness row");
  if(const char *capture=std::getenv("ASTRA_PK9_CAPTURE")) AE_EXPECT_TRUE(captureUi(std::string(capture)+".lit.ppm",1200,800,list,font,icons),"capture Lit");
  // Tipo de superfície → Transparente pelo segmento.
  const auto transparent=find(list,"Transparente");
  AE_EXPECT_TRUE(!transparent.isEmpty(),"surface segment");
  tap(transparent,10,1);
  AE_EXPECT_EQ(session.findMaterialAsset(guid)->surface.alphaMode,scene::MaterialAlphaBlend,"touch made it transparent");
  frame(list,router);
  AE_EXPECT_TRUE(!find(list,"Mistura").isEmpty(),"blending mode appears only when transparent");
  tap(find(list,"Alfa"),11,2);frame(list,router);
  AE_EXPECT_TRUE(!find(list,"Aditivo").isEmpty(),"blending list open");
  tap(find(list,"Aditivo"),14,2.5);
  AE_EXPECT_EQ(session.findMaterialAsset(guid)->shading.blend,scene::MaterialBlendAdditive,"additive by touch");
  // Shader → Simple Lit pela lista.
  frame(list,router);
  tap(findShader(list,"Lit"),12,3);frame(list,router);
  const auto simple=find(list,"Simple Lit");
  AE_EXPECT_TRUE(!simple.isEmpty(),"shader list open");
  tap(simple,13,4);
  AE_EXPECT_EQ(session.findMaterialAsset(guid)->shading.shader,scene::MaterialShaderSimpleLit,"shader changed by touch");
  frame(list,router);
  AE_EXPECT_TRUE(!find(list,"Cor especular").isEmpty()&&find(list,"Metálico").isEmpty(),"Simple Lit swaps metallic for specular colour");
  if(const char *capture=std::getenv("ASTRA_PK9_CAPTURE")) AE_EXPECT_TRUE(captureUi(std::string(capture)+".simplelit.ppm",1200,800,list,font,icons),"capture Simple Lit");
  // Histórico: desfazer volta o shader, persistido no arquivo.
  AE_EXPECT_TRUE(session.history().undo(session.document())&&session.findMaterialAsset(guid)->shading.shader==scene::MaterialShaderLit,"undo shader");
  std::vector<u8> bytes;resources::MaterialAsset disk;
  AE_EXPECT_TRUE(EditorImportTransaction::read(project.path/"Tinta.mat.material",bytes)&&
                 resources::MaterialAsset::deserialize(std::string(bytes.begin(),bytes.end()),disk)&&disk.shading.blend==scene::MaterialBlendAdditive&&
                 disk.shading.shader==scene::MaterialShaderLit,"file follows the history");
  // Slider: arrastar a suavidade grava só ao soltar.
  session.update();frame(list,router);
  const float before=session.findMaterialAsset(guid)->values.roughness;
  ui::UiRect smooth{};
  for(const auto &[field,track]:session.layout().materialSliders)
    if(materialFieldSpecs[field].key=="pk9.smoothness") smooth=track;
  AE_EXPECT_TRUE(smooth.width>0,"smoothness track laid out");
  if(smooth.width>0) {
    const ui::UiPoint start{smooth.x+smooth.width*.5f,smooth.y+smooth.height*.5f},end{smooth.x+smooth.width*.9f,start.y};
    session.handlePointer({20,ui::UiPointerPhase::Down,start,5});
    session.handlePointer({20,ui::UiPointerPhase::Move,{start.x+20,start.y},5.05});
    session.handlePointer({20,ui::UiPointerPhase::Move,end,5.1});
    AE_EXPECT_EQ(session.findMaterialAsset(guid)->values.roughness,before,"dragging previews without writing");
    session.handlePointer({20,ui::UiPointerPhase::Up,end,5.15});session.update();
    AE_EXPECT_TRUE(std::abs(session.findMaterialAsset(guid)->values.roughness-.1f)<.02f,"release writes smoothness 0.9");
  }
}

// Projeto de validação no aparelho (ASTRA_PK9_PROJECT=<pasta>): o Cenário de
// referência (sol físico, exposição, vistas), a grade Metálico × Suavidade em
// esferas e cubos, um material com detalhe e altura e três transparentes
// (vidro com Preserve Specular, aditivo e multiplicar). As texturas são
// procedurais, geradas aqui: nada de pacote de terceiros entra no projeto.
namespace {
struct Image {u32 width=256,height=256;std::vector<u8> rgba=std::vector<u8>(256*256*4,255);};
bool writeTga(const std::filesystem::path &path,const Image &image) {
  std::vector<u8> bytes(18,0);
  bytes[2]=2;bytes[12]=static_cast<u8>(image.width&255);bytes[13]=static_cast<u8>(image.width>>8);
  bytes[14]=static_cast<u8>(image.height&255);bytes[15]=static_cast<u8>(image.height>>8);bytes[16]=32;bytes[17]=8|0x20;
  for(usize i=0;i<image.rgba.size();i+=4) {bytes.push_back(image.rgba[i+2]);bytes.push_back(image.rgba[i+1]);bytes.push_back(image.rgba[i]);bytes.push_back(image.rgba[i+3]);}
  return EditorImportTransaction::write(path,bytes);
}
float hashNoise(u32 x,u32 y) {u32 h=x*374761393u+y*668265263u;h=(h^(h>>13))*1274126177u;return static_cast<float>((h^(h>>16))&0xffff)/65535.0f;}
// Tijolos 4 × 8 com argamassa de 6 texels: altura 1 no tijolo, chanfro de 10 texels, 0 na argamassa.
float brickHeight(u32 x,u32 y) {
  const u32 row=y/32,shift=row%2?32:0;const u32 bx=(x+shift)%64,by=y%32;
  const float edge=static_cast<float>(std::min({bx,63-bx,by,31-by}));
  return std::clamp((edge-3.0f)/10.0f,0.0f,1.0f)*(.85f+.15f*hashNoise((x+shift)/64,row));
}
Image normalFrom(const std::function<float(u32,u32)> &height,float strength) {
  Image image;
  for(u32 y=0;y<256;++y) for(u32 x=0;x<256;++x) {
    const float dx=height((x+1)&255,y)-height((x+255)&255,y),dy=height(x,(y+1)&255)-height(x,(y+255)&255);
    // OpenGL (Y+ para cima da imagem, a convenção da Unity): +v da linha é para baixo.
    float n[3]{-dx*strength,dy*strength,1};const float l=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
    u8 *p=&image.rgba[(y*256+x)*4];
    for(u32 c=0;c<3;++c) p[c]=static_cast<u8>((n[c]/l*.5f+.5f)*255+.5f);
  }
  return image;
}
std::string meta(const char *guid) {return std::string("fileFormatVersion: 2\nguid: ")+guid+"\n";}
std::string urpMaterial(const std::string &name,const std::string &textures,const std::string &floats,const std::string &colors,const std::string &keywords) {
  return "%YAML 1.1\n--- !u!21 &2100000\nMaterial:\n  m_Name: "+name+"\n  m_Shader: "+urpLit+"\n  m_ValidKeywords:\n"+keywords+
         "  m_SavedProperties:\n    m_TexEnvs:\n"+textures+"    m_Floats:\n"+floats+"    m_Colors:\n"+colors;
}
} // namespace

AE_TEST(pk9_build_device_validation_project) {
  const char *target=std::getenv("ASTRA_PK9_PROJECT");
  if(!target) return;
  const std::filesystem::path root(target);
  std::error_code error;std::filesystem::remove_all(root,error);
  std::filesystem::create_directories(root/"scenes");std::filesystem::create_directories(root/"PK9");
  EditorImportTransaction::writeText(root/"project.json",
    "{\n  \"format\": \"ASTRA-PROJECT-1\",\n  \"resourceSource\": \"independent\",\n  \"project\": {\n    \"name\": \"PK9 Materiais\",\n"
    "    \"path\": \"\",\n    \"template\": \"empty\",\n    \"scenes\": 1,\n    \"assets\": 0\n  },\n  \"mainScene\": \"scenes/main.ascene\",\n"
    "  \"editorScene\": \"scenes/editor.aescene\"\n}\n");
  EditorImportTransaction::writeText(root/"scenes/main.ascene","{\n  \"format\": \"ASTRA-SCENE-1\",\n  \"template\": \"empty\",\n  \"nodes\": []\n}\n");
  // Texturas procedurais (TGA RGBA, 256²).
  Image albedo,height,detail,mask,glass;
  for(u32 y=0;y<256;++y) for(u32 x=0;x<256;++x) {
    const float h=brickHeight(x,y),n=hashNoise(x,y);
    u8 *a=&albedo.rgba[(y*256+x)*4];
    const float brick=.75f+.25f*hashNoise(x/64+(y/32)*7,y/32);
    a[0]=static_cast<u8>(h>0?(150+40*n)*brick:150);a[1]=static_cast<u8>(h>0?(70+20*n)*brick:146);a[2]=static_cast<u8>(h>0?(52+16*n)*brick:138);
    u8 *g=&height.rgba[(y*256+x)*4];g[0]=g[1]=g[2]=static_cast<u8>(h*255);
    // Detalhe: cinza sRGB 128 (neutro no ×2) com grão fino.
    u8 *d=&detail.rgba[(y*256+x)*4];d[0]=d[1]=d[2]=static_cast<u8>(std::clamp(128+(n-.5f)*110,0.0f,255.0f));
    // Máscara no alfa: detalhe em tudo, menos num círculo no meio.
    const float cx=x-127.5f,cy=y-127.5f;mask.rgba[(y*256+x)*4+3]=cx*cx+cy*cy<60*60?0:255;
    glass.rgba[(y*256+x)*4+3]=51;
  }
  const auto noiseHeight=[](u32 x,u32 y) {return hashNoise(x,y)*.5f+hashNoise(x/2,y/2)*.5f;};
  AE_EXPECT_TRUE(writeTga(root/"PK9/tijolo_cor.tga",albedo)&&writeTga(root/"PK9/tijolo_altura.tga",height)&&
                 writeTga(root/"PK9/tijolo_normal.tga",normalFrom(brickHeight,4))&&writeTga(root/"PK9/detalhe_cor.tga",detail)&&
                 writeTga(root/"PK9/detalhe_normal.tga",normalFrom(noiseHeight,3))&&writeTga(root/"PK9/detalhe_mascara.tga",mask)&&
                 writeTga(root/"PK9/vidro.tga",glass),"procedural textures");
  const char *guids[]{"a0000000000000000000000000000001","a0000000000000000000000000000002","a0000000000000000000000000000003",
                      "a0000000000000000000000000000004","a0000000000000000000000000000005","a0000000000000000000000000000006",
                      "a0000000000000000000000000000007"};
  const char *names[]{"tijolo_cor","tijolo_altura","tijolo_normal","detalhe_cor","detalhe_normal","detalhe_mascara","vidro"};
  for(u32 i=0;i<7;++i) EditorImportTransaction::writeText(root/("PK9/"+std::string(names[i])+".tga.meta"),meta(guids[i]));
  const auto tex=[&](const char *property,u32 index,float scale=1) {return texture(property,guids[index],scale);};
  const std::string gold="    - _BaseColor: {r: 0.95, g: 0.64, b: 0.37, a: 1}\n";
  // Grade: metálico 0..1 nas colunas, suavidade 0..1 nas linhas.
  std::vector<std::string> materials;
  for(u32 s=0;s<4;++s) for(u32 m=0;m<4;++m) {
    const auto name="Grade_M"+std::to_string(m)+"_S"+std::to_string(s);
    EditorImportTransaction::writeText(root/("PK9/"+name+".mat"),urpMaterial(name,"","    - _Smoothness: 0.5\n",gold,""));
    materials.push_back("PK9/"+name+".mat");
  }
  EditorImportTransaction::writeText(root/"PK9/Tijolo_Detalhe.mat",urpMaterial("Tijolo_Detalhe",
    tex("_BaseMap",0)+tex("_ParallaxMap",1)+tex("_BumpMap",2)+tex("_DetailAlbedoMap",3,4)+tex("_DetailNormalMap",4,4)+tex("_DetailMask",5),
    "    - _Smoothness: 0.35\n    - _Parallax: 0.05\n    - _DetailAlbedoMapScale: 1.6\n    - _DetailNormalMapScale: 1\n",
    "    - _BaseColor: {r: 1, g: 1, b: 1, a: 1}\n","  - _NORMALMAP\n  - _PARALLAXMAP\n  - _DETAIL_MULX2\n"));
  EditorImportTransaction::writeText(root/"PK9/Vidro.mat",urpMaterial("Vidro",tex("_BaseMap",6),
    "    - _Surface: 1\n    - _Blend: 0\n    - _Cull: 0\n    - _Metallic: 1\n    - _Smoothness: 0.95\n    - _BlendModePreserveSpecular: 1\n",
    "    - _BaseColor: {r: 0.85, g: 0.9, b: 1, a: 1}\n","  - _SURFACE_TYPE_TRANSPARENT\n"));
  EditorImportTransaction::writeText(root/"PK9/Aditivo.mat",urpMaterial("Aditivo",tex("_BaseMap",6),
    "    - _Surface: 1\n    - _Blend: 2\n    - _Smoothness: 0.5\n    - _BlendModePreserveSpecular: 0\n",
    "    - _BaseColor: {r: 1, g: 0.45, b: 0.1, a: 1}\n","  - _SURFACE_TYPE_TRANSPARENT\n"));
  EditorImportTransaction::writeText(root/"PK9/Multiplicar.mat",urpMaterial("Multiplicar",tex("_BaseMap",6),
    "    - _Surface: 1\n    - _Blend: 3\n    - _Smoothness: 0.5\n",
    "    - _BaseColor: {r: 0.3, g: 0.7, b: 1, a: 1}\n","  - _SURFACE_TYPE_TRANSPARENT\n"));
  EditorImportTransaction::writeText(root/"PK9/Verniz.mat",urpMaterial("Verniz","",
    "    - _Smoothness: 0.2\n    - _ClearCoatMask: 1\n    - _ClearCoatSmoothness: 0.95\n",
    "    - _BaseColor: {r: 0.62, g: 0.3, b: 0.12, a: 1}\n","  - _CLEARCOAT\n"));
  EditorImportTransaction::writeText(root/"PK9/Sem_Verniz.mat",urpMaterial("Sem_Verniz","","    - _Smoothness: 0.2\n",
    "    - _BaseColor: {r: 0.62, g: 0.3, b: 0.12, a: 1}\n",""));
  EditorSession session;session.initialize(nullptr,nullptr);
  AE_EXPECT_TRUE(session.setProjectDirectory(root.string().c_str()),"project");
  std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> records;
  AE_EXPECT_TRUE(renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,records)&&
                 session.importMap(draws,records,false,vertices,indices,0),"primitives");
  AE_EXPECT_TRUE(session.createSceneTemplate(0),"reference scene (sun, exposure, views)");
  std::string diagnostic;
  std::string failed;
  const auto import=[&](const std::string &path)->resources::AssetGuid {
    const auto guid=session.importUnityMaterial(path,diagnostic);
    if(!guid.valid()&&failed.empty()) failed=path+": "+diagnostic;
    return guid;
  };
  u32 sphere=~0u,cube=~0u;
  for(u32 i=0;i<16;++i) {
    const auto type=renderer::primitiveFromFlags(session.mapScene().materialFlagsForAsset(i));
    if(type==scene::PrimitiveType::Sphere&&sphere==~0u) sphere=i;
    if(type==scene::PrimitiveType::Cube&&cube==~0u) cube=i;
  }
  AE_EXPECT_TRUE(sphere!=~0u&&cube!=~0u,"sphere and cube assets");
  const auto group=session.document().createEntity(session.document().root(),EditorEntityKind::Folder,"PK-9");
  const auto place=[&](u32 asset,const char *name,float x,float y,float z,float size,const resources::AssetGuid &material) {
    const auto id=session.document().createEntity(group,EditorEntityKind::Mesh,name);
    auto object=*session.document().find(id);
    AE_EXPECT_TRUE(runtime::configurePrimitive(object,asset==sphere?scene::PrimitiveType::Sphere:scene::PrimitiveType::Cube,
                                               {1,session.mapScene().assetGuid(asset),session.mapScene().materialForAsset(asset)}),"primitive");
    object.transform.position[0]=x;object.transform.position[1]=y;object.transform.position[2]=z;
    for(auto &axis:object.transform.scale) axis=size;
    AE_EXPECT_TRUE(session.document().applyEntityValues(id,object)&&session.assignSlotMaterial(id,0,material),name);
  };
  // A grade fica no exterior, de frente para a vista "Exterior" (alvo 0,1,2.5).
  for(u32 s=0;s<4;++s) for(u32 m=0;m<4;++m) {
    const auto guid=import(materials[s*4+m]);
    session.openMaterialInspector(guid);
    const float metallic[3]{m/3.0f,0,0},smoothness[3]{s/3.0f,0,0};
    AE_EXPECT_TRUE(session.setMaterialField("pk9.metallic",metallic,diagnostic)&&session.setMaterialField("pk9.smoothness",smoothness,diagnostic),diagnostic.c_str());
    const auto label="M"+std::to_string(m)+" S"+std::to_string(s);
    place(sphere,("Esfera "+label).c_str(),-3.3f+m*1.1f,.55f+(3-s)*1.1f,6,.9f,guid);
    place(cube,("Cubo "+label).c_str(),1.6f+m*1.1f,.55f+(3-s)*1.1f,6,.75f,guid);
  }
  const auto bricks=import("PK9/Tijolo_Detalhe.mat");
  place(cube,"Tijolo com detalhe e altura",-2.2f,1.3f,9,2.4f,bricks);
  place(sphere,"Esfera de tijolo",.6f,1.1f,9,2.0f,bricks);
  place(sphere,"Vidro (Preserve Specular)",3,1,9,1.6f,import("PK9/Vidro.mat"));
  place(sphere,"Aditivo",5,1,9,1.2f,import("PK9/Aditivo.mat"));
  place(cube,"Multiplicar",3,1,7.6f,1.0f,import("PK9/Multiplicar.mat"));
  place(sphere,"Verniz (Complex Lit)",-5.2f,1,9,1.6f,import("PK9/Verniz.mat"));
  place(sphere,"Sem verniz (Lit)",-7.2f,1,9,1.6f,import("PK9/Sem_Verniz.mat"));
  AE_EXPECT_TRUE(failed.empty(),failed.c_str());
  // Câmera de Play enquadrando a grade (Play aplica exposição e pós do volume).
  const float eye[3]{.4f,2.4f,-1.6f};
  session.setCameraPose(eye,0,.06f);
  AE_EXPECT_TRUE(session.saveSceneView("PK-9 grade"),"saved view");
  u32 cameraRecipe=0;
  AE_EXPECT_TRUE(findCreationRecipe("basic.camera",&cameraRecipe)&&session.createRecipe(cameraRecipe,group)!=kInvalidEntity,"play camera");
  AE_EXPECT_TRUE(session.save((root/"scenes/editor.aescene").string().c_str(),0),"scene saved");
}
