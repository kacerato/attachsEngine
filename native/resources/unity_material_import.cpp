#include "resources/unity_material_import.h"
#include "scene/mesh_renderer.h"
#include <charconv>
#include <cmath>
#include <map>
#include <sstream>
#include <algorithm>
#include <set>
#include <tuple>

#include <cstdlib>
#include <cstring>
#include <string_view>
namespace {
// NDK libc++ nao implementa from_chars de ponto flutuante: strtod sobre
// copia terminada, recusando hex/inf/nan para manter o contrato decimal.
inline bool parseDecimal(std::string_view text,double &value) {
  if(text.empty()||text.size()>64||text.find_first_of("xXnNiI")!=text.npos)return false;
  char buffer[65];std::memcpy(buffer,text.data(),text.size());buffer[text.size()]=0;
  char *end=nullptr;value=std::strtod(buffer,&end);
  return end==buffer+text.size();
}
}
namespace ae::resources {
namespace {
std::string_view trim(std::string_view text) {
  const auto begin=text.find_first_not_of(" \t\r");
  return begin==text.npos?std::string_view():text.substr(begin,text.find_last_not_of(" \t\r")-begin+1);
}
bool number(std::string_view text,float &value) {
  text=trim(text);double parsed=0;
  if(!parseDecimal(text,parsed)||!std::isfinite(parsed))return false;
  value=static_cast<float>(parsed);return std::isfinite(value);
}
std::string_view member(std::string_view text,std::string_view key) {
  const auto at=text.find(std::string(key)+":");if(at==text.npos) return {};
  text=text.substr(at+key.size()+1);return trim(text.substr(0,text.find_first_of(",}")));
}
bool vector(std::string_view text,const char *const *keys,float *values,u32 count) {
  for(u32 i=0;i<count;++i) if(!number(member(text,keys[i]),values[i])) return false;
  return true;
}
}


namespace {
// Shaders com conversor. Embutidos do Built-in: Standard (fileID 46) e
// Standard (Specular setup) (45). URP 17, por GUID do .shader do pacote
// com.unity.render-pipelines.universal (fileID 4800000).
enum class UnityShader : u8 {None,Standard,StandardSpecular,UrpLit,UrpSimpleLit,UrpUnlit,UrpParticlesLit,UrpParticlesUnlit};
UnityShader shaderFrom(std::string_view guid,std::string_view id) {
  if(guid=="0000000000000000f000000000000000"||guid.empty()) return id=="46"?UnityShader::Standard:id=="45"?UnityShader::StandardSpecular:UnityShader::None;
  if(id!="4800000") return UnityShader::None;
  if(guid=="933532a4fcc9baf4fa0491de14d08ed7") return UnityShader::UrpLit;
  if(guid=="8d2bb70cbf9db8d4da26e15b26e74248") return UnityShader::UrpSimpleLit;
  if(guid=="650dd9526735d5b46b79224bc6e94025") return UnityShader::UrpUnlit;
  if(guid=="b7839dad95683814aa64166edc107ae2") return UnityShader::UrpParticlesLit;
  if(guid=="0406db5a14f94604a8c57ccfbc9f3b46") return UnityShader::UrpParticlesUnlit;
  return UnityShader::None;
}
bool urp(UnityShader shader) {return shader>=UnityShader::UrpLit;}
}

bool readUnityMaterial(std::string_view text,UnityMaterialImport &out) {
  out={};
  if(text.size()>256u*1024u || text.find('\0')!=text.npos) {out.diagnostic="Material YAML excede o limite ou contém dados binários.";return false;}
  UnityMaterialImport result;
  std::map<std::string,float> floats;
  std::map<std::string,std::array<float,4>> colors;
  std::map<std::string,UnityMaterialTexture> textures;
  enum class Section {None,Textures,Floats,Colors,Keywords,InvalidKeywords};Section section=Section::None;
  std::string keywords;
  std::string current;bool material=false;UnityShader shader=UnityShader::None;bool shaderSeen=false;
  std::istringstream lines{std::string(text)};std::string storage;
  const auto fail=[&](const std::string &why) {out={};out.diagnostic=why;return false;};
  while(std::getline(lines,storage)) {
    const auto line=trim(storage);if(line.empty()||line.starts_with('%')||line.starts_with("---")) continue;
    if(line=="Material:") {if(material) return fail("Mais de um Material no YAML.");material=true;continue;}
    if(line.starts_with("m_Name:")) {result.material.name=std::string(trim(line.substr(7)));continue;}
    if(line.starts_with("m_ShaderKeywords:")) {keywords+=" "+std::string(line.substr(17))+" ";continue;}
    if(line.starts_with("m_ValidKeywords:")) {keywords+=" "+std::string(line.substr(16))+" ";section=Section::Keywords;continue;}
    if(line.starts_with("m_InvalidKeywords:")) {section=Section::InvalidKeywords;continue;}
    if(line.starts_with("m_Shader:")) {
      shaderSeen=true;
      shader=shaderFrom(member(line,"guid"),member(line,"fileID"));
      if(shader==UnityShader::None) result.unsupported.push_back("Shader sem conversor: "+std::string(line.substr(9)));
      continue;
    }
    if(line.starts_with("m_TexEnvs:")) {section=Section::Textures;current.clear();continue;}
    if(line.starts_with("m_Floats:")||line.starts_with("m_Ints:")) {section=Section::Floats;continue;}
    if(line.starts_with("m_Colors:")) {section=Section::Colors;continue;}
    if(line.starts_with("- ")) {
      if(section==Section::Keywords) {keywords+=" "+std::string(line.substr(2))+" ";continue;}
      if(section==Section::InvalidKeywords) continue;
      const auto colon=line.find(':');if(colon==line.npos) return fail("Propriedade YAML sem valor.");
      current=std::string(line.substr(2,colon-2));const auto value=trim(line.substr(colon+1));
      if(section==Section::Textures) {if(textures.contains(current)) return fail("Textura duplicada: "+current);textures[current].property=current;}
      else if(section==Section::Floats) {
        float parsed;if(!number(value,parsed)||floats.contains(current)) return fail("Número inválido ou duplicado: "+current);
        floats[current]=parsed;
      } else if(section==Section::Colors) {
        std::array<float,4> parsed;const char *keys[]{"r","g","b","a"};
        if(!vector(value,keys,parsed.data(),4)||colors.contains(current)) return fail("Cor inválida ou duplicada: "+current);
        colors[current]=parsed;
      }
      continue;
    }
    if(section==Section::Textures&&!current.empty()) {
      auto &texture=textures[current];
      if(line.starts_with("m_Texture:")) {
        const auto id=member(line,"fileID"),guid=member(line,"guid");
        if(id!="0" && (id!="2800000"||!AssetGuid::parse(guid,texture.unityGuid)||!texture.unityGuid.valid()))
          return fail("Referência de textura não suportada: "+current);
      } else if(line.starts_with("m_Scale:")||line.starts_with("m_Offset:")) {
        const char *keys[]{"x","y"};auto *values=line.starts_with("m_Scale:")?texture.sampling.scale:texture.sampling.offset;
        if(!vector(line,keys,values,2)||!scene::validMaterialSampling(texture.sampling)) return fail("Transformação UV inválida: "+current);
      }
    }
  }
  if(!material||result.material.name.empty()) return fail("Não é um Material Unity textual válido.");
  if(!shaderSeen&&result.unsupported.empty()) result.unsupported.push_back("Material sem shader identificável.");
  auto &asset=result.material;asset.guid=assetGuidFromSeed("unity-material-parser");asset.values.enabled=true;
  asset.textures.fill(scene::MaterialTextureNone);asset.occlusionTexture=scene::MaterialTextureNone;
  asset.channels.occlusionSource=scene::MaterialOcclusionNone;asset.channels.normalFlipY=scene::MaterialToggleOff;
  asset.surface.alphaMode=scene::MaterialAlphaOpaque;asset.surface.sides=scene::MaterialSidesSingle;
  const auto get=[&](const char *key,float fallback) {const auto it=floats.find(key);return it==floats.end()?fallback:it->second;};
  const bool isUrp=urp(shader);
  // Shader variants consume complete keyword tokens. _Mode is Inspector
  // bookkeeping; conflicting serialized state must never change appearance.
  for(auto &character:keywords) if(character=='['||character==']'||character==','||character=='"'||character==39) character=' ';
  std::set<std::string> activeKeywords;std::istringstream keywordStream(keywords);std::string keyword;
  while(keywordStream>>keyword) activeKeywords.insert(keyword);
  const auto active=[&](const char *name) {return activeKeywords.contains(name);};
  const auto hasTexture=[&](const char *name) {return textures.contains(name)&&textures[name].unityGuid.valid();};
  // PK-9: o shader escolhido no Material.
  auto &shading=asset.shading;
  switch(shader) {
  case UnityShader::UrpSimpleLit: shading.shader=scene::MaterialShaderSimpleLit;break;
  case UnityShader::UrpUnlit: shading.shader=scene::MaterialShaderUnlit;break;
  case UnityShader::UrpParticlesLit: shading.shader=scene::MaterialShaderParticlesLit;break;
  case UnityShader::UrpParticlesUnlit: shading.shader=scene::MaterialShaderParticlesUnlit;break;
  default: shading.shader=active("_CLEARCOAT")?scene::MaterialShaderComplexLit:scene::MaterialShaderLit;break;
  }
  const bool specularSetup=shader==UnityShader::StandardSpecular||active("_SPECULAR_SETUP")||
                           (shader==UnityShader::UrpLit&&floats.contains("_WorkflowMode")&&get("_WorkflowMode",1)==0);
  if(specularSetup&&shading.shader!=scene::MaterialShaderSimpleLit) shading.workflow=scene::MaterialWorkflowSpecular;
  const char *colorName=isUrp?"_BaseColor":"_Color";
  const char *baseName=isUrp?"_BaseMap":"_MainTex";
  const auto color=colors.contains(colorName)?colors[colorName]:std::array<float,4>{1,1,1,1};
  std::copy_n(color.data(),3,asset.values.baseColor);
  // Standard.shader (UnityStandardInput.cginc, MetallicGloss) e URP
  // (LitInput.hlsl, SampleMetallicSpecGloss): com o mapa o metálico é o R
  // (ou a cor especular é o RGB) e a suavidade o A dele (ou o alfa do
  // albedo), vezes _GlossMapScale/_Smoothness; sem mapa, os escalares.
  // _Metallic é [Gamma] nos dois: o Inspector grava sRGB e o shader recebe linear.
  const std::string mapName=specularSetup||shader==UnityShader::UrpSimpleLit?"_SpecGlossMap":"_MetallicGlossMap";
  const char *mapKeyword=isUrp?(shader==UnityShader::UrpSimpleLit?"_SPECGLOSSMAP":"_METALLICSPECGLOSSMAP"):
                         specularSetup?"_SPECGLOSSMAP":"_METALLICGLOSSMAP";
  const bool glossMap=active(mapKeyword)&&hasTexture(mapName.c_str());
  const bool albedoSmoothness=active("_SMOOTHNESS_TEXTURE_ALBEDO_CHANNEL_A");
  if(active(mapKeyword)&&!glossMap) result.unsupported.push_back(std::string(mapKeyword)+" ativo sem textura "+mapName+".");
  if(get("_SmoothnessTextureChannel",albedoSmoothness?1.f:0.f)!=(albedoSmoothness?1.f:0.f))
    result.unsupported.push_back("_SmoothnessTextureChannel diverge da keyword efetiva do shader.");
  const auto linearFromGamma=[](float value) {
    return value<=.04045f?value/12.92f:std::pow((value+.055f)/1.055f,2.4f);
  };
  const float smoothnessScale=std::clamp(isUrp?get("_Smoothness",.5f):get("_GlossMapScale",1),0.f,1.f);
  const float smoothnessValue=std::clamp(isUrp?get("_Smoothness",.5f):get("_Glossiness",.5f),0.f,1.f);
  if(specularSetup||shader==UnityShader::UrpSimpleLit) {
    const auto specular=colors.contains("_SpecColor")?colors["_SpecColor"]:std::array<float,4>{.2f,.2f,.2f,1};
    for(u32 i=0;i<3;++i) shading.specularColor[i]=std::clamp(specular[i],0.f,1.f);
    asset.values.metallic=0;
    // Com o mapa, o RGB dele é a cor especular inteira (vezes branco).
    if(glossMap) for(auto &channel:shading.specularColor) channel=1;
  } else if(glossMap) {
    asset.values.metallic=1;asset.channels.metallic=scene::MaterialChannelR;
  } else asset.values.metallic=linearFromGamma(std::clamp(get("_Metallic",0),0.f,1.f));
  if(albedoSmoothness) {
    asset.values.roughness=1-smoothnessScale;asset.channels.roughnessSource=scene::MaterialRoughnessSmoothnessBaseAlpha;
  } else if(glossMap) {
    asset.values.roughness=1-smoothnessScale;asset.channels.roughness=scene::MaterialChannelA;
    asset.channels.roughnessSource=scene::MaterialRoughnessSmoothnessMap;
  } else asset.values.roughness=1-smoothnessValue;
  asset.values.normalScale=get("_BumpScale",1);asset.surface.alphaCutoff=get("_Cutoff",.5f);
  const bool emission=active("_EMISSION");
  if(emission&&colors.contains("_EmissionColor")) {
    const auto &emitted=colors["_EmissionColor"];
    const auto peak=std::max({1.0f,emitted[0],emitted[1],emitted[2]});
    asset.values.emissionStrength=peak;
    for(u32 i=0;i<3;++i) asset.values.emission[i]=emitted[i]/peak;
  }
  if(isUrp) {
    // URP Surface Options: _Surface, _Blend, _Cull, _AlphaClip e _QueueOffset,
    // conferidos contra as keywords efetivas.
    const bool transparentSurface=get("_Surface",0)>.5f;
    const bool alphaClip=active("_ALPHATEST_ON");
    if((get("_AlphaClip",alphaClip?1.f:0.f)>.5f)!=alphaClip) result.unsupported.push_back("_AlphaClip diverge da keyword _ALPHATEST_ON.");
    if(transparentSurface!=active("_SURFACE_TYPE_TRANSPARENT"))
      result.unsupported.push_back("_Surface diverge da keyword _SURFACE_TYPE_TRANSPARENT.");
    if(transparentSurface) {
      asset.surface.alphaMode=scene::MaterialAlphaBlend;shading.transparentClip=alphaClip;
      const int blend=static_cast<int>(get("_Blend",0)+.5f);
      if(blend<0||blend>3) result.unsupported.push_back("_Blend fora dos modos Alpha/Premultiply/Additive/Multiply.");
      shading.blend=static_cast<std::uint8_t>(std::clamp(blend,0,3));
      shading.preserveSpecular=get("_BlendModePreserveSpecular",1)>.5f;
    } else if(alphaClip) asset.surface.alphaMode=scene::MaterialAlphaMask;
    // _Cull: 0 Off (Both), 1 Front (desenha a de trás), 2 Back (desenha a da frente).
    const int cull=static_cast<int>(get("_Cull",2)+.5f);
    asset.surface.sides=cull==0?scene::MaterialSidesDouble:cull==1?scene::MaterialSidesBack:scene::MaterialSidesSingle;
    shading.sortingPriority=static_cast<std::int8_t>(std::clamp(static_cast<int>(get("_QueueOffset",0)),-50,50));
    shading.specularHighlights=!active("_SPECULARHIGHLIGHTS_OFF")&&get("_SpecularHighlights",1)>.5f;
    shading.environmentReflections=!active("_ENVIRONMENTREFLECTIONS_OFF")&&get("_EnvironmentReflections",1)>.5f;
    // Simple Lit: o brilho existe com Specular Color/Map (keyword _SPECULAR_COLOR ou _SPECGLOSSMAP).
    if(shading.shader==scene::MaterialShaderSimpleLit) shading.specularHighlights=active("_SPECGLOSSMAP")||active("_SPECULAR_COLOR");
    if(color[3]!=1&&transparentSurface) result.unsupported.push_back("Alfa multiplicado pela cor do material ainda não possui consumidor Astra.");
    asset.channels.alphaSource=(!transparentSurface&&!alphaClip)||albedoSmoothness?scene::MaterialAlphaSourceOpaque:scene::MaterialAlphaSourceBase;
    if(active("_RECEIVE_SHADOWS_OFF")||get("_ReceiveShadows",1)<.5f)
      result.approximations.push_back("Receive Shadows do material desligado: na Astra ele é do Mesh Renderer (desligue lá).");
    if(scene::materialShaderIsParticle(shading.shader)) {
      // _ColorMode: Multiply, Additive, Subtract, Overlay, Color, Difference.
      shading.particleColorMode=static_cast<std::uint8_t>(std::clamp(static_cast<int>(get("_ColorMode",0)+.5f),0,5));
      shading.cameraFading=active("_FADING_ON");
      shading.cameraFadeNear=std::clamp(get("_CameraNearFadeDistance",1),0.f,1000.f);
      shading.cameraFadeFar=std::clamp(get("_CameraFarFadeDistance",2),shading.cameraFadeNear,1000.f);
      if(active("_SOFTPARTICLES_ON")) result.approximations.push_back("Soft Particles ainda sem consumidor (precisa do depth da cena).");
      if(active("_FLIPBOOKBLENDING_ON")) result.approximations.push_back("Flip-Book Blending fica com o PK-10 (Texture Sheet Animation).");
    }
    if(shading.shader==scene::MaterialShaderComplexLit) {
      shading.clearCoat=true;
      shading.clearCoatMask=std::clamp(get("_ClearCoatMask",0),0.f,1.f);
      shading.clearCoatSmoothness=std::clamp(get("_ClearCoatSmoothness",0),0.f,1.f);
    }
  } else {
    const bool alphaTest=active("_ALPHATEST_ON"),alphaBlend=active("_ALPHABLEND_ON"),premultiplied=active("_ALPHAPREMULTIPLY_ON");
    const u32 effectiveMode=alphaTest?1:alphaBlend?2:premultiplied?3:0;
    // No modo opaco o alfa não chega à tela; nos outros a Astra não multiplica
    // alfa da cor do material.
    if(color[3]!=1&&effectiveMode!=0) result.unsupported.push_back("Alfa multiplicado pela cor do material ainda não possui consumidor Astra.");
    if(static_cast<u32>(alphaTest)+alphaBlend+premultiplied>1) result.unsupported.push_back("Keywords alfa simultâneas e incoerentes.");
    if(floats.contains("_Mode")&&get("_Mode",0)!=effectiveMode) result.unsupported.push_back("_Mode diverge das keywords alfa efetivas do shader.");
    if(alphaTest) asset.surface.alphaMode=scene::MaterialAlphaMask;
    // Fade: alfa reto, o brilho some junto. Transparent (premultiplicado): o
    // difuso vezes alfa e o brilho inteiro -- o Preserve Specular da URP.
    else if(alphaBlend||premultiplied) {
      asset.surface.alphaMode=scene::MaterialAlphaBlend;
      shading.blend=scene::MaterialBlendAlpha;shading.preserveSpecular=premultiplied;
    }
    // Com a suavidade no alfa do albedo o Standard usa só _Color.a como alfa;
    // _Color.a diferente de 1 fora do opaco já foi recusado acima.
    asset.channels.alphaSource=(effectiveMode==0||albedoSmoothness)?scene::MaterialAlphaSourceOpaque:scene::MaterialAlphaSourceBase;
    // Forward Rendering Options: cada uma tem consumidor próprio desde o PK-9.
    shading.specularHighlights=!(active("_SPECULARHIGHLIGHTS_OFF")||get("_SpecularHighlights",1)==0);
    shading.environmentReflections=!(active("_GLOSSYREFLECTIONS_OFF")||get("_GlossyReflections",1)==0);
    const float expectedSrc=alphaBlend?5.f:1.f,expectedDst=(alphaBlend||premultiplied)?10.f:0.f,expectedDepth=(alphaBlend||premultiplied)?0.f:1.f;
    for(const auto &[name,expected,fallback]:std::array<std::tuple<const char*,float,float>,3>{{{"_SrcBlend",expectedSrc,1},{"_DstBlend",expectedDst,0},{"_ZWrite",expectedDepth,1}}})
      if(get(name,fallback)!=expected) result.unsupported.push_back(std::string("Estado de blend/profundidade personalizado ou incoerente: ")+name);
  }
  // Height Map (_PARALLAXMAP): altura no G, amplitude _Parallax.
  const bool parallax=active("_PARALLAXMAP");
  if(parallax) shading.heightScale=std::clamp(get("_Parallax",isUrp?.005f:.02f),0.f,.1f);
  // Detail Inputs (_DETAIL_MULX2, _DETAIL_SCALED na URP): máscara, Base Map
  // ×2 e Normal, com o _DetailAlbedoMap_ST e, no Standard, o _UVSec.
  const bool detailMaps=active("_DETAIL_MULX2")||active("_DETAIL_SCALED");
  if(detailMaps) {
    shading.detailAlbedoScale=std::clamp(get("_DetailAlbedoMapScale",1),0.f,2.f);
    shading.detailNormalScale=std::clamp(get("_DetailNormalMapScale",1),0.f,2.f);
    if(textures.contains("_DetailAlbedoMap")) {
      const auto &st=textures["_DetailAlbedoMap"].sampling;
      std::copy_n(st.scale,2,shading.detailSampling.scale);std::copy_n(st.offset,2,shading.detailSampling.offset);
    }
    shading.detailSampling.uvSet=!isUrp&&get("_UVSec",0)>.5f?scene::MaterialUv1:scene::MaterialUv0;
  }
  if(!scene::materialShaderHasSurfaceDetail(shading.shader)&&(parallax||detailMaps))
    result.approximations.push_back("Altura e detalhe são do Lit; este shader não os lê.");
  for(auto &[property,texture]:textures) {
    // "<noninit>" é o nome que a Unity 5 deixava em propriedade sem nome; o
    // shader nunca a amostra (MapMagic 2, Demo/Stones.mat).
    if(!texture.unityGuid.valid()||property=="<noninit>") continue;
    if(property=="_EmissionMap"&&!emission) continue;
    if(property=="_BumpMap"&&!active("_NORMALMAP")) continue;
    if(property==mapName&&!glossMap) continue;
    if((property=="_MetallicGlossMap"||property=="_SpecGlossMap")&&property!=mapName) continue;
    if(property=="_ParallaxMap"&&!parallax) continue;
    if((property=="_DetailMask"||property=="_DetailAlbedoMap"||property=="_DetailNormalMap")&&!detailMaps) continue;
    if(property=="_ClearCoatMap"&&!active("_CLEARCOATMAP")) continue;
    if(isUrp&&property=="_OcclusionMap"&&!active("_OCCLUSIONMAP")) continue;
    bool detailTexture=false;
    if(property==baseName) texture.binding=0;
    else if(property=="_BumpMap") texture.binding=1;
    else if(property==mapName) texture.binding=2;
    else if(property=="_EmissionMap") texture.binding=3;
    else if(property=="_OcclusionMap") {texture.binding=scene::MaterialOcclusionTextureBinding;asset.channels.occlusionSource=scene::MaterialOcclusionTexture;
      asset.channels.occlusion=scene::MaterialChannelG;asset.channels.occlusionStrength=get("_OcclusionStrength",1);}
    else if(property=="_ParallaxMap") texture.binding=scene::MaterialExtraTextureBindingBase+scene::MaterialExtraHeight;
    else if(property=="_DetailMask") texture.binding=scene::MaterialExtraTextureBindingBase+scene::MaterialExtraDetailMask;
    else if(property=="_DetailAlbedoMap") {texture.binding=scene::MaterialExtraTextureBindingBase+scene::MaterialExtraDetailAlbedo;detailTexture=true;}
    else if(property=="_DetailNormalMap") {texture.binding=scene::MaterialExtraTextureBindingBase+scene::MaterialExtraDetailNormal;detailTexture=true;}
    else if(property=="_ClearCoatMap") texture.binding=scene::MaterialExtraTextureBindingBase+scene::MaterialExtraClearCoat;
    else {result.unsupported.push_back("Textura requer conversão de shader/canais: "+property);continue;}
    // Os mapas primários usam o _MainTex_ST (_BaseMap_ST na URP); a Unity
    // grava escala/deslocamento em cada TexEnv mesmo sem usar. O detalhe usa
    // o _DetailAlbedoMap_ST, que já está no shading.
    if(detailTexture) texture.sampling=shading.detailSampling;
    else {
      texture.sampling=textures.contains(baseName)?textures[baseName].sampling:scene::MaterialSampling{};
      texture.sampling.uvSet=scene::MaterialUv0;
    }
    result.textures.push_back(texture);
  }
  if(!asset.valid()) return fail("Material possui valores fora do contrato nativo (cor, emissão, escala, metálico ou UV).");
  out=std::move(result);return true;
}
} // namespace ae::resources
