#include "editor/editor_session.h"
#include "runtime/animation_property_binding.h"
#include "scene/animator.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <limits>

namespace ae::editor {
namespace {
using namespace resources;namespace w=clip_widget;
constexpr const char *operations[]={"Entrada","Constante","Somar","Multiplicar","Escala + offset","Clamp","Remapear","Curva"};
AnimationDriverNode *node(AnimationClipAsset &draft,u64 id) {
  for(auto &n:draft.drivers.graph.nodes)if(n.id==id)return &n;
  return nullptr;
}
std::string number(double value){char text[64];std::snprintf(text,sizeof text,"%.5g",value);return text;}
}
void EditorSession::refreshAnimationDrivers() {
  if(!state_.clipDriversShown||!clipDriverDraft_)return;
  state_.clipDriverDraft=&*clipDriverDraft_;state_.clipDriverEntries.clear();state_.clipDriverInspection.clear();
  const auto target=state_.clipTargetNode;
  if(state_.clipDriverRoute==3||state_.clipDriverRoute==4) {
    std::string error;
    if(state_.clipDriverRoute==3) {
      EditorScreenState::ClipDriverEntry time;time.label="Tempo local do clipe · segundos";state_.clipDriverEntries.push_back(time);
      if(const auto *object=document_.find(target))for(usize i=0;i<object->components.size();++i) {
        const auto &component=*object->components.at(i);if(&component.type()!=&scene::Animator::descriptor)continue;
        const auto &animator=static_cast<const scene::Animator&>(component);scene::Animator resolved;
        if(!resolveAnimatorController(animator,animatorControllers_,resolved,error))continue;
        for(const auto &parameter:resolved.parameters)if(parameter.type==scene::AnimatorParameterType::Float) {
          EditorScreenState::ClipDriverEntry entry;entry.target=target;entry.label="Animator · "+parameter.name;
          entry.input.kind=AnimationDriverInputKind::AnimatorParameter;entry.input.parameter=parameter.id;entry.input.controller=animator.controller;
          auto &a=entry.input.address;a.component=component.instanceId();a.version=component.type().version;a.type=component.type().id;a.property="parameter";
          state_.clipDriverEntries.push_back(std::move(entry));
        }
      }
    }
    for(const auto &property:animationPropertyCatalog(target,error))if(property.address.kind==AnimationPropertyKind::Number) {
      EditorScreenState::ClipDriverEntry entry;entry.target=target;entry.label=property.label;
      entry.input.kind=AnimationDriverInputKind::Property;entry.input.address=property.address;state_.clipDriverEntries.push_back(std::move(entry));
    }
    if(state_.clipDriverRoute==4&&state_.clipDriverEntries.empty())state_.clipDriverInspection="Selecione um objeto com propriedade numérica efetiva";
  }
  const auto *selected=node(*clipDriverDraft_,state_.clipDriverNode);if(!selected)return;
  if(state_.clipDriverRoute==1) {
    // A selected intermediate node is inspected with the same math compiler,
    // observing the current composed preview without publishing the draft.
    auto graph=clipDriverDraft_->drivers.graph;graph.outputs={selected->id};CompiledAnimationDrivers compiled;std::string error;
    if(!compileAnimationDrivers(graph,compiled,error)){state_.clipDriverInspection=error;return;}
    runtime::SourceAnimations source;
    for(const auto &binding:clipDriverDraft_->bindings){source.nodes.push_back(binding.sourceNode);source.nodeNames.push_back(binding.name);source.nodePaths.push_back(binding.path);}
    std::vector<runtime::ObjectId> targets;runtime::resolveAnimationTargets(document_,state_.clipOwner,source,targets);
    std::vector<double> sources(clipDriverDraft_->drivers.inputs.size()),scratch(compiled.scratchSize()),output(1);
    for(usize i=0;i<sources.size();++i) {
      const auto &input=clipDriverDraft_->drivers.inputs[i];if(input.kind==AnimationDriverInputKind::ClipTime){sources[i]=state_.clipTime;continue;}
      const auto found=std::find_if(clipDriverDraft_->bindings.begin(),clipDriverDraft_->bindings.end(),[&](const auto &b){return b.id==input.binding;});
      if(found==clipDriverDraft_->bindings.end()){error="Binding de entrada ausente";break;}
      const auto index=usize(found-clipDriverDraft_->bindings.begin());if(index>=targets.size()){error="Objeto de entrada ausente";break;}
      if(input.kind==AnimationDriverInputKind::AnimatorParameter) {
        if(!animationDriverParameter(targets[index],input.address.component,input.parameter,input.controller,sources[i],error))break;
      } else {
        AnimationPropertyValue value;
        if(!clipPreview_.readProperty(targets[index],input.address,value,error))break;
        const auto *v=std::get_if<float>(&value);if(!v){error="Entrada sem valor numérico";break;}sources[i]=*v;
      }
    }
    if(error.empty()&&compiled.evaluate(sources,scratch,output,error)) {
      state_.clipDriverInspection="Resultado · "+number(output.front());
      if(selected->operation==AnimationDriverOp::Source&&selected->source<sources.size())state_.clipDriverInspection="Entrada · "+number(sources[selected->source])+"  →  "+number(output.front());
    } else state_.clipDriverInspection=error;
  }
}
bool EditorSession::handleAnimationDriverWidget(u32 code) {
  if(code<w::Drivers||code>=w::DriverEnd)return false;
  const auto *asset=animationClipAsset(state_.clipGuid);if(!asset)return true;std::string error;
  if(code==w::Drivers) {
    if(!clipDriverDraft_||clipDriverDraft_->guid!=asset->guid||clipDriverDraft_->revision!=asset->revision)clipDriverDraft_=*asset;
    state_.clipDriversShown=true;state_.clipDriverRoute=0;state_.clipDriverPage=0;
    state_.clipPropertiesShown=state_.clipEditPicker=state_.clipRegionsShown=state_.clipRangeShown=state_.clipLayersShown=state_.clipBakeShown=false;
    state_.clipPoseToolsShown=state_.clipPoseShown=state_.clipCurveView=state_.clipTangentPicker=false;state_.clipAuthoringPicker=0;state_.clipPlaying=false;
    if(!document_.exists(state_.clipTargetNode))state_.clipTargetNode=state_.clipOwner;
  } else if(!clipDriverDraft_)return true;
  auto &draft=*clipDriverDraft_;auto *selected=node(draft,state_.clipDriverNode);
  if(code==w::DriverCancel) {clipDriverDraft_.reset();state_.clipDriverDraft=nullptr;state_.clipDriversShown=false;return true;}
  if(code==w::DriverApply) {
    const auto guid=draft.guid;const auto revision=draft.revision;
    if(editAnimationClipAsset(guid,revision,[&](auto &candidate){
      auto value=draft;
      if(!configureAnimationDrivers(value,state_.clipOwner,value.drivers,error))return false;
      candidate=std::move(value);return true;
    },error)) {clipDriverDraft_.reset();state_.clipDriverDraft=nullptr;state_.clipDriversShown=false;refreshAnimationClip();}
  } else if(code==w::DriverBack) {state_.clipDriverRoute=0;state_.clipDriverPage=0;}
  else if(code==w::DriverAdd) {state_.clipDriverRoute=2;state_.clipDriverPage=0;}
  else if(code==w::DriverPrevious) {if(state_.clipDriverPage)--state_.clipDriverPage;}
  else if(code==w::DriverNext)++state_.clipDriverPage;
  else if(code==w::DriverInput&&selected) {state_.clipDriverRoute=3;state_.clipDriverPage=0;}
  else if(code==w::DriverOutput&&selected) {state_.clipDriverRoute=4;state_.clipDriverPage=0;}
  else if(code==w::DriverCurve&&selected&&selected->operation==AnimationDriverOp::Curve) {state_.clipDriverRoute=5;state_.clipDriverPage=0;state_.clipDriverCurveKey=0;}
  else if(code==w::DriverFields) {
    if(state_.clipDriverRoute==5)state_.clipDriverCurveMode=(state_.clipDriverCurveMode+1)%3;
    else state_.clipDriverFields=!state_.clipDriverFields;
  }
  else if(code==w::DriverDelete&&selected) {
    const u64 id=selected->id;const auto op=selected->operation;const u32 source=selected->source;
    for(usize i=draft.drivers.graph.outputs.size();i-->0;)if(draft.drivers.graph.outputs[i]==id) {
      draft.drivers.graph.outputs.erase(draft.drivers.graph.outputs.begin()+i);draft.drivers.targets.erase(draft.drivers.targets.begin()+i);
    }
    std::erase_if(draft.drivers.graph.nodes,[&](const auto &n){return n.id==id;});
    if(op==AnimationDriverOp::Source) {
      draft.drivers.inputs.erase(draft.drivers.inputs.begin()+source);--draft.drivers.graph.sources;
      for(auto &n:draft.drivers.graph.nodes)if(n.operation==AnimationDriverOp::Source&&n.source>source)--n.source;
    }
    state_.clipDriverNode=0;state_.clipDriverRoute=0;state_.clipDriverPage=0;
  } else if(code==w::DriverClearOutput&&selected) {
    for(usize i=draft.drivers.graph.outputs.size();i-->0;)if(draft.drivers.graph.outputs[i]==selected->id) {
      draft.drivers.graph.outputs.erase(draft.drivers.graph.outputs.begin()+i);draft.drivers.targets.erase(draft.drivers.targets.begin()+i);
    }
  } else if(code==w::DriverFirst||code==w::DriverSecond) {
    if(selected) {
      auto &id=code==w::DriverFirst?selected->first:selected->second;
      std::vector<u64> available;for(const auto &n:draft.drivers.graph.nodes)if(n.id!=selected->id)available.push_back(n.id);
      if(!available.empty()){const auto at=std::find(available.begin(),available.end(),id);id=at==available.end()||at+1==available.end()?available.front():*(at+1);}
    }
  } else if(code>=w::DriverConstant&&code<w::DriverConstant+4&&selected) {
    const double values[]={selected->a,selected->b,selected->c,selected->d};clipDriverNumberNode_=selected->id;clipDriverNumberRevision_=draft.revision;
    beginAnimationClipNumber(code,values[code-w::DriverConstant]);return true;
  } else if(code>=w::DriverOperation&&code<w::DriverOperation+8) {
    if(draft.drivers.graph.nodes.size()>=AnimationDriverGraph::MaximumNodes)error="Orçamento de nodes de driver atingido";
    else {
      AnimationDriverNode n;n.id=draft.nextId++;n.operation=static_cast<AnimationDriverOp>(code-w::DriverOperation);
      if(n.operation==AnimationDriverOp::Source) {
        if(draft.drivers.inputs.size()>=AnimationDriverGraph::MaximumSources)error="Orçamento de entradas atingido";
        else {n.source=draft.drivers.graph.sources++;draft.drivers.inputs.push_back({});}
      } else if(n.operation!=AnimationDriverOp::Constant) {
        n.first=draft.drivers.graph.nodes.empty()?0:draft.drivers.graph.nodes.back().id;
        if(n.operation==AnimationDriverOp::Add||n.operation==AnimationDriverOp::Multiply)n.second=n.first;
      }
      if(n.operation==AnimationDriverOp::ScaleBias){n.a=1;n.b=0;}
      if(n.operation==AnimationDriverOp::Curve)for(u32 i=0;i<2;++i) {AnimationCurveKey k;k.id=draft.nextId++;k.time=k.value=float(i);k.incoming=k.outgoing=AnimationTangentMode::Linear;n.curve.keys.push_back(k);}
      if(error.empty()){draft.drivers.graph.nodes.push_back(n);state_.clipDriverNode=n.id;state_.clipDriverRoute=1;state_.clipDriverFields=false;}
    }
  } else if(code>=w::DriverEntry&&code<w::DriverEntry+2) {
    const usize index=state_.clipDriverPage*2+code-w::DriverEntry;
    if(state_.clipDriverRoute==0&&index<draft.drivers.graph.nodes.size()){state_.clipDriverNode=draft.drivers.graph.nodes[index].id;state_.clipDriverRoute=1;state_.clipDriverFields=false;}
    else if((state_.clipDriverRoute==3||state_.clipDriverRoute==4)&&selected&&index<state_.clipDriverEntries.size()) {
      auto entry=state_.clipDriverEntries[index];
      if(entry.input.kind!=AnimationDriverInputKind::ClipTime&&!animationPropertyBinding(draft,state_.clipOwner,entry.target,entry.input.binding,error)) {}
      else if(state_.clipDriverRoute==3&&selected->operation==AnimationDriverOp::Source) {draft.drivers.inputs[selected->source]=entry.input;state_.clipDriverRoute=1;}
      else if(state_.clipDriverRoute==4) {
        auto &targets=draft.drivers.targets;const auto found=std::find_if(targets.begin(),targets.end(),[&](const auto &t){return t.binding==entry.input.binding&&t.address==entry.input.address;});
        if(found!=targets.end())draft.drivers.graph.outputs[found-targets.begin()]=selected->id;
        else if(targets.size()<AnimationDriverGraph::MaximumOutputs) {targets.push_back({entry.input.binding,entry.input.address});draft.drivers.graph.outputs.push_back(selected->id);}
        else error="Orçamento de saídas atingido";
        if(error.empty())state_.clipDriverRoute=1;
      }
    }
  } else if(selected&&state_.clipDriverRoute==5) {
    auto &keys=selected->curve.keys;
    if(code==w::DriverCurvePrevious&&state_.clipDriverCurveKey)--state_.clipDriverCurveKey;
    else if(code==w::DriverCurveNext&&state_.clipDriverCurveKey+1<keys.size())++state_.clipDriverCurveKey;
    else if(code==w::DriverCurveAdd) {
      AnimationCurveKey k;k.id=0;k.time=keys.empty()?0:keys.back().time+1;k.value=keys.empty()?0:keys.back().value;
      u64 created=0;if(!putAnimationCurveKey(selected->curve,k,draft.nextId,created))error="Curva excede orçamento ou precisão";
      else state_.clipDriverCurveKey=u32(keys.size()-1);
    } else if(code==w::DriverCurveDelete&&keys.size()>1&&state_.clipDriverCurveKey<keys.size()) {keys.erase(keys.begin()+state_.clipDriverCurveKey);state_.clipDriverCurveKey=0;}
    else if(state_.clipDriverCurveKey<keys.size()) {
      auto &k=keys[state_.clipDriverCurveKey];
      if(code==w::DriverCurveTangent)k.incoming=k.outgoing=static_cast<AnimationTangentMode>((u32(k.outgoing)+1)%7);
      else if(code>=w::DriverCurveValue&&code<w::DriverCurveValue+6) {
        const double values[]={k.time,k.value,k.inSlope,k.outSlope,k.inWeight,k.outWeight};
        clipDriverNumberNode_=selected->id;clipDriverNumberKey_=k.id;clipDriverNumberRevision_=draft.revision;
        beginAnimationClipNumber(code,values[code-w::DriverCurveValue]);return true;
      } else if(code==w::DriverCurveWeighted)k.weightedIn=k.weightedOut=!(k.weightedIn&&k.weightedOut);
    }
  }
  state_.clipDiagnostic=error;refreshAnimationDrivers();appearanceChanged_=true;return true;
}
bool EditorSession::applyAnimationDriverNumber(u32 code,double value) {
  if(!clipDriverDraft_||clipDriverDraft_->revision!=clipDriverNumberRevision_||state_.clipDriverNode!=clipDriverNumberNode_||!std::isfinite(value))return false;
  auto *selected=node(*clipDriverDraft_,clipDriverNumberNode_);if(!selected)return false;
  if(code>=w::DriverConstant&&code<w::DriverConstant+4) {
    double *fields[]={&selected->a,&selected->b,&selected->c,&selected->d};*fields[code-w::DriverConstant]=value;
  } else if(code>=w::DriverCurveValue&&code<w::DriverCurveValue+6) {
    auto candidate=selected->curve;auto found=std::find_if(candidate.keys.begin(),candidate.keys.end(),[&](const auto &k){return k.id==clipDriverNumberKey_;});
    if(found==candidate.keys.end()||std::abs(value)>std::numeric_limits<float>::max())return false;
    float *fields[]={&found->time,&found->value,&found->inSlope,&found->outSlope,&found->inWeight,&found->outWeight};*fields[code-w::DriverCurveValue]=float(value);
    std::sort(candidate.keys.begin(),candidate.keys.end(),[](const auto &a,const auto &b){return a.time<b.time;});
    if(!validAnimationCurve(candidate)){state_.clipDiagnostic="Curva inválida: tempo, tangente ou peso";return false;}
    selected->curve=std::move(candidate);
  } else return false;
  refreshAnimationDrivers();return true;
}
}
