#pragma once
#include "editor/editor_import_transaction.h"
#include "scene/component_schema.h"
#include <span>

namespace ae::editor {
struct EditorRecipeReference {
  std::string property;
  u32 input=0; // 0: object receiving the recipe; 1..N: explicit object input.
};
// Um componente dentro de um preset. Tipo e versão são contrato de arquivo; o
// payload é o mesmo texto que a serialização do componente produz.
struct EditorComponentPresetEntry {
  std::string type,payload;
  u32 version=0;
  std::vector<EditorRecipeReference> references{};
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
// ObjectId não viaja no recurso: receitas declaram endereços próprios ou
// entradas externas. O destino resolve essas entradas antes da transação.
struct EditorComponentPreset {
  u64 id=0;
  std::string name;
  std::vector<EditorComponentPresetEntry> entries;
  std::vector<std::string> inputs{};
  bool recipe() const noexcept {
    return entries.size()>1 || std::any_of(entries.begin(),entries.end(),[](const auto &e){return !e.references.empty();});
  }
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
  static constexpr const char *Magic="ASTRA_COMPONENT_PRESETS_3";
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
    if(!(input>>magic>>count)||(magic!="ASTRA_COMPONENT_PRESETS_1"&&magic!="ASTRA_COMPONENT_PRESETS_2"&&magic!=Magic)||count>256) {error="Biblioteca de presets inválida";return false;}
    const bool legacy=magic=="ASTRA_COMPONENT_PRESETS_1";
    const bool bindings=magic==Magic;
    std::vector<EditorComponentPreset> candidate;
    for(u32 i=0;i<count;++i) {
      EditorComponentPreset record;u32 parts=1;
      if(!(input>>record.id>>std::quoted(record.name))||!record.id||!validName(record.name)) {error="Registro de preset inválido";return false;}
      if(!legacy&&(!(input>>parts)||!parts||parts>scene::Components::MaximumCount)) {error="Registro de preset inválido";return false;}
      if(bindings) {
        u32 inputs=0;if(!(input>>inputs)||inputs>256){error="Entradas de receita inválidas";return false;}
        for(u32 j=0;j<inputs;++j){std::string name;if(!(input>>std::quoted(name))||!validName(name)){error="Nome de entrada inválido";return false;}record.inputs.push_back(std::move(name));}
      }
      for(u32 part=0;part<parts;++part) {
        EditorComponentPresetEntry entry;
        if(!(input>>std::quoted(entry.type)>>entry.version>>std::quoted(entry.payload)) ||
            entry.type.empty()||entry.type.size()>256||!entry.version||entry.payload.size()>1024u*1024u) {
          error="Registro de preset inválido";return false;
        }
        if(bindings) {
          u32 references=0;if(!(input>>references)||references>256){error="Referências de receita inválidas";return false;}
          for(u32 j=0;j<references;++j){EditorRecipeReference r;
            if(!(input>>std::quoted(r.property)>>r.input)||r.property.empty()||r.property.size()>256||r.input>record.inputs.size()||
               std::any_of(entry.references.begin(),entry.references.end(),[&](const auto &v){return v.property==r.property;})) {error="Endereço de referência inválido";return false;}
            entry.references.push_back(std::move(r));
          }
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
  bool captureRecipe(std::string name,const scene::Components &components,std::string &error,u64 sourceObject=0) {
    std::vector<EditorComponentPresetEntry> parts;
    std::vector<u64> targets;std::vector<std::string> inputs;
    for(usize i=0;i<components.size();++i) {
      const auto *value=components.at(i);
      // Ownership belongs to the scene, not this reusable component recipe.
      if(value->type().id=="astra.prefab.link" || value->type().id=="astra.import.link")continue;
      EditorComponentPresetEntry entry;
      if(!encode(*value,entry,error)) {error="Receita preservada: "+std::string(value->type().id)+" — "+error+"; use prefab para a composição completa";return false;}
      for(const auto &reference:value->type().references)if(reference.read) {
        const auto target=reference.read(*value);if(!target)continue;
        if(!reference.write){error="Referência não possui autoria portátil";return false;}
        u32 input=0;
        if(!sourceObject || target!=sourceObject) {
          auto at=std::find(targets.begin(),targets.end(),target);
          if(at==targets.end()) {
            if(targets.size()>=256){error="Limite de entradas de receita";return false;}
            const auto *schema=scene::findComponentSchema(value->type().id);u32 occurrence=0;
            for(usize n=0;n<=i;++n)if(components.at(n)->type().id==value->type().id)++occurrence;
            auto label=std::string(schema->name)+" "+std::to_string(occurrence)+" · "+reference.name;
            if(label.size()>96){usize end=96;while(end && (static_cast<unsigned char>(label[end])&0xc0)==0x80)--end;label.resize(end);}
            targets.push_back(target);inputs.push_back(std::move(label));input=static_cast<u32>(targets.size());
          } else input=static_cast<u32>(at-targets.begin())+1;
        }
        entry.references.push_back({std::string(reference.id),input});
      }
      parts.push_back(std::move(entry));
    }
    if(parts.empty()) {error="Nenhum componente deste objeto tem preset portátil";return false;}
    return store(std::move(name),std::move(parts),error,std::move(inputs));
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
    if(!entry||entry->recipe()) {error=entry?"Este preset é uma receita; resolva suas entradas":"Preset ausente";return {};}
    return decode(entry->entries.front(),error);
  }
  // Toda a receita, na ordem gravada. Vazio com `error` preenchido quando
  // qualquer componente não puder ser reconstruído: aplicar meia receita
  // deixaria o objeto num estado que ninguém pediu.
  std::vector<std::unique_ptr<scene::ComponentValue>> instantiateAll(u64 id,std::string &error,u64 self=0,
      std::span<const u64> inputs={},bool preview=false) const {
    std::vector<std::unique_ptr<scene::ComponentValue>> values;
    const auto *entry=find(id);
    if(!entry) {error="Preset ausente";return values;}
    if(!preview && !entry->inputs.empty() && (inputs.size()!=entry->inputs.size() || std::any_of(inputs.begin(),inputs.end(),[](u64 target){return !target;}))) {
      error="Escolha todas as entradas de objeto da receita";return values;
    }
    for(const auto &part:entry->entries) {
      auto value=decode(part,error);
      if(!value) {values.clear();return values;}
      for(const auto &binding:part.references) {
        if(binding.input>entry->inputs.size()){error="Entrada de referência ausente na receita";values.clear();return values;}
        const scene::ComponentObjectReference *property=nullptr;
        for(const auto &r:value->type().references)if(r.id==binding.property)property=&r;
        if(!property||!property->write){error="Endereço de referência da receita indisponível";values.clear();return values;}
        if(!preview) {
          const auto target=binding.input?inputs[binding.input-1]:self;
          if(!target){error="Esta receita exige o objeto de destino";values.clear();return values;}
          property->write(*value,target);
        }
      }
      if(!value->valid()){error="Referências fora do contrato da receita";values.clear();return values;}
      values.push_back(std::move(value));
    }
    return values;
  }
private:
  bool store(std::string name,std::vector<EditorComponentPresetEntry> parts,std::string &error,std::vector<std::string> inputs={}) {
    if(!loaded_||entries.size()>=256||!validName(name)) {error="Nome inválido ou limite de 256 presets";return false;}
    u64 id=1;for(const auto &entry:entries) {if(entry.id==std::numeric_limits<u64>::max()) {error="Limite de identidades";return false;}id=std::max(id,entry.id+1);}
    auto candidate=entries;candidate.push_back({id,std::move(name),std::move(parts),std::move(inputs)});
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
      out<<' '<<entry.inputs.size();for(const auto &name:entry.inputs)out<<' '<<std::quoted(name);
      for(const auto &part:entry.entries) {
        out<<' '<<std::quoted(part.type)<<' '<<part.version<<' '<<std::quoted(part.payload)<<' '<<part.references.size();
        for(const auto &r:part.references)out<<' '<<std::quoted(r.property)<<' '<<r.input;
      }
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
