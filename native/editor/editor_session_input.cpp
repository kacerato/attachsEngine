#include "editor/editor_session.h"
#include "editor/editor_widgets.h"
#include <cmath>

namespace ae::editor {
using namespace ae::ui;

bool EditorSession::inputBindingCaptureActive() const noexcept {
  return inputCapture_.has_value() && state_.inputCapturing &&
      state_.workspace==EditorWorkspace::Project && state_.projectSection==EditorProjectSection::Input;
}

void EditorSession::cancelInputBindingCapture() {
  inputCapture_.reset();state_.inputCapturing=false;state_.inputCaptureNegative=false;state_.inputCapturePrompt.clear();state_.inputCaptureFeedback.clear();
}

bool EditorSession::validateInputCapture() {
  if(!inputBindingCaptureActive()) return false;
  const auto &capture=*inputCapture_;const auto version=sceneVersion();
  const auto &actions=document_.inputActions().actions();
  return version.epoch==capture.version.epoch && version.revision==capture.version.revision &&
      files_.rootPath()==capture.project && document_.inputActions()==capture.before &&
      state_.inputActionIndex<actions.size() && actions[state_.inputActionIndex].id==capture.action &&
      state_.inputBindingIndex==capture.binding && !history_.isOpen();
}

bool EditorSession::beginInputBindingCapture(bool negative) {
  cancelInputBindingCapture();
  if(state_.workspace!=EditorWorkspace::Project || state_.projectSection!=EditorProjectSection::Input ||
     history_.isOpen() || pendingTextEdit().purpose!=EditorTextPurpose::None) {
    state_.status="Finalize o campo atual e abra o mapa de entrada para capturar";return false;
  }
  const auto &actions=document_.inputActions().actions();
  if(state_.inputActionIndex>=actions.size()) return false;
  const auto &action=actions[state_.inputActionIndex];
  if(state_.inputBindingIndex>=action.bindings.size()) return false;
  const auto source=action.bindings[state_.inputBindingIndex].source;
  if((source!=runtime::InputSource::Key && source!=runtime::InputSource::GamepadButton && source!=runtime::InputSource::GamepadAxis && source!=runtime::InputSource::MouseButton) ||
     (negative && (source!=runtime::InputSource::Key || action.kind==runtime::ActionKind::Button))) {
    state_.status="A captura exige Tecla, Botão mouse, Botão gamepad ou Eixo gamepad";return false;
  }
  InputCapture capture;capture.version=sceneVersion();capture.before=document_.inputActions();
  capture.project=files_.rootPath();capture.action=action.id;capture.binding=state_.inputBindingIndex;capture.negative=negative;
  inputCapture_=std::move(capture);state_.inputCapturing=true;state_.inputCaptureNegative=negative;
  state_.inputCapturePrompt=source==runtime::InputSource::Key?
      negative?"Pressione a tecla negativa":"Pressione uma tecla":
      source==runtime::InputSource::GamepadButton?"Pressione um botão":source==runtime::InputSource::MouseButton?"Clique um botão do mouse":"Mova o eixo do gamepad";
  state_.inputCaptureFeedback=source==runtime::InputSource::GamepadAxis?"Solte o eixo antes de mover":"Aguardando nova pressão";
  state_.status="Capturando · "+action.id;return true;
}

bool EditorSession::commitInputCapture(u32 code,bool invert) {
  if(!validateInputCapture()) {cancelInputBindingCapture();state_.status="Captura cancelada: a autoria mudou";return false;}
  auto map=inputCapture_->before;auto action=*map.find(inputCapture_->action);auto &binding=action.bindings[inputCapture_->binding];
  if(binding.source==runtime::InputSource::Key) {
    if(code==(inputCapture_->negative?binding.code:binding.negativeCode) && code) {
      state_.status="Use teclas diferentes para as duas direções";state_.inputCaptureFeedback=state_.status;return false;
    }
    if(inputCapture_->negative) binding.negativeCode=code;else binding.code=code;
  } else {
    binding.code=code;if(binding.source==runtime::InputSource::GamepadAxis) binding.invert=invert;
  }
  const auto name=action.id;
  if(!map.replace(name,action) || !history_.setInputActions(document_,map)) {
    cancelInputBindingCapture();state_.status="O vínculo capturado foi recusado";return false;
  }
  cancelInputBindingCapture();state_.status="Vínculo capturado · "+name+" · código "+std::to_string(code);return true;
}

bool EditorSession::captureInputKey(u32 code,bool gamepad,bool down,bool repeated) {
  if(!inputCapture_) return false;
  if(!validateInputCapture()) {cancelInputBindingCapture();return false;}
  if(down && !gamepad && (code==4 || code==111)) {cancelInputBindingCapture();state_.status="Captura cancelada";return true;}
  if(repeated) {state_.inputCaptureFeedback="Solte a tecla antes de pressionar";return true;}
  if(!down || !code || code>65535 || code==3 || code==24 || code==25 || code==26) return true;
  const auto &action=*inputCapture_->before.find(inputCapture_->action);
  const auto source=action.bindings[inputCapture_->binding].source;
  if(source==(gamepad?runtime::InputSource::GamepadButton:runtime::InputSource::Key)) commitInputCapture(code,false);
  else state_.inputCaptureFeedback="Entrada de outra fonte ignorada";
  return true;
}

bool EditorSession::captureInputMouseButton(u32 code,bool down,UiPoint position) {
  if(!inputCapture_) return false;
  if(!validateInputCapture()) {cancelInputBindingCapture();return false;}
  if(!down || code>=5) return true;
  const auto hit=router_.hitTest(position);
  if(code==0 && hit.target==UiPointerTarget::Widget && hit.widgetId==widgetId(EditorWidget::InputBindingCaptureCancel)) {
    cancelInputBindingCapture();state_.status="Captura cancelada";return true;
  }
  const auto &action=*inputCapture_->before.find(inputCapture_->action);
  if(action.bindings[inputCapture_->binding].source==runtime::InputSource::MouseButton)commitInputCapture(code,false);
  else state_.inputCaptureFeedback="Entrada de outra fonte ignorada";
  return true;
}

bool EditorSession::captureInputAxes(const std::array<float,8> &axes) {
  if(!inputCapture_) return false;
  if(!validateInputCapture()) {cancelInputBindingCapture();return false;}
  const auto &action=*inputCapture_->before.find(inputCapture_->action);
  if(action.bindings[inputCapture_->binding].source!=runtime::InputSource::GamepadAxis) return true;
  u32 selected=8;float magnitude=.65f;bool heldBeforeArm=false;
  for(u32 i=0;i<axes.size();++i) {
    if(!std::isfinite(axes[i]) || std::abs(axes[i])>1.01f) continue;
    if(std::abs(axes[i])<.2f) inputCapture_->neutralAxes|=1u<<i;
    if(!(inputCapture_->neutralAxes&(1u<<i)) && std::abs(axes[i])>.65f) heldBeforeArm=true;
    if((inputCapture_->neutralAxes&(1u<<i)) && std::abs(axes[i])>magnitude) {selected=i;magnitude=std::abs(axes[i]);}
  }
  if(selected<axes.size()) commitInputCapture(selected,axes[selected]<0);
  else state_.inputCaptureFeedback=heldBeforeArm || !inputCapture_->neutralAxes?"Solte o eixo antes de mover":"Mova o eixo com mais força";
  return true;
}

bool EditorSession::startPlay() {
  cancelInputBindingCapture();
  if(isPlaying()) return true;
  // As mesmas travas do botão Play: fonte compilando ou não publicada.
  if(state_.codeBuildBusy) return false;
  if(state_.code && (state_.code->catalogState()==EditorCodeCatalogState::Failed ||
      state_.code->catalogState()==EditorCodeCatalogState::Stale || state_.code->dirty())) return false;
  state_.workspace=EditorWorkspace::Play;state_.playPaused=false;state_.playStepRequested=false;
  preparePlay();
  return playRequested_;
}

void EditorSession::preparePlay() {
  if(playScene_.active()) {endPlayInspect();playScene_.stop();runtimeTextures_.clear();}
  state_.playTimeScale=1;
  std::vector<EditorEntityId> tagObjects;document_.collectSubtree(document_.root(),tagObjects);
  for(const auto id:tagObjects) if(!document_.tags().contains(document_.find(id)->tag)) {
    state_.status="Tag ausente no catálogo: "+document_.find(id)->tag;
    state_.workspace=EditorWorkspace::Scene;playRequested_=false;return;
  }
  state_.playHasScripts=runtime::ScriptBridge::hasScripts(document_);
  // O primeiro frame do projeto antecede a compilação automática. Um catálogo
  // vazio não é um assembly pronto: a mesma barreira vale para botão e autostart.
  if(state_.playHasScripts && code_.catalogState()!=EditorCodeCatalogState::Current) {
    state_.status="Aguarde a publicação dos scripts antes de Play";
    state_.workspace=EditorWorkspace::Scene;playRequested_=false;return;
  }
  state_.playSecondaryActionLabel.clear();state_.playHudMessage.clear();
  for(const auto &action:document_.inputActions().actions()) {
    if(action.kind!=runtime::ActionKind::Button) continue;
    for(const auto &binding:action.bindings)
      if(binding.source==runtime::InputSource::TouchButton && binding.code==1) {
        state_.playSecondaryActionLabel=action.id;break;
      }
    if(!state_.playSecondaryActionLabel.empty()) break;
  }
  const auto view=resolveSceneCamera(document_);
  const auto *camera=document_.find(view.entity);
  state_.playFirstPerson=camera&&cameraLook(*camera);
  state_.playHasCharacter=false;
  for(auto *ancestor=camera;ancestor;ancestor=document_.find(ancestor->parent))
    if(characterComponent(*ancestor)) {state_.playHasCharacter=true;break;}
  playRequested_ = true;
  cancelPointers();
}

} // namespace ae::editor
