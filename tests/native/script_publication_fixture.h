#pragma once
// Simula o hospedeiro que compila e carrega os scripts do projeto: abre um
// projeto temporário, deixa a compilação automática pedir o build, entrega um
// relatório bem-sucedido e confirma a publicação, como o Android faz depois de
// carregar o assembly. A barreira "publique antes de Play" do editor continua
// valendo; o teste só passa pelo mesmo caminho do app.
#include "editor/editor_session.h"
#include <chrono>
#include <filesystem>
#include <string>

struct PublishedScriptProject {
  std::filesystem::path root;
  PublishedScriptProject() = default;
  PublishedScriptProject(const PublishedScriptProject &) = delete;
  PublishedScriptProject &operator=(const PublishedScriptProject &) = delete;
  ~PublishedScriptProject() {
    if(root.empty()) return;
    std::error_code error;std::filesystem::remove_all(root,error);
  }
};

// Projeto já aberto pelo teste: só a compilação e a publicação.
inline bool publishCompiledScript(ae::editor::EditorSession &session,const char *typeId,const char *file) {
  session.setCodeCompilerAvailable(true);
  session.pumpCodeAutoBuild(0.0);session.pumpCodeAutoBuild(10.0);
  if(session.takeCodeBuildRequest().empty()) return false;
  std::string report="ASTRA_CODE 1 1 0 1 \"";
  report+=typeId;report+="\" \"Comportamento\" \"";report+=file;report+="\" 0";
  if(!session.completeCodeBuild(report)) return false;
  session.reportCodeCommit(true);
  return true;
}
// Sessão sem projeto: abre um projeto temporário, apagado com `project`.
inline bool publishCompiledScript(ae::editor::EditorSession &session,PublishedScriptProject &project,
                                  const char *typeId,const char *file) {
  namespace fs=std::filesystem;
  project.root=fs::temp_directory_path()/("aether-scripts-publicados-"+
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code error;fs::create_directories(project.root,error);
  if(error || !session.setProjectDirectory(project.root.string().c_str())) return false;
  return publishCompiledScript(session,typeId,file);
}
