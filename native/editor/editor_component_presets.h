#pragma once
#include "editor/editor_import_transaction.h"
#include "scene/component_schema.h"

namespace ae::editor {
// Um componente dentro de um preset. Tipo e versão são contrato de arquivo; o
// payload é o mesmo texto que a serialização do componente produz.
struct EditorComponentPresetEntry {
  std::string type,payload;
  u32 version=0;
};
// Um preset é uma LISTA ordenada de componentes, e não um componente só.
//
// Com uma entrada, é o preset de componente que sempre existiu. Com várias, é a
// receita que o plano universal pede: "conjunto ordenado de componentes,
// parâmetros, referências internas e requisitos externos declarados" — uma
// porta interativa é colisor + corpo + junta + comportamento, e salvar isso em
// quatro presets separados transfere para o autor o trabalho de lembrar a
// ordem, as exigências e o que combina com o quê.
//
// O que NÃO entra: identidade de objeto de cena. Referências são zeradas na
// captura; um preset que levasse ObjectId apontaria, no destino, para o objeto
// que por acaso tivesse aquele número.
struct EditorComponentPreset {
  u64 id=0;
  std::string name;
  std::vector<EditorComponentPresetEntry> entries;
  bool recipe() const noexcept {return entries.size()>1;}
  std::string_view firstType() const noexcept {return entries.empty()?std::string_view{}:std::string_view(entries.front().type);}
  bool contains(std::string_view type) const noexcept {
    for(const auto &entry:entries) if(entry.type==type) return true;
    return false;
  }
};
// Project-local values, never object identities. Unknown records are retained
// verbatim on save; only a registered, supported version may be instantiated.
class EditorComponentPresets {
public:
  static constexpr const char *Path=".astra/component-presets.astra";
  static constexpr const char *Magic="ASTRA_COMPONENT_PRESETS_2";
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
    // A versão 1 tinha exatamente um componente por registro. Ela continua
    // sendo lida: uma biblioteca antiga não pode virar "arquivo inválido"
    // porque o formato passou a aceitar receitas.
    if(!(input>>magic>>count)||(magic!="ASTRA_COMPONENT_PRESETS_1"&&magic!=Magic)||count>256) {error="Biblioteca de presets inválida";return false;}
    const bool legacy=magic=="ASTRA_COMPONENT_PRESETS_1";
    std::vector<EditorComponentPreset> candidate;
    for(u32 i=0;i<count;++i) {
      EditorComponentPreset record;u32 parts=1;
      if(!(input>>record.id>>std::quoted(record.name))||!record.id||!validName(record.name)) {error="Registro de preset inválido";return false;}
      if(!legacy&&(!(input>>parts)||!parts||parts>scene::Components::MaximumCount)) {error="Registro de preset inválido";return false;}
      for(u32 part=0;part<parts;++part) {
        EditorComponentPresetEntry entry;
        if(!(input>>std::quoted(entry.type)>>entry.version>>std::quoted(entry.payload)) ||
            entry.type.empty()||entry.type.size()>256||!entry.version||entry.payload.size()>1024u*1024u) {
          error="Registro de preset inválido";return false;
        }
        // Dois componentes do mesmo tipo numa receita só fazem sentido quando o
        // tipo aceita múltiplas instâncias; recusar aqui evita uma receita que
        // nunca poderia ser aplicada.
        const auto *schema=scene::findComponentSchema(entry.type);
        for(const auto &other:record.entries)
          if(other.type==entry.type&&(!schema||!schema->allowMultiple())) {error="Receita com componente repetido";return false;}
        record.entries.push_back(std::move(entry));
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
  // Serializa um componente para o formato do preset, zerando as referências de
  // cena. Falha quando o tipo não tem contrato portátil.
  static bool encode(const scene::ComponentValue &source,EditorComponentPresetEntry &out,std::string &error) {
    if(source.unresolved()||!source.valid()) {error="Componente inválido para preset";return false;}
    if(!scene::findComponentSchema(source.type())||source.type().id=="astra.script.behavior") {error="Este tipo ainda não possui preset portátil";return false;}
    auto copy=source.clone();
    for(const auto &reference:copy->type().references) if(reference.write) reference.write(*copy,0);
    std::ostringstream data;data.imbue(std::locale::classic());data<<std::setprecision(std::numeric_limits<float>::max_digits10);copy->write(data);
    if(!data||!copy->valid()||data.str().size()>1024u*1024u) {error="Valores inválidos para preset";return false;}
    out={std::string(copy->type().id),data.str(),copy->type().version};return true;
  }
  bool capture(std::string name,const scene::ComponentValue &source,std::string &error) {
    EditorComponentPresetEntry entry;
    if(!encode(source,entry,error)) return false;
    return store(std::move(name),{std::move(entry)},error);
  }
  // A receita: vários componentes do MESMO objeto, na ordem em que estão nele.
  // A ordem é preservada porque é ela que decide qual componente resolve a
  // exigência de qual, e um Add fora de ordem pediria dependências que a própria
  // receita traz logo em seguida.
  bool captureRecipe(std::string name,const scene::Components &components,std::string &error) {
    std::vector<EditorComponentPresetEntry> parts;
    for(usize i=0;i<components.size();++i) {
      const auto *value=components.at(i);
      // Um objeto costuma ter componentes sem preset portátil (comportamento em
      // C#, tipo não registrado). Eles ficam de fora, e a receita continua
      // valendo para o resto — recusar a captura inteira seria pior.
      EditorComponentPresetEntry entry;std::string ignored;
      if(encode(*value,entry,ignored)) parts.push_back(std::move(entry));
    }
    if(parts.empty()) {error="Nenhum componente deste objeto tem preset portátil";return false;}
    return store(std::move(name),std::move(parts),error);
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
  static std::unique_ptr<scene::ComponentValue> decode(const EditorComponentPresetEntry &entry,std::string &error) {
    const auto *schema=scene::findComponentSchema(entry.type);
    if(!schema||entry.type=="astra.script.behavior") {error="Tipo de preset indisponível";return {};}
    std::ostringstream envelope;envelope<<"1 "<<std::quoted(entry.type)<<' '<<entry.version<<' '<<std::quoted(entry.payload);
    std::istringstream input(envelope.str());const scene::ComponentType *registry[]{schema->type};scene::Components parsed;
    if(!parsed.read(input,registry)||parsed.size()!=1) {error="Versão ou valores do preset incompatíveis";return {};}
    auto value=parsed.at(0)->clone();
    for(const auto &reference:value->type().references) if(reference.write) reference.write(*value,0);
    return value;
  }
  std::unique_ptr<scene::ComponentValue> instantiate(u64 id,std::string &error) const {
    const auto *entry=find(id);
    if(!entry||entry->entries.size()!=1) {error=entry?"Este preset é uma receita de vários componentes":"Preset ausente";return {};}
    return decode(entry->entries.front(),error);
  }
  // Toda a receita, na ordem gravada. Vazio com `error` preenchido quando
  // qualquer componente não puder ser reconstruído: aplicar meia receita
  // deixaria o objeto num estado que ninguém pediu.
  std::vector<std::unique_ptr<scene::ComponentValue>> instantiateAll(u64 id,std::string &error) const {
    std::vector<std::unique_ptr<scene::ComponentValue>> values;
    const auto *entry=find(id);
    if(!entry) {error="Preset ausente";return values;}
    for(const auto &part:entry->entries) {
      auto value=decode(part,error);
      if(!value) {values.clear();return values;}
      values.push_back(std::move(value));
    }
    return values;
  }
private:
  bool store(std::string name,std::vector<EditorComponentPresetEntry> parts,std::string &error) {
    if(!loaded_||entries.size()>=256||!validName(name)) {error="Nome inválido ou limite de 256 presets";return false;}
    u64 id=1;for(const auto &entry:entries) {if(entry.id==std::numeric_limits<u64>::max()) {error="Limite de identidades";return false;}id=std::max(id,entry.id+1);}
    auto candidate=entries;candidate.push_back({id,std::move(name),std::move(parts)});
    return publish(std::move(candidate),error);
  }
  bool publish(std::vector<EditorComponentPreset> candidate,std::string &error) {
    if(!loaded_) {error="Reabra a biblioteca de presets";return false;}
    std::filesystem::path path;
    if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root_),Path,path)) {error="Caminho de presets inválido";return false;}
    std::error_code ec;const bool exists=std::filesystem::exists(path,ec);std::vector<u8> bytes;
    if(ec||exists!=existed_||(exists&&(!EditorImportTransaction::read(path,bytes,8u*1024u*1024u)||std::string(bytes.begin(),bytes.end())!=original_))) {
      error="Presets alterados externamente; reabra o painel";return false;
    }
    std::ostringstream out;out.imbue(std::locale::classic());out<<Magic<<' '<<candidate.size()<<'\n';
    for(const auto &entry:candidate) {
      out<<entry.id<<' '<<std::quoted(entry.name)<<' '<<entry.entries.size();
      for(const auto &part:entry.entries) out<<' '<<std::quoted(part.type)<<' '<<part.version<<' '<<std::quoted(part.payload);
      out<<'\n';
    }
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
