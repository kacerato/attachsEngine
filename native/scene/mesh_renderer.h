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
inline const ComponentType MeshRenderer::descriptor{
  "astra.render.mesh",8,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<MeshRenderer>();},
  meshRendererNumbers,meshRendererBooleans,{},nullptr,false,{},{},meshRendererResources
};
}
