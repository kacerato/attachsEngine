#include "editor/editor_import_transaction.h"
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
void EditorCodeWorkspace::clear() {
  // Fechar o projeto é a ÚNICA operação que apaga o catálogo publicado: o
  // assembly daquele projeto deixou de existir para esta sessão.
  ++generation_;scriptTypes_.clear();stagedTypes_.clear();buffers_.clear();diagnostics_.clear();
  selected_=0;publishedGeneration_=0;stagedGeneration_=0;lastBuildFailed_=false;stagedValid_=false;
  error_.clear();checkpointGeneration_=0;checkpointDirty_=false;
}
bool EditorCodeWorkspace::select(u64 id) {
  endTypingRun();
  for(const auto &buffer:buffers_) if(buffer.id==id) {selected_=id;return true;}
  return false;
}
bool EditorCodeWorkspace::open(EditorFileSystem &files,const std::string &relative) {
  endTypingRun();
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
  endTypingRun();auto *buffer=active();
  if(!buffer || buffer->id!=id || buffer->revision!=revision) {error_="O arquivo mudou durante a edição";return false;}
  if(text.size()>MaximumFileBytes || text.find('\0')!=std::string_view::npos) {error_="Código excede o limite ou contém bytes nulos";return false;}
  if(buffer->text==text) return true;
  remember(buffer->undo,buffer->text);buffer->redo.clear();buffer->text=text;
  // O texto mudou: o rascunho fica mais novo que o publicado. Os diagnósticos
  // são do rascunho e saem; o catálogo publicado continua descrevendo o
  // assembly que está em uso.
  ++buffer->revision;++generation_;diagnostics_.clear();error_.clear();return true;
}
bool EditorCodeWorkspace::type(u64 id,std::string_view text) {
  auto *buffer=active();
  if(!buffer || buffer->id!=id) {error_="O arquivo mudou durante a edição";return false;}
  if(text.size()>MaximumFileBytes || text.find('\0')!=std::string_view::npos) {
    error_="Código excede o limite ou contém bytes nulos";return false;
  }
  if(buffer->text==text) return true;
  // O instantâneo entra UMA vez, na primeira tecla da sessão. As seguintes
  // escrevem por cima do texto sem empilhar nada: desfazer volta ao que estava
  // antes de a sessão começar, e não uma tecla atrás.
  if(typingRun_!=id) { remember(buffer->undo,buffer->text); buffer->redo.clear(); typingRun_=id; }
  buffer->text=text;
  ++buffer->revision;++generation_;diagnostics_.clear();error_.clear();return true;
}

bool EditorCodeWorkspace::undo() {
  endTypingRun();auto *buffer=active();if(!buffer || buffer->undo.empty()) return false;
  remember(buffer->redo,buffer->text);buffer->text=std::move(buffer->undo.back());buffer->undo.pop_back();
  buffer->selectionStart=buffer->selectionEnd=static_cast<u32>(std::min<usize>(buffer->selectionEnd,buffer->text.size()));
  ++buffer->viewRevision;++buffer->revision;++generation_;diagnostics_.clear();return true;
}
bool EditorCodeWorkspace::redo() {
  endTypingRun();auto *buffer=active();if(!buffer || buffer->redo.empty()) return false;
  remember(buffer->undo,buffer->text);buffer->text=std::move(buffer->redo.back());buffer->redo.pop_back();
  buffer->selectionStart=buffer->selectionEnd=static_cast<u32>(std::min<usize>(buffer->selectionEnd,buffer->text.size()));
  ++buffer->viewRevision;++buffer->revision;++generation_;diagnostics_.clear();return true;
}

namespace {
bool codeBoundary(std::string_view text,usize at) {
  return at<=text.size() && (at==text.size() || (static_cast<unsigned char>(text[at])&0xc0)!=0x80);
}
}
bool EditorCodeWorkspace::editDelta(u64 id,u64 revision,usize start,usize erased,
                                    std::string_view inserted,bool transaction) {
  auto *buffer=active();
  if(!buffer || buffer->id!=id || buffer->revision!=revision) {
    error_="Edição recebida de uma revisão antiga; rascunho preservado para recuperação";return false;
  }
  if(start>buffer->text.size() || erased>buffer->text.size()-start ||
     !codeBoundary(buffer->text,start) || !codeBoundary(buffer->text,start+erased) ||
     inserted.find('\0')!=std::string_view::npos ||
     inserted.size()>MaximumFileBytes-(buffer->text.size()-erased)) {
    error_="Intervalo de texto inválido ou arquivo maior que 512 KiB";return false;
  }
  if(std::string_view(buffer->text).substr(start,erased)==inserted) return true;
  const auto now=std::chrono::steady_clock::now();
  const bool deleting=inserted.empty() && erased>0;
  if(transaction || now-lastDeltaTime_>std::chrono::milliseconds(1000) ||
      deleting!=lastDeltaDeleting_ || (start!=lastDeltaEnd_ && !(deleting && start+erased==lastDeltaEnd_)) ||
      inserted.find('\n')!=std::string_view::npos) endTypingRun();
  if(typingRun_!=id) {remember(buffer->undo,buffer->text);buffer->redo.clear();typingRun_=id;}
  buffer->text.replace(start,erased,inserted);
  lastDeltaTime_=now;lastDeltaEnd_=start+inserted.size();lastDeltaDeleting_=deleting;
  ++buffer->revision;++generation_;diagnostics_.clear();error_.clear();
  if(transaction) endTypingRun();
  return true;
}
bool EditorCodeWorkspace::setSelection(u64 id,u64 revision,u32 start,u32 end,float x,float y) {
  auto *buffer=active();
  if(!buffer || buffer->id!=id || buffer->revision!=revision ||
     !codeBoundary(buffer->text,start) || !codeBoundary(buffer->text,end)) return false;
  buffer->selectionStart=start;buffer->selectionEnd=end;
  buffer->scrollX=x;buffer->scrollY=y;return true;
}
bool EditorCodeWorkspace::locate(u32 line,u32 column,u32 length) {
  auto *buffer=active();if(!buffer) return false;
  usize at=0;
  for(u32 i=1;i<line;++i) {
    const auto next=buffer->text.find('\n',at);
    if(next==std::string::npos) {at=buffer->text.size();break;}
    at=next+1;
  }
  // Compiler columns are UTF-16 units (Roslyn), not UTF-8 bytes.
  for(u32 i=1;i<column && at<buffer->text.size() && buffer->text[at]!='\n';) {
    const auto c=static_cast<unsigned char>(buffer->text[at]);
    const usize width=c<0x80?1:c<0xe0?2:c<0xf0?3:4;
    if(width==4 && i+1>=column) break;
    at=std::min(at+width,buffer->text.size());i+=width==4?2:1;
  }
  buffer->selectionStart=static_cast<u32>(at);
  auto end=std::min(at+length,buffer->text.size());
  while(!codeBoundary(buffer->text,end)) --end;
  buffer->selectionEnd=static_cast<u32>(end);
  buffer->firstLine=line>3?line-3:0;buffer->scrollX=0;buffer->scrollY=0;
  ++buffer->viewRevision;endTypingRun();return true;
}
bool EditorCodeWorkspace::saveBuffer(EditorFileSystem &files,EditorCodeBuffer &buffer) {
  if(!buffer.dirty()) return true;
  const auto path=files.resolveFile(buffer.path);std::string current;
  if(path.empty() || !loadText(path,current)) {error_="Arquivo removido ou indisponível; conteúdo editado foi mantido";return false;}
  if(current!=buffer.saved) {error_="Arquivo alterado externamente; gravação interrompida para preservar ambas as versões";return false;}
  WriteText input{buffer.text};
  if(!platform::replaceAssetFile(path.c_str(),buffer.text.size(),readText,&input)) {error_="Falha ao salvar código; buffer preservado";return false;}
  buffer.saved=buffer.text;checkpointGeneration_=0;error_.clear();return true;
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
    buffers_.erase(it);checkpointGeneration_=0;
    if(selected_==id) selected_=buffers_.empty()?0:buffers_.back().id;
    return true;
  }
  return false;
}
namespace {
std::filesystem::path recoveryPath(const EditorFileSystem &files) {
  std::filesystem::path path;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files.rootPath()),".astra/code-drafts.astra",path)) return {};
  return path;
}
}
bool EditorCodeWorkspace::hasRecovery(EditorFileSystem &files) const {
  const auto path=recoveryPath(files);if(path.empty()) return false;
  std::error_code error;return std::filesystem::is_regular_file(path,error) && !error;
}
bool EditorCodeWorkspace::discardRecovery(EditorFileSystem &files) {
  const auto path=recoveryPath(files);if(path.empty()) return false;
  std::error_code error;std::filesystem::remove(path,error);
  if(error) {error_="Não foi possível descartar o checkpoint; rascunhos preservados.";return false;}
  checkpointGeneration_=generation_;checkpointDirty_=false;return true;
}
bool EditorCodeWorkspace::checkpoint(EditorFileSystem &files) {
  const bool changed=dirty();
  if(checkpointGeneration_==generation_ && checkpointDirty_==changed) return true;
  if(!changed) return discardRecovery(files);
  const auto path=recoveryPath(files);if(path.empty()) {error_="Caminho de recuperação inválido.";return false;}
  std::error_code error;std::filesystem::create_directories(path.parent_path(),error);
  if(error) {error_="Não foi possível preparar a recuperação de código.";return false;}
  usize count=0;for(const auto &buffer:buffers_) if(buffer.dirty()) ++count;
  std::ostringstream out;out<<"ASTRA_DRAFTS_1 "<<count<<'\n';
  for(const auto &buffer:buffers_) if(buffer.dirty())
    out<<std::quoted(buffer.path)<<' '<<std::quoted(buffer.saved)<<' '<<std::quoted(buffer.text)<<' '
       <<buffer.selectionStart<<' '<<buffer.selectionEnd<<' '<<(buffer.id==selected_)<<'\n';
  if(!EditorImportTransaction::writeText(path,out.str())) {error_="Falha ao guardar rascunhos; texto continua aberto.";return false;}
  checkpointGeneration_=generation_;checkpointDirty_=true;return true;
}
bool EditorCodeWorkspace::restoreRecovery(EditorFileSystem &files) {
  std::vector<u8> bytes;const auto path=recoveryPath(files);
  if(path.empty() || !EditorImportTransaction::read(path,bytes,MaximumBuffers*(MaximumFileBytes*4+4096))) {
    error_="Checkpoint indisponível ou acima do limite; arquivo preservado.";return false;
  }
  std::istringstream in(std::string(bytes.begin(),bytes.end()));std::string magic;usize count=0;
  if(!(in>>magic>>count) || magic!="ASTRA_DRAFTS_1" || count>MaximumBuffers || !buffers_.empty()) {
    error_="Checkpoint incompatível ou workspace já aberto; rascunhos preservados.";return false;
  }
  std::vector<EditorCodeBuffer> recovered;u64 selected=0;
  for(usize i=0;i<count;++i) {
    EditorCodeBuffer buffer;bool active=false;std::filesystem::path resolved;
    if(!(in>>std::quoted(buffer.path)>>std::quoted(buffer.saved)>>std::quoted(buffer.text)>>buffer.selectionStart>>buffer.selectionEnd>>active) ||
       buffer.path.starts_with(".astra/") || !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files.rootPath()),buffer.path,resolved) ||
       buffer.text.size()>MaximumFileBytes || buffer.saved.size()>MaximumFileBytes ||
       buffer.text.find('\0')!=std::string::npos || buffer.saved.find('\0')!=std::string::npos ||
       std::any_of(recovered.begin(),recovered.end(),[&](const auto &other){return other.path==buffer.path;})) {
      error_="Checkpoint inválido; nada foi substituído.";return false;
    }
    buffer.id=nextId_++;buffer.selectionStart=std::min<u32>(buffer.selectionStart,buffer.text.size());
    buffer.selectionEnd=std::min<u32>(buffer.selectionEnd,buffer.text.size());
    while(!codeBoundary(buffer.text,buffer.selectionStart)) --buffer.selectionStart;
    while(!codeBoundary(buffer.text,buffer.selectionEnd)) --buffer.selectionEnd;
    buffer.undo.push_back(buffer.saved);if(active) selected=buffer.id;
    recovered.push_back(std::move(buffer));
  }
  in>>std::ws;if(!in.eof()) {error_="Checkpoint contém dados excedentes.";return false;}
  buffers_=std::move(recovered);selected_=selected?selected:(buffers_.empty()?0:buffers_.front().id);
  ++generation_;checkpointGeneration_=0;error_.clear();return true;
}

bool EditorCodeWorkspace::createScript(EditorFileSystem &files,std::string_view className,u32 templateIndex,std::string_view directory) {
  if(className.empty() || className.size()>64) {error_="Nome de classe inválido";return false;}
  for(usize i=0;i<className.size();++i) {
    const unsigned char c=className[i];
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||(i&&c>='0'&&c<='9'))) {error_="Use letras, números e sublinhado no nome da classe";return false;}
  }
  if(buffers_.size()>=MaximumBuffers) {error_="Feche um arquivo antes de criar outro";return false;}
  if(!directory.empty() && !files.createDirectory(std::string(directory))) {error_=files.error();return false;}
  const std::string prefix=directory.empty()?"":std::string(directory)+"/";
  const std::string relative=prefix+std::string(className)+".cs";
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
       (!files.createDirectory("Scripts") || !files.createTextFile(contracts,kEditorScriptContracts))) {error_=files.error();return false;}
  }
  if(templateIndex==HelperTemplate) text="public static class "+std::string(className)+"\n{\n}\n";
  if(!files.createTextFile(relative,text)) {error_=files.error();return false;}
  // Criar um arquivo torna o rascunho mais novo; não apaga o que já foi
  // publicado nem os componentes que dependem desses tipos.
  ++generation_;return open(files,relative);
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
  if(!(in>>magic>>version>>success>>count) || magic!="ASTRA_CODE" || (version!=1 && version!=2) || success>1 || count>4096) {
    error_="Resposta inválida do compilador";return false;
  }
  std::vector<EditorCodeDiagnostic> diagnostics;
  for(u32 i=0;i<count;++i) {
    EditorCodeDiagnostic d;u32 error=0;
    if(!(in>>std::quoted(d.file)>>d.line>>d.column>>error>>std::quoted(d.code)>>std::quoted(d.message)) || error>1) return false;
    if(version==2 && !(in>>d.excerptLine>>std::quoted(d.sourceExcerpt))) return false;
    d.generation=generation;
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
  if(success) {
    // Encenado, não publicado: o hospedeiro ainda pode recusar a compilação, e
    // anunciar tipos de um assembly que não entrou em uso faria o inspetor
    // prometer o que o Play não executaria.
    stagedTypes_=std::move(types);stagedGeneration_=generation;stagedValid_=true;
    lastBuildFailed_=false;error_.clear();return true;
  }
  stagedTypes_.clear();stagedValid_=false;lastBuildFailed_=true;
  // A mensagem sempre disse isto; agora é verdade — o catálogo anterior fica.
  error_="Compilação com erros; versão aplicada anterior preservada";return false;
}

void EditorCodeWorkspace::publishBuild() {
  if(!stagedValid_) return;
  scriptTypes_=std::move(stagedTypes_);
  stagedTypes_.clear();stagedValid_=false;
  publishedGeneration_=stagedGeneration_;
  lastBuildFailed_=false;
}

void EditorCodeWorkspace::discardBuild() {
  stagedTypes_.clear();stagedValid_=false;
  // Não marca falha: a compilação funcionou, quem recusou foi a publicação. O
  // catálogo anterior continua correto porque o assembly anterior continua em
  // uso.
}

}
