#include "resources/unity_material_import.h"
#include "scene/mesh_renderer.h"
#include <charconv>
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
  if(!parseDecimal(text,parsed)||!std::isfinite(parsed))return false;value=static_cast<float>(parsed);return std::isfinite(value);
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

bool readUnityMaterial(std::string_view text,UnityMaterialImport &out) {
  out={};
  if(text.size()>256u*1024u || text.find('\0')!=text.npos) {out.diagnostic="Material YAML excede o limite ou contém dados binários.";return false;}
  UnityMaterialImport result;
  std::map<std::string,float> floats;
  std::map<std::string,std::array<float,4>> colors;
  std::map<std::string,UnityMaterialTexture> textures;
  enum class Section {None,Textures,Floats,Colors,Keywords,InvalidKeywords};Section section=Section::None;
  std::string keywords;
  std::string current;bool material=false,shader=false;
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
      // Standard metallic is the built-in shader with local fileID 46.
      const auto guid=member(line,"guid"),id=member(line,"fileID");
      shader=id=="46" && (guid=="0000000000000000f000000000000000" || guid.empty());
      if(!shader) result.unsupported.push_back("Shader sem conversor: "+std::string(line.substr(9)));
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
  if(!shader&&result.unsupported.empty()) result.unsupported.push_back("Material sem shader identificável.");
  auto &asset=result.material;asset.guid=assetGuidFromSeed("unity-material-parser");asset.values.enabled=true;
  asset.textures.fill(scene::MaterialTextureNone);asset.occlusionTexture=scene::MaterialTextureNone;
  asset.channels.occlusionSource=scene::MaterialOcclusionNone;asset.channels.normalFlipY=scene::MaterialToggleOff;
  asset.surface.alphaMode=scene::MaterialAlphaOpaque;asset.surface.sides=scene::MaterialSidesSingle;
  const auto get=[&](const char *key,float fallback) {const auto it=floats.find(key);return it==floats.end()?fallback:it->second;};
  // Shader variants consume complete keyword tokens. _Mode is Inspector
  // bookkeeping; conflicting serialized state must never change appearance.
  for(auto &character:keywords) if(character=='['||character==']'||character==','||character=='\"'||character=='\'') character=' ';
  std::set<std::string> activeKeywords;std::istringstream keywordStream(keywords);std::string keyword;
  while(keywordStream>>keyword) activeKeywords.insert(keyword);
  const auto active=[&](const char *name) {return activeKeywords.contains(name);};
  for(const auto *name:{"_SPECULARHIGHLIGHTS_OFF","_GLOSSYREFLECTIONS_OFF","_SMOOTHNESS_TEXTURE_ALBEDO_CHANNEL_A","_METALLICGLOSSMAP","_PARALLAXMAP","_DETAIL_MULX2"})
    if(active(name)) result.unsupported.push_back(std::string("Variante de shader sem conversor equivalente: ")+name);
  const auto color=colors.contains("_Color")?colors["_Color"]:std::array<float,4>{1,1,1,1};
  std::copy_n(color.data(),3,asset.values.baseColor);
  if(color[3]!=1) result.unsupported.push_back("Alfa multiplicado pela cor do material ainda não possui consumidor Astra.");
  asset.values.roughness=1-get("_Glossiness",.5f);asset.values.metallic=get("_Metallic",0);
  asset.values.normalScale=get("_BumpScale",1);asset.surface.alphaCutoff=get("_Cutoff",.5f);
  const bool emission=active("_EMISSION");
  if(emission&&colors.contains("_EmissionColor")) {
    const auto &color=colors["_EmissionColor"];
    const auto peak=std::max({1.0f,color[0],color[1],color[2]});
    asset.values.emissionStrength=peak;
    for(u32 i=0;i<3;++i) asset.values.emission[i]=color[i]/peak;
  }
  const bool alphaTest=active("_ALPHATEST_ON"),alphaBlend=active("_ALPHABLEND_ON"),premultiplied=active("_ALPHAPREMULTIPLY_ON");
  const u32 effectiveMode=alphaTest?1:alphaBlend?2:premultiplied?3:0;
  if(static_cast<u32>(alphaTest)+alphaBlend+premultiplied>1) result.unsupported.push_back("Keywords alfa simultâneas e incoerentes.");
  if(floats.contains("_Mode")&&get("_Mode",0)!=effectiveMode) result.unsupported.push_back("_Mode diverge das keywords alfa efetivas do shader.");
  if(premultiplied) result.unsupported.push_back("Alfa premultiplicado ainda não possui consumidor equivalente.");
  if(alphaTest) asset.surface.alphaMode=scene::MaterialAlphaMask;
  else if(alphaBlend) asset.surface.alphaMode=scene::MaterialAlphaBlend;
  asset.channels.alphaSource=effectiveMode==0?scene::MaterialAlphaSourceOpaque:scene::MaterialAlphaSourceBase;
  const float expectedSrc=alphaBlend?5.f:1.f,expectedDst=(alphaBlend||premultiplied)?10.f:0.f,expectedDepth=(alphaBlend||premultiplied)?0.f:1.f;
  for(const auto &[name,expected,fallback]:std::array<std::tuple<const char*,float,float>,3>{{{"_SrcBlend",expectedSrc,1},{"_DstBlend",expectedDst,0},{"_ZWrite",expectedDepth,1}}})
    if(get(name,fallback)!=expected) result.unsupported.push_back(std::string("Estado de blend/profundidade personalizado ou incoerente: ")+name);
  for(auto &[property,texture]:textures) {
    if(!texture.unityGuid.valid()) continue;
    if(property=="_EmissionMap"&&!emission) continue;
    if(property=="_BumpMap"&&!active("_NORMALMAP")) continue;
    if(property=="_MainTex") texture.binding=0;
    else if(property=="_BumpMap") texture.binding=1;
    else if(property=="_EmissionMap") texture.binding=3;
    else if(property=="_OcclusionMap") {texture.binding=scene::MaterialOcclusionTextureBinding;asset.channels.occlusionSource=scene::MaterialOcclusionTexture;
      asset.channels.occlusion=scene::MaterialChannelG;asset.channels.occlusionStrength=get("_OcclusionStrength",1);}
    else {result.unsupported.push_back("Textura requer conversão de shader/canais: "+property);continue;}
    // Built-in Standard samples every primary map using _MainTex_ST. Unity
    // serializes scale/offset on each TexEnv even though those are not used.
    texture.sampling=textures.contains("_MainTex")?textures["_MainTex"].sampling:scene::MaterialSampling{};
    texture.sampling.uvSet=scene::MaterialUv0;result.textures.push_back(texture);
  }
  for(const auto &[key,value]:floats) {
    if((key=="_DetailNormalMapScale"&&textures["_DetailNormalMap"].unityGuid.valid())||
       (key=="_SpecularHighlights"&&value==0)||(key=="_GlossyReflections"&&value==0)||
       (key=="_SmoothnessTextureChannel"&&value!=0)) result.unsupported.push_back("Propriedade sem conversão equivalente: "+key);
  }
  if(!asset.valid()) return fail("Material possui valores fora do contrato nativo (cor, emissão, escala, metálico ou UV).");
  out=std::move(result);return true;
}
} // namespace ae::resources
