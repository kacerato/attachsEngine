#pragma once
#include "editor/editor_import_transaction.h"
#include "editor/editor_archive.h"
#include "runtime/object_tags.h"

namespace ae::editor {
inline bool loadProjectTags(const std::string &root,runtime::ObjectTags &tags,std::string &error) {
  std::filesystem::path file;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),".astra/tags.astra",file)) {
    error="Caminho inválido para o catálogo de tags";return false;
  }
  std::error_code ec;const bool exists=std::filesystem::exists(file,ec);
  if(ec) {error="Não foi possível ler o catálogo de tags";return false;}
  if(!exists) {tags={};return true;}
  std::vector<u8> bytes;
  if(!EditorImportTransaction::read(file,bytes,32768)) {error="Catálogo de tags ilegível ou acima do limite";return false;}
  std::istringstream in(std::string(bytes.begin(),bytes.end()));
  if(!tags.read(in)) {error="Catálogo de tags inválido; o arquivo foi preservado";return false;}
  return true;
}
inline bool saveProjectTags(const std::string &root,const runtime::ObjectTags &expected,
                            const runtime::ObjectTags &next,std::string &error) {
  runtime::ObjectTags disk;
  if(!loadProjectTags(root,disk,error)) return false;
  if(disk!=expected) {error="Tags mudaram no disco; reabra o projeto antes de editar";return false;}
  std::filesystem::path file;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),".astra/tags.astra",file)) return false;
  std::error_code ec;std::filesystem::create_directories(file.parent_path(),ec);
  std::ostringstream out;next.write(out);
  if(ec || !EditorImportTransaction::writeText(file,out.str())) {error="Não foi possível salvar as tags; nada aplicado";return false;}
  return true;
}
// Excluir uma definição não pode deixar cenas fechadas com referência órfã.
// Em dúvida (arquivo inválido, symlink, limite), a remoção é recusada.
inline bool projectTagUnused(const std::string &root,const EditorDocument &current,
                             std::string_view tag,std::string &error) {
  const auto uses=[&](const EditorDocument &doc) {
    std::vector<EditorEntityId> ids;doc.collectSubtree(doc.root(),ids);
    for(const auto id:ids) if(doc.find(id)->tag==tag) return true;
    return false;
  };
  if(uses(current)) {error="Tag em uso na cena aberta";return false;}
  std::error_code ec;usize inspected=0;
  for(std::filesystem::recursive_directory_iterator it(EditorImportTransaction::fromUtf8(root),ec),end;
      !ec && it!=end;it.increment(ec)) {
    const auto status=it->symlink_status(ec);if(ec) break;
    if(std::filesystem::is_symlink(status)) {
      error="Link simbólico impede conferir todas as cenas; tag preservada";return false;
    }
    if(it->is_directory(ec) && it->path().filename()==".astra") {it.disable_recursion_pending();continue;}
    if(it->path().extension()!=".aescene" || !it->is_regular_file(ec)) continue;
    if(++inspected>1024) {error="Muitas cenas para conferir a exclusão da tag";return false;}
    std::vector<u8> bytes;
    if(!EditorImportTransaction::read(it->path(),bytes,32*1024*1024)) {error="Uma cena não pôde ser conferida; tag preservada";return false;}
    const std::string text(bytes.begin(),bytes.end());std::istringstream header(text);
    std::string magic;u32 version=0;u64 fingerprint=0;EditorDocument scene;
    if(!(header>>magic>>version>>fingerprint) || !deserializeEditorDocument(text,fingerprint,scene)) {
      error="Uma cena está inválida; tag preservada";return false;
    }
    if(uses(scene)) {error="Tag em uso numa cena salva; remova a atribuição e salve antes de excluir";return false;}
  }
  if(ec) {error="Não foi possível conferir todas as cenas; tag preservada";return false;}
  return true;
}
}
