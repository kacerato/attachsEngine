#include "editor/editor_component_catalog.h"
#include "editor/editor_collider_fit.h"
#include "scene/script_behavior.h"
#include "editor/editor_water_body_component.h"
#include "editor/editor_route_component.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "scene/import_link.h"
#include "core/sha256.h"
#include "editor/editor_script_templates.h"
#include "editor/editor_reference_picker.h"
#include "editor/editor_properties.h"
#include "editor/editor_theme.h"
#include "renderer/water_authoring_geometry.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <locale>
#include <cstring>

namespace ae::editor {
namespace {

using namespace ae::ui;

float distanceBetween(UiPoint a, UiPoint b) noexcept {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

} // namespace

u64 EditorSession::nextSceneEpoch() noexcept {
  static std::atomic<u64> sequence{1};
  return sequence.fetch_add(1,std::memory_order_relaxed);
}

void EditorSession::initialize(const UiFont *font, const UiIconAtlas *icons) {
  font_ = font;
  icons_ = icons;
  state_.document = &document_;
  state_.resources = &mapScene_;

}

EditorActionResult EditorSession::dispatch(const EditorActionRequest &request) {
  auto result=[&](EditorActionStatus status,EditorEntityId entity=0) {
    return EditorActionResult{status,sceneVersion(),entity};
  };
  const auto version=sceneVersion();
  if(request.version.epoch!=version.epoch || request.version.revision!=version.revision)
    return result(EditorActionStatus::StaleScene);
  if(isPlaying() || history_.isOpen()) return result(EditorActionStatus::Busy);
  const auto *entity=document_.find(request.entity);
  if(request.action!=EditorAction::Undo && request.action!=EditorAction::Redo && !entity)
    return result(EditorActionStatus::InvalidTarget);
  bool applied=false;
  auto changed=request.entity;
  switch(request.action) {
    case EditorAction::Select: setSelection(request.entity);applied=true;break;
    case EditorAction::FrameSelection: setSelection(request.entity);frameSelection();applied=true;break;
    case EditorAction::Rename: {
      if(request.entity==document_.root() || request.name.empty() ||
         request.name.size()>=kEditorNameCapacity || request.name.find('\0')!=std::string::npos) break;
      auto values=*entity;assignEntityName(values,request.name);
      applied=history_.applyValues(document_,request.entity,values);break;
    }
    case EditorAction::Transform:
      applied=history_.setTransform(document_,request.entity,request.transform);break;
    case EditorAction::ComponentProperty: {
      if(const auto *target=std::get_if<scene::ObjectReference>(&request.componentValue)) {
        const auto *property=editorReferenceProperty(*entity,request.componentInstance,request.componentProperty);
        if(!property||!editorReferenceAccepts(document_,request.entity,*property,target->id)) break;
      }
      auto values=*entity;
      if(scene::setComponentProperty(values.components,request.componentType,request.componentProperty,
          request.componentValue,request.componentInstance)==scene::ComponentPropertyStatus::Applied)
        applied=history_.applyValues(document_,request.entity,values);
      break;
    }
    case EditorAction::AddComponent: {
      const auto *entry=findEditorComponent(request.componentType);
      if(!entry||(!entry->type->allowMultiple&&entity->components.find(*entry->type))||entry->unavailable(*entity)) break;
      auto values=*entity;auto *added=values.components.add(*entry->type);if(!added) break;
      if(entry->type==&EditorCollider::descriptor && meshAsset(*entity)) {
        // Missing geometry leaves the explicit default shape available; a fit
        // only publishes a fully prepared conservative candidate.
        auto *collider=editCollider(values,added->instanceId());if(collider) fitEditorCollider(mapScene_,document_,request.entity,*collider);
      }
      applied=history_.applyValues(document_,request.entity,values);break;
    }
    case EditorAction::AddScript: {
      for(const auto &type:code_.scriptTypes()) if(type.id==request.scriptType) {
        auto values=*entity;auto *base=values.components.add(scene::ScriptBehavior::descriptor);if(!base) break;
        auto &script=static_cast<scene::ScriptBehavior &>(*base);script.scriptType=type.id;script.source=type.file;
        applied=history_.applyValues(document_,request.entity,values);break;
      }
      break;
    }
    case EditorAction::RemoveComponent: {
      auto values=*entity;
      if(request.componentInstance&&values.components.removeInstance(request.componentInstance))
        applied=history_.applyValues(document_,request.entity,values);
      break;
    }
    case EditorAction::ScriptProperty:
    case EditorAction::ScriptEnabled: {
      const auto *script=scene::scriptBehavior(entity->components.findInstance(request.componentInstance));
      if(!script) break;
      auto replacement=*script;
      if(request.action==EditorAction::ScriptEnabled) replacement.enabled=request.enabled;
      else {
        bool declared=false;
        for(const auto &type:code_.scriptTypes()) if(type.id==script->scriptType)
          for(const auto &property:type.properties) if(property.id==request.componentProperty&&property.valueType==request.scriptPropertyType) declared=true;
        if(!declared||!replacement.setProperty(request.componentProperty,request.scriptPropertyType,request.scriptPropertyValue)) break;
      }
      auto values=*entity;
      if(values.components.replaceInstance(request.componentInstance,replacement)) applied=history_.applyValues(document_,request.entity,values);
      break;
    }
    case EditorAction::FitCollider: {
      auto values=*entity;auto *collider=editCollider(values,request.componentInstance);
      if(collider && fitEditorCollider(mapScene_,document_,request.entity,*collider)) applied=history_.applyValues(document_,request.entity,values);
      break;
    }
    case EditorAction::AssignMesh: {
      if(!meshRenderer(*entity) || entity->kind==EditorEntityKind::Water || request.property>mapScene_.assetCount()) break;
      if(request.property && (mapScene_.materialFlagsForAsset(request.property-1)&renderer::MapMaterialWater)) break;
      auto values=*entity;auto *mesh=editMeshRenderer(values);if(!mesh) break;
      mesh->mesh=request.property;
      if(!mesh->material.enabled) mesh->material=request.property?mapScene_.materialForAsset(request.property-1):scene::MaterialParameters{};
      applied=history_.applyValues(document_,request.entity,values);break;
    }
    case EditorAction::RestoreMaterial: {
      if(!meshRenderer(*entity)) break;
      auto values=*entity;auto *mesh=editMeshRenderer(values);if(!mesh) break;
      mesh->material=mesh->mesh?mapScene_.materialForAsset(mesh->mesh-1):scene::MaterialParameters{};
      applied=history_.applyValues(document_,request.entity,values);break;
    }
    case EditorAction::NumericProperty: {
      auto values=*entity;
      if(setEditorPropertyValue(values,request.property,request.number))
        applied=history_.applyValues(document_,request.entity,values);
      break;
    }
    case EditorAction::Duplicate:
      changed=history_.duplicateEntity(document_,request.entity);
      applied=changed!=kInvalidEntity;if(applied) setSelection(changed);break;
    case EditorAction::Remove:
      applied=history_.destroyEntity(document_,request.entity);break;
    case EditorAction::Reparent:
      applied=history_.reparentKeepingWorld(document_,request.entity,request.parent);break;
    case EditorAction::Undo: applied=history_.undo(document_);break;
    case EditorAction::Redo: applied=history_.redo(document_);break;
  }
  if(!document_.exists(state_.selection)) state_.selection=kInvalidEntity;
  return result(applied?EditorActionStatus::Applied:EditorActionStatus::InvalidValue,changed);
}

void EditorSession::setSurface(const UiRect &surface, const UiInsets &safeArea) {
  state_.surface = surface;
  state_.safeArea = safeArea;
}

void EditorSession::setSelection(EditorEntityId entity) {
  if (!document_.exists(entity)) return;
  if(state_.selection!=entity) {state_.routePoint=0;state_.propertyPage=0;state_.componentPage=0;state_.expandedScript=0;state_.scriptMenu=0;state_.importLinkMenu=false;
    state_.materialSlot=0;state_.materialShared=false;state_.materialPicker=false;state_.inspectorMenu=false;state_.transformMenu=false;}
  state_.selection=entity;
  // Reveal the selected object through collapsed ancestors and long lists.
  for(auto parent=document_.find(entity);parent;parent=document_.find(parent->parent)) {
    auto &collapsed=state_.collapsedEntities;
    collapsed.erase(std::remove(collapsed.begin(),collapsed.end(),parent->parent),collapsed.end());
  }
  std::vector<EditorEntityId> stack;
  const auto roots=document_.childrenOf(document_.root());
  for(auto it=roots.rbegin();it!=roots.rend();++it) stack.push_back(*it);
  u32 row=0;
  while(!stack.empty()) {
    const auto id=stack.back();stack.pop_back();
    if(id==entity) {
      const auto visible=std::max(1u,layout_.hierarchyVisibleRows);
      if(row<state_.hierarchyScroll) state_.hierarchyScroll=row;
      else if(row>=state_.hierarchyScroll+visible) state_.hierarchyScroll=row-visible+1;
      break;
    }
    ++row;
    if(std::find(state_.collapsedEntities.begin(),state_.collapsedEntities.end(),id)!=state_.collapsedEntities.end()) continue;
    const auto children=document_.childrenOf(id);
    for(auto it=children.rbegin();it!=children.rend();++it) stack.push_back(*it);
  }
}

void EditorSession::setCameraPose(const float position[3], float yaw, float pitch) {
  EditorCamera camera;camera.yaw=yaw;camera.pitch=pitch;
  camera.target[0]=position[0]+std::cos(pitch)*std::sin(yaw)*camera.distance;
  camera.target[1]=position[1]-std::sin(pitch)*camera.distance;
  camera.target[2]=position[2]+std::cos(pitch)*std::cos(yaw)*camera.distance;
  if(isEditorCameraValid(camera)) camera_=camera;
}

void EditorSession::setProjectName(const char *name) {
  if (name != nullptr) state_.projectName = name;
}

bool EditorSession::setProjectDirectory(const char *path) {
  if(code_.dirty()) {state_.status="Salve os arquivos de código antes de trocar de projeto";return false;}
  if(path && *path) {
    std::string recovery;
    if(!EditorImportTransaction::recover(path,recovery)) {state_.status=recovery;return false;}
    if(!recovery.empty()) reportProblem(EditorConsoleSeverity::Warning,recovery);
  }
  code_.clear();state_.code=&code_;
  state_.files=&files_;state_.fileScroll=0;state_.fileScrollOffset=0;
  state_.console=&console_;
  closeImportPreview();state_.importAccept=false;state_.importCancel=false;reimportPath_.clear();
  if(!files_.setRoot(path)) return false;
  state_.codeRecoveryPending=code_.hasRecovery(files_);
  return true;
}

EditorSession::ViewportPointer *EditorSession::findViewportPointer(u32 id) noexcept {
  for (ViewportPointer &pointer : viewportPointers_)
    if (pointer.id == id) return &pointer;
  return nullptr;
}

void EditorSession::buildPickCandidates() {
  candidates_.clear();
  std::vector<EditorEntityId> subtree;
  document_.collectSubtree(document_.root(), subtree);
  for (const EditorEntityId id : subtree) {
    if (id == document_.root()) continue;
    const EditorEntity *entity = document_.find(id);
    if (entity == nullptr) continue;
    // Selection follows the visible capability, including meshes on logical objects.
    const auto *mesh=meshRenderer(*entity);if(!mesh || !mesh->enabled || !mesh->mesh) continue;
    EditorPickCandidate candidate{};
    candidate.id = id;
    // Missing resources have no viewport silhouette. Keep their document row
    // available for repair, but never invent an invisible pick sphere.
    if(!mapScene_.bounds(document_, id, candidate.center, candidate.radius)) continue;
    candidate.selectable = entity->visible && entity->active;
    for(auto parent=document_.find(entity->parent);parent;parent=document_.find(parent->parent))
      candidate.selectable &= parent->visible && parent->active;
    mapScene_.pickGeometry(document_,id,candidate);
    const auto base=candidate;
    candidates_.push_back(std::move(candidate));
    // Um candidato por slot adicional, com o MESMO objeto: tocar qualquer
    // primitiva seleciona o objeto inteiro.
    for(u32 slot=1;slot<mesh->slotCount();++slot) {
      const auto meshSlot=mesh->slotMesh(slot);if(!meshSlot) continue;
      auto extra=base;extra.mesh={};
      if(!mapScene_.slotBounds(document_,id,meshSlot-1,extra.center,extra.radius) ||
         !mapScene_.pickSlotGeometry(document_,id,slot,extra)) continue;
      candidates_.push_back(std::move(extra));
    }
  }
}

void EditorSession::handleGizmoPointer(const UiPointerRouting &routing, u32 axis) {
  const EditorEntity *entity = document_.find(state_.selection);
  if (entity == nullptr || axis > 5) return;
  const bool plane = axis >= 3;
  if(plane && state_.tool != EditorGizmoMode::Translate) return;

  if (!gizmoDrag_.active) {
    EditorGizmoSettings settings{};
    settings.screenLengthPixels = 72.0f;
    float world[16];if(!editorWorldMatrix(document_,entity->id,world)) return;
    float parent[16];editorTransformMatrix(EditorTransform{},parent);
    if(entity->parent && !editorWorldMatrix(document_,entity->parent,parent)) return;
    if(!renderer::buildNormalMatrix(parent,parentInverseTranspose_)) return;
    EditorGizmoFrame frame = buildGizmoFrame(view_, world+12, settings);
    if(plane) {
      if(!gizmoPlanePoint(view_,world+12,axis-3,routing.start,planeStart_)) return;
      rotationView_=view_;
    }
    if(state_.tool==EditorGizmoMode::Rotate) {
      if(!gizmoRingAngle(view_,world+12,axis,routing.start,rotationLastAngle_)) return;
      rotationTotalAngle_=0;rotationView_=view_;
      std::copy(world,world+16,rotationWorld_);std::copy(parent,parent+16,rotationParent_);
      frame.axisUsable[axis]=true;
    }
    const EditorGizmoHandle handle = static_cast<EditorGizmoHandle>(axis + 1);
    if (!beginGizmoDrag(frame, handle, entity->transform, settings, gizmoDrag_)) return;
    state_.activeGizmoAxis = handle;
    dragMode_ = state_.tool;
    dragEntity_ = state_.selection;
    // Uma transação por gesto: o arraste inteiro vira UM passo de desfazer, e
    // não um por frame. É a razão de o histórico ter transações.
    gizmoTransactionOpen_ = history_.begin(state_.tool == EditorGizmoMode::Rotate ? "Rotate" :
                                            state_.tool == EditorGizmoMode::Scale ? "Escala" : "Move");
  }

  EditorTransform moved{};
  bool resolved=false;
  if(plane) {
    float hit[3];
    if(gizmoPlanePoint(rotationView_,gizmoDrag_.frame.origin,axis-3,routing.position,hit)) {
      moved=gizmoDrag_.initial;
      for(u32 i=0;i<3;++i) moved.position[i]+=hit[i]-planeStart_[i];
      resolved=isTransformValid(moved);
    }
  } else if(dragMode_==EditorGizmoMode::Rotate) {
    float angle;
    if(gizmoRingAngle(rotationView_,rotationWorld_+12,axis,routing.position,angle)) {
      rotationTotalAngle_+=std::remainder(angle-rotationLastAngle_,6.28318530718f);
      rotationLastAngle_=angle;
      float rotated[16];std::copy(rotationWorld_,rotationWorld_+16,rotated);
      const u32 u=(axis+1)%3,v=(axis+2)%3;
      const float c=std::cos(rotationTotalAngle_),s=std::sin(rotationTotalAngle_);
      for(u32 column=0;column<3;++column) {
        rotated[column*4+u]=c*rotationWorld_[column*4+u]-s*rotationWorld_[column*4+v];
        rotated[column*4+v]=s*rotationWorld_[column*4+u]+c*rotationWorld_[column*4+v];
      }
      resolved=editorLocalTransformForWorld(rotated,rotationParent_,moved);
      if(!resolved) state_.status="Rotacao exige transformacao sem shear";
    }
  } else resolved=resolveGizmoTransform(gizmoDrag_,dragMode_,routing.totalDelta,moved);
  if (resolved) {
    // O token de fusão é o eixo: comandos consecutivos do mesmo arraste viram
    // um só dentro da transação.
    if(dragMode_==EditorGizmoMode::Translate) {
      float delta[3];for(u32 a=0;a<3;++a) delta[a]=moved.position[a]-gizmoDrag_.initial.position[a];
      for(u32 a=0;a<3;++a) {
        moved.position[a]=gizmoDrag_.initial.position[a];
        for(u32 b=0;b<3;++b) moved.position[a]+=parentInverseTranspose_[a*4+b]*delta[b];
      }
    }
    history_.setTransform(document_, dragEntity_, moved, 0x6000u + axis);
  }

  if (routing.released) {
    if (gizmoTransactionOpen_) history_.end();
    gizmoTransactionOpen_ = false;
    gizmoDrag_ = EditorGizmoDrag{};
    state_.activeGizmoAxis = EditorGizmoHandle::None;
  }
}

bool EditorSession::handleViewportPointer(const UiPointerEvent &event,
                                          const UiPointerRouting &routing) {
  if (event.phase == UiPointerPhase::Down) {
    viewportPointers_.erase(std::remove_if(viewportPointers_.begin(), viewportPointers_.end(),
        [&](const ViewportPointer &p) { return p.id == event.pointerId; }), viewportPointers_.end());
    viewportPointers_.push_back({event.pointerId, event.position, false});
    if (viewportPointers_.size() > 1)
      for (auto &p : viewportPointers_) p.moved = true;
    pinchDistance_ = viewportPointers_.size() == 2
        ? distanceBetween(viewportPointers_[0].position, viewportPointers_[1].position)
        : 0.0f;
    return true;
  }

  ViewportPointer *pointer = findViewportPointer(event.pointerId);
  if (pointer == nullptr) return false;

  if (event.phase == UiPointerPhase::Move) {
    const UiPoint previous = pointer->position;
    pointer->position = event.position;
    if (routing.dragging) pointer->moved = true;

    if (viewportPointers_.size() >= 2) {
      // Dois dedos: o deslocamento do ponto médio desloca o alvo, e a variação
      // da distância entre eles afasta ou aproxima. As duas coisas ao mesmo
      // tempo, porque é assim que a mão faz.
      const UiPoint first = viewportPointers_[0].position;
      const UiPoint second = viewportPointers_[1].position;
      const float distance = distanceBetween(first, second);
      if (pinchDistance_ > 1.0f && distance > 1.0f)
        zoomEditorCamera(camera_, pinchDistance_ / distance);
      pinchDistance_ = distance;
      const UiPoint delta{(event.position.x - previous.x) * 0.5f,
                          (event.position.y - previous.y) * 0.5f};
      panEditorCamera(camera_, delta, layout_.viewport, projection_);
    } else if (routing.dragging) {
      const UiPoint delta{event.position.x-previous.x,event.position.y-previous.y};
      switch(state_.navigation) {
        case EditorNavigationMode::Orbit: orbitEditorCamera(camera_,delta);break;
        case EditorNavigationMode::Pan: panEditorCamera(camera_,delta,layout_.viewport,projection_);break;
        case EditorNavigationMode::Zoom:
          zoomEditorCamera(camera_,std::exp(std::clamp(delta.y/std::max(1.0f,layout_.viewport.height)*4.0f,-2.0f,2.0f)));break;
      }
    }
    return true;
  }

  // Levantou. Um toque curto que não virou arraste é uma seleção.
  const bool wasTap = !pointer->moved && event.phase == UiPointerPhase::Up;
  const UiPoint at = event.position;
  viewportPointers_.erase(viewportPointers_.begin() +
                          (pointer - viewportPointers_.data()));
  pinchDistance_ = viewportPointers_.size() == 2
      ? distanceBetween(viewportPointers_[0].position, viewportPointers_[1].position)
      : 0.0f;

  if (wasTap && viewportPointers_.empty()) {
    buildPickCandidates();
    const EditorPickResult hit = pickNearest(candidates_, screenPointToRay(view_, at));
    // Tocar no vazio LIMPA a seleção. É o gesto que todo editor tem, e sem ele
    // não há como desmarcar sem selecionar outra coisa.
    if(hit.hit) setSelection(hit.id); else state_.selection=kInvalidEntity;
  }
  return true;
}

EditorTextEdit EditorSession::pendingTextEdit() const {
  EditorTextEdit edit;edit.version=sceneVersion();
  if(state_.editingCode && !state_.platformCodeView) {
    if(const auto *buffer=code_.active()) {
      edit.purpose=EditorTextPurpose::Code;edit.text=buffer->text;
      edit.bufferId=buffer->id;edit.bufferRevision=buffer->revision;
    }
    return edit;
  }
  if(state_.editingScriptInstance) {
    edit.purpose=EditorTextPurpose::ScriptProperty;edit.entity=state_.editingScriptEntity;
    edit.componentInstance=state_.editingScriptInstance;edit.propertyId=state_.editingScriptProperty;
    edit.propertyType=state_.editingScriptType;
    if(const auto *entity=document_.find(edit.entity))
      if(const auto *script=scene::scriptBehavior(entity->components.findInstance(edit.componentInstance)))
        for(const auto &p:script->properties) if(p.id==edit.propertyId) edit.text=p.value;
    return edit;
  }
  if(state_.renamingResource && !state_.selectedFile.empty()) {
    edit.purpose=EditorTextPurpose::ResourceName;
    const auto slash=state_.selectedFile.find_last_of('/');
    edit.text=slash==std::string::npos?state_.selectedFile:state_.selectedFile.substr(slash+1);
    return edit;
  }
  if(state_.creatingScript) {edit.purpose=EditorTextPurpose::ScriptName;return edit;}
  if(state_.searchingCode) {edit.purpose=EditorTextPurpose::CodeSearch;edit.text=state_.codeQuery;return edit;}
  if(state_.searchingConsole) {edit.purpose=EditorTextPurpose::ConsoleSearch;edit.text=state_.consoleQuery;return edit;}
  if(state_.goingToLine) {edit.purpose=EditorTextPurpose::CodeLine;return edit;}
  if(state_.creatingCodeFolder) {edit.purpose=EditorTextPurpose::CodeFolder;return edit;}
  if(state_.editingComponentSearch) {edit.purpose=EditorTextPurpose::ComponentSearch;edit.text=state_.renameText;return edit;}
  if(state_.editingReferenceSearch) {edit.purpose=EditorTextPurpose::ReferenceSearch;edit.text=state_.renameText;return edit;}
  if(state_.editingMeshSearch) {edit.purpose=EditorTextPurpose::MeshSearch;edit.text=state_.renameText;return edit;}
  if(state_.numericField) {
    edit.purpose=EditorTextPurpose::Number;edit.entity=state_.numericEntity;
    edit.field=state_.numericField;edit.text=state_.numericText;
    edit.componentInstance=state_.numericInstance;edit.propertyId=state_.numericProperty;
  } else {
    if(!state_.editingCreationSearch && !state_.editingHierarchySearch && !state_.renameEntity) return edit;
    edit.entity=state_.renameEntity;edit.text=state_.renameText;
    if(state_.editingCreationSearch) edit.purpose=EditorTextPurpose::CreationSearch;
    else if(state_.editingHierarchySearch) edit.purpose=EditorTextPurpose::HierarchySearch;
    else if(state_.renameEntity) edit.purpose=EditorTextPurpose::Rename;
  }
  return edit;
}

bool EditorSession::updateTextDraft(const EditorTextEdit &edit,std::string_view text,u32 caret) {
  const auto current=pendingTextEdit();
  if(edit.purpose==EditorTextPurpose::None || current.purpose!=edit.purpose ||
     current.entity!=edit.entity || current.field!=edit.field || edit.version.epoch!=sceneEpoch_) return false;
  // O mesmo teto que a ponte aplica. Um rascunho maior que o campo aceita não é
  // rascunho: é um commit que vai ser recusado no fim, depois de o usuário ter
  // digitado tudo.
  if(text.size()>EditorCodeWorkspace::MaximumFileBytes) return false;
  state_.platformDraft.assign(text);
  state_.platformCaret=static_cast<u32>(std::min<usize>(caret,text.size()));
  // Codigo nao tem "aplicar": o texto digitado E o buffer. Ele entra ja, por
  // `type`, que agrupa a sessao inteira de digitacao em UMA entrada de
  // desfazer -- `replace` guarda uma copia do arquivo por chamada, e uma
  // chamada por tecla faria desfazer voltar caractere a caractere.
  if(edit.purpose==EditorTextPurpose::Code) {
    if(!typeCode(text)) return false;
    followCodeCaret();
    return true;
  }
  // Busca filtra enquanto se digita. É o único efeito permitido antes de
  // confirmar, e ele não toca no documento nem no histórico.
  switch(edit.purpose) {
    case EditorTextPurpose::HierarchySearch:
    case EditorTextPurpose::CreationSearch:
    case EditorTextPurpose::ComponentSearch:
    case EditorTextPurpose::MeshSearch:
    case EditorTextPurpose::ReferenceSearch: {
      const auto size=std::min(text.size(),sizeof(state_.renameText)-1);
      std::memcpy(state_.renameText,text.data(),size);
      state_.renameText[size]='\0';
      break;
    }
    default: break;
  }
  return true;
}

// Rolagem acompanha o cursor. Sem isto, digitar no fim de um arquivo longo
// escreveria fora da tela.
// Tocar numa linha do console LEVA ate o lugar dela. Sem isso o console e um
// mural: diz que algo aconteceu e deixa o usuario procurar onde.
void EditorSession::reportScriptSchemaChanges() {
  std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
  for(const auto id:ids) {
    const auto *object=document_.find(id);
    for(usize i=0;i<object->components.size();++i) if(const auto *script=scene::scriptBehavior(object->components.at(i))) {
      auto emit=[&](std::string message) {
        EditorConsoleEntry entry;entry.origin=EditorConsoleOrigin::Editor;entry.severity=EditorConsoleSeverity::Warning;
        entry.message=std::move(message);entry.object=id;entry.component=script->instanceId();
        entry.project=files_.rootPath();entry.buildGeneration=code_.publishedGeneration();console_.add(std::move(entry));
      };
      const auto schema=std::find_if(code_.scriptTypes().begin(),code_.scriptTypes().end(),[&](const auto &type){return type.id==script->scriptType;});
      if(schema==code_.scriptTypes().end()) {emit("Tipo não resolvido: "+script->scriptType+". Instância e valores preservados.");continue;}
      for(const auto &value:script->properties) {
        const auto property=std::find_if(schema->properties.begin(),schema->properties.end(),[&](const auto &field){return field.id==value.id;});
        if(property==schema->properties.end()) emit("Campo removido da fonte: "+value.id+". Valor autorado preservado para recuperação.");
        else if(property->valueType!=value.valueType) emit("Tipo do campo "+value.id+" mudou de "+value.valueType+" para "+property->valueType+". Revise o valor antes de Play.");
      }
    }
  }
}
void EditorSession::jumpToConsoleEntry(u32 index) {
  const auto *entry=console_.at(index);
  if(!entry) return;
  if(!entry->project.empty() && entry->project!=files_.rootPath()) {state_.status="Evento de outro projeto";return;}
  if(entry->playSession && entry->playSession!=playScene_.world().worldId() && entry->object) {
    state_.status="Objeto de uma sessão Play encerrada; evento preservado no console";return;
  }
  if(!entry->file.empty()) {
    if(!code_.open(files_,entry->file)) { state_.status=code_.error(); return; }
    state_.code=&code_;state_.workspace=EditorWorkspace::Code;
    auto *buffer=code_.active();
    if(!buffer) return;
    if(entry->buildGeneration && entry->buildGeneration!=code_.generation()) {
      state_.codeFiles=false;state_.status="Fonte alterada; consulte o trecho original no detalhe do evento";return;
    }
    // A linha do diagnostico e 1-based; a do editor conta do zero.
    const u32 target=entry->line>0?entry->line-1:0;
    const u32 visible=layout_.codeVisibleLines>2?layout_.codeVisibleLines:12;
    buffer->firstLine=target>visible/2?target-visible/2:0;
    usize start=0;u32 line=0;
    while(line<target) {
      const auto end=buffer->text.find(char(10),start);
      if(end==std::string::npos) { start=buffer->text.size(); break; }
      start=end+1;++line;
    }
    code_.locate(entry->line,entry->column);
    state_.platformCaret=buffer->selectionEnd;
    state_.codeFiles=false;
    state_.platformDraft=buffer->text;
    state_.status="Linha "+std::to_string(entry->line)+" de "+entry->file;
    return;
  }
  if(entry->object!=0 && document_.find(static_cast<EditorEntityId>(entry->object))) {
    setSelection(static_cast<EditorEntityId>(entry->object));
    state_.workspace=EditorWorkspace::Scene;
    return;
  }
  state_.status="Esta mensagem não aponta para um lugar";
}

// Colagem e UMA transacao, e nao a continuacao da digitacao.
//
// Uma insercao de varios caracteres num evento so nao veio de teclas: veio da
// area de transferencia ou de uma correcao inteira. Junta-la a sessao de
// digitacao faria um desfazer levar embora as duas coisas de uma vez.
bool EditorSession::typeCode(std::string_view text) {
  auto *buffer=code_.active();
  if(!buffer) { state_.status="Nenhum arquivo de código aberto"; return false; }
  const auto before=buffer->text.size();
  const bool bulk=text.size()>before+2 || before>text.size()+2;
  if(bulk) code_.endTypingRun();
  if(!code_.type(buffer->id,text)) { state_.status=code_.error(); return false; }
  if(bulk) code_.endTypingRun();
  return true;
}

void EditorSession::followCodeCaret() {
  auto *buffer=code_.active();
  if(!buffer) return;
  const auto caret=std::min<usize>(state_.platformCaret,buffer->text.size());
  const auto line=static_cast<u32>(std::count(buffer->text.begin(),buffer->text.begin()+static_cast<long>(caret),10));
  const u32 visible=layout_.codeVisibleLines>2?layout_.codeVisibleLines:12;
  if(line<buffer->firstLine) buffer->firstLine=line;
  else if(line>=buffer->firstLine+visible) buffer->firstLine=line-visible+1;
}

// O toque escolhe a LINHA e o cursor vai para o fim dela.
//
// Coluna exata exigiria medir o texto, que so acontece na construcao das
// instancias; aproximar por largura media poria o cursor no lugar errado em
// cada linha com indentacao, que e toda linha de codigo. Fim de linha e
// previsivel e util, e a coluna exata e o que falta do M05.3.
void EditorSession::placeCodeCaret(ui::UiPoint position) {
  auto *buffer=code_.active();
  if(!buffer || layout_.codeBody.height<=0 || layout_.codeLineHeight<=0) return;
  const float offset=(position.y-layout_.codeBody.y)/layout_.codeLineHeight;
  const u32 row=buffer->firstLine+static_cast<u32>(std::max(0.0f,offset));
  usize start=0;u32 line=0;
  while(line<row) {
    const auto end=buffer->text.find(10,start);
    if(end==std::string::npos) { start=buffer->text.size();break; }
    start=end+1;++line;
  }
  const auto end=buffer->text.find(10,start);
  state_.platformCaret=static_cast<u32>(end==std::string::npos?buffer->text.size():end);
  state_.platformDraft=buffer->text;
  followCodeCaret();
}

void EditorSession::setPlatformImeFraction(float fraction) {
  state_.platformImeFraction=std::isfinite(fraction)?std::clamp(fraction,0.0f,0.9f):0.0f;
}

void EditorSession::setCodeViewState(u64 id,u64 revision,u32 start,u32 end,float x,float y,bool focus) {
  if(!code_.setSelection(id,revision,start,end,std::isfinite(x)?std::max(0.0f,x):0,
                          std::isfinite(y)?std::max(0.0f,y):0)) return;
  state_.platformCaret=end;state_.editingCode=focus;
  if(!focus) {code_.endTypingRun();state_.codeComposing=false;}
}
void EditorSession::codeHistoryAction(bool redo) {
  if(redo) code_.redo();else code_.undo();
}
void EditorSession::recoverCodeDraft(std::string_view text) {
  if(text.size()>EditorCodeWorkspace::MaximumFileBytes) return;
  const std::string folder=".astra/recovery";
  if(!files_.createDirectory(folder)) {state_.status=files_.error();return;}
  std::string path;
  for(u64 i=1;;++i) {path=folder+"/Codigo-recuperado-"+std::to_string(i)+".txt";if(!files_.exists(path)) break;}
  if(!files_.createTextFile(path,text)) {state_.status=files_.error();return;}
  EditorConsoleEntry entry;entry.severity=EditorConsoleSeverity::Warning;
  entry.message="Edição concorrente: rascunho preservado. Toque para abrir a recuperação.";
  entry.file=path;entry.line=1;console_.add(std::move(entry));
  state_.consoleCollapsed=false;state_.status="Rascunho preservado em "+path;
}

namespace {
// Espaco e quebra de linha nas bordas de um NOME nao sao conteudo.
//
// O teclado do sistema acrescenta os dois sem o usuario ver: a sugestao vem com
// espaco no fim, e a tecla de confirmar pode chegar como quebra de linha antes
// do evento de acao. Recusar o nome por causa deles devolve "use letras,
// numeros e sublinhado" para quem digitou exatamente isso -- foi o que
// aconteceu ao criar um script no aparelho.
std::string_view trimmedName(std::string_view text) {
  const auto blank=[](char c) { return c==' '||c==char(9)||c==char(10)||c==char(13); };
  while(!text.empty() && blank(text.front())) text.remove_prefix(1);
  while(!text.empty() && blank(text.back())) text.remove_suffix(1);
  return text;
}
} // namespace

bool EditorSession::completeTextEdit(const EditorTextEdit &edit,std::string_view text,bool accept) {
  const auto current=pendingTextEdit();
  if(edit.purpose==EditorTextPurpose::None || current.purpose!=edit.purpose ||
     current.entity!=edit.entity || current.field!=edit.field || edit.version.epoch!=sceneEpoch_) return false;
  const auto close=[&] {
    code_.endTypingRun();
    state_.editingCode=false;state_.creatingScript=false;state_.searchingCode=false;
    state_.goingToLine=false;state_.creatingCodeFolder=false;state_.codeComposing=false;
    state_.searchingConsole=false;
    state_.renamingResource=false;
    state_.choosingTemplate=false;
    state_.editingScriptInstance=0;state_.editingScriptEntity=0;state_.editingScriptProperty.clear();state_.editingScriptType.clear();
    state_.numericField=0;state_.numericInstance=0;state_.numericProperty.clear();state_.renameEntity=0;
    state_.editingComponentSearch=false;state_.editingMeshSearch=false;state_.editingReferenceSearch=false;
    state_.editingHierarchySearch=false;state_.editingCreationSearch=false;
    cancelPointers();
  };
  if(!accept) {close();return true;}
  if(edit.purpose==EditorTextPurpose::ConsoleSearch) {
    state_.consoleQuery=std::string(text);state_.consoleScroll=0;state_.consoleAnchor=0;close();return true;
  }
  if(edit.purpose==EditorTextPurpose::CodeLine) {
    u32 line=0;std::istringstream value{std::string(text)};
    if(!(value>>line) || line==0) {state_.status="Informe um número de linha positivo";return false;}
    value>>std::ws;if(!value.eof()) return false;
    code_.locate(line);close();return true;
  }
  if(edit.purpose==EditorTextPurpose::CodeFolder) {
    const auto name=trimmedName(text);
    if(name.empty() || name=="." || name==".." || name.find_first_of("/\\")!=std::string_view::npos) {
      state_.status="Informe apenas o nome da nova pasta";return false;
    }
    std::string parent;
    for(const auto &entry:files_.tree()) if(entry.relativePath==state_.selectedFile) {
      if(entry.directory) parent=entry.relativePath;
      else {const auto slash=entry.relativePath.find_last_of('/');if(slash!=std::string::npos) parent=entry.relativePath.substr(0,slash);}
    }
    const auto path=(parent.empty()?"":parent+"/")+std::string(name);
    if(files_.exists(path)) {state_.status="Já existe um recurso com esse nome";return false;}
    if(!files_.createDirectory(path)) {state_.status=files_.error();return false;}
    files_.rebuildTree();state_.selectedFile=path;state_.status="Pasta criada: "+path;close();return true;
  }
  if(edit.purpose==EditorTextPurpose::ScriptProperty) {
    const auto *entity=document_.find(edit.entity);
    if(!entity || isPlaying() || edit.version.revision!=document_.revision() ||
       current.componentInstance!=edit.componentInstance || current.propertyId!=edit.propertyId || current.propertyType!=edit.propertyType) return false;
    const auto *script=scene::scriptBehavior(entity->components.findInstance(edit.componentInstance));
    if(!script) return false;
    auto replacement=*script;
    if(!replacement.setProperty(edit.propertyId,edit.propertyType,text)) {state_.status="Valor incompatível com o tipo do campo";return false;}
    auto value=*entity;
    if(!value.components.replaceInstance(edit.componentInstance,replacement) || !history_.applyValues(document_,edit.entity,value)) return false;
    close();return true;
  }
  if(edit.purpose==EditorTextPurpose::Code) {
    // Digitando, o texto ja entrou tecla a tecla e a revisao do instantaneo que
    // abriu o campo ficou para tras de proposito. Confirmar so fecha.
    const auto *buffer=code_.active();
    if(buffer && buffer->id==edit.bufferId && buffer->text==text) { close(); return true; }
    const bool accepted=code_.replace(edit.bufferId,
        buffer && buffer->id==edit.bufferId?buffer->revision:edit.bufferRevision,text);
    if(accepted) close();else state_.status=code_.error();
    return accepted;
  }
  if(edit.purpose==EditorTextPurpose::ResourceName) {
    if(state_.selectedFile.empty()) return false;
    // Nome, nao caminho: barra e ".." aqui virariam mover disfarcado de
    // renomear, e a checagem de destino do sistema de arquivos passaria a ser a
    // unica barreira.
    const auto name=trimmedName(text);
    if(name.empty() || name.find('/')!=std::string_view::npos ||
       name.find(char(92))!=std::string_view::npos || name==".." || name==".") {
      state_.status="Nome invalido para um arquivo";return false;
    }
    const auto slash=state_.selectedFile.find_last_of('/');
    const std::string destination=
        (slash==std::string::npos?std::string():state_.selectedFile.substr(0,slash+1))+std::string(name);
    ResourceChangeReport report;
    if(!moveResource(state_.selectedFile,destination,report)) {
      state_.status=report.diagnostic;return false;
    }
    state_.selectedFile=destination;
    close();return true;
  }
  if(edit.purpose==EditorTextPurpose::ScriptName) {
    std::string directory="Scripts";
    for(const auto &entry:files_.tree()) if(!state_.selectedFile.empty() && entry.relativePath==state_.selectedFile) {
      if(entry.directory) directory=entry.relativePath;
      else {const auto slash=entry.relativePath.find_last_of('/');directory=slash==std::string::npos?"":entry.relativePath.substr(0,slash);}
    }
    if(!code_.createScript(files_,trimmedName(text),state_.scriptTemplate,directory)) {state_.status=code_.error();return false;}
    state_.scriptTemplate=~0u;
    files_.rebuildTree();state_.codeFiles=false;state_.codeFirstTab=static_cast<u32>(code_.buffers().size()-1);
    state_.workspace=EditorWorkspace::Code;close();return true;
  }
  if(edit.purpose==EditorTextPurpose::CodeSearch) {
    state_.codeQuery=text;const auto matches=code_.find(text);
    if(auto *buffer=code_.active();buffer && !matches.empty()) {
      buffer->selectionStart=static_cast<u32>(matches.front().offset);
      buffer->selectionEnd=static_cast<u32>(matches.front().offset+matches.front().length);
      buffer->firstLine=matches.front().line-1;++buffer->viewRevision;
    }
    state_.status=std::to_string(matches.size())+" ocorrências";close();return true;
  }
  if(edit.purpose==EditorTextPurpose::ComponentSearch || edit.purpose==EditorTextPurpose::MeshSearch || edit.purpose==EditorTextPurpose::ReferenceSearch) {
    if(text.size()>63 || text.find('\0')!=std::string_view::npos) return false;
    if(edit.purpose==EditorTextPurpose::ComponentSearch) {state_.componentQuery=text;state_.componentPage=0;}
    else if(edit.purpose==EditorTextPurpose::ReferenceSearch) {state_.referenceQuery=text;state_.referencePage=0;}
    else {state_.meshQuery=text;state_.meshPage=0;}
    close();return true;
  }
  if(edit.version.revision!=document_.revision() || history_.isOpen() || isPlaying()) {
    close();state_.status="Edição cancelada: a cena mudou";return false;
  }
  const usize capacity=edit.purpose==EditorTextPurpose::Number?sizeof(state_.numericText):sizeof(state_.renameText);
  if(text.size()>=capacity || text.find('\0')!=std::string_view::npos) {
    state_.status="Texto excede o limite do campo";return false;
  }
  std::string value(text);
  if(edit.purpose==EditorTextPurpose::Number) {
    // Android keyboards may use a decimal comma. Mixed separators remain invalid.
    if(value.find('.')==std::string::npos) std::replace(value.begin(),value.end(),',','.');
    float number=0;std::istringstream input(value);input.imbue(std::locale::classic());
    const bool parsed=static_cast<bool>(input>>number);input>>std::ws;
    const auto *entity=document_.find(edit.entity);
    state_.numericError=true;
    if(!entity || !parsed || !input.eof() || !std::isfinite(number)) return false;
    // Campo de material de slot: vale no alcance escolhido no inspetor.
    if(edit.field>=widgetId(EditorWidget::MaterialNumberBase) &&
       edit.field-widgetId(EditorWidget::MaterialNumberBase)<scene::meshRendererNumbers.size()) {
      std::string diagnostic;
      if(!setSlotMaterialValue(edit.entity,state_.materialSlot,state_.materialShared?MaterialScope::Shared:MaterialScope::Instance,
                               edit.field-widgetId(EditorWidget::MaterialNumberBase),number,diagnostic)) {
        state_.status=diagnostic;return false;
      }
      state_.numericError=false;close();return true;
    }
    auto values=*entity;
    bool changed=false;
    if(edit.componentInstance) {
      const auto *component=values.components.findInstance(edit.componentInstance);
      if(current.componentInstance!=edit.componentInstance || current.propertyId!=edit.propertyId || !component) return false;
      changed=scene::setComponentProperty(values.components,component->type().id,edit.propertyId,number,edit.componentInstance)==scene::ComponentPropertyStatus::Applied;
    } else changed=setEditorPropertyValue(values,edit.field-widgetId(EditorWidget::TransformFieldBase),number);
    if(!changed || !history_.applyValues(document_,edit.entity,values)) return false;
  } else if(edit.purpose==EditorTextPurpose::Rename) {
    const auto first=value.find_first_not_of(' '),last=value.find_last_not_of(' ');
    if(first==std::string::npos) {state_.status="Nome não pode ficar vazio";return false;}
    for(unsigned char c:value) if(c<32 || c==127) return false;
    const auto *entity=document_.find(edit.entity);if(!entity) {close();return false;}
    auto values=*entity;assignEntityName(values,value.substr(first,last-first+1));
    if(!history_.applyValues(document_,edit.entity,values)) return false;
  } else if(edit.purpose==EditorTextPurpose::HierarchySearch) {
    std::snprintf(state_.hierarchySearch,sizeof(state_.hierarchySearch),"%s",value.c_str());
    state_.hierarchyScroll=0;
  } else {
    std::snprintf(state_.creationSearch,sizeof(state_.creationSearch),"%s",value.c_str());
    state_.creationPage=0;const auto query=editorSearchKey(value);
    for(u32 i=0;i<editorCreationCatalog.size();++i)
      if(creationAvailable(state_,i) && (query.empty()?editorCreationCatalog[i].category==state_.creationCategory:
         editorSearchKey(editorCreationCatalog[i].name).find(query)!=std::string::npos)) {state_.creationSelection=i;break;}
  }
  close();return true;
}

bool EditorSession::handlePointer(const UiPointerEvent &event) {
  if(event.phase==UiPointerPhase::Cancel) playTouches_.cancel();
  if(state_.platformTextInput && pendingTextEdit().purpose!=EditorTextPurpose::None) return true;
  const UiPointerRouting routing = router_.route(event);
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
  if(state_.importPanel) {
    if(routing.tapped && routing.widgetId==widgetId(EditorWidget::ImportPreviousPage) && state_.importPage) --state_.importPage;
    if(routing.tapped && routing.widgetId==widgetId(EditorWidget::ImportNextPage)) ++state_.importPage;
    if(routing.tapped && routing.widgetId==widgetId(EditorWidget::ImportCancel)) {state_.importCancel=!state_.importError;closeImportPreview();}
    if(routing.tapped && routing.widgetId==widgetId(EditorWidget::ImportMatchInOrder)) state_.importAmbiguityChoice=1;
    if(routing.tapped && routing.widgetId==widgetId(EditorWidget::ImportTreatAsNew)) state_.importAmbiguityChoice=2;
    // Com ambiguidade, publicar só depois da escolha explícita.
    if(routing.tapped && (routing.widgetId==widgetId(EditorWidget::ImportAccept) ||
        routing.widgetId==widgetId(EditorWidget::ImportIntoScene)) && state_.importReady &&
        (!state_.importAmbiguities || state_.importAmbiguityChoice)) {
      state_.importIntoScene=routing.widgetId==widgetId(EditorWidget::ImportIntoScene);
      state_.importAccept=true;state_.importReady=false;state_.importStatus="Publicando recurso…";
    }
    return true;
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
  if(routing.tapped && !isPlaying()) {
    if(state_.componentSelection!=state_.selection) {
      state_.componentSelection=state_.selection;state_.referenceInstance=0;state_.expandedComponent.clear();state_.expandedNative=0;state_.nativeMenu=0;
      state_.expandedScript=0;state_.scriptMenu=0;state_.meshPicker=false;state_.componentPage=0;state_.propertyPage=0;state_.addingComponent=false;
    }
    const u32 key=routing.widgetId;
    if(key==widgetId(EditorWidget::ReferenceClose)) {state_.referenceInstance=0;return true;}
    if(key==widgetId(EditorWidget::ReferencePrevious)) {if(state_.referencePage) --state_.referencePage;return true;}
    if(key==widgetId(EditorWidget::ReferenceNext)) {++state_.referencePage;return true;}
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
    if(key==widgetId(EditorWidget::ReferenceClear)||(key>=widgetId(EditorWidget::ReferenceChoiceBase)&&key<widgetId(EditorWidget::ReferenceChoiceBase)+0x01000000u)) {
      const auto *entity=document_.find(state_.selection);if(!entity||!state_.referenceInstance) return true;
      const auto *property=state_.referenceScript?&editorAnyObjectReference:editorReferenceProperty(*entity,state_.referenceInstance,state_.referenceProperty);
      if(!property) {state_.referenceInstance=0;return true;}
      u64 target=0;
      if(key!=widgetId(EditorWidget::ReferenceClear)) {
        const auto choices=editorReferenceChoices(document_,entity->id,*property,state_.referenceQuery);const auto index=key-widgetId(EditorWidget::ReferenceChoiceBase);
        if(index>=choices.size()) return true;
        target=choices[index];
      }
      EditorActionRequest request;request.version=sceneVersion();request.entity=entity->id;request.componentInstance=state_.referenceInstance;request.componentProperty=state_.referenceProperty;
      if(state_.referenceScript) {request.action=EditorAction::ScriptProperty;request.scriptPropertyType="object";request.scriptPropertyValue=std::to_string(target);}
      else {request.action=EditorAction::ComponentProperty;request.componentType=entity->components.findInstance(state_.referenceInstance)->type().id;request.componentValue=scene::ObjectReference{target};}
      if(dispatch(request).status==EditorActionStatus::Applied) {state_.referenceInstance=0;state_.status="Referência atualizada";}
      else state_.status="A referência não é compatível com este campo";
      return true;
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
    if(key==widgetId(EditorWidget::ComponentSearch) || key==widgetId(EditorWidget::MeshSearch)) {
      state_.editingComponentSearch=key==widgetId(EditorWidget::ComponentSearch);state_.editingMeshSearch=!state_.editingComponentSearch;
      const auto &query=state_.editingComponentSearch?state_.componentQuery:state_.meshQuery;
      std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",query.c_str());return true;
    }
    if(key==widgetId(EditorWidget::ComponentSearchClear)) {state_.componentQuery.clear();state_.componentPage=0;return true;}
    if(key==widgetId(EditorWidget::ComponentCategory)) {state_.componentCategory=(state_.componentCategory+1)%5;state_.componentPage=0;return true;}
    if(key==widgetId(EditorWidget::ObjectFold)) {
      state_.componentSelection=state_.selection;state_.expandedComponent=state_.expandedComponent=="astra.object"?"":"astra.object";
      state_.expandedNative=0;state_.expandedScript=0;state_.nativeMenu=0;state_.scriptMenu=0;state_.meshPicker=false;
      state_.transformMenu=false;state_.propertyPage=0;return true;
    }
    if(key==widgetId(EditorWidget::TransformFold)) {
      state_.componentSelection=state_.selection;state_.expandedComponent=state_.expandedComponent=="astra.transform"?"":"astra.transform";
      state_.expandedNative=0;state_.expandedScript=0;state_.nativeMenu=0;state_.scriptMenu=0;state_.meshPicker=false;state_.propertyPage=0;return true;
    }
    if(key==widgetId(EditorWidget::MeshChoose)) {state_.meshPicker=true;state_.meshPage=0;return true;}
    // Vínculo com a fonte importada (M08.2).
    if(key==widgetId(EditorWidget::ImportLinkMenu)) {state_.importLinkMenu=!state_.importLinkMenu;return true;}
    if(key>=widgetId(EditorWidget::ImportLinkRevertBase) && key<widgetId(EditorWidget::ImportLinkRevertBase)+64u) {
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
    if(key==widgetId(EditorWidget::MeshPickerClose)) {state_.meshPicker=false;return true;}
    if(key==widgetId(EditorWidget::MeshPrevious)) {if(state_.meshPage) --state_.meshPage;return true;}
    if(key==widgetId(EditorWidget::MeshNext)) {++state_.meshPage;return true;}
    if(key==widgetId(EditorWidget::MeshGeometryTab)||key==widgetId(EditorWidget::MeshMaterialTab)) {state_.meshTab=key==widgetId(EditorWidget::MeshMaterialTab);state_.propertyPage=0;state_.meshPicker=false;return true;}
    if((key>=widgetId(EditorWidget::MeshChoiceBase) && key<widgetId(EditorWidget::MeshChoiceBase)+0x01000000u) || key==widgetId(EditorWidget::MeshClear) || key==widgetId(EditorWidget::MaterialRestore)) {
      EditorActionRequest request;request.version=sceneVersion();request.entity=state_.selection;
      request.action=key==widgetId(EditorWidget::MaterialRestore)?EditorAction::RestoreMaterial:EditorAction::AssignMesh;
      request.property=key>=widgetId(EditorWidget::MeshChoiceBase)?key-widgetId(EditorWidget::MeshChoiceBase)+1:0;
      if(dispatch(request).status==EditorActionStatus::Applied) {state_.meshPicker=false;state_.status=request.action==EditorAction::RestoreMaterial?"Material da origem restaurado":"Referência de malha atualizada";}
      else state_.status="Referência incompatível com este objeto";
      return true;
    }
    if(key>=widgetId(EditorWidget::ComponentAddBase)&&key-widgetId(EditorWidget::ComponentAddBase)<editorComponentCatalog.size()) {
      const auto &entry=editorComponentCatalog[key-widgetId(EditorWidget::ComponentAddBase)];
      EditorActionRequest request;request.version=sceneVersion();request.action=EditorAction::AddComponent;
      request.entity=state_.selection;request.componentType=entry.type->id;
      const auto result=dispatch(request);
      if(result.status==EditorActionStatus::Applied) {
        state_.componentSelection=state_.selection;state_.expandedComponent.clear();state_.expandedNative=0;state_.expandedScript=0;
        state_.addingComponent=false;state_.componentPage=~u32{0};state_.nativeMenu=0;state_.scriptMenu=0;
        state_.status="Componente adicionado";
      } else if(const auto *entity=document_.find(state_.selection)) {
        const auto *reason=entry.unavailable(*entity);state_.status=reason?reason:"Não foi possível adicionar o componente";
      }
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
    if(key>=widgetId(EditorWidget::ScriptAddBase)&&key<widgetId(EditorWidget::ScriptFieldBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);if(!entity||state_.workspace!=EditorWorkspace::Scene||history_.isOpen()) return true;
      const u32 operation=key&0xff000000u;
      const u32 index=key&(operation==widgetId(EditorWidget::ScriptFieldBase)?0xffu:0x00ffffffu);
      state_.componentSelection=state_.selection;
      if(operation==widgetId(EditorWidget::ScriptAddBase)) {
        if(index>=code_.scriptTypes().size()) return true;
        auto value=*entity;auto *base=value.components.add(scene::ScriptBehavior::descriptor);
        if(!base) {state_.status="Limite de componentes atingido";return true;}
        auto &script=static_cast<scene::ScriptBehavior &>(*base);const auto &type=code_.scriptTypes()[index];
        script.scriptType=type.id;script.source=type.file;
        if(history_.applyValues(document_,state_.selection,value)) {
          state_.addingComponent=false;state_.expandedComponent.clear();state_.expandedNative=0;state_.expandedScript=0;state_.scriptMenu=0;state_.componentPage=~u32{0};
          state_.status="Comportamento anexado";
        }
        return true;
      }
      const auto *script=scene::scriptBehavior(entity->components.at(index));if(!script) return true;
      if(operation==widgetId(EditorWidget::ScriptFoldBase)) {
        state_.expandedScript=state_.expandedScript==script->instanceId()?0:script->instanceId();state_.expandedComponent.clear();state_.expandedNative=0;state_.nativeMenu=0;state_.scriptMenu=0;state_.meshPicker=false;state_.scriptPropertyPage=0;
      } else if(operation==widgetId(EditorWidget::ScriptMenuBase)) {
        state_.scriptMenu=state_.scriptMenu==script->instanceId()?0:script->instanceId();state_.expandedScript=0;state_.expandedComponent.clear();state_.expandedNative=0;
      } else if(operation==widgetId(EditorWidget::ScriptSourceBase)) {
        if(code_.open(files_,script->source)) {state_.code=&code_;state_.workspace=EditorWorkspace::Code;} else state_.status=code_.error();
      } else if(operation==widgetId(EditorWidget::ScriptFieldBase)) {
        const u32 field=(key&0x00ffffffu)>>8;
        for(const auto &type:code_.scriptTypes()) if(type.id==script->scriptType && field<type.properties.size()) {
          const auto &property=type.properties[field];
          if(property.valueType=="object") {
            state_.referenceInstance=script->instanceId();state_.referenceProperty=property.id;state_.referenceScript=true;
            state_.referenceQuery.clear();state_.referencePage=0;return true;
          }
          state_.editingScriptInstance=script->instanceId();state_.editingScriptEntity=entity->id;
          state_.editingScriptProperty=property.id;state_.editingScriptType=property.valueType;
        }
      } else {
        auto value=*entity;
        if(operation==widgetId(EditorWidget::ScriptRemoveBase)) {
          if(!value.components.removeInstance(script->instanceId())) return true;
          state_.scriptMenu=0;if(state_.expandedScript==script->instanceId()) state_.expandedScript=0;
        } else if(operation==widgetId(EditorWidget::ScriptEnabledBase)) {
          auto replacement=*script;replacement.enabled=!replacement.enabled;
          if(!value.components.replaceInstance(script->instanceId(),replacement)) return true;
        } else return true;
        history_.applyValues(document_,state_.selection,value);
      }
      return true;
    }
    if(key==widgetId(EditorWidget::CodeOpen)) {state_.code=&code_;state_.workspace=EditorWorkspace::Code;return true;}
    if(state_.workspace==EditorWorkspace::Code) {
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
      case EditorWidget::CodeMenu:state_.codeMenu=!state_.codeMenu;return true;
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
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::JumpCharacter)) {
    // O toque no botão alimenta o dispositivo; quem traduz isso em salto é o
    // mapa de ações, no próximo quadro de Play.
    if(isPlaying()&&!state_.playPaused) jumpPressed_=true;
    return true;
  }
  if(routing.widgetId==widgetId(EditorWidget::FilesSplitter)) {
    if(routing.dragging && !state_.filesCollapsed) {
      const float height=layout_.hierarchyPanel.height+layout_.filesPanel.height;
      if(height>0) state_.filePanelRatio=std::clamp(state_.filePanelRatio-routing.stepDelta.y/height,.28f,.58f);
    }
    return true;
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
  if(routing.tapped && state_.files && !history_.isOpen() && !isPlaying()) {
    const auto key=routing.widgetId;
    if(key==widgetId(EditorWidget::FilesCollapse)) {state_.filesCollapsed=!state_.filesCollapsed;return true;}
    const u32 base=widgetId(EditorWidget::FileRowBase);
    if(key==widgetId(EditorWidget::ImportModel)) {state_.modelImportRequested=true;state_.codeFiles=false;return true;}
    if(key==widgetId(EditorWidget::AssetInstantiate)) {
      const auto *record=assets_.findByPath(state_.selectedFile);
      ModelImportReport report;
      if(!record) setImportStatus("Registre o recurso com Reimportar antes de instanciar.",EditorConsoleSeverity::Warning);
      else if(!instantiateModel(record->guid,report)) setImportStatus(report.diagnostic,EditorConsoleSeverity::Error);
      else setImportStatus("Instância criada: "+std::to_string(report.objects)+" objetos. Desfazer remove somente esta instância.");
      return true;
    }
    if(key==widgetId(EditorWidget::AssetReimport)) {reimportPath_=state_.selectedFile;return true;}
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
      // Escolher e a acao mais barata do toque, e vale para pasta e arquivo:
      // as acoes de recurso precisam de um alvo, e abrir um arquivo nem sempre
      // e possivel.
      state_.selectedFile=entry.relativePath;
      state_.pendingResourceDelete.clear();
      if(entry.directory) files_.toggle(key-base);
      else if(entry.name.ends_with(".cs") || entry.name.ends_with(".json") || entry.name.ends_with(".md")) {
        state_.code=&code_;
        if(code_.open(files_,entry.relativePath)) {state_.workspace=EditorWorkspace::Code;state_.codeFiles=false;}
        else state_.status=code_.error();
      }
      else if(entry.name.ends_with(".aescene"))
        requestedScenePath_=files_.resolveFile(entry.relativePath);
      else state_.status=entry.name.ends_with(".glb")?"Recurso GLB · Instanciar adiciona à cena; Reimportar atualiza a fonte":"Arquivo de origem";
      return true;
    }
  }
  if(state_.draggingAsset && event.pointerId!=assetPointer_) return true;
  if(state_.draggingEntity && event.pointerId!=hierarchyPointer_) return true;
  if(isPlaying() && routing.target==UiPointerTarget::Viewport) {
    const auto *controlled=document_.find(state_.selection);
    const auto view=sceneCameraPose();const auto *cameraEntity=document_.find(view.entity);
    const bool canLook=cameraEntity&&cameraLook(*cameraEntity);
    if(((!controlled || !characterComponent(*controlled))&&!canLook) || state_.playPaused) return true;
    const float x=event.position.x-layout_.viewport.x,y=event.position.y-layout_.viewport.y;
    if(event.phase==UiPointerPhase::Down) playTouches_.pointerDown(event.pointerId,x,y,layout_.viewport.width,layout_.viewport.height);
    else if(event.phase==UiPointerPhase::Move) playTouches_.pointerMove(event.pointerId,x,y,layout_.viewport.width,layout_.viewport.height);
    else if(event.phase==UiPointerPhase::Up) playTouches_.pointerUp(event.pointerId);
    else playTouches_.cancel();
    return true;
  }
  if(routing.tapped) for(u32 i=0;i<editorCreationCatalog.size();++i)
    if(routing.widgetId==widgetId(editorCreationCatalog[i].action) && !creationAvailable(state_,i)) return true;
  if(routing.tapped && state_.creationMenu) {
    const auto key=routing.widgetId;
    if(key>=widgetId(EditorWidget::CreationCategoryBase)&&key<widgetId(EditorWidget::CreationCategoryBase)+4) {
      state_.creationCategory=key-widgetId(EditorWidget::CreationCategoryBase);state_.creationPage=0;state_.creationSearch[0]=0;
      for(u32 i=0;i<editorCreationCatalog.size();++i) if(creationAvailable(state_,i) && editorCreationCatalog[i].category==state_.creationCategory) {state_.creationSelection=i;break;}
      return true;
    }
    if(key>=widgetId(EditorWidget::CreationRowBase)&&key<widgetId(EditorWidget::CreationRowBase)+editorCreationCatalog.size()) {
      state_.creationSelection=key-widgetId(EditorWidget::CreationRowBase);return true;
    }
  }
  if (state_.renameEntity != kInvalidEntity || state_.editingHierarchySearch || state_.editingCreationSearch || state_.editingComponentSearch || state_.editingMeshSearch || state_.editingReferenceSearch) {
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
      if (key == widgetId(EditorWidget::NumericCancel)) completeTextEdit(pendingTextEdit(),{},false);
      else if (key == widgetId(EditorWidget::NumericClear)) state_.numericText[0] = 0;
      else if (key == widgetId(EditorWidget::NumericBackspace)) {
        const auto n=std::strlen(state_.numericText);if(n) state_.numericText[n-1]=0;
      } else if (key >= keyBase && key < keyBase+12) {
        if(state_.numericReplace) {state_.numericText[0]=0;state_.numericReplace=false;}
        const auto n=std::strlen(state_.numericText);
        if(n+1<sizeof(state_.numericText)) {
          state_.numericText[n]="123456789.0-"[key-keyBase];state_.numericText[n+1]=0;
        }
      } else if (key == widgetId(EditorWidget::NumericApply)) {
        completeTextEdit(pendingTextEdit(),state_.numericText,true);
      }
    }
    return true;
  }
  if(routing.tapped && (routing.widgetId==widgetId(EditorWidget::CreateGround) || routing.widgetId==widgetId(EditorWidget::CreateCube))) {
    const bool ground=routing.widgetId==widgetId(EditorWidget::CreateGround);
    for(u32 i=0;i<mapScene_.assetCount();++i) if(mapScene_.materialFlagsForAsset(i)&renderer::BoxAuthoringResource) {
      const float position[]{camera_.target[0],ground?-.1f:.5f,camera_.target[2]};
      EditorAssetInstantiation options;options.name=ground?"Chão":"Cubo";
      if(ground) {options.scale[0]=20;options.scale[1]=.2f;options.scale[2]=20;}
      if(instantiateAsset(i,document_.root(),position,&options)) {state_.creationMenu=false;frameSelection();}
      break;
    }
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::ImportModel)) {
    // O editor não conhece Android nem o seletor de arquivos: levanta o pedido
    // e quem tem o sistema na mão abre o diálogo e devolve os bytes.
    state_.modelImportRequested=true;state_.creationMenu=false;
    state_.status="Escolha um arquivo .glb";
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::CreateCamera)) {
    history_.begin("Criar câmera");
    const auto id=history_.createEntity(document_,document_.root(),EditorEntityKind::Camera,"Câmera");
    if(id) {
      auto value=*document_.find(id);editCamera(value);editorCameraPosition(camera_,value.transform.position);
      value.transform.rotationDegrees[0]=camera_.pitch*57.2957795f;
      value.transform.rotationDegrees[1]=camera_.yaw*57.2957795f;
      history_.applyValues(document_,id,value);setSelection(id);
    }
    history_.end();state_.creationMenu=false;return true;
  }
  if(routing.tapped && (routing.widgetId==widgetId(EditorWidget::CreateFiniteWater) ||
                        routing.widgetId==widgetId(EditorWidget::CreateOceanWater))) {
    state_.creationMenu=false;
    createWaterSurface(routing.widgetId==widgetId(EditorWidget::CreateOceanWater));
    return true;
  }
  if(routing.tapped && (routing.widgetId==widgetId(EditorWidget::CreateRiverWater) || routing.widgetId==widgetId(EditorWidget::CreateBuoyantBox))) {
    const bool box=routing.widgetId==widgetId(EditorWidget::CreateBuoyantBox);
    for(u32 i=0;i<mapScene_.assetCount();++i) if(mapScene_.materialFlagsForAsset(i)&(box?renderer::BoxAuthoringResource:renderer::WaterRouteResource)) {
      const float position[]{camera_.target[0],box?3.0f:0.0f,camera_.target[2]};
      if(instantiateAsset(i,document_.root(),position)) {
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
    state_.entityMenu=false;
    if(result.status!=EditorActionStatus::Applied) state_.status="Operação indisponível";
    return true;
  }
  const EditorPointerOutcome outcome =
      applyEditorPointer(state_, layout_, routing, document_, history_);
  if (outcome.requestPlay) { playRequested_ = true; cancelPointers(); }
  return outcome.consumed;
}

void EditorSession::cancelPointers() {
  playTouches_.cancel();
  state_.draggingAsset=false;
  state_.draggingEntity=kInvalidEntity;
  router_.cancelAllPointers();
  viewportPointers_.clear();
  pinchDistance_ = 0.0f;
  if (gizmoTransactionOpen_ || fieldWidget_ != 0) history_.end();
  fieldWidget_ = 0;
  gizmoTransactionOpen_ = false;
  gizmoDrag_ = EditorGizmoDrag{};
  state_.activeGizmoAxis = EditorGizmoHandle::None;
}

void EditorSession::advanceClock(float wallSeconds) noexcept {
  if(!isPlaying() && playScene_.active()) playScene_.stop();
  if (!std::isfinite(wallSeconds)) return;
  if (!clockPrimed_) {
    lastWallSeconds_ = wallSeconds;
    clockPrimed_ = true;
    return;
  }
  const float delta = wallSeconds - lastWallSeconds_;
  lastWallSeconds_ = wallSeconds;
  // Passo negativo ou absurdo é o relógio de parede saltando (retomada do app,
  // mudança de fonte de tempo). Descartar é melhor do que teleportar a
  // simulação para um futuro que ninguém viu acontecer.
  if (!isPlaying() || state_.playPaused || delta <= 0.0f || delta > 1.0f) return;
  sceneTime_ += delta;
}

void EditorSession::frameSelection() {
  const EditorEntity *entity = document_.find(state_.selection);
  if (entity == nullptr) return;
  if(!meshAsset(*entity)) {frameSubtree(entity->id);return;}
  float center[3]{entity->transform.position[0],entity->transform.position[1],entity->transform.position[2]};
  float radius=0;
  if(!mapScene_.bounds(document_, entity->id, center, radius)) {
    float world[16];if(editorWorldMatrix(document_,entity->id,world)) std::copy(world+12,world+15,camera_.target);
    return;
  }
  renderer::PerspectiveVisibilitySettings fit=projection_;
  if(layout_.viewport.height>0 && layout_.viewport.width<layout_.viewport.height)
    fit.verticalFieldOfViewRadians=2*std::atan(std::tan(fit.verticalFieldOfViewRadians*0.5f)*
      layout_.viewport.width/layout_.viewport.height);
  frameEditorCamera(camera_, center, std::max(radius, 0.01f), fit);
}

bool EditorSession::save(const char *path, u64 fingerprint) {
  state_.saveRequested=false;
  const bool ok=saveEditorDocument(path,document_,fingerprint);
  state_.status=ok?"Salvo":"Erro ao salvar";
  return ok;
}

bool EditorSession::load(const char *path, u64 fingerprint) {
  if(isPlaying()) return false;
  EditorDocument candidate;
  if(!loadEditorDocument(path,fingerprint,candidate)) return false;
  // Reconciliar ANTES de conferir a extração: a cena pode trazer identidades
  // cujos slots mudaram, e a checagem tem de valer para o documento que vai
  // realmente ser adotado.
  mapScene_.reconcileAssets(candidate);
  // Vínculos de importação (M08.2): a cena pode ter sido salva antes da última
  // reimportação, e objetos de cenas antigas ganham vínculo quando há prova.
  // Sem histórico: é o estado de abertura, não uma edição a desfazer.
  ImportReconcileReport reconcile;
  for(const auto &block:importedSources_) if(block.map.revision) {
    adoptLegacyImportInstances(candidate,nullptr,block.guid,block.map,reconcile);
    reconcileImportInstances(candidate,nullptr,block.guid,block.map,reconcile,
                             [this](const resources::AssetGuid &guid){return mapScene_.assetSlot(guid);});
  }
  if(reconcile.changed()) mapScene_.reconcileAssets(candidate);
  std::vector<renderer::MapDrawState> check;
  if(!mapScene_.extract(candidate,check)) return false;
  mapScene_.hydrateMaterials(candidate);
  cancelPointers();document_=std::move(candidate);history_.clear();
  sceneEpoch_=nextSceneEpoch();
  reportImportReconcile(reconcile,"Cena aberta");
  state_.selection=kInvalidEntity;state_.status="Cena restaurada";
  state_.collapsedEntities.clear();state_.hierarchyScroll=0;state_.renameEntity=kInvalidEntity;
  return true;
}

EditorSession::ImportedLibrary EditorSession::flattenSources(const std::vector<ImportedSource> &sources) {
  ImportedLibrary library;
  for(const auto &source:sources) {
    const auto vertexBase=static_cast<u32>(library.vertices.size()/renderer::MapVertexStride);
    const auto indexBase=static_cast<u32>(library.indices.size());
    const auto materialBase=static_cast<u32>(library.materials.size());
    library.vertices.insert(library.vertices.end(),source.vertices.begin(),source.vertices.end());
    library.indices.insert(library.indices.end(),source.indices.begin(),source.indices.end());
    // Cada fonte numera as próprias texturas a partir de zero; na biblioteca
    // achatada, os índices dos materiais andam junto com o bloco.
    const auto textureBase=static_cast<u32>(library.textures.size());
    for(auto material:source.materials) {
      for(auto &texture:material.textureIndices) if(texture!=renderer::InvalidMapTexture) texture+=textureBase;
      library.materials.push_back(material);
    }
    library.textures.insert(library.textures.end(),source.textures.begin(),source.textures.end());
    for(const auto &draw:source.draws) {
      auto moved=draw;
      moved.firstIndex+=indexBase;moved.vertexOffset+=vertexBase;moved.materialIndex+=materialBase;
      library.draws.push_back(moved);
    }
    library.identities.insert(library.identities.end(),source.identities.begin(),source.identities.end());
    library.names.insert(library.names.end(),source.names.begin(),source.names.end());
    // Pivô na origem do nó: a geometria importada já vive no espaço dele.
    for(usize i=0;i<source.draws.size();++i) {library.pivots.push_back(0);library.pivots.push_back(0);library.pivots.push_back(0);}
  }
  return library;
}

// Sobe a biblioteca para o consumidor gráfico e faz o editor adotar o pacote
// que voltou. Um caminho só, usado pela importação e pela reidratação: eram
// duas cópias desta sequência que deixavam uma delas para trás.
bool EditorSession::publishAndAdopt(const ImportedLibrary &library, std::string &diagnostic,
                                    usize *outPrimitives) {
  if(!publishGeometry_) { diagnostic="Este ambiente não publica geometria importada."; return false; }
  PublishedGeometry published;
  if(!publishGeometry_(library.vertices,library.indices,library.draws,library.materials,library.textures,published)) {
    diagnostic="O consumidor gráfico recusou a geometria importada.";return false;
  }
  // O pacote publicado é primitivas internas + biblioteca, nessa ordem. As
  // primitivas mantêm a identidade derivada da impressão digital.
  if(published.draws.size()<library.draws.size()) { diagnostic="Pacote publicado inconsistente."; return false; }
  const auto primitives=published.draws.size()-library.draws.size();
  if(outPrimitives) *outPrimitives=primitives;
  std::vector<resources::AssetGuid> identities(primitives);
  identities.insert(identities.end(),library.identities.begin(),library.identities.end());
  // Pivô: as primitivas internas mantêm a convenção do pacote (centro dos
  // limites), a geometria importada usa a origem do nó.
  std::vector<float> pivots;
  pivots.reserve(published.draws.size()*3);
  for(usize i=0;i<primitives;++i)
    for(u32 axis=0;axis<3;++axis) pivots.push_back(published.draws[i].boundsCenter[axis]);
  pivots.insert(pivots.end(),library.pivots.begin(),library.pivots.end());
  if(pivots.size()!=published.draws.size()*3) { diagnostic="Pacote publicado inconsistente."; return false; }
  cancelPointers();
  if(!mapScene_.adoptPackage(document_,published.draws,published.materials,published.vertices,
                             published.indices,identities,packageFingerprint_,pivots)) {
    diagnostic="O editor recusou o pacote publicado.";return false;
  }
  return true;
}

// Quantos objetos da cena usam este recurso. E a pergunta que apagar precisa
// responder ANTES de apagar.
u32 EditorSession::sceneUsersOf(const resources::AssetGuid &guid) const {
  u32 users=0;
  std::vector<EditorEntityId> subtree;
  document_.collectSubtree(document_.root(),subtree);
  for(const auto id:subtree) {
    const auto *entity=document_.find(id);
    if(!entity) continue;
    const auto *render=meshRenderer(*entity);
    // Um slot usa o recurso pela malha ou pelo material do projeto.
    if(render) for(u32 slot=0;slot<render->slotCount();++slot)
      if(render->slotAsset(slot)==guid || render->slotMaterialAsset(slot)==guid) {++users;break;}
  }
  return users;
}

bool EditorSession::moveResource(const std::string &relative,const std::string &destination,
                                 ResourceChangeReport &report) {
  report={};
  if(relative.empty() || destination.empty() || relative==destination) {
    report.diagnostic="Origem e destino precisam ser caminhos diferentes.";return false;
  }
  // O registro primeiro, porque ele e o unico que sabe dizer "nao" sem deixar
  // rastro. Se o arquivo fosse antes, uma colisao de caminho descobriria o
  // problema com o arquivo ja no lugar novo.
  const int retargeted=assets_.retargetPrefix(relative,destination);
  if(retargeted<0) {
    report.diagnostic="Ja existe um recurso registrado nesse caminho.";return false;
  }
  if(!files_.movePath(relative,destination)) {
    // Desfaz o reapontamento: o disco nao mudou, entao o registro tambem nao
    // pode ter mudado.
    assets_.retargetPrefix(destination,relative);
    report.diagnostic=files_.error();
    return false;
  }
  report.retargeted=static_cast<u32>(retargeted);
  assetRegistryDirty_=true;
  // A cena NAO e tocada, de proposito: os objetos guardam o GUID, nao o
  // caminho. Renomear um arquivo nao pode quebrar um objeto.
  state_.status=report.retargeted>0
      ? std::to_string(report.retargeted)+" recurso(s) reapontado(s)"
      : "Movido";
  return true;
}

bool EditorSession::deleteResource(const std::string &relative,bool force,
                                   ResourceChangeReport &report) {
  report={};
  if(relative.empty()) { report.diagnostic="Caminho vazio."; return false; }
  // Tudo que vive sob este caminho: apagar uma pasta apaga os recursos dela.
  std::vector<resources::AssetGuid> doomed;
  for(const auto &record:assets_.records()) {
    const auto &path=record.path;
    if(path.size()<relative.size() || path.compare(0,relative.size(),relative)!=0) continue;
    if(path.size()!=relative.size() && path[relative.size()]!='/') continue;
    doomed.push_back(record.guid);
  }
  for(const auto &guid:doomed) {
    report.sceneUsers+=sceneUsersOf(guid);
    // A cena nao aponta para o ARQUIVO, e sim para cada malha que saiu dele.
    // Um GLB de cinco nos vira cinco identidades, e sao elas que os objetos
    // guardam. Contar so o recurso da fonte diria "ninguem usa" com a cena
    // inteira montada em cima dele.
    for(const auto &source:importedSources_)
      if(source.guid==guid)
        for(const auto &identity:source.identities) report.sceneUsers+=sceneUsersOf(identity);
    for(const auto &dependent:assets_.dependents(guid)) {
      bool alsoDoomed=false;
      for(const auto &other:doomed) if(other==dependent) {alsoDoomed=true;break;}
      if(!alsoDoomed) ++report.registryDependents;
    }
  }
  if(!force && (report.sceneUsers>0 || report.registryDependents>0)) {
    report.diagnostic="Em uso: "+std::to_string(report.sceneUsers)+" objeto(s) da cena e "+
        std::to_string(report.registryDependents)+" recurso(s) dependem deste arquivo.";
    return false;
  }
  if(!files_.removePath(relative)) { report.diagnostic=files_.error(); return false; }

  // O registro e a biblioteca so mudam DEPOIS que o arquivo saiu: ate aqui o
  // projeto ainda podia ser recuperado do disco.
  for(const auto &guid:doomed) {
    for(auto source=importedSources_.begin();source!=importedSources_.end();++source)
      if(source->guid==guid) { importedSources_.erase(source); break; }
    removeImportMapFile(guid);
    // Material apagado: os slots que o usavam voltam ao da fonte, visivelmente
    // marcados como referência sem arquivo no inspetor.
    std::erase_if(materials_,[&](const auto &material){return material.guid==guid;});
    assets_.remove(guid);
    ++report.retargeted;
  }
  assetRegistryDirty_=true;
  if(!doomed.empty()) publishMaterialLibrary();
  std::string diagnostic;
  if(!doomed.empty() && !republishGeometry(diagnostic)) {
    report.diagnostic=diagnostic;
    return false;
  }
  // Objeto que apontava para o recurso apagado fica SEM malha, visivelmente. A
  // reconciliacao ja faz isso: identidade ausente vira slot zero, em vez de
  // apontar para a malha que por acaso ocupar o indice antigo.
  mapScene_.reconcileAssets(document_);
  state_.status=report.sceneUsers>0
      ? std::to_string(report.sceneUsers)+" objeto(s) ficaram sem malha"
      : "Apagado";
  return true;
}

bool EditorSession::republishGeometry(std::string &diagnostic) {
  // Sem geometria importada não há o que reidratar: a biblioteca do consumidor
  // novo já são as mesmas primitivas internas, com a mesma impressão digital.
  if(importedSources_.empty()) return true;
  return publishAndAdopt(flattenSources(importedSources_),diagnostic);
}

bool EditorSession::importModel(std::span<const u8> bytes, std::string_view sourceName,
                                const resources::GltfImportProgress &progress, ModelImportReport &report) {
  report = {};
  if(isPlaying()) { report.diagnostic="Pare a execução antes de importar."; return false; }
  if(!publishGeometry_) { report.diagnostic="Este ambiente não publica geometria importada."; return false; }
  if(sourceName.empty()) { report.diagnostic="Nome de arquivo vazio."; return false; }

  resources::GltfImport model;
  if(!resources::importGlb(bytes,{},progress,model)) {
    report.diagnostic=model.diagnostic;report.cancelled=model.cancelled;return false;
  }
  if(!publishModel(model,Sha256::hex(bytes),sourceName,report)) return false;
  // Reabertura: o mapa acompanha a fonte no projeto. Mesmo conteúdo produz o
  // mesmo mapa, e nada é regravado; um mapa novo (fonte trocada fora do editor,
  // ou projeto anterior ao mapa) é gravado para a próxima revisão partir dele.
  persistImportMap(report.source);
  // Compatibility entry point for existing loaders; already registered sources
  // only rehydrate geometry. Interactive import uses resource-only publication.
  if(!report.reimported) {
    ModelImportReport instance;
    // Preserve scene shape for the legacy API; interactive resource instances
    // use a movable wrapper when the source scene contains independent roots.
    if(!instantiateModel(report.source,instance,false)) {report.diagnostic=instance.diagnostic;return false;}
    report.objects=instance.objects;report.groups=instance.groups;
  }
  return true;
}

bool EditorSession::publishModel(const resources::GltfImport &model, std::string_view hash,
                                std::string_view sourceName, ModelImportReport &report,
                                resources::ImportAmbiguityPolicy policy) {
  report={};
  if(isPlaying() || history_.isOpen()) {report.diagnostic="Finalize a edição antes de publicar o recurso.";return false;}
  if(model.draws.empty() || model.nodes.empty()) {report.diagnostic="Modelo sem geometria utilizável.";return false;}
  const float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  for(usize n=0;n<model.nodes.size();++n) {
    EditorTransform local;
    if(model.nodes[n].parent>=static_cast<i32>(n) || model.nodes[n].parent < -1 ||
       !runtime::localTransformForWorld(model.nodes[n].localMatrix,identity,local)) {
      report.diagnostic="Hierarquia ou matriz local incompatível com TRS; fonte anterior preservada.";return false;
    }
  }
  if(model.drawNodes.size()!=model.draws.size()) {report.diagnostic="Mapa de desenhos incompleto.";return false;}
  for(const auto node:model.drawNodes) if(node>=model.nodes.size()) {report.diagnostic="Desenho sem nó.";return false;}
  report.skippedTextures=model.skippedTextures;
  report.skippedAnimations=model.skippedAnimations;
  report.skippedSkins=model.skippedSkins;

  // Identidade da FONTE: derivada do caminho dentro do projeto, não do conteúdo.
  // Reimportar o arquivo editado precisa cair no mesmo recurso; é o hash que
  // muda, não a identidade.
  const std::string path(sourceName);
  const auto *existing=assets_.findByPath(path);
  const resources::AssetGuid source=existing?existing->guid:resources::assetGuidFromSeed("fonte:"+path);
  report.source=source;

  // Mapa de nós (M08.2): a revisão anterior, em memória ou no projeto, é o ponto
  // de partida da correspondência. Sem ela as identidades nascem da fonte e das
  // chaves legadas — as mesmas que cenas anteriores ao mapa já gravaram.
  resources::ImportNodeMap previousNodeMap;
  const bool hasPrevious=existing && previousImportMap(source,previousNodeMap);
  resources::ImportNodeMap nodeMap;
  std::string mapDiagnostic;
  if(!resources::buildImportNodeMap(model,source,hash,hasPrevious?&previousNodeMap:nullptr,policy,nodeMap,report.match,mapDiagnostic)) {
    report.diagnostic=mapDiagnostic.empty()?"Mapa de nós recusado; fonte anterior preservada.":mapDiagnostic;
    return false;
  }

  ImportedSource block;
  block.guid=source;
  block.vertices.assign(model.vertices.begin(),model.vertices.end());
  block.indices.assign(model.indices.begin(),model.indices.end());
  block.materials.assign(model.materials.begin(),model.materials.end());
  block.draws.assign(model.draws.begin(),model.draws.end());
  block.nodes=model.nodes;
  block.drawNodes=model.drawNodes;
  std::vector<usize> primitiveOfNode(model.nodes.size(),0);
  for(usize i=0;i<model.draws.size();++i) {
    // A identidade de cada malha vem do mapa: um nó reconhecido na revisão nova
    // mantém a identidade das suas primitivas, mesmo renomeado ou movido. O mapa
    // já recusa identidades repetidas.
    const auto node=model.drawNodes[i];
    const auto primitive=primitiveOfNode[node]++;
    if(primitive>=nodeMap.nodes[node].draws.size()) {report.diagnostic="Mapa de nós incompleto; fonte anterior preservada.";return false;}
    block.identities.push_back(nodeMap.nodes[node].draws[primitive]);
    block.names.push_back(i<model.names.size()?model.names[i]:std::string("Malha"));
  }
  block.map=std::move(nodeMap);
  block.materialNames=model.materialNames;
  block.textures=model.textures;

  // Candidato completo antes de publicar: a versão anterior continua valendo
  // até a nova estar inteira na GPU e aceita pelo editor.
  auto candidateSources=importedSources_;
  usize slot=candidateSources.size();
  for(usize i=0;i<candidateSources.size();++i) if(candidateSources[i].guid==source) {slot=i;break;}
  // Reimportação é decidida pelo REGISTRO, não pela biblioteca deste processo.
  // Ao reabrir o projeto, a fonte está no registro e a biblioteca está vazia --
  // se a decisão fosse pela biblioteca, cada abertura criaria os objetos de
  // novo e a cena cresceria sozinha a cada vez.
  report.reimported=existing!=nullptr;
  // Onde o bloco entra é decidido pela BIBLIOTECA (substituir o que já está
  // carregado); se objetos são criados, pelo REGISTRO. São perguntas diferentes:
  // ao reabrir o projeto a fonte está no registro e não na biblioteca.
  if(slot<candidateSources.size()) candidateSources[slot]=std::move(block);
  else candidateSources.push_back(std::move(block));
  const auto candidate=flattenSources(candidateSources);

  resources::AssetRecord record=existing?*existing:resources::AssetRecord{};
  record.guid=source;record.type=resources::AssetType::Mesh;record.path=path;record.source=path;
  record.contentHash=std::string(hash);record.importerVersion=1;record.importerParameters="glb";
  record.derived.clear();
  auto nextAssets=assets_;
  const bool registered=existing
      ?nextAssets.publishImport(source,record.contentHash,1,"glb",{},{})
      :nextAssets.add(record);
  if(!registered) {report.diagnostic="Registro recusou o recurso; publicação cancelada.";return false;}
  const auto previousDocument=document_;
  const auto previousMap=mapScene_;
  if(!publishAndAdopt(candidate,report.diagnostic)) {
    std::string rollback;
    if(!publishAndAdopt(flattenSources(importedSources_),rollback))
      report.diagnostic+=" Falha ao restaurar a GPU: "+rollback;
    document_=previousDocument;mapScene_=previousMap;return false;
  }
  importedSources_=std::move(candidateSources);
  assets_=std::move(nextAssets);assetRegistryDirty_=true;
  state_.status=report.reimported?"Recurso reimportado; instâncias preservadas":"Recurso registrado; pronto para instanciar";
  if(report.reimported) if(const auto *published=importNodeMap(source)) {
    // Um passo de desfazer para tudo o que a reimportação fez na cena: vínculo
    // de objetos legados comprovados e reconciliação das instâncias.
    history_.begin("Reimportar recurso");
    // Objetos legados são provados contra a revisão que a cena USOU — a
    // anterior. Provar contra a nova faria os nós recém-chegados parecerem
    // conhecidos e apagados pelo usuário, e eles nunca entrariam.
    adoptLegacyImportInstances(document_,&history_,source,hasPrevious?previousNodeMap:*published,report.reconcile);
    reconcileImportInstances(document_,&history_,source,*published,report.reconcile,
                             [this](const resources::AssetGuid &guid){return mapScene_.assetSlot(guid);});
    history_.end();
    if(report.reconcile.changed()) mapScene_.hydrateMaterials(document_);
    reportImportReconcile(report.reconcile,"Reimportação");
  }
  return true;
}

bool EditorSession::instantiateModel(resources::AssetGuid source, ModelImportReport &report,bool wrapMultipleRoots) {
  report={};report.source=source;
  if(isPlaying() || history_.isOpen()) {report.diagnostic="Finalize a edição antes de instanciar.";return false;}
  usize slot=0,firstNew=0,total=0;
  for(;slot<importedSources_.size() && importedSources_[slot].guid!=source;++slot)
    firstNew+=importedSources_[slot].draws.size();
  if(slot==importedSources_.size()) {report.diagnostic="Recurso não carregado; reimporte a fonte.";return false;}
  for(const auto &block:importedSources_) total+=block.draws.size();
  if(mapScene_.assetCount()<total) {report.diagnostic="Biblioteca gráfica incompleta.";return false;}
  const usize primitives=mapScene_.assetCount()-total;
  const auto newDrawCount=importedSources_[slot].draws.size();
  const auto previousDocument=document_;const auto previousHistory=history_;
  std::vector<EditorEntityId> newRoots;
  const auto rollback=[&](const char *message) {
    document_=previousDocument;history_=previousHistory;report.objects=0;report.groups=0;
    report.diagnostic=message;return false;
  };
  // Reimportar NÃO cria objetos: os que já existem apontam para as mesmas
  // identidades e acabaram de ser reconciliados com a geometria nova.
  {
    // A árvore do arquivo vira árvore de objetos. Um nó por objeto, INCLUSIVE
    // os sem malha: é o grupo vazio que segura a porta no lugar quando a
    // carroceria se move, e descartá-lo é exatamente o que achata a hierarquia.
    const auto &tree=importedSources_[slot];
    // Vínculo de cada objeto com o nó da fonte (M08.2). A identidade da
    // instância precisa ser única entre sessões: relógio e contador.
    const auto &nodeMap=tree.map;
    const bool linkable=nodeMap.revision && nodeMap.nodes.size()==tree.nodes.size();
    const auto instance=resources::assetGuidFromSeed("instancia:"+source.text()+":"+
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));
    history_.begin("Instanciar recurso");
    auto instanceParent=document_.root();
    const auto rootCount=std::count_if(tree.nodes.begin(),tree.nodes.end(),[](const auto &node) {return node.parent<0;});
    if(wrapMultipleRoots && rootCount>1) {
      std::string name="Modelo importado";
      if(const auto *record=assets_.find(source)) {
        name=record->path;const auto slash=name.find_last_of("/\\");if(slash!=std::string::npos) name.erase(0,slash+1);
        const auto dot=name.find_last_of('.');if(dot!=std::string::npos) name.resize(dot);
      }
      instanceParent=history_.createEntity(document_,document_.root(),EditorEntityKind::Folder,name.c_str());
      if(!instanceParent) return rollback("Não foi possível criar o grupo da importação.");
      if(linkable) {
        auto wrapper=*document_.find(instanceParent);
        if(auto *link=scene::editImportLink(wrapper.components)) {
          link->source=source;link->instance=instance;link->root=true;link->revision=nodeMap.revision;link->baseName=wrapper.name;
          history_.applyValues(document_,instanceParent,wrapper);
        }
      }
      newRoots.push_back(instanceParent);++report.objects;++report.groups;
    }
    std::vector<EditorEntityId> created(tree.nodes.size(),kInvalidEntity);
    const float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    for(usize n=0;n<tree.nodes.size();++n) {
      const auto &node=tree.nodes[n];
      const auto parent=node.parent>=0 && static_cast<usize>(node.parent)<created.size()
          ? created[static_cast<usize>(node.parent)] : instanceParent;
      if(parent==kInvalidEntity) { return rollback("Hierarquia do modelo fora de ordem."); }
      const auto id=history_.createEntity(document_,parent,EditorEntityKind::Mesh,
                                          node.name.empty()?"Objeto":node.name.c_str());
      if(!id) { return rollback("Não foi possível criar os objetos do modelo."); }
      created[n]=id;
      if(node.parent<0 && instanceParent==document_.root()) newRoots.push_back(id);
      auto value=*document_.find(id);
      // A pose local precisa caber em TRS; nunca substituir shear por identidade.
      EditorTransform local;
      if(runtime::localTransformForWorld(node.localMatrix,identity,local)) value.transform=local;
      else return rollback("Matriz do nó não pode ser instanciada sem perder sua transformação.");
      if(linkable) if(auto *link=scene::editImportLink(value.components)) {
        link->source=source;link->instance=instance;
        setImportLinkBase(*link,nodeMap.nodes[n],local,-1,nodeMap.revision);
        link->root=node.parent<0 && instanceParent==document_.root();
      }
      history_.applyValues(document_,id,value);
      ++report.objects;
    }
    // Várias primitivas no mesmo nó são SLOTS do mesmo objeto (Entrega 2): um
    // nó autoral continua sendo um objeto, sem filhos inventados por desenho.
    std::vector<u32> placed(tree.nodes.size(),0);
    for(usize i=0;i<newDrawCount;++i) {
      const auto drawSlot=static_cast<u32>(primitives+firstNew+i)+1;
      const auto nodeIndex=i<tree.drawNodes.size()?tree.drawNodes[i]:0u;
      if(nodeIndex>=created.size()) { return rollback("Desenho sem nó no modelo."); }
      const auto target=created[nodeIndex];
      const u32 slot=placed[nodeIndex]++;
      if(slot>scene::MeshRenderer::MaximumSubmeshes) { return rollback("Primitivas demais num nó para os slots de material."); }
      auto value=*document_.find(target);
      auto *render=editMeshRenderer(value);
      if(!render) { return rollback("Não foi possível criar a malha do objeto."); }
      if(slot) render->submeshes.resize(slot);
      *render->editSlotMesh(slot)=drawSlot;
      *render->editSlotAsset(slot)=tree.identities[i];
      *render->editSlotMaterial(slot)=mapScene_.materialForAsset(drawSlot-1);
      history_.applyValues(document_,target,value);
    }
    for(usize n=0;n<tree.nodes.size();++n) if(!placed[n]) ++report.groups;
    history_.end();
  }

  state_.status="Recurso instanciado: "+std::to_string(report.objects)+" objetos";
  // The newly imported object must be discoverable immediately. Fit only this
  // instance (all its roots), preserving authored scale and source transforms.
  if(!newRoots.empty()) {
    state_.workspace=EditorWorkspace::Scene;setSelection(newRoots.front());
    float low[3]{},high[3]{};bool first=true;
    std::vector<EditorEntityId> importedIds;
    for(const auto root:newRoots) {std::vector<EditorEntityId> subtree;document_.collectSubtree(root,subtree);importedIds.insert(importedIds.end(),subtree.begin(),subtree.end());}
    for(const auto id:importedIds) {
      float center[3],radius;
      if(!mapScene_.bounds(document_,id,center,radius)) continue;
      for(u32 axis=0;axis<3;++axis) {
        low[axis]=first?center[axis]-radius:std::min(low[axis],center[axis]-radius);
        high[axis]=first?center[axis]+radius:std::max(high[axis],center[axis]+radius);
      }
      first=false;
    }
    if(!first) {
      float center[3],radius2=0;
      for(u32 axis=0;axis<3;++axis) {center[axis]=(low[axis]+high[axis])*.5f;radius2+=(high[axis]-center[axis])*(high[axis]-center[axis]);}
      auto fit=projection_;
      if(layout_.viewport.height>0 && layout_.viewport.width<layout_.viewport.height)
        fit.verticalFieldOfViewRadians=2*std::atan(std::tan(fit.verticalFieldOfViewRadians*.5f)*layout_.viewport.width/layout_.viewport.height);
      frameEditorCamera(camera_,center,std::max(.01f,std::sqrt(radius2)),fit);
    }
  }
  return true;
}

bool EditorSession::commitModelImport(std::span<const u8> bytes,const resources::GltfImport &model,
                                      const std::string &path,const std::string &expectedHash,ModelImportReport &report,
                                      resources::ImportAmbiguityPolicy policy) {
  report={};
  if(isPlaying()) {report.diagnostic="Pare a execução antes de publicar.";return false;}
  // O mapa de nós entra na MESMA transação que fonte e registro.
  const auto *known=assets_.findByPath(path);
  const auto mapSource=known?known->guid:resources::assetGuidFromSeed("fonte:"+path);
  EditorImportTransaction transaction(files_.rootPath());
  if(!transaction.begin(path,expectedHash,report.diagnostic,resources::importNodeMapPath(mapSource))) {
    if(report.diagnostic.empty()) report.diagnostic="Não foi possível preparar os backups da importação.";
    return false;
  }
  const auto previousSources=importedSources_;const auto previousAssets=assets_;
  const auto previousDocument=document_;const auto previousMap=mapScene_;const auto previousHistory=history_;
  const bool previousDirty=assetRegistryDirty_;
  bool published=publishModel(model,Sha256::hex(bytes),path,report,policy);
  const auto *nodeMap=published?importNodeMap(report.source):nullptr;
  if(published && nodeMap && transaction.commit(bytes,assets_.serialize(),nodeMap->serialize())) {
    assetRegistryDirty_=false;files_.rebuildTree();state_.selectedFile=path;
    // Reveal each ancestor so the selected resource is actually visible.
    for(usize i=0;i<files_.tree().size();++i) {
      const auto entry=files_.tree()[i];
      if(entry.directory && !entry.expanded && !entry.relativePath.empty() && path.starts_with(entry.relativePath+"/"))
        files_.toggle(static_cast<unsigned>(i));
    }
    return true;
  }
  if(published) {
    report.diagnostic="Não foi possível gravar fonte e registro; importação revertida.";
    importedSources_=previousSources;assets_=previousAssets;
    std::string rollback;
    if(!publishAndAdopt(flattenSources(importedSources_),rollback)) report.diagnostic+=" GPU: "+rollback;
    // A reconciliação entrou no histórico: cena e histórico voltam juntos.
    document_=previousDocument;mapScene_=previousMap;history_=previousHistory;assetRegistryDirty_=previousDirty;
  }
  if(!transaction.rollback()) report.diagnostic+=" Recuperação de disco pendente; backups preservados.";
  return false;
}

void EditorSession::showImportPreview(std::string path,const resources::GltfImport &model,std::string_view contentHash) {
  state_.importAmbiguities=0;state_.importAmbiguityChoice=0;
  state_.importPanel=true;state_.importReady=true;state_.importError=false;state_.importPage=0;state_.importPath=std::move(path);
  state_.importStatus=assets_.findByPath(state_.importPath)?"Atualizar recurso existente":"Registrar novo recurso";
  state_.importSummary=std::to_string(model.nodes.size())+" nós · "+std::to_string(model.draws.size())+" malhas · "+
      std::to_string(model.materials.size())+" materiais";
  state_.importSummary+="\nSó recurso: guarda no projeto.\nImportar na cena: guarda, instancia e enquadra o modelo.";
  // Texturas (M09.1): o que entrou, o que não entrou e por quê. Ausência de
  // textura nunca pode parecer material final correto.
  if(!model.textures.empty())
    state_.importSummary+="\nTexturas: "+std::to_string(model.textures.size())+" aplicadas ("+
      std::to_string((model.textureBytes+(u64{1}<<19))>>20)+" MB com mipmaps).";
  if(model.reducedTextures)
    state_.importSummary+="\nResolução reduzida em "+std::to_string(model.reducedTextures)+" textura(s), até "+
      std::to_string(model.residentTextureDimension)+" px, para caber no limite do aparelho.";
  if(model.skippedTextures)
    state_.importSummary+="\nTexturas não aplicadas: "+std::to_string(model.skippedTextures)+"; esses slots ficam só com os fatores.";
  for(const auto &note:model.textureNotes) state_.importSummary+="\n• "+note;
  if(model.unappliedTextureTransforms)
    state_.importSummary+="\nTransformação de UV (KHR_texture_transform) não aplicada em "+
      std::to_string(model.unappliedTextureTransforms)+" slot(s): a textura aparece sem ela.";
  if(model.unappliedOcclusion)
    state_.importSummary+="\nOclusão não aplicada em "+std::to_string(model.unappliedOcclusion)+" material(is).";
  if(model.skippedAnimations||model.skippedSkins)
    state_.importSummary+="\nNão suportado neste perfil: "+std::to_string(model.skippedAnimations)+" animações, "+
      std::to_string(model.skippedSkins)+" skins.";
  if(!model.appearanceExtensions.empty()) {
    state_.importSummary+="\nGeometria estática; aparência avançada não reproduzida:";
    for(const auto &extension:model.appearanceExtensions) state_.importSummary+="\n"+extension;
  }
  if(const auto *record=assets_.findByPath(state_.importPath)) {
    // A mesma correspondência que a publicação fará, sem publicar: o usuário
    // vê o que muda nas instâncias ANTES de aceitar.
    resources::ImportNodeMap previous,candidate;resources::ImportMatchReport match;std::string diagnostic;
    const bool hasPrevious=previousImportMap(record->guid,previous);
    resources::buildImportNodeMap(model,record->guid,contentHash,hasPrevious?&previous:nullptr,
                                  resources::ImportAmbiguityPolicy::Refuse,candidate,match,diagnostic);
    auto &summary=state_.importSummary;
    if(match.sameContent) summary+="\nMesmo conteúdo da versão publicada; nada muda nas instâncias.";
    else if(hasPrevious) {
      summary+="\nCorrespondência: "+std::to_string(match.byAuthoredId)+" por id do autor · "+std::to_string(match.byStructure)+
          " por estrutura · "+std::to_string(match.renamed)+" renomeados · "+std::to_string(match.reparented)+" com pai novo";
      if(match.ambiguities.empty())
        summary+="\nNós novos: "+std::to_string(match.added)+" · removidos da fonte: "+std::to_string(match.removed);
    } else summary+="\nSem mapa anterior: identidades derivadas das chaves da fonte.";
    summary+="\nInstâncias: alterações locais preservadas; removidos com dados locais ficam órfãos.";
    if(!match.ambiguities.empty()) {
      state_.importAmbiguities=static_cast<u32>(match.ambiguities.size());
      summary+="\nAmbíguos — escolha abaixo antes de publicar:";
      for(const auto &item:match.ambiguities)
        summary+="\n• "+(item.parent.empty()?std::string("raiz"):item.parent)+" / "+item.name+": "+
            std::to_string(item.previous)+" anteriores, "+std::to_string(item.incoming)+" novos";
    }
  }
}

bool EditorSession::importMap(std::span<const renderer::MapDrawRecord> draws, std::span<const renderer::MapMaterialRecord> materials, bool instantiate, std::span<const u8> vertices, std::span<const u32> indices, u64 packageFingerprint) {
  if(isPlaying()) return false;
  cancelPointers();
  if (!mapScene_.import(document_, draws, materials, instantiate,vertices,indices,packageFingerprint)) return false;
  packageFingerprint_=packageFingerprint;
  importedSources_.clear();
  state_.creationAvailable=3|(1u<<8);
  for(u32 i=0;i<mapScene_.assetCount();++i) {
    const auto flags=mapScene_.materialFlagsForAsset(i);
    if(flags & renderer::BoxAuthoringResource) {
      state_.creationAvailable|=(1u<<2)|(1u<<3);
      if(flags & renderer::WaterAuthoringResource) state_.creationAvailable|=1u<<7;
    }
    if(flags & renderer::WaterRouteResource) state_.creationAvailable|=1u<<6;
    if((flags & renderer::WaterAuthoringResource) && (flags & renderer::MapMaterialWater) && !(flags & renderer::WaterRouteResource))
      state_.creationAvailable|=1u<<((flags & renderer::MapMaterialWaterCameraGrid)?5:4);
  }
  if((state_.workspace==EditorWorkspace::Assets && !mapScene_.assetCount()) ||
     (state_.workspace==EditorWorkspace::Settings && !waterCreationAvailable(state_)))
    state_.workspace=EditorWorkspace::Scene;
  state_.creationCategory=0;state_.creationSelection=0;state_.creationPage=0;

  sceneEpoch_=nextSceneEpoch();
  history_.clear();
  state_.selection = kInvalidEntity;
  state_.hierarchyScroll = 0;
  state_.collapsedEntities.clear();state_.renameEntity=kInvalidEntity;
  frameAll();
  return true;
}

EditorEntityId EditorSession::instantiateAsset(u32 index, EditorEntityId parent, const float worldPosition[3], const EditorAssetInstantiation *options) {
  if(!worldPosition || !mapScene_.asset(index) || !document_.exists(parent) || history_.isOpen()) return kInvalidEntity;
  EditorEntity values;auto *render=editMeshRenderer(values);if(!render) return kInvalidEntity;render->mesh=index+1;render->material=mapScene_.materialForAsset(index);
  float parentWorld[16],world[16];EditorTransform pose;
  std::copy(worldPosition,worldPosition+3,pose.position);editorTransformMatrix(pose,world);
  if(!editorWorldMatrix(document_,parent,parentWorld) ||
     !editorLocalTransformForWorld(world,parentWorld,values.transform) || !validEditorAppearance(values)) return kInvalidEntity;
  const bool water=(mapScene_.materialFlagsForAsset(index)&renderer::MapMaterialWater)!=0;
  values.rigidBodyEnabled=(mapScene_.materialFlagsForAsset(index)&(renderer::BoxAuthoringResource|renderer::WaterAuthoringResource))==(renderer::BoxAuthoringResource|renderer::WaterAuthoringResource);
  if(!setWaterBodyFlags(values,true,(mapScene_.materialFlagsForAsset(index)&renderer::MapMaterialWaterCameraGrid)!=0)) return kInvalidEntity;
  if(mapScene_.materialFlagsForAsset(index)&renderer::WaterRouteResource) {
    auto *route=editWaterRoute(values);if(!route) return kInvalidEntity;
    route->count=3;
    route->points[0].position[2]=-12;
    route->points[1].position[0]=8;
    route->points[2].position[2]=12;
  }
  const char *role=waterRoute(values).count?"Rio":waterBody(values).infinite?"Oceano":water?"Superfície de água":values.rigidBodyEnabled?"Corpo rígido":"Malha";
  char name[64];std::snprintf(name,sizeof(name),"%s %u",role,document_.entityCount());assignEntityName(values,name);
  if(options) {
    if(!options->name.empty()) assignEntityName(values,options->name);
    std::copy(options->scale,options->scale+3,values.transform.scale);values.rigidBodyEnabled=options->rigidBody;
    if(!isTransformValid(values.transform)) return kInvalidEntity;
  }
  if(!history_.begin("Instanciar malha")) return kInvalidEntity;
  const auto id=history_.createEntity(document_,parent,water?EditorEntityKind::Water:EditorEntityKind::Mesh,values.name);
  if(id) {history_.applyValues(document_,id,values);setSelection(id);state_.status="Malha adicionada";}
  history_.end();return id;
}

EditorEntityId EditorSession::createWaterSurface(bool cameraRelative) {
  for(u32 i=0;i<mapScene_.assetCount();++i) {
    const auto flags=mapScene_.materialFlagsForAsset(i);
    if(!(flags & renderer::WaterAuthoringResource) || !(flags & renderer::MapMaterialWater) || (flags & renderer::WaterRouteResource) ||
       bool(flags & renderer::MapMaterialWaterCameraGrid)!=cameraRelative) continue;
    const float position[3]{camera_.target[0],0,camera_.target[2]};
    const auto id=instantiateAsset(i,document_.root(),position);
    if(id) {state_.workspace=EditorWorkspace::Scene;state_.status="Água criada; ondas e óptica em Configurações";
      if(cameraRelative) {std::copy(position,position+3,camera_.target);camera_.distance=40;} else frameSelection();}
    return id;
  }
  state_.status="Gerador de agua indisponivel nesta biblioteca";
  return kInvalidEntity;
}

void EditorSession::frameAll() { frameSubtree(document_.root()); }

void EditorSession::frameSubtree(EditorEntityId root) {
  buildPickCandidates();
  if (candidates_.empty()) { camera_ = EditorCamera{}; return; }
  float low[3]{}, high[3]{};
  bool first = true;
  for (const auto &candidate : candidates_) {
    if (!candidate.selectable || !document_.isDescendantOf(candidate.id,root)) continue;
    for (u32 axis = 0; axis < 3; ++axis) {
      const float a = candidate.center[axis] - candidate.radius;
      const float b = candidate.center[axis] + candidate.radius;
      low[axis] = first ? a : std::min(low[axis], a);
      high[axis] = first ? b : std::max(high[axis], b);
    }
    first = false;
  }
  if (first) return;
  float center[3]{}, radiusSquared = 0;
  for (u32 axis = 0; axis < 3; ++axis) {
    center[axis] = (low[axis] + high[axis]) * 0.5f;
    radiusSquared += (high[axis]-center[axis])*(high[axis]-center[axis]);
  }
  renderer::PerspectiveVisibilitySettings fit=projection_;
  if(layout_.viewport.height>0 && layout_.viewport.width<layout_.viewport.height)
    fit.verticalFieldOfViewRadians=2*std::atan(std::tan(fit.verticalFieldOfViewRadians*0.5f)*
      layout_.viewport.width/layout_.viewport.height);
  frameEditorCamera(camera_, center, std::max(0.01f, std::sqrt(radiusSquared)), fit);
}

void EditorSession::update() {
  refreshImportLinkView();
  refreshMaterialSlotView();
  if(const auto *selected=document_.find(state_.selection)) state_.routePoint=waterRoute(*selected).count?std::min(state_.routePoint,waterRoute(*selected).count-1):0;
  if (font_ == nullptr || icons_ == nullptr) return;
  state_.assetCount=mapScene_.assetCount();
  state_.canUndo = history_.canUndo();
  state_.canRedo = history_.canRedo();
  u32 pressed = 0;
  state_.pressedWidget = router_.pressedWidget(pressed) ? pressed : 0;

  const UiFontMetrics metrics = font_->metrics(UiFontWeight::Regular);

  // Duas passagens, e é o preço de fazer o certo. A grade e o gizmo precisam da
  // projeção, a projeção precisa do retângulo da cena, e o retângulo da cena só
  // existe depois que os painéis tomaram a largura deles. Usar o retângulo do
  // frame anterior custaria um quadro de atraso justamente quando o usuário
  // está arrastando um divisor — o momento em que ele mais olha para a borda.
  list_.begin(state_.surface, metrics);
  router_.beginFrame();
  layout_ = buildEditorScreen(state_, state_.workspace==EditorWorkspace::Code?editorCodeTheme():editorTheme(), list_, router_);
  // A rolagem persegue o cursor a cada quadro enquanto o codigo esta aberto: a
  // altura util so encolhe quando o teclado termina de subir, um ou dois
  // quadros depois do toque, e a contagem de linhas visiveis daquele instante e
  // a unica que vale.
  if(state_.editingCode && state_.platformTextInput && !state_.platformCodeView) followCodeCaret();
  const u32 maximumScroll=layout_.hierarchyRowCount>layout_.hierarchyVisibleRows
      ? layout_.hierarchyRowCount-layout_.hierarchyVisibleRows : 0;
  state_.hierarchyScroll=std::min(state_.hierarchyScroll,maximumScroll);


  // Identidade, e não a pré-rotação do display: o retângulo da vista já está no
  // espaço lógico em paisagem, e o shader da interface roda tudo uma vez no fim.
  view_ = buildEditorViewport(camera_, layout_.viewport, {}, projection_);
  state_.view = &view_;

  list_.begin(state_.surface, metrics);
  router_.beginFrame();
  layout_ = buildEditorScreen(state_, state_.workspace==EditorWorkspace::Code?editorCodeTheme():editorTheme(), list_, router_);
  // A rolagem persegue o cursor a cada quadro enquanto o codigo esta aberto: a
  // altura util so encolhe quando o teclado termina de subir, um ou dois
  // quadros depois do toque, e a contagem de linhas visiveis daquele instante e
  // a unica que vale.
  if(state_.editingCode && state_.platformTextInput && !state_.platformCodeView) followCodeCaret();

  instances_.clear();
  buildUiInstances(list_, *font_, *icons_, kMaximumInstances, instances_);
}

void EditorSession::refreshMaterialSlotView() {
  auto &view=state_.materialSlotView;view={};
  state_.projectMaterials.clear();
  for(const auto &material:materials_) state_.projectMaterials.push_back(material.name);
  const auto *entity=document_.find(state_.selection);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(!render) {state_.materialPicker=false;return;}
  view.slots=render->slotCount();
  state_.materialSlot=std::min(state_.materialSlot,view.slots-1);
  const u32 slot=state_.materialSlot;
  const auto guid=render->slotMaterialAsset(slot);
  const auto *shared=findMaterialAsset(guid);
  view.shared=shared!=nullptr;view.missing=guid.valid() && !shared;view.overridden=render->slotMaterial(slot).enabled;
  view.name=shared?shared->name:view.missing?std::string("Referência sem arquivo"):sourceMaterialName(render->slotAsset(slot));
  if(view.name.empty()) view.name="Material do pacote";
  // Sem recurso do projeto não há alcance compartilhado a mostrar.
  if(!view.shared) state_.materialShared=false;
  scene::MaterialParameters values=state_.materialShared?shared->values:mapScene_.slotMaterial(*render,slot);
  if(!state_.materialShared && !values.enabled && render->slotMesh(slot)) values=mapScene_.materialForAsset(render->slotMesh(slot)-1);
  scene::MeshRenderer probe;probe.material=values;
  for(u32 field=0;field<scene::meshRendererNumbers.size() && field<std::size(view.values);++field)
    view.values[field]=scene::meshRendererNumbers[field].read(probe);
}

void EditorSession::publishMaterialLibrary() {
  std::vector<std::pair<resources::AssetGuid,scene::MaterialParameters>> library;
  library.reserve(materials_.size());
  for(const auto &material:materials_) library.emplace_back(material.guid,material.values);
  mapScene_.setMaterialLibrary(std::move(library));
  appearanceChanged_=true;
}

void EditorSession::loadMaterialAssets() {
  materials_.clear();
  const auto root=files_.rootPath();
  if(!root.empty()) for(const auto &record:assets_.records()) {
    if(record.type!=resources::AssetType::Material) continue;
    std::filesystem::path absolute;std::vector<u8> bytes;resources::MaterialAsset material;
    // Arquivo ausente ou ilegível NÃO some em silêncio: o slot que o referencia
    // volta ao material da fonte, e o motivo fica no console.
    if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),record.path,absolute) ||
       !EditorImportTransaction::read(absolute,bytes,64u*1024u) ||
       !resources::MaterialAsset::deserialize(std::string_view(reinterpret_cast<const char *>(bytes.data()),bytes.size()),material) ||
       material.guid!=record.guid) {
      reportProblem(EditorConsoleSeverity::Warning,"Material do projeto ilegível ou ausente: "+record.path+"; slots usam o material da fonte.");
      continue;
    }
    materials_.push_back(std::move(material));
  }
  publishMaterialLibrary();
}

bool EditorSession::writeMaterialAsset(const resources::MaterialAsset &material,const std::string &path,std::string &diagnostic) {
  const auto root=files_.rootPath();
  std::filesystem::path absolute;
  if(root.empty() || !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),path,absolute)) {
    diagnostic="Materiais precisam de um projeto aberto.";return false;
  }
  std::error_code error;std::filesystem::create_directories(absolute.parent_path(),error);
  if(error || !EditorImportTransaction::writeText(absolute,material.serialize())) {
    diagnostic="Não foi possível gravar "+path+".";return false;
  }
  return true;
}

std::string EditorSession::sourceMaterialName(const resources::AssetGuid &draw) const {
  for(const auto &block:importedSources_)
    for(usize i=0;i<block.identities.size() && i<block.draws.size();++i) {
      if(block.identities[i]!=draw) continue;
      const auto index=block.draws[i].materialIndex;
      if(index<block.materialNames.size() && !block.materialNames[index].empty()) return block.materialNames[index];
      return "Material "+std::to_string(index+1);
    }
  return {};
}

resources::AssetGuid EditorSession::createMaterialFromSlot(EditorEntityId id,u32 slot,std::string &diagnostic) {
  diagnostic.clear();
  const auto *entity=document_.find(id);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(isPlaying() || history_.isOpen()) {diagnostic="Finalize a edição antes de criar material.";return {};}
  if(!render || slot>=render->slotCount()) {diagnostic="Slot de material inexistente.";return {};}
  resources::MaterialAsset material;
  material.name=sourceMaterialName(render->slotAsset(slot));
  if(material.name.empty()) material.name="Material";
  // Os valores que o usuário VÊ agora viram o recurso: substituição local, ou
  // material já compartilhado, ou o da fonte.
  material.values=mapScene_.slotMaterial(*render,slot);
  if(!material.values.enabled && render->slotMesh(slot)) material.values=mapScene_.materialForAsset(render->slotMesh(slot)-1);
  material.values.enabled=true;
  std::string stem;
  for(unsigned char c:material.name) stem.push_back(c<32 || c==127 || c=='/' || c=='\\' || c==':' || c=='"' ? '_' : static_cast<char>(c));
  if(stem.empty() || stem=="." || stem=="..") stem="Material";
  std::string path="Materiais/"+stem+".material";
  for(u32 n=2;assets_.findByPath(path) || files_.exists(path);++n) path="Materiais/"+stem+" "+std::to_string(n)+".material";
  material.guid=resources::assetGuidFromSeed("material:"+path+":"+
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));
  if(!material.valid()) {diagnostic="Valores de material inválidos.";return {};}
  resources::AssetRecord record;
  record.guid=material.guid;record.type=resources::AssetType::Material;record.path=path;
  const auto serialized=material.serialize();
  record.contentHash=Sha256::hex(std::span<const u8>(reinterpret_cast<const u8 *>(serialized.data()),serialized.size()));
  auto nextAssets=assets_;
  if(!nextAssets.add(record)) {diagnostic="O registro recusou o material.";return {};}
  if(!writeMaterialAsset(material,path,diagnostic)) return {};
  assets_=std::move(nextAssets);assetRegistryDirty_=true;
  materials_.push_back(material);
  publishMaterialLibrary();
  files_.rebuildTree();
  // O slot passa a usar o recurso; a substituição local sai, porque os valores
  // dela acabaram de virar o próprio recurso.
  auto values=*entity;auto *edit=editMeshRenderer(values);
  *edit->editSlotMaterialAsset(slot)=material.guid;
  edit->editSlotMaterial(slot)->enabled=false;
  history_.applyValues(document_,id,values);
  state_.status="Material do projeto criado: "+path;
  return material.guid;
}

bool EditorSession::assignSlotMaterial(EditorEntityId id,u32 slot,const resources::AssetGuid &material) {
  const auto *entity=document_.find(id);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(isPlaying() || history_.isOpen() || !render || slot>=render->slotCount()) return false;
  if(material.valid() && !findMaterialAsset(material)) return false;
  auto values=*entity;
  *editMeshRenderer(values)->editSlotMaterialAsset(slot)=material;
  return history_.applyValues(document_,id,values);
}

bool EditorSession::setSlotMaterialValue(EditorEntityId id,u32 slot,MaterialScope scope,u32 field,float value,std::string &diagnostic) {
  diagnostic.clear();
  const auto *entity=document_.find(id);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(isPlaying() || history_.isOpen()) {diagnostic="Finalize a edição antes de mudar o material.";return false;}
  if(!render || slot>=render->slotCount() || field>=scene::meshRendererNumbers.size()) {diagnostic="Campo de material inexistente.";return false;}
  const auto &number=scene::meshRendererNumbers[field];
  if(!std::isfinite(value) || value<number.minimum || value>number.maximum) {diagnostic="Valor fora do intervalo do campo.";return false;}
  if(scope==MaterialScope::Shared) {
    const auto guid=render->slotMaterialAsset(slot);
    auto found=std::find_if(materials_.begin(),materials_.end(),[&](const auto &material){return material.guid==guid;});
    if(!guid.valid() || found==materials_.end()) {
      diagnostic="Este slot usa o material da fonte; crie um material do projeto para editar todos os usos.";return false;
    }
    const auto *record=assets_.find(guid);
    if(!record) {diagnostic="Material fora do registro.";return false;}
    auto candidate=*found;
    scene::MeshRenderer probe;probe.material=candidate.values;
    *number.write(probe)=value;
    candidate.values=probe.material;candidate.values.enabled=true;++candidate.revision;
    if(!candidate.valid() || !writeMaterialAsset(candidate,record->path,diagnostic)) return false;
    const auto serialized=candidate.serialize();
    assets_.publishImport(guid,Sha256::hex(std::span<const u8>(reinterpret_cast<const u8 *>(serialized.data()),serialized.size())),0,"",{},{});
    assetRegistryDirty_=true;
    *found=std::move(candidate);
    publishMaterialLibrary();
    state_.status="Material compartilhado atualizado em todos os usos";
    return true;
  }
  auto values=*entity;
  auto *edit=editMeshRenderer(values);
  auto *material=edit->editSlotMaterial(slot);
  if(!material->enabled) {
    // Primeira substituição: parte do que está na tela, não de valores padrão.
    *material=mapScene_.slotMaterial(*render,slot);
    if(!material->enabled && render->slotMesh(slot)) *material=mapScene_.materialForAsset(render->slotMesh(slot)-1);
  }
  scene::MeshRenderer probe;probe.material=*material;
  *number.write(probe)=value;
  *material=probe.material;material->enabled=true;
  return history_.applyValues(document_,id,values);
}

bool EditorSession::clearSlotMaterialOverride(EditorEntityId id,u32 slot) {
  const auto *entity=document_.find(id);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(isPlaying() || history_.isOpen() || !render || slot>=render->slotCount() || !render->slotMaterial(slot).enabled) return false;
  auto values=*entity;
  auto *edit=editMeshRenderer(values);
  auto *material=edit->editSlotMaterial(slot);
  *material=render->slotMesh(slot)?mapScene_.materialForAsset(render->slotMesh(slot)-1):scene::MaterialParameters{};
  material->enabled=false;
  return history_.applyValues(document_,id,values);
}

bool EditorSession::previousImportMap(const resources::AssetGuid &source,resources::ImportNodeMap &out) const {
  for(const auto &block:importedSources_) if(block.guid==source && block.map.revision) {out=block.map;return true;}
  const auto root=files_.rootPath();
  if(root.empty()) return false;
  std::filesystem::path absolute;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),resources::importNodeMapPath(source),absolute)) return false;
  std::error_code error;
  if(!std::filesystem::exists(absolute,error)) return false;
  std::vector<u8> bytes;
  // Mapa ilegível não trava a importação: as identidades voltam a nascer das
  // chaves da fonte, que é o comportamento de antes do mapa.
  return EditorImportTransaction::read(absolute,bytes,64u*1024u*1024u) &&
         resources::ImportNodeMap::deserialize(std::string_view(reinterpret_cast<const char *>(bytes.data()),bytes.size()),out);
}

bool EditorSession::persistImportMap(const resources::AssetGuid &source) {
  const auto *nodeMap=importNodeMap(source);const auto root=files_.rootPath();
  if(!nodeMap || root.empty()) return false;
  std::filesystem::path absolute;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),resources::importNodeMapPath(source),absolute)) return false;
  const auto text=nodeMap->serialize();
  std::vector<u8> current;
  if(EditorImportTransaction::read(absolute,current,64u*1024u*1024u) && std::string(current.begin(),current.end())==text) return true;
  std::error_code error;std::filesystem::create_directories(absolute.parent_path(),error);
  return !error && EditorImportTransaction::writeText(absolute,text);
}

void EditorSession::removeImportMapFile(const resources::AssetGuid &source) {
  const auto root=files_.rootPath();
  std::filesystem::path absolute;
  if(root.empty() || !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),resources::importNodeMapPath(source),absolute)) return;
  std::error_code error;std::filesystem::remove(absolute,error);
}

void EditorSession::reportImportReconcile(const ImportReconcileReport &report,const char *context) {
  if(!report.changed() && !report.conflicts && !report.skippedInstances) return;
  std::string text=std::string(context)+": "+std::to_string(report.updated)+" atualizados · "+std::to_string(report.created)+
      " criados · "+std::to_string(report.removed)+" removidos · "+std::to_string(report.orphaned)+" órfãos · "+
      std::to_string(report.conflicts)+" conflitos";
  if(report.adopted) text+=" · "+std::to_string(report.adopted)+" objetos anteriores vinculados";
  if(report.unproven) text+=" · "+std::to_string(report.unproven)+" sem prova de vínculo";
  const bool attention=report.conflicts || report.orphaned || report.skippedInstances;
  setImportStatus(text,attention?EditorConsoleSeverity::Warning:EditorConsoleSeverity::Info);
  for(const auto &line:report.notes) {
    EditorConsoleEntry entry;entry.origin=EditorConsoleOrigin::Importer;entry.severity=EditorConsoleSeverity::Warning;
    entry.message=line;entry.project=files_.rootPath();console_.add(std::move(entry));
  }
}

void EditorSession::refreshImportLinkView() {
  auto &view=state_.importLink;view={};
  const auto *entity=document_.find(state_.selection);
  const auto *link=entity?scene::importLink(entity->components):nullptr;
  if(!link || link->unlinked) {state_.importLinkMenu=false;return;}
  view.linked=true;view.orphan=link->orphan;view.root=link->root;view.overrides=importOverrides(document_,entity->id);
  if(const auto *record=assets_.find(link->source)) {
    view.source=record->path;
    const auto slash=view.source.find_last_of('/');if(slash!=std::string::npos) view.source.erase(0,slash+1);
  } else view.source="Fonte ausente do registro";
  if(const auto *nodeMap=importNodeMap(link->source)) if(const auto *node=nodeMap->find(link->node)) view.node=node->name;
  if(!link->node.valid()) view.node="instância";
}

bool EditorSession::revertImportLink(EditorEntityId id,u32 mask) {
  if(isPlaying() || history_.isOpen()) return false;
  if(!revertImportOverrides(document_,history_,id,mask,[this](const resources::AssetGuid &guid){return mapScene_.assetSlot(guid);}))
    return false;
  mapScene_.hydrateMaterials(document_);
  return true;
}

u32 EditorSession::unlinkImport(EditorEntityId id) {
  return isPlaying()?0:unlinkImportInstance(document_,history_,id);
}

bool EditorSession::resolveImportOrphan(EditorEntityId id,bool keep) {
  const auto *entity=document_.find(id);
  const auto *link=entity?scene::importLink(entity->components):nullptr;
  if(isPlaying() || !link || !link->orphan || history_.isOpen()) return false;
  if(keep) return unlinkImportObject(document_,history_,id);
  // Apagar é decisão explícita do usuário; o histórico ainda desfaz.
  const bool destroyed=history_.destroyEntity(document_,id);
  if(destroyed) state_.selection=kInvalidEntity;
  return destroyed;
}

} // namespace ae::editor
