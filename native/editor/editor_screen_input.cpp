#include "editor/editor_screen.h"
#include "editor/editor_screen_internal.h"
#include "editor/editor_component_catalog.h"
#include "editor/editor_component_visuals.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_reference_picker.h"
#include "scene/event_connection.h"
#include "scene/script_behavior.h"
#include "scene/import_link.h"
#include "scene/prefab_link.h"
#include <algorithm>
#include <cstdio>

namespace ae::editor {
using namespace ae::ui;

std::vector<std::string_view> componentGroups(const scene::ComponentValue &component) {
  std::vector<std::string_view> groups;
  const auto collect=[&](const auto &properties) {
    for(const auto &p:properties) if(p.presentation.isVisible(component)&&!p.presentation.group.empty() &&
        std::find(groups.begin(),groups.end(),p.presentation.group)==groups.end()) groups.push_back(p.presentation.group);
  };
  const auto &type=component.type();
  collect(type.numbers);collect(type.booleans);collect(type.enums);
  collect(type.references);collect(type.resourceBindings);collect(type.slotNumbers);
  if(type.id==std::string_view{"astra.physics2d.joint"}) {
    const auto rank=[](std::string_view group){return group=="Conexão"?0:(group=="Anchors"||group=="Âncoras")?1:2;};
    std::stable_sort(groups.begin(),groups.end(),[&](std::string_view a,std::string_view b){return rank(a)<rank(b);});
  }
  // Conexão de evento lê como frase: primeiro o gatilho, depois a resposta.
  if(&type==&scene::EventConnection::descriptor) {
    const auto rank=[](std::string_view group){return group=="Quando"?0:group=="Então"?1:2;};
    std::stable_sort(groups.begin(),groups.end(),[&](std::string_view a,std::string_view b){return rank(a)<rank(b);});
  }
  return groups;
}

std::vector<u32> namedPhysicsLayers(const runtime::GameplayLayers &layers) {
  std::vector<u32> ids;
  for(u32 i=0;i<runtime::GameplayLayers::kCount;++i) if(layers.named(i)) ids.push_back(i);
  return ids;
}

EditorPointerOutcome applyEditorPointer(EditorScreenState &state,
                                        const EditorScreenLayout &layout,
                                        const ui::UiPointerRouting &routing,
                                        EditorDocument &document, EditorHistory &history) {
  EditorPointerOutcome outcome{};
  if (routing.target == ui::UiPointerTarget::Viewport) {
    outcome.viewport = true;
    return outcome;
  }
  if (routing.target != ui::UiPointerTarget::Widget) {
    outcome.consumed = true;
    return outcome;
  }
  outcome.consumed = true;
  const u32 widget = routing.widgetId;

  // Os divisores respondem ao ARRASTE, e continuamente. Esperar o dedo levantar
  // para reposicionar o painel tornaria impossível encontrar a largura certa.
  if (widget == widgetId(EditorWidget::SplitterLeft) && routing.dragging) {
    const float base =
        layout.hierarchyPanel.width > 0.0f ? layout.hierarchyPanel.width : state.hierarchyWidth;
    state.hierarchyWidth = std::max(kPanelMinimum, base + routing.stepDelta.x);
    return outcome;
  }
  if (widget == widgetId(EditorWidget::SplitterRight) && routing.dragging) {
    const float base =
        layout.inspectorPanel.width > 0.0f ? layout.inspectorPanel.width : state.inspectorWidth;
    state.inspectorWidth = std::max(kPanelMinimum, base - routing.stepDelta.x);
    return outcome;
  }

  if (((widget >= widgetId(EditorWidget::HierarchyRowBase) && widget < widgetId(EditorWidget::TransformFieldBase)) ||
       (widget >= widgetId(EditorWidget::HierarchyCollapseBase) && widget < widgetId(EditorWidget::ImpactOpenBase))) && routing.dragging) {
    state.hierarchyScrollRemainder -= routing.stepDelta.y;
    const int lines=static_cast<int>(state.hierarchyScrollRemainder/kRowHeight);
    state.hierarchyScrollRemainder-=static_cast<float>(lines)*kRowHeight;
    const int maximum=std::max(0,static_cast<int>(layout.hierarchyRowCount)-static_cast<int>(layout.hierarchyVisibleRows));
    state.hierarchyScroll=static_cast<u32>(std::clamp(static_cast<int>(state.hierarchyScroll)+lines,0,maximum));
    return outcome;
  }

  // A lista do Add rola pelo arraste sobre as linhas, como a Hierarquia; o
  // toque que não virou arraste continua abrindo a prévia do tipo.
  if(state.addingComponent && routing.dragging &&
     ((widget>=widgetId(EditorWidget::ComponentAddBase) && widget<widgetId(EditorWidget::ComponentAddBase)+detail::kRange) ||
      (widget>=widgetId(EditorWidget::ScriptAddBase) && widget<widgetId(EditorWidget::ScriptAddBase)+detail::kRange))) {
    constexpr float kCatalogRowHeight=52.0f;
    state.addScrollRemainder-=routing.stepDelta.y;
    const int lines=static_cast<int>(state.addScrollRemainder/kCatalogRowHeight);
    state.addScrollRemainder-=static_cast<float>(lines)*kCatalogRowHeight;
    const int maximum=std::max(0,static_cast<int>(layout.addRowCount)-static_cast<int>(layout.addVisibleRows));
    state.addScroll=static_cast<u32>(std::clamp(static_cast<int>(state.addScroll)+lines,0,maximum));
    return outcome;
  }
  if(state.creationMenu && routing.dragging && widget>=widgetId(EditorWidget::CreationRowBase) &&
     widget<widgetId(EditorWidget::CreationRowBase)+detail::kRange) {
    constexpr float kCardPitch=100.0f;
    state.addScrollRemainder-=routing.stepDelta.y;
    const int rows=static_cast<int>(state.addScrollRemainder/kCardPitch);
    state.addScrollRemainder-=static_cast<float>(rows)*kCardPitch;
    const int maximum=std::max(0,static_cast<int>(layout.creationRowCount)-static_cast<int>(layout.creationVisibleRows));
    state.creationScroll=static_cast<u32>(std::clamp(static_cast<int>(state.creationScroll)+rows,0,maximum));
    return outcome;
  }
  if(state.addingComponent && routing.dragging && widget>=widgetId(EditorWidget::ComponentFamilyBase) &&
     widget<widgetId(EditorWidget::ComponentFamilyBase)+detail::kRange) {
    constexpr float kRailCell=44.0f;
    state.addScrollRemainder-=routing.stepDelta.y;
    const int cells=static_cast<int>(state.addScrollRemainder/kRailCell);
    state.addScrollRemainder-=static_cast<float>(cells)*kRailCell;
    const int maximum=std::max(0,static_cast<int>(layout.addRailCount)-static_cast<int>(layout.addRailVisible));
    state.addRailScroll=static_cast<u32>(std::clamp(static_cast<int>(state.addRailScroll)+cells,0,maximum));
    return outcome;
  }

  // O resto age no toque completo, e não no `Down`: um dedo que desce num botão
  // e desliza para fora desistiu dele.
  if (!routing.tapped) return outcome;

  if(widget>=widgetId(EditorWidget::ComponentSlotEnumBase)&&widget<widgetId(EditorWidget::ComponentSlotEnumBase)+0x01000000u) {
    const auto *entity=document.find(state.selection);
    const u32 encoded=widget-widgetId(EditorWidget::ComponentSlotEnumBase);
    const u32 index=encoded&0xffu,field=(encoded>>8)&0xffu,slot=(encoded>>16)&0xffu;
    const auto *component=entity&&index<entity->components.size()?entity->components.at(index):nullptr;
    if(!component||field>=component->type().slotEnums.size()) return outcome;
    const auto &property=component->type().slotEnums[field];
    if(!property.write||!property.read||slot>=property.slotCount(*component)||property.options.empty()||!property.presentation.isEditable(*component)) return outcome;
    usize selected=0;
    for(usize i=0;i<property.options.size();++i) if(property.options[i].value==property.read(*component,slot)) {selected=(i+1)%property.options.size();break;}
    if(property.options.size()==2 && state.multi.count>1 && state.multi.isMixed(multiKey(component->instanceId(),std::string(property.id)+"@"+std::to_string(slot)))) selected=1;
    auto candidate=*entity;
    if(scene::setComponentSlotProperty(candidate.components,component->type().id,property.id,slot,property.options[selected].value,component->instanceId())!=scene::ComponentPropertyStatus::Applied) {
      state.status="Valor por slot recusado pelo componente";return outcome;
    }
    outcome.documentChanged=history.applyValues(document,state.selection,candidate);
    return outcome;
  }

  if(widget>=widgetId(EditorWidget::InspectorOverviewOpenBase) && widget<widgetId(EditorWidget::InspectorOverviewOpenBase)+detail::kRange) {
    const auto target=state.inspectorLocked?state.inspectorLocked:state.selection;
    const auto *entity=document.find(target);
    const auto index=widget-widgetId(EditorWidget::InspectorOverviewOpenBase);
    if(!entity || index>=entity->components.size()) return outcome;
    const auto *component=entity->components.at(index);
    if(component->unresolved() || scene::prefabLink(entity->components)==component || scene::importLink(entity->components)==component) return outcome;
    state.inspectorSurface=EditorInspectorSurface::Inspection;state.componentSelection=target;
    state.expandedComponent.clear();state.nativeMenu=0;state.scriptMenu=0;
    state.expandedNative=scene::scriptBehavior(component)?0:component->instanceId();
    state.expandedScript=scene::scriptBehavior(component)?component->instanceId():0;
    state.propertyPage=0;state.scriptPropertyPage=0;state.propertyQuery.clear();
    return outcome;
  }

  if(widget>=widgetId(EditorWidget::ProjectSectionBase) && widget<=widgetId(EditorWidget::ProjectSectionBase)+3) {
    state.projectSection=static_cast<EditorProjectSection>(widget-widgetId(EditorWidget::ProjectSectionBase));
    state.tagPage=0;state.tagQuery.clear();state.tagPicker=false;state.physicsMatrixPage=0;state.inputActionPage=0;state.inputBindingPage=0;state.propertyPage=0;
    // As propriedades de água moram no objeto raiz, como antes na aba avulsa.
    if(state.projectSection==EditorProjectSection::Water) state.selection=document.root();
    return outcome;
  }
  if(widget==widgetId(EditorWidget::PhysicsLayerPrevious) || widget==widgetId(EditorWidget::PhysicsLayerNext)) {
    const auto named=namedPhysicsLayers(document.layers());
    const auto it=std::find(named.begin(),named.end(),state.physicsLayer);
    const u32 index=it==named.end()?0u:static_cast<u32>(it-named.begin());
    if(!named.empty()) state.physicsLayer=named[widget==widgetId(EditorWidget::PhysicsLayerNext)?
      (index+1)%named.size():(index+named.size()-1)%named.size()];
    state.physicsMatrixPage=0;return outcome;
  }
  if(widget==widgetId(EditorWidget::PhysicsLayerAdd)) {
    auto layers=document.layers();
    for(u32 i=1;i<runtime::GameplayLayers::kCount;++i) if(!layers.named(i)) {
      if(layers.setName(i,"Camada "+std::to_string(i)) && history.setLayers(document,layers)) {
        state.physicsLayer=i;state.physicsMatrixPage=0;
      }
      break;
    }
    return outcome;
  }
  if(widget==widgetId(EditorWidget::PhysicsLayerRename)) {
    if(document.layers().named(state.physicsLayer)) {
      state.editingPhysicsLayerName=true;
      const auto name=document.layers().name(state.physicsLayer);
      std::snprintf(state.renameText,sizeof(state.renameText),"%.*s",static_cast<int>(name.size()),name.data());
    }
    return outcome;
  }
  if(widget==widgetId(EditorWidget::PhysicsMatrixPrevious)) {if(state.physicsMatrixPage) --state.physicsMatrixPage;return outcome;}
  if(widget==widgetId(EditorWidget::PhysicsMatrixNext)) {++state.physicsMatrixPage;return outcome;}
  if(widget>=widgetId(EditorWidget::PhysicsInteractionBase) &&
     widget<widgetId(EditorWidget::PhysicsInteractionBase)+runtime::GameplayLayers::kCount) {
    const u32 target=widget-widgetId(EditorWidget::PhysicsInteractionBase);
    if(!document.layers().named(state.physicsLayer) || !document.layers().named(target)) return outcome;
    auto layers=document.layers();
    layers.setInteraction(state.physicsLayer,target,!layers.interacts(state.physicsLayer,target));
    history.setLayers(document,layers);
    return outcome;
  }

  if(widget>=widgetId(EditorWidget::InputActionRowBase) &&
     widget<widgetId(EditorWidget::InputActionRowBase)+runtime::InputActionMap::kMaximumActions) {
    const u32 index=widget-widgetId(EditorWidget::InputActionRowBase);
    if(index<document.inputActions().actions().size()) {
      state.inputActionIndex=index;state.inputBindingIndex=0;state.inputBindingPage=0;
      state.inputTab=0;state.inputDetails=false;
    }
    return outcome;
  }
  if(widget>=widgetId(EditorWidget::InputBindingRowBase) &&
     widget<widgetId(EditorWidget::InputBindingRowBase)+runtime::InputActionMap::kMaximumBindings) {
    const auto &actions=document.inputActions().actions();
    if(state.inputActionIndex<actions.size() &&
       widget-widgetId(EditorWidget::InputBindingRowBase)<actions[state.inputActionIndex].bindings.size()) {
      state.inputBindingIndex=widget-widgetId(EditorWidget::InputBindingRowBase);
      state.inputTab=1;state.inputDetails=false;
    }
    return outcome;
  }
  if(widget>=widgetId(EditorWidget::InputActionPrevious) && widget<=widgetId(EditorWidget::InputDetailsToggle)) {
    if(widget==widgetId(EditorWidget::InputDetailsToggle)) {state.inputDetails=!state.inputDetails;return outcome;}
    if(widget==widgetId(EditorWidget::InputActionPagePrevious)) {if(state.inputActionPage) --state.inputActionPage;return outcome;}
    if(widget==widgetId(EditorWidget::InputActionPageNext)) {++state.inputActionPage;return outcome;}
    if(widget==widgetId(EditorWidget::InputBindingPagePrevious)) {if(state.inputBindingPage) --state.inputBindingPage;return outcome;}
    if(widget==widgetId(EditorWidget::InputBindingPageNext)) {++state.inputBindingPage;return outcome;}
    auto map=document.inputActions();
    const auto &actions=map.actions();
    if(widget==widgetId(EditorWidget::InputActionAdd)) {
      runtime::InputAction action;action.kind=runtime::ActionKind::Button;
      action.deadzone=0;action.bindings.push_back({runtime::InputSource::TouchButton,1,0,0,1,false});
      for(u32 i=1;i<=runtime::InputActionMap::kMaximumActions;++i) {
        action.id="Ação "+std::to_string(i);
        if(!map.find(action.id)) break;
      }
      if(map.add(action) && history.setInputActions(document,map)) {
        state.inputActionIndex=static_cast<u32>(map.actions().size()-1);state.inputBindingIndex=0;
        state.inputTab=0;state.inputActionPage=state.inputActionIndex/8;
        state.inputBindingPage=0;outcome.documentChanged=true;
      } else state.status="Limite de ações atingido";
      return outcome;
    }
    if(actions.empty()) return outcome;
    state.inputActionIndex=std::min(state.inputActionIndex,static_cast<u32>(actions.size()-1));
    const auto original=actions[state.inputActionIndex];
    if(widget==widgetId(EditorWidget::InputActionPrevious) || widget==widgetId(EditorWidget::InputActionNext)) {
      state.inputActionIndex=widget==widgetId(EditorWidget::InputActionNext)?
        (state.inputActionIndex+1)%actions.size():(state.inputActionIndex+actions.size()-1)%actions.size();
      state.inputBindingIndex=0;state.inputBindingPage=0;return outcome;
    }
    if(widget==widgetId(EditorWidget::InputTabAction) || widget==widgetId(EditorWidget::InputTabBinding) || widget==widgetId(EditorWidget::InputTabResponse)) {
      state.inputTab=widget==widgetId(EditorWidget::InputTabResponse)?2u:widget==widgetId(EditorWidget::InputTabBinding)?1u:0u;state.inputDetails=false;return outcome;
    }
    if(widget==widgetId(EditorWidget::InputActionRename)) {
      state.editingInputActionName=true;std::snprintf(state.renameText,sizeof(state.renameText),"%s",original.id.c_str());return outcome;
    }
    if(widget==widgetId(EditorWidget::InputActionContext)) {
      state.editingInputContext=true;std::snprintf(state.renameText,sizeof(state.renameText),"%s",original.context.c_str());return outcome;
    }
    if(widget==widgetId(EditorWidget::InputActionRemove)) {
      if(map.remove(original.id)) {outcome.documentChanged=history.setInputActions(document,map);
        state.inputActionIndex=std::min(state.inputActionIndex,static_cast<u32>(map.actions().empty()?0:map.actions().size()-1));
        state.inputBindingIndex=0;state.inputActionPage=0;state.inputBindingPage=0;}
      return outcome;
    }
    if(widget==widgetId(EditorWidget::InputDuration) || widget==widgetId(EditorWidget::InputDeadzone) || widget==widgetId(EditorWidget::InputSensitivity) ||
       widget==widgetId(EditorWidget::InputBindingCode) || widget==widgetId(EditorWidget::InputBindingNegativeCode) ||
       widget==widgetId(EditorWidget::InputBindingScale)) {
      state.inputEditField=widget;state.numericField=widget;state.numericReplace=true;
      std::string initial;
      if(widget==widgetId(EditorWidget::InputDuration)) initial=std::to_string(original.duration);
      else if(widget==widgetId(EditorWidget::InputDeadzone)) initial=std::to_string(original.deadzone);
      else if(widget==widgetId(EditorWidget::InputSensitivity)) initial=std::to_string(original.sensitivity);
      else if(!original.bindings.empty()) {
        const auto &binding=original.bindings[std::min(state.inputBindingIndex,static_cast<u32>(original.bindings.size()-1))];
        state.inputBindingIndex=std::min(state.inputBindingIndex,static_cast<u32>(original.bindings.size()-1));
        if(widget==widgetId(EditorWidget::InputBindingCode)) initial=std::to_string(binding.code);
        else if(widget==widgetId(EditorWidget::InputBindingNegativeCode)) initial=std::to_string(binding.negativeCode);
        else initial=std::to_string(binding.scale);
      } else {state.inputEditField=0;state.numericField=0;return outcome;}
      std::snprintf(state.numericText,sizeof(state.numericText),"%s",initial.c_str());return outcome;
    }
    if(widget==widgetId(EditorWidget::InputRoleMove) || widget==widgetId(EditorWidget::InputRoleLook) ||
       widget==widgetId(EditorWidget::InputRoleJump)) {
      const bool jump=widget==widgetId(EditorWidget::InputRoleJump);
      if(original.kind!=(jump?runtime::ActionKind::Button:runtime::ActionKind::Axis2D)) {
        state.status=jump?"Saltar exige uma ação Botão":"Mover e Olhar exigem Eixo 2D";return outcome;
      }
      const auto &current=jump?map.jumpAction():widget==widgetId(EditorWidget::InputRoleMove)?map.moveAction():map.lookAction();
      const auto role=current==original.id?std::string_view{}:std::string_view(original.id);
      if(jump?map.setJumpAction(role):widget==widgetId(EditorWidget::InputRoleMove)?map.setMoveAction(role):map.setLookAction(role))
        outcome.documentChanged=history.setInputActions(document,map);
      return outcome;
    }
    if(widget==widgetId(EditorWidget::InputBindingPrevious) || widget==widgetId(EditorWidget::InputBindingNext)) {
      if(!original.bindings.empty()) state.inputBindingIndex=widget==widgetId(EditorWidget::InputBindingNext)?
        (state.inputBindingIndex+1)%original.bindings.size():
        (state.inputBindingIndex+original.bindings.size()-1)%original.bindings.size();
      return outcome;
    }
    auto action=original;
    if(widget==widgetId(EditorWidget::InputActionEnabled))action.enabled=!action.enabled;
    else if(widget>=widgetId(EditorWidget::InputInteractionPress)&&widget<=widgetId(EditorWidget::InputInteractionTap)) {
      if(action.kind!=runtime::ActionKind::Button)return outcome;
      action.interaction=static_cast<runtime::InputInteraction>(widget-widgetId(EditorWidget::InputInteractionPress));
    } else if(widget>=widgetId(EditorWidget::InputGroupTouch)&&widget<=widgetId(EditorWidget::InputGroupGamepad)) {
      action.deviceGroups^=1u<<(widget-widgetId(EditorWidget::InputGroupTouch));
    } else if(widget==widgetId(EditorWidget::InputActionKind)) {
      action.kind=static_cast<runtime::ActionKind>((static_cast<u32>(action.kind)+1)%3);
      if(action.kind!=runtime::ActionKind::Button)action.interaction=runtime::InputInteraction::Press;
      if(action.kind!=runtime::ActionKind::Axis2D) for(auto &binding:action.bindings) binding.axis=0;
      if(action.kind!=runtime::ActionKind::Axis2D) {
        if(map.moveAction()==action.id) map.setMoveAction({});
        if(map.lookAction()==action.id) map.setLookAction({});
      }
      if(action.kind!=runtime::ActionKind::Button && map.jumpAction()==action.id) map.setJumpAction({});
    } else if(widget==widgetId(EditorWidget::InputBindingAdd)) {
      if(action.bindings.size()>=runtime::InputActionMap::kMaximumBindings) {state.status="Máximo de vínculos atingido";return outcome;}
      action.bindings.push_back({runtime::InputSource::TouchButton,1,0,0,1,false});
      state.inputBindingIndex=static_cast<u32>(action.bindings.size()-1);
      state.inputBindingPage=state.inputBindingIndex/4;
    } else {
      if(action.bindings.empty()) return outcome;
      state.inputBindingIndex=std::min(state.inputBindingIndex,static_cast<u32>(action.bindings.size()-1));
      if(widget==widgetId(EditorWidget::InputBindingRemove)) {
        action.bindings.erase(action.bindings.begin()+state.inputBindingIndex);
        state.inputBindingPage=0;
      }
      else {
        auto &binding=action.bindings[state.inputBindingIndex];
        if(widget==widgetId(EditorWidget::InputBindingSource)) {
          binding.source=static_cast<runtime::InputSource>(1+(static_cast<u32>(binding.source)%8));
          if(binding.source==runtime::InputSource::TouchButton && binding.code>=32) binding.code=0;
          if(binding.source!=runtime::InputSource::Key) binding.negativeCode=0;
          if(binding.source==runtime::InputSource::MouseButton && binding.code>=5)binding.code=0;
          if(binding.source==runtime::InputSource::MouseAxis && binding.code>=4)binding.code=0;
          if(binding.source==runtime::InputSource::TouchMove || binding.source==runtime::InputSource::TouchLook) binding.code=0;
        } else if(widget==widgetId(EditorWidget::InputBindingAxis)) {
          if(action.kind!=runtime::ActionKind::Axis2D) {state.status="Esta ação tem apenas um eixo";return outcome;}
          binding.axis=1-binding.axis;
        } else if(widget==widgetId(EditorWidget::InputBindingInvert)) binding.invert=!binding.invert;
        else return outcome;
      }
    }
    if(map.replace(original.id,action)) outcome.documentChanged=history.setInputActions(document,map);
    if(!outcome.documentChanged) state.status="Vínculo ou ação inválida";
    return outcome;
  }

  if(widget==widgetId(EditorWidget::ComponentVisualsToggle)) {state.showComponentVisuals=!state.showComponentVisuals;return outcome;}
  if(widget==widgetId(EditorWidget::CameraViewClose)) {state.cameraViewEntity=0;return outcome;}
  if(widget==widgetId(EditorWidget::SceneLightingToggle)) {state.sceneLighting=!state.sceneLighting;return outcome;}
  if(widget==widgetId(EditorWidget::SceneEffectsToggle)) {state.sceneEffects=!state.sceneEffects;return outcome;}
  if(widget==widgetId(EditorWidget::SceneEffectsMenu)) {state.sceneEffectsMenu=!state.sceneEffectsMenu;return outcome;}
  if(widget==widgetId(EditorWidget::SceneSkyToggle)) {state.sceneSky=!state.sceneSky;return outcome;}
  if(widget==widgetId(EditorWidget::SceneFogToggle)) {state.sceneFog=!state.sceneFog;return outcome;}
  if(widget==widgetId(EditorWidget::ScenePostToggle)) {state.scenePost=!state.scenePost;return outcome;}
  if(widget>=widgetId(EditorWidget::ComponentVisualBase)&&widget<widgetId(EditorWidget::ComponentVisualBase)+0x01000000u) {
    const auto id=widget-widgetId(EditorWidget::ComponentVisualBase);
    if(componentVisualSelectable(document,id,state.hiddenLayers,state.unpickableLayers,
        state.sceneHidden,state.scenePickOff)) {
      state.selection=id;state.textureInspector=false;state.textureViewer=false;state.propertyPage=0;
    }
    return outcome;
  }
  if(widget>=widgetId(EditorWidget::ComponentGroupBase)+0x10000u&&widget<widgetId(EditorWidget::ComponentGroupBase)+0x20000u) {
    // Subseção da coluna: componente no byte alto, grupo no baixo; dobra por tipo.
    const auto *entity=document.find(state.selection);
    const u32 at=(widget>>8)&0xffu,group=widget&0xffu;
    const auto *value=entity&&at<entity->components.size()?entity->components.at(at):nullptr;
    if(!value) return outcome;
    const auto groups=value->type().id==std::string_view{"astra.physics.constant_force"}?
        std::vector<std::string_view>{"Força mundo","Força local"}:componentGroups(*value);
    if(group<groups.size()) EditorScreenState::flip(state.inspectorSections,std::string(value->type().id)+"/"+std::string(groups[group]));
    return outcome;
  }
  if(widget>=widgetId(EditorWidget::ComponentGroupBase)&&widget<widgetId(EditorWidget::ComponentGroupBase)+0x01000000u) {
    const auto *entity=document.find(state.selection);const auto *value=entity?entity->components.findInstance(state.expandedNative):nullptr;
    if(!value) return outcome;
    const auto groups=componentGroups(*value);
    const auto i=widget-widgetId(EditorWidget::ComponentGroupBase);
    if(i<groups.size()) {state.componentGroup=groups[i];state.propertyPage=0;}
    return outcome;
  }

  if(widget==widgetId(EditorWidget::ComponentClipAdd)) {
    const auto *entity=document.find(state.selection);
    const auto *component=entity?entity->components.findInstance(state.expandedNative):nullptr;
    // Na coluna o cartão da Animação pode estar aberto sem foco.
    if(entity && (!component || &component->type()!=&scene::Animation::descriptor))
      component=entity->components.find(scene::Animation::descriptor);
    if(!entity || state.workspace!=EditorWorkspace::Scene || history.isOpen() ||
       !component || &component->type()!=&scene::Animation::descriptor) return outcome;
    auto value=*entity;
    auto *animation=static_cast<scene::Animation *>(value.components.editInstance(component->instanceId()));
    if(animation && animation->appendClip()) outcome.documentChanged=history.applyValues(document,entity->id,value);
    return outcome;
  }
  const u32 clipOperation=widget&0xff000000u;
  if(clipOperation==widgetId(EditorWidget::ComponentClipMoveUpBase) ||
     clipOperation==widgetId(EditorWidget::ComponentClipMoveDownBase) ||
     clipOperation==widgetId(EditorWidget::ComponentClipRemoveBase)) {
    const auto *entity=document.find(state.selection);
    const u32 encoded=widget&0x00ffffffu,type=encoded&0xffu,bindingIndex=(encoded>>8)&0xffu,slot=(encoded>>16)&0xffu;
    if(!entity || state.workspace!=EditorWorkspace::Scene || history.isOpen() || type>=entity->components.size()) return outcome;
    const auto *component=entity->components.at(type);
    if(&component->type()!=&scene::Animation::descriptor || bindingIndex>=component->type().resourceBindings.size()) return outcome;
    const auto &binding=component->type().resourceBindings[bindingIndex];
    if(binding.id!="clips" || slot>=binding.slotCount(*component)) return outcome;
    const u64 element=binding.elementAt(*component,slot);
    if(!element) return outcome;
    auto value=*entity;
    auto *animation=static_cast<scene::Animation *>(value.components.editInstance(component->instanceId()));
    bool changed=false;
    if(clipOperation==widgetId(EditorWidget::ComponentClipRemoveBase)) changed=animation->removeClip(element);
    else if(clipOperation==widgetId(EditorWidget::ComponentClipMoveUpBase) && slot>0) changed=animation->moveClip(element,slot-1);
    else if(clipOperation==widgetId(EditorWidget::ComponentClipMoveDownBase) && slot+1<animation->clips.size())
      changed=animation->moveClip(element,slot+1);
    if(changed) outcome.documentChanged=history.applyValues(document,entity->id,value);
    return outcome;
  }

  if(state.enumPicker && (widget==widgetId(EditorWidget::ComponentEnumPickerClose) ||
     (widget>=widgetId(EditorWidget::ComponentEnumOptionBase) && widget<widgetId(EditorWidget::ComponentEnumOptionBase)+detail::kRange))) {
    const u32 field=state.enumPicker,index=field&0xffu,enumIndex=(field>>8)&0xffffu;
    state.enumPicker=0;
    if(widget==widgetId(EditorWidget::ComponentEnumPickerClose)) return outcome;
    const auto *entity=document.find(state.selection);
    const auto *component=entity&&index<entity->components.size()?entity->components.at(index):nullptr;
    const u32 option=widget-widgetId(EditorWidget::ComponentEnumOptionBase);
    if(!component || enumIndex>=component->type().enums.size() || history.isOpen()) return outcome;
    const auto &property=component->type().enums[enumIndex];
    if(option>=property.options.size() || property.options[option].value==property.read(*component)) return outcome;
    auto value=*entity;
    if(scene::setComponentProperty(value.components,component->type().id,property.id,property.options[option].value,
                                   component->instanceId())!=scene::ComponentPropertyStatus::Applied) {
      state.status="Esta opção não é aceita com os valores atuais";return outcome;
    }
    outcome.documentChanged=history.applyValues(document,entity->id,value);
    return outcome;
  }
  // Menu comum a componentes nativos e comportamentos C#. O índice é a
  // posição do componente no objeto, a mesma que o Inspector usa nos cartões.
  {
    const u32 operation=widget&0xff000000u,index=widget&0x00ffffffu;
    const auto is=[&](EditorWidget base){return operation==widgetId(base);};
    const auto *entity=document.find(state.selection);
    const auto *component=entity&&index<entity->components.size()?entity->components.at(index):nullptr;
    const auto *script=scene::scriptBehavior(component);
    const bool generic=is(EditorWidget::ComponentMoveUpBase)||is(EditorWidget::ComponentMoveDownBase)||
        is(EditorWidget::ComponentPasteNewBase)||is(EditorWidget::ComponentHelpBase)||is(EditorWidget::ComponentEnableBase)||
        (script&&(is(EditorWidget::ComponentCopyBase)||is(EditorWidget::ComponentPasteBase)||is(EditorWidget::ComponentResetBase)));
    if(generic) {
      if(!component || state.workspace!=EditorWorkspace::Scene || history.isOpen()) return outcome;
      // Como o menu de contexto da Unity, escolher uma ação fecha o menu.
      state.nativeMenu=0;state.scriptMenu=0;
      auto value=*entity;
      if(is(EditorWidget::ComponentHelpBase)) {
        const auto *schema=scene::findComponentSchema(component->type().id);
        if(schema && schema->reference.starts_with("https://")) {
          state.externalLink=std::string(schema->reference);
          state.status=std::string("Referência de ")+schema->name+" aberta no navegador";
        } else state.status="Este componente não tem página de referência";
        return outcome;
      }
      if(is(EditorWidget::ComponentCopyBase)) {
        state.componentClipboard=component->clone();state.status="Componente copiado";return outcome;
      }
      if(is(EditorWidget::ComponentMoveUpBase)||is(EditorWidget::ComponentMoveDownBase)) {
        // O vizinho é o próximo cartão visível: vínculos de importação e outros
        // dados sem cartão não contam como posição.
        const bool up=is(EditorWidget::ComponentMoveUpBase);
        const auto visible=[&](usize i) {
          const auto *other=entity->components.at(i);
          return findEditorComponent(other->type().id)||scene::scriptBehavior(other)||other->unresolved();
        };
        usize target=index;bool found=false;
        if(up) {for(usize i=index;i>0;--i) if(visible(i-1)) {target=i-1;found=true;break;}}
        else for(usize i=index+1;i<entity->components.size();++i) if(visible(i)) {target=i;found=true;break;}
        if(!found || !value.components.moveInstance(component->instanceId(),target)) return outcome;
        state.status=up?"Componente movido para cima":"Componente movido para baixo";
      } else if(is(EditorWidget::ComponentPasteNewBase)) {
        const auto &clip=state.componentClipboard;
        if(!clip) {state.status="Copie um componente antes";return outcome;}
        auto plan=scene::planComponentAddition(value.components,clip->type().id);
        if(!plan.ready) {state.status=plan.error?plan.error:"Não foi possível colar como novo";return outcome;}
        if(!plan.candidate.replaceInstance(plan.requestedInstance,*clip) ||
           !editorReferencesAccept(document,entity->id,*clip)) {state.status="Valores ou referências incompatíveis";return outcome;}
        value.components=std::move(plan.candidate);state.status="Componente colado como novo";
      } else if(is(EditorWidget::ComponentPasteBase)) {
        const auto *copied=scene::scriptBehavior(state.componentClipboard.get());
        if(!copied || copied->scriptType!=script->scriptType ||
           !value.components.replaceInstance(component->instanceId(),*copied)) {state.status="Copie um comportamento do mesmo tipo";return outcome;}
        state.status="Valores do comportamento colados";
      } else if(is(EditorWidget::ComponentResetBase)) {
        // Sem valores autorados, o comportamento volta aos padrões do código.
        auto replacement=*script;replacement.properties.clear();
        if(!value.components.replaceInstance(component->instanceId(),replacement)) return outcome;
        state.status="Comportamento redefinido para os padrões do código";
      } else if(is(EditorWidget::ComponentEnableBase)) {
        if(script) {
          auto replacement=*script;replacement.enabled=!replacement.enabled;
          if(!value.components.replaceInstance(component->instanceId(),replacement)) return outcome;
        } else {
          const scene::ComponentBoolean *enabled=nullptr;
          for(const auto &p:component->type().booleans) if(p.id=="enabled") enabled=&p;
          if(!enabled || scene::setComponentProperty(value.components,component->type().id,"enabled",!enabled->read(*component),
                                                     component->instanceId())!=scene::ComponentPropertyStatus::Applied) return outcome;
        }
      }
      outcome.documentChanged=history.applyValues(document,entity->id,value);
      return outcome;
    }
  }

  if(widget>=widgetId(EditorWidget::ComponentAddBase) && widget<widgetId(EditorWidget::ComponentEnumBase)+0x01000000u) {
    const auto *entity=document.find(state.selection);
    if(!entity || state.workspace!=EditorWorkspace::Scene) return outcome;
    if(state.componentSelection!=state.selection) {
      state.componentSelection=state.selection;state.expandedComponent.clear();state.expandedNative=0;state.nativeMenu=0;state.addingComponent=false;state.componentPage=0;state.expandedScript=0;state.scriptMenu=0;
    }
    const u32 operation=widget&0xff000000u;
    const u32 index=widget&((operation==widgetId(EditorWidget::ComponentBooleanBase)||operation==widgetId(EditorWidget::ComponentEnumBase))?0xffu:0x00ffffffu);
    const bool adding=operation==widgetId(EditorWidget::ComponentAddBase);
    const auto *component=adding?nullptr:entity->components.at(index);
    const auto *metadata=adding?(index<editorComponentCatalog.size()?&editorComponentCatalog[index]:nullptr):component?findEditorComponent(component->type().id):nullptr;
    if(!metadata) return outcome;
    const auto &entry=*metadata;
    const u64 instance=component?component->instanceId():0;
    if(operation==widgetId(EditorWidget::ComponentMenuBase)) {
      state.nativeMenu=state.nativeMenu==instance?0:instance;state.expandedNative=0;state.expandedComponent.clear();state.expandedScript=0;state.propertyQuery.clear();
    } else if(operation==widgetId(EditorWidget::ComponentFoldBase)) {
      state.expandedNative=state.tapInspectorCard(entry.type->id,state.expandedNative==instance)?instance:0;state.expandedComponent.clear();state.expandedScript=0;state.nativeMenu=0;state.scriptMenu=0;state.meshPicker=false;state.propertyQuery.clear();state.propertyPage=0;
    } else if(operation==widgetId(EditorWidget::ComponentCopyBase)) {
      state.componentClipboard=component->clone();state.status="Componente copiado";state.nativeMenu=0;
    } else {
      auto value=*entity;
      if(adding) {
        if(const auto *reason=entry.unavailable(value)) {state.status=reason;return outcome;}
        auto plan=scene::planComponentAddition(value.components,entry.type->id);
        if(!plan.ready) {state.status=plan.error;return outcome;}
        value.components=std::move(plan.candidate);
        state.expandedComponent.clear();state.expandedNative=0;state.expandedScript=0;state.componentPage=~u32{0};state.addingComponent=false;state.nativeMenu=0;
      } else if(operation==widgetId(EditorWidget::ComponentBooleanBase)) {
        const u32 field=(widget&0x00ffffffu)>>8;if(field>=entry.type->booleans.size()) return outcome;
        const auto &property=entry.type->booleans[field];
        if(scene::setComponentProperty(value.components,entry.type->id,property.id,!property.read(*component),instance)!=scene::ComponentPropertyStatus::Applied) return outcome;
      } else if(operation==widgetId(EditorWidget::ComponentEnumBase)) {
        const u32 field=(widget&0x00ffffffu)>>8;if(field>=entry.type->enums.size()) return outcome;
        state.enumPicker=widget;return outcome;
      } else if(operation==widgetId(EditorWidget::ComponentPasteBase)) {
        state.nativeMenu=0;
        if(!state.componentClipboard||&state.componentClipboard->type()!=entry.type||
           !editorReferencesAccept(document,entity->id,*state.componentClipboard)||
           !value.components.replaceInstance(instance,*state.componentClipboard)) {state.status="Valores ou referências incompatíveis";return outcome;}
      } else if(operation==widgetId(EditorWidget::ComponentResetBase)) {
        state.nativeMenu=0;
        const auto defaults=entry.type->create();if(!defaults||!value.components.replaceInstance(instance,*defaults)) return outcome;
      } else return outcome;
      outcome.documentChanged=history.applyValues(document,state.selection,value);
    }
    state.propertyPage=0;
    return outcome;
  }

  if (widget >= widgetId(EditorWidget::HierarchyActiveBase) && widget < widgetId(EditorWidget::HierarchyActiveBase)+EditorDocument::kMaximumEntities) {
    // Caixa de ativo da linha (ITsMagic): liga e desliga o objeto daquela
    // linha, com Desfazer, sem trocar a seleção.
    const EditorEntityId entity=widget-widgetId(EditorWidget::HierarchyActiveBase);
    if(const auto *found=document.find(entity);found && entity!=document.root()) {
      auto values=*found;values.active=!values.active;
      outcome.documentChanged=history.applyValues(document,entity,values);
    }
    return outcome;
  }
  if (widget >= widgetId(EditorWidget::HierarchyCollapseBase) && widget < widgetId(EditorWidget::ImpactOpenBase)) {
    const auto entity=widget-widgetId(EditorWidget::HierarchyCollapseBase);
    if(document.find(entity)) {
      auto &ids=state.collapsedEntities;
      auto it=std::find(ids.begin(),ids.end(),entity);
      if(it==ids.end()) ids.push_back(entity); else ids.erase(it);
      state.hierarchyScroll=0;
    }
    return outcome;
  }
  if (widget >= widgetId(EditorWidget::HierarchyEyeBase) &&
      widget < widgetId(EditorWidget::TransformFieldBase)) {
    // Unity SceneVisibility: o olho esconde (ou mostra) o objeto e os
    // descendentes só na vista do editor; a cena e o jogo não mudam.
    const EditorEntityId entity = widget - widgetId(EditorWidget::HierarchyEyeBase);
    if (document.find(entity) != nullptr) {
      std::vector<EditorEntityId> subtree;document.collectSubtree(entity,subtree);
      const bool hide=!state.sceneHiddenHas(entity);
      for(const auto id:subtree) {
        auto found=std::find(state.sceneHidden.begin(),state.sceneHidden.end(),id);
        if(hide && found==state.sceneHidden.end()) state.sceneHidden.push_back(id);
        if(!hide && found!=state.sceneHidden.end()) state.sceneHidden.erase(found);
      }
      state.sceneVisibilityChanged=true;
    }
    return outcome;
  }
  if (widget >= widgetId(EditorWidget::HierarchyPickBase) && widget < widgetId(EditorWidget::HierarchyPickBase)+EditorDocument::kMaximumEntities) {
    // Unity Scene picking: a mão tira (ou devolve) o objeto e os descendentes
    // da seleção por toque na vista; a Hierarquia continua selecionando.
    const EditorEntityId entity = widget - widgetId(EditorWidget::HierarchyPickBase);
    if (document.find(entity) != nullptr) {
      std::vector<EditorEntityId> subtree;document.collectSubtree(entity,subtree);
      const bool off=!state.scenePickOffHas(entity);
      for(const auto id:subtree) {
        auto found=std::find(state.scenePickOff.begin(),state.scenePickOff.end(),id);
        if(off && found==state.scenePickOff.end()) state.scenePickOff.push_back(id);
        if(!off && found!=state.scenePickOff.end()) state.scenePickOff.erase(found);
      }
      state.sceneVisibilityChanged=true;
    }
    return outcome;
  }
  if (widget >= widgetId(EditorWidget::HierarchyRowBase) &&
      widget < widgetId(EditorWidget::HierarchyEyeBase)) {
    const auto target=widget-widgetId(EditorWidget::HierarchyRowBase);
    if(state.reparentEntity) {
      outcome.documentChanged=history.reparentKeepingWorld(document,state.reparentEntity,target);
      state.status=outcome.documentChanged?"Pai alterado":"Pai invalido ou transformacao incompativel";
      state.reparentEntity=kInvalidEntity;
    } else {state.selection=target;state.selectionSet={target};state.propertyPage=0;state.routePoint=0;}
    return outcome;
  }

  const auto toggleField = [&](bool EditorEntity::*field) {
    const EditorEntity *found = document.find(state.selection);
    if (found == nullptr) return;
    EditorEntity values = *found;
    values.*field = !(values.*field);
    outcome.documentChanged = history.applyValues(document, state.selection, values);
  };

  switch (static_cast<EditorWidget>(widget)) {
    case EditorWidget::AddComponentMenu:
      if(state.componentSelection!=state.selection) {state.componentSelection=state.selection;state.expandedComponent.clear();state.expandedNative=0;state.expandedScript=0;state.componentPage=0;state.addingComponent=false;}
      state.addingComponent=!state.addingComponent;state.addScroll=0;state.componentPreview=0;state.componentPreviewValues=false;state.componentPreviewPage=0;state.scriptPreviewType.clear();state.meshPicker=false;state.propertyPage=0;state.componentPage=0;break;
    case EditorWidget::ComponentPreviewBack: state.componentPreview=0;state.componentPreviewValues=false;state.componentPreviewPage=0;state.scriptPreviewType.clear();break;
    case EditorWidget::ComponentPreviewComposition: state.componentPreviewValues=false;state.componentPreviewPage=0;break;
    case EditorWidget::ComponentPreviewValues: state.componentPreviewValues=true;state.componentPreviewPage=0;break;
    case EditorWidget::ComponentPreviewPrevious: if(state.componentPreviewPage) --state.componentPreviewPage;break;
    case EditorWidget::ComponentPreviewNext: ++state.componentPreviewPage;break;
    case EditorWidget::PropertyPrevious: if(state.propertyPage) --state.propertyPage; break;
    // The rendered pager already knows its exact field rectangle and only
    // registers Next when another page exists. Do not duplicate layout math.
    case EditorWidget::PropertyNext: ++state.propertyPage;break;
    case EditorWidget::ReparentSelection:
      state.reparentEntity=state.selection;state.entityMenu=false;state.inspectorMenu=false;state.status="Toque no novo pai na hierarquia";break;
    case EditorWidget::MoveToRoot:
      outcome.documentChanged=history.reparentKeepingWorld(document,state.selection,document.root());
      state.entityMenu=false;state.inspectorMenu=false;state.status=outcome.documentChanged?"Movido para raiz":"Transformacao incompativel";break;
    case EditorWidget::MoveEarlier:
    case EditorWidget::MoveLater: {
      u32 index=0;const auto *selected=document.find(state.selection);
      if(selected && document.childIndexOf(selected->id,index)) {
        const auto count=document.childrenOf(selected->parent).size();
        const u32 next=widget==widgetId(EditorWidget::MoveEarlier)?(index?index-1:0):std::min(index+1,static_cast<u32>(count-1));
        if(next!=index) outcome.documentChanged=history.reparent(document,selected->id,selected->parent,next);
      }
      state.entityMenu=false;state.inspectorMenu=false;break;
    }
    case EditorWidget::AssetsPrevious:
    case EditorWidget::AssetsNext: {
      const u32 page=std::max(1u,static_cast<u32>(std::max(0.0f,layout.viewport.height-108)/44));
      if(widget==widgetId(EditorWidget::AssetsNext)) state.assetScroll=std::min(state.assetScroll+page,state.assetCount?state.assetCount-1:0);
      else state.assetScroll=state.assetScroll>page?state.assetScroll-page:0;
      break;
    }
    case EditorWidget::InspectorMenu: state.inspectorMenu=!state.inspectorMenu;state.transformMenu=false;state.propertyPage=0;break;
    case EditorWidget::RenameSelection:
      if(const auto *entity=document.find(state.selection)) {
        state.renameEntity=entity->id;
        std::snprintf(state.renameText,sizeof(state.renameText),"%s",entity->name);
      }
      state.entityMenu=false;state.inspectorMenu=false;break;
    case EditorWidget::NavigationOrbit: state.navigation=EditorNavigationMode::Orbit;break;
    case EditorWidget::NavigationPan: state.navigation=EditorNavigationMode::Pan;break;
    case EditorWidget::NavigationZoom: state.navigation=EditorNavigationMode::Zoom;break;
    case EditorWidget::CreationSearch: state.editingCreationSearch=true;std::copy(std::begin(state.creationSearch),std::end(state.creationSearch),state.renameText);break;
    case EditorWidget::CreationClearSearch: state.creationSearch[0]=0;state.creationScroll=0;break;
    case EditorWidget::HierarchySearch:
      state.editingHierarchySearch=true;std::copy(std::begin(state.hierarchySearch),std::end(state.hierarchySearch),state.renameText);break;
    case EditorWidget::HierarchyClearSearch: state.hierarchySearch[0]=0;state.hierarchyScroll=0;break;
    case EditorWidget::HierarchyExpandAll: state.collapsedEntities.clear();state.hierarchyScroll=0;break;
    case EditorWidget::HierarchyCollapseAll:
      document.collectSubtree(document.root(),state.collapsedEntities);state.hierarchyScroll=0;break;
    case EditorWidget::HierarchyMenu: {
      // O "⋮" abre o menu de contexto (a sessão o posiciona no toque).
      const bool object=document.exists(state.selection) && state.selection!=document.root();
      state.contextMenu=true;state.contextMenuLevel=state.contextMenuRootLevel=object?0u:1u;state.contextMenuScroll=0;
      state.creationAsChild=object;state.entityMenu=false;
      break;
    }
    case EditorWidget::ProjectMenu:
      // O menu Cena é o mesmo menu de contexto, num nível próprio.
      state.contextMenu=!(state.contextMenu && state.contextMenuLevel==context_menu_widget::WorkspaceLevel);
      state.contextMenuLevel=state.contextMenuRootLevel=context_menu_widget::WorkspaceLevel;state.contextMenuScroll=0;
      state.entityMenu=false;
      break;
    case EditorWidget::DuplicateSelection: {
      const auto copy=history.duplicateEntity(document,state.selection);
      if(copy) {state.selection=copy;outcome.documentChanged=true;}
      state.entityMenu=false;state.inspectorMenu=false;
      break;
    }
    case EditorWidget::CreateGroup: {
      const auto parent=(state.creationMenu || state.contextMenuCreating) && state.creationAsChild && document.exists(state.selection) && state.selection!=document.root()?state.selection:document.root();
      const auto copy=history.createEntity(document,parent,EditorEntityKind::Folder,"Objeto vazio");
      if(copy) {state.selection=copy;outcome.documentChanged=true;}
      state.entityMenu=false;state.inspectorMenu=false;
      break;
    }
    case EditorWidget::DeleteSelection:
      outcome.documentChanged=history.destroyEntity(document,state.selection);
      if(outcome.documentChanged) state.selection=kInvalidEntity;
      state.entityMenu=false;state.inspectorMenu=false;
      break;
    case EditorWidget::SceneChip:
    case EditorWidget::SaveDocument: state.saveRequested=true; break;
    case EditorWidget::Undo: outcome.documentChanged = history.undo(document); break;
    case EditorWidget::Redo: outcome.documentChanged = history.redo(document); break;
    case EditorWidget::PlayFromTopBar:
    case EditorWidget::TabPlay:
      if(state.workspace!=EditorWorkspace::Play) {
        if(state.codeBuildBusy) {state.status="Aguarde a compilação antes de Play";break;}
        if(state.code && (state.code->catalogState()==EditorCodeCatalogState::Failed ||
            state.code->catalogState()==EditorCodeCatalogState::Stale || state.code->dirty())) {
          state.status="Play bloqueado: a fonte atual ainda não foi publicada";
          state.consoleCollapsed=false;state.consoleProblems=true;break;
        }
      }
      state.workspace = state.workspace == EditorWorkspace::Play ? EditorWorkspace::Scene : EditorWorkspace::Play;
      state.playPaused=false;state.playStepRequested=false;
      outcome.requestPlay = state.workspace == EditorWorkspace::Play;
      break;
    case EditorWidget::TabGui: state.contextMenu=false;state.workspace=EditorWorkspace::Gui;break;
    case EditorWidget::TabScene: state.contextMenu=false;state.workspace = EditorWorkspace::Scene; break;
    case EditorWidget::TabAssets: if(state.assetCount) {state.contextMenu=false;state.workspace = EditorWorkspace::Assets;} break;
    case EditorWidget::TabLighting: state.contextMenu=false;state.workspace = EditorWorkspace::Lighting; state.selection=document.root(); state.propertyPage=0; break;
    case EditorWidget::TabProject:
      state.contextMenu=false;state.workspace=EditorWorkspace::Project;state.propertyPage=0;
      state.tagPage=0;state.tagQuery.clear();state.tagPicker=false;state.physicsMatrixPage=0;state.inputActionPage=0;state.inputBindingPage=0;
      if(state.projectSection==EditorProjectSection::Water && !waterCreationAvailable(state)) state.projectSection=EditorProjectSection::Layers;
      if(state.projectSection==EditorProjectSection::Water) state.selection=document.root();
      break;
    case EditorWidget::InspectorInspection: state.inspectorSurface=EditorInspectorSurface::Inspection;break;
    case EditorWidget::InspectorComponents: state.inspectorSurface=EditorInspectorSurface::Components;state.componentOverviewScroll=0;break;
    case EditorWidget::InspectorMeshLightmap: state.meshTab=2;state.propertyPage=0;state.meshPicker=false;break;
    case EditorWidget::InspectorReferenceFlags: state.inspectorReferenceFlags=true;state.propertyPage=0;break;
    case EditorWidget::InspectorReferenceFlagsClose: state.inspectorReferenceFlags=false;state.propertyPage=0;break;
    case EditorWidget::InspectorOverviewObject:
      state.inspectorSurface=EditorInspectorSurface::Inspection;state.componentSelection=state.inspectorLocked?state.inspectorLocked:state.selection;
      state.expandedComponent="astra.object";state.expandedNative=0;state.expandedScript=0;
      state.nativeMenu=0;state.scriptMenu=0;state.transformMenu=false;state.inspectorReferenceFlags=false;state.propertyPage=0;break;
    case EditorWidget::InspectorComponentPrevious:
    case EditorWidget::InspectorComponentNext: {
      const auto target=state.inspectorLocked?state.inspectorLocked:state.selection;
      const auto *entity=document.find(target);if(!entity) break;
      std::vector<i32> items{-2,-1}; // Objetos e Transform são dados universais.
      i32 current=state.expandedComponent=="astra.object"?-2:-1;
      for(u32 i=0;i<entity->components.size();++i) {
        const auto *component=entity->components.at(i);
        if(component->unresolved()||(!findEditorComponent(component->type().id)&&!scene::scriptBehavior(component))) continue;
        if(state.multi.count>1&&target==state.selection&&!state.multi.isCommon(component->instanceId())) continue;
        items.push_back(static_cast<i32>(i));
        if(component->instanceId()==state.expandedNative||component->instanceId()==state.expandedScript) current=static_cast<i32>(i);
      }
      const auto found=std::find(items.begin(),items.end(),current);
      const usize at=found==items.end()?0:static_cast<usize>(found-items.begin());
      const usize next=widget==widgetId(EditorWidget::InspectorComponentNext)?(at+1)%items.size():(at+items.size()-1)%items.size();
      state.componentSelection=target;state.expandedNative=0;state.expandedScript=0;
      state.nativeMenu=0;state.scriptMenu=0;state.transformMenu=false;state.propertyPage=0;state.scriptPropertyPage=0;
      state.propertyQuery.clear();state.inspectorReferenceFlags=false;
      const auto selected=items[next];
      state.expandedComponent=selected==-2?"astra.object":selected==-1?"astra.transform":"";
      if(selected>=0) {
        const auto *component=entity->components.at(static_cast<u32>(selected));
        if(scene::scriptBehavior(component)) state.expandedScript=component->instanceId();
        else state.expandedNative=component->instanceId();
      }
      break;
    }
    case EditorWidget::InspectorOverviewTransform:
      state.inspectorSurface=EditorInspectorSurface::Inspection;state.componentSelection=state.inspectorLocked?state.inspectorLocked:state.selection;
      state.expandedComponent="astra.transform";state.expandedNative=0;state.expandedScript=0;state.propertyPage=0;break;
    case EditorWidget::InspectorTabTransform: state.tab = EditorInspectorTab::Transform; break;
    case EditorWidget::InspectorTabMaterial:
      if (const auto *selected=document.find(state.selection); selected && meshAsset(*selected)) {
        state.tab = EditorInspectorTab::Material; state.propertyPage=0;
      }
      break;
    case EditorWidget::InspectorTabProperties: state.tab = EditorInspectorTab::Properties; break;
    case EditorWidget::DiagnosticDockToggle:
      state.diagnosticDockOpen=!state.diagnosticDockOpen;
      state.consoleScroll=0;state.consoleSelected=0;break;
    case EditorWidget::DiagnosticOlder:
      state.consoleScroll=std::min<u32>(EditorConsole::Capacity,state.consoleScroll+3);break;
    case EditorWidget::DiagnosticNewer:
      state.consoleScroll=state.consoleScroll>3?state.consoleScroll-3:0;break;
    case EditorWidget::ToolSelect: state.tool = EditorGizmoMode::Select; break;
    case EditorWidget::ToolMove: state.tool = EditorGizmoMode::Translate; break;
    case EditorWidget::ToolRotate: state.tool = EditorGizmoMode::Rotate; break;
    case EditorWidget::ToolScale: state.tool = EditorGizmoMode::Scale; break;
    case EditorWidget::ViewModeSolid: state.showGrid = !state.showGrid; break;
    case EditorWidget::CompactPanelMenu: state.compactPanelMenu=!state.compactPanelMenu;break;
    case EditorWidget::CompactFiles: state.compactPanel=EditorScreenState::CompactPanel::Files;state.compactPanelMenu=false;break;
    case EditorWidget::CompactViewport: state.compactPanel=EditorScreenState::CompactPanel::Viewport;state.compactPanelMenu=false;break;
    case EditorWidget::HierarchyToggle: state.compactPanel=EditorScreenState::CompactPanel::Hierarchy;state.compactPanelMenu=false;break;
    case EditorWidget::InspectorToggle: state.compactPanel=EditorScreenState::CompactPanel::Inspector;state.compactPanelMenu=false;break;
    case EditorWidget::Fullscreen:
      state.compactPanel=EditorScreenState::CompactPanel::Viewport;
      // Tela cheia é esconder os dois painéis, e não um modo separado: o estado
      // continua o mesmo e voltar devolve as larguras que o usuário tinha.
      state.hierarchyVisible = !state.hierarchyVisible;
      state.inspectorVisible = state.hierarchyVisible;
      break;
    case EditorWidget::InspectorActive: toggleField(&EditorEntity::active); break;
    case EditorWidget::ToggleVisible: toggleField(&EditorEntity::visible); break;
    case EditorWidget::ToggleCastShadow: toggleField(&EditorEntity::castShadow); break;
    case EditorWidget::ToggleReceiveShadow: toggleField(&EditorEntity::receiveShadow); break;
    case EditorWidget::ToggleStatic: toggleField(&EditorEntity::isStatic); break;
    case EditorWidget::CreateChildGroup: {
      const auto parent=document.exists(state.selection)?state.selection:document.root();
      const auto child=history.createEntity(document,parent,EditorEntityKind::Folder,"Objeto vazio");
      if(child) {state.selection=child;outcome.documentChanged=true;}
      state.inspectorMenu=false;break;
    }
    case EditorWidget::TransformMenu: state.transformMenu=!state.transformMenu;state.inspectorMenu=false;break;
    case EditorWidget::TransformCopy:
      if(const auto *entity=document.find(state.selection)) {state.transformClipboard=entity->transform;state.hasTransformClipboard=true;state.status="Transformação copiada";}
      state.transformMenu=false;state.inspectorMenu=false;break;
    case EditorWidget::TransformPaste:
      if(state.hasTransformClipboard && document.exists(state.selection) && state.selection!=document.root())
        outcome.documentChanged=history.setTransform(document,state.selection,state.transformClipboard);
      state.transformMenu=false;state.inspectorMenu=false;break;
    case EditorWidget::TransformReset:
    case EditorWidget::TransformResetPosition:
    case EditorWidget::TransformResetRotation:
    case EditorWidget::TransformResetScale:
      if(const auto *entity=document.find(state.selection); entity && state.selection!=document.root()) {
        auto transform=entity->transform;const EditorTransform identity;
        const bool all=widget==widgetId(EditorWidget::TransformReset);
        if(all || widget==widgetId(EditorWidget::TransformResetPosition)) std::copy(identity.position,identity.position+3,transform.position);
        if(all || widget==widgetId(EditorWidget::TransformResetRotation)) std::copy(identity.rotationDegrees,identity.rotationDegrees+3,transform.rotationDegrees);
        if(all || widget==widgetId(EditorWidget::TransformResetScale)) std::copy(identity.scale,identity.scale+3,transform.scale);
        outcome.documentChanged=history.setTransform(document,state.selection,transform);
      }
      state.transformMenu=false;state.inspectorMenu=false;break;
    case EditorWidget::ObjectLayerPrevious:
    case EditorWidget::ObjectLayerNext:
      if(const auto *entity=document.find(state.selection)) {
        // Só camadas NOMEADAS: uma camada sem nome é uma camada que o projeto
        // não declarou, e escolher uma delas às cegas não diz nada à física.
        const auto &layers=document.layers();const u32 count=runtime::GameplayLayers::kCount;
        const bool forward=widget==widgetId(EditorWidget::ObjectLayerNext);
        u32 layer=entity->layer%count;
        for(u32 step=1;step<count;++step) {
          const u32 candidate=forward?(layer+step)%count:(layer+count-step)%count;
          if(layers.named(candidate)) {layer=candidate;break;}
        }
        if(layer!=entity->layer) {auto values=*entity;values.layer=layer;outcome.documentChanged=history.applyValues(document,state.selection,values);}
        else state.status="Nenhuma outra camada nomeada no projeto";
      }
      break;
    case EditorWidget::HierarchyAdd:
      state.creationAsChild=document.exists(state.selection) && state.selection!=document.root();
      state.creationMenu=true;
      // A folha abre com uma receita da categoria atual selecionada: sem ela o
      // detalhe (destino e Criar) não aparece.
      if(const auto query=editorSearchKey(state.creationSearch);!creationAvailable(state,state.creationSelection) ||
         !creationListed(editorCreationCatalog[state.creationSelection],query,state.creationCategory))
        for(u32 i=0;i<editorCreationCatalog.size();++i)
          if(creationAvailable(state,i) && creationListed(editorCreationCatalog[i],query,state.creationCategory)) {state.creationSelection=i;break;}
      break;
    case EditorWidget::CreationAtRoot: state.creationAsChild=false;break;
    case EditorWidget::CreationAsChild:
      if(document.exists(state.selection) && state.selection!=document.root()) state.creationAsChild=true;
      break;
    case EditorWidget::CreateMenuClose: state.creationMenu=false;break;
    case EditorWidget::ToggleWaterPhysics:
      if(const auto *entity=document.find(state.selection)) {
        auto value=*entity;
        if(setWaterBodyFlags(value,!waterBody(value).physicsEnabled,waterBody(value).infinite))
          outcome.documentChanged=history.applyValues(document,state.selection,value);
      }
      break;
    case EditorWidget::ToggleSceneBody:
    case EditorWidget::ToggleDynamicBody: {
      if(const auto *current=document.find(state.selection)) {
        auto value=*current;
        if(static_cast<EditorWidget>(widget)==EditorWidget::ToggleSceneBody) {
          if(physicsBody(value)) value.components.remove(EditorPhysicsBody::descriptor);
          else editPhysicsBody(value);
        } else if(physicsBody(value)) {auto *body=editPhysicsBody(value);if(body) body->motion=body->motion==scene::BodyMotion::Dynamic?scene::BodyMotion::Static:scene::BodyMotion::Dynamic;}
        outcome.documentChanged=history.applyValues(document,value.id,value);
        state.propertyPage=0;
      }
      break;
    case EditorWidget::PausePlay:
      if(state.workspace==EditorWorkspace::Play && !waterCreationAvailable(state)) {
        state.playPaused=!state.playPaused;state.playStepRequested=false;
      }
      break;
    case EditorWidget::StepPlay:
      if(state.workspace==EditorWorkspace::Play && state.playPaused && !waterCreationAvailable(state)) state.playStepRequested=true;
      break;
    case EditorWidget::PlayInspect:
      if(state.workspace==EditorWorkspace::Play) {
        state.playInspect=!state.playInspect;
        // Numa tela estreita só cabe um painel: abrir já no Inspector.
        if(state.playInspect && state.compactPanel==EditorScreenState::CompactPanel::Viewport)
          state.compactPanel=EditorScreenState::CompactPanel::Inspector;
      }
      break;
    }
    case EditorWidget::ToggleCharacter: {
      if(const auto *current=document.find(state.selection);current && !physicsBody(*current)) {
        auto value=*current;
        if(characterComponent(value)) value.components.remove(EditorCharacter::descriptor);
        else editCharacter(value);
        outcome.documentChanged=history.applyValues(document,value.id,value);state.propertyPage=0;
      }
      break;
    }
    case EditorWidget::ToggleCameraLook: {
      if(const auto *current=document.find(state.selection);current&&cameraComponent(*current)) {
        auto value=*current;
        if(cameraLook(value)) value.components.remove(EditorCameraLook::descriptor);else editCameraLook(value);
        outcome.documentChanged=history.applyValues(document,value.id,value);state.propertyPage=0;
      }
      break;
    }
    case EditorWidget::ToggleRigidBody: toggleField(&EditorEntity::rigidBodyEnabled);state.propertyPage=0;break;
    case EditorWidget::WaterTabSurface:state.waterTab=0;state.propertyPage=0;break;
    case EditorWidget::WaterTabRoute:state.waterTab=1;state.propertyPage=0;break;
    case EditorWidget::WaterTabPhysics:state.waterTab=2;state.propertyPage=0;break;
    case EditorWidget::WaterTabEffects:state.waterTab=3;state.propertyPage=0;break;
    default: break;
  }
  return outcome;
}

} // namespace ae::editor
