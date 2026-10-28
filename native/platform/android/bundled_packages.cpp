#include "platform/android/bundled_packages.h"
#include "editor/editor_import_transaction.h"
#include "core/sha256.h"

#include <android/asset_manager.h>
#include <fstream>

namespace ae::platform::android {

std::vector<editor::BundledPackage> readBundledPackages(AAssetManager *assets,std::string &diagnostic) {
  diagnostic.clear();std::vector<editor::BundledPackage> packages;
  if(!assets)return packages;
  AAsset *asset=AAssetManager_open(assets,editor::BundledPackageIndexAsset,AASSET_MODE_BUFFER);
  if(!asset)return packages;
  const auto length=AAsset_getLength64(asset);std::string text;
  if(length>0&&length<=(16ll<<20)) {
    text.resize(static_cast<usize>(length));
    if(AAsset_read(asset,text.data(),text.size())!=static_cast<int>(text.size()))text.clear();
  }
  AAsset_close(asset);
  if(text.empty()){diagnostic="Índice de pacotes incluídos ilegível.";return packages;}
  editor::parseBundledPackageIndex(text,packages,diagnostic);
  return packages;
}

bool extractBundledPackage(AAssetManager *assets,const editor::BundledPackage &package,const std::filesystem::path &directory,
    const std::function<bool()> &cancelled,std::atomic<u64> &done,std::string &diagnostic) {
  diagnostic.clear();
  if(editor::bundledPackageExtracted(directory,package)) {done+=package.bytes;return true;}
  std::error_code error;
  // Extração anterior incompleta ou de outra versão do pacote: recomeça limpa.
  std::filesystem::remove_all(directory,error);error.clear();
  std::filesystem::create_directories(directory,error);
  if(error){diagnostic="Não foi possível criar a pasta de pacotes do app.";return false;}
  std::vector<char> buffer(1u<<20);
  for(const auto &file:package.files) {
    if(cancelled&&cancelled()){diagnostic="Importação do pacote cancelada.";return false;}
    std::filesystem::path target;
    if(!editor::EditorImportTransaction::safePath(directory,file.relative,target)){diagnostic="Caminho fora do pacote: "+file.relative;return false;}
    std::filesystem::create_directories(target.parent_path(),error);
    if(error){diagnostic="Não foi possível criar a pasta de pacotes do app.";return false;}
    AAsset *asset=AAssetManager_open(assets,file.asset.c_str(),AASSET_MODE_STREAMING);
    if(!asset){diagnostic="Arquivo do pacote ausente no APK: "+file.relative;return false;}
    auto partial=target;partial+=".parcial";
    std::ofstream out(partial,std::ios::binary|std::ios::trunc);
    Sha256 hash;u64 bytes=0;bool ok=static_cast<bool>(out);
    while(ok) {
      const int count=AAsset_read(asset,buffer.data(),buffer.size());
      if(count<0){ok=false;break;}
      if(count==0)break;
      hash.update({reinterpret_cast<const u8*>(buffer.data()),static_cast<usize>(count)});
      out.write(buffer.data(),count);bytes+=static_cast<u64>(count);done+=static_cast<u64>(count);
      ok=static_cast<bool>(out);
    }
    AAsset_close(asset);out.close();
    if(!ok||!out||bytes!=file.bytes||Sha256::hex(hash.digest())!=file.sha256) {
      std::filesystem::remove(partial,error);diagnostic="Arquivo do pacote no APK não confere: "+file.relative;return false;
    }
    std::filesystem::rename(partial,target,error);
    if(error){diagnostic="Não foi possível gravar o pacote: "+file.relative;return false;}
  }
  if(!editor::writeBundledPackageMarker(directory,package)){diagnostic="Não foi possível marcar o pacote extraído.";return false;}
  return true;
}
}
