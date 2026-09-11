#include "editor/editor_code_workspace.h"
#include "editor/editor_script_templates.h"
#include "editor/editor_filesystem.h"
#include "platform/atomic_asset_file.h"
#include <algorithm>
#include <fstream>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace ae::editor {
namespace {
bool loadText(const std::string &path,std::string &text) {
  std::ifstream in(std::filesystem::path(std::u8string(path.begin(),path.end())),std::ios::binary|std::ios::ate);
  if(!in) return false;
  const auto length=in.tellg();
  if(length<0 || static_cast<u64>(length)>EditorCodeWorkspace::MaximumFileBytes) return false;
  text.resize(static_cast<usize>(length));in.seekg(0);
  if(length && !in.read(text.data(),length)) return false;
  return text.find('\0')==std::string::npos;
}
struct WriteText {std::string_view text;usize offset=0;};
int readText(void *context,void *data,size_t capacity) {
  auto &input=*static_cast<WriteText*>(context);
  const usize count=std::min(capacity,input.text.size()-input.offset);
  std::memcpy(data,input.text.data()+input.offset,count);input.offset+=count;
  return static_cast<int>(count);
}
}
EditorCodeBuffer *EditorCodeWorkspace::active() {
  for(auto &buffer:buffers_) if(buffer.id==selected_) return &buffer;
  return nullptr;
}
const EditorCodeBuffer *EditorCodeWorkspace::active() const {
  for(const auto &buffer:buffers_) if(buffer.id==selected_) return &buffer;
  return nullptr;
}
bool EditorCodeWorkspace::dirty() const {
  return std::any_of(buffers_.begin(),buffers_.end(),[](const auto &b){return b.dirty();});
}
void EditorCodeWorkspace::clear() {++generation_;scriptTypes_.clear();buffers_.clear();diagnostics_.clear();selected_=0;error_.clear();}
bool EditorCodeWorkspace::select(u64 id) {
  for(const auto &buffer:buffers_) if(buffer.id==id) {selected_=id;return true;}
  return false;
}
bool EditorCodeWorkspace::open(EditorFileSystem &files,const std::string &relative) {
  for(const auto &buffer:buffers_) if(buffer.path==relative) {selected_=buffer.id;return true;}
  if(buffers_.size()>=MaximumBuffers) {error_="Feche um arquivo antes de abrir outro";return false;}
  const auto path=files.resolveFile(relative);
  EditorCodeBuffer buffer;buffer.path=relative;
  if(path.empty() || !loadText(path,buffer.text)) {error_="Arquivo de texto indisponível, binário ou maior que 512 KiB";return false;}
  buffer.id=nextId_++;buffer.saved=buffer.text;selected_=buffer.id;
  buffers_.push_back(std::move(buffer));error_.clear();return true;
}
void EditorCodeWorkspace::remember(std::vector<std::string> &history,const std::string &text) {
  history.push_back(text);usize bytes=0;for(const auto &item:history) bytes+=item.size();
  while(history.size()>1 && (history.size()>64 || bytes>2*1024*1024)) {
    bytes-=history.front().size();history.erase(history.begin());
  }
}
bool EditorCodeWorkspace::replace(u64 id,u64 revision,std::string_view text) {
  auto *buffer=active();
  if(!buffer || buffer->id!=id || buffer->revision!=revision) {error_="O arquivo mudou durante a edição";return false;}
  if(text.size()>MaximumFileBytes || text.find('\0')!=std::string_view::npos) {error_="Código excede o limite ou contém bytes nulos";return false;}
  if(buffer->text==text) return true;
  remember(buffer->undo,buffer->text);buffer->redo.clear();buffer->text=text;
  ++buffer->revision;++generation_;scriptTypes_.clear();diagnostics_.clear();error_.clear();return true;
}
bool EditorCodeWorkspace::undo() {
  auto *buffer=active();if(!buffer || buffer->undo.empty()) return false;
  remember(buffer->redo,buffer->text);buffer->text=std::move(buffer->undo.back());buffer->undo.pop_back();
  ++buffer->revision;++generation_;scriptTypes_.clear();diagnostics_.clear();return true;
}
bool EditorCodeWorkspace::redo() {
  auto *buffer=active();if(!buffer || buffer->redo.empty()) return false;
  remember(buffer->undo,buffer->text);buffer->text=std::move(buffer->redo.back());buffer->redo.pop_back();
  ++buffer->revision;++generation_;scriptTypes_.clear();diagnostics_.clear();return true;
}
bool EditorCodeWorkspace::saveBuffer(EditorFileSystem &files,EditorCodeBuffer &buffer) {
  if(!buffer.dirty()) return true;
  const auto path=files.resolveFile(buffer.path);std::string current;
  if(path.empty() || !loadText(path,current)) {error_="Arquivo removido ou indisponível; conteúdo editado foi mantido";return false;}
  if(current!=buffer.saved) {error_="Arquivo alterado externamente; gravação interrompida para preservar ambas as versões";return false;}
  WriteText input{buffer.text};
  if(!platform::replaceAssetFile(path.c_str(),buffer.text.size(),readText,&input)) {error_="Falha ao salvar código; buffer preservado";return false;}
  buffer.saved=buffer.text;error_.clear();return true;
}
bool EditorCodeWorkspace::save(EditorFileSystem &files) {
  auto *buffer=active();return buffer && saveBuffer(files,*buffer);
}
bool EditorCodeWorkspace::saveAll(EditorFileSystem &files) {
  for(auto &buffer:buffers_) if(!saveBuffer(files,buffer)) return false;
  return true;
}
bool EditorCodeWorkspace::close(u64 id,bool discard) {
  for(auto it=buffers_.begin();it!=buffers_.end();++it) if(it->id==id) {
    if(it->dirty() && !discard) {error_="Salve o arquivo antes de fechar";return false;}
    buffers_.erase(it);
    if(selected_==id) selected_=buffers_.empty()?0:buffers_.back().id;
    return true;
  }
  return false;
}
bool EditorCodeWorkspace::createScript(EditorFileSystem &files,std::string_view className,u32 templateIndex) {
  if(className.empty() || className.size()>64) {error_="Nome de classe inválido";return false;}
  for(usize i=0;i<className.size();++i) {
    const unsigned char c=className[i];
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||(i&&c>='0'&&c<='9'))) {error_="Use letras, números e sublinhado no nome da classe";return false;}
  }
  if(buffers_.size()>=MaximumBuffers) {error_="Feche um arquivo antes de criar outro";return false;}
  if(!files.createDirectory("Scripts")) {error_=files.error();return false;}
  const std::string relative="Scripts/"+std::string(className)+".cs";
  std::string text="using Astra;\n\n[ComponentId(\"project."+std::string(className)+"\")]\npublic sealed class "+std::string(className)+" : Behavior\n{\n    public override void Start()\n    {\n    }\n\n    public override void Update(float deltaTime)\n    {\n    }\n}\n";
  if(templateIndex<editorScriptTemplates.size()) {
    const auto &model=editorScriptTemplates[templateIndex];
    // O modelo traz o próprio nome de classe; trocá-lo pelo que o usuário
    // digitou mantém o arquivo compilável e o ComponentId único.
    text=std::string(model.source);
    const std::string from(model.className),to(className);
    for(usize at=text.find(from);at!=std::string::npos;at=text.find(from,at+to.size()))
      text.replace(at,from.size(),to);
    // Os contratos acompanham quem interage por capacidade, e só na primeira
    // vez: recriá-los apagaria o que o usuário tivesse acrescentado neles.
    const std::string contracts="Scripts/"+std::string(kEditorScriptContractsFile);
    if(model.contracts && !files.exists(contracts) &&
       !files.createTextFile(contracts,kEditorScriptContracts)) {error_=files.error();return false;}
  }
  if(!files.createTextFile(relative,text)) {error_=files.error();return false;}
  ++generation_;scriptTypes_.clear();return open(files,relative);
}
std::vector<EditorCodeMatch> EditorCodeWorkspace::find(std::string_view query) const {
  std::vector<EditorCodeMatch> result;const auto *buffer=active();
  if(!buffer || query.empty()) return result;
  usize offset=0,scanned=0;u32 line=1,column=1;
  while((offset=buffer->text.find(query,offset))!=std::string::npos && result.size()<4096) {
    while(scanned<offset) {if(buffer->text[scanned++]=='\n') {++line;column=1;} else ++column;}
    result.push_back({offset,query.size(),line,column});offset+=query.size();
  }
  return result;
}
bool EditorCodeWorkspace::applyBuildReport(std::string_view report,u64 generation) {
  if(generation!=generation_) {error_="O código mudou durante a compilação; aplique a versão atual";return false;}
  if(report.size()>4*1024*1024) {error_="Diagnóstico de compilação excede o limite";return false;}
  std::istringstream in{std::string(report)};std::string magic;u32 version=0,success=0,count=0;
  if(!(in>>magic>>version>>success>>count) || magic!="ASTRA_CODE" || version!=1 || success>1 || count>4096) {
    error_="Resposta inválida do compilador";return false;
  }
  std::vector<EditorCodeDiagnostic> diagnostics;
  for(u32 i=0;i<count;++i) {
    EditorCodeDiagnostic d;u32 error=0;
    if(!(in>>std::quoted(d.file)>>d.line>>d.column>>error>>std::quoted(d.code)>>std::quoted(d.message)) || error>1) return false;
    d.error=error!=0;diagnostics.push_back(std::move(d));
  }
  if(!(in>>count) || count>4096) return false;
  std::vector<EditorScriptType> types;
  for(u32 i=0;i<count;++i) {
    EditorScriptType type;u32 properties=0;
    if(!(in>>std::quoted(type.id)>>std::quoted(type.name)>>std::quoted(type.file)>>properties) || properties>4096) return false;
    for(u32 field=0;field<properties;++field) {
      EditorScriptProperty property;
      if(!(in>>std::quoted(property.id)>>std::quoted(property.name)>>std::quoted(property.valueType))) return false;
      type.properties.push_back(std::move(property));
    }
    types.push_back(std::move(type));
  }
  in>>std::ws;if(!in.eof()) return false;
  diagnostics_=std::move(diagnostics);
  if(success) {scriptTypes_=std::move(types);error_.clear();return true;}
  scriptTypes_.clear();error_="Compilação com erros; versão aplicada anterior preservada";return false;
}

}
