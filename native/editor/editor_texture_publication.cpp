#include "editor/editor_session.h"
#include "editor/editor_texture_history.h"
#include "resources/tiff_decode.h"

namespace ae::editor {
// Prepared candidate + the existing publisher + the existing journal form one
// operation. Rendering must consume new texels, not merely the same GUID.
bool EditorSession::publishTextureImportCandidate(const std::optional<resources::AssetRecord> &expected,
    const std::optional<resources::AssetRecord> &target,std::span<const u8> source,
    const std::string &expectedDiskHash,std::string &diagnostic) {
  if(!expected&&!target)return false;
  const auto &identity=target?*target:*expected;
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());
  const auto *current=assets_.findByPath(identity.path);
  if((expected?(!current||!texture_history::sameRecord(*current,*expected)):current!=nullptr)||
     !texture_history::persisted(root,expected?&*expected:nullptr,identity.path)||
     Sha256::hex(source)!=identity.contentHash) {
    diagnostic="Fonte ou metadados da textura mudaram; nada publicado.";return false;
  }
  auto next=assets_;
  if(!target) {
    std::vector<UsedTexture> used;collectUsedTextures(used);
    if(std::any_of(used.begin(),used.end(),[&](const auto &item){return item.guid==identity.guid;})||!next.remove(identity.guid)) {
      diagnostic="Textura ainda usada; desfaça os vínculos antes da importação.";return false;
    }
  } else if(current) {
    if(!next.publishImport(target->guid,target->contentHash,target->importerVersion,target->importerParameters,target->derived,target->dependencies))return false;
  } else if(!next.add(*target))return false;
  std::string index;
  if(!prepareLibraryIndex(next,index,diagnostic))return false;
  resources::TextureProfile targetProfile;
  if(target&&resources::isTiff(source)&&!resources::parseTextureProfile(target->importerParameters,targetProfile)) {
    diagnostic="Receita TIFF inválida; publicação preservada.";return false;
  }
  if(!texture_history::sourceMetadata(index,identity.path,source,target?&targetProfile:nullptr,importLimits_.image,diagnostic))return false;
  EditorImportTransaction transaction(files_.rootPath());
  if(!transaction.begin(identity.path,expectedDiskHash,diagnostic,LibraryIndexPath))return false;

  const auto previousAssets=assets_;const auto previousProfiles=textureProfiles_;
  const auto previousDecoded=decodedTextures_;const auto previousViewer=viewerChain_;
  const auto previousDirty=assetRegistryDirty_;
  bool attemptedPublication=false;
  const auto restore=[&]() {
    const auto reason=diagnostic;
    const bool diskRestored=transaction.rollback();
    assets_=previousAssets;textureProfiles_=previousProfiles;decodedTextures_=previousDecoded;
    viewerChain_=previousViewer;assetRegistryDirty_=previousDirty;
    diagnostic=reason;
    if(!diskRestored)diagnostic+=" Recuperação de disco pendente; backups preservados.";
    if(attemptedPublication) {
      std::string error;
      if(!publishAndAdopt(flattenSources(importedSources_),error))diagnostic+=" Publicação gráfica anterior pendente: "+error;
    }
  };
  assets_=std::move(next);
  std::erase_if(decodedTextures_,[&](const auto &entry){return entry.guid==identity.guid;});
  std::erase_if(textureProfiles_,[&](const auto &entry){return entry.first==identity.guid;});
  if(viewerChain_.guid==identity.guid)viewerChain_={};
  if(target) {
    resources::TextureProfile profile;
    if(!resources::parseTextureProfile(target->importerParameters,profile)) {
      std::filesystem::path legacy;std::vector<u8> bytes;
      if(EditorImportTransaction::safePath(root,resources::textureProfilePath(identity.guid),legacy)&&
         EditorImportTransaction::read(legacy,bytes,65536))
        resources::parseTextureProfile(std::string(bytes.begin(),bytes.end()),profile);
    }
    textureProfiles_.emplace_back(identity.guid,profile);
    resources::TextureImportLimits limits;limits.image=importLimits_.image;
    limits.projectMaximumDimension=importLimits_.maximumTextureDimension;
    std::vector<UsedTexture> used;collectUsedTextures(used);bool consumed=false;
    for(const auto &item:used)if(item.guid==identity.guid) {
      auto effective=profile;
      if(item.normal&&effective.interpretation==resources::TextureInterpretationUse)
        effective.interpretation=resources::TextureInterpretationNormal;
      resources::PreparedTextureImport prepared;
      if(!resources::prepareTextureImport(source,effective,item.srgb,item.sampler,limits,nullptr,prepared)) {
        diagnostic="Binding efetivo da textura recusado: "+prepared.diagnostic;restore();return false;
      }
      decodedTextures_.push_back({identity.guid,item.srgb,item.normal,prepared.texture->samplerFlags,identity.contentHash,prepared.texture});
      consumed=true;
    }
    if(consumed) {
      attemptedPublication=true;
      if(!publishAndAdopt(flattenSources(importedSources_),diagnostic)) {restore();return false;}
    }
  }
  if(!transaction.commit(source,assets_.serialize(),index,&diagnostic)) {restore();return false;}
  assetRegistryDirty_=false;
  loadTextureAssets();loadLibraryIndex(diagnostic);files_.rebuildTree();appearanceChanged_=true;
  if(!target)state_.textureViewer=false;
  return true;
}
}
