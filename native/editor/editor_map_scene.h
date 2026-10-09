#pragma once
#include "editor/editor_document.h"
#include "runtime/transform_math.h"
#include "renderer/map_draw_update.h"
#include "renderer/authoring_texture.h"
#include "editor/editor_view.h"
#include "resources/asset_registry.h"
#include "resources/skeletal_animation.h"
#include "runtime/scene_animation.h"
#include "scene/skinned_mesh.h"
#include "scene/lightmap_uv.h"

namespace ae::editor {
// Immutable package geometry plus authored transforms. No Vulkan or Android.
// assetId identifies a package draw, not a process pointer. The package fingerprint
// must accompany saved documents so references cannot silently target another map.
using EditorMapUpdate = renderer::MapDrawState;
class EditorMapScene final : public runtime::AnimationLibrary {
public:
  // G6-B: o que deforma cada desenho do pacote (0-based; nulo = estático) —
  // skin, blend shapes e a cópia de CPU da forma base usada por seleção e
  // limites — e os clipes de cada fonte importada. Dado imutável da fonte,
  // trocado junto com o pacote.
  struct DrawDeformation {
    std::shared_ptr<const resources::SkinDefinition> skin;
    std::shared_ptr<const resources::MorphTargetSet> morph;
    std::vector<float> restPositions; // xyz por vértice da faixa do desenho
    std::vector<u32> localIndices;     // índices relativos a `vertexOffset`
    std::vector<u8> influences;        // SkinInfluenceStride por vértice; vazio sem skin
  };
  void setDeformation(std::vector<std::shared_ptr<const DrawDeformation>> draws,
                      std::vector<std::shared_ptr<const runtime::SourceAnimations>> animations);
  const DrawDeformation *deformation(u32 index) const {
    return index<deformations_.size()?deformations_[index].get():nullptr;
  }
  const resources::SkinDefinition *drawSkin(u32 index) const {
    const auto *d=deformation(index);return d?d->skin.get():nullptr;
  }
  const resources::MorphTargetSet *drawMorph(u32 index) const {
    const auto *d=deformation(index);return d?d->morph.get():nullptr;
  }
  bool findClip(const resources::AssetGuid &clip, runtime::AnimationClipView &out) const override;
  // Todos os clipes carregados, para o seletor do Inspector.
  struct ClipEntry { resources::AssetGuid clip, source; std::string name; float duration=0; };
  std::vector<ClipEntry> clipCatalog() const;
  // Independent authored resources do not republish geometry or deform buffers.
  // Swap immutable evaluated data only after resource validation/publication.
  bool setAuthoredAnimations(std::vector<std::shared_ptr<const runtime::SourceAnimations>> animations);
  bool validateAuthoredAnimations(const std::vector<std::shared_ptr<const runtime::SourceAnimations>> &animations) const;
  const std::vector<std::shared_ptr<const runtime::SourceAnimations>> &animationSources() const {return animationSources_;}
  // Pose deformada de um slot: paleta (vazia sem skin), pesos 0..1 dos blend
  // shapes (vazios sem eles) e limites no MUNDO. Osso ausente fica na pose de
  // bind (paleta identidade). Falso quando a paleta é singular ou o desenho não
  // deforma; o desenho segue na forma base.
  struct DeformedPose {
    std::shared_ptr<std::vector<float>> palette,weights;
    float center[3]{};
    float radius=0;
    u32 missingBones=0;
  };
  bool deformedPose(const runtime::SceneGraph &document, const scene::SkinnedMesh &mesh, u32 assetIndex,
                    const float drawModel[16], DeformedPose &out) const;
  scene::LightmapUvStatus lightmapUvStatus(u32 index) const {
    return index<pickSlots_.size()?pickSlots_[pickSlots_[index].canonical].lightmapUvStatus:scene::LightmapUvStatus::MissingGeometry;
  }
  bool lightmapUvUsable(u32 index) const {return lightmapUvStatus(index)==scene::LightmapUvStatus::Valid;}
  const char *lightmapUvDiagnostic(u32 index) const {return scene::lightmapUvDiagnostic(lightmapUvStatus(index));}
  u32 assetCount() const { return static_cast<u32>(source_.size()); }
  const renderer::MapDrawRecord *asset(u32 index) const { return index<source_.size()?&source_[index]:nullptr; }
  std::string_view assetName(u32 index) const { return index<assetNames_.size()?assetNames_[index]:std::string_view{}; }
  // Loading a resource library need not instantiate its contents in the scene.
  bool import(EditorDocument &document, std::span<const renderer::MapDrawRecord> draws,
              std::span<const renderer::MapMaterialRecord> materials = {}, bool instantiate = true, std::span<const u8> vertices = {}, std::span<const u32> indices = {},
              u64 packageFingerprint = 0);
  // A identidade estável do desenho `index` (0-based) do pacote carregado.
  //
  // Ela é DERIVADA, não sorteada: a mesma impressão digital de pacote e o mesmo
  // índice produzem o mesmo GUID em qualquer máquina e em qualquer dia. É o que
  // permite uma cena salva antes do registro existir ganhar identidade sem
  // precisar de um arquivo de tradução guardado em lugar nenhum.
  resources::AssetGuid assetGuid(u32 index) const {
    return index < assets_.size() ? assets_[index] : resources::AssetGuid{};
  }
  // O slot (1-based, como `MeshRenderer::mesh`) de uma identidade, ou zero
  // quando o recurso não está neste pacote — uma referência ausente, que quem
  // chama precisa explicar em vez de desenhar outra malha no lugar.
  u32 assetSlot(const resources::AssetGuid &guid) const;
  // Casa slot e identidade depois de carregar uma cena. Cena com identidade
  // manda: o slot é recalculado. Cena antiga, sem identidade, ganha a derivada
  // do slot que ela já trazia.
  void reconcileAssets(EditorDocument &document) const;
  // Adota um pacote novo SEM tocar no documento. É o que a importação usa: a
  // biblioteca do processo cresce, os objetos que já existem continuam onde
  // estão, e a reconciliação reata slot e identidade.
  //
  // `identities` é opcional e vale por desenho: uma entrada válida é usada como
  // está — é o caso da geometria importada, cuja identidade vem do arquivo de
  // origem e da chave estável, não do índice — e uma entrada vazia recebe a
  // identidade derivada da impressão digital do pacote.
  // `pivots` traz três floats por desenho: a origem em torno da qual o objeto
  // gira e pela qual o gizmo o pega. Vazio mantém a convenção do pacote — pivô
  // no centro dos limites —, que é o que faz um cubo primitivo girar em torno
  // de si. Geometria importada usa a origem do NÓ, porque é ela que segura a
  // dobradiça de uma porta; o centro visual não é o pivô do arquivo.
  bool adoptPackage(EditorDocument &document, std::span<const renderer::MapDrawRecord> draws,
                    std::span<const renderer::MapMaterialRecord> materials,
                    std::span<const u8> vertices, std::span<const u32> indices,
                    std::span<const resources::AssetGuid> identities, u64 packageFingerprint,
                    std::span<const float> pivots = {}, std::span<const std::string> names = {});
  // O pivô do desenho `index` (0-based), em espaço do mesh.
  void pivotOf(u32 index, float out[3]) const;
  // Um desenho por SLOT de cada objeto, todos com o mesmo `objectId`.
  bool extract(const runtime::SceneGraph &document, std::vector<EditorMapUpdate> &out) const;
  // Limites do objeto inteiro: a união dos slots.
  bool bounds(const runtime::SceneGraph &document, EditorEntityId entity, float center[3], float &radius) const;
  // Limites de um desenho do pacote levado à pose do objeto.
  bool slotBounds(const runtime::SceneGraph &document, EditorEntityId entity, u32 assetIndex, float center[3], float &radius) const;
  bool localGeometry(u32 assetId,std::span<const EditorPickMesh::Triangle> &triangles,float relative[16]) const;
  // CPU snapshot of the same skin + morph consumer used for drawing/picking.
  // Unlike rendering's bind-pose fallback, authoring refuses missing bones.
  bool authoredGeometry(const runtime::SceneGraph &,EditorEntityId,u32 assetId,
                        std::vector<EditorPickMesh::Triangle> &,std::string &) const;
  struct CollisionHullPreview {
    std::span<const EditorPickMesh::Triangle> triangles;
    u32 inputPointCount=0,vertexCount=0,faceCount=0;
    std::string_view diagnostic;
  };
  // Casco que o Jolt efetivamente produz para uma ou mais malhas no espaço do
  // objeto. O cache pertence à biblioteca: troca de pacote o invalida e editar
  // a tolerância cria uma entrada distinta, sem cozinhar novamente por frame.
  bool collisionHullPreview(std::span<const u32> assetSlots,float hullTolerance,
                            CollisionHullPreview &out) const;
  // Authoring-surface query: full source triangles or cooked preview hull,
  // never the visual bounds sphere. BVHs follow the existing resource caches.
  bool intersectColliderMesh(std::span<const u32> assetSlots,bool convex,float hullTolerance,
                             const EditorRay &ray,const float pose[16],float &distance) const;
  bool pickGeometry(const runtime::SceneGraph &document, EditorEntityId id, EditorPickCandidate &out) const;
  bool pickSlotGeometry(const runtime::SceneGraph &document, EditorEntityId id, u32 slot, EditorPickCandidate &out) const;
  // Materiais do projeto (MaterialAsset) disponíveis para resolver slots. Trocar
  // a biblioteca não toca geometria: o próximo `extract` já usa os valores.
  struct SharedMaterial {
    scene::MaterialParameters values;
    scene::SlotTextures textures{};
    scene::MaterialSurface surface{};
    scene::SlotSampling sampling{};
    scene::MaterialChannels channels{};
    resources::AssetGuid occlusionTexture{};
  };
  void setMaterialLibrary(std::vector<std::pair<resources::AssetGuid,SharedMaterial>> library) {materialLibrary_=std::move(library);}
  const SharedMaterial *sharedMaterial(const resources::AssetGuid &guid) const {
    if(!guid.valid()) return nullptr;
    for(const auto &[id,value]:materialLibrary_) if(id==guid) return &value;
    return nullptr;
  }
  const scene::MaterialParameters *materialAsset(const resources::AssetGuid &guid) const {
    const auto *shared=sharedMaterial(guid);
    return shared?&shared->values:nullptr;
  }
  // R4: texturas do PROJETO publicadas na biblioteca, por identidade e espaço de
  // cor (a mesma imagem usada como cor e como dado vira duas texturas).
  // A mesma imagem com outro sampler (repetição, filtro) também vira outra
  // textura publicada: o sampler mora na textura, não no material.
  struct TextureBinding {
    resources::AssetGuid guid;
    bool srgb=true;
    u32 sampler=DefaultTextureSampler;
    u32 index=0; // posição na lista de texturas da biblioteca publicada
    bool normal=false;
  };
  static constexpr u32 DefaultTextureSampler=renderer::AuthoringTextureLinearFilter|renderer::AuthoringTextureLinearMip|
                                             renderer::AuthoringTextureRepeatU|renderer::AuthoringTextureRepeatV;
  // Flags de sampler de uma textura do projeto com a amostragem pedida; herdar
  // mantém o padrão (linear com mips, repetindo).
  static u32 samplerFlags(const scene::MaterialSampling &sampling) {
    u32 flags=DefaultTextureSampler;
    if(sampling.wrap==scene::MaterialWrapClamp) flags&=~(renderer::AuthoringTextureRepeatU|renderer::AuthoringTextureRepeatV);
    else if(sampling.wrap==scene::MaterialWrapMirror) flags|=renderer::AuthoringTextureMirrorU|renderer::AuthoringTextureMirrorV;
    if(sampling.filter==scene::MaterialFilterNearest) flags&=~(renderer::AuthoringTextureLinearFilter|renderer::AuthoringTextureLinearMip);
    return flags;
  }
  void setTextureLibrary(std::vector<TextureBinding> library) {textureLibrary_=std::move(library);}
  const std::vector<TextureBinding> &textureLibrary() const {return textureLibrary_;}
  // Cor base e emissão são cor (sRGB); normal e metálico/rugosidade são dados.
  static bool bindingIsSrgb(u32 binding) {return binding==0 || binding==3;}
  // Índice publicado de uma identidade num binding. Identidade que ainda não
  // foi publicada devolve "herdar": a textura da fonte fica até a publicação.
  u32 resolveTexture(const resources::AssetGuid &guid,u32 binding,u32 sampler=DefaultTextureSampler) const {
    if(guid==scene::MaterialTextureNone) return renderer::InvalidMapTexture;
    if(!guid.valid()) return scene::MaterialTextureKeep;
    for(const auto &entry:textureLibrary_)
      if(entry.guid==guid && entry.srgb==bindingIsSrgb(binding) && entry.sampler==sampler && entry.normal==(binding==1)) return entry.index;
    return scene::MaterialTextureKeep;
  }
  // R4: canais, oclusão, normal e alfa efetivos, campo a campo: instância, senão
  // material compartilhado, senão herdar (a fonte decide).
  scene::MaterialChannels slotChannels(const scene::MeshRenderer &render,u32 slot) const {
    auto result=render.slotChannels(slot);
    if(const auto *shared=sharedMaterial(render.slotMaterialAsset(slot))) {
      const auto &inherited=shared->channels;
      if(!result.roughness) result.roughness=inherited.roughness;
      if(!result.metallic) result.metallic=inherited.metallic;
      if(!result.occlusion) result.occlusion=inherited.occlusion;
      if(!result.occlusionSource) result.occlusionSource=inherited.occlusionSource;
      if(result.occlusionStrength<0) result.occlusionStrength=inherited.occlusionStrength;
      if(!result.normalFlipY) result.normalFlipY=inherited.normalFlipY;
      if(!result.alphaSource) result.alphaSource=inherited.alphaSource;
    }
    return result;
  }
  resources::AssetGuid slotOcclusionTexture(const scene::MeshRenderer &render,u32 slot) const {
    const auto local=render.slotOcclusionTexture(slot);
    if(local.valid()) return local;
    if(const auto *shared=sharedMaterial(render.slotMaterialAsset(slot))) return shared->occlusionTexture;
    return {};
  }
  // Isolar na prévia: um objeto e um slot por vez, só no documento autoral.
  bool setMaterialIsolation(EditorEntityId entity,u32 slot,std::uint8_t channel) {
    if(isolateEntity_==entity && isolateSlot_==slot && isolateChannel_==channel) return false;
    isolateEntity_=entity;isolateSlot_=slot;isolateChannel_=channel;return true;
  }
  // R4: amostragem efetiva de um binding, campo a campo: instância, senão
  // material compartilhado, senão herdar (a fonte decide).
  scene::MaterialSampling slotSampling(const scene::MeshRenderer &render,u32 slot,u32 binding) const {
    if(binding>=scene::MaterialTextureCount) return {};
    auto result=render.slotSampling(slot)[binding];
    if(const auto *shared=sharedMaterial(render.slotMaterialAsset(slot))) {
      const auto &inherited=shared->sampling[binding];
      if(result.uvSet==scene::MaterialUvKeep) result.uvSet=inherited.uvSet;
      if(result.wrap==scene::MaterialWrapKeep) result.wrap=inherited.wrap;
      if(result.filter==scene::MaterialFilterKeep) result.filter=inherited.filter;
      // A transformação é uma unidade: a da instância inteira, senão a do material.
      if(!result.transformed()) {
        std::copy(std::begin(inherited.offset),std::end(inherited.offset),result.offset);
        std::copy(std::begin(inherited.scale),std::end(inherited.scale),result.scale);
        result.rotation=inherited.rotation;
      }
    }
    return result;
  }
  // Identidade efetiva de um binding: a trocada nesta instância, senão a do
  // material compartilhado, senão nenhuma (vale a da fonte).
  resources::AssetGuid slotTexture(const scene::MeshRenderer &render,u32 slot,u32 binding) const {
    if(binding>=scene::MaterialTextureCount) return {};
    const auto &local=render.slotTextures(slot)[binding];
    if(local.valid()) return local;
    if(const auto *shared=sharedMaterial(render.slotMaterialAsset(slot))) return shared->textures[binding];
    return {};
  }
  // Material efetivo de um slot: substituição da instância, senão o material
  // compartilhado, senão o da fonte (`enabled` falso = valores do pacote). As
  // texturas resolvem binding a binding, independentes dos escalares.
  scene::MaterialParameters slotMaterial(const scene::MeshRenderer &render, u32 slot) const {
    const auto &local=render.slotMaterial(slot);
    scene::MaterialParameters value=local;
    if(!local.enabled)
      if(const auto *shared=materialAsset(render.slotMaterialAsset(slot))) {value=*shared;value.enabled=true;}
    value=scene::withoutResolvedTextures(value);
    for(u32 binding=0;binding<scene::MaterialTextureCount;++binding) {
      const auto sampling=slotSampling(render,slot,binding);
      value.textures[binding]=resolveTexture(slotTexture(render,slot,binding),binding,samplerFlags(sampling));
      value.uvSets[binding]=sampling.uvSet;
      if(sampling.transformed()) {
        value.uvTransformMask|=static_cast<std::uint8_t>(1u<<binding);
        scene::materialUvTransformRows(sampling,value.uvTransforms[binding]);
      }
    }
    value.channels=slotChannels(render,slot);
    // A textura de oclusão é dado (linear) e usa a amostragem do metal/rugosidade.
    value.occlusionTexture=resolveTexture(slotOcclusionTexture(render,slot),2,samplerFlags(slotSampling(render,slot,2)));
    const auto &lm=render.slotLightmap(slot);
    if(lm.enabled && render.slotMesh(slot) && lightmapUvUsable(render.slotMesh(slot)-1)) {
      value.lightmapTexture=resolveTexture(lm.texture,2,samplerFlags(scene::lightmapSampling()));
      value.lightmapScaleOffset[0]=lm.scale[0];value.lightmapScaleOffset[1]=lm.scale[1];
      value.lightmapScaleOffset[2]=lm.offset[0];value.lightmapScaleOffset[3]=lm.offset[1];
      value.lightmapIntensity=lm.intensity;
    }
    const auto surface=slotSurface(render,slot);
    value.alphaMode=surface.alphaMode;value.sides=surface.sides;value.alphaCutoff=surface.alphaCutoff;
    return value;
  }
  // R4: superfície efetiva de um slot, campo a campo: a trocada nesta instância,
  // senão a do material compartilhado, senão herdar (a fonte decide). O corte
  // acompanha quem definiu o modo de alfa.
  scene::MaterialSurface slotSurface(const scene::MeshRenderer &render,u32 slot) const {
    const auto &local=render.slotSurface(slot);
    const auto *shared=sharedMaterial(render.slotMaterialAsset(slot));
    scene::MaterialSurface result;
    if(local.alphaMode!=scene::MaterialAlphaKeep) {result.alphaMode=local.alphaMode;result.alphaCutoff=local.alphaCutoff;}
    else if(shared && shared->surface.alphaMode!=scene::MaterialAlphaKeep) {
      result.alphaMode=shared->surface.alphaMode;result.alphaCutoff=shared->surface.alphaCutoff;
    }
    result.sides=local.sides!=scene::MaterialSidesKeep?local.sides:shared?shared->surface.sides:scene::MaterialSidesKeep;
    return result;
  }
  void hydrateMaterials(EditorDocument &document) const;
  renderer::MaterialOverride materialForAsset(u32 index) const;
  u32 materialFlagsForAsset(u32 index) const {
    return index<source_.size() && source_[index].materialIndex<materials_.size()
        ? materials_[source_[index].materialIndex].flags : 0;
  }
private:
  std::vector<std::pair<resources::AssetGuid,SharedMaterial>> materialLibrary_;
  std::vector<TextureBinding> textureLibrary_;
  std::vector<std::shared_ptr<const DrawDeformation>> deformations_;
  std::vector<std::shared_ptr<const runtime::SourceAnimations>> animationSources_;
  std::vector<std::shared_ptr<const runtime::SourceAnimations>> authoredAnimationSources_;
  struct ClipLocation { u32 source=0, clip=0; };
  std::unordered_map<resources::AssetGuid,ClipLocation,resources::AssetGuidHash> authoredClipIndex_;
  std::unordered_map<resources::AssetGuid,ClipLocation,resources::AssetGuidHash> clipIndex_;
  // Malha de seleção deformada de um objeto, refeita quando a pose muda.
  bool deformedPickMesh(const runtime::SceneGraph &document,EditorEntityId id,u32 assetIndex,const float drawModel[16],
                        std::shared_ptr<const EditorPickMesh> &out) const;
  EditorEntityId isolateEntity_=kInvalidEntity;
  u32 isolateSlot_=0;
  std::uint8_t isolateChannel_=0;
  std::vector<resources::AssetGuid> assets_;
  std::vector<std::string> assetNames_;
  // Três floats por desenho; vazio significa "pivô no centro dos limites".
  std::vector<float> pivots_;
  // Seleção por desenho sob demanda. Adotar um pacote copia só posições e
  // índices (validados na hora, como antes); a BVH de um desenho é construída
  // no primeiro uso — toque, prévia de colisor — e compartilhada entre desenhos
  // com a mesma geometria. Construir as 3,7 M faces do Sponza na adoção prendia
  // a thread do editor por ~20 s a cada superfície nova (ANR no aparelho).
  struct PickGeometry {std::vector<float> positions;std::vector<u32> indices;};
  struct PickSlot {u32 firstIndex=0,indexCount=0;i32 vertexOffset=0;u32 canonical=0;scene::LightmapUvStatus lightmapUvStatus=scene::LightmapUvStatus::MissingGeometry;};
  std::shared_ptr<const PickGeometry> pickGeometry_;
  std::vector<PickSlot> pickSlots_;
  mutable std::vector<std::shared_ptr<const EditorPickMesh>> pickMeshes_;
  std::shared_ptr<const EditorPickMesh> pickMesh(u32 index) const;
  static bool preparePickGeometry(std::span<const renderer::MapDrawRecord> draws,std::span<const u8> vertices,
                                  std::span<const u32> indices,std::shared_ptr<const PickGeometry> &geometry,
                                  std::vector<PickSlot> &slots);
  std::vector<renderer::MapMaterialRecord> materials_;
  std::vector<renderer::MapDrawRecord> source_;
  struct CollisionHullCacheEntry {
    std::vector<u32> slots;
    float tolerance=0;
    std::vector<EditorPickMesh::Triangle> triangles;
    std::shared_ptr<const EditorPickMesh> pickMesh;
    std::string diagnostic;
    u32 inputPointCount=0,vertexCount=0,faceCount=0;
  };
  mutable std::vector<CollisionHullCacheEntry> collisionHullCache_;
};
// Column-major local/world matrices with Rz * Ry * Rx Euler convention.
// A convenção mora em runtime/transform_math.h: o editor apenas a reexporta com
// os nomes que seus arquivos já usam, para que gizmo, física e scripts não
// possam divergir por terem cópias da mesma decomposição.
inline void editorTransformMatrix(const EditorTransform &transform, float out[16]) {
  runtime::transformMatrix(transform, out);
}
inline bool editorWorldMatrix(const runtime::SceneGraph &graph, EditorEntityId entity, float out[16]) {
  return runtime::worldMatrix(graph, entity, out);
}
inline bool editorLocalTransformForWorld(const float world[16], const float parent[16], EditorTransform &out) {
  return runtime::localTransformForWorld(world, parent, out);
}
} // namespace ae::editor
