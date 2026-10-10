#pragma once
#include "editor/editor_import_transaction.h"
#include "resources/asset_registry.h"
#include "resources/library_index.h"
#include "resources/tiff_decode.h"
#include "resources/texture_profile.h"
#include <algorithm>

namespace ae::editor::texture_history {
// Rebuild only TIFF conversion losses in the candidate index. This runs before
// the same journal commit for import, profile edits, Undo and Redo.
inline bool sourceMetadata(std::string &encoded,const std::string &path,std::span<const u8> source,
    const resources::TextureProfile *profile,const resources::ImageDecodeLimits &limits,std::string &diagnostic) {
  const bool tiff=resources::isTiff(source);
  std::vector<std::string> losses;
  if(profile&&tiff) {
    resources::TiffInfo info;
    if(!resources::inspectTiff(source,limits,info,diagnostic)||
        !resources::tiffPreparationLosses(info,{static_cast<resources::TiffUndefinedExtra>(profile->tiffUndefinedExtra),
            profile->tiffQuantize16,profile->tiffIgnoreColorProfile},losses,diagnostic))return false;
  }
  resources::LibraryIndex catalog;
  if(!resources::LibraryIndex::deserialize(encoded,catalog,diagnostic))return false;
  const auto packages=std::vector<resources::LibraryPackage>(catalog.packages().begin(),catalog.packages().end());
  for(auto package:packages) {
    bool changed=false;
    for(auto &item:package.items)if(item.path==path) {
      changed=true;
      std::erase_if(item.knownLosses,[](const std::string &loss) {
        return loss.starts_with("Quarto canal TIFF ")||loss.starts_with("Canais TIFF ")||loss.starts_with("Perfil ICC TIFF ");
      });
      item.knownLosses.insert(item.knownLosses.end(),losses.begin(),losses.end());
      if(profile&&tiff) {item.converter="astra-tiff-classic";item.diagnostic="TIFF preparado com interpretação explícita; perdas registradas na receita.";}
    }
    if(changed&&!catalog.replace(std::move(package))) {diagnostic="Perdas TIFF excedem o contrato da biblioteca.";return false;}
  }
  encoded=catalog.serialize();return !encoded.empty();
}
inline bool sameRecord(const resources::AssetRecord &a,const resources::AssetRecord &b) {
  return a.guid==b.guid&&a.type==b.type&&a.path==b.path&&a.source==b.source&&a.contentHash==b.contentHash&&
      a.importerVersion==b.importerVersion&&a.importerParameters==b.importerParameters&&
      a.dependencies==b.dependencies&&a.derived==b.derived;
}
inline bool persisted(const std::filesystem::path &root,const resources::AssetRecord *expected,
                      const std::string &path) {
  std::error_code error;const auto registryPath=root/".astra/assets.astra";
  if(!std::filesystem::exists(registryPath,error))return !error&&!expected;
  std::vector<u8> bytes;resources::AssetRegistry registry;
  if(!EditorImportTransaction::read(registryPath,bytes,32u*1024u*1024u)||
      !resources::AssetRegistry::deserialize(std::string(bytes.begin(),bytes.end()),registry))return false;
  const auto *record=registry.findByPath(path);
  return expected?(record&&sameRecord(*record,*expected)):!record;
}
inline std::string snapshotPath(const std::string &hash) {
  return ".astra/imports/texture-sources/"+hash+".source";
}
// Immutable encoded sources are authoring history, distinct from regenerable
// GPU caches. Orphan snapshots after a refused journal are harmless; no live
// registry or history entry can refer to them before a successful commit.
inline bool snapshot(const std::filesystem::path &root,const std::string &hash,std::span<const u8> bytes,
                     std::string &diagnostic) {
  if(hash.size()!=64||Sha256::hex(bytes)!=hash)return false;
  std::filesystem::path path;if(!EditorImportTransaction::safePath(root,snapshotPath(hash),path))return false;
  std::error_code error;
  if(std::filesystem::exists(path,error)) {
    std::vector<u8> current;
    if(!error&&EditorImportTransaction::read(path,current,bytes.size())&&Sha256::hex(current)==hash)return true;
    diagnostic="A cópia autoral da textura mudou; histórico preservado sem sobrescrever.";return false;
  }
  std::filesystem::create_directories(path.parent_path(),error);
  if(error||!EditorImportTransaction::write(path,bytes,&diagnostic))return false;
  return true;
}
inline bool readSnapshot(const std::filesystem::path &root,const std::string &hash,usize maximum,
                         std::vector<u8> &bytes) {
  std::filesystem::path path;
  return hash.size()==64&&EditorImportTransaction::safePath(root,snapshotPath(hash),path)&&
      EditorImportTransaction::read(path,bytes,maximum)&&Sha256::hex(bytes)==hash;
}
}
