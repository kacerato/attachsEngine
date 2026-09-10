#pragma once
#include "editor/editor_map_scene.h"
#include "editor/editor_scene_physics.h"
#include "editor/editor_script_bridge.h"

namespace ae::editor {
// Editor adapter owns the isolated execution copy. Resource geometry remains
// immutable in EditorMapScene; neither renderer poses nor runtime mutations
// write into the authoring document. One owner thread, no platform dependency.
class EditorPlayScene final {
public:
  ~EditorPlayScene(){stop();}
  void setScriptRuntime(scene::ScriptRuntimeApi api,const std::string &root) {scripts_.configure(api,root);}
  void setScriptLogSink(EditorScriptBridge::LogSink sink) {scripts_.setLogSink(std::move(sink));}
  const std::string &scriptDiagnostics() const {return scripts_.diagnostics();}
  const std::string &physicsError() const {return physics_.error();}
  bool active() const noexcept { return active_; }
  const EditorDocument &document() const noexcept { return document_; }
  EditorDocument *executionDocument() noexcept { return active_ ? &document_ : nullptr; }
  static EditorEntityId unresolvedEntity(const EditorDocument &source) {
    std::vector<EditorEntityId> ids;source.collectSubtree(source.root(),ids);
    for(auto id:ids) if(source.find(id)->components.hasUnresolved()) return id;
    return kInvalidEntity;
  }
  bool start(const EditorDocument &source, const EditorMapScene &resources) {
    if(active_ || unresolvedEntity(source)!=kInvalidEntity) return false;
    EditorDocument candidate=source;
    std::vector<renderer::MapDrawState> validated;
    if(!resources.extract(candidate,validated)) return false;
    if(!physics_.start(candidate)) return false;
    document_=std::move(candidate);
    if(!scripts_.start(document_,physics_)) {physics_.stop();document_=EditorDocument{};return false;}
    active_=true;
    paused_=false;
    return true;
  }
  bool extract(const EditorMapScene &resources, std::vector<renderer::MapDrawState> &draws) const {
    return active_ && resources.extract(document_,draws);
  }
  void stop() {
    scripts_.stop();
    physics_.stop();
    document_=EditorDocument{};
    active_=false;
    paused_=false;
  }
  void pause(bool value) {if(active_) paused_=value;}
  bool step() {return active_ && paused_ && scripts_.update(1.0f/60.0f) && physics_.advance(1.0/60.0,document_,fixedStep,this,triggerEvent);}
  bool setCharacterMove(EditorEntityId id,float right,float forward,float yaw) {
    return active_ && physics_.setCharacterMove(id,right,forward,yaw);
  }
  bool jumpCharacter(EditorEntityId id) {return active_&&!paused_&&physics_.jumpCharacter(id);}
  bool advance(double elapsed) {return active_ && (paused_ || (scripts_.update(static_cast<float>(std::min(elapsed,.25))) && physics_.advance(elapsed,document_,fixedStep,this,triggerEvent)));}
private:
  static bool triggerEvent(void *context,EditorEntityId sensor,EditorEntityId other,u32 phase) {return static_cast<EditorPlayScene *>(context)->scripts_.trigger(sensor,other,phase);}
  static bool fixedStep(void *context,float dt) {return static_cast<EditorPlayScene *>(context)->scripts_.fixedUpdate(dt);}
  EditorScenePhysics physics_;
  EditorDocument document_;
  EditorScriptBridge scripts_;
  bool active_=false;
  bool paused_=false;
};
}
