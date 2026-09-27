#pragma once
#include "core/base.h"
#include <string>
#include <string_view>
#include <vector>
#include <atomic>
#include <chrono>

namespace ae::editor {
class EditorFileSystem;
struct EditorCodeBuffer {
  u64 id=0,revision=1;
  std::string path,text,saved;
  std::vector<std::string> undo,redo;
  u32 firstLine=0;
  // View state belongs to each buffer; changing tabs must not move another caret.
  u32 selectionStart=0,selectionEnd=0;
  float scrollX=0,scrollY=0;
  u64 viewRevision=1;
  bool dirty() const {return text!=saved;}
};
struct EditorCodeMatch {usize offset=0,length=0;u32 line=1,column=1;};
// `hidden`: [HideInInspector] — guardado e entregue no Play, fora do Inspector.
struct EditorScriptProperty {std::string id,name,valueType;bool hidden=false;};
struct EditorScriptType {std::string id,name,file;std::vector<EditorScriptProperty> properties;};
struct EditorCodeDiagnostic {
  std::string file,code,message;
  u32 line=1,column=1;
  bool error=true;
  std::string sourceExcerpt;
  u32 excerptLine=0;
  u64 generation=0;
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
    if(lastBuildFailed_) return EditorCodeCatalogState::Failed;
    if(scriptTypes_.empty() && !publishedGeneration_) return EditorCodeCatalogState::Empty;
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
  static constexpr u32 HelperTemplate=~1u;
  bool createScript(EditorFileSystem &files,std::string_view className,u32 templateIndex=~0u,
                    std::string_view directory="Scripts");
  bool replace(u64 id,u64 revision,std::string_view text);
  // Digitação contínua: UMA entrada de desfazer por sessão de digitação, não
  // por tecla.
  //
  // `replace` guarda um instantâneo do texto inteiro a cada chamada. Com uma
  // chamada por tecla — que é o que escrever direto no editor significa —
  // desfazer voltaria caractere a caractere e o histórico cresceria com uma
  // cópia do arquivo por tecla digitada.
  //
  // Também não exige a revisão: quem digita É a fonte da revisão. O confronto
  // de revisões existe para uma edição que partiu de um instantâneo antigo, que
  // não é o caso de um fluxo contínuo de teclas.
  bool type(u64 id,std::string_view text);
  // UTF-8 byte ranges, revision checked before any mutation. Android projects
  // this document through an Editable; it does not own a second undo history.
  bool editDelta(u64 id,u64 revision,usize start,usize erased,std::string_view inserted,bool transaction);
  bool setSelection(u64 id,u64 revision,u32 start,u32 end,float x,float y);
  bool locate(u32 line,u32 column=1,u32 length=0);
  // Fecha a sessão: a próxima tecla começa uma entrada de desfazer nova.
  // Chamado ao sair do campo, e por qualquer comando que não seja digitar.
  void endTypingRun() noexcept { typingRun_=0; }
  bool typing() const noexcept { return typingRun_!=0; }
  bool undo();
  bool redo();
  bool save(EditorFileSystem &files);
  bool saveAll(EditorFileSystem &files);
  bool close(u64 id,bool discard=false);
  bool select(u64 id);
  bool dirty() const;
  bool checkpoint(EditorFileSystem &files);
  bool hasRecovery(EditorFileSystem &files) const;
  bool restoreRecovery(EditorFileSystem &files);
  bool discardRecovery(EditorFileSystem &files);
  void clear();
private:
  // Buffer cuja sessão de digitação está aberta. Zero é "nenhuma".
  u64 typingRun_=0;
  std::chrono::steady_clock::time_point lastDeltaTime_{};
  usize lastDeltaEnd_=0;
  bool lastDeltaDeleting_=false;
public:
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
  // Transient buffer handles remain unique across NativeActivity instances;
  // an old queued edit must never match the first buffer of a new session.
  inline static std::atomic<u64> nextId_{1};
  u64 selected_=0,generation_=1,publishedGeneration_=0,stagedGeneration_=0;
  bool lastBuildFailed_=false,stagedValid_=false;
  std::string error_;
  u64 checkpointGeneration_=0;
  bool checkpointDirty_=false;
};
}
