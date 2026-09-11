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
// Buffers own edits independently of scene history. IO occurs only in explicit
// open/save/create actions on the session thread, never while drawing the UI.
class EditorCodeWorkspace final {
public:
  static constexpr usize MaximumFileBytes=512*1024,MaximumBuffers=16;
  u64 generation() const {return generation_;}
  bool applyBuildReport(std::string_view report,u64 generation);
  const std::vector<EditorScriptType> &scriptTypes() const {return scriptTypes_;}
  void invalidateBuild() {scriptTypes_.clear();}
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
  std::vector<EditorScriptType> scriptTypes_;
  u64 selected_=0,nextId_=1,generation_=1;
  std::string error_;
};
}
