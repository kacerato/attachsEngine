#pragma once
#include "editor/editor_import_transaction.h"
#include "resources/terrain_asset.h"
#include "resources/terrain_history_store.h"

namespace ae::editor {
inline bool terrainRecoveryHash(std::string_view hash) {
  return hash.size()==64 && hash.find_first_not_of("0123456789abcdef")==std::string_view::npos;
}
inline bool terrainRecoveryPaths(const std::string &project, std::string_view hash,
    std::filesystem::path &objects, std::filesystem::path &reference) {
  const auto root=EditorImportTransaction::fromUtf8(project);
  return terrainRecoveryHash(hash) &&
      EditorImportTransaction::safePath(root,".astra/terrain-history/objects",objects) &&
      EditorImportTransaction::safePath(root,".astra/terrain-history/revisions/"+std::string(hash)+".ref",reference);
}
inline bool readTerrainRecovery(const std::string &project, std::string_view hash,
    resources::TerrainRecipe &recipe, std::string &error) {
  std::filesystem::path objects, reference;
  std::vector<u8> bytes;
  if(!terrainRecoveryPaths(project,hash,objects,reference) ||
      !EditorImportTransaction::read(reference,bytes,64))return false;
  const std::string id(bytes.begin(),bytes.end());
  if(!terrainRecoveryHash(id))return false;
  std::error_code ec;
  if(!std::filesystem::is_directory(objects,ec) || ec)return false;
  resources::TerrainHistoryStore store(objects);
  std::string text;
  return store.read(id,text) && resources::deserializeTerrainRecipe(text,recipe,error);
}
// Store the authoring recipe before publishing the immutable GLB. A failed
// publication may leave an unreferenced checkpoint, never an edited scene.
inline bool writeTerrainRecovery(const std::string &project, std::string_view hash,
    const resources::TerrainRecipe &recipe, std::string_view previousHash,
    std::string &error) {
  std::filesystem::path objects, reference;
  if(!terrainRecoveryPaths(project,hash,objects,reference)) {
    error="Caminho de recuperação do terreno inválido.";return false;
  }
  resources::TerrainHistoryStore store(objects,{8u<<20,16});
  std::size_t recovered=0;
  if(!store.recover(recovered)) {
    error="Não foi possível recuperar o armazenamento do histórico.";return false;
  }
  const auto text=resources::serializeTerrainRecipe(recipe);
  std::error_code ec;
  if(std::filesystem::exists(reference,ec)) {
    resources::TerrainRecipe saved;
    if(!readTerrainRecovery(project,hash,saved,error) || resources::serializeTerrainRecipe(saved)!=text) {
      error="Checkpoint existente inválido; revisão anterior preservada.";return false;
    }
    return true;
  }
  if(ec) {error="Não foi possível conferir o checkpoint do terreno.";return false;}
  std::string base;
  if(!previousHash.empty()) {
    std::filesystem::path oldObjects, oldReference;
    if(!terrainRecoveryPaths(project,previousHash,oldObjects,oldReference))return false;
    if(std::filesystem::exists(oldReference,ec)) {
      std::vector<u8> bytes;
      if(!EditorImportTransaction::read(oldReference,bytes,64))return false;
      base.assign(bytes.begin(),bytes.end());
    }
    if(ec)return false;
  }
  std::string id;
  if(!store.put(text,base,id)) {
    error="Falha ao gravar checkpoint/delta; terreno e histórico preservados.";return false;
  }
  std::filesystem::create_directories(reference.parent_path(),ec);
  std::filesystem::path temporary;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(project),
      ".astra/terrain-history/revisions/"+std::string(hash)+".ref.aether-tmp",temporary)) {
    error="Temporário do checkpoint inválido.";return false;
  }
  if(ec || !EditorImportTransaction::writeText(reference,id,&error))return false;
  return true;
}
inline bool readTerrainAuthoring(const std::string &project, const std::string &path,
    const std::string &hash, resources::TerrainRecipe &recipe, std::string &error,
    bool &recovered) {
  recovered=false;
  std::filesystem::path absolute;
  std::vector<u8> bytes;
  if(EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(project),path,absolute) &&
      EditorImportTransaction::read(absolute,bytes,64u<<20) &&
      terrainRecoveryHash(hash) && Sha256::hex(bytes)==hash &&
      resources::terrainRecipeFromGlb(bytes,recipe,error))return true;
  if(!readTerrainRecovery(project,hash,recipe,error))return false;
  recovered=true;
  error.clear();
  return true;
}
} // namespace ae::editor
