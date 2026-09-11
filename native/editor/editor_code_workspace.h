#pragma once
#include "core/base.h"
#include <string>
#include <string_view>
#include <vector>

namespace ae::editor {
class EditorFileSystem;
struct EditorCodeBuffer {
  u64 id=0,revision=1;
  std::string path,text,saved;
  std::vector<std::string> undo,redo;
  u32 firstLine=0;
  bool dirty() const {return text!=saved;}
};
struct EditorCodeMatch {usize offset=0,length=0;u32 line=1,column=1;};
struct EditorScriptProperty {std::string id,name,valueType;};
struct EditorScriptType {std::string id,name,file;std::vector<EditorScriptProperty> properties;};
struct EditorCodeDiagnostic {
  std::string file,code,message;
  u32 line=1,column=1;
  bool error=true;
};
// O estado do catálogo publicado em relação ao rascunho em edição. São coisas
// diferentes de propósito: o usuário precisa saber que o texto mudou sem que a
// lista de componentes conhecidos desapareça embaixo dele.
enum class EditorCodeCatalogState : u32 {
  Empty,   // nada foi publicado ainda neste projeto
  Current, // o catálogo publicado corresponde ao texto atual
  Stale,   // há rascunho mais novo; o publicado continua valendo
  Failed   // a última compilação falhou; o publicado anterior continua valendo
};

// Buffers own edits independently of scene history. IO occurs only in explicit
// open/save/create actions on the session thread, never while drawing the UI.
class EditorCodeWorkspace final {
public:
  static constexpr usize MaximumFileBytes=512*1024,MaximumBuffers=16;
  u64 generation() const {return generation_;}
  // Recebe o relatório do compilador e **encena** os tipos: nada é publicado
  // aqui. Quem publica é `publishBuild`, depois de o hospedeiro aceitar a
  // compilação — é a fronteira atômica que impede o catálogo de anunciar tipos
  // de um assembly que não entrou em uso.
  bool applyBuildReport(std::string_view report,u64 generation);
  // Promove o que foi encenado. Só depois disto o inspetor vê os tipos novos.
  void publishBuild();
  // Descarta o que foi encenado. O catálogo publicado ANTERIOR continua valendo:
  // ele descreve o assembly que ainda está em uso.
  void discardBuild();
  // O catálogo PUBLICADO. Ele não é limpo por digitar, desfazer, refazer, criar
  // arquivo ou por uma compilação que falhou.
  //
  // Isto já foi o contrário, e o defeito era observável: editar o texto apagava
  // a lista, o componente sumia de "Adicionar componente" e do inspetor, e
  // mesmo assim continuava anexado e executando no Play. Instância autorada,
  // schema conhecido e versão em execução são três coisas distintas.
  const std::vector<EditorScriptType> &scriptTypes() const {return scriptTypes_;}
  EditorCodeCatalogState catalogState() const {
    if(scriptTypes_.empty() && !publishedGeneration_) return EditorCodeCatalogState::Empty;
    if(lastBuildFailed_) return EditorCodeCatalogState::Failed;
    return publishedGeneration_==generation_?EditorCodeCatalogState::Current
                                            :EditorCodeCatalogState::Stale;
  }
  // A geração do texto que produziu o catálogo publicado. Zero quando nunca
  // houve publicação.
  u64 publishedGeneration() const {return publishedGeneration_;}
  bool open(EditorFileSystem &files,const std::string &relative);
  // `templateIndex` escolhe um modelo de editor_script_templates.h; fora da
  // faixa cria o arquivo vazio de sempre. O modelo que interage por capacidade
  // traz o arquivo de contratos junto, quando o projeto ainda não o tem.
  bool createScript(EditorFileSystem &files,std::string_view className,u32 templateIndex=~0u);
  bool replace(u64 id,u64 revision,std::string_view text);
  bool undo();
  bool redo();
  bool save(EditorFileSystem &files);
  bool saveAll(EditorFileSystem &files);
  bool close(u64 id,bool discard=false);
  bool select(u64 id);
  bool dirty() const;
  void clear();
  EditorCodeBuffer *active();
  const EditorCodeBuffer *active() const;
  const std::vector<EditorCodeBuffer> &buffers() const {return buffers_;}
  std::vector<EditorCodeMatch> find(std::string_view query) const;
  const std::string &error() const {return error_;}
  void setDiagnostics(std::vector<EditorCodeDiagnostic> value) {diagnostics_=std::move(value);}
  const std::vector<EditorCodeDiagnostic> &diagnostics() const {return diagnostics_;}
private:
  bool saveBuffer(EditorFileSystem &files,EditorCodeBuffer &buffer);
  static void remember(std::vector<std::string> &history,const std::string &text);
  std::vector<EditorCodeBuffer> buffers_;
  std::vector<EditorCodeDiagnostic> diagnostics_;
  std::vector<EditorScriptType> scriptTypes_,stagedTypes_;
  u64 selected_=0,nextId_=1,generation_=1,publishedGeneration_=0,stagedGeneration_=0;
  bool lastBuildFailed_=false,stagedValid_=false;
  std::string error_;
};
}
