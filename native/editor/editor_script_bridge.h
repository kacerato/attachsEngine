#pragma once
#include "scene/script_runtime.h"
#include "editor/editor_document.h"
#include "editor/editor_scene_physics.h"
#include <functional>

namespace ae::editor {
// Platform injects the managed service. This adapter is the only place where
// the general ABI knows the transitional editor execution-document format.
class EditorScriptBridge final {
public:
  using LogSink = std::function<void(u64, std::string_view)>;
  void setLogSink(LogSink sink) {logSink_=std::move(sink);}
  ~EditorScriptBridge(){stop();}
  void configure(scene::ScriptRuntimeApi api,std::string root) {if(!running_) {api_=api;root_=std::move(root);}}
  bool start(EditorDocument &document,EditorScenePhysics &physics);
  bool update(float elapsed);
  bool fixedUpdate(float elapsed);
  bool trigger(EditorEntityId sensor,EditorEntityId other,u32 phase);
  void stop();
  const std::string &diagnostics() const {return diagnostics_;}
  static bool hasScripts(const EditorDocument &document);
private:
  void collectDiagnostics();
  scene::ScriptRuntimeApi api_{};
  scene::ScriptSceneAccess access_{};
  std::string root_,diagnostics_;
  LogSink logSink_;
  EditorDocument *document_=nullptr;
  EditorScenePhysics *physics_=nullptr;
  bool running_=false;
};
}
