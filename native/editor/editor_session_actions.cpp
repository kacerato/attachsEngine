#include "editor/editor_screen.h"
#include "resources/model_import.h"
#include "resources/unity_material_import.h"
#include "editor/editor_material_fields.h"
#include "scene/path.h"
#include "scene/animation.h"
#include "editor/editor_component_impact.h"
#include "editor/editor_texture_import_screen.h"
#include "editor/editor_component_visuals.h"
#include "editor/editor_color_picker.h"
#include "editor/editor_component_catalog.h"
#include "editor/editor_project_tags.h"
#include "editor/editor_collider_fit.h"
#include "editor/editor_lod_group.h"
#include "scene/script_behavior.h"
#include "scene/prefab_link.h"
#include "scene/collision_recipe.h"
#include "editor/editor_water_body_component.h"
#include "editor/editor_route_component.h"
#include "editor/editor_creation_catalog.h"
#include "renderer/primitive_geometry.h"
#include "runtime/primitive_object.h"
#include "editor/editor_session.h"
#include "editor/editor_studio_tools.h"
#include "editor/editor_texture_history.h"
#include "editor/editor_property_tween.h"
#include "scene/property_tween.h"
#include "editor/editor_number_text.h"
#include "editor/editor_scene_template.h"
#include "scene/environment.h"
#include "resources/import_report.h"
#include "editor/editor_import_transaction.h"
#include "resources/texture_compression.h"
#include "resources/image_decode.h"
#include <fstream>
#include "resources/gltf_package.h"
#include "scene/import_link.h"
#include "core/sha256.h"
#include "editor/editor_script_templates.h"
#include "editor/editor_reference_picker.h"
#include "editor/editor_properties.h"
#include "editor/editor_theme.h"
#include "renderer/water_authoring_geometry.h"
#include <algorithm>
#include <map>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <cmath>
#include <cstdio>
#include <sstream>
#include "editor/editor_global_search.h"
#include "scene/component_schema.h"
#include <locale>
#include <cstring>
#include <cstdlib>
#include <limits>
#include "editor/editor_session_internal.h"

namespace ae::editor {
using namespace ae::ui;
using namespace session_detail;

// Coluna do Inspector (ITsMagic Properties): arraste vertical em qualquer
// ponto dela rola, inclusive sobre campos; o horizontal continua do widget (o
// arraste de valor da Transformação). Segurar no cabeçalho antes de mover é
// reordenar, como o toque longo das listas do Android.
bool EditorSession::handleInspectorScroll(const UiPointerEvent &event,const UiPointerRouting &routing) {
  const u32 id=event.pointerId+1;
  if(inspectorWidgetPointer_==id) {if(routing.released) inspectorWidgetPointer_=0;return false;}
  if(inspectorScrollPointer_!=id) {
    if(!routing.dragging || event.phase!=UiPointerPhase::Move || state_.componentReorder || state_.scriptArrayDrag) return false;
    const bool main=layout_.inspectorScrollWindow.contains(routing.start);
    const bool focused=!main && layout_.focusedScrollWindow.contains(routing.start);
    if(!main && !focused) return false;
    const u32 operation=routing.widgetId&0xff000000u;
    const bool header=operation==widgetId(EditorWidget::ComponentFoldBase) || operation==widgetId(EditorWidget::ScriptFoldBase);
    const bool ownDrag=operation==widgetId(EditorWidget::ScriptArrayHandleBase) ||
        (routing.widgetId>=widgetId(EditorWidget::LodBarDividerBase) && routing.widgetId<widgetId(EditorWidget::LodBarDividerBase)+scene::LodGroupMaximumLevels) ||
        (routing.widgetId>=widgetId(EditorWidget::LodBarSegmentBase) && routing.widgetId<=widgetId(EditorWidget::LodBarSegmentBase)+scene::LodGroupMaximumLevels);
    if(ownDrag || (header && routing.heldSeconds>=0.35) || std::abs(routing.totalDelta.y)<std::abs(routing.totalDelta.x)) {
      inspectorWidgetPointer_=id;return false;
    }
    inspectorScrollPointer_=id;
    // O dedo que começou num campo arrastável larga a edição sem deixar passo.
    if(fieldWidget_ && fieldPointer_==event.pointerId) {history_.cancel(document_);fieldWidget_=0;}
  }
  const bool focused=layout_.focusedScrollWindow.contains(routing.start) && !layout_.inspectorScrollWindow.contains(routing.start);
  const auto &window=focused?layout_.focusedScrollWindow:layout_.inspectorScrollWindow;
  const float limit=std::max(0.f,(focused?layout_.focusedScrollContent:layout_.inspectorScrollContent)-window.height);
  float &scroll=focused?state_.focusedInspectorScroll:state_.inspectorScroll;
  scroll=std::clamp(scroll-routing.stepDelta.y,0.f,limit);
  if(routing.released) inspectorScrollPointer_=0;
  return true;
}

// Biblioteca: a lista (ou o diagnóstico) rola pelo arrasto vertical dentro da
// janela dela; um toque continua chegando à linha.
bool EditorSession::handleLibraryScroll(const UiPointerRouting &routing) {
  if(state_.workspace!=EditorWorkspace::Library||!routing.dragging) return false;
  const bool detail=layout_.libraryDetailWindow.contains(routing.start);
  if(!detail&&!layout_.libraryScrollWindow.contains(routing.start)) return false;
  if(std::abs(routing.totalDelta.y)<std::abs(routing.totalDelta.x)) return false;
  const auto &window=detail?layout_.libraryDetailWindow:layout_.libraryScrollWindow;
  const float limit=std::max(0.f,(detail?layout_.libraryDetailContent:layout_.libraryScrollContent)-window.height);
  float &scroll=detail?state_.libraryDiagnosticScroll:state_.libraryScroll;
  scroll=std::clamp(scroll-routing.stepDelta.y,0.f,limit);
  return true;
}

EditorEntityId EditorSession::creationParent() const {
  const bool chosen=(state_.creationMenu||state_.contextMenuCreating)&&state_.creationAsChild;
  return chosen&&document_.exists(state_.selection)&&state_.selection!=document_.root()?state_.selection:document_.root();
}

// Menu de contexto (ITsMagic, Unity): o "+" da Hierarquia abre Criar, o "⋮" e
// o toque longo numa linha abrem as ações do objeto (o toque longo seleciona a
// linha, como o clique direito). Com ele aberto, a lista rola pelo dedo, o
// fundo fecha e uma ação tocada fecha o menu e segue pelo caminho normal, com
// o destino de criação escolhido nele.
bool EditorSession::handleContextMenu(const UiPointerEvent &event,const UiPointerRouting &routing) {
  namespace cm=context_menu_widget;
  if(event.phase==UiPointerPhase::Down) state_.contextMenuCreating=false;
  const auto open=[&](u32 level,UiPoint at) {
    const bool object=document_.exists(state_.selection)&&state_.selection!=document_.root();
    state_.contextMenu=true;state_.contextMenuLevel=state_.contextMenuRootLevel=level>=cm::WorkspaceLevel?level:object?level:1u;
    state_.contextMenuAnchor=at;state_.contextMenuScroll=0;
    if(level<cm::WorkspaceLevel)state_.creationAsChild=object;
    state_.entityMenu=false;state_.creationMenu=false;
  };
  if(!state_.contextMenu) {
    if(!routing.tapped||routing.target!=UiPointerTarget::Widget) return false;
    const u32 key=routing.widgetId,rows=widgetId(EditorWidget::HierarchyRowBase);
    // O menu Cena abre em qualquer espaço de trabalho, inclusive no Play.
    if(key==widgetId(EditorWidget::ProjectMenu)) {open(cm::WorkspaceLevel,routing.position);return true;}
    if(key==clip_widget::id(clip_widget::StudioMenu)) {open(cm::StudioLevel,routing.position);return true;}
    if(key==widgetId(EditorWidget::ImportMenu)) {open(cm::ImportLevel,routing.position);return true;}
    if(isPlaying()) return false;
    if(key==widgetId(EditorWidget::HierarchyAdd)) {open(1,routing.position);return true;}
    if(key==widgetId(EditorWidget::HierarchyMenu)) {open(0,routing.position);return true;}
    if(routing.heldSeconds>=ui::kUiLongPressSeconds&&key>=rows&&key<rows+EditorDocument::kMaximumEntities&&!state_.reparentEntity) {
      setSelection(key-rows);open(0,routing.position);return true;
    }
    return false;
  }
  if(routing.dragging&&layout_.contextMenuWindow.contains(routing.start)) {
    const float limit=std::max(0.f,layout_.contextMenuContent-layout_.contextMenuWindow.height);
    state_.contextMenuScroll=std::clamp(state_.contextMenuScroll-routing.stepDelta.y,0.f,limit);
    return true;
  }
  if(!routing.tapped) return true;
  const u32 key=routing.widgetId;
  if(cm::owns(key)) {
    const u32 code=key-widgetId(EditorWidget::ContextMenuBase);
    if(code==cm::Close) state_.contextMenu=false;
    else if(code==cm::Back) {
      if(state_.contextMenuLevel<=state_.contextMenuRootLevel) state_.contextMenu=false;
      else state_.contextMenuLevel=state_.contextMenuLevel>=2?1u:0u;
      state_.contextMenuScroll=0;
    } else if(code==cm::Create) {state_.contextMenuLevel=1;state_.contextMenuScroll=0;}
    else if(code==cm::AllRecipes) {
      // A folha completa (busca, prévia, receitas indisponíveis com o motivo)
      // abre com o destino escolhido aqui e uma receita da categoria atual.
      state_.contextMenu=false;state_.creationMenu=true;
      const auto query=editorSearchKey(state_.creationSearch);
      if(!creationAvailable(state_,state_.creationSelection)||
         !creationListed(editorCreationCatalog[state_.creationSelection],query,state_.creationCategory))
        for(u32 i=0;i<editorCreationCatalog.size();++i)
          if(creationAvailable(state_,i)&&creationListed(editorCreationCatalog[i],query,state_.creationCategory)) {state_.creationSelection=i;break;}
    } else if(code>=cm::Category&&code<cm::Category+std::size(creationCategories)) {
      state_.contextMenuLevel=2+(code-cm::Category);state_.contextMenuScroll=0;
    }
    return true;
  }
  // Trocar o destino (raiz ou filho) não fecha o menu.
  if(key==widgetId(EditorWidget::CreationAtRoot)||key==widgetId(EditorWidget::CreationAsChild)) return false;
  state_.contextMenu=false;state_.contextMenuCreating=true;
  return false;
}

bool EditorSession::handlePointerNow(const UiPointerEvent &event) {
  if(event.phase==UiPointerPhase::Cancel) {playTouches_.cancel();cancelPlayButtons();}
  if(state_.platformTextInput && pendingTextEdit().purpose!=EditorTextPurpose::None) {
    // O campo do sistema tem o foco; só o botão "Expressão" da barra responde,
    // trocando o teclado numérico pelo de texto sem perder o que foi digitado.
    const auto routed=router_.route(event);
    if(routed.tapped && routed.widgetId==widgetId(EditorWidget::NumericExpressionToggle) && state_.numericField) {
      if(!state_.platformDraft.empty() && state_.platformDraft.size()<sizeof(state_.numericText))
        std::snprintf(state_.numericText,sizeof(state_.numericText),"%s",state_.platformDraft.c_str());
      state_.numericExpression=!state_.numericExpression;state_.numericReplace=false;
    }
    return true;
  }
  UiPointerRouting routing = router_.route(event);
  if(handleContextMenu(event,routing)) return true;
  if(handleColliderAuthoringInput(event,routing))return true;
  if(routing.target==UiPointerTarget::Widget &&
     ((routing.widgetId>=widgetId(EditorWidget::GuiHierarchyRowBase)&&routing.widgetId<widgetId(EditorWidget::GuiHierarchyRowBase)+0x00100000u) ||
      (routing.widgetId>=widgetId(EditorWidget::GuiHierarchyCollapseBase)&&routing.widgetId<widgetId(EditorWidget::GuiHierarchyCollapseBase)+0x00100000u))) {
    if(routing.tapped) {
      const bool fold=routing.widgetId>=widgetId(EditorWidget::GuiHierarchyCollapseBase);
      const auto *row=guiTree_.find(routing.widgetId-widgetId(fold?EditorWidget::GuiHierarchyCollapseBase:EditorWidget::GuiHierarchyRowBase));
      if(row) {
        if(fold) {
          auto &closed=state_.collapsedGui;const auto at=std::find(closed.begin(),closed.end(),row->target);
          if(at==closed.end())closed.push_back(row->target);else closed.erase(at);
        } else if(!row->error.empty())state_.status=row->error;
        else selectGuiElement(row->target);
      }
    }
    return true;
  }
  if(routing.tapped && routing.widgetId>=widgetId(EditorWidget::HierarchyRowBase) &&
     routing.widgetId<widgetId(EditorWidget::HierarchyRowBase)+EditorDocument::kMaximumEntities) {
    gui_.history().commit(gui_.document());gui_.cancelPointers();state_.guiSelection={};state_.guiInspector=false;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::GuiCanvasEdit)) {
    const auto *object=document_.find(state_.selection);
    const auto *value=object?object->components.findInstance(state_.expandedNative):nullptr;
    if(value&&value->type().id==scene::UiCanvas::descriptor.id) {
      const auto asset=static_cast<const scene::UiCanvas&>(*value).document;const auto *record=assets_.find(asset);
      if(record&&record->type==resources::AssetType::UiDocument&&gui_.openResource(record->path))openGui();
      else state_.status="Documento UI ausente, inválido ou documento atual ainda não salvo";
    }
    return true;
  }
  if(guiActive() && routing.tapped) {
    if(routing.widgetId==widgetId(EditorWidget::Undo)) {gui_.history().commit(gui_.document());history_.undo(document_);return true;}
    if(routing.widgetId==widgetId(EditorWidget::Redo)) {history_.redo(document_);return true;}
    if(routing.widgetId==widgetId(EditorWidget::SaveDocument)) {gui_.save();return true;}
  }
  if(event.phase==UiPointerPhase::Down) {
    const auto old=std::find_if(playButtonPointers_.begin(),playButtonPointers_.end(),[&](const auto&p){return p.first==event.pointerId;});
    if(old!=playButtonPointers_.end()) {playButtonPointers_.erase(old);playButtonCanceled_|=runtime::InputTouch;}
  }
  if(routing.target==ui::UiPointerTarget::Widget &&
     (routing.widgetId==widgetId(EditorWidget::JumpCharacter)||routing.widgetId==widgetId(EditorWidget::PlaySecondaryAction))) {
    const u32 mask=routing.widgetId==widgetId(EditorWidget::JumpCharacter)?1u:2u;
    if(event.phase==UiPointerPhase::Down && isPlaying()&&!state_.playPaused&&gameplayInputFocused()) {
      playButtonPointers_.erase(std::remove_if(playButtonPointers_.begin(),playButtonPointers_.end(),
          [&](const auto&p){return p.first==event.pointerId;}),playButtonPointers_.end());
      playButtonPointers_.emplace_back(event.pointerId,mask);
      if(mask==1)jumpPressed_=true;else secondaryPressed_=true;
    }
    if(routing.released) {
      playButtonPointers_.erase(std::remove_if(playButtonPointers_.begin(),playButtonPointers_.end(),
          [&](const auto&p){return p.first==event.pointerId;}),playButtonPointers_.end());
      if(event.phase==UiPointerPhase::Cancel){playButtonCanceled_|=runtime::InputTouch;jumpPressed_=secondaryPressed_=false;}
    }
    return true;
  }
  if(inputCapture_ && !validateInputCapture()) cancelInputBindingCapture();
  if(state_.inputCapturing) {
    if(!routing.tapped) return true;
    cancelInputBindingCapture();
    if(routing.widgetId==widgetId(EditorWidget::InputBindingCaptureCancel)) return true;
  }
  if(routing.tapped) {
    const auto key=routing.widgetId;
    if(key>=widgetId(EditorWidget::TimerControlBase)&&key<widgetId(EditorWidget::TimerControlBase)+0x10000u) {
      if(!playScene_.active()||!playInspecting())return true;
      const auto target=state_.inspectorTarget?state_.inspectorTarget:state_.selection;
      const auto *entity=playScene_.document().find(target);const auto offset=key-widgetId(EditorWidget::TimerControlBase);
      const auto *component=entity?entity->components.at(offset/4):nullptr;
      if(!component||&component->type()!=&scene::Timer::descriptor||offset%4>2)return true;
      const auto *current=playScene_.timers().state(target,component->instanceId());
      const u32 operation=offset%4==0?1u:offset%4==2?2u:(current&&current->paused?4u:3u);
      runtime::SceneTimers::State result;const auto status=playScene_.timerCommand(target,component->instanceId(),operation,0,result);
      state_.status=status==runtime::WorldStatus::Ok?(operation==1?"Timer iniciado":operation==2?"Timer parado":operation==3?"Timer pausado":"Timer retomado"):"Comando de timer recusado";
      return true;
    }
    if(key==widgetId(EditorWidget::InputBindingCode) && state_.workspace==EditorWorkspace::Project && state_.projectSection==EditorProjectSection::Input) {
      auto map=document_.inputActions();const auto &actions=map.actions();
      if(state_.inputActionIndex<actions.size()) {
        auto action=actions[state_.inputActionIndex];
        if(state_.inputBindingIndex<action.bindings.size()) {
          auto &binding=action.bindings[state_.inputBindingIndex];
          const u32 count=binding.source==runtime::InputSource::MouseButton?5u:binding.source==runtime::InputSource::MouseAxis?4u:0u;
          if(count) {
            if(history_.isOpen())return true;
            binding.code=(binding.code+1)%count;
            if(map.replace(action.id,action) && history_.setInputActions(document_,map))state_.status="Canal do mouse atualizado";
            return true;
          }
        }
      }
    }
    if(key==widgetId(EditorWidget::InputBindingCapture) || key==widgetId(EditorWidget::InputBindingCaptureNegative)) {
      beginInputBindingCapture(key==widgetId(EditorWidget::InputBindingCaptureNegative));return true;
    }
    if(key==widgetId(EditorWidget::PlayTimeScale)) {
      constexpr float scales[]{0,.25f,.5f,1,2,4};
      const float current=playScene_.active()?playScene_.world().clock().scale():1.f;
      float next=0;for(float scale:scales) if(scale>current){next=scale;break;}
      if(!setPlayTimeScale(next)) state_.status="Tempo indisponível: inicie a execução da cena";
      return true;
    }
    if(key==widgetId(EditorWidget::ObjectTagOpen)) {state_.tagPicker=true;state_.tagPage=0;state_.tagQuery.clear();return true;}
    if(key==widgetId(EditorWidget::ObjectGroupsOpen)) {
      const auto target=state_.inspectorTarget?state_.inspectorTarget:state_.selection;
      if(document_.find(target)) {state_.groupPicker=true;state_.groupEntity=target;state_.groupPage=0;state_.tagPicker=false;}
      return true;
    }
    if(key==widgetId(EditorWidget::GroupsClose)) {state_.groupPicker=false;state_.editingGroupName=false;return true;}
    if(key==widgetId(EditorWidget::GroupsPrevious)) {if(state_.groupPage) --state_.groupPage;return true;}
    if(key==widgetId(EditorWidget::GroupsNext)) {++state_.groupPage;return true;}
    if(key==widgetId(EditorWidget::GroupNew)) {
      if(document_.find(state_.groupEntity)) {state_.editingGroupName=true;state_.groupEditSlot=0;state_.renameText[0]=0;}
      return true;
    }
    if(key>=widgetId(EditorWidget::GroupEditBase) && key<widgetId(EditorWidget::GroupEditBase)+runtime::ObjectGroups::MaximumCount) {
      const auto index=key-widgetId(EditorWidget::GroupEditBase);const auto *object=document_.find(state_.groupEntity);
      if(state_.groupPicker && object && index<object->groups.names().size()) {
        state_.groupEditSlot=index+1;state_.editingGroupName=true;
        std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",object->groups.names()[index].c_str());
      }
      return true;
    }
    if(key>=widgetId(EditorWidget::GroupRemoveBase) && key<widgetId(EditorWidget::GroupRemoveBase)+runtime::ObjectGroups::MaximumCount) {
      const auto index=key-widgetId(EditorWidget::GroupRemoveBase);const auto *object=document_.find(state_.groupEntity);
      if(state_.groupPicker && object && index<object->groups.names().size() && !history_.isOpen()) {
        auto value=*object;const auto name=value.groups.names()[index];value.groups.remove(name);
        if(history_.applyValues(document_,object->id,value)) state_.status="Associação removida: "+name;
      }
      return true;
    }
    if(key==widgetId(EditorWidget::TagClose)) {state_.tagPicker=false;return true;}
    if(key==widgetId(EditorWidget::TagNew)) {state_.editingTagName=true;state_.renameText[0]=0;return true;}
    if(key==widgetId(EditorWidget::TagSearch)) {
      state_.editingTagSearch=true;std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",state_.tagQuery.c_str());return true;
    }
    if(key==widgetId(EditorWidget::TagPrevious)) {if(state_.tagPage) --state_.tagPage;return true;}
    if(key==widgetId(EditorWidget::TagNext)) {++state_.tagPage;return true;}
    if(key==widgetId(EditorWidget::TagDelete)) {
      auto tags=projectTags_;if(tags.remove(state_.tagSelected) && changeProjectTags(tags)) {
        state_.tagSelected="Untagged";state_.status="Tag excluída do catálogo";
      }
      return true;
    }
    if(key>=widgetId(EditorWidget::TagRowBase) && key<widgetId(EditorWidget::TagRowBase)+runtime::ObjectTags::MaximumCount) {
      const auto index=key-widgetId(EditorWidget::TagRowBase);const auto &names=document_.tags().names();
      if(index<names.size()) {if(state_.tagPicker) assignTag(names[index]);else state_.tagSelected=names[index];}
      return true;
    }
  }
  if(multiAssetEditing_ && state_.materialInspector.valid() && routing.tapped &&
     routing.heldSeconds>=ui::kUiLongPressSeconds && state_.setValueMenu.key.empty()) {
    const auto field=materialWidgetField(routing.widgetId,state_.textureBinding);
    if(!field.empty() && state_.materialMixedHas(field)) {
      state_.setValueMenu.key="asset.mat."+field;state_.setValueMenu.label="Valor do material";
      if(field.starts_with("num")) state_.setValueMenu.label=scene::meshRendererNumbers[std::stoul(field.substr(3))].name;
      else if(const auto *spec=findMaterialField(field)) state_.setValueMenu.label=std::string(spec->label);
      state_.setValueMenu.rows.clear();
      for(u32 i=0;i<multiMaterials_.size();++i) if(const auto *material=findMaterialAsset(multiMaterials_[i])) {
        auto value=materialFieldText(*material,field);
        if(field.starts_with("tex") || field=="ch1") {
          const auto texture=field=="ch1"?material->occlusionTexture:material->textures[std::stoul(field.substr(3))];
          const auto *record=assets_.find(texture);
          value=texture==scene::MaterialTextureNone?"Sem textura":!texture.valid()?"Como a fonte":record?baseName(record->path):"Textura ausente";
        }
        state_.setValueMenu.rows.push_back({i,material->name+" · "+value});
      }
      return true;
    }
  }

  if(multiAssetEditing_ && state_.profileInspector.valid() && routing.tapped &&
     routing.heldSeconds>=ui::kUiLongPressSeconds && state_.setValueMenu.key.empty() &&
     routing.widgetId>=widgetId(EditorWidget::ProfileRowBase) &&
     routing.widgetId<widgetId(EditorWidget::ProfileRowBase)+state_.profileRows.size())
    if(showProfileValueMenu(routing.widgetId-widgetId(EditorWidget::ProfileRowBase))) return true;

  if(multiAssetEditing_ && state_.environmentInspector.valid() && routing.tapped &&
     routing.heldSeconds>=ui::kUiLongPressSeconds && state_.setValueMenu.key.empty()) {
    const u32 key=routing.widgetId;
    const bool down=key>=widgetId(EditorWidget::EnvironmentRecipeDownBase) && key<widgetId(EditorWidget::EnvironmentRecipeDownBase)+5;
    const bool up=key>=widgetId(EditorWidget::EnvironmentRecipeUpBase) && key<widgetId(EditorWidget::EnvironmentRecipeUpBase)+5;
    if((down||up) && showEnvironmentValueMenu(key-widgetId(down?EditorWidget::EnvironmentRecipeDownBase:EditorWidget::EnvironmentRecipeUpBase))) return true;
  }
  // "Definir como o valor de…": modal pequeno com um objeto por linha.
  if(!state_.setValueMenu.key.empty() && routing.tapped) {
    const u32 key=routing.widgetId;
    if(key>=widgetId(EditorWidget::SetValueRowBase) && key<widgetId(EditorWidget::SetValueRowBase)+state_.setValueMenu.rows.size())
      applySetValue(key-widgetId(EditorWidget::SetValueRowBase));
    state_.setValueMenu={};
    return true;
  }
  // Toque longo num campo com valores diferentes abre o menu (Unity: clique
  // direito › Set to Value of).
  if(routing.tapped && routing.heldSeconds>=ui::kUiLongPressSeconds && state_.selectionSet.size()>1 && !isPlaying()) {
    const u32 key=routing.widgetId,operation=key&0xff000000u,low=key&0x00ffffffu;
    const auto *entity=document_.find(state_.selection);
    std::string field,label;
    if(key>=widgetId(EditorWidget::TransformFieldBase) && key<widgetId(EditorWidget::TransformFieldBase)+9) {
      const u32 row=(key-widgetId(EditorWidget::TransformFieldBase))/3,axis=(key-widgetId(EditorWidget::TransformFieldBase))%3;
      field="t."+std::to_string(row)+std::to_string(axis);
      label=std::string(row==0?"Posição":row==1?"Rotação":"Escala")+" "+"XYZ"[axis];
    } else if(entity && (operation==widgetId(EditorWidget::ComponentNumberBase) || operation==widgetId(EditorWidget::ComponentBooleanBase) ||
              operation==widgetId(EditorWidget::ComponentEnumBase) || operation==widgetId(EditorWidget::ComponentReferenceBase))) {
      const auto *component=entity->components.at(low&0xffu);const u32 property=low>>8;
      if(component) {
        const auto &type=component->type();
        const auto id=operation==widgetId(EditorWidget::ComponentNumberBase)?(property<type.numbers.size()?type.numbers[property].id:std::string_view{}):
            operation==widgetId(EditorWidget::ComponentBooleanBase)?(property<type.booleans.size()?type.booleans[property].id:std::string_view{}):
            operation==widgetId(EditorWidget::ComponentEnumBase)?(property<type.enums.size()?type.enums[property].id:std::string_view{}):
            (property<type.references.size()?type.references[property].id:std::string_view{});
        const char *name=operation==widgetId(EditorWidget::ComponentNumberBase)?(property<type.numbers.size()?type.numbers[property].name:""):
            operation==widgetId(EditorWidget::ComponentBooleanBase)?(property<type.booleans.size()?type.booleans[property].name:""):
            operation==widgetId(EditorWidget::ComponentEnumBase)?(property<type.enums.size()?type.enums[property].name:""):
            (property<type.references.size()?type.references[property].name:"");
        if(!id.empty()) {field=multiKey(component->instanceId(),id);label=name;}
      }
    }
    if(!field.empty()) {
      // O Down do campo da Transformação já abriu a transação do arraste: o
      // toque longo não edita, então ela é descartada.
      if(fieldWidget_==key) {history_.cancel(document_);fieldWidget_=0;}
      if(!state_.multi.isMixed(field)) {state_.status="Todos os selecionados já têm o mesmo valor";return true;}
      state_.setValueMenu.key=field;state_.setValueMenu.label=label;state_.setValueMenu.rows.clear();
      // Cada objeto com o valor que tem hoje, para escolher de quem copiar.
      const auto slash=field.find('/');
      const scene::ComponentValue *reference=slash==std::string::npos?nullptr:entity->components.findInstance(std::stoull(field.substr(0,slash)));
      for(const auto id:state_.selectionSet) if(const auto *object=document_.find(id)) {
        std::string value;
        if(field.starts_with("t.")) value=transformChannel(object->transform,field[2]-'0',field[3]-'0');
        else if(reference) if(const auto *mine=matchComponent(object->components,componentKind(*reference),componentOccurrence(entity->components,*reference)))
          value=propertyText(*mine,field.substr(slash+1));
        state_.setValueMenu.rows.push_back({id,std::string(object->name)+"  \xC2\xB7  "+value});
      }
      return true;
    }
  }
  // Toque longo no cabeçalho de um componente abre o menu dele, como o clique
  // direito da Unity (Manual/UsingComponents). O toque longo numa linha da
  // Hierarquia abre o menu de contexto (handleContextMenu).
  if(routing.tapped && routing.heldSeconds>=ui::kUiLongPressSeconds) {
    const u32 operation=routing.widgetId&0xff000000u,index=routing.widgetId&0x00ffffffu;
    if(operation==widgetId(EditorWidget::ComponentFoldBase)) routing.widgetId=widgetId(EditorWidget::ComponentMenuBase)+index;
    else if(operation==widgetId(EditorWidget::ScriptFoldBase)) routing.widgetId=widgetId(EditorWidget::ScriptMenuBase)+index;
  }
  const u32 colliderHandleBase=widgetId(EditorWidget::ColliderHandleBase);
  if(colliderDragOpen_ || (routing.widgetId>=colliderHandleBase &&
      routing.widgetId<colliderHandleBase+static_cast<u32>(ColliderHandleKind::Count))) {
    if(!colliderDragOpen_ && event.phase==UiPointerPhase::Down && !isPlaying() &&
       viewportPointers_.empty() && !history_.isOpen() && state_.showComponentVisuals &&
       !state_.cameraViewEntity && !state_.multiSelect && state_.selectionSet.size()<=1 &&
       !state_.guiSelection.valid() && componentVisualSelectable(document_,state_.selection,
           state_.hiddenLayers,state_.unpickableLayers,state_.sceneHidden,state_.scenePickOff)) {
      const auto *entity=document_.find(state_.selection);
      const auto kind=static_cast<ColliderHandleKind>(routing.widgetId-colliderHandleBase);
      if(entity && colliderHandleMatchesTool(kind,state_.tool) && colliderHandleGeometry(document_,entity->id,
          state_.expandedNative,kind,view_,colliderDragHandle_)) {
        const bool resolved=colliderDragHandle_.rotation?
            colliderRingAngle(view_,colliderDragHandle_,event.position,colliderDragStart_):
            cameraHandleRayParameter(view_,colliderDragHandle_,event.position,colliderDragStart_);
        if(resolved && history_.begin("Editar Colisor")) {
          colliderDragOpen_=true;colliderDragPointer_=event.pointerId;
          colliderDragInitial_=*entity;colliderDragView_=view_;
          colliderDragLastAngle_=colliderDragStart_;colliderDragAngle_=0;
          state_.activeColliderHandle=static_cast<u32>(kind)+1;
        }
      }
    }
    if(colliderDragOpen_ && event.pointerId==colliderDragPointer_) {
      if(event.phase==UiPointerPhase::Cancel || state_.selection!=colliderDragInitial_.id ||
         state_.expandedNative!=colliderDragHandle_.instance || !document_.exists(colliderDragInitial_.id)) {
        history_.cancel(document_);colliderDragOpen_=false;state_.activeColliderHandle=0;
      } else {
        if(routing.dragging) {
          float parameter;
          const bool resolved=colliderDragHandle_.rotation?
              colliderRingAngle(colliderDragView_,colliderDragHandle_,event.position,parameter):
              cameraHandleRayParameter(colliderDragView_,colliderDragHandle_,event.position,parameter);
          if(resolved) {
            float delta=parameter-colliderDragStart_;
            if(colliderDragHandle_.rotation) {
              colliderDragAngle_+=std::remainder(parameter-colliderDragLastAngle_,6.28318530718f);
              colliderDragLastAngle_=parameter;delta=colliderDragAngle_;
            }
            auto changed=colliderDragInitial_;
            if(applyColliderHandleDelta(changed,colliderDragHandle_,delta) &&
               history_.applyValues(document_,changed.id,changed,0xCA04u)) {
              state_.status="Colisor "+std::to_string(colliderDragHandle_.instance)+" · "+
                  std::string(colliderDragHandle_.property)+" alterado";
            }
          }
        }
        if(routing.released) {history_.end();colliderDragOpen_=false;state_.activeColliderHandle=0;}
      }
    }
    // Another finger cannot switch selection, tool or camera during the gesture.
    return true;
  }
  const u32 lensHandleBase=widgetId(EditorWidget::CameraHandleBase);
  if(lensDragOpen_ || (routing.widgetId>=lensHandleBase && routing.widgetId<lensHandleBase+3)) {
    if(!lensDragOpen_ && event.phase==UiPointerPhase::Down && !isPlaying() &&
       viewportPointers_.empty() && !history_.isOpen()) {
      const auto *entity=document_.find(state_.selection);
      const u32 kind=routing.widgetId-lensHandleBase;
      if(entity && cameraHandleGeometry(document_,entity->id,kind,lensDragHandle_) &&
         cameraHandleRayParameter(view_,lensDragHandle_,event.position,lensDragStart_) &&
         history_.begin(kind==0?"Ajustar lente":"Ajustar recorte")) {
        lensDragOpen_=true;lensDragPointer_=event.pointerId;lensDragKind_=kind;
        lensDragInitial_=*entity;lensDragView_=view_;
      }
    }
    if(lensDragOpen_ && event.pointerId==lensDragPointer_) {
      if(event.phase==UiPointerPhase::Cancel) {history_.cancel(document_);lensDragOpen_=false;}
      else {
        if(routing.dragging) {
          float parameter;
          if(cameraHandleRayParameter(lensDragView_,lensDragHandle_,event.position,parameter)) {
            auto changed=lensDragInitial_;auto *camera=editCamera(changed);
            if(camera && applyCameraHandleDelta(*camera,lensDragKind_,lensDragHandle_.depth,parameter-lensDragStart_))
              history_.applyValues(document_,changed.id,changed,0xCA02u);
          }
        }
        if(routing.released) {history_.end();lensDragOpen_=false;}
      }
    }
    return true;
  }
  const u32 componentHandleBase=widgetId(EditorWidget::ComponentHandleBase);
  if(componentDragOpen_ || (routing.widgetId>=componentHandleBase &&
                            routing.widgetId<componentHandleBase+static_cast<u32>(EditorComponentHandleKind::Count))) {
    if(!componentDragOpen_ && event.phase==UiPointerPhase::Down && !isPlaying() &&
       viewportPointers_.empty() && !history_.isOpen()) {
      const auto *entity=document_.find(state_.selection);
      const auto kind=static_cast<EditorComponentHandleKind>(routing.widgetId-componentHandleBase);
      if(entity && componentHandleGeometry(document_,entity->id,state_.expandedNative,kind,componentDragHandle_,
                                           probeView_.editing?probeView_.selectedPoint:0) &&
         cameraHandleRayParameter(view_,componentDragHandle_,event.position,componentDragStart_) &&
         history_.begin("Ajustar componente")) {
        componentDragOpen_=true;componentDragPointer_=event.pointerId;
        componentDragInitial_=*entity;componentDragView_=view_;
      }
    }
    if(componentDragOpen_ && event.pointerId==componentDragPointer_) {
      if(event.phase==UiPointerPhase::Cancel) {history_.cancel(document_);componentDragOpen_=false;}
      else {
        if(routing.dragging) {
          float parameter;
          if(cameraHandleRayParameter(componentDragView_,componentDragHandle_,event.position,parameter)) {
            auto changed=componentDragInitial_;
            if(applyComponentHandleDelta(changed,componentDragHandle_,parameter-componentDragStart_))
              history_.applyValues(document_,changed.id,changed,0xCA03u);
          }
        }
        if(routing.released) {history_.end();componentDragOpen_=false;}
      }
    }
    return true;
  }
  if(cameraGestureOpen_ && routing.target==UiPointerTarget::Widget) finishCameraGesture(false);
  if(!state_.numericField && handlePathEditorInput(event,routing)) return true;
  if(handleRigTool(routing)) return true;
  if(state_.colorField) return handleColorWindow(event,routing);
  if(state_.gradientField) return handleGradientEditor(event,routing);
  // PK-9: Inspector do Material por shader (campos, seções, lista, rolagem).
  if(!state_.numericField && state_.materialInspector.valid() && !state_.texturePicker && !isPlaying() &&
     (isMaterialFieldWidget(routing.widgetId) ||
      (routing.widgetId>=widgetId(EditorWidget::MaterialSectionBase) && routing.widgetId<=widgetId(EditorWidget::MaterialChoiceClose))) &&
     handleMaterialFieldPointer(event,routing)) return true;
  if(routing.tapped&&routing.widgetId==animator_widget::id(animator_widget::Open)) {openAnimatorEditor();return true;}
  if(routing.tapped&&navigation_widget::owns(routing.widgetId)) {handleNavigationWidget(routing.widgetId);return true;}
  if(routing.tapped&&probe_widget::owns(routing.widgetId)) {handleProbeWidget(routing.widgetId);return true;}
  if(routing.tapped&&lightmap_widget::owns(routing.widgetId)) {handleLightmapWidget(routing.widgetId);return true;}
  if(routing.tapped&&routing.widgetId==clip_widget::id(clip_widget::Open)) {
    // Animar: abre o primeiro clipe do próprio objeto (componente Animação);
    // sem clipe, cria um e já o toca nele.
    const auto owner=state_.selection?state_.selection:document_.root();std::string error;
    const auto *entity=document_.find(owner);
    const auto *animation=entity?static_cast<const scene::Animation*>(entity->components.find(scene::Animation::descriptor)):nullptr;
    resources::AssetGuid first;
    if(animation)for(const auto &entry:animation->clips)if(animationClipAsset(entry.asset)) {first=entry.asset;break;}
    if(first.valid())openAnimationClip(first,owner,error);
    else if(owner==document_.root()&&!animationClipAssets_.empty())openAnimationClip(animationClipAssets_.front().guid,owner,error);
    else {resources::AssetGuid guid;if(createAnimationClip(owner,2,guid,error)&&(owner==document_.root()||playNewClipOnOwner(owner,guid,error)))openAnimationClip(guid,owner,error);}
    state_.status=error;return true;
  }
  if(state_.clipOpen&&!state_.numericField&&!state_.editingAnimatorName) return handleAnimationClip(event,routing);
  if(state_.animatorOpen&&!state_.numericField&&!state_.editingAnimatorName) return handleAnimatorEditor(event,routing);
  if(state_.curveField && !state_.numericField) return handleCurveEditor(event,routing);
  if(state_.codeRecoveryPending) {
    if(routing.tapped && routing.widgetId==widgetId(EditorWidget::CodeRecover)) {
      if(code_.restoreRecovery(files_)) {
        state_.codeRecoveryPending=false;state_.workspace=EditorWorkspace::Code;state_.codeFiles=false;
        reportProblem(EditorConsoleSeverity::Info,"Rascunhos recuperados. Arquivos alterados externamente continuam protegidos contra sobrescrita.");
      } else reportProblem(EditorConsoleSeverity::Error,code_.error());
    }
    if(routing.tapped && routing.widgetId==widgetId(EditorWidget::CodeDiscardRecovery)) {
      if(code_.discardRecovery(files_)) state_.codeRecoveryPending=false;
      else reportProblem(EditorConsoleSeverity::Error,code_.error());
    }
    return true;
  }
  // R3: o importador é um painel, não uma janela. Os toques nos controles dele
  // são tratados aqui; o resto do editor (viewport, hierarquia, arquivos)
  // continua respondendo enquanto a prévia está aberta.
  if(state_.importPanel && routing.tapped) {
    const auto is=[&routing](EditorWidget widget) {return routing.widgetId==widgetId(widget);};
    if(state_.importBatch) {
      if(is(EditorWidget::ImportCancel)) {state_.importCancel=true;closeImportPreview();}
      return true;
    }
    if(state_.importTexture) {
      if(is(EditorWidget::ImportCancel)) {state_.importCancel=true;closeImportPreview();}
      else if(is(EditorWidget::ImportPreviousPage)) {if(state_.importPage) --state_.importPage;}
      else if(is(EditorWidget::ImportNextPage)) state_.importPage=std::min(textureImportPageCount(state_)-1,state_.importPage+1);
      else if(is(EditorWidget::TextureTiffLosses)&&state_.textureImportTiff)state_.importPage=7;
      else if(is(EditorWidget::ImportAccept) && state_.importReady &&
              resources::sameTexturePreparation(state_.textureImportSettings,state_.textureImportPreparedSettings)) {
        state_.importAccept=true;state_.importReady=false;state_.importStatus="Publicando textura…";
      } else {
        auto &profile=state_.textureImportSettings;
        bool changed=true;
        if(is(EditorWidget::TextureProfileInterpretation)) profile.interpretation=static_cast<u8>((profile.interpretation+1u)%4u);
        else if(is(EditorWidget::TextureProfileDimension)) {
          const auto &steps=resources::TextureDimensionSteps;
          const auto current=std::find(steps.begin(),steps.end(),profile.maximumDimension);
          profile.maximumDimension=current==steps.end()||current+1==steps.end()?steps.front():*(current+1);
        } else if(is(EditorWidget::TextureProfileMipmaps)) profile.mipmaps=!profile.mipmaps;
        else if(is(EditorWidget::TextureProfileEdges)) profile.dilateEdges=!profile.dilateEdges;
        else if(is(EditorWidget::TextureProfileAnisotropy)) profile.anisotropy=!profile.anisotropy;
        else if(is(EditorWidget::TextureProfileNormalGreen)) profile.invertNormalGreen=!profile.invertNormalGreen;
        else if(is(EditorWidget::TextureProfileCoverage)) profile.preserveAlphaCoverage=!profile.preserveAlphaCoverage;
        else if(is(EditorWidget::TextureTiffExtra)&&state_.textureImportTiffUndefinedExtra)
          profile.tiffUndefinedExtra=static_cast<u8>((profile.tiffUndefinedExtra+1)%3);
        else if(is(EditorWidget::TextureTiffPrecision)&&state_.textureImportTiffBits==16)profile.tiffQuantize16=!profile.tiffQuantize16;
        else if(is(EditorWidget::TextureTiffColor)&&state_.textureImportTiffHasIcc&&!state_.textureImportTiffMatrixSrgb)
          profile.tiffIgnoreColorProfile=!profile.tiffIgnoreColorProfile;
        else if(is(EditorWidget::TextureProfileCoverageCutoff)) {
          profile.alphaCoverageCutoff=std::round((profile.alphaCoverageCutoff+.05f)*20.0f)/20.0f;
          if(profile.alphaCoverageCutoff>1.0f) profile.alphaCoverageCutoff=0;
        }
        // Streaming não muda os bytes preparados: vale na publicação, sem repreparar.
        else if(is(EditorWidget::TextureProfileStreaming)) {profile.streamingMipmaps=!profile.streamingMipmaps;changed=false;}
        else if(is(EditorWidget::TextureProfileStreamingPriority)) {
          profile.streamingPriority=nextStreamingPriority(profile.streamingPriority);changed=false;
        }
        else changed=false;
        if(changed) {
          state_.textureImportReprepare=true;state_.importReady=false;state_.importError=false;
          state_.importStatus="Repreparando textura com a receita…";
        }
      }
      return true;
    }
    if(state_.importEnvironment) {
      if(is(EditorWidget::ImportCancel)) {state_.importCancel=true;closeImportPreview();}
      else if(is(EditorWidget::ImportAccept) && state_.importReady) {
        state_.importAccept=true;state_.importReady=false;state_.importStatus="Publicando mapa HDRI…";
      } else {
        const auto step=[&](u32 &value,std::span<const u32> choices,EditorWidget down,EditorWidget up) {
          if(!is(down)&&!is(up)) return false;
          usize index=0;while(index+1<choices.size()&&choices[index]<value) ++index;
          if(is(down)&&index) --index;
          if(is(up)&&index+1<choices.size()) ++index;
          if(value==choices[index]) return true;
          value=choices[index];state_.environmentImportReprepare=true;state_.importReady=false;
          state_.importError=false;state_.importStatus="Repreparando iluminação HDRI…";return true;
        };
        const auto &panorama=resources::EnvironmentPanoramaSteps;
        const auto &specular=resources::EnvironmentSpecularSizeSteps;
        const auto &brdf=resources::EnvironmentBrdfSizeSteps;
        const auto &specularSamples=resources::EnvironmentSpecularSampleSteps;
        const auto &brdfSamples=resources::EnvironmentBrdfSampleSteps;
        const bool handled=step(state_.environmentImportSettings.panoramaWidth,panorama,
             EditorWidget::EnvironmentPanoramaDown,EditorWidget::EnvironmentPanoramaUp)||
        step(state_.environmentImportSettings.specularSize,specular,
             EditorWidget::EnvironmentSpecularDown,EditorWidget::EnvironmentSpecularUp)||
        step(state_.environmentImportSettings.brdfSize,brdf,
             EditorWidget::EnvironmentBrdfDown,EditorWidget::EnvironmentBrdfUp)||
        step(state_.environmentImportSettings.specularSamples,specularSamples,
             EditorWidget::EnvironmentSpecularSamplesDown,EditorWidget::EnvironmentSpecularSamplesUp)||
        step(state_.environmentImportSettings.brdfSamples,brdfSamples,
             EditorWidget::EnvironmentBrdfSamplesDown,EditorWidget::EnvironmentBrdfSamplesUp);
        (void)handled;
      }
      return true;
    }
    using Tab=EditorScreenState::ImportTab;
    // Só o que muda a SAÍDA do importador pede nova preparação: desmarcar um nó
    // na Estrutura não relê o arquivo.
    const bool profileApplied=resources::sameImportPreparation(importProfileDraft(),importProfilePrepared());
    bool handled=true;
    if(is(EditorWidget::ImportTabSummary)) {state_.importTab=Tab::Summary;state_.importPage=0;}
    else if(is(EditorWidget::ImportTabStructure)) {state_.importTab=Tab::Structure;state_.importPage=0;}
    else if(is(EditorWidget::ImportTabMeshes)) {state_.importTab=Tab::Meshes;state_.importPage=0;state_.importMeshDetail=false;}
    else if(is(EditorWidget::ImportTabTextures)) {state_.importTab=Tab::Textures;state_.importPage=0;}
    else if(is(EditorWidget::ImportTabProfile)) {state_.importTab=Tab::Profile;state_.importPage=0;}
    else if(is(EditorWidget::ImportTabRig)) {state_.importTab=Tab::Rig;state_.importPage=0;}
    else if(is(EditorWidget::ImportFbxDeformation))state_.importFbxDeformation=!state_.importFbxDeformation;
    else if(is(EditorWidget::ImportFbxWeights))state_.importFbxReduceWeights=!state_.importFbxReduceWeights;
    else if(is(EditorWidget::ImportFbxSteps))state_.importFbxApproximateSteps=!state_.importFbxApproximateSteps;
    else if(is(EditorWidget::ImportFbxRate))state_.importFbxBakeRate=state_.importFbxBakeRate==30?60:30;
    else if(is(EditorWidget::ImportTakePrevious)) {if(state_.importTake)--state_.importTake;state_.importRigDiagnostic.clear();}
    else if(is(EditorWidget::ImportTakeNext)) {
      if(state_.importTake+1<state_.importTakes.size())++state_.importTake;
      state_.importRigDiagnostic.clear();
    }
    else if(is(EditorWidget::ImportTakeRoot)) {
      if(!state_.importNodes.empty())state_.importTakeRoot=(state_.importTakeRoot+1)%static_cast<u32>(state_.importNodes.size());
      state_.importRigDiagnostic.clear();
    }
    else if(is(EditorWidget::ImportTakeExtract)&&state_.importPublishedView) {
      resources::AssetGuid guid;std::string diagnostic;
      if(extractModelTake(state_.importTakeSource,state_.importTake,state_.importTakeRoot,state_.selection,guid,diagnostic)) {
        closeImportPreview();if(!openAnimationClip(guid,state_.selection,diagnostic))state_.status=diagnostic;
      } else state_.importRigDiagnostic=diagnostic;
    }
    else if(is(EditorWidget::ImportMeshClose)) state_.importMeshDetail=false;
    else if(routing.widgetId>=widgetId(EditorWidget::ImportMeshRowBase) &&
            routing.widgetId<widgetId(EditorWidget::ImportMeshRowBase)+state_.importMeshes.size()) {
      state_.importMeshSelected=routing.widgetId-widgetId(EditorWidget::ImportMeshRowBase);
      state_.importMeshDetail=true;
    }
    else if(is(EditorWidget::ImportPreviousPage)) {if(state_.importPage) --state_.importPage;}
    else if(is(EditorWidget::ImportNextPage)) ++state_.importPage;
    else if(is(EditorWidget::ImportCancel)) {state_.importCancel=!state_.importError||state_.importRetryable;closeImportPreview();}
    else if(is(EditorWidget::ImportMatchInOrder)) state_.importAmbiguityChoice=1;
    else if(is(EditorWidget::ImportTreatAsNew)) state_.importAmbiguityChoice=2;
    else if(is(EditorWidget::ImportScaleDown) || is(EditorWidget::ImportScaleUp)) {
      auto step=resources::nearestImportScaleStep(state_.importScale);
      if(is(EditorWidget::ImportScaleDown) && step>0) --step;
      if(is(EditorWidget::ImportScaleUp) && step+1<resources::ImportScaleSteps.size()) ++step;
      state_.importScale=resources::ImportScaleSteps[step];
    }
    else if(is(EditorWidget::ImportTextureDimension256)) state_.importTextureDimension=256;
    else if(is(EditorWidget::ImportTextureDimension512)) state_.importTextureDimension=512;
    else if(is(EditorWidget::ImportTextureDimension1024)) state_.importTextureDimension=1024;
    else if(is(EditorWidget::ImportTextureDimension2048)) state_.importTextureDimension=2048;
    else if(is(EditorWidget::ImportNormalsCycle))
      state_.importNormals=state_.importNormals==resources::GltfNormalsImport?resources::GltfNormalsCalculate:resources::GltfNormalsImport;
    else if(is(EditorWidget::ImportNormalWeightingCycle))
      state_.importNormalWeighting=state_.importNormalWeighting==resources::GltfNormalWeightArea?
          resources::GltfNormalWeightAngle:resources::GltfNormalWeightArea;
    else if(is(EditorWidget::ImportTangentsCycle))
      state_.importTangents=state_.importTangents==resources::GltfTangentsImport?resources::GltfTangentsCalculate:resources::GltfTangentsImport;
    else if(is(EditorWidget::ImportCamerasToggle)) state_.importCameras=!state_.importCameras;
    else if(is(EditorWidget::ImportLightsToggle)) state_.importLights=!state_.importLights;
    else if(is(EditorWidget::ImportTextureCompressionCycle)) {
      // 6x6 -> 4x4 -> 8x8 -> sem -> 6x6: começa no padrão e vai para qualidade, economia, desligado.
      const u8 value=state_.importTextureCompression;
      state_.importTextureCompression=value==6?4:value==4?8:value==8?0:6;
    }
    else if(is(EditorWidget::ImportTextureStreamingToggle)) state_.importTextureStreaming=!state_.importTextureStreaming;
    else if(is(EditorWidget::ImportGenerateLodsToggle)) state_.importGenerateLods=!state_.importGenerateLods;
    else if(is(EditorWidget::ImportLodLevelsCycle) && state_.importGenerateLods)
      state_.importLodLevels=static_cast<u8>(state_.importLodLevels>=4?2:state_.importLodLevels+1);
    else if(is(EditorWidget::ImportOptimizeOrderToggle)) state_.importOptimizeOrder=!state_.importOptimizeOrder;
    else if(is(EditorWidget::ImportTextureStreamingPriorityCycle))
      state_.importTextureStreamingPriority=nextStreamingPriority(state_.importTextureStreamingPriority);
    // Passos de 15°: o controle deslizante da Unity em toque, sem arrasto fino.
    else if(is(EditorWidget::ImportSmoothingDown)) state_.importSmoothingAngle=state_.importSmoothingAngle>=15?state_.importSmoothingAngle-15:0;
    else if(is(EditorWidget::ImportSmoothingUp)) state_.importSmoothingAngle=std::min(180u,state_.importSmoothingAngle+15);
    else if(routing.widgetId>=widgetId(EditorWidget::ImportNodeToggleBase) &&
            routing.widgetId<widgetId(EditorWidget::ImportNodeToggleBase)+65536)
      toggleImportNodeExclusion(routing.widgetId-widgetId(EditorWidget::ImportNodeToggleBase));
    else if(is(EditorWidget::ImportApplyProfile)) {
      if((state_.importReady||state_.importRetryable) && !state_.importPublishedView && (!profileApplied||state_.importRetryable)) {
        state_.importReprepare=true;state_.importReady=false;state_.importRetryable=false;state_.importError=false;state_.importStatus="Preparando com o perfil…";
      }
    }
    else if(is(EditorWidget::ImportSaveDefaultProfile))
      setImportStatus(saveProjectImportProfile(importProfileDraft())?"Perfil salvo como padrão do projeto":
                      "Não foi possível salvar o perfil padrão do projeto",
                      EditorConsoleSeverity::Info);
    else if(is(EditorWidget::ImportAccept) || is(EditorWidget::ImportIntoScene)) {
      // Publicar só com a escolha explícita das ambiguidades e com a prévia do
      // perfil que está na tela.
      if(state_.importReady && profileApplied && (!state_.importAmbiguities || state_.importAmbiguityChoice)) {
        state_.importIntoScene=is(EditorWidget::ImportIntoScene);
        state_.importAccept=true;state_.importReady=false;state_.importStatus="Publicando recurso…";
      }
    }
    else if(routing.tapped && lightmap_widget::owns(routing.widgetId)) handleLightmapWidget(routing.widgetId);
    else handled=false;
    if(handled) return true;
  }
  // O console responde PRIMEIRO.
  //
  // Ele e desenhado por cima do editor de codigo e da cena, e a cadeia abaixo
  // tem varios blocos que consomem o toque por workspace. Um painel que aparece
  // em cima e decidido embaixo e um painel que so funciona por acidente.
  if(state_.console) {
    const auto widget=routing.widgetId;
    if(widget==widgetId(EditorWidget::ConsoleResize) && routing.dragging) {
      state_.consoleCollapsed=false;state_.consoleExpanded=false;
      state_.consoleFraction=std::clamp(state_.consoleFraction-routing.stepDelta.y/std::max(1.0f,layout_.consolePanel.height/state_.consoleFraction),.25f,.82f);
      return true;
    }
    if(event.phase==UiPointerPhase::Down) state_.consoleDragRemainder=0;
    if(routing.dragging && widget>=widgetId(EditorWidget::ConsoleRowBase) &&
        widget-widgetId(EditorWidget::ConsoleRowBase)<EditorConsole::Capacity) {
      // Rolar anda a partir do FIM: zero e a linha mais nova.
      const auto rows=layout_.consoleRowCount,visible=layout_.consoleVisibleRows;
      const auto maximum=rows>visible?rows-visible:0u;
      state_.consoleDragRemainder+=routing.stepDelta.y;
      const int steps=static_cast<int>(state_.consoleDragRemainder/56);
      if(!steps) return true;
      state_.consoleDragRemainder-=steps*56;
      const int next=static_cast<int>(state_.consoleScroll)+steps;
      state_.consoleScroll=static_cast<u32>(std::clamp(next,0,static_cast<int>(maximum)));
      state_.consoleFollow=state_.consoleScroll==0;
      const auto order=console_.filtered(state_.consoleProblems?1:0,state_.consoleOrigin,state_.consoleQuery);
      const auto first=maximum>state_.consoleScroll?maximum-state_.consoleScroll:0;
      if(first<order.size()) state_.consoleAnchor=console_.at(order[first])->eventId;
      return true;
    }
    if(routing.tapped) {
      if(widget==widgetId(EditorWidget::ConsoleInfo)) {console_.toggle(EditorConsoleSeverity::Info);return true;}
      if(widget==widgetId(EditorWidget::ConsoleWarning)) {console_.toggle(EditorConsoleSeverity::Warning);return true;}
      if(widget==widgetId(EditorWidget::ConsoleError)) {console_.toggle(EditorConsoleSeverity::Error);return true;}
      if(widget==widgetId(EditorWidget::ConsoleClear)) {console_.clearLogs();state_.consoleScroll=0;return true;}
      if(widget==widgetId(EditorWidget::ConsoleCollapse)) {state_.consoleCollapsed=!state_.consoleCollapsed;return true;}
      if(widget==widgetId(EditorWidget::ConsoleExpand)) {state_.consoleExpanded=!state_.consoleExpanded;return true;}
      if(widget==widgetId(EditorWidget::ConsoleProblems) || widget==widgetId(EditorWidget::ConsoleLogs)) {
        state_.consoleProblems=widget==widgetId(EditorWidget::ConsoleProblems);state_.consoleOrigin=-1;
        state_.consoleScroll=0;state_.consoleAnchor=0;state_.consoleSelected=0;return true;
      }
      if(widget==widgetId(EditorWidget::ConsoleSource)) {
        state_.consoleOrigin=state_.consoleOrigin==3?-1:state_.consoleOrigin+1;
        state_.consoleScroll=0;state_.consoleAnchor=0;return true;
      }
      if(widget==widgetId(EditorWidget::ConsoleSearch)) {state_.searchingConsole=true;return true;}
      if(widget==widgetId(EditorWidget::ConsoleFollow)) {
        state_.consoleFollow=!state_.consoleFollow;
        if(state_.consoleFollow) {state_.consoleScroll=0;state_.consoleAnchor=0;}
        else {
          const auto order=console_.filtered(state_.consoleProblems?1:0,state_.consoleOrigin,state_.consoleQuery);
          const auto first=order.size()>layout_.consoleVisibleRows?order.size()-layout_.consoleVisibleRows:0;
          if(first<order.size()) state_.consoleAnchor=console_.at(order[first])->eventId;
        }
        return true;
      }
      if(widget==widgetId(EditorWidget::ConsoleDetailClose)) {state_.consoleSelected=0;return true;}
      if(widget==widgetId(EditorWidget::ConsoleDetailPrevious)) {
        if(state_.consoleDetailPage) --state_.consoleDetailPage;
        return true;
      }
      if(widget==widgetId(EditorWidget::ConsoleDetailNext)) {++state_.consoleDetailPage;return true;}
      if(widget==widgetId(EditorWidget::ConsoleCopy)) {
        if(const auto *entry=console_.find(state_.consoleSelected)) consoleCopy_=EditorConsole::describe(*entry);
        return true;
      }
      if(widget==widgetId(EditorWidget::ConsoleOpenSource)) {
        for(u32 i=0;i<console_.entries().size();++i) if(console_.at(i)->eventId==state_.consoleSelected) {
          jumpToConsoleEntry(i);state_.consoleExpanded=false;break;
        }
        return true;
      }
      if(widget==widgetId(EditorWidget::ConsoleExport)) {
        std::string text="ASTRA · Console\nTempos relativos ao início desta sessão. Repetições consecutivas agrupadas.\n";
        for(const auto i:console_.filtered(state_.consoleProblems?1:0,state_.consoleOrigin,state_.consoleQuery))
          text+='\n'+EditorConsole::describe(*console_.at(i))+"\n";
        const std::string directory="Logs";
        const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const auto path=directory+"/console-"+std::to_string(now)+".txt";
        if((files_.exists(directory)||files_.createDirectory(directory)) && files_.createTextFile(path,text)) {
          files_.rebuildTree();state_.selectedFile=path;state_.status="Console exportado: "+path;
          reportProblem(EditorConsoleSeverity::Info,state_.status);
        } else reportProblem(EditorConsoleSeverity::Error,"Não foi possível exportar: "+files_.error());
        return true;
      }
      const auto base=widgetId(EditorWidget::ConsoleRowBase);
      if(widget>=base && widget-base<console_.entries().size()) {
        state_.consoleSelected=console_.at(widget-base)->eventId;state_.consoleDetailPage=0;return true;
      }
    }
  }
  if(state_.workspace==EditorWorkspace::Code && routing.widgetId==widgetId(EditorWidget::CodeBody)) {
    if(routing.dragging) if(auto *buffer=code_.active()) {
      const auto count=static_cast<u32>(std::count(buffer->text.begin(),buffer->text.end(),'\n'));
      const int next=static_cast<int>(buffer->firstLine)-static_cast<int>(routing.stepDelta.y/12);
      buffer->firstLine=std::min(count,static_cast<u32>(std::max(0,next)));
    }
    if(routing.tapped) {
      state_.editingCode=code_.active()!=nullptr;
      if(state_.editingCode) placeCodeCaret(routing.position);
    }
    return true;
  }
  if(routing.dragging && state_.texturePicker) {
    const u32 key=routing.widgetId;
    const bool control=key==widgetId(EditorWidget::TexturePickerScroll) || key==widgetId(EditorWidget::TextureUseInherited) ||
        key==widgetId(EditorWidget::TextureUseNone) || key==widgetId(EditorWidget::TextureSamplingUv) ||
        key==widgetId(EditorWidget::TextureSamplingWrap) || key==widgetId(EditorWidget::TextureSamplingFilter) ||
        key==widgetId(EditorWidget::TextureUvReset) ||
        (key>=widgetId(EditorWidget::TextureUvStepBase) && key<widgetId(EditorWidget::TextureUvStepBase)+10) ||
        (key>=widgetId(EditorWidget::TextureChoiceBase) && key<widgetId(EditorWidget::TextureChoiceBase)+state_.projectTextureNames.size()) ||
        (key>=widgetId(EditorWidget::TextureViewBase) && key<widgetId(EditorWidget::TextureViewBase)+state_.projectTextureNames.size());
    if(control) {
      const u32 scope=previewSecondary_?1u:0u;
      const float limit=std::max(0.f,layout_.texturePickerContent[scope]-layout_.texturePickerWindow[scope]);
      state_.texturePickerScroll=std::clamp(state_.texturePickerScroll-routing.stepDelta.y,0.f,limit);return true;
    }
  }
  if(handleLibraryScroll(routing)) return true;
  if(handleInspectorScroll(event,routing)) return true;
  // A aba de componentes usa arraste para navegar, não para reordenar por
  // acidente. A ordem continua disponível pelo menu de cada componente.
  if(routing.dragging && state_.inspectorSurface==EditorInspectorSurface::Components &&
     layout_.componentOverviewWindow.contains(routing.start)) {
    const float limit=std::max(0.f,layout_.componentOverviewContent-layout_.componentOverviewWindow.height);
    state_.componentOverviewScroll=std::clamp(state_.componentOverviewScroll-routing.stepDelta.y,0.f,limit);
    return true;
  }
  // Inspector de vários recursos: arrastar em qualquer ponto dele rola.
  if(routing.dragging && state_.multiAsset.items.size()>1 && routing.widgetId>=widgetId(EditorWidget::MultiAssetApply) &&
     routing.widgetId<widgetId(EditorWidget::FilesMultiToggle)+detail::kRange) {
    const float limit=std::max(0.0f,layout_.multiAssetContent-layout_.multiAssetWindow);
    state_.multiAssetScroll=std::clamp(state_.multiAssetScroll-routing.stepDelta.y,0.0f,limit);
    return true;
  }
  // Inspector de textura: arrastar sobre os cartões (fundo ou controle) rola a
  // janela entre o cabeçalho e o rodapé; o toque curto continua sendo toque.
  if(routing.dragging && state_.textureViewer && (state_.textureInspector||state_.selection!=kInvalidEntity)) {
    const u32 key=routing.widgetId;
    const auto within=[&](EditorWidget first,EditorWidget last) {return key>=widgetId(first) && key<=widgetId(last);};
    const bool card=(key>=widgetId(EditorWidget::TextureInspectorBase) && key<widgetId(EditorWidget::TextureInspectorBase)+0x100u &&
                     key!=widgetId(EditorWidget::TextureInspectorBase)+detail::TextureInspectorExpandClose) ||
                    (key>=widgetId(EditorWidget::TextureUserBase) && key<widgetId(EditorWidget::TextureUserBase)+0x10000u) ||
                    within(EditorWidget::TextureViewerChannel,EditorWidget::TextureViewerBackground) ||
                    within(EditorWidget::TextureProfileInterpretation,EditorWidget::TextureProfileStreamingPriority);
    if(card && !state_.textureViewerExpanded) {
      const float limit=std::max(0.0f,layout_.textureInspectorContent-layout_.textureInspectorWindow);
      state_.textureInspectorScroll=std::clamp(state_.textureInspectorScroll-routing.stepDelta.y,0.0f,limit);
      return true;
    }
  }
  if(routing.tapped && !isPlaying()) {
    const auto presetKey=routing.widgetId;
    if(presetKey==widgetId(EditorWidget::PresetOpen)) {
      // Vindo da folha do Add, o painel de presets toma o lugar dela.
      const bool fromAdd=state_.addingComponent;
      if(openComponentPresets(state_.selection,fromAdd?0:state_.nativeMenu) && fromAdd) state_.addingComponent=false;
      return true;
    }
    if(state_.presetPanel && ((presetKey>=widgetId(EditorWidget::PresetClose)&&presetKey<=widgetId(EditorWidget::PresetRecipeNext)) ||
        (presetKey>=widgetId(EditorWidget::PresetChoiceBase)&&presetKey<widgetId(EditorWidget::PresetChoiceBase)+256) ||
        (presetKey>=widgetId(EditorWidget::PresetFieldBase)&&presetKey<widgetId(EditorWidget::PresetFieldBase)+4096) ||
        (presetKey>=widgetId(EditorWidget::PresetInputBase)&&presetKey<widgetId(EditorWidget::PresetInputBase)+256))) {
      if(state_.selection!=state_.presetEntity || state_.presetEpoch!=sceneEpoch_) {state_.presetPanel=false;return true;}
      if(presetKey==widgetId(EditorWidget::PresetClose)) {state_.presetPanel=false;state_.presetNaming=false;return true;}
      if(presetKey==widgetId(EditorWidget::PresetLibrary)) {state_.presetSelected=0;state_.presetInputValues.clear();refreshComponentPresets();return true;}
      if(presetKey==widgetId(EditorWidget::PresetRecipeDetails)) {state_.presetRecipeDetails=!state_.presetRecipeDetails;state_.presetRecipePage=0;return true;}
      if(presetKey==widgetId(EditorWidget::PresetRecipePrevious)) {if(state_.presetRecipePage)--state_.presetRecipePage;return true;}
      if(presetKey==widgetId(EditorWidget::PresetRecipeNext)) {++state_.presetRecipePage;return true;}
      if(presetKey==widgetId(EditorWidget::PresetPrevious)) {if(state_.presetPage) --state_.presetPage;return true;}
      if(presetKey==widgetId(EditorWidget::PresetNext)) {if((state_.presetPage+1)*4<state_.presetChoices.size()) ++state_.presetPage;return true;}
      if(presetKey>=widgetId(EditorWidget::PresetInputBase)&&presetKey<widgetId(EditorWidget::PresetInputBase)+256) {
        openRecipeInput(presetKey-widgetId(EditorWidget::PresetInputBase));return true;
      }
      // Escolha por campo: marcar, desmarcar e paginar o diff não tocam a cena.
      if(presetKey>=widgetId(EditorWidget::PresetFieldBase)) {
        const auto index=presetKey-widgetId(EditorWidget::PresetFieldBase);
        if(index<state_.presetFields.size()&&state_.presetFields[index].applicable)
          state_.presetFields[index].selected=!state_.presetFields[index].selected;
        return true;
      }
      if(presetKey==widgetId(EditorWidget::PresetSelectAll)||presetKey==widgetId(EditorWidget::PresetSelectNone)) {
        const bool value=presetKey==widgetId(EditorWidget::PresetSelectAll);
        for(auto &field:state_.presetFields) if(field.applicable) field.selected=value;
        return true;
      }
      if(presetKey==widgetId(EditorWidget::PresetFieldsPrevious)) {if(state_.presetFieldPage) --state_.presetFieldPage;return true;}
      if(presetKey==widgetId(EditorWidget::PresetFieldsNext)) {
        // Quantas linhas cabem por página depende da altura do painel, que é da
        // tela; aqui só se garante que a página nunca passa do número de linhas
        // (uma por página no pior caso). A tela limita a página ao total real.
        if(state_.presetFieldPage+1<state_.presetFields.size()) ++state_.presetFieldPage;
        return true;
      }
      if(presetKey==widgetId(EditorWidget::PresetSave)||presetKey==widgetId(EditorWidget::PresetRename)||
         presetKey==widgetId(EditorWidget::PresetSaveRecipe)) {
        state_.presetRenaming=presetKey==widgetId(EditorWidget::PresetRename);
        state_.presetRecipeNaming=presetKey==widgetId(EditorWidget::PresetSaveRecipe);
        const auto *entry=componentPresets_.find(state_.presetSelected);
        state_.presetName=state_.presetRenaming&&entry?entry->name:document_.find(state_.presetEntity)->name;
        std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",state_.presetName.c_str());
        state_.presetNaming=true;return true;
      }
      if(presetKey>=widgetId(EditorWidget::PresetChoiceBase)) {
        const auto index=presetKey-widgetId(EditorWidget::PresetChoiceBase);
        if(index<state_.presetChoices.size()) {state_.presetSelected=state_.presetChoices[index].first;state_.presetDeleteConfirm=false;state_.presetInputValues.clear();state_.presetRecipeDetails=false;state_.presetRecipePage=0;refreshComponentPresets();}
        return true;
      }
      std::string error;
      if(presetKey==widgetId(EditorWidget::PresetDelete)) {
        if(!state_.presetDeleteConfirm) {state_.presetDeleteConfirm=true;return true;}
        const bool erased=componentPresets_.erase(state_.presetSelected,error);state_.status=erased?"Preset excluído":error;
        if(erased) {state_.presetSelected=0;refreshComponentPresets();}state_.presetDeleteConfirm=false;return true;
      }
      // Uma receita não tem "aplicar valores" e "adicionar" separados: ela
      // resolve os dois de uma vez, componente a componente, na transação.
      const bool applied=state_.presetSelectedIsRecipe?
        applyComponentRecipe(state_.presetSelected,state_.presetEntity,{state_.presetEpoch,state_.presetRevision},state_.presetInputValues,error):
        applyComponentPreset(state_.presetSelected,state_.presetEntity,state_.presetInstance,
          {state_.presetEpoch,state_.presetRevision},presetKey==widgetId(EditorWidget::PresetAdd),error);
      state_.status=error;if(applied) {state_.presetPanel=false;state_.nativeMenu=0;state_.addingComponent=false;}return true;
    }
    const auto impactKey=routing.widgetId;
    if(impactKey==widgetId(EditorWidget::ImpactClose)) {
      if(state_.impactRepair) {state_.impactRepair=false;state_.impactReplacement={};state_.impactRepairMaterial={};state_.impactPage=0;return true;}
      if(!state_.impactTrail.empty()) {
        const auto previous=state_.impactTrail.back();state_.impactTrail.pop_back();
        state_.impactAsset=previous.first;state_.impactPage=previous.second;
      } else {state_.impactInstance=0;state_.impactAsset={};state_.impactRemoval=false;}
      return true;
    }
    if(impactKey==widgetId(EditorWidget::ImpactPrevious)) {if(state_.impactPage)--state_.impactPage;return true;}
    if(impactKey==widgetId(EditorWidget::ImpactNext)) {++state_.impactPage;return true;}
    if(impactKey==widgetId(EditorWidget::ImpactRemoveConfirm)) {
      if(!state_.impactRemoval || state_.impactAsset.valid() || state_.selection!=state_.impactEntity) return true;
      const auto version=EditorSceneVersion{state_.impactRemovalEpoch,state_.impactRemovalRevision};
      if(sceneVersion().epoch!=version.epoch || sceneVersion().revision!=version.revision) {
        state_.status="Cena alterada; reabra a remoção";return true;
      }
      EditorActionRequest request;request.action=EditorAction::RemoveComponent;
      request.entity=state_.impactEntity;request.componentInstance=state_.impactInstance;request.version=version;
      if(dispatch(request).status==EditorActionStatus::Applied) {
        if(state_.expandedNative==request.componentInstance) state_.expandedNative=0;
        if(state_.expandedScript==request.componentInstance) state_.expandedScript=0;
        state_.nativeMenu=0;state_.scriptMenu=0;state_.impactInstance=0;state_.impactAsset={};state_.impactTrail.clear();state_.impactRemoval=false;
        state_.status="Componente removido";
      } else state_.status="Remoção bloqueada; revise as dependências";
      return true;
    }
    if(impactKey==widgetId(EditorWidget::ImpactRepairScope)&&state_.impactRepair&&!state_.impactRepairMaterial.valid()) {
      state_.impactRepairScene=!state_.impactRepairScene;state_.impactPage=0;return true;
    }
    if(impactKey==widgetId(EditorWidget::ImpactRepair)||impactKey==widgetId(EditorWidget::ImpactRepairShared)) {
      state_.impactRepairMaterial=impactKey==widgetId(EditorWidget::ImpactRepairShared)?repairMaterialContext(state_.impactTrail,state_.impactAsset,&mapScene_):resources::AssetGuid{};
      const auto *material=findMaterialAsset(state_.impactRepairMaterial);
      if(impactKey==widgetId(EditorWidget::ImpactRepairShared)&&!material) return true;
      state_.impactRepairMaterialRevision=material?material->revision:0;
      state_.impactRepairScene=false;
      state_.impactRepair=true;state_.impactReplacement={};state_.impactRepairRevision=document_.revision();state_.impactRepairEpoch=sceneEpoch_;state_.impactPage=0;return true;
    }
    if(impactKey==widgetId(EditorWidget::ImpactRepairApply)) {
      std::string diagnostic;
      const auto version=EditorSceneVersion{state_.impactRepairEpoch,state_.impactRepairRevision};
      const bool applied=state_.impactRepairMaterial.valid()?
          repairSharedTexture(state_.impactRepairMaterial,state_.impactAsset,state_.impactReplacement,state_.impactRepairMaterialRevision,version,diagnostic):
          state_.impactRepairScene?repairSceneResource(state_.impactAsset,state_.impactReplacement,version,diagnostic):
          repairComponentResource(state_.impactEntity,state_.impactInstance,state_.impactAsset,state_.impactReplacement,version,diagnostic);
      if(applied) {
        state_.impactAsset=state_.impactReplacement;state_.impactRepair=false;state_.impactReplacement={};state_.impactRepairMaterial={};state_.impactPage=0;
      }
      state_.status=diagnostic;return true;
    }
    if(impactKey>=widgetId(EditorWidget::ImpactOpenBase)&&impactKey<widgetId(EditorWidget::ImpactOpenBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);const auto index=impactKey-widgetId(EditorWidget::ImpactOpenBase);
      if(entity&&index<entity->components.size()) {state_.impactEntity=entity->id;state_.impactInstance=entity->components.at(index)->instanceId();state_.impactPage=0;state_.impactAsset={};state_.impactTrail.clear();state_.impactRepair=false;state_.impactRemoval=false;state_.impactReplacement={};state_.impactRepairMaterial={};}
      return true;
    }
    const bool removeNative=impactKey>=widgetId(EditorWidget::ComponentRemoveBase)&&impactKey<widgetId(EditorWidget::ComponentRemoveBase)+0x01000000u;
    const bool removeScript=impactKey>=widgetId(EditorWidget::ScriptRemoveBase)&&impactKey<widgetId(EditorWidget::ScriptRemoveBase)+0x01000000u;
    if(removeNative || removeScript) {
      const auto *entity=document_.find(state_.selection);
      const auto index=impactKey-widgetId(removeNative?EditorWidget::ComponentRemoveBase:EditorWidget::ScriptRemoveBase);
      const auto *component=entity?entity->components.at(index):nullptr;
      if(component && (removeNative || scene::scriptBehavior(component))) {
        state_.impactEntity=entity->id;state_.impactInstance=component->instanceId();
        const auto version=sceneVersion();state_.impactRemovalEpoch=version.epoch;state_.impactRemovalRevision=version.revision;
        state_.impactPage=0;state_.impactAsset={};state_.impactTrail.clear();state_.impactRepair=false;
        state_.impactRemoval=true;state_.impactReplacement={};state_.impactRepairMaterial={};state_.nativeMenu=0;state_.scriptMenu=0;
      }
      return true;
    }
    if(impactKey>=widgetId(EditorWidget::ImpactRowBase)&&impactKey<widgetId(EditorWidget::ImpactRowBase)+0x01000000u) {
      if(state_.impactRepair) {
        if(state_.impactReplacement.valid()) return true;
        const auto *object=document_.find(state_.impactEntity);
        const auto shared=sharedTextureBindings(state_.impactRepairMaterial,&mapScene_);
        const auto choices=resourceRepairChoices(state_.impactRepairMaterial.valid()?&shared:object?object->components.findInstance(state_.impactInstance):nullptr,state_.impactAsset,&assets_,&mapScene_);
        const auto index=impactKey-widgetId(EditorWidget::ImpactRowBase);
        if(index<choices.size()) {state_.impactReplacement=choices[index].asset;state_.impactPage=0;}
        return true;
      }
      const auto entries=state_.impactAsset.valid()?resourceImpact(document_,state_.impactAsset,&assets_,&mapScene_):
          componentImpact(document_,state_.impactEntity,state_.impactInstance,&assets_,&mapScene_);const auto index=impactKey-widgetId(EditorWidget::ImpactRowBase);
      if(index<entries.size()&&entries[index].asset.valid()) {
        if(state_.impactTrail.size()>=64) {state_.status="Volte para abrir outro recurso";return true;}
        state_.impactTrail.push_back({state_.impactAsset,state_.impactPage});state_.impactAsset=entries[index].asset;state_.impactPage=0;
        return true;
      }
      if(index<entries.size()&&document_.find(entries[index].object)) {
        setSelection(entries[index].object);state_.componentSelection=entries[index].object;
        state_.expandedNative=entries[index].instance;state_.nativeMenu=0;state_.propertyPage=0;state_.impactInstance=0;state_.impactAsset={};state_.impactTrail.clear();state_.impactRepair=false;state_.impactReplacement={};state_.impactRepairMaterial={};
      }
      return true;
    }

    if(routing.widgetId==widgetId(EditorWidget::CameraPreviewPin)) {
      cameraPreview_.pin(document_,sceneVersion(),state_.selection);return true;
    }
    if(routing.widgetId==widgetId(EditorWidget::CameraPreviewClose)) {cameraPreview_.close();return true;}
    if(routing.widgetId==widgetId(EditorWidget::CameraPreviewRetry)) {cameraPreview_.invalidateTarget();return true;}
    if(routing.widgetId==widgetId(EditorWidget::MaterialPreviewShape)) {cameraPreview_.changeShape();return true;}
    if(routing.widgetId>=widgetId(EditorWidget::MaterialPreviewLeft)&&routing.widgetId<=widgetId(EditorWidget::MaterialPreviewFar)) {
      const auto key=routing.widgetId;
      cameraPreview_.orbit(key==widgetId(EditorWidget::MaterialPreviewLeft)?-.3f:key==widgetId(EditorWidget::MaterialPreviewRight)?.3f:0,
        key==widgetId(EditorWidget::MaterialPreviewUp)?-.2f:key==widgetId(EditorWidget::MaterialPreviewDown)?.2f:0,
        key==widgetId(EditorWidget::MaterialPreviewNear)?.8f:key==widgetId(EditorWidget::MaterialPreviewFar)?1.25f:1);
      return true;
    }
    if(routing.widgetId==widgetId(EditorWidget::CameraPreviewResolution) ||
       routing.widgetId==widgetId(EditorWidget::CameraPreviewFrequency)) {
      auto width=cameraPreview_.width();auto height=cameraPreview_.height();
      float frequency=cameraPreview_.frequency();
      if(routing.widgetId==widgetId(EditorWidget::CameraPreviewResolution)) {
        width=width<640?640:width<960?960:320;height=width*9/16;
      } else frequency=frequency<15?15:frequency<30?30:5;
      renderer::PreviewViewBudget budget;
      budget.maximumWidth=width;budget.maximumHeight=height;
      budget.maximumPixels=static_cast<u64>(width)*height;budget.updatesPerSecond=frequency;
      cameraPreview_.configure(width,height,budget);return true;
    }
    // R4: textura em Propriedades e gerenciador de texturas.
    {
      const auto key=routing.widgetId;
      if(key==widgetId(EditorWidget::TextureManagerClose)) {state_.textureManager=false;return true;}
      if(key==widgetId(EditorWidget::TextureSearch) && state_.textureManager) {state_.searchingTextures=true;return true;}
      if(key>=widgetId(EditorWidget::TextureFilterBase) && key<widgetId(EditorWidget::TextureFilterBase)+TextureFilterCount) {
        setTextureFilter(static_cast<u8>(key-widgetId(EditorWidget::TextureFilterBase)));return true;
      }
      if(key==widgetId(EditorWidget::TextureManagerPrevious)) {if(state_.textureManagerPage) --state_.textureManagerPage;return true;}
      if(key==widgetId(EditorWidget::TextureManagerNext)) {++state_.textureManagerPage;return true;}
      // Bloco F: texturas das fontes no mesmo gerenciador.
      if(key==widgetId(EditorWidget::TextureManagerShowProject) || key==widgetId(EditorWidget::TextureManagerShowSources)) {
        state_.textureManagerSources=key==widgetId(EditorWidget::TextureManagerShowSources);
        state_.textureManagerPage=0;return true;
      }
      if(key>=widgetId(EditorWidget::TextureManagerRowBase)+detail::TextureManagerSourceRowOffset &&
         key-widgetId(EditorWidget::TextureManagerRowBase)-detail::TextureManagerSourceRowOffset<sourceTextures_.size()) {
        if(!openSourceTextureInspector(key-widgetId(EditorWidget::TextureManagerRowBase)-detail::TextureManagerSourceRowOffset))
          state_.status=state_.textureViewerInfo;
        return true;
      }
      if(key==widgetId(EditorWidget::TextureSourceShowOrigin) && state_.textureViewerSource &&
         state_.textureViewerSourceIndex<sourceTextures_.size()) {
        // A fonte é onde ficam perfil (compressão, LOD, streaming) e Reimportar.
        const auto path=sourceTextures_[state_.textureViewerSourceIndex].sourcePath;
        closeTextureViewer();state_.textureInspector=false;state_.textureManager=false;
        state_.selectedFile=path;
        state_.status="Fonte selecionada em Arquivos · Reimportar abre o perfil de importação";
        return true;
      }
      if(key==widgetId(EditorWidget::TextureSourceShowAll)) {
        const bool fromSource=state_.textureViewerSource;
        closeTextureViewer();
        openTextureManager();
        state_.textureManagerSources=fromSource;
        return true;
      }
      // Inspector de textura em cartões.
      if(key>=widgetId(EditorWidget::TextureInspectorBase) && key<widgetId(EditorWidget::TextureInspectorBase)+0x100u && state_.textureViewer) {
        const u32 sub=key-widgetId(EditorWidget::TextureInspectorBase);
        if(sub<detail::TextureInspectorCopy) {state_.textureCardsClosed^=1u<<sub;return true;}
        if(sub<detail::TextureInspectorCrumb) {
          const u32 row=sub-detail::TextureInspectorCopy;
          if(row<state_.textureOrigin.size() && !state_.textureOrigin[row].copy.empty()) {
            consoleCopy_=state_.textureOrigin[row].copy;
            state_.status=state_.textureOrigin[row].label+" copiado: "+consoleCopy_;
          }
          return true;
        }
        if(sub<detail::TextureInspectorScroll) {
          const u32 depth=sub-detail::TextureInspectorCrumb;
          std::string folder;
          for(u32 i=0;i<=depth && i<state_.textureBreadcrumb.size();++i) folder+=(i?"/":"")+state_.textureBreadcrumb[i];
          if(!folder.empty() && revealInFiles(folder)) state_.status="Pasta aberta em Arquivos: "+folder;
          return true;
        }
        if(sub==detail::TextureInspectorExpand) {state_.textureViewerExpanded=!state_.textureViewerImage.isEmpty();return true;}
        if(sub==detail::TextureInspectorExpandClose) {state_.textureViewerExpanded=false;return true;}
        if(sub==detail::TextureInspectorUsersMore) {state_.textureUsersExpanded=!state_.textureUsersExpanded;return true;}
        if(sub==detail::TextureInspectorGpuInfo) {state_.status=state_.textureGpuNote;return true;}
        if(sub==detail::TextureInspectorLocate) {
          // A imagem em Arquivos (Ping da Unity): pastas abertas até ela e linha à vista.
          const std::string file=state_.textureOrigin.size()>1?state_.textureOrigin[1].copy:std::string();
          if(!file.empty() && revealInFiles(file)) state_.status="Localizado em Arquivos: "+file;
          else state_.status="Esta textura vem de dentro do modelo; não há arquivo próprio para localizar.";
          return true;
        }
        if(sub==detail::TextureInspectorReimport) {
          if(state_.textureViewerSource && state_.textureViewerSourceIndex<sourceTextures_.size()) {
            reimportPath_=sourceTextures_[state_.textureViewerSourceIndex].sourcePath;
            state_.status="Reimportando a fonte "+baseName(reimportPath_)+" com o perfil dela";
          } else if(!state_.textureViewerSource && state_.textureViewerIndex<textures_.size()) {
            textureReimportPath_=textures_[state_.textureViewerIndex].path;
            state_.status="Reimportando "+textures_[state_.textureViewerIndex].name;
          }
          return true;
        }
        return true;
      }
      if(key>=widgetId(EditorWidget::TextureManagerRowBase) && key-widgetId(EditorWidget::TextureManagerRowBase)<textures_.size()) {
        openTextureInspector(key-widgetId(EditorWidget::TextureManagerRowBase));return true;
      }
      if(key>=widgetId(EditorWidget::TextureUserBase) && key-widgetId(EditorWidget::TextureUserBase)<state_.textureUserEntities.size()) {
        const auto id=state_.textureUserEntities[key-widgetId(EditorWidget::TextureUserBase)];
        if(id!=kInvalidEntity && document_.find(id)) {
          state_.textureInspector=false;state_.textureViewer=false;
          setSelection(id);
          state_.status="Objeto que usa a textura selecionado";
        }
        return true;
      }
    }
    if(state_.componentSelection!=state_.selection) {
      state_.componentSelection=state_.selection;state_.referenceInstance=0;state_.expandedComponent.clear();state_.expandedNative=0;state_.nativeMenu=0;
      state_.expandedScript=0;state_.scriptMenu=0;state_.meshPicker=false;state_.resourceInstance=0;state_.resourceProperty.clear();
      state_.componentPage=0;state_.componentPreview=0;state_.scriptPreviewType.clear();state_.propertyPage=0;state_.propertyQuery.clear();state_.addingComponent=false;
    }
    const u32 key=routing.widgetId;
    if(key==widgetId(EditorWidget::CameraViewClose)) {cancelPointers();state_.cameraViewEntity=0;state_.cameraPiloting=false;return true;}
    if(key==widgetId(EditorWidget::CameraView) || key==widgetId(EditorWidget::CameraPilot)) {
      cancelPointers();
      const auto pose=resolveSceneCamera(document_,state_.selection,true);
      if(pose.entity) {state_.cameraViewEntity=pose.entity;state_.cameraPiloting=key==widgetId(EditorWidget::CameraPilot);}
      else state_.status="A câmera precisa de uma transformação válida";
      return true;
    }
    if(key==widgetId(EditorWidget::CameraAlignView)) {
      // Qualquer objeto vai para a pose da vista (Align With View da Unity);
      // numa camera, o Play passa a ver o que o editor mostra.
      const auto *entity=document_.find(state_.selection);
      if(!entity || state_.selection==document_.root() || history_.isOpen()) return true;
      state_.inspectorMenu=false;
      if(!isViewportValid(view_)) {state_.status="A vista precisa de uma projeção válida";return true;}
      float world[16]{},parent[16];editorTransformMatrix(EditorTransform{},world);editorTransformMatrix(EditorTransform{},parent);
      const auto basis=renderer::buildCameraViewBasis(view_.frustum.yaw,view_.frustum.pitch,view_.frustum.roll);
      for(u32 k=0;k<3;++k) {world[k]=basis.row0[k];world[4+k]=basis.row1[k];world[8+k]=basis.row2[k];world[12+k]=view_.frustum.cameraPosition[k];}
      EditorTransform pose;
      if((!entity->parent || editorWorldMatrix(document_,entity->parent,parent)) && editorLocalTransformForWorld(world,parent,pose)) {
        std::copy(entity->transform.scale,entity->transform.scale+3,pose.scale);
        if(history_.setTransform(document_,entity->id,pose)) state_.status=cameraComponent(*entity)?"Câmera alinhada à vista":"Objeto alinhado à vista";
      } else state_.status="A hierarquia não permite alinhar esta pose sem shear";
      return true;
    }
    if(key==widgetId(EditorWidget::ReferenceClose)) {state_.referenceInstance=0;state_.presetInputIndex=0;state_.presetInputProperty=nullptr;return true;}
    if(key==widgetId(EditorWidget::ReferencePrevious)) {if(state_.referencePage) --state_.referencePage;return true;}
    if(key==widgetId(EditorWidget::ReferenceNext)) {++state_.referencePage;return true;}
    if(key==widgetId(EditorWidget::ReferenceModeToggle)) {
      state_.pickerAdvanced=!state_.pickerAdvanced;state_.referenceHighlight=0;state_.referencePage=0;
      saveEditorPreferences();return true;
    }
    if(key==widgetId(EditorWidget::ReferenceTypeFilter)) {
      state_.referenceTypeFilter=!state_.referenceTypeFilter;state_.referencePage=0;return true;
    }
    if(key>=widgetId(EditorWidget::ReferenceViewBase) && key<widgetId(EditorWidget::ReferenceViewBase)+3) {
      state_.pickerView=static_cast<u8>(key-widgetId(EditorWidget::ReferenceViewBase));state_.referencePage=0;return true;
    }
    if(key==widgetId(EditorWidget::ReferenceSearch)) {
      state_.editingReferenceSearch=true;std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",state_.referenceQuery.c_str());return true;
    }
    if(key>=widgetId(EditorWidget::ComponentReferenceBase)&&key<widgetId(EditorWidget::ComponentReferenceBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);if(!entity) return true;
      const auto *component=entity->components.at(key&0xffu);const auto field=(key&0x00ffffffu)>>8;
      if(!component||field>=component->type().references.size()) return true;
      state_.referenceInstance=component->instanceId();state_.referenceProperty=component->type().references[field].id;
      state_.referenceScript=false;state_.referenceQuery.clear();state_.referencePage=0;return true;
    }
    if(key==widgetId(EditorWidget::ReferenceClear)||(key>=widgetId(EditorWidget::ReferenceChoiceBase)&&key<widgetId(EditorWidget::ReferenceChoiceBase)+0x01000000u)||
       key==widgetId(EditorWidget::ReferenceAssign)||(key>=widgetId(EditorWidget::ReferenceResultBase)&&key<widgetId(EditorWidget::ReferenceResultBase)+0x10000u)) {
      const auto *entity=document_.find(state_.presetInputIndex?state_.presetEntity:state_.selection);if(!entity||!state_.referenceInstance) return true;
      const auto scriptReference=editorScriptReference(state_.referenceScriptType);
      const auto *property=state_.presetInputIndex?state_.presetInputProperty:state_.referenceScript?&scriptReference:editorReferenceProperty(*entity,state_.referenceInstance,state_.referenceProperty);
      if(!property) {state_.referenceInstance=0;return true;}
      u64 target=0;
      if(key==widgetId(EditorWidget::ReferenceAssign) ||
         (key>=widgetId(EditorWidget::ReferenceResultBase)&&key<widgetId(EditorWidget::ReferenceResultBase)+0x10000u)) {
        // Avançado: o primeiro toque destaca (painel de inspeção); o segundo
        // toque ou "Escolher" atribui — só quando o campo aceita o objeto.
        const auto results=state_.presetInputIndex?editorRecipeReferenceResults(document_,entity->id,state_.presetInputChoices,state_.referenceQuery,state_.referenceTypeFilter):editorAdvancedReferenceResults(document_,entity->id,*property,state_.referenceQuery,state_.referenceTypeFilter);
        const EditorReferenceResult *chosen=nullptr;
        if(key==widgetId(EditorWidget::ReferenceAssign)) {
          for(const auto &result:results) if(result.id==state_.referenceHighlight) chosen=&result;
        } else {
          const u32 index=key-widgetId(EditorWidget::ReferenceResultBase);
          if(index>=results.size()) return true;
          if(results[index].id!=state_.referenceHighlight) {state_.referenceHighlight=results[index].id;return true;}
          chosen=&results[index];
        }
        if(!chosen) return true;
        if(!chosen->compatible) {state_.status="O campo recusa este objeto";return true;}
        target=chosen->id;state_.referenceHighlight=0;
      } else if(key!=widgetId(EditorWidget::ReferenceClear)) {
        auto choices=state_.presetInputIndex?editorRecipeReferenceChoices(document_,state_.presetInputChoices,state_.referenceQuery):editorReferenceChoices(document_,entity->id,*property,state_.referenceQuery);
        const auto index=key-widgetId(EditorWidget::ReferenceChoiceBase);
        if(index>=choices.size()) return true;
        target=choices[index];
      }
      if(state_.presetInputIndex) {
        const auto index=state_.presetInputIndex-1;
        if(state_.presetEpoch!=sceneEpoch_ || state_.presetRevision!=document_.revision() || index>=state_.presetInputValues.size() ||
           (target && std::find(state_.presetInputChoices.begin(),state_.presetInputChoices.end(),target)==state_.presetInputChoices.end())) {
          state_.status="Entrada de receita incompatível ou cena alterada";return true;
        }
        state_.presetInputValues[index]=target;state_.referenceInstance=0;state_.presetInputIndex=0;state_.presetInputProperty=nullptr;
        refreshComponentPresets();state_.status="Entrada da receita preparada; cena preservada";return true;
      }
      // Como na Unity, pôr objetos no LOD 0 recalcula os limites do grupo. Vai
      // num comando só com a referência: um Desfazer volta os dois.
      if(!state_.referenceScript && state_.referenceProperty=="level_0" && !history_.isOpen() &&
         &entity->components.findInstance(state_.referenceInstance)->type()==&scene::LodGroup::descriptor &&
         editorReferenceAccepts(document_,entity->id,*property,target)) {
        auto value=*entity;
        auto *group=static_cast<scene::LodGroup *>(value.components.editInstance(state_.referenceInstance));
        if(group) {
          group->levels[0]=target;
          const bool measured=target && fitLodGroupSize(mapScene_,document_,entity->id,*group);
          if(group->valid() && history_.applyValues(document_,entity->id,value)) {
            state_.referenceInstance=0;
            state_.status=measured?"LOD 0 atribuído; tamanho medido pela malha":"Referência atualizada";
            return true;
          }
        }
      }
      EditorActionRequest request;request.version=sceneVersion();request.entity=entity->id;request.componentInstance=state_.referenceInstance;request.componentProperty=state_.referenceProperty;
      if(state_.referenceScript && state_.referenceScriptElement) {
        // Elemento de uma lista de referências: troca só aquele elemento.
        const auto *script=scene::scriptBehavior(entity->components.findInstance(state_.referenceInstance));
        const auto *declared=script?scriptProperty(script->scriptType,state_.referenceProperty):nullptr;
        const auto value=editorScriptReferenceValue(document_,state_.referenceScriptType,target);
        auto items=script?scriptArrayItems(*script,state_.referenceProperty):std::vector<std::string>{};
        if(!declared || value.empty() || state_.referenceScriptElement>items.size()) {state_.status="A referência não é compatível com este campo";return true;}
        items[state_.referenceScriptElement-1]=value;
        if(setScriptArray(entity->id,state_.referenceInstance,state_.referenceProperty,declared->valueType,items)) {
          state_.referenceInstance=0;state_.referenceScriptElement=0;state_.status="Referência atualizada";
        }
        return true;
      }
      if(state_.referenceScript) {
        request.action=EditorAction::ScriptProperty;request.scriptPropertyType=state_.referenceScriptType;
        request.scriptPropertyValue=editorScriptReferenceValue(document_,state_.referenceScriptType,target);
      }
      else {request.action=EditorAction::ComponentProperty;request.componentType=entity->components.findInstance(state_.referenceInstance)->type().id;request.componentValue=scene::ObjectReference{target};}
      if(dispatch(request).status==EditorActionStatus::Applied) {state_.referenceInstance=0;state_.status="Referência atualizada";}
      else state_.status="A referência não é compatível com este campo";
      return true;
    }
    if(key>=widgetId(EditorWidget::CameraLens) && key<=widgetId(EditorWidget::CameraFar)) {
      const auto *entity=document_.find(state_.cameraViewEntity);
      const auto *component=entity?cameraComponent(*entity):nullptr;
      if(!component || history_.isOpen()) return true;
      const u32 index=key==widgetId(EditorWidget::CameraLens)?
        (component->projection==scene::CameraProjection::Orthographic?4u:0u):
        (key==widgetId(EditorWidget::CameraNear)?1u:2u);
      const auto &property=component->type().numbers[index];
      state_.numericField=key;state_.numericEntity=entity->id;
      state_.numericInstance=component->instanceId();state_.numericProperty=property.id;
      std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g",static_cast<double>(property.read(*component)));
      state_.numericReplace=true;state_.numericError=false;return true;
    }
    if(key>=widgetId(EditorWidget::ComponentColorBase)&&key<widgetId(EditorWidget::ComponentColorBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);const u32 index=key&0xffu,field=(key&0x00ffffffu)>>8;
      if(!entity||index>=entity->components.size()||history_.isOpen()) return true;
      const auto *component=entity->components.at(index);
      if(field>=component->type().triples.size()) return true;
      const auto &triple=component->type().triples[field];if(!scene::componentTripleIsColor(triple.kind)) return true;
      float rgb[3]{};
      for(u32 axis=0;axis<3;++axis) {
        bool found=false;
        for(const auto &p:component->type().numbers) if(p.id==triple.channels[axis]) {
          if(!p.presentation.isEditable(*component)) return true;
          rgb[axis]=p.read(*component);found=true;
        }
        if(!found) return true;
      }
      // O seletor trabalha em linear; um campo sRGB é convertido na ida e na volta.
      if(triple.kind==scene::ComponentTripleKind::SrgbColor) for(float &channel:rgb) channel=scene::srgbChannelToLinear(channel);
      state_.colorEntity=entity->id;state_.colorInstance=component->instanceId();
      state_.colorProperty=triple.id;state_.colorTarget=0;
      openColorWindow(key,{rgb[0],rgb[1],rgb[2],1},false,false);return true;
    }
    if(key>=widgetId(EditorWidget::ComponentTripleBase) && key<widgetId(EditorWidget::ComponentTripleBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);const u32 type=key&0xffu,field=(key&0x00ffffffu)>>8;
      if(!entity||type>=entity->components.size()||history_.isOpen()) return true;
      const auto *component=entity->components.at(type);
      if(field>=component->type().triples.size()) return true;
      const auto &triple=component->type().triples[field];float values[3]{};
      for(u32 axis=0;axis<triple.dimensions();++axis) {
        bool found=false;
        for(const auto &p:component->type().numbers) if(p.id==triple.channels[axis]) {
          if(!p.presentation.isEditable(*component)) return true;
          values[axis]=p.read(*component);found=true;
        }
        if(!found) return true;
      }
      state_.numericField=key;state_.numericEntity=entity->id;state_.numericInstance=component->instanceId();state_.numericProperty=triple.id;
      if(triple.dimensions()==2)std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g %.9g",static_cast<double>(values[0]),static_cast<double>(values[1]));
      else std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g %.9g %.9g",static_cast<double>(values[0]),static_cast<double>(values[1]),static_cast<double>(values[2]));
      state_.numericReplace=true;state_.numericError=false;return true;
    }
    if(key>=widgetId(EditorWidget::ComponentNumberBase) && key<widgetId(EditorWidget::ComponentNumberBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);const u32 type=key&0xffu,field=(key&0x00ffffffu)>>8;
      if(!entity || type>=entity->components.size() || history_.isOpen()) return true;
      const auto *component=entity->components.at(type);
      if(!component || field>=component->type().numbers.size()) return true;
      const auto &property=component->type().numbers[field];
      state_.numericField=key;state_.numericEntity=entity->id;state_.numericInstance=component->instanceId();state_.numericProperty=property.id;
      std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g",static_cast<double>(property.read(*component)));
      state_.numericReplace=true;state_.numericError=false;return true;
    }
    if(key>=widgetId(EditorWidget::ComponentSlotNumberBase) && key<widgetId(EditorWidget::ComponentSlotNumberBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);const u32 encoded=key-widgetId(EditorWidget::ComponentSlotNumberBase);
      const u32 type=encoded&0xffu,field=(encoded>>8)&0xffu,slot=(encoded>>16)&0xffu;
      if(!entity||type>=entity->components.size()||history_.isOpen()) return true;
      const auto *component=entity->components.at(type);
      if(!component||field>=component->type().slotNumbers.size()) return true;
      const auto &property=component->type().slotNumbers[field];
      if(!property.read||slot>=property.slotCount(*component)) return true;
      // O endereço viaja no próprio campo; o commit decodifica e confere de novo.
      state_.numericField=key;state_.numericEntity=entity->id;state_.numericInstance=component->instanceId();state_.numericProperty=property.id;
      std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g",static_cast<double>(property.read(*component,slot)));
      state_.numericReplace=true;state_.numericError=false;return true;
    }
    if(key>=widgetId(EditorWidget::ComponentResourceBase) && key<widgetId(EditorWidget::ComponentResourceBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);const u32 encoded=key-widgetId(EditorWidget::ComponentResourceBase);
      const u32 type=encoded&0xffu,bindingIndex=(encoded>>8)&0xffu,slot=(encoded>>16)&0xffu;
      if(!entity||type>=entity->components.size()||history_.isOpen()) return true;
      const auto *component=entity->components.at(type);
      if(bindingIndex>=component->type().resourceBindings.size()) return true;
      const auto &binding=component->type().resourceBindings[bindingIndex];
      if((binding.kind!=resources::AssetType::Mesh&&binding.kind!=resources::AssetType::EnvironmentProfile&&
          binding.kind!=resources::AssetType::SkyboxMaterial&&
          binding.kind!=resources::AssetType::EnvironmentMap&&binding.kind!=resources::AssetType::AnimationClip&&binding.kind!=resources::AssetType::Texture&&binding.kind!=resources::AssetType::UiDocument&&
          binding.kind!=resources::AssetType::PhysicsMaterial&&binding.kind!=resources::AssetType::AnimatorController)||
         slot>=binding.slotCount(*component)||
         !binding.presentation.isEditable(*component)) return true;
      state_.resourceInstance=component->instanceId();state_.resourceProperty=std::string(binding.id);state_.resourceSlot=slot;
      if(binding.id=="collision_mesh") refreshCollisionMeshDraft();
      state_.meshPicker=true;state_.meshPage=0;return true;
    }
    if(key>=widgetId(EditorWidget::ComponentFieldResetBase) && key<widgetId(EditorWidget::ComponentFieldResetBase)+0x10000000u) {
      const u32 encoded=key-widgetId(EditorWidget::ComponentFieldResetBase);
      const u32 index=encoded&0xffu,kind=(encoded>>8)&0xfu,field=(encoded>>12)&0xffu,slot=(encoded>>20)&0xffu;
      const auto *entity=document_.find(state_.selection);
      const auto *component=entity&&index<entity->components.size()?entity->components.at(index):nullptr;
      if(!component || component->instanceId()!=state_.expandedNative || history_.isOpen()) return true;
      const auto &type=component->type();
      const auto defaults=type.create();if(!defaults) return true;
      const auto changes=scene::componentDelta(*component,*defaults);
      std::vector<scene::FieldAddress> fields;
      const auto add=[&](std::string_view id,scene::FieldKind fieldKind,u32 fieldSlot=0) {
        for(const auto &change:changes) if(change.address==scene::FieldAddress{id,fieldSlot,fieldKind} && change.differs && change.applicable)
          fields.push_back(change.address);
      };
      switch(kind) {
      case 0:if(field<type.booleans.size()) add(type.booleans[field].id,scene::FieldKind::Boolean);break;
      case 1:if(field<type.enums.size()) add(type.enums[field].id,scene::FieldKind::Enum);break;
      case 2:if(field<type.numbers.size()) add(type.numbers[field].id,scene::FieldKind::Number);break;
      case 4:if(field<type.references.size()) add(type.references[field].id,scene::FieldKind::Reference);break;
      case 5:if(field<type.triples.size()) for(const auto channel:type.triples[field].channels) add(channel,scene::FieldKind::Number);break;
      case 9:if(field<type.slotNumbers.size()) add(type.slotNumbers[field].id,scene::FieldKind::SlotNumber,slot);break;
      case 10:if(field<type.slotEnums.size()) add(type.slotEnums[field].id,scene::FieldKind::SlotEnum,slot);break;
      default:return true;
      }
      if(fields.empty()) return true;
      auto candidate=*entity;
      const auto result=scene::applyComponentFields(candidate.components,*defaults,fields,component->instanceId());
      const auto *applied=candidate.components.findInstance(component->instanceId());
      if(!result.ok() || result.rejected || !applied || !editorReferencesAccept(document_,entity->id,*applied)) {
        state_.status=result.error?result.error:"Valor padrão incompatível com este objeto";return true;
      }
      if(history_.applyValues(document_,entity->id,candidate)) state_.status="Propriedade restaurada; disponível em Desfazer";
      return true;
    }
    // Lupa de um cartão da coluna: o índice põe o foco nele antes de buscar.
    if(key>=widgetId(EditorWidget::ComponentPropertySearch)+0x100u && key<widgetId(EditorWidget::ComponentPropertySearch)+0x200u) {
      const auto *entity=document_.find(state_.selection);const u32 at=key-widgetId(EditorWidget::ComponentPropertySearch)-0x100u;
      const auto *value=entity&&at<entity->components.size()?entity->components.at(at):nullptr;
      if(!value) return true;
      if(state_.expandedNative!=value->instanceId()) {state_.propertyQuery.clear();state_.propertyPage=0;}
      state_.componentSelection=state_.selection;state_.expandedNative=value->instanceId();
      state_.expandedComponent.clear();state_.expandedScript=0;state_.nativeMenu=0;state_.scriptMenu=0;
      state_.editingPropertySearch=true;
      std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",state_.propertyQuery.c_str());
      return true;
    }
    if(key==widgetId(EditorWidget::ComponentPropertySearch)) {
      if(!state_.expandedNative || !document_.find(state_.selection)) return true;
      state_.editingPropertySearch=true;
      std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",state_.propertyQuery.c_str());
      return true;
    }
    if(key==widgetId(EditorWidget::ComponentPropertySearchClear)) {state_.propertyQuery.clear();state_.propertyPage=0;return true;}
    if(key==widgetId(EditorWidget::ComponentSearch) || key==widgetId(EditorWidget::MeshSearch)) {
      state_.editingComponentSearch=key==widgetId(EditorWidget::ComponentSearch);state_.editingMeshSearch=!state_.editingComponentSearch;
      const auto &query=state_.editingComponentSearch?state_.componentQuery:state_.meshQuery;
      std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",query.c_str());return true;
    }
    if(key==widgetId(EditorWidget::ComponentSearchClear)) {state_.componentQuery.clear();state_.componentPage=0;state_.addScroll=0;return true;}
    if(key>=widgetId(EditorWidget::ComponentFamilyBase) &&
       key<=widgetId(EditorWidget::ComponentFamilyBase)+static_cast<u32>(scene::ComponentFamily::Count)) {
      state_.componentCategory=key-widgetId(EditorWidget::ComponentFamilyBase);state_.addScroll=0;return true;
    }
    if(key==widgetId(EditorWidget::ObjectFold)) {
      state_.componentSelection=state_.selection;state_.expandedComponent=state_.tapInspectorCard("astra.object",state_.expandedComponent=="astra.object")?"astra.object":"";
      state_.expandedNative=0;state_.expandedScript=0;state_.nativeMenu=0;state_.scriptMenu=0;state_.meshPicker=false;
      state_.transformMenu=false;state_.propertyPage=0;state_.propertyQuery.clear();return true;
    }
    if(key==widgetId(EditorWidget::TransformFold)) {
      state_.componentSelection=state_.selection;state_.expandedComponent=state_.tapInspectorCard("astra.transform",state_.expandedComponent=="astra.transform")?"astra.transform":"";
      state_.expandedNative=0;state_.expandedScript=0;state_.nativeMenu=0;state_.scriptMenu=0;state_.meshPicker=false;state_.propertyPage=0;state_.propertyQuery.clear();return true;
    }
    if(key==widgetId(EditorWidget::MeshChoose)) {
      state_.resourceInstance=0;state_.resourceProperty.clear();state_.meshPicker=true;state_.meshPage=0;return true;
    }
    // Vínculo com a fonte importada (M08.2).
    if(key==widgetId(EditorWidget::ImportLinkMenu)) {state_.importLinkMenu=!state_.importLinkMenu;return true;}
    if(key>=widgetId(EditorWidget::ImportLinkRevertBase) && key<widgetId(EditorWidget::ImportLinkRevertBase)+128u) {
      state_.status=revertImportLink(state_.selection,key-widgetId(EditorWidget::ImportLinkRevertBase))?"Revertido para a fonte":"Nada a reverter";
      state_.importLinkMenu=false;return true;
    }
    if(key==widgetId(EditorWidget::ImportLinkUnlink)) {
      const auto count=unlinkImport(state_.selection);
      state_.status=count?std::to_string(count)+" objeto(s) desvinculado(s); a fonte não altera mais esta instância":"Sem vínculo";
      state_.importLinkMenu=false;return true;
    }
    if(key==widgetId(EditorWidget::ImportLinkKeep) || key==widgetId(EditorWidget::ImportLinkDelete)) {
      const bool keep=key==widgetId(EditorWidget::ImportLinkKeep);
      state_.status=resolveImportOrphan(state_.selection,keep)?(keep?"Órfão mantido como objeto independente":"Órfão apagado"):"Nada a resolver";
      state_.importLinkMenu=false;return true;
    }
    // Recurso aberto em Propriedades → janela focada.
    if(key==widgetId(EditorWidget::AssetInspectorFocus)) {
      using Kind=EditorScreenState::FocusedAsset;
      const bool ok=state_.materialInspector.valid()?openFocusedAsset(Kind::Material,state_.materialInspector):
          state_.environmentInspector.valid()?openFocusedAsset(Kind::EnvironmentMap,state_.environmentInspector):
          state_.profileInspector.valid()?openFocusedAsset(Kind::EnvironmentProfile,state_.profileInspector):false;
      if(!ok) state_.status="Nada para abrir numa janela";
      return true;
    }
    // Material físico do projeto em Propriedades.
    if(state_.physicsMaterialInspector.valid()) {
      if(key==widgetId(EditorWidget::PhysicsMaterialInspectorClose)) {state_.physicsMaterialInspector={};state_.selectedFile.clear();state_.selectedFiles.clear();return true;}
      if(key>=widgetId(EditorWidget::PhysicsMaterialStepBase)&&key<widgetId(EditorWidget::PhysicsMaterialStepBase)+10) {
        const u32 code=key-widgetId(EditorWidget::PhysicsMaterialStepBase),field=code/2;const bool up=code%2;
        stepPhysicsMaterial(field,up);return true;
      }
    }
    // Perfil de ambiente do projeto em Propriedades.
    if(state_.profileInspector.valid()) {
      if(key==widgetId(EditorWidget::ProfileInspectorClose)) {state_.profileInspector={};state_.selectedFile.clear();state_.selectedFiles.clear();return true;}
      if(key==widgetId(EditorWidget::ProfileInspectorUses)) {
        const auto &users=state_.profileObjects;
        if(users.empty()) {state_.status="Nenhum componente Ambiente da cena usa este perfil";return true;}
        const u32 index=state_.profileUse%static_cast<u32>(users.size());
        pingEntity(users[index]);state_.profileUse=index+1;
        state_.status="Uso "+std::to_string(index+1)+" de "+std::to_string(users.size())+": "+document_.find(users[index])->name;
        return true;
      }
      if(key>=widgetId(EditorWidget::ProfileGroupBase) && key<widgetId(EditorWidget::ProfileGroupBase)+state_.profileGroups.size()) {
        state_.profileGroup=key-widgetId(EditorWidget::ProfileGroupBase);state_.propertyPage=0;refreshProfileInspector();return true;
      }
      if(key>=widgetId(EditorWidget::ProfileRowBase) && key<widgetId(EditorWidget::ProfileRowBase)+state_.profileRows.size()) {
        const auto row=state_.profileRows[key-widgetId(EditorWidget::ProfileRowBase)];
        if(!row.editable) {state_.status="Sem consumidor neste aparelho ou dependente de outra opção";return true;}
        using Kind=EditorScreenState::ProfileRow::Kind;
        std::string diagnostic;bool applied=false;
        if(row.kind==Kind::Boolean) applied=editProfileProperty(row,row.mixed?true:!row.on,nullptr,nullptr,diagnostic);
        else if(row.kind==Kind::Enum) {
          for(const auto &p:scene::Environment::descriptor.enums) if(p.id==row.id) {
            const auto *profile=findEnvironmentProfile(state_.profileInspector);
            scene::Environment proxy;proxy.values=profile->values;
            const u32 current=p.read(proxy);usize next=0;
            for(usize i=0;i<p.options.size();++i) if(p.options[i].value==current) next=(i+1)%p.options.size();
            applied=editProfileProperty(row,p.options[next].value,nullptr,nullptr,diagnostic);
          }
        } else if(row.kind==Kind::EnvironmentMap) {
          // Percorre os mapas HDRI do projeto e o ambiente padrão (sem mapa).
          const auto *profile=findEnvironmentProfile(state_.profileInspector);
          std::vector<resources::AssetGuid> maps{resources::AssetGuid{}};
          for(const auto &record:assets_.records()) if(record.type==resources::AssetType::EnvironmentMap) maps.push_back(record.guid);
          usize next=0;
          for(usize i=0;i<maps.size();++i) if(maps[i]==profile->values.environmentMap) next=(i+1)%maps.size();
          applied=editProfileProperty(row,false,nullptr,&maps[next],diagnostic);
        } else {
          // Número ou trio: o teclado numérico (trio como "r g b").
          if(history_.isOpen()) return true;
          state_.numericField=key;state_.numericEntity=0;state_.numericInstance=0;state_.numericProperty=std::string(row.id);
          std::snprintf(state_.numericText,sizeof(state_.numericText),"%s",row.kind==Kind::Triple?row.value.c_str():
                        row.value.substr(0,row.value.find(' ')).c_str());
          state_.numericReplace=true;state_.numericError=false;return true;
        }
        state_.status=applied?diagnostic:diagnostic;
        return true;
      }
    }
    // Mapa HDRI do projeto em Propriedades.
    if(state_.environmentInspector.valid()) {
      auto &draft=state_.environmentDraft;
      if(key==widgetId(EditorWidget::EnvironmentInspectorClose)) {state_.environmentInspector={};state_.selectedFile.clear();state_.selectedFiles.clear();return true;}
      if(key==widgetId(EditorWidget::EnvironmentExposureDown) || key==widgetId(EditorWidget::EnvironmentExposureUp)) {
        state_.environmentExposure=std::clamp(state_.environmentExposure+(key==widgetId(EditorWidget::EnvironmentExposureUp)?.5f:-.5f),-6.0f,6.0f);
        writeEnvironmentPreview();return true;
      }
      if(key==widgetId(EditorWidget::EnvironmentInspectorUses)) {
        const auto &users=state_.environmentObjects;
        if(users.empty()) {state_.status="Nenhum componente Ambiente da cena usa este HDRI";return true;}
        const u32 index=state_.environmentUse%static_cast<u32>(users.size());
        pingEntity(users[index]);state_.environmentUse=index+1;
        state_.status="Uso "+std::to_string(index+1)+" de "+std::to_string(users.size())+": "+document_.find(users[index])->name;
        return true;
      }
      // Receita: os mesmos degraus do painel de importação.
      const bool down=key>=widgetId(EditorWidget::EnvironmentRecipeDownBase) && key<widgetId(EditorWidget::EnvironmentRecipeDownBase)+5;
      const bool up=key>=widgetId(EditorWidget::EnvironmentRecipeUpBase) && key<widgetId(EditorWidget::EnvironmentRecipeUpBase)+5;
      if(down || up) {
        const u32 row=key-widgetId(down?EditorWidget::EnvironmentRecipeDownBase:EditorWidget::EnvironmentRecipeUpBase);
        editEnvironmentRecipe(row,up);
        return true;
      }
      if(key==widgetId(EditorWidget::EnvironmentRecipeRevert)) {revertEnvironmentRecipes();state_.status="Receita HDRI revertida";return true;}
      if(key==widgetId(EditorWidget::EnvironmentRecipeApply)) {
        if(multiAssetEditing_ && multiEnvironments_.size()>1) {requestEnvironmentBatch();return true;}
        // O shell reimporta pela mesma trilha do botão Reimportar, com esta receita.
        environmentReimportPath_=state_.environmentInspectorPath;environmentReimportOverride_=draft;
        state_.status="Reimportando o HDRI com a receita nova";return true;
      }
    }
    if(key==widgetId(EditorWidget::LibraryOpen)) {
      if(!isPlaying()){cameraPreview_.close();state_.workspace=EditorWorkspace::Library;refreshLibraryRows();}return true;
    }
    // Malhas do projeto: a Biblioteca no trilho Projeto (o antigo espaço de
    // "Recursos importados" era uma lista de texto à parte).
    if(key==widgetId(EditorWidget::TabAssets)) {
      if(!isPlaying()&&mapScene_.assetCount()) {
        cameraPreview_.close();state_.contextMenu=false;state_.workspace=EditorWorkspace::Library;
        state_.libraryKind=11;state_.libraryScroll=0;state_.libraryPackage={};refreshLibraryRows();
      }
      return true;
    }
    if(key==widgetId(EditorWidget::LibraryClose)) {state_.workspace=EditorWorkspace::Scene;return true;}
    if(key==widgetId(EditorWidget::LibraryImport)) {state_.libraryImportRequested=true;return true;}
    if(key==widgetId(EditorWidget::LibrarySearch)) {state_.searchingLibrary=true;return true;}
    if(key==widgetId(EditorWidget::LibraryDetailsClose)) {state_.libraryDiagnostic.clear();state_.libraryDiagnosticScroll=0;state_.libraryConflict=false;state_.libraryDetailItem={};state_.libraryDetailName.clear();return true;}
    if(key==widgetId(EditorWidget::LibraryKinds)) {state_.libraryKind=(state_.libraryKind+1)%12;state_.libraryAuditFilter=0;state_.libraryScroll=0;refreshLibraryRows();return true;}
    if(key>=widgetId(EditorWidget::LibraryKindBase)&&key<widgetId(EditorWidget::LibraryKindBase)+12) {
      state_.libraryKind=key-widgetId(EditorWidget::LibraryKindBase);state_.libraryAuditFilter=0;state_.libraryScroll=0;refreshLibraryRows();return true;
    }
    // G3a: um bloco da auditoria filtra os materiais daquela classe; o mesmo
    // bloco de novo volta a mostrar todos.
    if(key>=widgetId(EditorWidget::LibraryAuditBase)&&key<widgetId(EditorWidget::LibraryAuditBase)+3) {
      const auto chosen=static_cast<u8>(key-widgetId(EditorWidget::LibraryAuditBase)+1);
      state_.libraryAuditFilter=state_.libraryAuditFilter==chosen?0:chosen;
      if(state_.libraryAuditFilter)state_.libraryKind=static_cast<u32>(resources::LibraryItemKind::Material)+1;
      state_.libraryScroll=0;refreshLibraryRows();return true;
    }
    if(key==widgetId(EditorWidget::LibraryReimport)||key==widgetId(EditorWidget::LibraryKeepLocal)||key==widgetId(EditorWidget::LibraryReceiveSource)) {
      const auto id=state_.libraryDetailItem;const auto *item=library_.item(id);if(!item)return true;
      const auto path=item->path;std::string error;
      const auto policy=key==widgetId(EditorWidget::LibraryKeepLocal)?resources::UnityReimportPolicy::KeepLocal:
        key==widgetId(EditorWidget::LibraryReceiveSource)?resources::UnityReimportPolicy::ReceiveSource:resources::UnityReimportPolicy::RefuseConflict;
      const auto guid=importUnityAsset(path,state_.selection,policy,error);
      state_.libraryConflict=!guid.valid()&&error.find("Manter local ou Receber origem")!=error.npos;
      state_.libraryDiagnostic=error;state_.libraryDiagnosticScroll=0;state_.status=error;
      return true;
    }
    if(key==widgetId(EditorWidget::LibraryInstantiate)||key==widgetId(EditorWidget::LibraryControllerAssign)) {
      const auto resolved=library_.resolve(state_.libraryDetailItem,assets_);if(!resolved.output)return true;
      const auto guid=resolved.output->guid;std::string error;
      if(key==widgetId(EditorWidget::LibraryInstantiate)) {
        if(!instantiatePrefab(guid,document_.root(),error)){state_.status=state_.libraryDiagnostic=error;return true;}
        state_.libraryDiagnostic.clear();state_.workspace=EditorWorkspace::Scene;
      } else {
        const auto *owner=document_.find(state_.selection);
        if(!owner||!owner->components.find(scene::Animator::descriptor)) {
          state_.libraryDiagnostic="Selecione um objeto com Animator para vincular este controller. Adicione Animator pelo menu de componentes; a Biblioteca não cria objetos ocultos.";return true;
        }
        state_.inspectorTarget=state_.selection;openAnimatorEditor();
        if(assignAnimatorController(guid)){state_.libraryDiagnostic.clear();state_.workspace=EditorWorkspace::Scene;}
        else state_.libraryDiagnostic=state_.status;
      }
      return true;
    }
    if(key>=widgetId(EditorWidget::LibraryItemDetailsBase)&&key<widgetId(EditorWidget::LibraryItemDetailsBase)+resources::LibraryIndex::MaximumItems) {
      const auto row=key-widgetId(EditorWidget::LibraryItemDetailsBase);if(row>=state_.libraryRows.size())return true;
      const auto *item=library_.item(state_.libraryRows[row].id);if(!item)return true;
      state_.libraryDetailItem=item->id;state_.libraryConflict=false;const auto resolution=library_.resolve(item->id,assets_);
      state_.libraryDetailName=std::filesystem::path(item->path).filename().string();
      const bool ready=resolution.output&&resolution.state==resources::LibraryResolutionState::Registered;
      state_.libraryCanInstantiate=ready&&item->kind==resources::LibraryItemKind::Prefab;
      state_.libraryCanAssignController=ready&&item->kind==resources::LibraryItemKind::Controller;
      state_.libraryCanReimport=item->path.ends_with(".anim")||item->path.ends_with(".prefab")||item->path.ends_with(".controller");
      state_.libraryDiagnostic=item->path+"\nOrigem: "+item->sourceIdentity+"\nSHA-256 original: "+item->sourceHash+"\n"+resolution.diagnostic;
      for(const auto &loss:item->knownLosses)
        state_.libraryDiagnostic+=(loss.starts_with(resources::UnityMaterialApproximationPrefix)?"\n":"\nConversão: ")+loss;
      for(const auto dep:item->dependencies){const auto *source=library_.item(dep);state_.libraryDiagnostic+="\nDependência: "+(source?source->path:dep.text()+" (ausente)");}
      state_.libraryDiagnosticScroll=0;return true;
    }
    if(key==widgetId(EditorWidget::LibraryPackages)) {
      usize next=0;for(usize i=0;i<state_.libraryPackages.size();++i)if(state_.libraryPackages[i].id==state_.libraryPackage)next=(i+1)%state_.libraryPackages.size();
      if(!state_.libraryPackages.empty())state_.libraryPackage=state_.libraryPackages[next].id;
      state_.libraryScroll=0;refreshLibraryRows();return true;
    }
    if(key>=widgetId(EditorWidget::LibraryItemBase)&&key<widgetId(EditorWidget::LibraryItemBase)+resources::LibraryIndex::MaximumItems) {
      const auto row=key-widgetId(EditorWidget::LibraryItemBase);std::string error;
      if(row<state_.libraryRows.size()&&(state_.libraryRows[row].projectAsset!=~0u||state_.libraryRows[row].unregistered)) {
        // Malha do projeto ou origem sem registro: detalhe próprio, sem item de pacote.
        const auto &item=state_.libraryRows[row];
        state_.libraryDetailItem=item.id;state_.libraryDetailName=item.name;state_.libraryConflict=false;
        state_.libraryCanInstantiate=state_.libraryCanAssignController=state_.libraryCanReimport=false;
        state_.libraryDiagnostic=item.diagnostic;state_.libraryDiagnosticScroll=0;return true;
      }
      if(row<state_.libraryRows.size()) {const auto id=state_.libraryRows[row].id;if(!openLibraryItem(id,error)) {
        state_.libraryConflict=error.find("Manter local ou Receber origem")!=error.npos;
        state_.status=error;state_.libraryDiagnostic=error;state_.libraryDiagnosticScroll=0;
        if(const auto *item=library_.item(id)) {state_.libraryDiagnostic+="\nFonte: "+item->path+"\nIdentidade: "+item->sourceIdentity+"\nSHA-256: "+item->sourceHash;
          for(const auto &loss:item->knownLosses)state_.libraryDiagnostic+="\nLimite: "+loss;
          for(const auto dep:item->dependencies)if(!library_.item(dep))state_.libraryDiagnostic+="\nDependência ausente: "+dep.text();}
      }}
      return true;
    }
    // Material do projeto em Propriedades.
    if(key==widgetId(EditorWidget::MaterialInspectorClose)) {
      state_.materialInspector={};state_.materialShared=false;state_.texturePicker=false;state_.selectedFile.clear();return true;
    }
    if(key==widgetId(EditorWidget::MaterialInspectorUses)) {
      // Cada toque revela o próximo objeto que usa o material (Ping), sem tirar o
      // material de Propriedades.
      const auto &users=state_.materialInspectorObjects;
      if(users.empty()) {state_.status="Nenhum objeto da cena usa este material";return true;}
      const u32 index=state_.materialInspectorUse%static_cast<u32>(users.size());
      pingEntity(users[index]);state_.materialInspectorUse=index+1;
      state_.status="Uso "+std::to_string(index+1)+" de "+std::to_string(users.size())+": "+document_.find(users[index])->name;
      return true;
    }
    if(key==widgetId(EditorWidget::MaterialInspectorStandalone)) {
      if(!cameraPreview_.pinStandalone(sceneVersion(),mapScene_,state_.materialInspector)) state_.status=cameraPreview_.diagnostic();
      else {state_.workspace=EditorWorkspace::Scene;state_.status="Prévia independente · material compartilhado";}
      return true;
    }
    if(key==widgetId(EditorWidget::MaterialInspectorPreview)) {
      if(!cameraPreview_.pinMaterial(document_,sceneVersion(),mapScene_,state_.selection,state_.materialInspector))
        state_.status=cameraPreview_.diagnostic();
      else {state_.workspace=EditorWorkspace::Scene;state_.status="Prévia do objeto real · inclui seus descendentes e substituições locais.";}
      return true;
    }
    if(key==widgetId(EditorWidget::MaterialInspectorApply)) {
      const auto *target=document_.find(state_.selection);
      const auto *render=target?meshRenderer(*target):nullptr;
      if(!render||!render->slotCount()||!state_.materialInspector.valid()) {
        state_.status="Selecione uma malha com slot de material.";return true;
      }
      const auto slot=std::min(state_.materialSlot,render->slotCount()-1);
      const std::string name=target->name;
      if(assignSlotMaterial(state_.selection,slot,state_.materialInspector)) {
        state_.status="Material vinculado no slot "+std::to_string(slot+1)+" de "+name+" · substituições locais preservadas";
        refreshMaterialSlotView();
      } else state_.status="Não foi possível aplicar material; finalize a edição e tente novamente.";
      return true;
    }
    // Material por slot (Entrega 2).
    if(key==widgetId(EditorWidget::MaterialSlotPrevious)) {if(state_.materialSlot) --state_.materialSlot;state_.propertyPage=0;return true;}
    if(key==widgetId(EditorWidget::MaterialSlotNext)) {++state_.materialSlot;state_.propertyPage=0;return true;}
    if(key==widgetId(EditorWidget::MaterialScopeInstance)) {state_.materialShared=false;return true;}
    if(key==widgetId(EditorWidget::MaterialScopeShared)) {
      if(state_.materialSlotView.shared) state_.materialShared=true;
      else state_.status="Este slot usa o material da fonte: crie um material do projeto para editar todos os usos";
      return true;
    }
    if(key==widgetId(EditorWidget::MaterialChoose)) {state_.materialPicker=true;state_.meshPage=0;return true;}
    if(key==widgetId(EditorWidget::MaterialPickerClose)) {state_.materialPicker=false;return true;}
    // R4: textura por binding, no alcance em edição.
    if(key>=widgetId(EditorWidget::MaterialTextureBase) && key<widgetId(EditorWidget::MaterialTextureBase)+scene::MaterialTextureCount) {
      state_.textureBinding=key-widgetId(EditorWidget::MaterialTextureBase);
      state_.texturePicker=true;state_.texturePickerScroll=0;state_.materialPicker=false;state_.meshPage=0;return true;
    }
    if(key==widgetId(EditorWidget::TexturePickerClose)) {state_.texturePicker=false;return true;}
    // R4: amostragem do binding aberto no seletor, no alcance em edição.
    if(key==widgetId(EditorWidget::TextureSamplingUv) || key==widgetId(EditorWidget::TextureSamplingWrap) ||
       key==widgetId(EditorWidget::TextureSamplingFilter) || key==widgetId(EditorWidget::TextureUvReset) ||
       (key>=widgetId(EditorWidget::TextureUvStepBase) && key<widgetId(EditorWidget::TextureUvStepBase)+10)) {
      const auto *entity=materialAssetMode()?nullptr:document_.find(state_.selection);
      const auto *render=entity?meshRenderer(*entity):nullptr;
      if((!render && !materialAssetMode()) || state_.textureBinding>=scene::MaterialTextureCount) return true;
      const bool shared=materialAssetMode() || state_.materialShared;
      const auto *asset=findMaterialAsset(sharedMaterialGuid(render));
      if(shared && !asset) {state_.status="Este slot usa o material da fonte: crie um material do projeto para editar todos os usos";return true;}
      auto sampling=shared?asset->sampling[state_.textureBinding]:render->slotSampling(state_.materialSlot)[state_.textureBinding];
      if(key==widgetId(EditorWidget::TextureSamplingUv)) sampling.uvSet=static_cast<std::uint8_t>((sampling.uvSet+1)%4);
      else if(key==widgetId(EditorWidget::TextureSamplingWrap)) sampling.wrap=static_cast<std::uint8_t>((sampling.wrap+1)%4);
      else if(key==widgetId(EditorWidget::TextureSamplingFilter)) sampling.filter=static_cast<std::uint8_t>((sampling.filter+1)%3);
      else if(key==widgetId(EditorWidget::TextureUvReset)) {
        sampling.offset[0]=sampling.offset[1]=0;sampling.scale[0]=sampling.scale[1]=1;sampling.rotation=0;
      } else {
        // Passos fixos e arredondados: somar floats repetidamente deixaria 0.30000001 no arquivo.
        const u32 step=key-widgetId(EditorWidget::TextureUvStepBase);
        const float sign=(step&1u)?1.0f:-1.0f;
        const auto rounded=[](float value){return std::round(value*100.0f)/100.0f;};
        switch(step/2) {
        case 0: sampling.offset[0]=std::clamp(rounded(sampling.offset[0]+.05f*sign),-100.0f,100.0f); break;
        case 1: sampling.offset[1]=std::clamp(rounded(sampling.offset[1]+.05f*sign),-100.0f,100.0f); break;
        case 2: sampling.scale[0]=std::clamp(rounded(sampling.scale[0]+.1f*sign),.1f,100.0f); break;
        case 3: sampling.scale[1]=std::clamp(rounded(sampling.scale[1]+.1f*sign),.1f,100.0f); break;
        default: sampling.rotation=std::fmod(sampling.rotation+15.0f*sign,360.0f); break;
        }
      }
      std::string diagnostic;
      state_.status=setSlotSampling(state_.selection,state_.materialSlot,state_.textureBinding,
                                    shared?MaterialScope::Shared:MaterialScope::Instance,sampling,diagnostic,
          "uv"+std::to_string(state_.textureBinding)+"."+
          (key==widgetId(EditorWidget::TextureSamplingUv)?"set":key==widgetId(EditorWidget::TextureSamplingWrap)?"wrap":
           key==widgetId(EditorWidget::TextureSamplingFilter)?"filter":key==widgetId(EditorWidget::TextureUvReset)?"reset":
           (key-widgetId(EditorWidget::TextureUvStepBase))/2==0?"offset0":(key-widgetId(EditorWidget::TextureUvStepBase))/2==1?"offset1":
           (key-widgetId(EditorWidget::TextureUvStepBase))/2==2?"scale0":(key-widgetId(EditorWidget::TextureUvStepBase))/2==3?"scale1":"rotation"))?
          (shared?"Amostragem compartilhada atualizada em todos os usos":"Amostragem desta instância atualizada"):diagnostic;
      return true;
    }
    // R4: visualizador de textura.
    if(key>=widgetId(EditorWidget::TextureViewBase) && key-widgetId(EditorWidget::TextureViewBase)<textures_.size()) {
      if(!openTextureViewer(key-widgetId(EditorWidget::TextureViewBase))) state_.status=state_.textureViewerInfo;
      return true;
    }
    if(key==widgetId(EditorWidget::TextureViewerClose)) {
      closeTextureViewer();
      // Em Propriedades, voltar leva ao gerenciador quando a textura veio dele.
      if(state_.textureInspector) {state_.textureInspector=false;state_.textureManager=textureInspectorFromManager_;}
      return true;
    }
    if(key==widgetId(EditorWidget::TextureViewerChannel)) {cycleTextureViewerChannel();return true;}
    if(key==widgetId(EditorWidget::TextureViewerZoom)) {cycleTextureViewerZoom();return true;}
    if(key==widgetId(EditorWidget::TextureProfilePrevious)) {
      if(state_.textureProfilePage) --state_.textureProfilePage;
      return true;
    }
    if(key==widgetId(EditorWidget::TextureProfileNext)) {
      state_.textureProfilePage=std::min(2u,state_.textureProfilePage+1);
      return true;
    }
    // R4: perfil da textura aberta no visualizador.
    if(key>=widgetId(EditorWidget::TextureProfileInterpretation) && key<=widgetId(EditorWidget::TextureProfileStreamingPriority) &&
       state_.textureViewer && state_.textureViewerIndex<textures_.size()) {
      auto &profile=state_.textureProfileDraft;
      stepTextureProfileField(profile,key-widgetId(EditorWidget::TextureProfileInterpretation));
      state_.textureProfileDirty=!resources::sameTextureProfile(profile,state_.textureProfileSaved);
      refreshTextureViewerImage();
      return true;
    }
    if((key==widgetId(EditorWidget::TextureProfileApply) || key==widgetId(EditorWidget::TextureProfileRevert)) &&
       state_.textureViewer && state_.textureViewerIndex<textures_.size()) {
      const auto texture=textures_[state_.textureViewerIndex];
      if(key==widgetId(EditorWidget::TextureProfileRevert)) {
        state_.textureProfileDraft=state_.textureProfileSaved;state_.textureProfileDirty=false;
        state_.status="Alterações do perfil descartadas";
      } else if(state_.textureProfileDirty) {
        std::string diagnostic;
        if(setTextureProfile(texture.guid,state_.textureProfileDraft,diagnostic)) {
          state_.textureProfileSaved=state_.textureProfileDraft;state_.textureProfileDirty=false;
          state_.status=diagnostic.empty()?"Perfil aplicado a todos os usos de "+texture.name:diagnostic;
        } else state_.status=std::move(diagnostic);
      }
      refreshTextureViewerImage();return true;
    }
    if(key==widgetId(EditorWidget::TextureViewerBackground)) {cycleTextureViewerBackground();return true;}
    if(key==widgetId(EditorWidget::TextureViewerMipDown) || key==widgetId(EditorWidget::TextureViewerMipUp)) {
      stepTextureViewerLevel(key==widgetId(EditorWidget::TextureViewerMipUp)?1:-1);
      return true;
    }
    // R4: modo de alfa, corte e faces, no alcance em edição.
    if(key==widgetId(EditorWidget::MaterialAlphaCycle) || key==widgetId(EditorWidget::MaterialSidesCycle) ||
       key==widgetId(EditorWidget::MaterialCutoffDown) || key==widgetId(EditorWidget::MaterialCutoffUp)) {
      const auto *entity=materialAssetMode()?nullptr:document_.find(state_.selection);
      const auto *render=entity?meshRenderer(*entity):nullptr;
      if(!render && !materialAssetMode()) return true;
      const bool shared=materialAssetMode() || state_.materialShared;
      const auto *asset=findMaterialAsset(sharedMaterialGuid(render));
      if(shared && !asset) {state_.status="Este slot usa o material da fonte: crie um material do projeto para editar todos os usos";return true;}
      auto surface=shared?asset->surface:render->slotSurface(state_.materialSlot);
      if(key==widgetId(EditorWidget::MaterialAlphaCycle)) surface.alphaMode=static_cast<std::uint8_t>((surface.alphaMode+1)%4);
      else if(key==widgetId(EditorWidget::MaterialSidesCycle)) surface.sides=static_cast<std::uint8_t>((surface.sides+1)%3);
      else if(surface.alphaMode==scene::MaterialAlphaMask)
        surface.alphaCutoff=std::clamp(std::round((surface.alphaCutoff+(key==widgetId(EditorWidget::MaterialCutoffUp)?.05f:-.05f))*100.0f)/100.0f,0.0f,1.0f);
      std::string diagnostic;
      state_.status=setSlotSurface(state_.selection,state_.materialSlot,shared?MaterialScope::Shared:MaterialScope::Instance,surface,diagnostic,
          key==widgetId(EditorWidget::MaterialAlphaCycle)?"alpha":key==widgetId(EditorWidget::MaterialSidesCycle)?"sides":"cutoff")?
          (shared?"Material compartilhado atualizado em todos os usos":"Material desta instância atualizado"):diagnostic;
      return true;
    }
    // R4: oclusão, canais, normal e origem do alfa, no alcance em edição.
    if(key==widgetId(EditorWidget::MaterialOcclusionSourceCycle) || key==widgetId(EditorWidget::MaterialOcclusionStrengthDown) ||
       key==widgetId(EditorWidget::MaterialOcclusionStrengthUp) || key==widgetId(EditorWidget::MaterialChannelRoughness) ||
       key==widgetId(EditorWidget::MaterialChannelMetallic) || key==widgetId(EditorWidget::MaterialChannelOcclusion) ||
       key==widgetId(EditorWidget::MaterialNormalFlipCycle) || key==widgetId(EditorWidget::MaterialAlphaSourceCycle) ||
       key==widgetId(EditorWidget::MaterialRoughnessSourceCycle)) {
      const auto *entity=materialAssetMode()?nullptr:document_.find(state_.selection);
      const auto *render=entity?meshRenderer(*entity):nullptr;
      if(!render && !materialAssetMode()) return true;
      const bool shared=materialAssetMode() || state_.materialShared;
      const auto *asset=findMaterialAsset(sharedMaterialGuid(render));
      if(shared && !asset) {state_.status="Este slot usa o material da fonte: crie um material do projeto para editar todos os usos";return true;}
      auto channels=shared?asset->channels:render->slotChannels(state_.materialSlot);
      const auto cycle=[](std::uint8_t value,unsigned count) {return static_cast<std::uint8_t>((value+1u)%count);};
      if(key==widgetId(EditorWidget::MaterialOcclusionSourceCycle)) channels.occlusionSource=cycle(channels.occlusionSource,4);
      else if(key==widgetId(EditorWidget::MaterialOcclusionStrengthDown) || key==widgetId(EditorWidget::MaterialOcclusionStrengthUp)) {
        const float current=channels.occlusionStrength<0?1.0f:channels.occlusionStrength;
        const float step=key==widgetId(EditorWidget::MaterialOcclusionStrengthUp)?.05f:-.05f;
        channels.occlusionStrength=std::clamp(std::round((current+step)*100.0f)/100.0f,0.0f,1.0f);
      }
      else if(key==widgetId(EditorWidget::MaterialChannelRoughness)) channels.roughness=cycle(channels.roughness,5);
      else if(key==widgetId(EditorWidget::MaterialChannelMetallic)) channels.metallic=cycle(channels.metallic,5);
      else if(key==widgetId(EditorWidget::MaterialChannelOcclusion)) channels.occlusion=cycle(channels.occlusion,5);
      else if(key==widgetId(EditorWidget::MaterialNormalFlipCycle)) channels.normalFlipY=cycle(channels.normalFlipY,3);
      else if(key==widgetId(EditorWidget::MaterialRoughnessSourceCycle)) channels.roughnessSource=cycle(channels.roughnessSource,4);
      else channels.alphaSource=cycle(channels.alphaSource,4);
      std::string diagnostic;
      state_.status=setSlotChannels(state_.selection,state_.materialSlot,shared?MaterialScope::Shared:MaterialScope::Instance,channels,diagnostic,
          key==widgetId(EditorWidget::MaterialOcclusionSourceCycle)?"ch0":
          key==widgetId(EditorWidget::MaterialChannelRoughness)?"ch3.0":
          key==widgetId(EditorWidget::MaterialChannelMetallic)?"ch3.1":
          key==widgetId(EditorWidget::MaterialChannelOcclusion)?"ch3.2":
          key==widgetId(EditorWidget::MaterialNormalFlipCycle)?"ch4":
          key==widgetId(EditorWidget::MaterialAlphaSourceCycle)?"ch5":
          key==widgetId(EditorWidget::MaterialRoughnessSourceCycle)?"ch6":"ch2")?
          (shared?"Material compartilhado atualizado em todos os usos":"Material desta instância atualizado"):diagnostic;
      return true;
    }
    if(key==widgetId(EditorWidget::MaterialIsolateCycle)) {
      state_.materialIsolate=static_cast<u8>((state_.materialIsolate+1u)%6u);
      state_.status=state_.materialIsolate?"Prévia isolando um dado do material (não é salvo)":"Prévia normal";
      return true;
    }
    if(key==widgetId(EditorWidget::MaterialOcclusionTexture)) {
      state_.textureBinding=scene::MaterialOcclusionTextureBinding;
      state_.texturePicker=true;state_.texturePickerScroll=0;state_.materialPicker=false;state_.meshPage=0;return true;
    }
    const auto applyTexture=[&](const resources::AssetGuid &texture,const std::string &done) {
      std::string diagnostic;
      const bool applied=setSlotTexture(state_.selection,state_.materialSlot,state_.textureBinding,
                                        state_.materialShared?MaterialScope::Shared:MaterialScope::Instance,texture,diagnostic);
      state_.status=applied?done:diagnostic;
      state_.texturePicker=false;
      return true;
    };
    if(key==widgetId(EditorWidget::TextureUseInherited)) return applyTexture({},"O binding voltou a herdar a textura");
    if(key==widgetId(EditorWidget::TextureUseNone)) return applyTexture(scene::MaterialTextureNone,"Binding sem textura");
    if(key>=widgetId(EditorWidget::TextureChoiceBase) && key-widgetId(EditorWidget::TextureChoiceBase)<textures_.size()) {
      const auto texture=textures_[key-widgetId(EditorWidget::TextureChoiceBase)];
      return applyTexture(texture.guid,"O binding usa "+texture.name);
    }
    if(key==widgetId(EditorWidget::MaterialClearOverride)) {
      state_.status=clearSlotMaterialOverride(state_.selection,state_.materialSlot)?"Substituição local removida":"Sem substituição local";
      return true;
    }
    if(key==widgetId(EditorWidget::MaterialCreateShared)) {
      std::string diagnostic;
      if(createMaterialFromSlot(state_.selection,state_.materialSlot,diagnostic).valid()) state_.materialShared=true;
      else state_.status=diagnostic;
      state_.materialPicker=false;return true;
    }
    if(key==widgetId(EditorWidget::MaterialUseSource)) {
      state_.status=assignSlotMaterial(state_.selection,state_.materialSlot,{})?"O slot voltou ao material da fonte":"Não foi possível trocar o material";
      state_.materialShared=false;state_.materialPicker=false;return true;
    }
    if(key>=widgetId(EditorWidget::MaterialChoiceBase) && key-widgetId(EditorWidget::MaterialChoiceBase)<materials_.size()) {
      const auto &material=materials_[key-widgetId(EditorWidget::MaterialChoiceBase)];
      state_.status=assignSlotMaterial(state_.selection,state_.materialSlot,material.guid)?"O slot usa "+material.name:"Não foi possível trocar o material";
      state_.materialPicker=false;return true;
    }
    if(key>=widgetId(EditorWidget::MaterialNumberBase) && key-widgetId(EditorWidget::MaterialNumberBase)<scene::meshRendererNumbers.size()) {
      if(history_.isOpen()) return true;
      const auto field=key-widgetId(EditorWidget::MaterialNumberBase);
      state_.numericField=key;state_.numericEntity=state_.selection;state_.numericInstance=0;state_.numericProperty.clear();
      std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g",static_cast<double>(state_.materialSlotView.values[field]));
      state_.numericReplace=true;state_.numericError=false;return true;
    }
    if(key==widgetId(EditorWidget::MeshPickerClose)) {state_.meshPicker=false;state_.resourceInstance=0;state_.resourceProperty.clear();return true;}
    if(key==widgetId(EditorWidget::MeshPrevious)) {if(state_.meshPage) --state_.meshPage;return true;}
    if(key==widgetId(EditorWidget::MeshNext)) {++state_.meshPage;return true;}
    // Tween de propriedade: abrir o seletor, fechar e aplicar a escolha. A
    // escolha grava tipo e PropertyId e usa o valor atual como destino inicial.
    if(key>=widgetId(EditorWidget::PropertyTweenPick)&&key<widgetId(EditorWidget::PropertyTweenPick)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);const u32 index=key-widgetId(EditorWidget::PropertyTweenPick);
      if(!entity||index>=entity->components.size()||isPlaying()||history_.isOpen()) return true;
      const auto *component=entity->components.at(index);
      if(&component->type()!=&scene::PropertyTween::descriptor) return true;
      state_.propertyTweenPicker=component->instanceId();state_.meshPage=0;return true;
    }
    if(key==widgetId(EditorWidget::PropertyTweenPickerClose)) {state_.propertyTweenPicker=0;return true;}
    if(key>=widgetId(EditorWidget::PropertyTweenChoiceBase)&&key<widgetId(EditorWidget::PropertyTweenChoiceBase)+0x01000000u) {
      const u32 choice=key-widgetId(EditorWidget::PropertyTweenChoiceBase);
      const auto *entity=document_.find(state_.selection);
      const auto *component=entity?entity->components.findInstance(state_.propertyTweenPicker):nullptr;
      if(!component||&component->type()!=&scene::PropertyTween::descriptor||isPlaying()||history_.isOpen()) {state_.propertyTweenPicker=0;return true;}
      const auto &tween=scene::propertyTween(*component);
      const auto *target=tween.target?document_.find(static_cast<EditorEntityId>(tween.target)):entity;
      const auto options=target?tweenablePropertyOptions(target->components):std::vector<TweenablePropertyOption>{};
      if(choice>=options.size()) return true;
      auto values=*entity;auto &edited=scene::propertyTween(*values.components.editInstance(state_.propertyTweenPicker));
      edited.componentType=options[choice].type;edited.property=options[choice].property;
      edited.destination=std::clamp(options[choice].value,-100000.f,100000.f);edited.relative=false;
      if(!edited.valid()||!history_.applyValues(document_,state_.selection,values)) {state_.status="Propriedade recusada";return true;}
      state_.propertyTweenPicker=0;state_.status="Propriedade do tween: "+options[choice].componentName+" · "+options[choice].propertyName;
      return true;
    }
    if(key==widgetId(EditorWidget::PhysicsMaterialCreate)||key==widgetId(EditorWidget::PhysicsMaterialUpdate)) {
      std::string diagnostic;
      const bool created=key==widgetId(EditorWidget::PhysicsMaterialCreate);
      const bool ok=created?createPhysicsMaterial(state_.selection,state_.resourceInstance,diagnostic).valid():
                            updatePhysicsMaterial(state_.selection,state_.resourceInstance,diagnostic);
      state_.status=diagnostic;if(ok) {state_.meshPicker=false;state_.resourceInstance=0;state_.resourceProperty.clear();}
      return true;
    }
    // PK-2: janela Iluminação. Material do céu novo (do céu atual), editar o
    // escolhido, e criar o Ambiente da cena quando ela só tem o padrão.
    if(key==widgetId(EditorWidget::LightingCreateSkybox)) {
      std::string diagnostic;
      const bool ok=createSkyboxMaterial(state_.selection,state_.resourceInstance?state_.resourceInstance:state_.expandedNative,diagnostic).valid();
      state_.status=diagnostic;if(ok) {state_.meshPicker=false;state_.resourceInstance=0;state_.resourceProperty.clear();}
      return true;
    }
    if(key==widgetId(EditorWidget::LightingEditSkybox)) {
      const auto *object=document_.find(state_.selection);
      const auto *component=object?object->components.findInstance(state_.resourceInstance?state_.resourceInstance:state_.expandedNative):nullptr;
      const auto *environment=component&&&component->type()==&scene::Environment::descriptor?
          static_cast<const scene::Environment*>(component):nullptr;
      state_.meshPicker=false;state_.resourceInstance=0;state_.resourceProperty.clear();
      if(!environment||!openProfileInspector(environment->values.skybox)) state_.status="Material do céu indisponível";
      return true;
    }
    if(key==widgetId(EditorWidget::LightingCreateEnvironment)) {
      std::string diagnostic;createSceneEnvironment(diagnostic);state_.status=diagnostic;return true;
    }
    if(key==widgetId(EditorWidget::EnvironmentProfileCreate)||key==widgetId(EditorWidget::EnvironmentProfileUpdate)) {
      std::string diagnostic;
      const bool created=key==widgetId(EditorWidget::EnvironmentProfileCreate);
      const bool ok=created?createEnvironmentProfile(state_.selection,state_.resourceInstance,diagnostic).valid():
                            updateEnvironmentProfile(state_.selection,state_.resourceInstance,diagnostic);
      state_.status=diagnostic;if(ok) {state_.meshPicker=false;state_.resourceInstance=0;state_.resourceProperty.clear();}
      return true;
    }
    constexpr u8 collisionQualitySteps[]{5,10,25,50,75,90};
    constexpr float collisionErrorSteps[]{.001f,.005f,.01f,.02f,.05f,.1f,.25f};
    const auto stepIndex=[](const auto &steps,auto value) {
      u32 closest=0;auto distance=std::abs(steps[0]-value);
      for(u32 i=1;i<std::size(steps);++i) if(const auto next=std::abs(steps[i]-value);next<distance) {closest=i;distance=next;}
      return closest;
    };
    if(key==widgetId(EditorWidget::MeshCollisionQualityDown)||key==widgetId(EditorWidget::MeshCollisionQualityUp)) {
      auto step=stepIndex(collisionQualitySteps,state_.collisionTrianglePercent);
      if(key==widgetId(EditorWidget::MeshCollisionQualityDown)&&step) --step;
      if(key==widgetId(EditorWidget::MeshCollisionQualityUp)&&step+1<std::size(collisionQualitySteps)) ++step;
      state_.collisionTrianglePercent=collisionQualitySteps[step];return true;
    }
    if(key==widgetId(EditorWidget::MeshCollisionErrorDown)||key==widgetId(EditorWidget::MeshCollisionErrorUp)) {
      auto step=stepIndex(collisionErrorSteps,state_.collisionMaximumError);
      if(key==widgetId(EditorWidget::MeshCollisionErrorDown)&&step) --step;
      if(key==widgetId(EditorWidget::MeshCollisionErrorUp)&&step+1<std::size(collisionErrorSteps)) ++step;
      state_.collisionMaximumError=collisionErrorSteps[step];return true;
    }
    if(key==widgetId(EditorWidget::MeshGenerateCollision)) {
      EditorActionRequest request;request.version=sceneVersion();request.entity=state_.selection;
      request.action=EditorAction::GenerateCollisionMesh;request.componentInstance=state_.resourceInstance;
      request.property=state_.collisionTrianglePercent;request.number=state_.collisionMaximumError;
      dispatch(request);
      return true;
    }
    if(key==widgetId(EditorWidget::MeshGeometryTab)||key==widgetId(EditorWidget::MeshMaterialTab)) {state_.meshTab=key==widgetId(EditorWidget::MeshMaterialTab);state_.propertyPage=0;state_.meshPicker=false;return true;}
    if(key==widgetId(EditorWidget::MeshLightingTab)) {state_.meshTab=3;state_.propertyPage=0;state_.meshPicker=false;return true;}
    if((key>=widgetId(EditorWidget::MeshChoiceBase) && key<widgetId(EditorWidget::MeshChoiceBase)+0x01000000u) || key==widgetId(EditorWidget::MeshClear) || key==widgetId(EditorWidget::MaterialRestore)) {
      EditorActionRequest request;request.version=sceneVersion();request.entity=state_.selection;
      request.property=key>=widgetId(EditorWidget::MeshChoiceBase)?key-widgetId(EditorWidget::MeshChoiceBase)+1:0;
      if(state_.resourceInstance&&key!=widgetId(EditorWidget::MaterialRestore)) {
        request.action=EditorAction::ComponentResource;request.componentInstance=state_.resourceInstance;
        request.componentProperty=state_.resourceProperty;request.componentResourceSlot=state_.resourceSlot;
        if(request.property) {
          const auto *object=document_.find(state_.selection);
          const auto *component=object?object->components.findInstance(state_.resourceInstance):nullptr;
          const scene::ComponentResourceBinding *binding=nullptr;
          if(component) for(const auto &candidate:component->type().resourceBindings)
            if(candidate.id==state_.resourceProperty) {binding=&candidate;break;}
          if(binding&&(binding->kind==resources::AssetType::EnvironmentProfile||
                      binding->kind==resources::AssetType::SkyboxMaterial||
                      binding->kind==resources::AssetType::EnvironmentMap||binding->kind==resources::AssetType::Texture||binding->kind==resources::AssetType::AudioClip||binding->kind==resources::AssetType::UiDocument||
                      binding->kind==resources::AssetType::PhysicsMaterial||binding->kind==resources::AssetType::AnimatorController)) {
            const auto index=request.property-1;
            if(index>=assets_.records().size()) return true;
            request.componentResource=assets_.records()[index].guid;
          } else if(binding&&binding->kind==resources::AssetType::AnimationClip) {
            // A lista do seletor é o catálogo de clipes carregados, na mesma ordem.
            const auto catalog=mapScene_.clipCatalog();
            const auto index=request.property-1;
            if(index>=catalog.size()) return true;
            request.componentResource=catalog[index].clip;
          } else request.componentResource=mapScene_.assetGuid(request.property-1);
        }
      } else request.action=key==widgetId(EditorWidget::MaterialRestore)?EditorAction::RestoreMaterial:EditorAction::AssignMesh;
      if(dispatch(request).status==EditorActionStatus::Applied) {
        state_.meshPicker=false;state_.resourceInstance=0;state_.resourceProperty.clear();
        state_.status=request.action==EditorAction::RestoreMaterial?"Material da origem restaurado":
                      request.action==EditorAction::ComponentResource?"Recurso do componente atualizado":"Referência de malha atualizada";
      }
      else state_.status="Referência incompatível com este objeto";
      return true;
    }
    const bool confirming=key==widgetId(EditorWidget::ComponentPreviewConfirm);
    if((confirming && state_.componentPreview) || (key>=widgetId(EditorWidget::ComponentAddBase)&&key-widgetId(EditorWidget::ComponentAddBase)<editorComponentCatalog.size())) {
      const u32 index=confirming?state_.componentPreview-1:key-widgetId(EditorWidget::ComponentAddBase);
      if(index>=editorComponentCatalog.size() || (confirming &&
         (!state_.componentPreview || !state_.addingComponent || state_.componentSelection!=state_.selection))) return true;
      const auto &entry=editorComponentCatalog[index];
      const auto *entity=document_.find(state_.selection);
      if(!entity) return true;
      const auto plan=scene::planComponentAddition(entity->components,entry.type->id,false,false);
      if(!confirming) {
        state_.componentPreview=index+1;state_.componentPreviewValues=false;state_.componentPreviewPage=0;
        state_.scriptPreviewType.clear();return true;
      }
      if(!plan.ready) {state_.status=plan.error?plan.error:"Não foi possível compor o objeto";return true;}
      EditorActionRequest request;request.version=sceneVersion();request.action=EditorAction::AddComponent;
      request.entity=state_.selection;request.componentType=entry.type->id;
      const auto result=dispatch(request);
      if(result.status==EditorActionStatus::Applied) {
        state_.componentSelection=state_.selection;state_.expandedComponent.clear();state_.expandedNative=0;state_.expandedScript=0;
        if(const auto *entity=document_.find(state_.selection))
          for(usize i=entity->components.size();i>0;--i)
            if(const auto *component=entity->components.at(i-1);component && &component->type()==entry.type) {
              state_.expandedNative=component->instanceId();break;
            }
        // Recentes: o tipo sobe para o topo, sem repetição, e a lista fica curta.
        auto &recent=state_.recentComponents;
        recent.erase(std::remove(recent.begin(),recent.end(),entry.type->id),recent.end());
        recent.insert(recent.begin(),std::string(entry.type->id));
        if(recent.size()>4) recent.resize(4);
        state_.addingComponent=false;state_.componentPreview=0;state_.componentPage=0;state_.componentGroup.clear();state_.propertyPage=0;
        state_.nativeMenu=0;state_.scriptMenu=0;
        state_.status="Componente adicionado";
      } else if(const auto *entity=document_.find(state_.selection)) {
        const auto *reason=entry.unavailable(*entity);state_.status=reason?reason:"Não foi possível adicionar o componente";
      }
      return true;
    }
    if(confirming && !state_.scriptPreviewType.empty()) {
      if(!state_.addingComponent || state_.componentSelection!=state_.selection ||
         code_.publishedGeneration()!=state_.scriptPreviewGeneration) {
        state_.status="Catálogo alterado; volte e escolha novamente";return true;
      }
      const auto type=std::find_if(code_.scriptTypes().begin(),code_.scriptTypes().end(),
          [&](const auto &candidate){return candidate.id==state_.scriptPreviewType;});
      if(type==code_.scriptTypes().end()) {state_.status="Tipo indisponível; volte e escolha novamente";return true;}
      EditorActionRequest request;request.version=sceneVersion();request.action=EditorAction::AddScript;
      request.entity=state_.selection;request.scriptType=type->id;
      if(dispatch(request).status==EditorActionStatus::Applied) {
        state_.expandedComponent.clear();state_.expandedNative=0;state_.expandedScript=0;
        if(const auto *entity=document_.find(state_.selection))
          if(const auto *script=scene::scriptBehavior(entity->components.at(entity->components.size()-1)))
            state_.expandedScript=script->instanceId();
        state_.addingComponent=false;state_.scriptPreviewType.clear();state_.componentPage=0;
        state_.componentGroup.clear();state_.scriptPropertyPage=0;state_.status="Comportamento anexado";
      } else state_.status="Não foi possível anexar o comportamento";
      return true;
    }
    if(key==widgetId(EditorWidget::ColliderFit)) {
      const auto *entity=document_.find(state_.selection);
      if(entity && colliderComponent(*entity) && !history_.isOpen()) {
        auto value=*entity;auto *collider=editCollider(value,state_.expandedNative);
        if(collider && fitEditorCollider(mapScene_,document_,entity->id,*collider) && history_.applyValues(document_,entity->id,value))
          state_.status="Colisor ajustado pela geometria; forma e centro continuam editáveis";
        else state_.status="Não foi possível ajustar: geometria ausente, escala ou dimensões incompatíveis";
      }
      return true;
    }
    if(key==widgetId(EditorWidget::LodGroupFit)) {
      const auto *entity=document_.find(state_.selection);
      const auto *current=entity?static_cast<const scene::LodGroup *>(entity->components.find(scene::LodGroup::descriptor)):nullptr;
      if(current && !history_.isOpen()) {
        auto value=*entity;auto *group=static_cast<scene::LodGroup *>(value.components.edit(scene::LodGroup::descriptor));
        if(group && fitLodGroupSize(mapScene_,document_,entity->id,*group) && history_.applyValues(document_,entity->id,value))
          state_.status="Tamanho do LOD Group medido pela malha do LOD 0";
        else state_.status="Não foi possível medir: o LOD 0 não tem malha";
      }
      return true;
    }
    if(key==widgetId(EditorWidget::ComponentPrevious)||key==widgetId(EditorWidget::ComponentNext)) {
      state_.componentPage=layout_.componentPage;
      if(key==widgetId(EditorWidget::ComponentPrevious)) {if(state_.componentPage) --state_.componentPage;}
      else ++state_.componentPage;
      state_.componentSelection=state_.selection;return true;
    }
    if(key==widgetId(EditorWidget::ScriptFieldsPrevious)||key==widgetId(EditorWidget::ScriptFieldsNext)) {
      if(key==widgetId(EditorWidget::ScriptFieldsPrevious)) {if(state_.scriptPropertyPage) --state_.scriptPropertyPage;}
      else ++state_.scriptPropertyPage;
      return true;
    }
    const u32 keyOperation=key&0xff000000u;
    const bool arrayWidget=keyOperation>=widgetId(EditorWidget::ScriptArraySizeBase) && keyOperation<=widgetId(EditorWidget::ScriptArrayRemoveBase);
    if((key>=widgetId(EditorWidget::ScriptAddBase)&&key<widgetId(EditorWidget::ScriptFieldBase)+0x01000000u) || arrayWidget) {
      const auto *entity=document_.find(state_.selection);if(!entity||state_.workspace!=EditorWorkspace::Scene||history_.isOpen()) return true;
      const u32 operation=key&0xff000000u;
      const u32 index=key&(operation==widgetId(EditorWidget::ScriptFieldBase)||arrayWidget?0xffu:0x00ffffffu);
      state_.componentSelection=state_.selection;
      if(operation==widgetId(EditorWidget::ScriptAddBase)) {
        if(index>=code_.scriptTypes().size() || !state_.addingComponent) return true;
        state_.scriptPreviewType=code_.scriptTypes()[index].id;
        state_.scriptPreviewGeneration=code_.publishedGeneration();state_.componentPreview=0;
        return true;
      }
      const auto *script=scene::scriptBehavior(entity->components.at(index));if(!script) return true;
      if(operation==widgetId(EditorWidget::ScriptFoldBase)) {
        state_.expandedScript=state_.tapInspectorCard("script:"+script->scriptType,state_.expandedScript==script->instanceId())?script->instanceId():0;state_.expandedComponent.clear();state_.expandedNative=0;state_.nativeMenu=0;state_.scriptMenu=0;state_.meshPicker=false;state_.scriptPropertyPage=0;state_.propertyQuery.clear();
      } else if(operation==widgetId(EditorWidget::ScriptMenuBase)) {
        state_.scriptMenu=state_.scriptMenu==script->instanceId()?0:script->instanceId();state_.expandedScript=0;state_.expandedComponent.clear();state_.expandedNative=0;state_.propertyQuery.clear();
      } else if(operation==widgetId(EditorWidget::ScriptSourceBase)) {
        if(code_.open(files_,script->source)) {state_.code=&code_;state_.workspace=EditorWorkspace::Code;} else state_.status=code_.error();
      } else if(operation==widgetId(EditorWidget::ScriptArraySizeBase) || operation==widgetId(EditorWidget::ScriptArrayElementBase) ||
                operation==widgetId(EditorWidget::ScriptArrayAddBase) || operation==widgetId(EditorWidget::ScriptArrayRemoveBase)) {
        const auto *declared=scriptProperty(script->scriptType,state_.expandedScriptArray);
        const auto element=declared?scene::scriptArrayElementType(declared->valueType):std::string_view{};
        if(element.empty()) return true;
        auto items=scriptArrayItems(*script,declared->id);
        const u32 at=(key&0x00ffffffu)>>8;
        if(operation==widgetId(EditorWidget::ScriptArraySizeBase)) {
          state_.editingScriptInstance=script->instanceId();state_.editingScriptEntity=entity->id;
          state_.editingScriptProperty=declared->id;state_.editingScriptType="int32";state_.editingScriptArraySize=true;
        } else if(operation==widgetId(EditorWidget::ScriptArrayElementBase)) {
          if(at>=items.size()) return true;
          state_.scriptArraySelected=at+1;
          if(scene::scriptCurveType(element)) {
            state_.curveEntity=entity->id;state_.curveInstance=script->instanceId();state_.curveProperty=declared->id;
            state_.curveElement=at+1;
            openCurveEditor(key,items[at],declared->valueType);
          } else if(scene::scriptGradientType(element)) {
            state_.gradientEntity=entity->id;state_.gradientInstance=script->instanceId();state_.gradientProperty=declared->id;
            state_.gradientElement=at+1;
            openGradientEditor(key,items[at],element);
            state_.gradientType=declared->valueType;
          } else if(bool hdr=false,alpha=true;scene::scriptColorType(element,&hdr,&alpha)) {
            float rgba[4]{1,1,1,1};scene::parseScriptColor(items[at],rgba);
            state_.colorEntity=entity->id;state_.colorInstance=script->instanceId();state_.colorProperty=declared->id;
            state_.colorTarget=2;state_.colorScriptType=declared->valueType;state_.colorScriptElement=at+1;
            openColorWindow(key,rgba,alpha,hdr);
          } else if(element=="bool") {
            items[at]=items[at]=="true"?"false":"true";
            setScriptArray(entity->id,script->instanceId(),declared->id,declared->valueType,items);
          } else if(element=="object" || !scene::scriptComponentTypeId(element).empty()) {
            state_.referenceInstance=script->instanceId();state_.referenceProperty=declared->id;state_.referenceScript=true;
            state_.referenceScriptType=std::string(element);state_.referenceScriptElement=at+1;
            state_.referenceQuery.clear();state_.referencePage=0;
          } else {
            state_.editingScriptInstance=script->instanceId();state_.editingScriptEntity=entity->id;
            state_.editingScriptProperty=declared->id;state_.editingScriptType=std::string(element);state_.editingScriptElement=at+1;
          }
        } else if(operation==widgetId(EditorWidget::ScriptArrayAddBase)) {
          // Unity: o elemento novo copia o anterior.
          items.push_back(items.empty()?scene::scriptElementDefault(element):items.back());
          if(items.size()>scene::kScriptArrayMaximum) {state_.status="Limite de 1024 elementos";return true;}
          if(setScriptArray(entity->id,script->instanceId(),declared->id,declared->valueType,items)) state_.scriptArraySelected=items.size();
        } else if(!items.empty()) {
          const usize removed=state_.scriptArraySelected&&state_.scriptArraySelected<=items.size()?state_.scriptArraySelected-1:items.size()-1;
          items.erase(items.begin()+static_cast<std::ptrdiff_t>(removed));
          if(setScriptArray(entity->id,script->instanceId(),declared->id,declared->valueType,items)) state_.scriptArraySelected=0;
        }
      } else if(operation==widgetId(EditorWidget::ScriptFieldBase)) {
        const u32 field=(key&0x00ffffffu)>>8;
        for(const auto &type:code_.scriptTypes()) if(type.id==script->scriptType && field<type.properties.size()) {
          const auto &property=type.properties[field];
          if(!scene::scriptArrayElementType(property.valueType).empty()) {
            for(const auto &issue:state_.scriptFieldIssues) if(issue.instance==script->instanceId() && issue.property==property.id && issue.isNull) {
              if(setScriptArray(entity->id,script->instanceId(),property.id,property.valueType,{})) state_.expandedScriptArray=property.id;
              return true;
            }
            // Lista: o toque abre e fecha os elementos logo abaixo do campo.
            state_.expandedScriptArray=state_.expandedScriptArray==property.id?std::string():property.id;
            state_.scriptArraySelected=0;return true;
          }
          if(scene::scriptCurveType(property.valueType)) {
            std::string value=scene::scriptElementDefault(property.valueType);
            for(const auto &p:script->properties) if(p.id==property.id&&p.valueType==property.valueType) value=p.value;
            state_.curveEntity=entity->id;state_.curveInstance=script->instanceId();state_.curveProperty=property.id;
            state_.curveElement=0;
            openCurveEditor(key,value,property.valueType);return true;
          }
          if(scene::scriptGradientType(property.valueType)) {
            std::string value=scene::scriptElementDefault(property.valueType);
            for(const auto &p:script->properties) if(p.id==property.id&&p.valueType==property.valueType) value=p.value;
            state_.gradientEntity=entity->id;state_.gradientInstance=script->instanceId();state_.gradientProperty=property.id;
            state_.gradientElement=0;
            openGradientEditor(key,value,property.valueType);return true;
          }
          if(bool hdr=false,alpha=true;scene::scriptColorType(property.valueType,&hdr,&alpha)) {
            float rgba[4]{1,1,1,1};
            for(const auto &p:script->properties) if(p.id==property.id&&p.valueType==property.valueType) scene::parseScriptColor(p.value,rgba);
            state_.colorEntity=entity->id;state_.colorInstance=script->instanceId();state_.colorProperty=property.id;
            state_.colorTarget=1;state_.colorScriptType=property.valueType;state_.colorScriptElement=0;
            openColorWindow(key,rgba,alpha,hdr);return true;
          }
          if(property.valueType=="bool") {
            // Interruptor: grava direto, como a caixa de seleção da Unity.
            bool on=false;for(const auto &p:script->properties) if(p.id==property.id&&p.valueType=="bool") on=p.value=="true";
            EditorActionRequest request;request.version=sceneVersion();request.entity=entity->id;
            request.componentInstance=script->instanceId();request.action=EditorAction::ScriptProperty;
            request.componentProperty=property.id;request.scriptPropertyType="bool";request.scriptPropertyValue=on?"false":"true";
            dispatch(request);return true;
          }
          if(property.valueType=="object" || !scene::scriptComponentTypeId(property.valueType).empty()) {
            state_.referenceInstance=script->instanceId();state_.referenceProperty=property.id;state_.referenceScript=true;
            state_.referenceScriptType=property.valueType;state_.referenceScriptElement=0;
            state_.referenceQuery.clear();state_.referencePage=0;return true;
          }
          state_.editingScriptInstance=script->instanceId();state_.editingScriptEntity=entity->id;
          state_.editingScriptProperty=property.id;state_.editingScriptType=property.valueType;
        }
      } else {
        auto value=*entity;
        if(operation==widgetId(EditorWidget::ScriptEnabledBase)) {
          auto replacement=*script;replacement.enabled=!replacement.enabled;
          if(!value.components.replaceInstance(script->instanceId(),replacement)) return true;
        } else return true;
        history_.applyValues(document_,state_.selection,value);
      }
      return true;
    }
    if(key==widgetId(EditorWidget::CodeOpen)) {state_.code=&code_;state_.workspace=EditorWorkspace::Code;return true;}
    if(state_.workspace==EditorWorkspace::Code) {
      if(key>=widgetId(EditorWidget::CodeToolBase) && key-widgetId(EditorWidget::CodeToolBase)<state_.codeToolCatalog.size()) {
        requestEditorTools(state_.codeToolCatalog[key-widgetId(EditorWidget::CodeToolBase)].id);return true;
      }
      if(key>=widgetId(EditorWidget::CodeTabBase) && key-widgetId(EditorWidget::CodeTabBase)<code_.buffers().size()) {
        code_.select(code_.buffers()[key-widgetId(EditorWidget::CodeTabBase)].id);return true;
      }
      switch(static_cast<EditorWidget>(key)) {
      case EditorWidget::CodeApply:
        state_.codeMenu=false;
        if(state_.codeCompilerAvailable && !state_.codeBuildBusy) {
          if(files_.rootPath().empty()) state_.status="Abra um projeto antes de aplicar código";
          else if(!code_.saveAll(files_)) state_.status=code_.error();
          else {codeBuildGeneration_=code_.generation();codeBuildRequest_=files_.rootPath();state_.codeBuildBusy=true;state_.status="Compilando código do projeto";}
        }
        return true;
      case EditorWidget::CodeMenu:state_.codeMenu=!state_.codeMenu;state_.codeTools=false;return true;
      case EditorWidget::CodeTools:state_.codeTools=true;state_.codeToolsPage=0;requestEditorTools();return true;
      case EditorWidget::CodeToolsBack:state_.codeTools=false;return true;
      case EditorWidget::CodeToolsPrevious:if(state_.codeToolsPage) --state_.codeToolsPage;return true;
      case EditorWidget::CodeToolsNext:if((state_.codeToolsPage+1)*6<state_.codeToolCatalog.size()) ++state_.codeToolsPage;return true;
      case EditorWidget::CodeFiles:state_.codeFiles=!state_.codeFiles;state_.codeMenu=false;return true;
      case EditorWidget::CodeConsole:state_.codeFiles=false;state_.codeMenu=false;state_.consoleCollapsed=false;state_.consoleExpanded=true;return true;
      case EditorWidget::CodeTabsPrevious:if(state_.codeFirstTab) --state_.codeFirstTab;return true;
      case EditorWidget::CodeTabsNext:if(state_.codeFirstTab+1<code_.buffers().size()) ++state_.codeFirstTab;return true;
      case EditorWidget::CodeNewFolder:state_.creatingCodeFolder=true;return true;
      case EditorWidget::CodeGoLine:state_.goingToLine=true;state_.codeMenu=false;return true;
      case EditorWidget::CodeFindPrevious:
      case EditorWidget::CodeFindNext: {
        auto *buffer=code_.active();const auto matches=code_.find(state_.codeQuery);
        if(buffer && !matches.empty()) {
          usize index=0;
          if(key==widgetId(EditorWidget::CodeFindNext)) {
            while(index<matches.size() && matches[index].offset<=buffer->selectionStart) ++index;
            if(index==matches.size()) index=0;
          } else {
            index=matches.size()-1;
            for(usize i=0;i<matches.size();++i) if(matches[i].offset<buffer->selectionStart) index=i;
          }
          buffer->selectionStart=static_cast<u32>(matches[index].offset);
          buffer->selectionEnd=static_cast<u32>(matches[index].offset+matches[index].length);
          buffer->firstLine=matches[index].line-1;++buffer->viewRevision;
          state_.status=std::to_string(index+1)+" / "+std::to_string(matches.size());
        }
        return true;
      }
      case EditorWidget::CodeSaveAll:
        state_.codeMenu=false;
        if(!code_.saveAll(files_)) state_.status=code_.error();
        else state_.status="Arquivos de código salvos";
        return true;
      case EditorWidget::CodeScene:state_.codeMenu=false;state_.workspace=EditorWorkspace::Scene;return true;
      case EditorWidget::CodeNew:state_.creatingScript=true;state_.scriptTemplate=~0u;state_.codeMenu=false;return true;
      case EditorWidget::CodeNewHelper:state_.creatingScript=true;state_.scriptTemplate=EditorCodeWorkspace::HelperTemplate;state_.codeMenu=false;return true;
      case EditorWidget::CodeTemplates:state_.choosingTemplate=true;state_.codeMenu=false;return true;
      case EditorWidget::CodeTemplateClose:state_.choosingTemplate=false;return true;
      case EditorWidget::CodeEdit:state_.editingCode=code_.active()!=nullptr;return true;
      case EditorWidget::CodeSave:state_.status=code_.save(files_)?"Código salvo":code_.error();return true;
      case EditorWidget::CodeUndo:state_.codeMenu=false;codeHistoryAction(false);return true;
      case EditorWidget::CodeRedo:state_.codeMenu=false;codeHistoryAction(true);return true;
      case EditorWidget::CodeSearch:
        state_.codeMenu=false;
        if(state_.platformCodeView) ++state_.codeSearchRequest;
        else state_.searchingCode=true;
        return true;
      case EditorWidget::CodeClose:
        state_.codeMenu=false;
        if(const auto *buffer=code_.active()) if(!code_.close(buffer->id)) state_.status=code_.error();
        return true;
      default:break;
      }
    }
  }
  if(routing.tapped && (routing.widgetId&0xff000000u)==widgetId(EditorWidget::CodeTemplateBase) &&
     state_.choosingTemplate) {
    const u32 choice=routing.widgetId-widgetId(EditorWidget::CodeTemplateBase);
    if(choice<=editorScriptTemplates.size()) {
      // Zero é o arquivo vazio; os demais deslocam um por causa dele.
      state_.scriptTemplate=choice?choice-1:~0u;
      state_.choosingTemplate=false;
      state_.creatingScript=true;
    }
    return true;
  }
  if(routing.widgetId==widgetId(EditorWidget::FilesSplitter)) {
    if(routing.dragging && !state_.filesCollapsed) {
      const float height=layout_.hierarchyPanel.height+layout_.filesPanel.height;
      if(height>0) state_.filePanelRatio=std::clamp(state_.filePanelRatio-routing.stepDelta.y/height,.28f,.58f);
    }
    return true;
  }
  // Toque longo no olho ou na mão: só o objeto, sem os descendentes (Unity:
  // Alt+clique).
  if(routing.tapped && routing.heldSeconds>=ui::kUiLongPressSeconds) {
    const u32 key=routing.widgetId;
    const bool eye=key>=widgetId(EditorWidget::HierarchyEyeBase) && key<widgetId(EditorWidget::HierarchyEyeBase)+EditorDocument::kMaximumEntities;
    const bool hand=key>=widgetId(EditorWidget::HierarchyPickBase) && key<widgetId(EditorWidget::HierarchyPickBase)+EditorDocument::kMaximumEntities;
    if(eye || hand) {
      const EditorEntityId id=key-widgetId(eye?EditorWidget::HierarchyEyeBase:EditorWidget::HierarchyPickBase);
      auto &list=eye?state_.sceneHidden:state_.scenePickOff;
      auto found=std::find(list.begin(),list.end(),id);
      if(found!=list.end()) list.erase(found); else if(document_.exists(id)) list.push_back(id);
      state_.sceneVisibilityChanged=true;
      return true;
    }
  }
  // Toque longo num recurso em Arquivos: abre-o numa janela focada (Unity:
  // Properties), sem trocar o que Propriedades mostra.
  if(state_.files && routing.tapped && routing.heldSeconds>=ui::kUiLongPressSeconds &&
     routing.widgetId>=widgetId(EditorWidget::FileRowBase) && routing.widgetId-widgetId(EditorWidget::FileRowBase)<files_.tree().size()) {
    const auto &entry=files_.tree()[routing.widgetId-widgetId(EditorWidget::FileRowBase)];
    using Kind=EditorScreenState::FocusedAsset;
    if(const auto *record=assets_.findByPath(entry.relativePath)) {
      const Kind kind=record->type==resources::AssetType::Material?Kind::Material:
          record->type==resources::AssetType::EnvironmentMap?Kind::EnvironmentMap:
          (record->type==resources::AssetType::EnvironmentProfile||record->type==resources::AssetType::SkyboxMaterial)?
          Kind::EnvironmentProfile:Kind::None;
      if(kind!=Kind::None && openFocusedAsset(kind,record->guid)) return true;
    }
  }
  if(state_.files && routing.dragging && routing.widgetId>=widgetId(EditorWidget::FileRowBase)
      && routing.widgetId-widgetId(EditorWidget::FileRowBase)<files_.tree().size()) {
    state_.fileScrollOffset=std::max(0.0f,state_.fileScrollOffset-routing.stepDelta.y);
    const float row=state_.workspace==EditorWorkspace::Code?34.0f:24.0f;
    const auto visible=static_cast<u32>(std::max(row,layout_.filesPanel.height-(state_.workspace==EditorWorkspace::Code?126:66))/row);
    const auto maximum=files_.tree().size()>visible?files_.tree().size()-visible:0;
    state_.fileScrollOffset=std::min(state_.fileScrollOffset,static_cast<float>(maximum)*row);
    state_.fileScroll=static_cast<u32>(state_.fileScrollOffset/row);
    return true;
  }
  if(routing.tapped && (routing.widgetId>=widgetId(EditorWidget::TweenRestart)&&routing.widgetId<widgetId(EditorWidget::TweenRestart)+0x10000u)) {
    if(!isPlaying()||!playScene_.active()) return false;
    auto &world=playScene_.world();
    const auto target=state_.inspectorTarget?state_.inspectorTarget:state_.selection;
    const u32 key=routing.widgetId-widgetId(EditorWidget::TweenRestart);
    const auto *object=world.find(world.handle(target));
    if(!object||key/4>=object->components.size()||key%4==3)return false;
    const auto component=runtime::ComponentHandle{world.handle(target),object->components.at(key/4)->instanceId()};
    const auto *value=world.readComponent(component);
    if(!value||value->type().id!=scene::TransformTween::descriptor.id) return false;
    const bool restart=key%4==0;
    const auto *live=playScene_.tweens().state(target,component.instance);
    const u32 operation=restart?1:key%4==1?2:live&&live->paused?4:3;
    runtime::SceneTweens::State snapshot;
    const bool accepted=playScene_.tweens().command(world,component,operation,snapshot)==runtime::WorldStatus::Ok;
    state_.status=accepted?(operation==3?"Tween pausado":operation==4?"Tween retomado":restart?"Tween reiniciado no mundo Play":"Tween cancelado; pose atual preservada"):"Tween indisponível no mundo Play";
    return accepted;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::ImportWave)) {
    if(isPlaying() || history_.isOpen()) return false;
    state_.waveImportRequested=true;state_.codeFiles=false;state_.creationMenu=false;
    return true;
  }
  if(routing.tapped && state_.files && !history_.isOpen() && !isPlaying()) {
    const auto key=routing.widgetId;
    if(key==widgetId(EditorWidget::FilesCollapse)) {state_.filesCollapsed=!state_.filesCollapsed;return true;}
    const u32 base=widgetId(EditorWidget::FileRowBase);
    // Vários arquivos (Unity: Ctrl/Shift+clique no Project): um modo no toque.
    if(key==widgetId(EditorWidget::FilesMultiToggle)) {
      state_.filesMultiSelect=!state_.filesMultiSelect;
      if(!state_.filesMultiSelect && state_.selectedFiles.size()>1)
        selectFiles(state_.selectedFile.empty()?std::vector<std::string>{}:std::vector<std::string>{state_.selectedFile});
      state_.status=state_.filesMultiSelect?"Selecionar vários: toque nos arquivos para somar ou tirar":"Seleção única em Arquivos";
      return true;
    }
    if(key==widgetId(EditorWidget::FilesSelectNone)) {selectFiles({});return true;}
    // Inspector de vários recursos.
    const auto &view=state_.multiAsset;
    if(key>=widgetId(EditorWidget::MultiAssetNarrowBase) && key<widgetId(EditorWidget::MultiAssetNarrowBase)+view.groups.size()) {
      // Unity: "Narrow the Selection" — só os arquivos daquele tipo.
      const auto &group=view.groups[key-widgetId(EditorWidget::MultiAssetNarrowBase)];
      std::vector<std::string> kept;
      for(usize i=0;i<state_.selectedFiles.size() && i<multiAssetGroups_.size();++i)
        if(fileKinds[multiAssetGroups_[i]].plural==group.label) kept.push_back(state_.selectedFiles[i]);
      selectFiles(std::move(kept));return true;
    }
    if(key>=widgetId(EditorWidget::MultiAssetItemBase) && key<widgetId(EditorWidget::MultiAssetItemBase)+view.items.size()) {
      selectFiles({view.items[key-widgetId(EditorWidget::MultiAssetItemBase)].path});return true;
    }
    if(key>=widgetId(EditorWidget::MultiAssetRemoveBase) && key<widgetId(EditorWidget::MultiAssetRemoveBase)+view.items.size()) {
      auto paths=state_.selectedFiles;const auto removed=view.items[key-widgetId(EditorWidget::MultiAssetRemoveBase)].path;
      std::erase(paths,removed);
      // O ativo continua o ativo; tirado ele, o último da lista assume.
      if(removed!=state_.selectedFile) if(auto found=std::find(paths.begin(),paths.end(),state_.selectedFile);found!=paths.end()) {
        paths.erase(found);paths.push_back(state_.selectedFile);
      }
      selectFiles(std::move(paths));return true;
    }
    if(view.kind==EditorScreenState::MultiAssetView::Kind::Textures && !multiTextures_.empty()) {
      if(key>=widgetId(EditorWidget::MultiAssetFieldBase) && key<widgetId(EditorWidget::MultiAssetFieldBase)+TextureProfileFields) {
        const u32 field=key-widgetId(EditorWidget::MultiAssetFieldBase);
        const bool mixed=(view.mixed>>field)&1u;
        if(routing.heldSeconds>=ui::kUiLongPressSeconds) {
          // Unity: clique direito › Set to Value of — de qual textura copiar.
          if(!mixed) {state_.status="Todas já têm o mesmo valor";return true;}
          const auto text=textureProfileFieldText(multiTextures_.front().draft,field);
          state_.setValueMenu.key="asset.tex."+std::to_string(field);state_.setValueMenu.label=text.substr(0,text.find(": "));
          state_.setValueMenu.rows.clear();
          for(u32 i=0;i<multiTextures_.size();++i) {
            const auto *record=assets_.find(multiTextures_[i].guid);
            const auto value=textureProfileFieldText(multiTextures_[i].draft,field);
            state_.setValueMenu.rows.push_back({i,(record?baseName(record->path):std::string("?"))+"  \xC2\xB7  "+value.substr(value.find(": ")+2)});
          }
          return true;
        }
        // O passo parte do valor da textura ativa; liga/desliga misturado liga
        // todas (como o toggle "—" da Unity).
        const auto *active=assets_.findByPath(state_.selectedFile);
        auto reference=multiTextures_.front().draft;
        for(const auto &texture:multiTextures_) if(active && texture.guid==active->guid) reference=texture.draft;
        if(mixed && textureProfileToggle(field)) setTextureProfileToggle(reference,field,true);
        else stepTextureProfileField(reference,field);
        for(auto &texture:multiTextures_) copyTextureProfileField(texture.draft,reference,field);
        return true;
      }
      if(key==widgetId(EditorWidget::MultiAssetRevert)) {
        for(auto &texture:multiTextures_) texture.draft=texture.saved;
        state_.status="Rascunho descartado";return true;
      }
      if(key==widgetId(EditorWidget::MultiAssetApply)) {applyMultiTextureProfiles();return true;}
    }
    if(state_.filesMultiSelect && key>=base && key-base<files_.tree().size()) {
      // Pasta no modo: só abre ou fecha, sem mexer no que está escolhido.
      if(files_.tree()[key-base].directory) {files_.toggle(key-base);return true;}
      auto paths=state_.selectedFiles;const auto path=files_.tree()[key-base].relativePath;
      if(const auto found=std::find(paths.begin(),paths.end(),path);found!=paths.end()) paths.erase(found);
      else paths.push_back(path);
      selectFiles(std::move(paths));
      return true;
    }
    if(key==widgetId(EditorWidget::ImportModel)) {state_.modelImportRequested=true;state_.codeFiles=false;return true;}
    if(key==widgetId(EditorWidget::ImportEnvironment)) {state_.environmentImportRequested=true;state_.codeFiles=false;return true;}
    if(key==widgetId(EditorWidget::ImportTexture)) {state_.textureImportRequested=true;state_.codeFiles=false;return true;}
    if(key==widgetId(EditorWidget::AssetInstantiate)) {
      const auto *record=assets_.findByPath(state_.selectedFile);
      if(record && record->type==resources::AssetType::Prefab) {
        std::string error;
        if(!instantiatePrefab(record->guid,document_.root(),error)) state_.status=error;
        return true;
      }
      ModelImportReport report;
      if(!record) setImportStatus("Registre o recurso com Reimportar antes de instanciar.",EditorConsoleSeverity::Warning);
      else if(!instantiateModel(record->guid,report)) setImportStatus(report.diagnostic,EditorConsoleSeverity::Error);
      else setImportStatus("Instância criada: "+std::to_string(report.objects)+" objetos. Desfazer remove somente esta instância.");
      return true;
    }
    if(key==widgetId(EditorWidget::AssetReimport)) {
      if(state_.selectedFile.ends_with(".mat")) {
        std::string diagnostic;
        const auto imported=importUnityMaterial(state_.selectedFile,diagnostic);
        setImportStatus(diagnostic,imported.valid()?EditorConsoleSeverity::Info:EditorConsoleSeverity::Warning);
        return true;
      }
      const auto *record=assets_.findByPath(state_.selectedFile);
      if(record&&record->type==resources::AssetType::EnvironmentMap)
        environmentReimportPath_=state_.selectedFile;
      else if(record&&record->type==resources::AssetType::Texture)
        textureReimportPath_=state_.selectedFile;
      else reimportPath_=state_.selectedFile;
      return true;
    }
    if(key==widgetId(EditorWidget::AssetExtractTextures)) {
      TextureExtraction report;std::string diagnostic;
      if(extractSourceTextures(state_.selectedFile,report,diagnostic)) setImportStatus(state_.status);
      else setImportStatus(diagnostic.empty()?std::string("Não foi possível extrair as texturas."):diagnostic,EditorConsoleSeverity::Warning);
      return true;
    }
    if(key==widgetId(EditorWidget::FilesRename)) {
      if(!state_.selectedFile.empty()) state_.renamingResource=true;
      return true;
    }
    if(key==widgetId(EditorWidget::FilesDelete)) {
      if(state_.selectedFile.empty()) return true;
      // Dois toques, nao um dialogo. O primeiro confronta quem usa e conta; o
      // segundo, no MESMO arquivo, confirma. Mudar de arquivo cancela.
      const bool force=state_.pendingResourceDelete==state_.selectedFile;
      ResourceChangeReport report;
      if(deleteResource(state_.selectedFile,force,report)) {
        state_.pendingResourceDelete.clear();
        state_.selectedFile.clear();
      } else if(!force && (report.sceneUsers>0 || report.registryDependents>0)) {
        state_.pendingResourceDelete=state_.selectedFile;
        state_.status=report.diagnostic+" Toque de novo para apagar mesmo assim.";
      } else {
        state_.pendingResourceDelete.clear();
        state_.status=report.diagnostic;
      }
      return true;
    }
    if(key>=base && key-base<files_.tree().size()) {
      const auto entry=files_.tree()[key-base];
      if(entry.directory) files_.toggle(key-base);
      else state_.selectedFiles={entry.relativePath};
      openProjectFile(entry);
      return true;
    }
  }
  if(state_.draggingAsset && event.pointerId!=assetPointer_) return true;
  if(state_.draggingEntity && event.pointerId!=hierarchyPointer_) return true;
  if(isPlaying() && routing.target==UiPointerTarget::Viewport) {
    if(event.device==UiPointerDevice::Mouse) return false;
    const auto view=sceneCameraPose();const auto *cameraEntity=document_.find(view.entity);
    const bool canLook=cameraEntity&&cameraLook(*cameraEntity);
    const bool canMove=playScene_.motorReceiver(view.entity,state_.selection)!=0;
    if((!state_.playHasScripts && !canMove && !canLook) || state_.playPaused) return true;
    const float x=event.position.x-layout_.viewport.x,y=event.position.y-layout_.viewport.y;
    if(event.phase==UiPointerPhase::Down) playTouches_.pointerDown(event.pointerId,x,y,layout_.viewport.width,layout_.viewport.height);
    else if(event.phase==UiPointerPhase::Move) playTouches_.pointerMove(event.pointerId,x,y,layout_.viewport.width,layout_.viewport.height);
    else if(event.phase==UiPointerPhase::Up) playTouches_.pointerUp(event.pointerId);
    else playTouches_.cancel();
    return true;
  }
  if(routing.tapped) for(u32 i=0;i<editorCreationCatalog.size();++i)
    if(routing.widgetId==creationWidget(i) && !creationAvailable(state_,i)) return true;
  if(routing.tapped && state_.creationMenu) {
    const auto key=routing.widgetId;
    if(key>=widgetId(EditorWidget::CreationCategoryBase)&&key<widgetId(EditorWidget::CreationCategoryBase)+std::size(creationCategories)) {
      state_.creationCategory=key-widgetId(EditorWidget::CreationCategoryBase);state_.creationScroll=0;state_.creationSearch[0]=0;
      for(u32 i=0;i<editorCreationCatalog.size();++i) if(creationAvailable(state_,i) && editorCreationCatalog[i].category==state_.creationCategory) {state_.creationSelection=i;break;}
      return true;
    }
    if(key>=widgetId(EditorWidget::CreationRowBase)&&key<widgetId(EditorWidget::CreationRowBase)+editorCreationCatalog.size()) {
      state_.creationSelection=key-widgetId(EditorWidget::CreationRowBase);return true;
    }
  }
  if (state_.renameEntity != kInvalidEntity || state_.editingHierarchySearch || state_.editingCreationSearch || state_.editingComponentSearch || state_.editingPropertySearch || state_.editingMeshSearch || state_.editingReferenceSearch || state_.editingGlobalSearch || state_.namingLayout || state_.presetNaming || state_.viewNaming || state_.editingInputActionName || state_.editingInputContext || state_.editingTagName || state_.editingTagSearch || state_.editingGroupName || state_.editingPhysicsLayerName || state_.editingAnimatorName) {
    if(routing.tapped) {
      const auto key=routing.widgetId;
      auto n=std::strlen(state_.renameText);
      if(key==widgetId(EditorWidget::NameCancel)) {completeTextEdit(pendingTextEdit(),{},false);}
      else if(key==widgetId(EditorWidget::NameClear)) state_.renameText[0]=0;
      else if(key==widgetId(EditorWidget::NameShift)) state_.renameUppercase=!state_.renameUppercase;
      else if(key==widgetId(EditorWidget::NameBackspace) && n) {
        do {--n;} while(n && (static_cast<unsigned char>(state_.renameText[n])&0xc0)==0x80);
        state_.renameText[n]=0;
      } else if(key>=widgetId(EditorWidget::NameKeyBase) && key<widgetId(EditorWidget::NameKeyBase)+40 && n+1<sizeof(state_.renameText)) {
        const char *keys=state_.renameUppercase?"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-. ":"abcdefghijklmnopqrstuvwxyz0123456789_-. ";
        state_.renameText[n]=keys[key-widgetId(EditorWidget::NameKeyBase)];state_.renameText[n+1]=0;
      } else if(key==widgetId(EditorWidget::NameApply)) {
        completeTextEdit(pendingTextEdit(),state_.renameText,true);
      }
    }
    return true;
  }
  if (state_.numericField != 0) {
    if (routing.tapped) {
      const u32 keyBase = widgetId(EditorWidget::NumericKeyBase);
      const u32 key = routing.widgetId;
      // Teclas de expressão: o texto inserido é o da sintaxe (`*`, `pi`), o
      // rótulo é o da tela (`×`, `pi`). As relativas trocam o prefixo.
      static constexpr const char *inserts[]{"1","2","3","4","5","6","7","8","9",".","0","-"," ",
                                            "+","*","/","(",")","^","pi",",","+=","-=","*=","/=","L(","R(","sqrt("};
      if (key == widgetId(EditorWidget::NumericCancel)) completeTextEdit(pendingTextEdit(),{},false);
      else if (key == widgetId(EditorWidget::NumericClear)) {state_.numericText[0] = 0;state_.numericError=false;}
      else if (key == widgetId(EditorWidget::NumericBackspace)) {
        const auto n=std::strlen(state_.numericText);if(n) state_.numericText[n-1]=0;
        state_.numericReplace=false;state_.numericError=false;
      } else if (key >= keyBase && key < keyBase+std::size(inserts)) {
        const u32 index=key-keyBase;
        if(state_.numericReplace) {state_.numericText[0]=0;state_.numericReplace=false;}
        std::string text=state_.numericText;
        if(index>=21 && index<=24) {
          if(text.size()>=2 && text[1]=='=' && std::strchr("+-*/",text[0])) text.erase(0,2);
          text.insert(0,inserts[index]);
        } else text+=inserts[index];
        if(text.size()<sizeof(state_.numericText)) std::snprintf(state_.numericText,sizeof(state_.numericText),"%s",text.c_str());
        state_.numericError=false;
      } else if (key == widgetId(EditorWidget::NumericApply)) {
        completeTextEdit(pendingTextEdit(),state_.numericText,true);
      }
    }
    return true;
  }
  if(routing.tapped && (routing.widgetId==widgetId(EditorWidget::CreateGround) || routing.widgetId==widgetId(EditorWidget::CreateCube))) {
    const bool ground=routing.widgetId==widgetId(EditorWidget::CreateGround);
    const auto parent=creationParent();
    for(u32 i=0;i<mapScene_.assetCount();++i) if(mapScene_.materialFlagsForAsset(i)&renderer::BoxAuthoringResource) {
      const float position[]{camera_.target[0],ground?-.1f:.5f,camera_.target[2]};
      EditorAssetInstantiation options;options.name=ground?"Chão":"Cubo";
      if(ground) {options.scale[0]=20;options.scale[1]=.2f;options.scale[2]=20;}
      if(instantiateAsset(i,parent,position,&options)) {state_.creationMenu=false;frameSelection();}
      break;
    }
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::QualityOpen)) {
    state_.qualityPanel=!state_.qualityPanel;return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::QualityTemporalDebugQuick)) {
    if(state_.qualityTemporalAvailable && state_.workspace==EditorWorkspace::Scene)
      state_.qualityTemporalDebug=nextTemporalDebugView();
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::QualityTemporalModeQuick)) {
    // Controle rápido: aplica na hora, sobre o que está salvo, e pula o que o
    // aparelho recusa dizendo por quê.
    using Mode=renderer::TemporalReconstruction;
    auto next=renderingSettings_;
    auto mode=renderer::temporalReconstruction(next);
    std::string skipped;
    for(u32 attempt=0;attempt<4;++attempt) {
      mode=static_cast<Mode>((static_cast<u32>(mode)+1u)%4u);
      const auto availability=mode==Mode::ArmAsr?state_.qualityArmAsr:mode==Mode::Fsr2?state_.qualityFsr2:
                              renderer::TemporalUpscalerAvailability::Available;
      if(availability==renderer::TemporalUpscalerAvailability::Available) break;
      if(!skipped.empty()) skipped+="; ";
      skipped+=std::string(renderer::temporalReconstructionLabel(mode))+" indisponível: "+
               renderer::temporalUpscalerAvailabilityLabel(availability);
    }
    if(renderingSettingsRequestInFlight_ || renderingSettingsRequested_) {
      state_.status="Aguarde a aplicação anterior da qualidade";return true;
    }
    renderer::setTemporalReconstruction(next,mode);
    requestedRenderingSettings_=next;
    renderingSettingsRequestRevision_=qualityRevision_;
    renderingSettingsRequested_=true;
    if(!state_.qualityDirty) state_.qualityDraft=next;
    state_.status=std::string("Ampliação temporal: ")+renderer::temporalReconstructionLabel(mode)+
                  (skipped.empty()?std::string():" ("+skipped+")");
    return true;
  }
  if(state_.lightExplorer && routing.tapped) {
    const auto key=routing.widgetId;
    const auto is=[&](EditorWidget widget) {return key==widgetId(widget);};
    // Degraus da intensidade em lote: cobrem lux de interior a sol e lúmen de
    // vela a holofote, na unidade de cada luz.
    static constexpr float steps[]{0,1,5,10,50,100,250,500,1000,2500,5000,10000,25000,50000,100000};
    if(is(EditorWidget::LightExplorerClose)) {state_.lightExplorer=false;return true;}
    if(is(EditorWidget::LightExplorerFilter)) {
      state_.lightExplorerDarkOnly=!state_.lightExplorerDarkOnly;state_.lightExplorerPage=0;refreshLightExplorer();return true;
    }
    if(is(EditorWidget::LightExplorerPrevious)) {
      if(state_.lightExplorerPage) --state_.lightExplorerPage;
      refreshLightExplorer();return true;
    }
    if(is(EditorWidget::LightExplorerNext)) {++state_.lightExplorerPage;refreshLightExplorer();return true;}
    if(is(EditorWidget::LightExplorerIntensityDown) || is(EditorWidget::LightExplorerIntensityUp)) {
      usize index=0;
      for(usize i=0;i<std::size(steps);++i) if(steps[i]<=state_.lightExplorerIntensity) index=i;
      if(is(EditorWidget::LightExplorerIntensityUp)) index=std::min(index+1,std::size(steps)-1);
      else if(index && steps[index]==state_.lightExplorerIntensity) --index;
      state_.lightExplorerIntensity=steps[index];return true;
    }
    if(is(EditorWidget::LightExplorerApply)) {
      const u32 changed=applyLightExplorerIntensity();
      state_.status=changed?std::to_string(changed)+" luz(es) com a intensidade nova":std::string("Nenhuma luz mudou");
      return true;
    }
    if(key>=widgetId(EditorWidget::LightExplorerRow0) && key<=widgetId(EditorWidget::LightExplorerRowLast)) {
      const u32 row=key-widgetId(EditorWidget::LightExplorerRow0);
      if(row<state_.lightExplorerRows.size()) setSelection(state_.lightExplorerRows[row].entity);
      return true;
    }
    if(key>=widgetId(EditorWidget::LightExplorerToggle0) && key<=widgetId(EditorWidget::LightExplorerToggleLast)) {
      const u32 row=key-widgetId(EditorWidget::LightExplorerToggle0);
      if(row>=state_.lightExplorerRows.size() || isPlaying() || history_.isOpen()) return true;
      const auto &target=state_.lightExplorerRows[row];
      if(const auto *entity=document_.find(target.entity)) {
        auto values=*entity;
        if(scene::setComponentProperty(values.components,scene::Light::descriptor.id,"enabled",!target.enabled,
                                       target.instance)==scene::ComponentPropertyStatus::Applied)
          history_.applyValues(document_,target.entity,values);
      }
      refreshLightExplorer();return true;
    }
  }
  if(state_.qualityPanel && routing.tapped) {
    auto &draft=state_.qualityDraft;
    const auto key=routing.widgetId;
    const auto is=[&](EditorWidget widget) {return key==widgetId(widget);};
    const auto cycleOverride=[](renderer::FeatureOverride value) {
      using Override=renderer::FeatureOverride;
      return value==Override::Inherit?Override::Disabled:value==Override::Disabled?Override::Enabled:Override::Inherit;
    };
    // Próximo/anterior numa escada fixa; valor fora dela volta ao degrau mais perto abaixo.
    const auto stepList=[](const auto &steps,u32 value,bool up) {
      usize index=0;
      for(usize i=0;i<std::size(steps);++i) if(steps[i]<=value) index=i;
      if(up) return steps[std::min(index+1,std::size(steps)-1)];
      return steps[index==0||steps[index]!=value?index:index-1];
    };
    const auto step=[](float value,float inherited,float initial,float delta,float minimum,float maximum,bool up) {
      const float current=value==inherited?initial:value;
      return std::round(std::clamp(current+(up?delta:-delta),minimum,maximum)/delta)*delta;
    };
    bool changed=true;
    if(is(EditorWidget::QualityClose)) {state_.qualityPanel=false;return true;}
    else if(is(EditorWidget::QualityTabGeneral)) {state_.qualityTab=0;state_.qualityPage=0;return true;}
    else if(is(EditorWidget::QualityTabShadows)) {state_.qualityTab=1;state_.qualityPage=0;return true;}
    else if(is(EditorWidget::QualityTabLighting)) {state_.qualityTab=2;state_.qualityPage=0;return true;}
    else if(is(EditorWidget::QualityTabPerformance)) {state_.qualityTab=3;state_.qualityPage=0;return true;}
    else if(is(EditorWidget::QualityTabTextures)) {state_.qualityTab=4;state_.qualityPage=0;return true;}
    else if(is(EditorWidget::LightExplorerOpen)) {
      state_.lightExplorer=true;state_.qualityPanel=false;state_.lightExplorerPage=0;refreshLightExplorer();return true;
    }
    else if(is(EditorWidget::QualitySceneStatistics)) {
      state_.sceneStatisticsVisible=!state_.sceneStatisticsVisible;return true;
    }
    else if(is(EditorWidget::QualityStreamingDebugView)) {
      state_.qualityTextureStreamingDebug=!state_.qualityTextureStreamingDebug;return true;
    }
    else if(is(EditorWidget::QualityPagePrevious)) {if(state_.qualityPage) --state_.qualityPage;return true;}
    else if(is(EditorWidget::QualityPageNext)) {++state_.qualityPage;return true;}
    else if(is(EditorWidget::QualityTemporalDebug)) {
      if(state_.qualityTemporalAvailable) state_.qualityTemporalDebug=nextTemporalDebugView();
      return true;
    }
    else if(is(EditorWidget::QualityTemporalMode)) {
      using Mode=renderer::TemporalReconstruction;
      const auto mode=renderer::temporalReconstruction(draft);
      renderer::setTemporalReconstruction(draft,static_cast<Mode>((static_cast<u32>(mode)+1u)%4u));
    } else if(is(EditorWidget::QualityTemporalQuality)) {
      using Quality=renderer::TemporalUpscalerQuality;
      draft.temporalUpscalerQuality=draft.temporalUpscalerQuality==Quality::Inherit?Quality::Quality:
          draft.temporalUpscalerQuality==Quality::Quality?Quality::Balanced:
          draft.temporalUpscalerQuality==Quality::Balanced?Quality::Performance:
          draft.temporalUpscalerQuality==Quality::Performance?Quality::UltraPerformance:Quality::Inherit;
    }
    else if(is(EditorWidget::QualityLevel)) {
      // Automático, Baixo, Médio, Alto, Ultra: a ordem dos níveis da Unity.
      using Preset=renderer::QualityPreset;
      draft.preset=draft.preset==Preset::Auto?Preset::C:draft.preset==Preset::C?Preset::B:
                   draft.preset==Preset::B?Preset::A:draft.preset==Preset::A?Preset::S:Preset::Auto;
    } else if(is(EditorWidget::QualityScaleDown) || is(EditorWidget::QualityScaleUp)) {
      // Passos de 5%, entre 50% e 100%, como o Render Scale do URP Asset.
      const float current=draft.resolutionScale>0?draft.resolutionScale:1.0f;
      if(is(EditorWidget::QualityScaleUp)&&current>=1.0f) draft.resolutionScale=0.0f;
      else {
        const float next=std::clamp(current+(is(EditorWidget::QualityScaleUp)?.05f:-.05f),.5f,1.0f);
        draft.resolutionScale=std::round(next*20.0f)/20.0f;
      }
    } else if(is(EditorWidget::QualityDynamic)) {
      draft.dynamicResolution=cycleOverride(draft.dynamicResolution);
    } else if(is(EditorWidget::QualityUpscaling)) {
      using Filter=renderer::UpscalingFilter;
      if(renderer::isTemporalUpscaler(draft.upscalingFilter)) return true; // o temporal substitui o espacial
      draft.upscalingFilter=draft.upscalingFilter==Filter::Inherit?Filter::Bilinear:
                            draft.upscalingFilter==Filter::Bilinear?Filter::CatmullRom:
                            draft.upscalingFilter==Filter::CatmullRom?Filter::Fsr1:Filter::Inherit;
    } else if(is(EditorWidget::QualityTextures)) {
      using Quality=renderer::TextureQuality;
      draft.textures=draft.textures==Quality::Inherit?Quality::Half:
                     draft.textures==Quality::Half?Quality::Full:Quality::Inherit;
    } else if(is(EditorWidget::QualityAntiAliasing)) {
      using Mode=renderer::AntiAliasingMode;
      if(renderer::isTemporalUpscaler(draft.upscalingFilter)) return true; // AA pertence ao ampliador
      draft.antiAliasing=draft.antiAliasing==Mode::Inherit?Mode::Off:draft.antiAliasing==Mode::Off?Mode::Fxaa:
                         draft.antiAliasing==Mode::Fxaa?Mode::Temporal:Mode::Inherit;
    } else if(is(EditorWidget::QualitySharpenDown) || is(EditorWidget::QualitySharpenUp)) {
      const float current=draft.postSharpen>=0?draft.postSharpen:0.0f;
      if(is(EditorWidget::QualitySharpenDown)&&draft.postSharpen==0.0f) draft.postSharpen=-1.0f;
      else draft.postSharpen=std::round(std::clamp(current+(is(EditorWidget::QualitySharpenUp)?.1f:-.1f),0.0f,1.0f)*10.0f)/10.0f;
    } else if(is(EditorWidget::QualityRate)) {
      draft.maximumRenderHz=draft.maximumRenderHz==30?60:draft.maximumRenderHz==60?90:
                            draft.maximumRenderHz==90?120:draft.maximumRenderHz==120?0:30;
    } else if(is(EditorWidget::QualityShadows)) {
      using Quality=renderer::ShadowQuality;
      draft.shadows=draft.shadows==Quality::Inherit?Quality::Off:draft.shadows==Quality::Off?Quality::Hard:
                    draft.shadows==Quality::Hard?Quality::Soft:draft.shadows==Quality::Soft?Quality::UltraSoft:Quality::Inherit;
    } else if(is(EditorWidget::QualityShadowCascades)) {
      draft.shadowCascadeCount=draft.shadowCascadeCount>=4?0:draft.shadowCascadeCount+1;
    } else if(is(EditorWidget::QualityShadowResolution)) {
      draft.shadowCascadeResolution=draft.shadowCascadeResolution==0?512:draft.shadowCascadeResolution==512?1024:
                                    draft.shadowCascadeResolution==1024?2048:draft.shadowCascadeResolution==2048?4096:0;
    } else if(is(EditorWidget::QualityShadowDistanceDown)||is(EditorWidget::QualityShadowDistanceUp)) {
      draft.shadowMaximumDistance=step(draft.shadowMaximumDistance,0.0f,160.0f,20.0f,20.0f,1000.0f,is(EditorWidget::QualityShadowDistanceUp));
    } else if(is(EditorWidget::QualityShadowBiasDown)||is(EditorWidget::QualityShadowBiasUp)) {
      draft.shadowDepthBiasConstant=step(draft.shadowDepthBiasConstant,-1.0f,1.0f,.1f,0.0f,10.0f,is(EditorWidget::QualityShadowBiasUp));
    } else if(is(EditorWidget::QualityShadowSlopeDown)||is(EditorWidget::QualityShadowSlopeUp)) {
      draft.shadowDepthBiasSlope=step(draft.shadowDepthBiasSlope,-1.0f,1.5f,.1f,0.0f,10.0f,is(EditorWidget::QualityShadowSlopeUp));
    } else if(is(EditorWidget::QualityShadowNormalDown)||is(EditorWidget::QualityShadowNormalUp)) {
      draft.shadowNormalOffsetTexels=step(draft.shadowNormalOffsetTexels,-1.0f,1.0f,.1f,0.0f,10.0f,is(EditorWidget::QualityShadowNormalUp));
    } else if(is(EditorWidget::QualityShadowCache)) {
      draft.staticShadowCache=cycleOverride(draft.staticShadowCache);
    } else if(is(EditorWidget::QualityAmbient)) {
      using Quality=renderer::AmbientQuality;
      draft.ambient=draft.ambient==Quality::Inherit?Quality::Constant:draft.ambient==Quality::Constant?Quality::Hemispheric:
                    draft.ambient==Quality::Hemispheric?Quality::HemisphericSpecular:Quality::Inherit;
    } else if(is(EditorWidget::QualityEnvironmentBrdf)) {
      draft.environmentSplitSumBrdf=cycleOverride(draft.environmentSplitSumBrdf);
    } else if(is(EditorWidget::QualityPost)) {
      using Quality=renderer::PostQuality;
      draft.post=draft.post==Quality::Inherit?Quality::None:draft.post==Quality::None?Quality::Tonemap:
                 draft.post==Quality::Tonemap?Quality::Bloom:Quality::Inherit;
    } else if(is(EditorWidget::QualityBloomThresholdDown)||is(EditorWidget::QualityBloomThresholdUp)) {
      draft.bloomThreshold=step(draft.bloomThreshold,-1.0f,1.0f,.1f,0.0f,8.0f,is(EditorWidget::QualityBloomThresholdUp));
    } else if(is(EditorWidget::QualityBloomIntensityDown)||is(EditorWidget::QualityBloomIntensityUp)) {
      draft.bloomIntensity=step(draft.bloomIntensity,-1.0f,.25f,.05f,0.0f,2.0f,is(EditorWidget::QualityBloomIntensityUp));
    } else if(is(EditorWidget::QualityTemporalWeightDown)||is(EditorWidget::QualityTemporalWeightUp)) {
      draft.temporalHistoryWeight=step(draft.temporalHistoryWeight,-1.0f,.88f,.01f,0.0f,.97f,is(EditorWidget::QualityTemporalWeightUp));
    } else if(is(EditorWidget::QualityVignette)) {
      draft.postVignette=cycleOverride(draft.postVignette);
    } else if(is(EditorWidget::QualityDynamicMinimumDown)||is(EditorWidget::QualityDynamicMinimumUp)) {
      draft.dynamicResolutionMinimumScale=step(draft.dynamicResolutionMinimumScale,0.0f,.70f,.05f,.50f,1.0f,is(EditorWidget::QualityDynamicMinimumUp));
    } else if(is(EditorWidget::QualityLodSelection)) {
      draft.lodSelection=cycleOverride(draft.lodSelection);
    } else if(is(EditorWidget::QualityLodErrorDown)||is(EditorWidget::QualityLodErrorUp)) {
      draft.lodPixelErrorBudget=step(draft.lodPixelErrorBudget,0.0f,1.0f,.25f,.25f,16.0f,is(EditorWidget::QualityLodErrorUp));
    } else if(is(EditorWidget::QualityLodHysteresisDown)||is(EditorWidget::QualityLodHysteresisUp)) {
      draft.lodHysteresisBandRatio=step(draft.lodHysteresisBandRatio,0.0f,.75f,.05f,.10f,.95f,is(EditorWidget::QualityLodHysteresisUp));
    } else if(is(EditorWidget::QualityMaterialVariants)) {
      draft.materialShaderVariants=cycleOverride(draft.materialShaderVariants);
    } else if(is(EditorWidget::QualityTextureStreaming)) {
      draft.textureStreaming=cycleOverride(draft.textureStreaming);
    } else if(is(EditorWidget::QualityStreamingBudgetDown)||is(EditorWidget::QualityStreamingBudgetUp)) {
      // Passos do Memory Budget; abaixo do menor volta a "Do nível".
      static constexpr u32 steps[]{0,32,64,128,256,384,512,768,1024,1536,2048,3072,4096};
      draft.textureStreamingBudgetMegabytes=stepList(steps,draft.textureStreamingBudgetMegabytes,is(EditorWidget::QualityStreamingBudgetUp));
    } else if(is(EditorWidget::QualityStreamingReductionDown)||is(EditorWidget::QualityStreamingReductionUp)) {
      static constexpr u32 steps[]{0,1,2,3,4,5,6,7};
      draft.textureStreamingMaxLevelReduction=stepList(steps,draft.textureStreamingMaxLevelReduction,is(EditorWidget::QualityStreamingReductionUp));
    } else if(is(EditorWidget::QualityStreamingUploadDown)||is(EditorWidget::QualityStreamingUploadUp)) {
      static constexpr u32 steps[]{0,512,1024,2048,4096,8192,16384,32768};
      draft.textureStreamingUploadKilobytesPerFrame=stepList(steps,draft.textureStreamingUploadKilobytesPerFrame,is(EditorWidget::QualityStreamingUploadUp));
    } else if(is(EditorWidget::QualityApply)) {
      // Um ampliador temporal que o aparelho recusa não é salvo como se fosse
      // funcionar: o motivo aparece e o rascunho continua pendente.
      if(renderer::isTemporalUpscaler(draft.upscalingFilter)) {
        const auto availability=draft.upscalingFilter==renderer::UpscalingFilter::ArmAsr?state_.qualityArmAsr:state_.qualityFsr2;
        if(availability!=renderer::TemporalUpscalerAvailability::Available) {
          state_.status=std::string(renderer::upscalingFilterLabel(draft.upscalingFilter))+" indisponível: "+
                        renderer::temporalUpscalerAvailabilityLabel(availability);
          return true;
        }
      }
      requestedRenderingSettings_=draft;
      renderingSettingsRequestRevision_=qualityRevision_;
      renderingSettingsRequested_=true;
      state_.status="Salvando qualidade do projeto";
      return true;
    } else changed=false;
    if(changed) {
      ++qualityRevision_;
      state_.qualityDirty=true;
      return true;
    }
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::CreateSceneTemplate)) {
    state_.templatePanel=true;state_.templatePage=0;state_.creationMenu=false;return true;
  }
  if(state_.templatePanel && routing.tapped) {
    if(routing.widgetId==widgetId(EditorWidget::SceneTemplateClose)) {state_.templatePanel=false;return true;}
    if(routing.widgetId==widgetId(EditorWidget::SceneTemplatePrevious)) {if(state_.templatePage)--state_.templatePage;return true;}
    if(routing.widgetId==widgetId(EditorWidget::SceneTemplateNext)) {++state_.templatePage;return true;}
    if(routing.widgetId>=widgetId(EditorWidget::SceneTemplateRowBase) &&
       routing.widgetId<widgetId(EditorWidget::SceneTemplateRowBase)+sceneTemplates().size()) {
      createSceneTemplate(routing.widgetId-widgetId(EditorWidget::SceneTemplateRowBase));
      state_.templatePanel=false;return true;
    }
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::ImportFolder)) {
    // Pasta inteira: o shell lista a pasta, e só os arquivos que o glTF
    // referencia são copiados para o projeto.
    state_.folderImportRequested=true;state_.creationMenu=false;state_.codeFiles=false;
    state_.status="Escolha a pasta do modelo (.gltf com .bin e texturas)";
    return true;
  }
  // Importar universal (menu da barra superior): os mesmos pedidos que os
  // painéis levantam, de qualquer espaço de trabalho.
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::LibraryImport)) {state_.libraryImportRequested=true;return true;}
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::ImportTexture)) {state_.textureImportRequested=true;state_.codeFiles=false;return true;}
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::ImportEnvironment)) {state_.environmentImportRequested=true;state_.codeFiles=false;return true;}
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::ImportModel)) {
    // O editor não conhece Android nem o seletor de arquivos: levanta o pedido
    // e quem tem o sistema na mão abre o diálogo e devolve os bytes.
    state_.modelImportRequested=true;state_.creationMenu=false;
    state_.status="Escolha GLB ou FBX estático; revise a receita e as perdas antes de importar.";
    return true;
  }
  if(routing.tapped && routing.widgetId>=widgetId(EditorWidget::CreationRecipeBase) &&
     routing.widgetId<widgetId(EditorWidget::CreationRecipeBase)+editorCreationCatalog.size()) {
    const auto parent=creationParent();
    if(createRecipe(routing.widgetId-widgetId(EditorWidget::CreationRecipeBase),parent)!=kInvalidEntity)
      state_.creationMenu=false;
    return true;
  }
  if(routing.tapped && (routing.widgetId==widgetId(EditorWidget::CreateFiniteWater) ||
                        routing.widgetId==widgetId(EditorWidget::CreateOceanWater))) {
    const auto parent=creationParent();
    state_.creationMenu=false;
    createWaterSurface(routing.widgetId==widgetId(EditorWidget::CreateOceanWater),parent);
    return true;
  }
  if(routing.tapped && (routing.widgetId==widgetId(EditorWidget::CreateRiverWater) || routing.widgetId==widgetId(EditorWidget::CreateBuoyantBox))) {
    const bool box=routing.widgetId==widgetId(EditorWidget::CreateBuoyantBox);
    const auto parent=creationParent();
    for(u32 i=0;i<mapScene_.assetCount();++i) if(mapScene_.materialFlagsForAsset(i)&(box?renderer::BoxAuthoringResource:renderer::WaterRouteResource)) {
      const float position[]{camera_.target[0],box?3.0f:0.0f,camera_.target[2]};
      if(instantiateAsset(i,parent,position)) {
        state_.workspace=EditorWorkspace::Scene;state_.waterTab=1;state_.routePoint=0;state_.creationMenu=false;frameSelection();
      }
      break;
    }
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::CreateGroup)) state_.creationMenu=false;
  if(routing.tapped && routing.widgetId>=widgetId(EditorWidget::RoutePointAdd) && routing.widgetId<=widgetId(EditorWidget::RoutePointNext)) {
    const auto *selected=document_.find(state_.selection);
    if(selected && waterRoute(*selected).count) {
      auto value=*selected;auto *editableRoute=editWaterRoute(value);if(!editableRoute) return false;auto &route=*editableRoute;state_.routePoint=std::min(state_.routePoint,route.count-1);
      const auto widget=static_cast<EditorWidget>(routing.widgetId);
      if(widget==EditorWidget::RoutePointPrevious) state_.routePoint=state_.routePoint?state_.routePoint-1:0;
      if(widget==EditorWidget::RoutePointNext) state_.routePoint=std::min(state_.routePoint+1,route.count-1);
      if(widget==EditorWidget::RoutePointAdd && route.count<renderer::MaximumWaterRoutePoints) {
        const u32 point=state_.routePoint;
        for(u32 i=route.count;i>point+1;--i) route.points[i]=route.points[i-1];
        route.points[point+1]=route.points[point];
        if(point+1<route.count) {
          for(u32 axis=0;axis<3;++axis) route.points[point+1].position[axis]=(route.points[point].position[axis]+route.points[point+2].position[axis])*.5f;
        } else {
          for(u32 axis=0;axis<3;++axis) route.points[point+1].position[axis]+=route.points[point].position[axis]-route.points[point-1].position[axis];
        }
        ++route.count;if(history_.applyValues(document_,selected->id,value)) ++state_.routePoint;
      }
      if(widget==EditorWidget::RoutePointRemove && route.count>2) {
        for(u32 i=state_.routePoint;i+1<route.count;++i) route.points[i]=route.points[i+1];
        --route.count;if(history_.applyValues(document_,selected->id,value)) state_.routePoint=std::min(state_.routePoint,route.count-1);
      }
      state_.propertyPage=0;
    }
    return true;
  }
  if(routing.widgetId>=widgetId(EditorWidget::RoutePointBase) && routing.widgetId<widgetId(EditorWidget::RoutePointBase)+renderer::MaximumWaterRoutePoints) {
    const auto *selected=document_.find(state_.selection);const u32 point=routing.widgetId-widgetId(EditorWidget::RoutePointBase);
    if(selected && point<waterRoute(*selected).count) {
      state_.routePoint=point;
      if(event.phase==ui::UiPointerPhase::Down && !history_.isOpen()) {history_.begin("Mover ponto do rio");fieldWidget_=routing.widgetId;fieldPointer_=event.pointerId;fieldInitial_=*selected;dragEntity_=selected->id;}
      if(routing.dragging && event.phase==UiPointerPhase::Move && fieldWidget_==routing.widgetId && fieldPointer_==event.pointerId) {
        float world[16],normal[12];
        const auto ray=screenPointToRay(view_,event.position);
        if(editorWorldMatrix(document_,selected->id,world) && renderer::buildNormalMatrix(world,normal) && ray.valid && std::abs(ray.direction[1])>1e-5f) {
          const auto &p=waterRoute(fieldInitial_).points[point];const float height=world[13]+world[1]*p.position[0]+world[5]*p.position[1]+world[9]*p.position[2];
          const float distance=(height-ray.origin[1])/ray.direction[1];
          if(distance>0) {
            float offset[3];for(u32 axis=0;axis<3;++axis) offset[axis]=ray.origin[axis]+distance*ray.direction[axis]-world[12+axis];
            auto value=*selected;
            auto *route=editWaterRoute(value);if(!route) return false;
            for(u32 axis=0;axis<3;++axis) route->points[point].position[axis]=normal[axis*4]*offset[0]+normal[axis*4+1]*offset[1]+normal[axis*4+2]*offset[2];
            history_.applyValues(document_,selected->id,value,routing.widgetId);
          }
        }
      }
      if(routing.released && fieldWidget_==routing.widgetId && fieldPointer_==event.pointerId) {history_.end();fieldWidget_=0;}
    }
    return true;
  }
  const u32 assetBase=widgetId(EditorWidget::AssetRowBase);
  if(routing.target==UiPointerTarget::Widget && routing.widgetId>=assetBase &&
      routing.widgetId<assetBase+mapScene_.assetCount()) {
    const u32 index=routing.widgetId-assetBase;
    if(routing.dragging && event.phase==UiPointerPhase::Move && !history_.isOpen() &&
       (!state_.draggingAsset || assetPointer_==event.pointerId)) {
      assetPointer_=event.pointerId;state_.draggingAsset=true;
      state_.assetDragPosition=event.position;state_.workspace=EditorWorkspace::Scene;
      state_.status="Solte na cena ou sobre um pai na hierarquia";
    }
    if(routing.tapped && !state_.draggingAsset) {
      if(instantiateAsset(index,document_.root(),camera_.target)) {
        state_.workspace=EditorWorkspace::Scene;frameSelection();
      }
    } else if(routing.released && state_.draggingAsset && assetPointer_==event.pointerId) {
      state_.draggingAsset=false;
      state_.status="Arraste cancelado";
      if(event.phase==UiPointerPhase::Up && !history_.isOpen()) {
        const auto target=router_.hitTest(event.position);
        const u32 rows=widgetId(EditorWidget::HierarchyRowBase);
        if(target.target==UiPointerTarget::Widget && dropAssetOnField(target.widgetId,index)) {
          state_.status="Recurso atribuído pelo arraste";return true;
        }
        if(target.target==UiPointerTarget::Widget && target.widgetId>=rows &&
           target.widgetId<rows+EditorDocument::kMaximumEntities) {
          const auto parent=target.widgetId-rows;
          float world[16];
          if(editorWorldMatrix(document_,parent,world)) instantiateAsset(index,parent,world+12);
        } else if(target.target==UiPointerTarget::Viewport && layout_.viewport.contains(event.position)) {
          const auto ray=screenPointToRay(view_,event.position);
          if(ray.valid && std::abs(ray.direction[1])>1e-5f) {
            const float distance=(camera_.target[1]-ray.origin[1])/ray.direction[1];
            if(distance>view_.frustum.nearPlane && distance<view_.frustum.farPlane) {
              float point[3];for(u32 a=0;a<3;++a) point[a]=ray.origin[a]+distance*ray.direction[a];
              instantiateAsset(index,document_.root(),point);
            }
          }
        }
      }
    }
    return true;
  }  const u32 fieldBase = widgetId(EditorWidget::TransformFieldBase);
  if (routing.target == UiPointerTarget::Widget && routing.widgetId >= fieldBase &&
      routing.widgetId < fieldBase + editorNumericProperties.size()) {
    if (event.phase == UiPointerPhase::Down && fieldWidget_ == 0 && !history_.isOpen()) {
      const u32 index=routing.widgetId-fieldBase;
      const auto *entity = document_.find((editorNumericProperties[index].group==EditorPropertyGroup::Environment || editorNumericProperties[index].group==EditorPropertyGroup::Water)?document_.root():state_.selection);
      if (entity && history_.begin(editorNumericProperties[index].name)) {
        fieldInitial_ = *entity;
        dragEntity_ = entity->id;
        fieldPointer_ = event.pointerId;
        fieldWidget_ = routing.widgetId;
      }
    }
    if (fieldWidget_ == routing.widgetId && fieldPointer_ == event.pointerId) {
      const u32 field = fieldWidget_ - fieldBase;
      if (routing.dragging) {
        auto value = fieldInitial_;
        const auto &property=editorNumericProperties[field];
        const float number=std::clamp(editorPropertyValue(fieldInitial_,field)+routing.totalDelta.x*property.dragStep,
                                      property.minimum,property.maximum);
        if(setEditorPropertyValue(value,field,number)) history_.applyValues(document_,dragEntity_,value,fieldWidget_);
      }
      if (routing.released) {
        history_.end();
        if(routing.tapped) {
          state_.numericField=fieldWidget_;state_.numericEntity=dragEntity_;state_.numericInstance=0;state_.numericProperty.clear();
          const float value=editorPropertyValue(fieldInitial_,field);
          std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g",static_cast<double>(value));
          state_.numericReplace=true;state_.numericError=false;
        }
        fieldWidget_=0;
      }
    }
    return true;
  }
  // A second finger must not switch tools or history during an active edit.
  if (history_.isOpen() && (fieldWidget_ != 0 || gizmoTransactionOpen_) &&
      routing.target == UiPointerTarget::Widget &&
      routing.widgetId < widgetId(EditorWidget::GizmoAxisBase)) return true;
  if (routing.tapped && routing.widgetId == widgetId(EditorWidget::ViewsOpen)) {
    state_.viewsPanel=!state_.viewsPanel;state_.sceneLayersPanel=false;return true;
  }
  if (routing.tapped && routing.widgetId == widgetId(EditorWidget::SceneLayersOpen)) {
    state_.sceneLayersPanel=!state_.sceneLayersPanel;state_.viewsPanel=false;state_.sceneLayersPage=0;return true;
  }
  if (state_.sceneLayersPanel && routing.tapped) {
    const u32 key=routing.widgetId;
    const u32 hidden=state_.hiddenLayers,locked=state_.unpickableLayers;
    if(key==widgetId(EditorWidget::SceneLayersClose)) {state_.sceneLayersPanel=false;return true;}
    if(key==widgetId(EditorWidget::SceneLayersPrevious)) {if(state_.sceneLayersPage) --state_.sceneLayersPage;return true;}
    if(key==widgetId(EditorWidget::SceneLayersNext)) {++state_.sceneLayersPage;return true;}
    if(key==widgetId(EditorWidget::SceneLayersShowAll)) state_.hiddenLayers=0;
    else if(key==widgetId(EditorWidget::SceneLayersHideAll)) state_.hiddenLayers=0xffffffffu;
    else if(key==widgetId(EditorWidget::SceneLayersPickAll)) state_.unpickableLayers=0;
    else if(key>=widgetId(EditorWidget::SceneLayerVisibleBase) && key<widgetId(EditorWidget::SceneLayerVisibleBase)+32)
      state_.hiddenLayers^=1u<<(key-widgetId(EditorWidget::SceneLayerVisibleBase));
    else if(key>=widgetId(EditorWidget::SceneLayerPickBase) && key<widgetId(EditorWidget::SceneLayerPickBase)+32)
      state_.unpickableLayers^=1u<<(key-widgetId(EditorWidget::SceneLayerPickBase));
    if(hidden!=state_.hiddenLayers || locked!=state_.unpickableLayers) {
      // O desenho muda sem a cena mudar de revisão: o shell republica.
      if(hidden!=state_.hiddenLayers) appearanceChanged_=true;
      // O que ficou escondido ou sem seleção sai da seleção da vista.
      if(const auto *selected=document_.find(state_.selection);
         selected && selected->layer<32 && ((state_.hiddenLayers|state_.unpickableLayers)&(1u<<selected->layer)) &&
         !((hidden|locked)&(1u<<selected->layer))) state_.status="O selecionado está numa camada escondida ou sem seleção";
      saveEditorPreferences();
      return true;
    }
  }
  if (state_.viewsPanel && routing.tapped) {
    const auto key=routing.widgetId;
    if(key==widgetId(EditorWidget::ViewsClose)) {state_.viewsPanel=false;return true;}
    if(key>=widgetId(EditorWidget::SceneViewRowBase) &&
       key<widgetId(EditorWidget::SceneViewRowBase)+runtime::SceneViews::kMaximum) {
      if(!applySceneView(key-widgetId(EditorWidget::SceneViewRowBase))) state_.status="Vista indisponível";
      return true;
    }
    if(key==widgetId(EditorWidget::ViewSave) || key==widgetId(EditorWidget::ViewRename)) {
      if(isPlaying() || history_.isOpen()) {state_.status="Finalize a edição antes de mexer nas vistas.";return true;}
      state_.viewRenaming=key==widgetId(EditorWidget::ViewRename);
      const auto *chosen=document_.views().at(state_.viewSelected);
      state_.viewName=state_.viewRenaming&&chosen?chosen->name:
          "Vista "+std::to_string(document_.views().count()+1);
      std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",state_.viewName.c_str());
      state_.viewNaming=true;return true;
    }
    if(key==widgetId(EditorWidget::ViewUpdate)) {
      const auto *chosen=document_.views().at(state_.viewSelected);
      // Atualizar guarda a vista ATUAL com o nome que já existe: é o gesto de
      // "esta é a comparação daqui em diante".
      state_.status=chosen&&saveSceneView(chosen->name)?"Vista atualizada com o enquadramento atual":
          "Não foi possível atualizar a vista";
      return true;
    }
    if(key==widgetId(EditorWidget::ViewDelete)) {
      state_.status=deleteSceneView(state_.viewSelected)?"Vista excluída":"Não foi possível excluir a vista";
      return true;
    }
  }
  if (routing.tapped && routing.widgetId == widgetId(EditorWidget::FrameSelection)) {
    state_.inspectorMenu=false;
    frameSelection(); return true;
  }
  if (routing.tapped && routing.widgetId == widgetId(EditorWidget::FrameAll)) {
    frameAll(); return true;
  }
  if (routing.target == UiPointerTarget::Viewport) {
    if(gizmoTransactionOpen_ || fieldWidget_) return true;
    return handleViewportPointer(event, routing);
  }
  if (routing.target == UiPointerTarget::None) return true;

  const u32 gizmoBase = widgetId(EditorWidget::GizmoAxisBase);
  if (routing.widgetId >= gizmoBase && routing.widgetId < gizmoBase + 6) {
    if(!viewportPointers_.empty() || fieldWidget_) return true;
    if(event.phase==UiPointerPhase::Down && !gizmoDrag_.active) gizmoPointer_=event.pointerId;
    if(event.pointerId!=gizmoPointer_) return true;
    handleGizmoPointer(routing, routing.widgetId - gizmoBase);
    return true;
  }

  // Inspectors focados. No escopo do Inspector, "a seleção" é o alvo dele.
  if(routing.tapped) {
    const u32 key=routing.widgetId;
    if(key==widgetId(EditorWidget::MotorSetupOpen)||key==widgetId(EditorWidget::MotorSetupPreserve)||key==widgetId(EditorWidget::MotorSetupFit)||key==widgetId(EditorWidget::MotorSetupConvex)||key==widgetId(EditorWidget::MotorSetupDecompose)) {
      if(key==widgetId(EditorWidget::MotorSetupOpen)){
        state_.motorSetupTarget=state_.selection;state_.motorSetupPolicy=0;
        if(const auto *e=document_.find(state_.selection))if(const auto *recipe=scene::collisionRecipe(e->components);recipe&&!recipe->parts.empty()) {
          state_.motorSetupPolicy=3;motorBakeSettings_=recipe->settings;state_.motorBakeDraft=recipe->settings;
          state_.motorBakeBudget=3;
          for(u32 budget=0;budget<3;++budget)if(recipe->settings.maximumParts==(budget==0?8u:budget==1?16u:32u)&&recipe->settings.voxelResolution==(budget==0?50000u:budget==1?100000u:400000u)&&recipe->settings.maximumVertices==(budget==2?64u:32u)&&recipe->settings.volumeErrorPercent==1&&recipe->settings.timeBudgetSeconds==(budget==2?120.f:60.f))state_.motorBakeBudget=budget;
        }
      }
      else if(state_.motorSetupTarget!=state_.selection){state_.status="Seleção mudou; abra a configuração novamente";return true;}
      else state_.motorSetupPolicy=key==widgetId(EditorWidget::MotorSetupPreserve)?0:key==widgetId(EditorWidget::MotorSetupFit)?1:key==widgetId(EditorWidget::MotorSetupConvex)?2:3;
      cancelMotorDecomposition();
      EditorEntity candidate;
      state_.motorBakeSourcesOpen=false;state_.motorBakeSettingsOpen=false;
      if(state_.motorSetupPolicy==3){refreshMotorBakeSources(state_.selection);state_.motorSetupError.clear();state_.motorSetupSummary="Escolha as fontes e gere a prévia. Requer sólidos fechados; hierarquia e visual preservados.";}
      else previewDynamicMotor(state_.selection,static_cast<MotorCollisionPolicy>(state_.motorSetupPolicy),candidate,state_.motorSetupSummary,state_.motorSetupError);
      state_.characterConversionTarget=0;state_.entityMenu=false;state_.inspectorMenu=true;state_.propertyPage=0;
      state_.compactPanel=EditorScreenState::CompactPanel::Inspector;return true;
    }
    if(key==widgetId(EditorWidget::MotorSetupBack)){if(state_.motorBakeSourcesOpen){state_.motorBakeSourcesOpen=false;return true;}if(state_.motorBakeSettingsOpen){state_.motorBakeSettingsOpen=false;return true;}cancelMotorDecomposition();state_.motorSetupTarget=0;state_.propertyPage=0;return true;}
    if(key==widgetId(EditorWidget::MotorBakeSettingsOpen)){state_.motorBakeSettingsOpen=true;state_.motorBakeSettingsPage=0;return true;}
    if(key==widgetId(EditorWidget::MotorBakeSettingsDone)){state_.motorBakeSettingsOpen=false;return true;}
    if(key==widgetId(EditorWidget::MotorBakeSettingsPrevious)){if(state_.motorBakeSettingsPage)--state_.motorBakeSettingsPage;return true;}
    if(key==widgetId(EditorWidget::MotorBakeSettingsNext)){if(state_.motorBakeSettingsPage<2)++state_.motorBakeSettingsPage;return true;}
    if(key==widgetId(EditorWidget::MotorBakePoseRest)||key==widgetId(EditorWidget::MotorBakePoseAuthored)||key==widgetId(EditorWidget::MotorBakePoseAnimation)) {
      cancelMotorDecomposition();state_.motorBakeDraft.pose=key==widgetId(EditorWidget::MotorBakePoseRest)?resources::ConvexBakePose::Rest:key==widgetId(EditorWidget::MotorBakePoseAuthored)?resources::ConvexBakePose::Authored:resources::ConvexBakePose::Animation;
      state_.motorBakeBudget=3;state_.motorSetupError.clear();return true;
    }
    if(key>=widgetId(EditorWidget::MotorBakeSettingsNumberBase)&&key<widgetId(EditorWidget::MotorBakeSettingsNumberBase)+6) {
      if(!state_.motorBakeSettingsOpen||isPlaying()||history_.isOpen())return true;
      const auto &s=state_.motorBakeDraft;const double numbers[]{double(s.maximumParts),double(s.voxelResolution),double(s.maximumVertices),s.volumeErrorPercent,double(s.timeBudgetSeconds),s.animationTime};
      state_.numericField=key;state_.numericEntity=state_.motorSetupTarget;state_.numericInstance=0;state_.numericProperty.clear();
      state_.numericCurrent=numbers[key-widgetId(EditorWidget::MotorBakeSettingsNumberBase)];
      std::snprintf(state_.numericText,sizeof(state_.numericText),"%.7g",state_.numericCurrent);
      state_.numericReplace=true;state_.numericError=false;return true;
    }
    if(key==widgetId(EditorWidget::MotorBakeSourcesOpen)){refreshMotorBakeSources(state_.selection);state_.motorBakeSourcesOpen=true;return true;}
    if(key==widgetId(EditorWidget::MotorBakeSourcesDone)){state_.motorBakeSourcesOpen=false;return true;}
    if(key==widgetId(EditorWidget::MotorBakeSourcesObject)||key==widgetId(EditorWidget::MotorBakeSourcesHierarchy)) {
      const auto options=motorDecompositionSources(state_.selection);std::vector<MotorBakeSource> sources;
      for(const auto &option:options)if(key==widgetId(EditorWidget::MotorBakeSourcesObject)?option.source.object==state_.selection:option.error.empty())sources.push_back(option.source);
      if(key==widgetId(EditorWidget::MotorBakeSourcesHierarchy)&&!options.empty()&&!options.back().source.object){state_.motorSetupError=options.back().error;return true;}
      selectMotorDecompositionSources(state_.selection,sources,state_.motorSetupError);return true;
    }
    if(key==widgetId(EditorWidget::MotorBakeSourcePrevious)){if(state_.motorBakeSourcePage)--state_.motorBakeSourcePage;return true;}
    if(key==widgetId(EditorWidget::MotorBakeSourceNext)){if((state_.motorBakeSourcePage+1)*state_.motorBakePartsPerPage()<state_.motorBakeSourceRows.size())++state_.motorBakeSourcePage;return true;}
    if(key>=widgetId(EditorWidget::MotorBakeSourceBase)&&key<widgetId(EditorWidget::MotorBakeSourceBase)+256) {
      const auto options=motorDecompositionSources(state_.selection);const auto row=key-widgetId(EditorWidget::MotorBakeSourceBase);
      if(row<options.size()&&(options[row].error.empty()||std::find(motorBakeSources_.begin(),motorBakeSources_.end(),options[row].source)!=motorBakeSources_.end())) {
        auto sources=motorBakeSources_;const auto found=std::find(sources.begin(),sources.end(),options[row].source);
        if(found==sources.end())sources.push_back(options[row].source);else sources.erase(found);
        selectMotorDecompositionSources(state_.selection,sources,state_.motorSetupError);
      }
      return true;
    }
    if(key==widgetId(EditorWidget::MotorBakeStart)||key==widgetId(EditorWidget::MotorBakeRegenerate)) {
      beginMotorDecomposition(state_.selection,state_.motorBakeDraft,state_.motorSetupError);return true;
    }
    if(key==widgetId(EditorWidget::MotorBakeConfirmMapping)){confirmMotorDecompositionMapping(state_.motorSetupError);return true;}
    if(key>=widgetId(EditorWidget::MotorBakeMappingBase)&&key<widgetId(EditorWidget::MotorBakeMappingBase)+32) {
      const auto part=key-widgetId(EditorWidget::MotorBakeMappingBase);
      if(part<motorBakePartMapping_.size()) {
        std::vector<u64> choices{0};choices.insert(choices.end(),state_.motorBakePreviousParts.begin(),state_.motorBakePreviousParts.end());
        const auto position=std::find(choices.begin(),choices.end(),motorBakePartMapping_[part]);
        const auto start=position==choices.end()?0:static_cast<usize>(position-choices.begin());
        for(usize offset=1;offset<=choices.size();++offset) {
          const auto candidate=choices[(start+offset)%choices.size()];bool taken=false;
          for(usize i=0;i<motorBakePartMapping_.size();++i)if(i!=part&&candidate&&motorBakePartMapping_[i]==candidate)taken=true;
          if(!taken){setMotorDecompositionPartMapping(part,candidate,state_.motorSetupError);break;}
        }
      }
      return true;
    }
    if(key==widgetId(EditorWidget::MotorBakeCancel)){cancelMotorDecomposition();state_.motorSetupSummary="Geração cancelada; a cena permanece intacta.";return true;}
    if(key>=widgetId(EditorWidget::MotorBakeBudgetLow)&&key<=widgetId(EditorWidget::MotorBakeBudgetHigh)) {
      cancelMotorDecomposition();state_.motorBakeBudget=key-widgetId(EditorWidget::MotorBakeBudgetLow);
      motorBakeSettings_.maximumParts=state_.motorBakeBudget==0?8:state_.motorBakeBudget==1?16:32;
      motorBakeSettings_.voxelResolution=state_.motorBakeBudget==0?50000:state_.motorBakeBudget==1?100000:400000;
      motorBakeSettings_.maximumVertices=state_.motorBakeBudget==2?64:32;motorBakeSettings_.volumeErrorPercent=1;motorBakeSettings_.timeBudgetSeconds=state_.motorBakeBudget==2?120:60;
      motorBakeSettings_.pose=state_.motorBakeDraft.pose;motorBakeSettings_.animationTime=state_.motorBakeDraft.animationTime;state_.motorBakeDraft=motorBakeSettings_;
      state_.motorSetupError.clear();state_.motorSetupSummary="Orçamento alterado; gere uma nova prévia.";return true;
    }
    if(key==widgetId(EditorWidget::MotorBakePrevious)){if(state_.motorBakePage)--state_.motorBakePage;return true;}
    if(key==widgetId(EditorWidget::MotorBakeNext)){if((state_.motorBakePage+1)*state_.motorBakePartsPerPage()<state_.motorBakeEnabled.size())++state_.motorBakePage;return true;}
    if(key>=widgetId(EditorWidget::MotorBakePartBase)&&key<widgetId(EditorWidget::MotorBakePartBase)+32) {
      const auto part=key-widgetId(EditorWidget::MotorBakePartBase);
      if(state_.motorBakeReady&&part<state_.motorBakeEnabled.size()) {
        if(motorBakeRegenerating_&&motorBakePartMapping_[part]) {
          const auto *e=document_.find(motorBakeObject_);
          if(!e||!e->components.findInstance(motorBakePartMapping_[part])){state_.motorSetupError="Remoção local preservada; restaurar o Colisor é uma edição separada";return true;}
        }
        state_.motorBakeEnabled[part]=!state_.motorBakeEnabled[part];
        refreshMotorBakeCandidate();
        if(std::none_of(state_.motorBakeEnabled.begin(),state_.motorBakeEnabled.end(),[](bool v){return v;}))state_.motorSetupError="Ative pelo menos uma parte";
      }
      return true;
    }
    if(key==widgetId(EditorWidget::MotorSetupApply)) {
      if(state_.motorSetupTarget!=state_.selection){state_.status="Seleção mudou; abra a configuração novamente";return true;}
      configureDynamicMotor(state_.selection,static_cast<MotorCollisionPolicy>(state_.motorSetupPolicy),state_.motorSetupError);return true;
    }
    if(key==widgetId(EditorWidget::CharacterConversionOpen)) {
      state_.motorSetupTarget=0;
      EditorEntity root,child;std::string error;
      state_.characterConversionTarget=state_.selection;
      const bool ready=prepareCharacterConversion(state_.selection,root,child,error);
      state_.characterConversionError=std::move(error);
      if(ready) {const auto *c=runtime::characterComponent(root);state_.characterConversionRadius=c->radius;state_.characterConversionHeight=2*(c->halfHeight+c->radius);}
      state_.entityMenu=false;state_.inspectorMenu=true;state_.propertyPage=0;
      state_.compactPanel=EditorScreenState::CompactPanel::Inspector;
      return true;
    }
    if(key==widgetId(EditorWidget::CharacterConversionBack)) {state_.characterConversionTarget=0;state_.propertyPage=0;return true;}
    if(key>=widgetId(EditorWidget::CharacterRecipeBase)&&key<widgetId(EditorWidget::CharacterRecipeEnd)) {
      // Criar › Personagem: a receita age sobre o personagem da seleção e
      // fecha o menu; o resumo ou o erro vai para a barra de estado.
      state_.creationMenu=false;
      const auto owner=EditorStudio::characterOf(document_,state_.selection);
      const auto recipe=static_cast<EditorStudio::CharacterRecipe>(1+key-widgetId(EditorWidget::CharacterRecipeBase));
      std::string summary,error;
      if(!owner)state_.status="Selecione um personagem com esqueleto";
      else if(EditorStudio::setupCharacter(*this,owner,recipe,summary,error)) {state_.status=summary;state_.selection=owner;}
      else state_.status=error;
      return true;
    }
    if(key==widgetId(EditorWidget::CharacterConversionApply)) {
      if(state_.characterConversionTarget!=state_.selection) {state_.status="Seleção mudou; abra a conversão novamente";return true;}
      const auto result=convertToCharacter(state_.selection);
      if(!result) state_.characterConversionError=state_.status;
      return true;
    }
    if(key==widgetId(EditorWidget::InspectorOpenFocused)) {state_.inspectorMenu=false;openFocusedInspector(state_.selection);return true;}
    if(key==widgetId(EditorWidget::HierarchyProperties)) {state_.entityMenu=false;openFocusedInspector(state_.selection);return true;}
    if(key>=widgetId(EditorWidget::ComponentPropertiesBase) && key<widgetId(EditorWidget::ComponentPropertiesBase)+0x100u) {
      const auto *entity=document_.find(state_.selection);
      const u32 index=key-widgetId(EditorWidget::ComponentPropertiesBase);
      if(entity && index<entity->components.size()) {
        state_.nativeMenu=0;state_.scriptMenu=0;
        openFocusedInspector(entity->id,entity->components.at(index)->instanceId());
      }
      return true;
    }
    const u32 count=static_cast<u32>(state_.focusedInspectors.size());
    if(key>=widgetId(EditorWidget::FocusedTabBase) && key<widgetId(EditorWidget::FocusedTabBase)+count) {
      state_.focusedActive=key-widgetId(EditorWidget::FocusedTabBase)+1;state_.focusedMenu=false;return true;
    }
    if(key>=widgetId(EditorWidget::FocusedCloseBase) && key<widgetId(EditorWidget::FocusedCloseBase)+count) {
      const u32 index=key-widgetId(EditorWidget::FocusedCloseBase);
      state_.focusedInspectors.erase(state_.focusedInspectors.begin()+index);
      if(index<focusedNames_.size()) focusedNames_.erase(focusedNames_.begin()+index);
      state_.focusedActive=state_.focusedInspectors.empty()?0:std::min<u32>(std::max(state_.focusedActive,1u),static_cast<u32>(state_.focusedInspectors.size()));
      state_.focusedMenu=false;saveEditorPreferences();return true;
    }
    if(key==widgetId(EditorWidget::FocusedMenu)) {state_.focusedMenu=!state_.focusedMenu;return true;}
    if(key==widgetId(EditorWidget::FocusedPing) && count) {
      state_.focusedMenu=false;
      const auto &focused=state_.focusedInspectors[std::min(std::max(state_.focusedActive,1u),count)-1];
      // Recurso: Ping no painel Arquivos (abre as pastas e marca o arquivo).
      if(focused.kind!=EditorScreenState::FocusedAsset::None) {
        if(const auto *record=assets_.find(focused.asset)) {
          state_.filesCollapsed=false;revealProjectPath(record->path);state_.selectedFile=record->path;
        }
      } else pingEntity(focused.entity);
      return true;
    }
    if(key==widgetId(EditorWidget::FocusedCloseAll)) {
      state_.focusedInspectors.clear();focusedNames_.clear();state_.focusedActive=0;state_.focusedMenu=false;saveEditorPreferences();return true;
    }
    if(key==widgetId(EditorWidget::FocusedCollapse)) {state_.focusedCollapsed=true;state_.focusedMenu=false;return true;}
    if(key==widgetId(EditorWidget::FocusedChip)) {state_.focusedCollapsed=false;return true;}
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::InspectorLock)) {
    // Dentro do escopo a "seleção" é o alvo do Inspector: travar prende nele.
    state_.inspectorLocked=state_.inspectorLocked?0:state_.selection;
    state_.status=state_.inspectorLocked?"Inspector travado neste objeto":"Inspector segue a seleção";
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::InspectorDebugToggle)) {
    state_.inspectorDebug=!state_.inspectorDebug;state_.inspectorMenu=false;state_.propertyPage=0;return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::InspectorPing)) {
    state_.inspectorMenu=false;pingEntity(state_.selection);return true;
  }
  if(handleComponentReorder(event,routing)) return true;
  if(handleScriptArrayDrag(event,routing)) return true;
  if(handleLodBar(event,routing)) return true;

  const u32 hierarchyRows=widgetId(EditorWidget::HierarchyRowBase);
  if(routing.widgetId>=hierarchyRows && routing.widgetId<hierarchyRows+EditorDocument::kMaximumEntities) {
    // Horizontal movement starts a hierarchy drag; vertical movement retains
    // the established touch-scroll behaviour.
    if(routing.dragging && event.phase==UiPointerPhase::Move && !history_.isOpen() &&
       (state_.draggingEntity || std::abs(routing.totalDelta.x)>std::max(24.0f,std::abs(routing.totalDelta.y)*1.5f))) {
      state_.draggingEntity=routing.widgetId-hierarchyRows;
      hierarchyPointer_=event.pointerId;state_.assetDragPosition=event.position;
      state_.status="Solte sobre o novo pai; arraste vertical para rolar";
    }
    if(state_.draggingEntity) {
      if(routing.released) {
        const auto entity=state_.draggingEntity;state_.draggingEntity=kInvalidEntity;
        bool accepted=false;
        if(event.phase==UiPointerPhase::Up) {
          const auto target=router_.hitTest(event.position);
          // Sobre um campo de referência o gesto é atribuir, nunca reparentear:
          // a recusa fica com o motivo do campo.
          const u32 operation=target.widgetId&0xff000000u;
          if(target.target==UiPointerTarget::Widget && (operation==widgetId(EditorWidget::ComponentReferenceBase) ||
                                                        operation==widgetId(EditorWidget::ScriptFieldBase))) {
            if(dropObjectOnField(target.widgetId,entity)) state_.status="Referência atribuída pelo arraste";
            return true;
          }
          if(target.target==UiPointerTarget::Widget && target.widgetId>=hierarchyRows &&
             target.widgetId<hierarchyRows+EditorDocument::kMaximumEntities)
            accepted=history_.reparentKeepingWorld(document_,entity,target.widgetId-hierarchyRows);
          else if(target.target==UiPointerTarget::None && layout_.hierarchyPanel.contains(event.position))
            accepted=history_.reparentKeepingWorld(document_,entity,document_.root());
        }
        state_.status=accepted?"Pai alterado; transformacao mundial preservada":"Mudanca de pai cancelada ou incompativel";
        if(accepted) setSelection(entity);
      }
      return true;
    }
  }
  // Barra de status: console e trabalhos.
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::StatusConsole)) {
    if(state_.workspace==EditorWorkspace::Code) {
      state_.codeFiles=false;state_.codeMenu=false;state_.consoleCollapsed=false;state_.consoleExpanded=true;
    } else if(!isPlaying()) {state_.workspace=EditorWorkspace::Scene;state_.diagnosticDockOpen=true;}
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::StatusTasks)) {state_.backgroundPanel=!state_.backgroundPanel;return true;}
  if(state_.backgroundPanel && routing.tapped) {
    const u32 key=routing.widgetId;
    if(key==widgetId(EditorWidget::StatusTasksClose)) {state_.backgroundPanel=false;return true;}
    if(key>=widgetId(EditorWidget::StatusTaskCancelBase) && key<widgetId(EditorWidget::StatusTaskCancelBase)+state_.backgroundTasks.size()) {
      const auto &task=state_.backgroundTasks[key-widgetId(EditorWidget::StatusTaskCancelBase)];
      if(task.cancelable) {
        // Importações usam o mesmo cancelamento do botão da preparação.
        if(task.kind==EditorBackgroundTask::Kind::ModelImport || task.kind==EditorBackgroundTask::Kind::EnvironmentImport ||
           task.kind==EditorBackgroundTask::Kind::TextureImport) state_.importCancel=true;
        else backgroundCancel_=task.kind;
        state_.status="Cancelando: "+task.label;
      }
      return true;
    }
  }
  // Layouts dos painéis.
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::LayoutsOpen)) {
    state_.layoutsPanel=true;return true;
  }
  if(state_.layoutsPanel && routing.target==UiPointerTarget::Widget) {
    if(!routing.tapped) return true;
    const u32 key=routing.widgetId;
    const auto builtins=editorBuiltinLayouts(state_.surface.width);
    if(key==widgetId(EditorWidget::LayoutsClose)) state_.layoutsPanel=false;
    else if(key==widgetId(EditorWidget::LayoutReset)) {applyLayout(builtins[0]);state_.status="Layout padrão restaurado";}
    else if(key==widgetId(EditorWidget::LayoutSave)) {
      state_.namingLayout=true;std::snprintf(state_.renameText,sizeof(state_.renameText),"Layout %u",static_cast<unsigned>(state_.userLayouts.size()+1));
    } else if(key>=widgetId(EditorWidget::LayoutBuiltinBase) && key<widgetId(EditorWidget::LayoutBuiltinBase)+builtins.size()) {
      const auto &layout=builtins[key-widgetId(EditorWidget::LayoutBuiltinBase)];
      applyLayout(layout);state_.status="Layout: "+layout.name;
    } else if(key>=widgetId(EditorWidget::LayoutDeleteBase) && key<widgetId(EditorWidget::LayoutDeleteBase)+state_.userLayouts.size()) {
      const u32 index=key-widgetId(EditorWidget::LayoutDeleteBase);
      state_.status="Layout apagado: "+state_.userLayouts[index].name;
      state_.userLayouts.erase(state_.userLayouts.begin()+index);saveEditorPreferences();
    } else if(key>=widgetId(EditorWidget::LayoutUserBase) && key<widgetId(EditorWidget::LayoutUserBase)+state_.userLayouts.size()) {
      const auto layout=state_.userLayouts[key-widgetId(EditorWidget::LayoutUserBase)];
      applyLayout(layout);state_.status="Layout: "+layout.name;
    }
    return true;
  }
  // Multisseleção: modo, ações de conjunto e linhas da Hierarquia no modo.
  if(routing.tapped) {
    const u32 key=routing.widgetId;
    if(key==widgetId(EditorWidget::HierarchyMultiToggle)) {
      state_.multiSelect=!state_.multiSelect;
      if(!state_.multiSelect && state_.selectionSet.size()>1) state_.selectionSet={state_.selection};
      state_.status=state_.multiSelect?"Selecionar vários: toque nos objetos para somar ou tirar":"Seleção única";
      return true;
    }
    std::vector<EditorEntityId> all;document_.collectSubtree(document_.root(),all);
    all.erase(std::remove(all.begin(),all.end(),document_.root()),all.end());
    if(key==widgetId(EditorWidget::SelectAll)) {selectEntities(all);return true;}
    if(key==widgetId(EditorWidget::SelectNone)) {selectEntities({});return true;}
    if(key==widgetId(EditorWidget::SelectInvert)) {
      std::vector<EditorEntityId> inverted;
      for(const auto id:all) if(!state_.isSelected(id)) inverted.push_back(id);
      selectEntities(inverted);return true;
    }
    if(key==widgetId(EditorWidget::SelectChildren)) {
      // Unity: Select Children — os selecionados e todos os descendentes.
      std::vector<EditorEntityId> result;
      for(const auto id:selectedRoots()) {std::vector<EditorEntityId> subtree;document_.collectSubtree(id,subtree);
        for(const auto member:subtree) if(std::find(result.begin(),result.end(),member)==result.end()) result.push_back(member);}
      selectEntities(result);return true;
    }
    if(state_.multiSelect && !state_.reparentEntity && key>=widgetId(EditorWidget::HierarchyRowBase) &&
       key<widgetId(EditorWidget::HierarchyRowBase)+EditorDocument::kMaximumEntities && routing.heldSeconds<ui::kUiLongPressSeconds) {
      toggleSelection(key-widgetId(EditorWidget::HierarchyRowBase));return true;
    }
  }
  // Busca global: modal sobre tudo; toques fora dela não chegam ao editor.
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::GlobalSearchOpen)) {openGlobalSearch();return true;}
  if(state_.globalSearch && routing.target==UiPointerTarget::Widget) {
    if(!routing.tapped) return true;
    const u32 key=routing.widgetId;
    if(key==widgetId(EditorWidget::GlobalSearchClose)) {state_.globalSearch=false;return true;}
    if(key==widgetId(EditorWidget::GlobalSearchField)) {
      state_.editingGlobalSearch=true;std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",state_.globalQuery.c_str());return true;
    }
    if(key==widgetId(EditorWidget::GlobalSearchPrevious)) {if(state_.globalPage) --state_.globalPage;return true;}
    if(key==widgetId(EditorWidget::GlobalSearchNext)) {++state_.globalPage;return true;}
    if(key>=widgetId(EditorWidget::GlobalSearchProviderBase) && key<widgetId(EditorWidget::GlobalSearchProviderBase)+4) {
      state_.globalProvider=static_cast<EditorSearchProvider>(key-widgetId(EditorWidget::GlobalSearchProviderBase));
      state_.globalPage=0;return true;
    }
    if(key>=widgetId(EditorWidget::GlobalSearchResultBase) && key<widgetId(EditorWidget::GlobalSearchResultBase)+state_.globalResults.size()) {
      openSearchResult(key-widgetId(EditorWidget::GlobalSearchResultBase));return true;
    }
    return true;
  }
  // Unity 6000.0 Manual/UndoWindow: o histórico abre pelo ícone de Desfazer;
  // aqui, pelo toque longo em Desfazer ou Refazer (o toque curto continua
  // desfazendo um passo).
  if(routing.tapped && routing.heldSeconds>=ui::kUiLongPressSeconds &&
     (routing.widgetId==widgetId(EditorWidget::Undo) || routing.widgetId==widgetId(EditorWidget::Redo))) {
    state_.undoHistory=true;state_.undoHistoryPage=0;
    return true;
  }
  if(routing.tapped && state_.undoHistory) {
    const u32 key=routing.widgetId;
    if(key==widgetId(EditorWidget::UndoHistoryClose)) {state_.undoHistory=false;return true;}
    if(key==widgetId(EditorWidget::UndoHistoryOrder)) {
      state_.undoNewestFirst=!state_.undoNewestFirst;state_.undoHistoryPage=0;saveEditorPreferences();return true;
    }
    if(key==widgetId(EditorWidget::UndoHistoryPrevious)) {if(state_.undoHistoryPage) --state_.undoHistoryPage;return true;}
    if(key==widgetId(EditorWidget::UndoHistoryNext)) {++state_.undoHistoryPage;return true;}
    if(key>=widgetId(EditorWidget::UndoHistoryRowBase) && key<=widgetId(EditorWidget::UndoHistoryRowBase)+history_.entryCount()) {
      const u32 point=key-widgetId(EditorWidget::UndoHistoryRowBase);
      const bool reached=moveHistoryTo(point);
      state_.status=!reached?"Histórico: um passo recusou; a cena parou no último ponto possível":
                    point?"Histórico: "+history_.describe(point-1):std::string("Histórico: cena como abriu");
      return true;
    }
  }
  if(routing.tapped && (routing.widgetId==widgetId(EditorWidget::Undo) ||
      routing.widgetId==widgetId(EditorWidget::Redo) ||
      routing.widgetId==widgetId(EditorWidget::DuplicateSelection) ||
      routing.widgetId==widgetId(EditorWidget::DeleteSelection))) {
    EditorActionRequest command;
    command.version=sceneVersion();command.entity=state_.selection;
    command.action=routing.widgetId==widgetId(EditorWidget::Undo)?EditorAction::Undo:
      routing.widgetId==widgetId(EditorWidget::Redo)?EditorAction::Redo:
      routing.widgetId==widgetId(EditorWidget::DuplicateSelection)?EditorAction::Duplicate:EditorAction::Remove;
    const auto result=dispatch(command);
    if(result.status==EditorActionStatus::Applied &&
       (command.action==EditorAction::Undo||command.action==EditorAction::Redo) &&
       state_.meshPicker&&state_.resourceProperty=="collision_mesh") refreshCollisionMeshDraft();
    state_.entityMenu=false;
    if(result.status!=EditorActionStatus::Applied) state_.status="Operação indisponível";
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::PrefabCreate)) {
    std::string error;
    if(state_.selectionSet.size()>1) state_.status="Selecione uma raiz para criar o prefab da subárvore";
    else if(!createPrefab(state_.inspectorTarget?state_.inspectorTarget:state_.selection,error).valid()) state_.status=error;
    state_.entityMenu=false;return true;
  }
  if(routing.tapped && (routing.widgetId==widgetId(EditorWidget::PrefabOverrides) ||
      routing.widgetId==widgetId(EditorWidget::PrefabOverridesRefresh))) {
    if(state_.selectionSet.size()>1) {state_.status="Selecione um objeto para comparar com a fonte";return true;}
    const auto target=state_.inspectorTarget?state_.inspectorTarget:state_.selection;
    state_.prefabOperationStatus.clear();
    std::string error;inspectPrefabOverrides(target,state_.prefabOverrides,error);
    state_.prefabOverridesOpen=true;state_.inspectorMenu=false;state_.inspectorDebug=false;
    state_.propertyPage=0;if(!error.empty()) state_.status=error;return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::PrefabOverridesClose)) {
    state_.prefabOverridesOpen=false;state_.propertyPage=0;return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::PrefabReceiveSource)) {
    std::string error;
    if(!refreshPrefabInstance(state_.prefabOverrides,error)) state_.status=error;
    else {const auto target=state_.prefabOverrides.object;inspectPrefabOverrides(target,state_.prefabOverrides,error);state_.propertyPage=0;}
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::PrefabOverridesRevertAll)) {
    std::vector<usize> rows;
    for(usize i=0;i<state_.prefabOverrides.rows.size();++i) rows.push_back(i);
    std::string error;const auto target=state_.prefabOverrides.object;
    if(!revertPrefabOverrides(state_.prefabOverrides,rows,error)) {
      state_.status=error;state_.prefabOperationStatus=error;
    } else {
      inspectPrefabOverrides(target,state_.prefabOverrides,error);state_.propertyPage=0;
      state_.prefabOperationStatus=error.empty()?"Diferenças revertidas neste objeto · Desfazer restaura o lote":error;
    }
    return true;
  }
  if(routing.tapped && (routing.widgetId&0xff000000u)==widgetId(EditorWidget::PrefabOverrideRevertBase)) {
    std::string error;
    if(!revertPrefabOverride(state_.prefabOverrides,routing.widgetId&0x00ffffffu,error)) state_.status=error;
    else {const auto target=state_.prefabOverrides.object;inspectPrefabOverrides(target,state_.prefabOverrides,error);}
    return true;
  }
  if(routing.tapped && (routing.widgetId&0xff000000u)==widgetId(EditorWidget::PrefabOverrideApplyBase)) {
    const auto row=routing.widgetId&0x00ffffffu;
    std::string error;PrefabApplyReport report;
    const bool conflict=row<state_.prefabOverrides.rows.size() &&
      state_.prefabOverrides.rows[row].origin==PrefabOverrideOrigin::Conflict;
    const auto target=state_.prefabOverrides.object;
    if(!applyPrefabOverride(state_.prefabOverrides,row,conflict,error,&report)) {
      state_.prefabOperationStatus=error;state_.status=error;
    } else {
      state_.prefabOperationStatus="Fonte atualizada · "+std::to_string(report.instances)+" instâncias · "+
        std::to_string(report.propagatedFields)+" campos herdados · "+std::to_string(report.conflicts)+" conflitos preservados";
      const auto published=state_.prefabOperationStatus;
      inspectPrefabOverrides(target,state_.prefabOverrides,error);
      state_.prefabOperationStatus=error.empty()?published:published+" · "+error;
    }
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::PrefabUnpack)) {
    std::string error;
    if(state_.selectionSet.size()>1) state_.status="Selecione uma instância para desvincular";
    else if(!unpackPrefab(state_.inspectorTarget?state_.inspectorTarget:state_.selection,error)) state_.status=error;
    state_.entityMenu=false;state_.prefabOverridesOpen=false;return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::InspectorMenu)) state_.prefabOverridesOpen=false;
  const auto previousSurface=state_.inspectorSurface;
  const EditorPointerOutcome outcome =
      applyEditorPointer(state_, layout_, routing, document_, history_);
  if(previousSurface!=state_.inspectorSurface) saveEditorPreferences();
  if (outcome.requestPlay) preparePlay();
  if(!isPlaying()) {jumpPressed_=false;secondaryPressed_=false;}
  return outcome.consumed;
}

} // namespace ae::editor
