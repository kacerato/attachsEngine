#include "editor/editor_session.h"

namespace ae::editor {

void EditorSession::configureScriptRendering(const renderer::ProjectRenderingSettings &authored,
                              const renderer::RenderingCapabilities &capabilities,
                              renderer::ThermalPressure thermal,
                              const renderer::ResolvedRenderingPolicy &effective) {
  playScene_.configureScriptRendering(authored,capabilities,thermal,effective,
    [this](u64 id,const renderer::ProjectRenderingSettings &settings,bool restoringAuthoring) {
      // Pedidos normais são serializados pelo estado runtime. A restauração
      // autoral de Stop é a única chamada que pode chegar com uma entrada na
      // fila; ela substitui o pedido não consumido para a configuração de
      // gameplay não escapar para o modo de edição.
      if(scriptRenderingRequestPending_&&!restoringAuthoring) return false;
      scriptRenderingRequestPending_=true;scriptRenderingRequestId_=id;scriptRenderingRequest_=settings;return true;
    },&assets_,&environmentProfiles_);
}

void EditorSession::setScriptLogSink(runtime::ScriptBridge::LogSink sink) {
  playScene_.setScriptLogSink([this,forward=std::move(sink)](u64 object,std::string_view text) {
    EditorConsoleEntry entry;
    entry.origin=EditorConsoleOrigin::Script;
    entry.message=std::string(text);
    if(isPlaying()) state_.playHudMessage=entry.message;
    entry.object=object;
    entry.project=files_.rootPath();entry.playSession=playScene_.world().worldId();
    entry.buildGeneration=runtimeCodeGeneration_;
    console_.add(std::move(entry));
    if(forward) forward(object,text);
  });
}

void EditorSession::pumpCodeAutoBuild(double seconds) {
  if(state_.codeRecoveryPending) return;
  // A bounded draft journal also works when the compiler is unavailable or
  // a build is running. It never publishes recovered code implicitly.
  if(seconds-codeCheckpointAt_>=1.0 && !state_.codeComposing && !files_.rootPath().empty()) {
    codeCheckpointAt_=seconds;
    if(!code_.checkpoint(files_)) reportProblem(EditorConsoleSeverity::Warning,code_.error());
  }
  if(state_.codeComposing) {codeQuietSince_=seconds;return;}
  if(codeAutoBuildDelay_<=0.0f || !state_.codeCompilerAvailable || state_.codeBuildBusy) return;
  if(files_.rootPath().empty()) return;
  const auto generation=code_.generation();
  if(generation!=codeSeenGeneration_) {
    codeSeenGeneration_=generation;codeQuietSince_=seconds;return;
  }
  if(generation==codeBuiltGeneration_ || codeQuietSince_<0.0) return;
  if(seconds-codeQuietSince_<static_cast<double>(codeAutoBuildDelay_)) return;
  if(!codeBuildRequest_.empty()) return;
  // Salvar ANTES de pedir: o compilador le o disco, e um build de um texto
  // que so existe na memoria compilaria a versao anterior e culparia o
  // usuario por um erro que ele acabou de corrigir.
  if(!code_.saveAll(files_)) { state_.status=code_.error(); codeQuietSince_=-1.0; return; }
  codeBuiltGeneration_=generation;
  codeBuildGeneration_=generation;
  codeBuildRequest_=files_.rootPath();
  state_.codeBuildBusy=true;
  state_.status="Compilando código do projeto";
  reportProblem(EditorConsoleSeverity::Info,"Compilação automática iniciada · revisão "+std::to_string(generation));
}

bool EditorSession::completeCodeBuild(std::string_view report) {
  if(codeBuildGeneration_!=code_.generation()) {
    state_.codeBuildBusy=false;
    state_.status="Fonte mais recente; aguardando nova compilação";
    reportProblem(EditorConsoleSeverity::Info,"Resultado de build antigo descartado · revisão "+std::to_string(codeBuildGeneration_));
    return false;
  }
  state_.codeBuildBusy=false;const bool ok=code_.applyBuildReport(report,codeBuildGeneration_);
  // O bloco do compilador troca inteiro: um diagnostico de um arquivo que
  // agora compila e mentira, e mentira que o usuario persegue.
  console_.replaceCompiler(code_.diagnostics(),files_.rootPath());
  // A linha do editor so entra quando NAO ha diagnostico. Com o build
  // automatico, "compilacao com erros" a cada pausa de digitacao seria uma
  // linha nova por pausa dizendo o que as linhas do compilador logo acima ja
  // dizem melhor -- e com o lugar.
  if(!ok && code_.diagnostics().empty()) reportProblem(EditorConsoleSeverity::Error,code_.error());
  if(!ok && !code_.diagnostics().empty()) {
    state_.consoleCollapsed=false;state_.consoleProblems=true;state_.consoleOrigin=-1;
  }
  reportProblem(EditorConsoleSeverity::Info,ok?"Compilação concluída; aguardando publicação":
      "Compilação recusada; publicação anterior preservada");
  state_.status=ok?"Código compilado; publicando":code_.error();return ok;
}

void EditorSession::reportCodeCommit(bool accepted) {
  // Fronteira atômica: os tipos compilados só passam a existir para o inspetor
  // quando o hospedeiro confirma que carregou o assembly. Recusar não apaga o
  // catálogo anterior — o assembly anterior continua sendo o que executa.
  if(accepted) code_.publishBuild();
  else code_.discardBuild();
  state_.status=accepted?"Código aplicado ao projeto":"Não foi possível publicar a compilação";
  reportProblem(accepted?EditorConsoleSeverity::Info:EditorConsoleSeverity::Error,
      accepted?"Código publicado · revisão "+std::to_string(code_.publishedGeneration()):"Publicação recusada; versão anterior preservada");
  if(accepted) {
    reportScriptSchemaChanges();state_.codeToolCatalog.clear();editorToolCatalogRoot_.clear();
    if(state_.codeTools) requestEditorTools();
  }
}

} // namespace ae::editor
