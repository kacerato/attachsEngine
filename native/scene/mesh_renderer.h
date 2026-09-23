#pragma once
#include "resources/asset_registry.h"
#include "scene/components.h"
#include "scene/material_parameters.h"
#include <array>
#include <vector>
namespace ae::scene {
// R4: textura de um binding de material, por identidade. Inválida herda (do
// material compartilhado, senão da fonte); `MaterialTextureNone` tira a textura.
inline constexpr resources::AssetGuid MaterialTextureNone{~0ull,~0ull};
using SlotTextures=std::array<resources::AssetGuid,MaterialTextureCount>;
// No arquivo: "-" herda, "none" sem textura, senão a identidade.
inline std::string materialTextureToken(const resources::AssetGuid &value) {
  if(value==MaterialTextureNone) return "none";
  return value.valid()?value.text():std::string("-");
}
inline bool parseMaterialTextureToken(const std::string &text,resources::AssetGuid &out) {
  out={};
  if(text=="-") return true;
  if(text=="none") {out=MaterialTextureNone;return true;}
  return resources::AssetGuid::parse(text,out);
}
// R4: amostragem de cada binding. No arquivo: três dígitos "uvf" (UV, repetição, filtro; 0 herda).
using SlotSampling=std::array<MaterialSampling,MaterialTextureCount>;
inline std::string materialSamplingToken(const MaterialSampling &value) {
  return {static_cast<char>('0'+value.uvSet),static_cast<char>('0'+value.wrap),static_cast<char>('0'+value.filter)};
}
inline bool parseMaterialSamplingToken(const std::string &text,MaterialSampling &out) {
  if(text.size()!=3) return false;
  for(char c:text) if(c<'0' || c>'9') return false;
  out={static_cast<std::uint8_t>(text[0]-'0'),static_cast<std::uint8_t>(text[1]-'0'),static_cast<std::uint8_t>(text[2]-'0')};
  return validMaterialSampling(out);
}

// Um slot de submesh além do primeiro (M07/M08, Entrega 2).
//
// Um nó com várias primitivas é UM objeto autoral: cada primitiva vira um slot
// deste mesmo componente, e não um filho inventado. O slot guarda a identidade
// da primitiva, o slot resolvido no pacote e o alcance do material — um
// `MaterialAsset` compartilhado (`materialAsset`) e/ou uma substituição só nesta
// instância (`material.enabled`).
struct MeshSubmesh {
  u32 mesh = 0;
  resources::AssetGuid asset{};
  resources::AssetGuid materialAsset{};
  MaterialParameters material;
  SlotTextures textures{};
  MaterialSurface surface{};
  SlotSampling sampling{};
  MaterialChannels channels{};
  resources::AssetGuid occlusionTexture{};
  friend bool operator==(const MeshSubmesh &a, const MeshSubmesh &b) {
    return a.mesh == b.mesh && a.asset == b.asset && a.materialAsset == b.materialAsset && a.material == b.material &&
           a.textures == b.textures && a.surface == b.surface && a.sampling == b.sampling && a.channels == b.channels &&
           a.occlusionTexture == b.occlusionTexture;
  }
};

class MeshRenderer final : public ComponentValue {
public:
  static constexpr usize MaximumSubmeshes = 256;
  // `mesh` é o SLOT resolvido no processo — índice 1-based no pacote carregado,
  // zero quando não há malha. `asset` é a IDENTIDADE persistente.
  //
  // Os dois existem porque respondem a perguntas diferentes. O slot é o que o
  // extrator e o pick usam em todo quadro, e precisa ser um índice. A identidade
  // é o que sobrevive a reordenar o pacote, reimportar a fonte ou renomear o
  // arquivo — coisas que trocam o índice sem trocar a malha. Ao carregar, o
  // slot é RECONCILIADO a partir da identidade; ao salvar, os dois vão ao
  // arquivo, e é a identidade que manda numa divergência.
  //
  // Os campos soltos são o slot 0; `submeshes` são os slots 1..n. Manter o slot
  // 0 onde sempre esteve preserva todos os consumidores de malha única.
  u32 mesh=0;
  resources::AssetGuid asset{};
  bool enabled=true;
  MaterialParameters material;
  resources::AssetGuid materialAsset{};
  // R4: texturas trocadas nesta instância, por binding, do slot 0.
  SlotTextures textures{};
  // R4: modo de alfa, corte e faces trocados nesta instância, do slot 0.
  MaterialSurface surface{};
  const MaterialSurface &slotSurface(u32 slot) const noexcept {
    static const MaterialSurface none{};
    return slot ? (slot - 1 < submeshes.size() ? submeshes[slot - 1].surface : none) : surface;
  }
  MaterialSurface *editSlotSurface(u32 slot) noexcept {
    return slot ? (slot - 1 < submeshes.size() ? &submeshes[slot - 1].surface : nullptr) : &surface;
  }
  std::vector<MeshSubmesh> submeshes;
  // R4: amostragem trocada nesta instância, por binding, do slot 0.
  SlotSampling sampling{};
  // R4: canais, oclusão, normal e alfa, e a textura de oclusão própria, do slot 0.
  MaterialChannels channels{};
  resources::AssetGuid occlusionTexture{};
  const MaterialChannels &slotChannels(u32 slot) const noexcept {
    static const MaterialChannels none{};
    return slot ? (slot - 1 < submeshes.size() ? submeshes[slot - 1].channels : none) : channels;
  }
  MaterialChannels *editSlotChannels(u32 slot) noexcept {
    return slot ? (slot - 1 < submeshes.size() ? &submeshes[slot - 1].channels : nullptr) : &channels;
  }
  resources::AssetGuid slotOcclusionTexture(u32 slot) const noexcept {
    return slot ? (slot - 1 < submeshes.size() ? submeshes[slot - 1].occlusionTexture : resources::AssetGuid{}) : occlusionTexture;
  }
  resources::AssetGuid *editSlotOcclusionTexture(u32 slot) noexcept {
    return slot ? (slot - 1 < submeshes.size() ? &submeshes[slot - 1].occlusionTexture : nullptr) : &occlusionTexture;
  }
  const SlotSampling &slotSampling(u32 slot) const noexcept {
    static const SlotSampling none{};
    return slot ? (slot - 1 < submeshes.size() ? submeshes[slot - 1].sampling : none) : sampling;
  }
  SlotSampling *editSlotSampling(u32 slot) noexcept {
    return slot ? (slot - 1 < submeshes.size() ? &submeshes[slot - 1].sampling : nullptr) : &sampling;
  }

  const SlotTextures &slotTextures(u32 slot) const noexcept {
    static const SlotTextures none{};
    return slot ? (slot - 1 < submeshes.size() ? submeshes[slot - 1].textures : none) : textures;
  }
  SlotTextures *editSlotTextures(u32 slot) noexcept {
    return slot ? (slot - 1 < submeshes.size() ? &submeshes[slot - 1].textures : nullptr) : &textures;
  }

  u32 slotCount() const noexcept { return static_cast<u32>(1 + submeshes.size()); }
  u32 slotMesh(u32 slot) const noexcept { return slot ? (slot - 1 < submeshes.size() ? submeshes[slot - 1].mesh : 0) : mesh; }
  resources::AssetGuid slotAsset(u32 slot) const noexcept {
    return slot ? (slot - 1 < submeshes.size() ? submeshes[slot - 1].asset : resources::AssetGuid{}) : asset;
  }
  resources::AssetGuid slotMaterialAsset(u32 slot) const noexcept {
    return slot ? (slot - 1 < submeshes.size() ? submeshes[slot - 1].materialAsset : resources::AssetGuid{}) : materialAsset;
  }
  const MaterialParameters &slotMaterial(u32 slot) const noexcept {
    static const MaterialParameters none;
    return slot ? (slot - 1 < submeshes.size() ? submeshes[slot - 1].material : none) : material;
  }
  // Acesso de escrita por slot; nulo fora do intervalo.
  u32 *editSlotMesh(u32 slot) noexcept { return slot ? (slot - 1 < submeshes.size() ? &submeshes[slot - 1].mesh : nullptr) : &mesh; }
  resources::AssetGuid *editSlotAsset(u32 slot) noexcept {
    return slot ? (slot - 1 < submeshes.size() ? &submeshes[slot - 1].asset : nullptr) : &asset;
  }
  resources::AssetGuid *editSlotMaterialAsset(u32 slot) noexcept {
    return slot ? (slot - 1 < submeshes.size() ? &submeshes[slot - 1].materialAsset : nullptr) : &materialAsset;
  }
  MaterialParameters *editSlotMaterial(u32 slot) noexcept {
    return slot ? (slot - 1 < submeshes.size() ? &submeshes[slot - 1].material : nullptr) : &material;
  }

  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<MeshRenderer>(*this);}
  static bool validMaterial(const MaterialParameters &value) {
    MeshRenderer probe;probe.material=value;
    for(const auto &p:descriptor.numbers) {const auto v=p.read(probe);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return true;
  }
  bool valid() const override {
    if(!validMaterial(material) || submeshes.size()>MaximumSubmeshes) return false;
    if(!validMaterialSurface(surface)) return false;
    for(const auto &slot:submeshes) if(!validMaterial(slot.material) || !validMaterialSurface(slot.surface)) return false;
    for(u32 slot=0;slot<slotCount();++slot)
      for(const auto &value:slotSampling(slot)) if(!validMaterialSampling(value)) return false;
    for(u32 slot=0;slot<slotCount();++slot) if(!validMaterialChannels(slotChannels(slot))) return false;
    return true;
  }
  void write(std::ostream &out) const override {
    const auto guid=[](const resources::AssetGuid &value) {return value.valid()?value.text():std::string("-");};
    out<<mesh<<' '<<enabled<<' '<<material.enabled<<' ';
    for(const auto &p:descriptor.numbers) out<<p.read(*this)<<' ';
    out<<guid(asset)<<' ';
    // v3: material compartilhado do slot 0 e os slots adicionais.
    out<<guid(materialAsset)<<' '<<submeshes.size();
    for(const auto &slot:submeshes) {
      MeshRenderer probe;probe.material=slot.material;
      out<<' '<<slot.mesh<<' '<<guid(slot.asset)<<' '<<guid(slot.materialAsset)<<' '<<slot.material.enabled;
      for(const auto &p:descriptor.numbers) out<<' '<<p.read(probe);
    }
    // v4: texturas por binding de cada slot, na ordem dos slots.
    for(u32 slot=0;slot<slotCount();++slot)
      for(const auto &texture:slotTextures(slot)) out<<' '<<materialTextureToken(texture);
    // v5: modo de alfa, faces e corte de cada slot.
    for(u32 slot=0;slot<slotCount();++slot) {
      const auto &value=slotSurface(slot);
      out<<' '<<static_cast<unsigned>(value.alphaMode)<<' '<<static_cast<unsigned>(value.sides)<<' '<<value.alphaCutoff;
    }
    // v6: amostragem de cada binding de cada slot.
    for(u32 slot=0;slot<slotCount();++slot)
      for(const auto &value:slotSampling(slot)) out<<' '<<materialSamplingToken(value);
    // v7: transformação de UV de cada binding de cada slot (deslocamento, escala, rotação).
    for(u32 slot=0;slot<slotCount();++slot)
      for(const auto &value:slotSampling(slot))
        out<<' '<<value.offset[0]<<' '<<value.offset[1]<<' '<<value.scale[0]<<' '<<value.scale[1]<<' '<<value.rotation;
    // v8: canais (rugosidade, metal, oclusão), origem e força da oclusão, inversão Y
    // do normal, origem do alfa e a textura de oclusão própria de cada slot.
    for(u32 slot=0;slot<slotCount();++slot) {
      const auto &value=slotChannels(slot);
      out<<' '<<static_cast<unsigned>(value.roughness)<<' '<<static_cast<unsigned>(value.metallic)<<' '<<static_cast<unsigned>(value.occlusion)
         <<' '<<static_cast<unsigned>(value.occlusionSource)<<' '<<value.occlusionStrength<<' '<<static_cast<unsigned>(value.normalFlipY)
         <<' '<<static_cast<unsigned>(value.alphaSource)<<' '<<materialTextureToken(slotOcclusionTexture(slot));
    }
    out<<' ';
  }
  bool read(std::istream &in,u32 version) override {
    const auto parse=[](const std::string &text,resources::AssetGuid &out) {
      out={};
      // "-" é ausência declarada, não falha de leitura: uma malha pode não ter
      // recurso nenhum, e isso precisa voltar do arquivo como ausência.
      return text=="-" || resources::AssetGuid::parse(text,out);
    };
    bool overridden=false;
    if(version<1 || version>8 || !(in>>mesh>>enabled>>overridden)) return false;
    for(const auto &p:descriptor.numbers) if(!(in>>*p.write(*this))) return false;
    material.enabled=overridden;
    asset={};materialAsset={};textures={};surface={};sampling={};channels={};occlusionTexture={};submeshes.clear();
    if(version>=2) {
      std::string guid;
      if(!(in>>guid) || !parse(guid,asset)) return false;
    }
    if(version>=3) {
      std::string shared;usize count=0;
      if(!(in>>shared>>count) || !parse(shared,materialAsset) || count>MaximumSubmeshes) return false;
      submeshes.resize(count);
      for(auto &slot:submeshes) {
        std::string slotAsset,slotShared;bool slotOverridden=false;
        if(!(in>>slot.mesh>>slotAsset>>slotShared>>slotOverridden) || !parse(slotAsset,slot.asset) ||
           !parse(slotShared,slot.materialAsset)) return false;
        MeshRenderer probe;
        for(const auto &p:descriptor.numbers) if(!(in>>*p.write(probe))) return false;
        slot.material=probe.material;slot.material.enabled=slotOverridden;
      }
    }
    if(version>=4) {
      for(u32 slot=0;slot<slotCount();++slot)
        for(auto &texture:*editSlotTextures(slot)) {
          std::string token;
          if(!(in>>token) || !parseMaterialTextureToken(token,texture)) return false;
        }
    }
    if(version>=5) {
      for(u32 slot=0;slot<slotCount();++slot) {
        unsigned alpha=0,sides=0;float cutoff=.5f;
        if(!(in>>alpha>>sides>>cutoff) || alpha>MaterialAlphaBlend || sides>MaterialSidesDouble) return false;
        *editSlotSurface(slot)={static_cast<std::uint8_t>(alpha),static_cast<std::uint8_t>(sides),cutoff};
        if(!validMaterialSurface(*editSlotSurface(slot))) return false;
      }
    }
    if(version>=6) {
      for(u32 slot=0;slot<slotCount();++slot)
        for(auto &value:*editSlotSampling(slot)) {
          std::string token;
          if(!(in>>token) || !parseMaterialSamplingToken(token,value)) return false;
        }
    }
    if(version>=7) {
      for(u32 slot=0;slot<slotCount();++slot)
        for(auto &value:*editSlotSampling(slot))
          if(!(in>>value.offset[0]>>value.offset[1]>>value.scale[0]>>value.scale[1]>>value.rotation) ||
             !validMaterialSampling(value)) return false;
    }
    if(version>=8) {
      for(u32 slot=0;slot<slotCount();++slot) {
        unsigned roughness=0,metallic=0,occlusion=0,source=0,flip=0,alpha=0;float strength=-1;std::string token;
        if(!(in>>roughness>>metallic>>occlusion>>source>>strength>>flip>>alpha>>token) || roughness>255 || metallic>255 ||
           occlusion>255 || source>255 || flip>255 || alpha>255) return false;
        auto &value=*editSlotChannels(slot);
        value={static_cast<std::uint8_t>(roughness),static_cast<std::uint8_t>(metallic),static_cast<std::uint8_t>(occlusion),
               static_cast<std::uint8_t>(source),strength,static_cast<std::uint8_t>(flip),static_cast<std::uint8_t>(alpha)};
        if(!validMaterialChannels(value) || !parseMaterialTextureToken(token,*editSlotOcclusionTexture(slot))) return false;
      }
    }
    return true;
  }
};
inline constexpr std::array<ComponentNumber,11> meshRendererNumbers{{
#define AE_MESH_NUMBER(id,label,field,lo,hi,step) {label,lo,hi,step,[](const ComponentValue &v)->const float&{return static_cast<const MeshRenderer&>(v).material.field;},[](ComponentValue &v)->float*{auto &m=static_cast<MeshRenderer&>(v).material;m.enabled=true;return &m.field;},id}
  AE_MESH_NUMBER("base_color.r","Cor R",baseColor[0],0,1,.01f),
  AE_MESH_NUMBER("base_color.g","Cor G",baseColor[1],0,1,.01f),
  AE_MESH_NUMBER("base_color.b","Cor B",baseColor[2],0,1,.01f),
  AE_MESH_NUMBER("roughness","Rugosidade",roughness,0,1,.01f),
  AE_MESH_NUMBER("metallic","Metálico",metallic,0,1,.01f),
  AE_MESH_NUMBER("normal_scale","Intensidade da normal",normalScale,0,16,.02f),
  AE_MESH_NUMBER("specular","Especular",specular,0,1,.01f),
  AE_MESH_NUMBER("emission.r","Emissão R",emission[0],0,1,.01f),
  AE_MESH_NUMBER("emission.g","Emissão G",emission[1],0,1,.01f),
  AE_MESH_NUMBER("emission.b","Emissão B",emission[2],0,1,.01f),
  AE_MESH_NUMBER("emission_strength","Potência de emissão",emissionStrength,0,10000,.1f)
#undef AE_MESH_NUMBER
}};
inline constexpr std::array<ComponentBoolean,1> meshRendererBooleans{{
  {"enabled","Renderizar",[](const ComponentValue &v){return static_cast<const MeshRenderer&>(v).enabled;},[](ComponentValue &v,bool b){static_cast<MeshRenderer&>(v).enabled=b;}}
}};
// Os recursos que este componente endereça, declarados como as demais
// propriedades. O grafo de impacto, o reparo de referência quebrada, o preset
// e o relatório de dependências passam a ler ESTA lista em vez de repetir, cada
// um, o passeio pelos slots.
//
// A ordem importa: é a ordem em que um slot é apresentado no inspetor e no
// relatório — malha, material compartilhado e os bindings de textura na ordem
// do pacote de mapa, com a oclusão própria por último, como o shader lê.
namespace detail {
inline u32 meshSlots(const ComponentValue &v) {return static_cast<const MeshRenderer&>(v).slotCount();}
} // namespace detail
inline constexpr std::array<ComponentResourceBinding,7> meshRendererResources{{
  {"mesh","Malha",resources::AssetType::Mesh,detail::meshSlots,
   [](const ComponentValue &v,u32 slot){return static_cast<const MeshRenderer&>(v).slotAsset(slot);},
   [](ComponentValue &v,u32 slot,resources::AssetGuid value){
     auto *target=static_cast<MeshRenderer&>(v).editSlotAsset(slot);
     if(!target) return false;
     *target=value;return true;},
   {"Geometria"}},
  {"material","Material",resources::AssetType::Material,detail::meshSlots,
   [](const ComponentValue &v,u32 slot){return static_cast<const MeshRenderer&>(v).slotMaterialAsset(slot);},
   [](ComponentValue &v,u32 slot,resources::AssetGuid value){
     auto *target=static_cast<MeshRenderer&>(v).editSlotMaterialAsset(slot);
     if(!target) return false;
     *target=value;return true;},
   {"Material"},true,MaterialTextureNone},
#define AE_MESH_TEXTURE(id,label,binding) {id,label,resources::AssetType::Texture,detail::meshSlots,\
   [](const ComponentValue &v,u32 slot){return static_cast<const MeshRenderer&>(v).slotTextures(slot)[binding];},\
   [](ComponentValue &v,u32 slot,resources::AssetGuid value){\
     auto *target=static_cast<MeshRenderer&>(v).editSlotTextures(slot);\
     if(target) (*target)[binding]=value;\
     return target!=nullptr;},\
   {"Material"},true,MaterialTextureNone}
  AE_MESH_TEXTURE("texture.base_color","Cor base",0),
  AE_MESH_TEXTURE("texture.normal","Normal",1),
  AE_MESH_TEXTURE("texture.metallic_roughness","Metal / rugosidade",2),
  AE_MESH_TEXTURE("texture.emissive","Emissão",3),
#undef AE_MESH_TEXTURE
  {"texture.occlusion","Oclusão",resources::AssetType::Texture,detail::meshSlots,
   [](const ComponentValue &v,u32 slot){return static_cast<const MeshRenderer&>(v).slotOcclusionTexture(slot);},
   [](ComponentValue &v,u32 slot,resources::AssetGuid value){
     auto *target=static_cast<MeshRenderer&>(v).editSlotOcclusionTexture(slot);
     if(!target) return false;
     *target=value;return true;},
   {"Material"},true,MaterialTextureNone}
}};
// Superfície, canais e amostragem POR SLOT, com identidade persistente.
//
// R4 trouxe esses valores como dado do componente, editáveis só pelo inspetor.
// Sem PropertyId eles ficavam fora de tudo o que endereça propriedade: API em
// C#, preset seletivo, animação e a matriz de contratos. Declarados aqui, o
// slot 2 de uma malha passa a ser tão alcançável quanto a cor base.
//
// Os rótulos seguem o vocabulário do URP Lit (Surface Type, Alpha Clipping,
// Render Face, Tiling, Offset), que é o que o autor já traz de fora.
//
// Amostragem possui duas formas de endereçamento. Os ids curtos `sampling.*`
// preservam presets e scripts antigos e escrevem todos os bindings. Os ids
// `sampling.<binding>.*` alcançam exatamente o mesmo valor individual que o
// editor de material. O binding de oclusão compartilha a amostragem do mapa
// metálico/rugosidade, como no consumidor existente.
// O próprio interruptor de override é endereçável: sem ele, um preset poderia
// ligar a substituição de material (ao escrever um fator) e nunca desligá-la.
inline constexpr std::array<ComponentEnumOption,2> materialOverrideOptions{{
  {0,"Herdado da fonte"},{1,"Substituído nesta instância"}
}};
inline constexpr std::array<ComponentEnumOption,4> materialAlphaModeOptions{{
  {MaterialAlphaKeep,"Herdar"},{MaterialAlphaOpaque,"Opaco"},{MaterialAlphaMask,"Recorte"},{MaterialAlphaBlend,"Mistura"}
}};
inline constexpr std::array<ComponentEnumOption,3> materialSidesOptions{{
  {MaterialSidesKeep,"Herdar"},{MaterialSidesSingle,"Face única"},{MaterialSidesDouble,"Face dupla"}
}};
inline constexpr std::array<ComponentEnumOption,5> materialChannelOptions{{
  {MaterialChannelKeep,"Herdar"},{MaterialChannelR,"R"},{MaterialChannelG,"G"},{MaterialChannelB,"B"},{MaterialChannelA,"A"}
}};
inline constexpr std::array<ComponentEnumOption,4> materialOcclusionSourceOptions{{
  {MaterialOcclusionKeep,"Herdar"},{MaterialOcclusionNone,"Sem oclusão"},{MaterialOcclusionPacked,"No mapa metal/rugosidade"},
  {MaterialOcclusionTexture,"Textura própria"}
}};
inline constexpr std::array<ComponentEnumOption,3> materialToggleOptions{{
  {MaterialToggleKeep,"Herdar"},{MaterialToggleOff,"Não"},{MaterialToggleOn,"Sim"}
}};
inline constexpr std::array<ComponentEnumOption,4> materialAlphaSourceOptions{{
  {MaterialAlphaSourceKeep,"Herdar"},{MaterialAlphaSourceBase,"Alfa da cor base"},{MaterialAlphaSourceOpaque,"Sempre opaco"},
  {MaterialAlphaSourceLuminance,"Luminância da cor base"}
}};
inline constexpr std::array<ComponentEnumOption,4> materialUvSetOptions{{
  {MaterialUvKeep,"Herdar"},{MaterialUv0,"UV 0"},{MaterialUv1,"UV 1"},{MaterialUvWorld,"Mundo (triplanar)"}
}};
inline constexpr std::array<ComponentEnumOption,4> materialWrapOptions{{
  {MaterialWrapKeep,"Herdar"},{MaterialWrapRepeat,"Repetir"},{MaterialWrapClamp,"Fixar na borda"},{MaterialWrapMirror,"Espelhar"}
}};
inline constexpr std::array<ComponentEnumOption,3> materialFilterOptions{{
  {MaterialFilterKeep,"Herdar"},{MaterialFilterLinear,"Linear"},{MaterialFilterNearest,"Vizinho mais próximo"}
}};

inline constexpr std::array<ComponentSlotEnum,24> meshRendererSlotEnums{{
  {"material.override","Material",materialOverrideOptions,detail::meshSlots,
   [](const ComponentValue &v,u32 slot)->u32{return static_cast<const MeshRenderer&>(v).slotMaterial(slot).enabled?1u:0u;},
   [](ComponentValue &v,u32 slot,u32 value)->bool{
     auto *m=static_cast<MeshRenderer&>(v).editSlotMaterial(slot);
     if(!m) return false;
     m->enabled=value!=0;return true;},{"Cor"}},
#define AE_SLOT_ENUM(id,label,options,group,getter,setter) {id,label,options,detail::meshSlots,\
  [](const ComponentValue &v,u32 slot)->u32{const auto &m=static_cast<const MeshRenderer&>(v);(void)m;return getter;},\
  [](ComponentValue &v,u32 slot,u32 value)->bool{auto &m=static_cast<MeshRenderer&>(v);(void)m;(void)value;setter},{group}}
  AE_SLOT_ENUM("surface.alpha_mode","Tipo de superfície",materialAlphaModeOptions,"Superfície",
    m.slotSurface(slot).alphaMode,
    {auto *s=m.editSlotSurface(slot);if(!s) return false;s->alphaMode=static_cast<std::uint8_t>(value);return true;}),
  AE_SLOT_ENUM("surface.sides","Faces",materialSidesOptions,"Superfície",
    m.slotSurface(slot).sides,
    {auto *s=m.editSlotSurface(slot);if(!s) return false;s->sides=static_cast<std::uint8_t>(value);return true;}),
  AE_SLOT_ENUM("channels.roughness","Canal da rugosidade",materialChannelOptions,"Canais",
    m.slotChannels(slot).roughness,
    {auto *c=m.editSlotChannels(slot);if(!c) return false;c->roughness=static_cast<std::uint8_t>(value);return true;}),
  AE_SLOT_ENUM("channels.metallic","Canal do metálico",materialChannelOptions,"Canais",
    m.slotChannels(slot).metallic,
    {auto *c=m.editSlotChannels(slot);if(!c) return false;c->metallic=static_cast<std::uint8_t>(value);return true;}),
  AE_SLOT_ENUM("channels.occlusion","Canal da oclusão",materialChannelOptions,"Canais",
    m.slotChannels(slot).occlusion,
    {auto *c=m.editSlotChannels(slot);if(!c) return false;c->occlusion=static_cast<std::uint8_t>(value);return true;}),
  AE_SLOT_ENUM("channels.occlusion_source","Origem da oclusão",materialOcclusionSourceOptions,"Canais",
    m.slotChannels(slot).occlusionSource,
    {auto *c=m.editSlotChannels(slot);if(!c) return false;c->occlusionSource=static_cast<std::uint8_t>(value);return true;}),
  AE_SLOT_ENUM("channels.normal_flip_y","Inverter Y da normal",materialToggleOptions,"Canais",
    m.slotChannels(slot).normalFlipY,
    {auto *c=m.editSlotChannels(slot);if(!c) return false;c->normalFlipY=static_cast<std::uint8_t>(value);return true;}),
  AE_SLOT_ENUM("channels.alpha_source","Origem do alfa",materialAlphaSourceOptions,"Canais",
    m.slotChannels(slot).alphaSource,
    {auto *c=m.editSlotChannels(slot);if(!c) return false;c->alphaSource=static_cast<std::uint8_t>(value);return true;}),
  AE_SLOT_ENUM("sampling.uv_set","Conjunto de UV",materialUvSetOptions,"Amostragem",
    m.slotSampling(slot)[0].uvSet,
    {auto *s=m.editSlotSampling(slot);if(!s) return false;for(auto &b:*s) b.uvSet=static_cast<std::uint8_t>(value);return true;}),
  AE_SLOT_ENUM("sampling.wrap","Repetição",materialWrapOptions,"Amostragem",
    m.slotSampling(slot)[0].wrap,
    {auto *s=m.editSlotSampling(slot);if(!s) return false;for(auto &b:*s) b.wrap=static_cast<std::uint8_t>(value);return true;}),
  AE_SLOT_ENUM("sampling.filter","Filtro",materialFilterOptions,"Amostragem",
    m.slotSampling(slot)[0].filter,
    {auto *s=m.editSlotSampling(slot);if(!s) return false;for(auto &b:*s) b.filter=static_cast<std::uint8_t>(value);return true;}),
#define AE_BINDING_SAMPLING_ENUM(name,label,binding) \
  AE_SLOT_ENUM("sampling." name ".uv_set",label " / UV",materialUvSetOptions,"Amostragem", \
    m.slotSampling(slot)[binding].uvSet, \
    {auto *s=m.editSlotSampling(slot);if(!s) return false;(*s)[binding].uvSet=static_cast<std::uint8_t>(value);return true;}), \
  AE_SLOT_ENUM("sampling." name ".wrap",label " / Repetição",materialWrapOptions,"Amostragem", \
    m.slotSampling(slot)[binding].wrap, \
    {auto *s=m.editSlotSampling(slot);if(!s) return false;(*s)[binding].wrap=static_cast<std::uint8_t>(value);return true;}), \
  AE_SLOT_ENUM("sampling." name ".filter",label " / Filtro",materialFilterOptions,"Amostragem", \
    m.slotSampling(slot)[binding].filter, \
    {auto *s=m.editSlotSampling(slot);if(!s) return false;(*s)[binding].filter=static_cast<std::uint8_t>(value);return true;})
  AE_BINDING_SAMPLING_ENUM("base_color","Cor base",0),
  AE_BINDING_SAMPLING_ENUM("normal","Normal",1),
  AE_BINDING_SAMPLING_ENUM("metallic_roughness","Metal / rugosidade",2),
  AE_BINDING_SAMPLING_ENUM("emissive","Emissão",3)
#undef AE_BINDING_SAMPLING_ENUM
#undef AE_SLOT_ENUM
}};
inline constexpr std::array<ComponentSlotNumber,27> meshRendererSlotNumbers{{
#define AE_SLOT_NUMBER(id,label,lo,hi,step,group,getter,setter) {id,label,lo,hi,step,detail::meshSlots,\
  [](const ComponentValue &v,u32 slot)->float{const auto &m=static_cast<const MeshRenderer&>(v);(void)m;return getter;},\
  [](ComponentValue &v,u32 slot,float value)->bool{auto &m=static_cast<MeshRenderer&>(v);(void)m;(void)value;setter},{group}}
  AE_SLOT_NUMBER("surface.alpha_cutoff","Corte do alfa",0,1,.01f,"Superfície",
    m.slotSurface(slot).alphaCutoff,
    {auto *s=m.editSlotSurface(slot);if(!s) return false;s->alphaCutoff=value;return true;}),
  // -1 é a herança declarada da força de oclusão: o domínio começa nela de
  // propósito, para que "herdar" seja um valor endereçável e não uma ausência.
  AE_SLOT_NUMBER("channels.occlusion_strength","Força da oclusão",-1,1,.01f,"Canais",
    m.slotChannels(slot).occlusionStrength,
    {auto *c=m.editSlotChannels(slot);if(!c) return false;c->occlusionStrength=value;return true;}),
  AE_SLOT_NUMBER("sampling.offset_u","Deslocamento U",-100,100,.01f,"Amostragem",
    m.slotSampling(slot)[0].offset[0],
    {auto *s=m.editSlotSampling(slot);if(!s) return false;for(auto &b:*s) b.offset[0]=value;return true;}),
  AE_SLOT_NUMBER("sampling.offset_v","Deslocamento V",-100,100,.01f,"Amostragem",
    m.slotSampling(slot)[0].offset[1],
    {auto *s=m.editSlotSampling(slot);if(!s) return false;for(auto &b:*s) b.offset[1]=value;return true;}),
  AE_SLOT_NUMBER("sampling.scale_u","Escala U",.01f,100,.01f,"Amostragem",
    m.slotSampling(slot)[0].scale[0],
    {auto *s=m.editSlotSampling(slot);if(!s) return false;for(auto &b:*s) b.scale[0]=value;return true;}),
  AE_SLOT_NUMBER("sampling.scale_v","Escala V",.01f,100,.01f,"Amostragem",
    m.slotSampling(slot)[0].scale[1],
    {auto *s=m.editSlotSampling(slot);if(!s) return false;for(auto &b:*s) b.scale[1]=value;return true;}),
  AE_SLOT_NUMBER("sampling.rotation","Rotação da UV",-360,360,1,"Amostragem",
    m.slotSampling(slot)[0].rotation,
    {auto *s=m.editSlotSampling(slot);if(!s) return false;for(auto &b:*s) b.rotation=value;return true;}),
#define AE_BINDING_SAMPLING_NUMBER(name,label,binding) \
  AE_SLOT_NUMBER("sampling." name ".offset_u",label " / Deslocamento U",-100,100,.01f,"Amostragem", \
    m.slotSampling(slot)[binding].offset[0], \
    {auto *s=m.editSlotSampling(slot);if(!s) return false;(*s)[binding].offset[0]=value;return true;}), \
  AE_SLOT_NUMBER("sampling." name ".offset_v",label " / Deslocamento V",-100,100,.01f,"Amostragem", \
    m.slotSampling(slot)[binding].offset[1], \
    {auto *s=m.editSlotSampling(slot);if(!s) return false;(*s)[binding].offset[1]=value;return true;}), \
  AE_SLOT_NUMBER("sampling." name ".scale_u",label " / Escala U",.01f,100,.01f,"Amostragem", \
    m.slotSampling(slot)[binding].scale[0], \
    {auto *s=m.editSlotSampling(slot);if(!s) return false;(*s)[binding].scale[0]=value;return true;}), \
  AE_SLOT_NUMBER("sampling." name ".scale_v",label " / Escala V",.01f,100,.01f,"Amostragem", \
    m.slotSampling(slot)[binding].scale[1], \
    {auto *s=m.editSlotSampling(slot);if(!s) return false;(*s)[binding].scale[1]=value;return true;}), \
  AE_SLOT_NUMBER("sampling." name ".rotation",label " / Rotação",-360,360,1,"Amostragem", \
    m.slotSampling(slot)[binding].rotation, \
    {auto *s=m.editSlotSampling(slot);if(!s) return false;(*s)[binding].rotation=value;return true;})
  AE_BINDING_SAMPLING_NUMBER("base_color","Cor base",0),
  AE_BINDING_SAMPLING_NUMBER("normal","Normal",1),
  AE_BINDING_SAMPLING_NUMBER("metallic_roughness","Metal / rugosidade",2),
  AE_BINDING_SAMPLING_NUMBER("emissive","Emissão",3)
#undef AE_BINDING_SAMPLING_NUMBER
#undef AE_SLOT_NUMBER
}};
// Os fatores PBR de CADA slot.
//
// `meshRendererNumbers` (acima) endereça os mesmos valores, mas só do slot 0:
// é a API publicada de quando a malha tinha um material só, e continua valendo
// para não quebrar preset salvo nem script escrito. O que faltava era alcançar
// o slot 2 de uma malha de seis primitivas — por API, por preset seletivo ou
// por animação —, e é isso que estas identidades fazem.
//
// Escrever um fator liga o override daquele slot, pela mesma razão que a versão
// de slot 0 liga: um valor autoral que não substitui nada não teria efeito.
inline constexpr std::array<ComponentSlotNumber,11> meshRendererSlotMaterial{{
#define AE_SLOT_MATERIAL(id,label,field,lo,hi,step,group) {id,label,lo,hi,step,detail::meshSlots,\
  [](const ComponentValue &v,u32 slot)->float{return static_cast<const MeshRenderer&>(v).slotMaterial(slot).field;},\
  [](ComponentValue &v,u32 slot,float value)->bool{\
    auto *m=static_cast<MeshRenderer&>(v).editSlotMaterial(slot);\
    if(!m) return false;\
    m->enabled=true;m->field=value;return true;},{group}}
  AE_SLOT_MATERIAL("material.base_color.r","Cor R",baseColor[0],0,1,.01f,"Cor"),
  AE_SLOT_MATERIAL("material.base_color.g","Cor G",baseColor[1],0,1,.01f,"Cor"),
  AE_SLOT_MATERIAL("material.base_color.b","Cor B",baseColor[2],0,1,.01f,"Cor"),
  AE_SLOT_MATERIAL("material.roughness","Rugosidade",roughness,0,1,.01f,"Superfície"),
  AE_SLOT_MATERIAL("material.metallic","Metálico",metallic,0,1,.01f,"Superfície"),
  AE_SLOT_MATERIAL("material.normal_scale","Intensidade da normal",normalScale,0,16,.02f,"Superfície"),
  AE_SLOT_MATERIAL("material.specular","Especular",specular,0,1,.01f,"Superfície"),
  AE_SLOT_MATERIAL("material.emission.r","Emissão R",emission[0],0,1,.01f,"Emissão"),
  AE_SLOT_MATERIAL("material.emission.g","Emissão G",emission[1],0,1,.01f,"Emissão"),
  AE_SLOT_MATERIAL("material.emission.b","Emissão B",emission[2],0,1,.01f,"Emissão"),
  AE_SLOT_MATERIAL("material.emission_strength","Potência de emissão",emissionStrength,0,10000,.1f,"Emissão")
#undef AE_SLOT_MATERIAL
}};
// As duas listas por slot viram uma só no descritor: para quem endereça
// propriedade, "corte do alfa" e "rugosidade" são a mesma espécie de campo.
inline const std::array<ComponentSlotNumber,38> meshRendererAllSlotNumbers=[]{
  std::array<ComponentSlotNumber,38> all{};
  usize at=0;
  for(const auto &p:meshRendererSlotNumbers) all[at++]=p;
  for(const auto &p:meshRendererSlotMaterial) all[at++]=p;
  return all;
}();
inline const ComponentType MeshRenderer::descriptor{
  "astra.render.mesh",8,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<MeshRenderer>();},
  meshRendererNumbers,meshRendererBooleans,{},nullptr,false,{},{},meshRendererResources,
  meshRendererAllSlotNumbers,meshRendererSlotEnums
};
}
