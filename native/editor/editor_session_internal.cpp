#include "editor/editor_session_internal.h"
#include "editor/editor_widgets.h"
#include "scene/script_behavior.h"
#include "scene/import_link.h"
#include "scene/prefab_link.h"
#include "scene/lod_group.h"
#include "scene/skinned_mesh.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ae::editor::session_detail {
std::string baseName(const std::string &path) {
  const auto slash=path.find_last_of('/');
  return slash==std::string::npos?path:path.substr(slash+1);
}

// Perfil de importação de textura campo a campo, na ordem dos widgets
// TextureProfileInterpretation…StreamingPriority: rótulo, cópia e liga/desliga,
// comuns ao Inspector de uma textura e ao de várias.

bool textureProfileToggle(u32 field) {return field==2||field==3||field==4||field==5||field==6||field==8;}
std::string textureProfileFieldText(const resources::TextureProfile &profile,u32 field) {
  static constexpr const char *interpretations[]{"Tipo: pelo uso","Tipo: cor (sRGB)","Tipo: dado (linear)","Tipo: mapa normal"};
  switch(field) {
    case 0: return interpretations[std::min<u32>(profile.interpretation,3u)];
    case 1: return profile.maximumDimension?"Tamanho: até "+std::to_string(profile.maximumDimension)+" px":std::string("Tamanho: teto do projeto");
    case 2: return profile.mipmaps?"Mipmaps: sim":"Mipmaps: não";
    case 3: return profile.dilateEdges?"Bordas: sem halo":"Bordas: do arquivo";
    case 4: return profile.anisotropy?"Anisotropia: da qualidade":"Anisotropia: desligada";
    case 5: return profile.invertNormalGreen?"Normal Y: inverter (DX)":"Normal Y: manter (GL)";
    case 6: return profile.preserveAlphaCoverage?"Cobertura alfa: preservar":"Cobertura alfa: desligada";
    case 7: return "Corte da cobertura: "+std::to_string(static_cast<u32>(std::lround(profile.alphaCoverageCutoff*100.0f)))+"%";
    case 8: return profile.streamingMipmaps?"Streaming de mips: sim":"Streaming de mips: não";
    default: return "Prioridade: "+std::to_string(profile.streamingPriority);
  }
}
void copyTextureProfileField(resources::TextureProfile &to,const resources::TextureProfile &from,u32 field) {
  switch(field) {
    case 0: to.interpretation=from.interpretation;break;
    case 1: to.maximumDimension=from.maximumDimension;break;
    case 2: to.mipmaps=from.mipmaps;break;
    case 3: to.dilateEdges=from.dilateEdges;break;
    case 4: to.anisotropy=from.anisotropy;break;
    case 5: to.invertNormalGreen=from.invertNormalGreen;break;
    case 6: to.preserveAlphaCoverage=from.preserveAlphaCoverage;break;
    case 7: to.alphaCoverageCutoff=from.alphaCoverageCutoff;break;
    case 8: to.streamingMipmaps=from.streamingMipmaps;break;
    default: to.streamingPriority=from.streamingPriority;break;
  }
}
void setTextureProfileToggle(resources::TextureProfile &profile,u32 field,bool on) {
  switch(field) {
    case 2: profile.mipmaps=on;break;
    case 3: profile.dilateEdges=on;break;
    case 4: profile.anisotropy=on;break;
    case 5: profile.invertNormalGreen=on;break;
    case 6: profile.preserveAlphaCoverage=on;break;
    case 8: profile.streamingMipmaps=on;break;
    default: break;
  }
}
// Campos de um material do projeto como o Inspector os mostra (chaves de
// buildMaterialSlots): quais diferem entre dois materiais, e a cópia do que
// mudou (antes × depois) para outro — números pelos mesmos descritores do
// MeshRenderer que o Inspector lê e grava.
void materialDifferences(const resources::MaterialAsset &a,const resources::MaterialAsset &b,std::vector<std::string> &out) {
  const auto add=[&](bool differ,std::string key) {if(differ && std::find(out.begin(),out.end(),key)==out.end()) out.push_back(std::move(key));};
  for(u32 i=0;i<scene::MaterialTextureCount;++i) {
    add(!(a.textures[i]==b.textures[i]),"tex"+std::to_string(i));
    const auto key="uv"+std::to_string(i)+".";const auto &x=a.sampling[i],&y=b.sampling[i];
    add(x.uvSet!=y.uvSet,key+"set");add(x.wrap!=y.wrap,key+"wrap");add(x.filter!=y.filter,key+"filter");
    for(u32 k=0;k<2;++k) {add(x.offset[k]!=y.offset[k],key+"offset"+std::to_string(k));add(x.scale[k]!=y.scale[k],key+"scale"+std::to_string(k));}
    add(x.rotation!=y.rotation,key+"rotation");
  }
  add(a.surface.alphaMode!=b.surface.alphaMode,"alpha");add(a.surface.sides!=b.surface.sides,"sides");
  add(a.surface.alphaCutoff!=b.surface.alphaCutoff,"cutoff");
  add(a.channels.occlusionSource!=b.channels.occlusionSource,"ch0");add(!(a.occlusionTexture==b.occlusionTexture),"ch1");
  add(a.channels.occlusionStrength!=b.channels.occlusionStrength,"ch2");
  add(a.channels.roughness!=b.channels.roughness,"ch3.0");add(a.channels.metallic!=b.channels.metallic,"ch3.1");
  add(a.channels.occlusion!=b.channels.occlusion,"ch3.2");
  add(a.channels.normalFlipY!=b.channels.normalFlipY,"ch4");add(a.channels.alphaSource!=b.channels.alphaSource,"ch5");
  scene::MeshRenderer pa,pb;pa.material=a.values;pb.material=b.values;
  for(u32 field=0;field<scene::meshRendererNumbers.size();++field)
    add(scene::meshRendererNumbers[field].read(pa)!=scene::meshRendererNumbers[field].read(pb),"num"+std::to_string(field));
}
bool copyMaterialChange(const resources::MaterialAsset &before,const resources::MaterialAsset &after,
                        resources::MaterialAsset &target,std::string_view field) {
  bool changed=false;
  const auto copy=[&](auto &to,const auto &from,const auto &was,const std::string &key) {
    if((field==key || !(from==was)) && !(to==from)) {to=from;changed=true;}
  };
  for(u32 i=0;i<scene::MaterialTextureCount;++i) {
    const auto key="tex"+std::to_string(i),uv="uv"+std::to_string(i)+".";
    copy(target.textures[i],after.textures[i],before.textures[i],key);
    auto &to=target.sampling[i];const auto &from=after.sampling[i],&was=before.sampling[i];
    copy(to.uvSet,from.uvSet,was.uvSet,uv+"set");copy(to.wrap,from.wrap,was.wrap,uv+"wrap");
    copy(to.filter,from.filter,was.filter,uv+"filter");
    for(u32 k=0;k<2;++k) {
      copy(to.offset[k],from.offset[k],was.offset[k],field==uv+"reset"?std::string(field):uv+"offset"+std::to_string(k));
      copy(to.scale[k],from.scale[k],was.scale[k],field==uv+"reset"?std::string(field):uv+"scale"+std::to_string(k));
    }
    copy(to.rotation,from.rotation,was.rotation,field==uv+"reset"?std::string(field):uv+"rotation");
  }
  copy(target.surface.alphaMode,after.surface.alphaMode,before.surface.alphaMode,"alpha");
  copy(target.surface.sides,after.surface.sides,before.surface.sides,"sides");
  copy(target.surface.alphaCutoff,after.surface.alphaCutoff,before.surface.alphaCutoff,"cutoff");
  auto &ch=target.channels;const auto &now=after.channels,&old=before.channels;
  copy(ch.roughness,now.roughness,old.roughness,"ch3.0");copy(ch.metallic,now.metallic,old.metallic,"ch3.1");
  copy(ch.occlusion,now.occlusion,old.occlusion,"ch3.2");copy(ch.occlusionSource,now.occlusionSource,old.occlusionSource,"ch0");
  copy(ch.occlusionStrength,now.occlusionStrength,old.occlusionStrength,"ch2");
  copy(ch.normalFlipY,now.normalFlipY,old.normalFlipY,"ch4");copy(ch.alphaSource,now.alphaSource,old.alphaSource,"ch5");
  copy(target.occlusionTexture,after.occlusionTexture,before.occlusionTexture,"ch1");
  copy(target.values.enabled,after.values.enabled,before.values.enabled,field.starts_with("num")?std::string(field):"enabled");
  scene::MeshRenderer was,now2,to;was.material=before.values;now2.material=after.values;to.material=target.values;
  for(u32 i=0;i<scene::meshRendererNumbers.size();++i) {
    const auto &number=scene::meshRendererNumbers[i];const float value=number.read(now2);
    if((field=="num"+std::to_string(i) || value!=number.read(was)) && number.write && value!=number.read(to)) {
      *number.write(to)=value;changed=true;
    }
  }
  target.values=to.material;
  return changed;
}
std::string materialWidgetField(u32 widget,u32 binding) {
  if(widget>=widgetId(EditorWidget::MaterialNumberBase) && widget-widgetId(EditorWidget::MaterialNumberBase)<scene::meshRendererNumbers.size())
    return "num"+std::to_string(widget-widgetId(EditorWidget::MaterialNumberBase));
  if(widget>=widgetId(EditorWidget::MaterialTextureBase) && widget-widgetId(EditorWidget::MaterialTextureBase)<scene::MaterialTextureCount)
    return "tex"+std::to_string(widget-widgetId(EditorWidget::MaterialTextureBase));
  const auto uv="uv"+std::to_string(binding)+".";
  if(widget>=widgetId(EditorWidget::TextureUvStepBase) && widget-widgetId(EditorWidget::TextureUvStepBase)<10) {
    static constexpr const char *keys[]{"offset0","offset1","scale0","scale1","rotation"};
    return uv+keys[(widget-widgetId(EditorWidget::TextureUvStepBase))/2];
  }
  switch(static_cast<EditorWidget>(widget)) {
    case EditorWidget::MaterialAlphaCycle:return "alpha";
    case EditorWidget::MaterialSidesCycle:return "sides";
    case EditorWidget::MaterialCutoffDown:case EditorWidget::MaterialCutoffUp:return "cutoff";
    case EditorWidget::MaterialOcclusionSourceCycle:return "ch0";
    case EditorWidget::MaterialOcclusionTexture:return "ch1";
    case EditorWidget::MaterialOcclusionStrengthDown:case EditorWidget::MaterialOcclusionStrengthUp:return "ch2";
    case EditorWidget::MaterialChannelRoughness:return "ch3.0";
    case EditorWidget::MaterialChannelMetallic:return "ch3.1";
    case EditorWidget::MaterialChannelOcclusion:return "ch3.2";
    case EditorWidget::MaterialNormalFlipCycle:return "ch4";
    case EditorWidget::MaterialAlphaSourceCycle:return "ch5";
    case EditorWidget::TextureSamplingUv:return uv+"set";
    case EditorWidget::TextureSamplingWrap:return uv+"wrap";
    case EditorWidget::TextureSamplingFilter:return uv+"filter";
    default:return {};
  }
}
std::string materialFieldText(const resources::MaterialAsset &material,std::string_view key) {
  const auto number=[](float value) {char text[32];std::snprintf(text,sizeof(text),"%.6g",static_cast<double>(value));return std::string(text);};
  if(key.starts_with("num")) {
    const auto i=static_cast<u32>(std::stoul(std::string(key.substr(3))));
    scene::MeshRenderer probe;probe.material=material.values;
    return i<scene::meshRendererNumbers.size()?number(scene::meshRendererNumbers[i].read(probe)):std::string();
  }
  if(key=="alpha") {static constexpr const char *labels[]{"Como a fonte","Opaco","Recorte","Transparente"};return labels[material.surface.alphaMode];}
  if(key=="sides") {static constexpr const char *labels[]{"Como a fonte","Uma face","Dupla face"};return labels[material.surface.sides];}
  if(key=="cutoff") return number(material.surface.alphaCutoff);
  if(key=="ch0") {static constexpr const char *labels[]{"Como a fonte","Sem oclusão","Metal/rugosidade","Textura própria"};return labels[material.channels.occlusionSource];}
  if(key=="ch2") return material.channels.occlusionStrength<0?"Como a fonte":number(material.channels.occlusionStrength);
  if(key.starts_with("ch3.")) {
    const u8 values[]{material.channels.roughness,material.channels.metallic,material.channels.occlusion};
    static constexpr const char *labels[]{"Como a fonte","R","G","B","A"};return labels[values[key.back()-'0']];
  }
  if(key=="ch4") {static constexpr const char *labels[]{"Como a fonte","OpenGL","DirectX"};return labels[material.channels.normalFlipY];}
  if(key=="ch5") {static constexpr const char *labels[]{"Como a fonte","Alfa da cor base","Ignorar alfa","Luminância"};return labels[material.channels.alphaSource];}
  if(key.starts_with("uv") && key.size()>4 && key[2]>='0' && static_cast<u32>(key[2]-'0')<scene::MaterialTextureCount) {
    const auto &uv=material.sampling[key[2]-'0'];const auto field=key.substr(4);
    if(field=="set") {static constexpr const char *labels[]{"Herdar","UV 0","UV 1","Mundo"};return labels[uv.uvSet];}
    if(field=="wrap") {static constexpr const char *labels[]{"Herdar","Repetir","Limitar","Espelhar"};return labels[uv.wrap];}
    if(field=="filter") {static constexpr const char *labels[]{"Herdar","Linear","Próximo"};return labels[uv.filter];}
    if(field.starts_with("offset")) return number(uv.offset[field.back()-'0']);
    if(field.starts_with("scale")) return number(uv.scale[field.back()-'0']);
    if(field=="rotation") return number(uv.rotation);
  }
  return {};
}
// Tipos de arquivo que o Inspector de vários recursos agrupa (a ordem é a dos
// grupos na tela).
// Tipo de componente para casar entre objetos: o id do tipo, ou o tipo do
// script (todos os comportamentos C# compartilham o mesmo tipo nativo).
std::string componentKind(const scene::ComponentValue &value) {
  if(const auto *script=scene::scriptBehavior(&value)) return "script:"+script->scriptType;
  return std::string(value.type().id);
}
u32 componentOccurrence(const scene::Components &components,const scene::ComponentValue &value) {
  const auto kind=componentKind(value);u32 occurrence=0;
  for(usize i=0;i<components.size();++i) {
    const auto *other=components.at(i);if(other==&value) break;
    if(componentKind(*other)==kind) ++occurrence;
  }
  return occurrence;
}
const scene::ComponentValue *matchComponent(const scene::Components &components,std::string_view kind,u32 occurrence) {
  u32 seen=0;
  for(usize i=0;i<components.size();++i) if(componentKind(*components.at(i))==kind) {if(seen==occurrence) return components.at(i);++seen;}
  return nullptr;
}
// Componentes cujo editor é por objeto (vínculo com a fonte, barra do LOD,
// blend shapes da malha, tipo ausente): a Unity também não os edita juntos.
bool multiEditable(const scene::ComponentValue &value) {
  const auto &type=value.type();
  return &type!=&scene::ImportLink::descriptor && &type!=&scene::PrefabLink::descriptor && &type!=&scene::LodGroup::descriptor &&
         &type!=&scene::SkinnedMesh::descriptor && !value.unresolved();
}
std::string transformChannel(const EditorTransform &t,u32 row,u32 axis) {
  const float v=row==0?t.position[axis]:row==1?t.rotationDegrees[axis]:t.scale[axis];
  char text[32];std::snprintf(text,sizeof text,"%.4g",static_cast<double>(v));return text;
}
std::string propertyText(const scene::ComponentValue &value,std::string_view property) {
  char text[48];
  for(const auto &p:value.type().numbers) if(p.id==property) {std::snprintf(text,sizeof text,"%.5g",static_cast<double>(p.read(value)));return text;}
  for(const auto &p:value.type().booleans) if(p.id==property) return p.read(value)?"ligado":"desligado";
  for(const auto &p:value.type().enums) if(p.id==property) {
    const u32 current=p.read(value);for(const auto &o:p.options) if(o.value==current) return o.name;
    return std::to_string(current);
  }
  for(const auto &p:value.type().references) if(p.id==property) {const auto target=p.read(value);return target?"#"+std::to_string(target):"nenhum";}
  if(const auto *script=scene::scriptBehavior(&value))
    for(const auto &p:script->properties) if(p.id==property) return p.value;
  return {};
}

}
