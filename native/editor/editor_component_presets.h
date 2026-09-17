#pragma once
#include "editor/editor_import_transaction.h"
#include "scene/component_schema.h"

namespace ae::editor {
struct EditorComponentPreset {
  u64 id=0;
  std::string name,type,payload;
  u32 version=0;
};
// Project-local values, never object identities. Unknown records are retained
// verbatim on save; only a registered, supported version may be instantiated.
class EditorComponentPresets {
public:
  static constexpr const char *Path=".astra/component-presets.astra";
  std::vector<EditorComponentPreset> entries;
  bool load(const std::string &root,std::string &error) {
    entries.clear();root_=root;original_.clear();loaded_=false;existed_=false;
    std::filesystem::path path;
    if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),Path,path)) {error="Projeto indisponível";return false;}
    std::error_code ec;existed_=std::filesystem::exists(path,ec);
    if(ec) {error="Não foi possível consultar os presets";return false;}
    if(!existed_) {loaded_=true;return true;}
    std::vector<u8> bytes;
    if(!EditorImportTransaction::read(path,bytes,8u*1024u*1024u)) {error="Não foi possível ler os presets";return false;}
    original_.assign(bytes.begin(),bytes.end());std::istringstream input(original_);input.imbue(std::locale::classic());
    std::string magic;u32 count=0;
    if(!(input>>magic>>count)||magic!="ASTRA_COMPONENT_PRESETS_1"||count>256) {error="Biblioteca de presets inválida";return false;}
    std::vector<EditorComponentPreset> candidate;
    for(u32 i=0;i<count;++i) {
      EditorComponentPreset record;
      if(!(input>>record.id>>std::quoted(record.name)>>std::quoted(record.type)>>record.version>>std::quoted(record.payload)) ||
          !record.id||!validName(record.name)||record.type.empty()||record.type.size()>256||!record.version||record.payload.size()>1024u*1024u) {
        error="Registro de preset inválido";return false;
      }
      for(const auto &other:candidate) if(other.id==record.id) {error="Identidade de preset duplicada";return false;}
      candidate.push_back(std::move(record));
    }
    input>>std::ws;if(!input.eof()) {error="Dados extras na biblioteca de presets";return false;}
    entries=std::move(candidate);loaded_=true;return true;
  }
  const EditorComponentPreset *find(u64 id) const {for(const auto &entry:entries) if(entry.id==id) return &entry;return nullptr;}
  static bool validName(std::string_view name) {
    if(name.empty()||name.size()>96||name.find_first_not_of(' ')==std::string_view::npos) return false;
    for(unsigned char c:name) if(c<32||c==127) return false;
    return true;
  }
  bool capture(std::string name,const scene::ComponentValue &source,std::string &error) {
    if(!loaded_||entries.size()>=256||!validName(name)||source.unresolved()||!source.valid()) {error="Nome ou componente inválido; limite de 256 presets";return false;}
    if(!scene::findComponentSchema(source.type())||source.type().id=="astra.script.behavior") {error="Este tipo ainda não possui preset portátil";return false;}
    auto copy=source.clone();
    for(const auto &reference:copy->type().references) if(reference.write) reference.write(*copy,0);
    std::ostringstream data;data.imbue(std::locale::classic());data<<std::setprecision(std::numeric_limits<float>::max_digits10);copy->write(data);
    if(!data||!copy->valid()||data.str().size()>1024u*1024u) {error="Valores inválidos para preset";return false;}
    u64 id=1;for(const auto &entry:entries) {if(entry.id==std::numeric_limits<u64>::max()) {error="Limite de identidades";return false;}id=std::max(id,entry.id+1);}
    auto candidate=entries;candidate.push_back({id,std::move(name),std::string(copy->type().id),data.str(),copy->type().version});
    return publish(std::move(candidate),error);
  }
  bool rename(u64 id,std::string name,std::string &error) {
    if(!validName(name)) {error="Use um nome de até 96 bytes";return false;}
    auto candidate=entries;for(auto &entry:candidate) if(entry.id==id) {entry.name=std::move(name);return publish(std::move(candidate),error);}
    error="Preset ausente";return false;
  }
  bool erase(u64 id,std::string &error) {
    auto candidate=entries;const auto at=std::find_if(candidate.begin(),candidate.end(),[&](const auto &entry){return entry.id==id;});
    if(at==candidate.end()) {error="Preset ausente";return false;}
    candidate.erase(at);return publish(std::move(candidate),error);
  }
  std::unique_ptr<scene::ComponentValue> instantiate(u64 id,std::string &error) const {
    const auto *entry=find(id);const auto *schema=entry?scene::findComponentSchema(entry->type):nullptr;
    if(!schema||entry->type=="astra.script.behavior") {error="Tipo de preset indisponível";return {};}
    std::ostringstream envelope;envelope<<"1 "<<std::quoted(entry->type)<<' '<<entry->version<<' '<<std::quoted(entry->payload);
    std::istringstream input(envelope.str());const scene::ComponentType *registry[]{schema->type};scene::Components parsed;
    if(!parsed.read(input,registry)||parsed.size()!=1) {error="Versão ou valores do preset incompatíveis";return {};}
    auto value=parsed.at(0)->clone();
    for(const auto &reference:value->type().references) if(reference.write) reference.write(*value,0);
    return value;
  }
private:
  bool publish(std::vector<EditorComponentPreset> candidate,std::string &error) {
    if(!loaded_) {error="Reabra a biblioteca de presets";return false;}
    std::filesystem::path path;
    if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root_),Path,path)) {error="Caminho de presets inválido";return false;}
    std::error_code ec;const bool exists=std::filesystem::exists(path,ec);std::vector<u8> bytes;
    if(ec||exists!=existed_||(exists&&(!EditorImportTransaction::read(path,bytes,8u*1024u*1024u)||std::string(bytes.begin(),bytes.end())!=original_))) {
      error="Presets alterados externamente; reabra o painel";return false;
    }
    std::ostringstream out;out.imbue(std::locale::classic());out<<"ASTRA_COMPONENT_PRESETS_1 "<<candidate.size()<<'\n';
    for(const auto &entry:candidate) out<<entry.id<<' '<<std::quoted(entry.name)<<' '<<std::quoted(entry.type)<<' '<<entry.version<<' '<<std::quoted(entry.payload)<<'\n';
    const auto serialized=out.str();
    if(serialized.size()>8u*1024u*1024u) {error="Biblioteca excede 8 MiB";return false;}
    std::filesystem::create_directories(path.parent_path(),ec);
    if(ec||!EditorImportTransaction::writeText(path,serialized)) {error="Não foi possível salvar os presets";return false;}
    entries=std::move(candidate);original_=serialized;existed_=true;return true;
  }
  std::string root_,original_;
  bool loaded_=false,existed_=false;
};
} // namespace ae::editor
