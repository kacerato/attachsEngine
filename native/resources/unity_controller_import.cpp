#include "resources/unity_controller_import.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace ae::resources {
namespace {
using Node=UnityYamlNode;
bool fail(std::string &error,const std::string &message){error=message;return false;}
bool text(const Node &node,const char *key,std::string &out) {
  const auto *value=node.get(key);if(!value||value->kind!=Node::Kind::Scalar)return false;out=value->scalar;return true;
}
bool integer(const Node &node,const char *key,i64 &out,i64 fallback=0) {const auto *value=node.get(key);if(!value){out=fallback;return true;}return value->integer(out);}
bool number(const Node &node,const char *key,float &out,float fallback=0) {
  const auto *value=node.get(key);double result=fallback;
  if(value&&!value->number(result))return false;
  out=static_cast<float>(result);return std::isfinite(out);
}
bool flag(const Node &node,const char *key,bool &out,bool fallback=false) {const auto *value=node.get(key);if(!value){out=fallback;return true;}return value->boolean(out);}
bool active(const Node &node,const char *key) {bool result=false;return flag(node,key,result)&&result;}
bool validFlags(const Node &node) {
  for(const auto &[name,value]:node.members) {
    if(name=="m_IKPass"||name=="m_IKOnFeet"||name=="m_Mirror"||name=="m_MirrorParameterActive"||name=="m_CycleOffsetParameterActive"||
       name=="m_TimeParameterActive"||name=="m_SpeedParameterActive"||name=="m_IsExit"||name=="m_Mute"||name=="m_Solo") {
      bool parsed=false;if(!value.boolean(parsed))return false;
    }
    if(!validFlags(value))return false;
  }
  for(const auto &value:node.elements)if(!validFlags(value))return false;
  return true;
}
bool nonempty(const Node &node,const char *key){const auto *value=node.get(key);return value&&!value->empty();}
bool reference(const Node &node,const char *key,UnityObjectReference &out) {const auto *value=node.get(key);return value&&unityObjectReference(*value,out);}
const Node *sequence(const Node &node,const char *key) {const auto *value=node.get(key);return value&&value->kind==Node::Kind::Sequence?value:nullptr;}
}
bool convertUnityController(const UnityYamlDocument &document,AssetGuid nativeGuid,
    const UnityControllerClipResolver &resolve,UnityControllerImport &out,std::string &diagnostic) {
  const UnityYamlObject *source=nullptr;
  for(const auto &object:document.objects) {
    if(!validFlags(object.value))return fail(diagnostic,"Flag de controller Unity inválida.");
    const char *expected=object.classId==91?"AnimatorController":object.classId==1101?"AnimatorStateTransition":object.classId==1102?"AnimatorState":object.classId==1107?"AnimatorStateMachine":object.classId==206?"BlendTree":"";
    if(object.type!=expected)return fail(diagnostic,"ClassID e tipo textual do controller Unity divergem.");
    if(object.stripped)return fail(diagnostic,"Controller Unity contém objeto stripped não resolvido.");
    if(object.classId==91){if(source)return fail(diagnostic,"Mais de um AnimatorController Unity no documento.");source=&object;}
    else if(object.classId!=1101&&object.classId!=1102&&object.classId!=1107&&object.classId!=206)
      return fail(diagnostic,"Controller Unity contém comportamento sem equivalente: "+object.type+" · classID "+std::to_string(object.classId));
  }
  if(!source||source->type!="AnimatorController"||!resolve)return fail(diagnostic,"AnimatorController ou resolvedor de clipes ausente.");
  UnityControllerImport candidate;auto &asset=candidate.controller;asset.guid=nativeGuid;
  if(!text(source->value,"m_Name",asset.name))return fail(diagnostic,"Controller Unity sem nome.");
  auto &graph=asset.graph;graph.layers.clear();graph.parameters.clear();
  std::map<i64,u64> ids;
  for(const auto &object:document.objects)if(object.classId!=91) {
    const auto id=graph.allocateId();if(!id)return fail(diagnostic,"Alocador de controller esgotado.");
    ids.emplace(object.fileId,id);candidate.identities.emplace_back(object.fileId,id);
  }
  const auto *parameters=sequence(source->value,"m_AnimatorParameters");
  if(!parameters||parameters->elements.size()>scene::Animator::MaximumParameters)return fail(diagnostic,"Parâmetros de controller Unity inválidos.");
  for(const auto &parameter:parameters->elements) {
    scene::AnimatorParameter value;value.id=graph.allocateId();i64 type=0,integerDefault=0;bool booleanDefault=false;
    if(!text(parameter,"m_Name",value.name)||!integer(parameter,"m_Type",type)||!number(parameter,"m_DefaultFloat",value.value)||
       !integer(parameter,"m_DefaultInt",integerDefault)||!flag(parameter,"m_DefaultBool",booleanDefault))return fail(diagnostic,"Parâmetro Unity inválido.");
    if(type==1)value.type=scene::AnimatorParameterType::Float;
    else if(type==3){value.type=scene::AnimatorParameterType::Int;value.value=static_cast<float>(integerDefault);if(double(value.value)!=double(integerDefault))return fail(diagnostic,"Default Int Unity não cabe exatamente no parâmetro nativo.");}
    else if(type==4){value.type=scene::AnimatorParameterType::Bool;value.value=booleanDefault?1.f:0.f;}
    else if(type==9){value.type=scene::AnimatorParameterType::Trigger;value.value=0;}
    else return fail(diagnostic,"Tipo de parâmetro Unity não suportado: "+std::to_string(type));
    graph.parameters.push_back(std::move(value));
  }
  const auto parameter=[&](const std::string &name)->u64 {for(const auto &value:graph.parameters)if(value.name==name)return value.id;return 0;};
  std::set<i64> adoptedStates,adoptedTransitions,adoptedMachines,adoptedTrees;
  const auto *layers=sequence(source->value,"m_AnimatorLayers");
  std::set<std::string> layerNames;
  if(!layers||layers->elements.empty()||layers->elements.size()>scene::Animator::MaximumLayers)return fail(diagnostic,"Camadas Unity ausentes ou acima do limite.");
  for(const auto &layer:layers->elements) {
    scene::AnimatorLayer native;native.id=graph.allocateId();UnityObjectReference machine,mask;i64 blend=0,sync=-1;
    if(!text(layer,"m_Name",native.name)||!reference(layer,"m_StateMachine",machine)||machine.external()||!machine.fileId||
       !reference(layer,"m_Mask",mask)||mask.fileId||mask.external()||!integer(layer,"m_BlendingMode",blend)||blend<0||blend>1||
       !integer(layer,"m_SyncedLayerIndex",sync,-1)||sync!=-1||active(layer,"m_IKPass")||nonempty(layer,"m_Behaviours")||nonempty(layer,"m_Motions"))
      return fail(diagnostic,"Camada Unity exige máscara/IK/sincronização/behaviour sem conversão equivalente.");
    if(blend==1)return fail(diagnostic,"Camada aditiva Unity exige pose de referência explícita, ausente no formato desta camada.");
    if(!layerNames.insert(native.name).second)return fail(diagnostic,"Nomes de camada Unity duplicados impedem identidade autoral estável.");
    if(!number(layer,"m_DefaultWeight",native.weight,1))return fail(diagnostic,"Peso de camada Unity inválido.");
    // Unity ignores the serialized default weight on the base layer.
    if(graph.layers.empty())native.weight=1;
    const auto *stateMachine=document.find(machine.fileId);
    if(!stateMachine||stateMachine->classId!=1107||!adoptedMachines.insert(machine.fileId).second)return fail(diagnostic,"StateMachine Unity ausente ou compartilhada entre camadas.");
    const auto &sm=stateMachine->value;
    for(const char *field:{"m_ChildStateMachines","m_EntryTransitions","m_StateMachineTransitions","m_StateMachineBehaviours"})
      if(nonempty(sm,field))return fail(diagnostic,"StateMachine Unity hierárquica/Entry/behaviour exige conversão própria: "+std::string(field));
    const auto *states=sequence(sm,"m_ChildStates");
    if(!states||states->elements.empty())return fail(diagnostic,"StateMachine Unity sem estados.");
    std::map<i64,std::vector<i64>> transitions;
    const auto readTransitionRefs=[&](const Node &owner,const char *name,i64 from)->bool {
      const auto *entries=sequence(owner,name);if(!entries)return fail(diagnostic,"Lista de transições Unity inválida.");
      for(const auto &entry:entries->elements) {
        UnityObjectReference value;
        if(!unityObjectReference(entry,value)||value.external()||!value.fileId)
          return fail(diagnostic,"Transição Unity sem referência local válida.");
        transitions[from].push_back(value.fileId);
      }
      return true;
    };
    if(!readTransitionRefs(sm,"m_AnyStateTransitions",0))return false;
    for(const auto &child:states->elements) {
      UnityObjectReference stateRef;if(!reference(child,"m_State",stateRef)||stateRef.external())return fail(diagnostic,"Referência de estado Unity inválida.");
      const auto *object=document.find(stateRef.fileId);
      if(!object||object->classId!=1102||!adoptedStates.insert(object->fileId).second)return fail(diagnostic,"Estado Unity ausente ou compartilhado.");
      const auto &state=object->value;scene::AnimatorState value;value.id=ids.at(object->fileId);
      if(!text(state,"m_Name",value.name)||!number(state,"m_Speed",value.speed,1)||!readTransitionRefs(state,"m_Transitions",object->fileId))return fail(diagnostic,"Estado Unity inválido.");
      for(const char *flag:{"m_IKOnFeet","m_Mirror","m_MirrorParameterActive","m_CycleOffsetParameterActive","m_TimeParameterActive"})
        if(active(state,flag))return fail(diagnostic,"Estado Unity exige semântica não convertida: "+std::string(flag));
      bool defaults=true;
      if(!flag(state,"m_WriteDefaultValues",defaults,true)||!defaults)
        return fail(diagnostic,"Write Defaults desativado exige retenção de propriedades entre estados, distinta do baseline de pose nativo.");
      if(nonempty(state,"m_StateMachineBehaviours"))return fail(diagnostic,"StateMachineBehaviour Unity não é código executável Astra.");
      float offset=0;if(!number(state,"m_CycleOffset",offset)||offset!=0)return fail(diagnostic,"CycleOffset de estado exige remapeamento de tempo explícito.");
      if(active(state,"m_SpeedParameterActive")){std::string name;if(!text(state,"m_SpeedParameter",name)||!(value.speedParameter=parameter(name)))return fail(diagnostic,"Parâmetro de velocidade Unity ausente.");}
      UnityObjectReference motion;
      if(!reference(state,"m_Motion",motion)||!motion.fileId)return fail(diagnostic,"Estado Unity sem motion reproduzível.");
      if(motion.external()) {
        UnityControllerClip clip;if(!resolve(motion,clip,diagnostic)||!clip.guid.valid())return false;
        value.motions.push_back({clip.guid});value.loop=clip.loop;
      } else {
        const auto *tree=document.find(motion.fileId);i64 mode=0;
        if(!tree||tree->classId!=206||!adoptedTrees.insert(tree->fileId).second||!integer(tree->value,"m_BlendType",mode)||mode!=0)
          return fail(diagnostic,"BlendTree Unity exige algoritmo nativo equivalente; este conversor admite somente 1D não aninhado.");
        std::string name;if(!text(tree->value,"m_BlendParameter",name)||!(value.blendX=parameter(name)))return fail(diagnostic,"Parâmetro de BlendTree Unity ausente.");
        value.kind=scene::AnimatorMotionKind::Blend1D;value.loop=true;
        const auto *motions=sequence(tree->value,"m_Childs");if(!motions||motions->elements.size()<2)return fail(diagnostic,"BlendTree Unity 1D precisa de ao menos dois motions.");
        for(const auto &childMotion:motions->elements) {
          UnityObjectReference clipRef;UnityControllerClip clip;scene::AnimatorMotion nativeMotion;float scale=1,cycle=0;
          if(!reference(childMotion,"m_Motion",clipRef)||!clipRef.external()||!number(childMotion,"m_Threshold",nativeMotion.threshold)||
             !number(childMotion,"m_TimeScale",scale,1)||scale!=1||!number(childMotion,"m_CycleOffset",cycle)||cycle!=0||active(childMotion,"m_Mirror"))
            return fail(diagnostic,"Child motion Unity exige time-scale/mirror/bake sem representação neste BlendTree.");
          if(!resolve(clipRef,clip,diagnostic)||!clip.guid.valid())return false;
          if(!clip.loop)return fail(diagnostic,"BlendTree Unity mistura clips sem looping; ciclo nativo único não preserva esse contrato.");
          nativeMotion.clip=clip.guid;value.motions.push_back(nativeMotion);
        }
      }
      if(const auto *position=child.get("m_Position")) {double x=0,y=0;const auto *px=position->get("x"),*py=position->get("y");
        if(!px||!py||!px->number(x)||!py->number(y))return fail(diagnostic,"Posição editorial Unity inválida.");
        value.x=static_cast<float>(x);value.y=static_cast<float>(y);
      }
      native.states.push_back(std::move(value));
    }
    UnityObjectReference initial;if(!reference(sm,"m_DefaultState",initial)||initial.external()||!ids.contains(initial.fileId)||!native.state(ids.at(initial.fileId)))
      return fail(diagnostic,"Estado inicial Unity ausente nesta camada.");
    native.defaultState=ids.at(initial.fileId);
    for(const auto &[from,entries]:transitions)for(const auto identity:entries) {
      const auto *transition=document.find(identity);
      if(!transition||transition->classId!=1101||!adoptedTransitions.insert(identity).second)return fail(diagnostic,"Transição Unity ausente ou reutilizada.");
      const auto &node=transition->value;scene::AnimatorTransition value;UnityObjectReference destination,machineDestination;i64 interruption=0;
      if(!reference(node,"m_DstState",destination)||destination.external()||!ids.contains(destination.fileId)||!native.state(ids.at(destination.fileId))||
         !reference(node,"m_DstStateMachine",machineDestination)||machineDestination.fileId||machineDestination.external()||active(node,"m_IsExit")||
         active(node,"m_Mute")||active(node,"m_Solo"))return fail(diagnostic,"Transição Unity usa Exit/machine/mute/solo sem materialização equivalente.");
      value.id=ids.at(identity);value.from=from?ids.at(from):0;value.to=ids.at(destination.fileId);
      if(!flag(node,"m_HasExitTime",value.hasExitTime)||!number(node,"m_ExitTime",value.exitTime,1)||!number(node,"m_TransitionDuration",value.duration,.25f)||
         !number(node,"m_TransitionOffset",value.offset)||!flag(node,"m_HasFixedDuration",value.fixedDuration,true)||
         !flag(node,"m_CanTransitionToSelf",value.canTransitionToSelf)||!flag(node,"m_OrderedInterruption",value.orderedInterruption,true)||
         !integer(node,"m_InterruptionSource",interruption)||interruption<0||interruption>4)return fail(diagnostic,"Tempos/interrupção de transição Unity inválidos.");
      value.interruption=static_cast<scene::AnimatorInterruption>(interruption);
      const auto *conditions=sequence(node,"m_Conditions");if(!conditions)return fail(diagnostic,"Condições de transição Unity inválidas.");
      for(const auto &condition:conditions->elements) {
        scene::AnimatorCondition nativeCondition;std::string name;i64 mode=0;
        if(!text(condition,"m_ConditionEvent",name)||!(nativeCondition.parameter=parameter(name))||!integer(condition,"m_ConditionMode",mode)||
           !number(condition,"m_EventTreshold",nativeCondition.threshold))return fail(diagnostic,"Condição Unity sem parâmetro/tipo/limiar válido.");
        switch(mode){case 1:nativeCondition.mode=scene::AnimatorConditionMode::If;break;case 2:nativeCondition.mode=scene::AnimatorConditionMode::IfNot;break;
          case 3:nativeCondition.mode=scene::AnimatorConditionMode::Greater;break;case 4:nativeCondition.mode=scene::AnimatorConditionMode::Less;break;
          case 6:nativeCondition.mode=scene::AnimatorConditionMode::Equals;break;case 7:nativeCondition.mode=scene::AnimatorConditionMode::NotEqual;break;
          default:return fail(diagnostic,"Modo de condição Unity sem equivalente.");}
        value.conditions.push_back(nativeCondition);
      }
      native.transitions.push_back(std::move(value));
    }
    graph.layers.push_back(std::move(native));
  }
  for(const auto &object:document.objects)if((object.classId==1102&&!adoptedStates.contains(object.fileId))||
      (object.classId==1101&&!adoptedTransitions.contains(object.fileId))||(object.classId==1107&&!adoptedMachines.contains(object.fileId))||
      (object.classId==206&&!adoptedTrees.contains(object.fileId)))return fail(diagnostic,"Controller contém objeto não alcançável; revise antes de descartar: fileID "+std::to_string(object.fileId));
  if(!asset.valid())return fail(diagnostic,"Grafo Unity convertido viola invariantes do Animator nativo.");
  candidate.transformations={"Parâmetros tipados, estados, condições, duração/offset/interrupção e árvore 1D conectados ao Animator Astra.",
      "fileIDs resolvidos localmente; motions externos exigem GUID/fileID exato e clipe nativo reproduzível.",
      "Posições do grafo são metadados editoriais; scripts, IK, masks, sync e blend não equivalente não são descartados silenciosamente."};
  out=std::move(candidate);diagnostic.clear();return true;
}
bool rebaseUnityController(UnityControllerImport &imported,const AnimatorControllerAsset &baseline,
    const AnimatorControllerAsset &local,std::span<const std::pair<i64,u64>> previous,std::string &diagnostic) {
  if(!baseline.valid()||!local.valid()||!imported.controller.valid()||baseline.guid!=local.guid||baseline.guid!=imported.controller.guid)
    return fail(diagnostic,"Baseline de identidade do controller inválido.");
  auto candidate=imported;auto &graph=candidate.controller.graph;
  u64 frontier=std::max(baseline.graph.nextId,local.graph.nextId);std::map<u64,u64> remap;
  std::map<i64,u64> old;std::set<u64> reserved;
  for(const auto &[file,id]:previous)if(!file||!id||id>=frontier||!old.emplace(file,id).second||!reserved.insert(id).second)
    return fail(diagnostic,"Mapa de fileIDs do controller inválido.");
  const auto allocate=[&]()->u64{return frontier<std::numeric_limits<u64>::max()?frontier++:0;};
  for(const auto &[file,id]:candidate.identities) {
    const auto found=old.find(file);const auto stable=found==old.end()?allocate():found->second;
    if(!stable||!remap.emplace(id,stable).second)return fail(diagnostic,"Identidades do controller esgotadas.");
    old[file]=stable;
  }
  for(const auto &parameter:graph.parameters) {
    const auto *prior=baseline.graph.parameter(parameter.name);const auto stable=prior?prior->id:allocate();
    if(!stable||!remap.emplace(parameter.id,stable).second)return fail(diagnostic,"Identidade de parâmetro inválida.");
  }
  for(const auto &layer:graph.layers) {
    u64 stable=0;for(const auto &prior:baseline.graph.layers)if(prior.name==layer.name)stable=prior.id;
    if(!stable)stable=allocate();
    if(!stable||!remap.emplace(layer.id,stable).second)return fail(diagnostic,"Identidade de camada inválida.");
  }
  const auto mapped=[&](u64 id)->u64{return id?remap.at(id):0;};
  for(auto &parameter:graph.parameters)parameter.id=mapped(parameter.id);
  for(auto &layer:graph.layers) {
    layer.id=mapped(layer.id);layer.defaultState=mapped(layer.defaultState);
    for(auto &state:layer.states) {
      state.id=mapped(state.id);state.blendX=mapped(state.blendX);state.blendY=mapped(state.blendY);
      state.speedParameter=mapped(state.speedParameter);state.machine=mapped(state.machine);
    }
    for(auto &transition:layer.transitions) {
      transition.id=mapped(transition.id);transition.from=mapped(transition.from);transition.to=mapped(transition.to);transition.machine=mapped(transition.machine);
      for(auto &condition:transition.conditions)condition.parameter=mapped(condition.parameter);
    }
  }
  graph.nextId=frontier;candidate.identities.assign(old.begin(),old.end());
  if(!candidate.controller.valid())return fail(diagnostic,"Controller reidentificado viola invariantes nativas.");
  imported=std::move(candidate);diagnostic.clear();return true;
}
} // namespace ae::resources
