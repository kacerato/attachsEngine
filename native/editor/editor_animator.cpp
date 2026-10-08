// Editor de grafo do Animator: toque, arrasto e edições (bloco I). O desenho
// está em editor_screen.cpp (buildAnimatorEditor) e a geometria comum em
// editor_animator_view.h. Toda edição passa pelo histórico; arrastar um estado
// vira um único passo de desfazer.
#include "editor/editor_session.h"
#include "editor/editor_animator_view.h"
#include "editor/editor_numeric_expression.h"
#include "scene/animator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ae::editor {
namespace {
namespace w=animator_widget;namespace view=animator_view;
using T=scene::AnimatorParameterType;using M=scene::AnimatorConditionMode;
bool numericParameter(const scene::AnimatorParameter &p) {return p.type==T::Float||p.type==T::Int;}
std::vector<M> modesFor(T type) {
  switch(type) {
    case T::Float: return {M::Greater,M::Less};
    case T::Int: return {M::Greater,M::Less,M::Equals,M::NotEqual};
    case T::Bool: return {M::If,M::IfNot};
    default: return {M::If};
  }
}
std::string uniqueName(const std::vector<std::string> &taken,std::string base) {
  for(u32 n=1;;++n) {
    const std::string name=n==1?base:base+" "+std::to_string(n);
    if(std::find(taken.begin(),taken.end(),name)==taken.end()) return name;
  }
}
// Próximo parâmetro numérico depois de `current`; com `allowNone`, o ciclo passa pelo zero.
u64 cycleParameter(const scene::Animator &a,u64 current,bool allowNone) {
  std::vector<u64> ids;if(allowNone) ids.push_back(0);
  for(const auto &p:a.parameters) if(numericParameter(p)) ids.push_back(p.id);
  if(ids.empty()) return 0;
  const auto f=std::find(ids.begin(),ids.end(),current);
  return f==ids.end()||f+1==ids.end()?ids.front():*(f+1);
}
}

const scene::Animator *EditorSession::openAnimator() const {
  const runtime::SceneGraph &graph=playScene_.active()&&(isPlaying()||playMirrorOpen_)?playScene_.document():static_cast<const runtime::SceneGraph&>(document_);
  const auto *entity=graph.find(state_.animatorEntity);
  const auto *c=entity?entity->components.findInstance(state_.animatorInstance):nullptr;
  if(!c||&c->type()!=&scene::Animator::descriptor) return nullptr;
  const auto &instance=scene::animator(*c);
  if(!instance.controller.valid()) return &instance;
  if(playScene_.active()&&(isPlaying()||playMirrorOpen_)) {
    const auto *resolved=playScene_.animatorGraphs().configuration(playScene_.world(),entity->id,c->instanceId());
    return resolved?resolved:&instance; // Inspection only; runtime never evaluates this fallback.
  }
  if(animatorResourceDrag_) return &*animatorResourceDrag_;
  std::string diagnostic;
  if(!resources::resolveAnimatorController(instance,animatorControllers_,animatorResolved_,diagnostic)) {
    // Keep the unavailable instance inspectable for replace/reload/detach
    // actions; this inline graph is never used as a runtime fallback.
    return &instance;
  }
  if(state_.animatorEditShared) {
    const auto *asset=resources::findAnimatorController(animatorControllers_,instance.controller);
    const auto controls=animatorResolved_;animatorResolved_=asset->graph;
    animatorResolved_.controller=instance.controller;animatorResolved_.target=controls.target;animatorResolved_.motionSource=controls.motionSource;
    for(auto &layer:animatorResolved_.layers) if(const auto *local=controls.layer(layer.id)) layer.mask=local->mask;
  }
  return &animatorResolved_;
}

void EditorSession::openAnimatorEditor() {
  const auto target=state_.inspectorTarget?state_.inspectorTarget:state_.selection;
  const runtime::SceneGraph &graph=isPlaying()&&playScene_.active()?playScene_.document():static_cast<const runtime::SceneGraph&>(document_);
  const auto *entity=graph.find(target);
  const auto *c=entity?entity->components.find(scene::Animator::descriptor):nullptr;
  if(!c) return;
  state_.animatorOpen=true;state_.animatorEntity=entity->id;state_.animatorInstance=c->instanceId();
  state_.animatorLayer=0;state_.animatorState=state_.animatorTransition=state_.animatorParameter=0;
  state_.animatorConnecting=false;state_.animatorConnectFrom=0;state_.animatorPicker=0;
  state_.animatorDrawer=0;state_.animatorDetailsScroll=state_.animatorParamsScroll=0;
  state_.animatorEditShared=false;animatorResourceDrag_.reset();
  animatorPointer_=animatorSecondPointer_=animatorScrollSheet_=0;animatorDragOriginal_.reset();
  frameAnimator();
}

void EditorSession::frameAnimator() {
  const auto *a=openAnimator();if(!a||a->layers.empty()) return;
  const auto &layer=a->layers[std::min<usize>(state_.animatorLayer,a->layers.size()-1)];
  float minX=view::EntryX,minY=view::EntryY,maxX=view::AnyX+view::NodeWidth,maxY=view::AnyY+view::NodeHeight;
  for(const auto &s:layer.states) {minX=std::min(minX,s.x);minY=std::min(minY,s.y);maxX=std::max(maxX,s.x+view::NodeWidth);maxY=std::max(maxY,s.y+view::NodeHeight);}
  state_.animatorPan[0]=-(minX+maxX)*.5f;state_.animatorPan[1]=-(minY+maxY)*.5f;
  const auto &canvas=layout_.animatorCanvas;
  if(canvas.width>0&&canvas.height>0)
    state_.animatorZoom=std::clamp(std::min((canvas.width-80)/(maxX-minX),(canvas.height-80)/(maxY-minY)),.45f,1.4f);
}

bool EditorSession::editAnimatorInstance(const std::function<bool(scene::Animator &)> &change) {
  if(animatorResourceDrag_) {state_.status="Conclua o arrasto antes de editar";return false;}
  if(isPlaying()||playMirrorOpen_||history_.isOpen()) {state_.status="Pare o Play para editar o grafo";return false;}
  const auto *entity=document_.find(state_.animatorEntity);if(!entity) return false;
  auto values=*entity;
  auto *c=values.components.editInstance(state_.animatorInstance);
  if(!c||&c->type()!=&scene::Animator::descriptor) return false;
  auto &a=scene::animator(*c);
  if(!change(a)) return false;
  if(!a.valid()) {state_.status="Mudança recusada: o grafo ficaria inválido";return false;}
  return history_.applyValues(document_,entity->id,values);
}
bool EditorSession::editAnimator(const std::function<bool(scene::Animator &)> &change) {
  if(animatorResourceDrag_) {state_.status="Conclua o arrasto antes de editar";return false;}
  const auto *current=openAnimator();if(!current) return false;
  if(!current->controller.valid()) return editAnimatorInstance(change);
  if(isPlaying()||playMirrorOpen_||history_.isOpen()||!state_.animatorEditShared) {
    state_.status="Instância: edite overrides ou abra o recurso compartilhado";return false;
  }
  const auto *asset=resources::findAnimatorController(animatorControllers_,current->controller);if(!asset) return false;
  auto candidate=*asset;if(!change(candidate.graph)) return false;
  candidate.graph=resources::AnimatorControllerAsset::portableGraph(candidate.graph);++candidate.revision;
  std::string diagnostic;const bool ok=commitAnimatorController(candidate,diagnostic);
  state_.status=ok?"Recurso atualizado · todas as instâncias":diagnostic;return ok;
}

void EditorSession::beginAnimatorNumber(u32 code,double current) {
  state_.numericField=w::id(code);state_.numericEntity=state_.animatorEntity;state_.numericInstance=0;state_.numericProperty.clear();
  state_.numericCurrent=current;std::snprintf(state_.numericText,sizeof state_.numericText,"%g",current);state_.numericError=false;
}
void EditorSession::beginAnimatorName(u32 code,const std::string &current) {
  state_.editingAnimatorName=true;state_.animatorNameField=code;
  std::snprintf(state_.renameText,sizeof state_.renameText,"%s",current.c_str());
}

bool EditorSession::applyAnimatorName(u32 code,const std::string &name) {
  if(!scene::Animator::validName(name)) {state_.status="Nome vazio, longo demais ou com aspas";return false;}
  const u32 layerIndex=state_.animatorLayer;
  return editAnimator([&](scene::Animator &a){
    if(layerIndex>=a.layers.size()) return false;
    auto &layer=a.layers[layerIndex];
    if(code==w::LayerName) {for(const auto &l:a.layers) if(l.id!=layer.id&&l.name==name) return false;layer.name=name;return true;}
    if(code==w::StateName) {
      auto *s=layer.state(state_.animatorState);if(!s) return false;
      for(const auto &o:layer.states) if(o.id!=s->id&&o.name==name) {state_.status="Já existe um estado com esse nome nesta camada";return false;}
      s->name=name;return true;
    }
    if(code>=w::ParameterName&&code<w::ParameterName+0x100) {
      const u32 i=code-w::ParameterName;if(i>=a.parameters.size()) return false;
      for(usize k=0;k<a.parameters.size();++k) if(k!=i&&a.parameters[k].name==name) {state_.status="Já existe um parâmetro com esse nome";return false;}
      a.parameters[i].name=name;return true;
    }
    return false;
  });
}

bool EditorSession::applyAnimatorNumber(u32 code,double number) {
  if(!std::isfinite(number)) return false;
  const float v=static_cast<float>(number);const u32 layerIndex=state_.animatorLayer;
  if((isPlaying()||playMirrorOpen_)&&(code==w::LayerWeight||code==w::LayerReferenceTime)) {
    runtime::SceneAnimatorGraphs::LayerSettings out;
    return playScene_.animatorGraphs().layerControl(playScene_.world(),state_.animatorEntity,state_.animatorInstance,
      layerIndex,code==w::LayerWeight?1:3,v,{},out)==runtime::WorldStatus::Ok;
  }
  if((isPlaying()||playMirrorOpen_)&&code>=w::ParameterValue&&code<w::ParameterValue+0x100) {
    const auto *a=openAnimator();const u32 i=code-w::ParameterValue;
    if(!a||i>=a->parameters.size()) return false;
    const auto &p=a->parameters[i];float result=0;
    using Op=runtime::SceneAnimatorGraphs::ParameterOperation;
    const auto op=p.type==T::Int?Op::SetInt:p.type==T::Bool?Op::SetBool:Op::SetFloat;
    return playScene_.animatorGraphs().parameter(playScene_.world(),state_.animatorEntity,state_.animatorInstance,p.name,op,v,result)==runtime::SceneAnimatorGraphs::Status::Ok;
  }
  return editAnimator([&](scene::Animator &a){
    if(layerIndex>=a.layers.size()) return false;
    auto &layer=a.layers[layerIndex];
    if(code==w::ParameterResponse||code==w::ParameterScale) {
      for(auto &p:a.parameters) if(p.id==state_.animatorParameter) {
        if(code==w::ParameterResponse) p.response=std::clamp(v,0.f,10.f);else p.scale=std::clamp(v,-100.f,100.f);
        return true;
      }
      return false;
    }
    auto *s=layer.state(state_.animatorState);
    scene::AnimatorTransition *t=nullptr;for(auto &x:layer.transitions) if(x.id==state_.animatorTransition) t=&x;
    if(code==w::LayerWeight) {layer.weight=std::clamp(v,0.f,1.f);return true;}
    if(code==w::LayerReferenceTime) {layer.referenceTime=std::clamp(v,0.f,86400.f);return true;}
    if(code>=w::ParameterValue&&code<w::ParameterValue+0x100) {
      const u32 i=code-w::ParameterValue;if(i>=a.parameters.size()) return false;
      a.parameters[i].value=a.parameters[i].type==T::Int?std::round(v):v;return true;
    }
    if(s) {
      if(code==w::StateSpeed) {s->speed=std::clamp(v,-10.f,10.f);return true;}
      const auto motion=[&](u32 base)->scene::AnimatorMotion*{const u32 m=code-base;return m<s->motions.size()?&s->motions[m]:nullptr;};
      if(code>=w::MotionThreshold&&code<w::MotionThreshold+0x100) {if(auto *m=motion(w::MotionThreshold)) {m->threshold=v;return true;}}
      if(code>=w::MotionX&&code<w::MotionX+0x100) {if(auto *m=motion(w::MotionX)) {m->x=v;return true;}}
      if(code>=w::MotionY&&code<w::MotionY+0x100) {if(auto *m=motion(w::MotionY)) {m->y=v;return true;}}
      if(code>=w::EventTime&&code<w::EventTime+0x100) {const u32 e=code-w::EventTime;if(e<s->events.size()) {s->events[e].time=std::clamp(v,0.f,1.f);return true;}}
      if(code>=w::EventTag&&code<w::EventTag+0x100) {const u32 e=code-w::EventTag;if(e<s->events.size()&&v>=0) {s->events[e].tag=static_cast<u32>(std::lround(v));return true;}}
    }
    if(t) {
      if(code==w::TransitionExitTime) {t->exitTime=std::clamp(v,0.f,100.f);return true;}
      if(code==w::TransitionDuration) {t->duration=std::clamp(v,0.f,60.f);return true;}
      if(code>=w::ConditionThreshold&&code<w::ConditionThreshold+0x100) {const u32 c=code-w::ConditionThreshold;if(c<t->conditions.size()) {t->conditions[c].threshold=v;return true;}}
    }
    return false;
  });
}

bool EditorSession::handleAnimatorEditor(const ui::UiPointerEvent &input,const ui::UiPointerRouting &routing) {
  // Capture fields use zero for no finger. Android's first physical finger is
  // pointer 0, so keep a one-based capture identity inside this gesture handler.
  // Router ownership continues to use the original input identity.
  auto event=input;++event.pointerId;
  const auto *a=openAnimator();
  if(!a||a->layers.empty()) {state_.animatorOpen=false;return false;}
  state_.animatorLayer=std::min<u32>(state_.animatorLayer,static_cast<u32>(a->layers.size())-1);
  const u32 layerIndex=state_.animatorLayer;const auto &layer=a->layers[layerIndex];
  const bool playing=isPlaying()||playMirrorOpen_;
  const bool readOnly=playing||(a->controller.valid()&&!state_.animatorEditShared);
  const u32 key=routing.widgetId;
  const view::View v{layout_.animatorCanvas,state_.animatorPan[0],state_.animatorPan[1],state_.animatorZoom};

  // A vertical drag scrolls the sheet even when it begins on a value row.
  // Taps still reach the field; a scrolled release never edits that field.
  if(!state_.animatorPicker&&key!=w::id(w::Canvas)) {
    const bool details=view::inside(layout_.animatorDetailsWindow,event.position);
    const bool params=view::inside(layout_.animatorParamsWindow,event.position);
    if(event.phase==ui::UiPointerPhase::Down&&(details||params)) {
      animatorPointer_=event.pointerId;animatorPress_=event.position;animatorDragged_=false;
      animatorPressScroll_=details?state_.animatorDetailsScroll:state_.animatorParamsScroll;
      animatorScrollSheet_=details?1:2;
    } else if(event.pointerId==animatorPointer_&&event.phase==ui::UiPointerPhase::Move&&routing.dragging&&animatorScrollSheet_) {
      const bool scrollDetails=animatorScrollSheet_==1;
      animatorDragged_=true;const auto window=scrollDetails?layout_.animatorDetailsWindow:layout_.animatorParamsWindow;
      const float extent=scrollDetails?layout_.animatorDetailsExtent:layout_.animatorParamsExtent;
      (scrollDetails?state_.animatorDetailsScroll:state_.animatorParamsScroll)=std::clamp(animatorPressScroll_+animatorPress_.y-event.position.y,0.f,std::max(0.f,extent-window.height));
      return true;
    } else if(event.pointerId==animatorPointer_&&event.phase==ui::UiPointerPhase::Up&&animatorDragged_) {animatorPointer_=animatorScrollSheet_=0;animatorDragged_=false;return true;}
    else if(event.pointerId==animatorPointer_&&(event.phase==ui::UiPointerPhase::Up||event.phase==ui::UiPointerPhase::Cancel)) animatorPointer_=animatorScrollSheet_=0;
  }

  // ---- Grafo: selecionar, mover estados, navegar, ligar transições -------
  if(key==w::id(w::DetailsScroll)||key==w::id(w::ParamsScroll)) {
    const bool details=key==w::id(w::DetailsScroll);
    float &scroll=details?state_.animatorDetailsScroll:state_.animatorParamsScroll;
    const auto window=details?layout_.animatorDetailsWindow:layout_.animatorParamsWindow;
    const float extent=details?layout_.animatorDetailsExtent:layout_.animatorParamsExtent;
    if(event.phase==ui::UiPointerPhase::Down) {animatorPointer_=event.pointerId;animatorPress_=event.position;animatorPressScroll_=scroll;}
    else if(event.pointerId==animatorPointer_&&event.phase==ui::UiPointerPhase::Move&&routing.dragging)
      scroll=std::clamp(animatorPressScroll_+animatorPress_.y-event.position.y,0.f,std::max(0.f,extent-window.height));
    return true;
  }
  if(key==w::id(w::Canvas)) {
    if(event.phase==ui::UiPointerPhase::Cancel) {
      animatorResourceDrag_.reset();
      if(animatorDragOriginal_) {document_.applyEntityValues(state_.animatorEntity,*animatorDragOriginal_);animatorDragOriginal_.reset();}
      animatorPointer_=animatorSecondPointer_=animatorScrollSheet_=0;animatorDragged_=true;return true;
    }
    if(event.phase==ui::UiPointerPhase::Down&&animatorPointer_&&animatorPointer_!=event.pointerId) {
      animatorResourceDrag_.reset();
      if(animatorDragOriginal_) {document_.applyEntityValues(state_.animatorEntity,*animatorDragOriginal_);animatorDragOriginal_.reset();}
      animatorSecondPointer_=event.pointerId;animatorSecondaryPosition_=event.position;
      animatorPinchDistance_=std::max(1.f,std::hypot(animatorPrimaryPosition_.x-event.position.x,animatorPrimaryPosition_.y-event.position.y));
      animatorPinchZoom_=state_.animatorZoom;
      const ui::UiPoint midpoint{(animatorPrimaryPosition_.x+event.position.x)*.5f,(animatorPrimaryPosition_.y+event.position.y)*.5f};
      view::toGraph(v,midpoint,animatorPinchAnchor_.x,animatorPinchAnchor_.y);animatorDragged_=true;return true;
    }
    if(animatorSecondPointer_) {
      if(event.phase==ui::UiPointerPhase::Move) {
        if(event.pointerId==animatorPointer_) animatorPrimaryPosition_=event.position;
        else if(event.pointerId==animatorSecondPointer_) animatorSecondaryPosition_=event.position;else return true;
        const float distance=std::hypot(animatorPrimaryPosition_.x-animatorSecondaryPosition_.x,animatorPrimaryPosition_.y-animatorSecondaryPosition_.y);
        const ui::UiPoint midpoint{(animatorPrimaryPosition_.x+animatorSecondaryPosition_.x)*.5f,(animatorPrimaryPosition_.y+animatorSecondaryPosition_.y)*.5f};
        state_.animatorZoom=std::clamp(animatorPinchZoom_*distance/animatorPinchDistance_,.35f,1.8f);
        state_.animatorPan[0]=(midpoint.x-v.canvas.x-v.canvas.width*.5f)/state_.animatorZoom-animatorPinchAnchor_.x;
        state_.animatorPan[1]=(midpoint.y-v.canvas.y-v.canvas.height*.5f)/state_.animatorZoom-animatorPinchAnchor_.y;
      } else if(event.phase==ui::UiPointerPhase::Up||event.phase==ui::UiPointerPhase::Cancel) {animatorPointer_=animatorSecondPointer_=0;animatorDragged_=true;}
      return true;
    }
    if(event.phase==ui::UiPointerPhase::Down) {
      animatorPointer_=event.pointerId;animatorPress_=event.position;animatorDragged_=false;
      animatorScrollSheet_=0;
      animatorPrimaryPosition_=event.position;
      animatorPressPan_[0]=state_.animatorPan[0];animatorPressPan_[1]=state_.animatorPan[1];
      animatorPressNode_=view::hitNode(v,layer,event.position);
      animatorPressTransition_=animatorPressNode_?0:view::hitTransition(v,layer,event.position);
      animatorDragOriginal_.reset();
      animatorResourceDrag_.reset();
      if(const auto *s=layer.state(animatorPressNode_)) {animatorNodeStart_[0]=s->x;animatorNodeStart_[1]=s->y;}
      return true;
    }
    if(event.pointerId!=animatorPointer_) return true;
    if(event.phase==ui::UiPointerPhase::Move&&routing.dragging) {
      animatorPrimaryPosition_=event.position;
      animatorDragged_=true;
      const float dx=(event.position.x-animatorPress_.x)/state_.animatorZoom,dy=(event.position.y-animatorPress_.y)/state_.animatorZoom;
      if(layer.state(animatorPressNode_)&&!readOnly&&!state_.animatorConnecting&&!history_.isOpen()) {
        if(a->controller.valid()) {
          if(!animatorResourceDrag_) animatorResourceDrag_=*a;
          if(auto *s=animatorResourceDrag_->layers[layerIndex].state(animatorPressNode_)) {s->x=animatorNodeStart_[0]+dx;s->y=animatorNodeStart_[1]+dy;}
          return true;
        }
        // Arrasto ao vivo direto no documento; o passo do histórico sai no fim.
        auto *entity=document_.find(state_.animatorEntity);if(!entity) return true;
        if(!animatorDragOriginal_) animatorDragOriginal_=*entity;
        auto values=*entity;
        auto &copy=scene::animator(*values.components.editInstance(state_.animatorInstance));
        if(auto *s=copy.layers[layerIndex].state(animatorPressNode_)) {s->x=animatorNodeStart_[0]+dx;s->y=animatorNodeStart_[1]+dy;}
        document_.applyEntityValues(entity->id,values);
      } else {
        state_.animatorPan[0]=animatorPressPan_[0]+dx;state_.animatorPan[1]=animatorPressPan_[1]+dy;
      }
      return true;
    }
    if(event.phase==ui::UiPointerPhase::Up) {
      animatorPointer_=0;
      if(animatorResourceDrag_) {
        const auto *moved=animatorResourceDrag_->layers[layerIndex].state(animatorPressNode_);
        const float x=moved?std::round(moved->x/16)*16:0,y=moved?std::round(moved->y/16)*16:0;
        animatorResourceDrag_.reset();
        editAnimator([&](scene::Animator &edit){if(auto *s=edit.layers[layerIndex].state(animatorPressNode_)) {s->x=x;s->y=y;return true;}return false;});
        return true;
      }
      if(animatorDragOriginal_) {
        // Fim do arrasto: volta ao original e grava a posição final (na grade de 16) num passo só.
        const auto *entity=document_.find(state_.animatorEntity);
        auto finalValues=*entity;
        auto &moved=scene::animator(*finalValues.components.editInstance(state_.animatorInstance));
        if(auto *s=moved.layers[layerIndex].state(animatorPressNode_)) {s->x=std::round(s->x/16)*16;s->y=std::round(s->y/16)*16;}
        document_.applyEntityValues(entity->id,*animatorDragOriginal_);animatorDragOriginal_.reset();
        history_.applyValues(document_,entity->id,finalValues);
        return true;
      }
      if(!routing.tapped||animatorDragged_) return true;
      const u64 node=animatorPressNode_;
      if(state_.animatorConnecting&&!readOnly) {
        if(!state_.animatorConnectFrom) {
          if(node&&node!=view::EntryNode) {state_.animatorConnectFrom=node;state_.status="Agora toque no estado de destino";}
          return true;
        }
        if(layer.state(node)) {
          const u64 from=state_.animatorConnectFrom==view::AnyNode?0:state_.animatorConnectFrom;
          if(from==node&&from!=0) {state_.status="Transição para o próprio estado: use Qualquer estado";return true;}
          u64 created=0;
          // Padrões da Unity: com tempo de saída em 0,75 e mistura de 0,25 s; de Qualquer estado, sem tempo de saída.
          if(editAnimator([&](scene::Animator &edit){
               auto &l=edit.layers[layerIndex];if(l.transitions.size()>=scene::Animator::MaximumTransitions) return false;
               scene::AnimatorTransition t;t.id=edit.allocateId();t.from=from;t.to=node;t.hasExitTime=from!=0;t.exitTime=.75f;t.duration=.25f;
               created=t.id;l.transitions.push_back(t);return true;})) {
            state_.animatorTransition=created;state_.animatorState=0;state_.status="Transição criada";
          }
          state_.animatorConnecting=false;state_.animatorConnectFrom=0;
        }
        return true;
      }
      state_.animatorParameter=0;
      state_.animatorDetailsScroll=0;
      if(node||animatorPressTransition_) state_.animatorDrawer=2;
      if(layer.state(node)) {state_.animatorState=node;state_.animatorTransition=0;}
      else if(animatorPressTransition_) {state_.animatorTransition=animatorPressTransition_;state_.animatorState=0;}
      else {state_.animatorState=0;state_.animatorTransition=0;}
      return true;
    }
    return true;
  }
  if(!routing.tapped) return true;
  if(!w::owns(key)) return true;   // o editor cobre a tela: o resto não recebe toque
  const u32 code=w::code(key);

  // ---- Seletores -----------------------------------------------------------
  if(code==w::ClipClose||code==w::MaskClose) {state_.animatorPicker=0;return true;}
  if(code==w::PickerPrevious) {if(state_.animatorPickerPage) --state_.animatorPickerPage;return true;}
  if(code==w::PickerNext) {++state_.animatorPickerPage;return true;}
  if(code>=w::ControllerChoice&&code<w::ControllerChoice+0xF00&&state_.animatorPicker==0x50000u) {
    const u32 index=code-w::ControllerChoice;state_.animatorPicker=0;
    if(index<animatorControllers_.size()) assignAnimatorController(animatorControllers_[index].guid);
    return true;
  }
  if(code>=w::ClipChoice&&code<w::ClipChoice+0xF00&&(state_.animatorPicker&0xF0000u)==0x40000u) {
    const auto *asset=resources::findAnimatorController(animatorControllers_,a->controller);
    const auto originals=asset?resources::animatorControllerClips(asset->graph):std::vector<resources::AssetGuid>{};
    const u32 index=state_.animatorPicker&0xFFFFu;state_.animatorPicker=0;
    const auto clips=mapScene_.clipCatalog();const u32 choice=code-w::ClipChoice;
    if(index<originals.size()&&choice<clips.size()) overrideAnimatorClip(originals[index],clips[choice].clip);
    return true;
  }
  if(code>=w::OverrideClip&&code<w::OverrideClip+0x400&&!isPlaying()&&!playMirrorOpen_) {
    state_.animatorPicker=0x40000u+(code-w::OverrideClip);state_.animatorPickerPage=0;return true;
  }
  if(code>=w::OverrideReset&&code<w::OverrideReset+0x400&&!isPlaying()&&!playMirrorOpen_) {
    const auto *asset=resources::findAnimatorController(animatorControllers_,a->controller);
    const auto originals=asset?resources::animatorControllerClips(asset->graph):std::vector<resources::AssetGuid>{};
    const u32 index=code-w::OverrideReset;if(index<originals.size()) overrideAnimatorClip(originals[index],{});return true;
  }
  if(code>=w::OverrideOrphanReset&&code<w::OverrideOrphanReset+scene::Animator::MaximumOverrides&&!playing) {
    const u32 index=code-w::OverrideOrphanReset;
    editAnimatorInstance([&](scene::Animator &edit){if(index>=edit.clipOverrides.size()) return false;edit.clipOverrides.erase(edit.clipOverrides.begin()+index);return true;});return true;
  }
  if(code>=w::MaskChoice&&code<w::MaskChoice+5&&state_.animatorPicker==0x30000u) {
    const auto source=static_cast<scene::AnimatorParameterSource>(code-w::MaskChoice);state_.animatorPicker=0;
    editAnimator([&](scene::Animator &edit){for(auto &p:edit.parameters) if(p.id==state_.animatorParameter&&scene::animatorSourceCompatible(p.type,source)) {p.source=source;return true;}return false;});return true;
  }
  if(code>=w::ClipChoice&&code<w::ClipChoice+0xF00&&state_.animatorPicker&&state_.animatorPicker<0x10000u) {
    const auto catalog=mapScene_.clipCatalog();const u32 i=code-w::ClipChoice;
    const u32 motion=state_.animatorPicker-1;state_.animatorPicker=0;
    if(i>=catalog.size()) return true;
    const auto clip=catalog[i].clip;
    editAnimator([&](scene::Animator &edit){
      auto *s=edit.layers[layerIndex].state(state_.animatorState);if(!s) return false;
      if(motion>=scene::Animator::MaximumMotions) return false;
      if(s->motions.size()<=motion) s->motions.resize(motion+1);
      s->motions[motion].clip=clip;return true;});
    return true;
  }
  if(code>=w::ClipChoice&&code<w::ClipChoice+0xF00&&state_.animatorPicker==0x60000u) {
    const auto catalog=mapScene_.clipCatalog();const u32 choice=code-w::ClipChoice;state_.animatorPicker=0;
    if(choice>=catalog.size()) return true;
    if(playing) {
      runtime::SceneAnimatorGraphs::LayerSettings out;
      playScene_.animatorGraphs().layerControl(playScene_.world(),state_.animatorEntity,state_.animatorInstance,layerIndex,4,0,catalog[choice].clip,out);
    } else editAnimator([&](scene::Animator &edit){edit.layers[layerIndex].referenceClip=catalog[choice].clip;return true;});
    return true;
  }
  if(code>=w::MaskChoice&&code<w::MaskChoice+0xF00&&state_.animatorPicker==0x10000u) {
    state_.animatorPicker=0;u64 mask=0;
    if(code>w::MaskChoice) {
      std::vector<EditorEntityId> subtree;
      const auto *entity=document_.find(state_.animatorEntity);
      document_.collectSubtree(a->target?static_cast<EditorEntityId>(a->target):entity->id,subtree);
      const u32 i=code-w::MaskChoice-1;if(i<subtree.size()) mask=subtree[i];
    }
    editAnimatorInstance([&](scene::Animator &edit){
      const auto *resolved=openAnimator();if(!resolved||layerIndex>=resolved->layers.size()) return false;
      const auto layerId=resolved->layers[layerIndex].id;
      if(edit.controller.valid()) {
        const auto *asset=resources::findAnimatorController(animatorControllers_,edit.controller);if(!asset) return false;
        const auto masks=edit.layers;edit.parameters=asset->graph.parameters;edit.layers=asset->graph.layers;edit.nextId=asset->graph.nextId;
        for(auto &layer:edit.layers) if(const auto found=std::find_if(masks.begin(),masks.end(),[&](const auto &local){return local.id==layer.id;});found!=masks.end()) layer.mask=found->mask;
      }
      auto *local=edit.layer(layerId);if(!local) return false;
      local->mask=mask;return true;});
    return true;
  }
  if(code>=w::MaskChoice&&code<w::MaskChoice+0xF00&&state_.animatorPicker==0x20000u) {
    std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
    const u32 i=code-w::MaskChoice;state_.animatorPicker=0;
    if(i==0) editAnimatorInstance([](scene::Animator &edit){edit.motionSource=0;return true;});
    else if(i-1<ids.size()) editAnimatorInstance([&](scene::Animator &edit){edit.motionSource=ids[i-1];return true;});
    return true;
  }

  // ---- Barra superior ----------------------------------------------------
  switch(code) {
    case w::Controller: state_.animatorDrawer=state_.animatorDrawer==3?0:3;state_.animatorDetailsScroll=0;return true;
    case w::ControllerCreate: createAnimatorController();return true;
    case w::ControllerChoose: if(!isPlaying()&&!playMirrorOpen_) {state_.animatorPicker=0x50000u;state_.animatorPickerPage=0;}return true;
    case w::ControllerDetach: detachAnimatorController();return true;
    case w::ControllerEdit:
      if(!isPlaying()&&!playMirrorOpen_&&resources::findAnimatorController(animatorControllers_,a->controller)) {
        animatorResourceDrag_.reset();
        state_.animatorEditShared=!state_.animatorEditShared;state_.animatorConnecting=false;state_.animatorState=state_.animatorParameter=state_.animatorTransition=0;
        state_.status=state_.animatorEditShared?"Recurso: alterações afetam todas as instâncias":"Instância: overrides locais";
      }return true;
    case w::ControllerReload: if(!isPlaying()&&!playMirrorOpen_) {loadAnimatorControllers();state_.status="Controllers recarregados; identidades e overrides preservados";}return true;
    case w::ControllerReset: editAnimatorInstance([](scene::Animator &edit){edit.clipOverrides.clear();return true;});return true;
    case w::ParameterTrigger:
      if(const auto *p=a->parameter(state_.animatorParameter);p&&playing&&p->type==T::Trigger) {
        float result=0;playScene_.animatorGraphs().parameter(playScene_.world(),state_.animatorEntity,state_.animatorInstance,p->name,
          runtime::SceneAnimatorGraphs::ParameterOperation::SetTrigger,1,result);
      }return true;
    case w::Parameters: state_.animatorDrawer=state_.animatorDrawer==1?0:1;return true;
    case w::Details: state_.animatorDrawer=state_.animatorDrawer==2?0:2;return true;
    case w::Undo: if(!isPlaying()&&!playMirrorOpen_) {history_.undo(document_);state_.animatorDetailsScroll=0;}return true;
    case w::Redo: if(!isPlaying()&&!playMirrorOpen_) {history_.redo(document_);state_.animatorDetailsScroll=0;}return true;
    case w::MotionSource: if(!isPlaying()&&!playMirrorOpen_) {state_.animatorPicker=0x20000u;state_.animatorPickerPage=0;}return true;
    case w::ParameterSource: if(!readOnly) {state_.animatorPicker=0x30000u;state_.animatorPickerPage=0;}return true;
    case w::ParameterResponse: case w::ParameterScale:
      if(const auto *p=a->parameter(state_.animatorParameter);p&&!readOnly) beginAnimatorNumber(code,code==w::ParameterResponse?p->response:p->scale);
      return true;
    case w::Duplicate: {
      u64 created=0;
      editAnimator([&](scene::Animator &edit){auto &l=edit.layers[layerIndex];const auto *original=l.state(state_.animatorState);
        if(!original||l.states.size()>=scene::Animator::MaximumStates) return false;
        auto copy=*original;copy.id=edit.allocateId();std::vector<std::string> names;for(const auto &s:l.states) names.push_back(s.name);
        copy.name=uniqueName(names,original->name);copy.x+=32;copy.y+=80;created=copy.id;l.states.push_back(std::move(copy));return true;});
      if(created) {state_.animatorState=created;state_.animatorDrawer=2;}return true;
    }
    case w::TransitionEarlier: case w::TransitionLater:
      editAnimator([&](scene::Animator &edit){
        auto &v=edit.layers[layerIndex].transitions;
        for(usize i=0;i<v.size();++i) {
          if(v[i].id!=state_.animatorTransition) continue;
          if(code==w::TransitionEarlier&&i>0) {std::swap(v[i],v[i-1]);return true;}
          if(code==w::TransitionLater&&i+1<v.size()) {std::swap(v[i],v[i+1]);return true;}
        }
        return false;
      });return true;
    case w::Close:
      animatorResourceDrag_.reset();
      if(animatorDragOriginal_) {document_.applyEntityValues(state_.animatorEntity,*animatorDragOriginal_);animatorDragOriginal_.reset();}
      animatorPointer_=animatorSecondPointer_=animatorScrollSheet_=0;
      state_.animatorOpen=false;state_.animatorConnecting=false;state_.animatorPicker=0;return true;
    case w::Frame: frameAnimator();return true;
    case w::ZoomIn: state_.animatorZoom=std::min(1.8f,state_.animatorZoom*1.25f);return true;
    case w::ZoomOut: state_.animatorZoom=std::max(.35f,state_.animatorZoom/1.25f);return true;
    case w::Connect: state_.animatorConnecting=!state_.animatorConnecting;state_.animatorConnectFrom=0;
      if(state_.animatorConnecting) {state_.status="Toque no estado de origem (ou em Qualquer estado)";}
      return true;
    case w::LayerAdd: {
      u32 created=0;
      editAnimator([&](scene::Animator &edit){
        std::vector<std::string> names;for(const auto &l:edit.layers) names.push_back(l.name);
        scene::AnimatorLayer l;l.id=edit.allocateId();l.name=uniqueName(names,"Camada");l.weight=1;
        scene::AnimatorState s;s.id=edit.allocateId();s.name="Vazio";s.x=-view::NodeWidth*.5f;s.y=-view::NodeHeight*.5f;
        l.defaultState=s.id;l.states.push_back(s);edit.layers.push_back(l);created=static_cast<u32>(edit.layers.size())-1;return true;});
      if(created) {state_.animatorLayer=created;state_.animatorState=state_.animatorTransition=0;}
      return true;
    }
    case w::LayerDelete:
      if(layerIndex>0&&editAnimator([&](scene::Animator &edit){edit.layers.erase(edit.layers.begin()+layerIndex);return true;})) {
        state_.animatorLayer=0;state_.animatorState=state_.animatorTransition=0;
      }
      return true;
    case w::LayerName: if(!readOnly) beginAnimatorName(code,layer.name);return true;
    case w::LayerWeight:
    case w::LayerReferenceTime: {
      float v=code==w::LayerWeight?layer.weight:layer.referenceTime;
      if(playing) {runtime::SceneAnimatorGraphs::LayerSettings out;if(playScene_.animatorGraphs().layerControl(playScene_.world(),state_.animatorEntity,state_.animatorInstance,layerIndex,0,0,{},out)==runtime::WorldStatus::Ok) v=code==w::LayerWeight?out.weight:out.referenceTime;}
      if(!readOnly||playing) beginAnimatorNumber(code,v);
      return true;
    }
    case w::LayerBlend: {
      if(playing) {
        runtime::SceneAnimatorGraphs::LayerSettings out;auto &graphs=playScene_.animatorGraphs();
        if(graphs.layerControl(playScene_.world(),state_.animatorEntity,state_.animatorInstance,layerIndex,0,0,{},out)==runtime::WorldStatus::Ok)
          graphs.layerControl(playScene_.world(),state_.animatorEntity,state_.animatorInstance,layerIndex,2,out.blend==scene::AnimatorLayerBlend::Override?1.f:0.f,{},out);
      } else if(!readOnly) editAnimator([&](scene::Animator &edit){auto &l=edit.layers[layerIndex];l.blend=l.blend==scene::AnimatorLayerBlend::Override?scene::AnimatorLayerBlend::Additive:scene::AnimatorLayerBlend::Override;return true;});
      return true;
    }
    case w::LayerReferenceClip: if(!readOnly||playing) {state_.animatorPicker=0x60000u;state_.animatorPickerPage=0;}return true;
    case w::LayerReferenceReset: {
      if(playing) {runtime::SceneAnimatorGraphs::LayerSettings out;auto &graphs=playScene_.animatorGraphs();graphs.layerControl(playScene_.world(),state_.animatorEntity,state_.animatorInstance,layerIndex,4,0,{},out);graphs.layerControl(playScene_.world(),state_.animatorEntity,state_.animatorInstance,layerIndex,3,0,{},out);}
      else if(!readOnly) editAnimator([&](scene::Animator &edit){auto &l=edit.layers[layerIndex];l.referenceClip={};l.referenceTime=0;return true;});
      return true;
    }
    case w::LayerRuntimeReset: {runtime::SceneAnimatorGraphs::LayerSettings out;if(playing) playScene_.animatorGraphs().layerControl(playScene_.world(),state_.animatorEntity,state_.animatorInstance,layerIndex,5,0,{},out);return true;}
    case w::LayerMask: if(!isPlaying()&&!playMirrorOpen_) {state_.animatorPicker=0x10000u;state_.animatorPickerPage=0;}return true;
    case w::AddState: {
      u64 created=0;
      editAnimator([&](scene::Animator &edit){
        auto &l=edit.layers[layerIndex];if(l.states.size()>=scene::Animator::MaximumStates) return false;
        std::vector<std::string> names;for(const auto &s:l.states) names.push_back(s.name);
        scene::AnimatorState s;s.id=edit.allocateId();s.name=uniqueName(names,"Estado");
        // Nasce no centro da vista; se cair sobre outro estado, desce até achar lugar.
        const auto &c=layout_.animatorCanvas;
        if(c.width>0) {view::toGraph(v,{c.x+c.width*.5f,c.y+c.height*.5f},s.x,s.y);s.x-=view::NodeWidth*.5f;s.y-=view::NodeHeight*.5f;}
        else view::freeSpot(l,s.x,s.y);
        s.x=std::round(s.x/16)*16;s.y=std::round(s.y/16)*16;
        for(u32 tries=0;tries<20;++tries) {
          bool overlaps=false;
          for(const auto &o:l.states) overlaps=overlaps||(std::abs(o.x-s.x)<view::NodeWidth+16&&std::abs(o.y-s.y)<view::NodeHeight+16);
          if(!overlaps) break;
          s.y+=view::NodeHeight+32;
        }
        if(!l.defaultState) l.defaultState=s.id;
        created=s.id;l.states.push_back(s);return true;});
      if(created) {state_.animatorState=created;state_.animatorTransition=0;}
      return true;
    }
    case w::SetDefault: editAnimator([&](scene::Animator &edit){auto &l=edit.layers[layerIndex];if(!l.state(state_.animatorState)) return false;l.defaultState=state_.animatorState;return true;});return true;
    case w::Delete:
      if(state_.animatorState) {
        const u64 doomed=state_.animatorState;
        if(editAnimator([&](scene::Animator &edit){
             auto &l=edit.layers[layerIndex];
             std::erase_if(l.states,[&](const scene::AnimatorState &s){return s.id==doomed;});
             std::erase_if(l.transitions,[&](const scene::AnimatorTransition &t){return t.from==doomed||t.to==doomed;});
             if(l.defaultState==doomed) l.defaultState=l.states.empty()?0:l.states.front().id;
             return true;})) state_.animatorState=0;
      } else if(state_.animatorTransition) {
        const u64 doomed=state_.animatorTransition;
        if(editAnimator([&](scene::Animator &edit){std::erase_if(edit.layers[layerIndex].transitions,[&](const scene::AnimatorTransition &t){return t.id==doomed;});return true;}))
          state_.animatorTransition=0;
      }
      return true;
    default: break;
  }
  if(code>=w::LayerTab&&code<w::LayerTab+scene::Animator::MaximumLayers) {
    state_.animatorLayer=code-w::LayerTab;state_.animatorState=state_.animatorTransition=0;state_.animatorConnecting=false;frameAnimator();return true;
  }

  // ---- Parâmetros ----------------------------------------------------------
  if(code>=w::ParameterAdd&&code<w::ParameterAdd+4) {
    const auto type=static_cast<T>(code-w::ParameterAdd);
    static constexpr const char *bases[]{"Float","Int","Bool","Gatilho"};
    u64 created=0;
    editAnimator([&](scene::Animator &edit){
      if(edit.parameters.size()>=scene::Animator::MaximumParameters) return false;
      std::vector<std::string> names;for(const auto &p:edit.parameters) names.push_back(p.name);
      edit.parameters.push_back({edit.allocateId(),uniqueName(names,bases[static_cast<u32>(type)]),type,0});created=edit.parameters.back().id;return true;});
    // O campo de nome abre com o nome sugerido: confirmar mantém, digitar troca.
    if(const auto *now=openAnimator();created&&now) {
      state_.animatorParameter=created;
      beginAnimatorName(w::ParameterName+static_cast<u32>(now->parameters.size()-1),now->parameters.back().name);
    }
    return true;
  }
  if(code>=w::ParameterRow&&code<w::ParameterRow+0x100) {const u32 i=code-w::ParameterRow;if(i<a->parameters.size()) {
    state_.animatorParameter=a->parameters[i].id;state_.animatorState=state_.animatorTransition=0;state_.animatorDrawer=2;state_.animatorDetailsScroll=0;
  }return true;}
  if(code>=w::ParameterName&&code<w::ParameterName+0x100) {const u32 i=code-w::ParameterName;if(i<a->parameters.size()&&!readOnly) beginAnimatorName(code,a->parameters[i].name);return true;}
  if(code>=w::ParameterValue&&code<w::ParameterValue+0x100) {
    const u32 i=code-w::ParameterValue;if(i>=a->parameters.size()) return true;
    const auto &p=a->parameters[i];
    if(p.source!=scene::AnimatorParameterSource::Manual) return true;
    if(readOnly) {
      float value=0;playScene_.animatorGraphs().parameter(playScene_.world(),state_.animatorEntity,state_.animatorInstance,p.name,
        runtime::SceneAnimatorGraphs::ParameterOperation::Get,0,value);
      if(p.type==T::Bool) applyAnimatorNumber(code,value==0?1:0);else if(p.type!=T::Trigger) beginAnimatorNumber(code,value);
    } else if(p.type==T::Bool) editAnimator([&](scene::Animator &edit){edit.parameters[i].value=edit.parameters[i].value!=0?0.f:1.f;return true;});
    else if(p.type!=T::Trigger) beginAnimatorNumber(code,p.value);
    return true;
  }
  if(code>=w::ParameterDelete&&code<w::ParameterDelete+0x100) {
    const u32 i=code-w::ParameterDelete;if(i>=a->parameters.size()) return true;
    const u64 doomed=a->parameters[i].id;
    editAnimator([&](scene::Animator &edit){
      std::erase_if(edit.parameters,[&](const scene::AnimatorParameter &p){return p.id==doomed;});
      for(auto &l:edit.layers) {
        for(auto &s:l.states) {if(s.blendX==doomed) s.blendX=0;if(s.blendY==doomed) s.blendY=0;if(s.speedParameter==doomed) s.speedParameter=0;}
        for(auto &t:l.transitions) std::erase_if(t.conditions,[&](const scene::AnimatorCondition &c){return c.parameter==doomed;});
      }
      return true;});
    state_.animatorParameter=0;return true;
  }

  // ---- Estado selecionado --------------------------------------------------
  if(const auto *s=layer.state(state_.animatorState);s&&!readOnly) {
    if(code==w::StateName) {beginAnimatorName(code,s->name);return true;}
    if(code>=w::StateKind&&code<w::StateKind+3) {
      const auto kind=static_cast<scene::AnimatorMotionKind>(code-w::StateKind);
      editAnimator([&](scene::Animator &edit){
        auto *x=edit.layers[layerIndex].state(s->id);x->kind=kind;
        if(kind!=scene::AnimatorMotionKind::Clip) {
          if(!x->blendX) x->blendX=cycleParameter(edit,0,false);
          if(kind==scene::AnimatorMotionKind::Blend2D&&!x->blendY) x->blendY=cycleParameter(edit,x->blendX,false);
          for(usize m=0;m<x->motions.size();++m) if(kind==scene::AnimatorMotionKind::Blend1D) x->motions[m].threshold=static_cast<float>(m);
        }
        return true;});
      if(code!=w::StateKind&&cycleParameter(*a,0,false)==0) state_.status="Crie um parâmetro Float para guiar a mistura";
      return true;
    }
    if(code==w::StateBlendX||code==w::StateBlendY) {
      editAnimator([&](scene::Animator &edit){auto *x=edit.layers[layerIndex].state(s->id);auto &slot=code==w::StateBlendX?x->blendX:x->blendY;slot=cycleParameter(edit,slot,false);return true;});
      if(!cycleParameter(*a,0,false)) state_.status="Crie um parâmetro Float para guiar a mistura";
      return true;
    }
    if(code==w::StateSpeed) {beginAnimatorNumber(code,s->speed);return true;}
    if(code==w::StateSpeedParameter) {editAnimator([&](scene::Animator &edit){auto *x=edit.layers[layerIndex].state(s->id);x->speedParameter=cycleParameter(edit,x->speedParameter,true);return true;});return true;}
    if(code==w::StateLoop) {editAnimator([&](scene::Animator &edit){auto *x=edit.layers[layerIndex].state(s->id);x->loop=!x->loop;return true;});return true;}
    if(code==w::MotionAdd) {
      editAnimator([&](scene::Animator &edit){
        auto *x=edit.layers[layerIndex].state(s->id);if(x->motions.size()>=scene::Animator::MaximumMotions) return false;
        scene::AnimatorMotion m;const usize n=x->motions.size();
        m.threshold=n?x->motions.back().threshold+1:0;
        // Mistura 2D: o novo ponto entra num círculo ao redor do centro.
        const float angle=static_cast<float>(n)*1.2566370614f;m.x=n?std::sin(angle):0;m.y=n?std::cos(angle):0;
        x->motions.push_back(m);return true;});
      state_.animatorPicker=static_cast<u32>(s->motions.size())+1;state_.animatorPickerPage=0;return true;
    }
    if(code>=w::MotionClip&&code<w::MotionClip+0x100) {state_.animatorPicker=code-w::MotionClip+1;state_.animatorPickerPage=0;return true;}
    if(code>=w::MotionThreshold&&code<w::MotionThreshold+0x100) {const u32 m=code-w::MotionThreshold;if(m<s->motions.size()) beginAnimatorNumber(code,s->motions[m].threshold);return true;}
    if(code>=w::MotionX&&code<w::MotionX+0x100) {const u32 m=code-w::MotionX;if(m<s->motions.size()) beginAnimatorNumber(code,s->motions[m].x);return true;}
    if(code>=w::MotionY&&code<w::MotionY+0x100) {const u32 m=code-w::MotionY;if(m<s->motions.size()) beginAnimatorNumber(code,s->motions[m].y);return true;}
    if(code>=w::MotionRemove&&code<w::MotionRemove+0x100) {const u32 m=code-w::MotionRemove;editAnimator([&](scene::Animator &edit){auto *x=edit.layers[layerIndex].state(s->id);if(m>=x->motions.size()) return false;x->motions.erase(x->motions.begin()+m);return true;});return true;}
    if(code==w::EventAdd) {
      editAnimator([&](scene::Animator &edit){auto *x=edit.layers[layerIndex].state(s->id);if(x->events.size()>=scene::Animator::MaximumEvents) return false;
        u32 tag=1;for(const auto &e:x->events) tag=std::max(tag,e.tag+1);x->events.push_back({.5f,tag});return true;});
      return true;
    }
    if(code>=w::EventTime&&code<w::EventTime+0x100) {const u32 e=code-w::EventTime;if(e<s->events.size()) beginAnimatorNumber(code,s->events[e].time);return true;}
    if(code>=w::EventTag&&code<w::EventTag+0x100) {const u32 e=code-w::EventTag;if(e<s->events.size()) beginAnimatorNumber(code,s->events[e].tag);return true;}
    if(code>=w::EventRemove&&code<w::EventRemove+0x100) {const u32 e=code-w::EventRemove;editAnimator([&](scene::Animator &edit){auto *x=edit.layers[layerIndex].state(s->id);if(e>=x->events.size()) return false;x->events.erase(x->events.begin()+e);return true;});return true;}
  }

  // ---- Transição selecionada -----------------------------------------------
  const scene::AnimatorTransition *t=nullptr;for(const auto &x:layer.transitions) if(x.id==state_.animatorTransition) t=&x;
  if(t&&!readOnly) {
    const auto transition=[&](scene::Animator &edit)->scene::AnimatorTransition*{for(auto &x:edit.layers[layerIndex].transitions) if(x.id==t->id) return &x;return nullptr;};
    if(code==w::TransitionExit) {editAnimator([&](scene::Animator &edit){auto *x=transition(edit);x->hasExitTime=!x->hasExitTime;return true;});return true;}
    if(code==w::TransitionExitTime) {beginAnimatorNumber(code,t->exitTime);return true;}
    if(code==w::TransitionDuration) {beginAnimatorNumber(code,t->duration);return true;}
    if(code==w::ConditionAdd) {
      editAnimator([&](scene::Animator &edit){
        auto *x=transition(edit);if(x->conditions.size()>=scene::Animator::MaximumConditions||edit.parameters.empty()) return false;
        const auto &p=edit.parameters.front();x->conditions.push_back({p.id,modesFor(p.type).front(),0});return true;});
      return true;
    }
    if(code>=w::ConditionParameter&&code<w::ConditionParameter+0x100) {
      const u32 c=code-w::ConditionParameter;
      editAnimator([&](scene::Animator &edit){
        auto *x=transition(edit);if(c>=x->conditions.size()||edit.parameters.empty()) return false;
        auto &cond=x->conditions[c];usize i=0;while(i<edit.parameters.size()&&edit.parameters[i].id!=cond.parameter) ++i;
        const auto &next=edit.parameters[(i+1)%edit.parameters.size()];
        cond.parameter=next.id;const auto modes=modesFor(next.type);
        if(std::find(modes.begin(),modes.end(),cond.mode)==modes.end()) cond.mode=modes.front();
        return true;});
      return true;
    }
    if(code>=w::ConditionMode&&code<w::ConditionMode+0x100) {
      const u32 c=code-w::ConditionMode;
      editAnimator([&](scene::Animator &edit){
        auto *x=transition(edit);if(c>=x->conditions.size()) return false;auto &cond=x->conditions[c];
        const auto *p=edit.parameter(cond.parameter);if(!p) return false;
        const auto modes=modesFor(p->type);const auto f=std::find(modes.begin(),modes.end(),cond.mode);
        cond.mode=f==modes.end()||f+1==modes.end()?modes.front():*(f+1);return true;});
      return true;
    }
    if(code>=w::ConditionThreshold&&code<w::ConditionThreshold+0x100) {const u32 c=code-w::ConditionThreshold;if(c<t->conditions.size()) beginAnimatorNumber(code,t->conditions[c].threshold);return true;}
    if(code>=w::ConditionRemove&&code<w::ConditionRemove+0x100) {
      const u32 c=code-w::ConditionRemove;
      editAnimator([&](scene::Animator &edit){auto *x=transition(edit);if(c>=x->conditions.size()) return false;x->conditions.erase(x->conditions.begin()+c);return true;});
      return true;
    }
  }
  return true;
}

} // namespace ae::editor
