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
  friend bool operator==(const MeshSubmesh &a, const MeshSubmesh &b) {
    return a.mesh == b.mesh && a.asset == b.asset && a.materialAsset == b.materialAsset && a.material == b.material &&
           a.textures == b.textures && a.surface == b.surface;
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
    if(version<1 || version>5 || !(in>>mesh>>enabled>>overridden)) return false;
    for(const auto &p:descriptor.numbers) if(!(in>>*p.write(*this))) return false;
    material.enabled=overridden;
    asset={};materialAsset={};textures={};surface={};submeshes.clear();
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
inline const ComponentType MeshRenderer::descriptor{
  "astra.render.mesh",5,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<MeshRenderer>();},meshRendererNumbers,meshRendererBooleans
};
}
