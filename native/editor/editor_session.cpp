#include "editor/editor_component_catalog.h"
#include "editor/editor_collider_fit.h"
#include "scene/script_behavior.h"
#include "editor/editor_water_body_component.h"
#include "editor/editor_route_component.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_session.h"
#include "core/sha256.h"
#include "editor/editor_script_templates.h"
#include "editor/editor_reference_picker.h"
#include "editor/editor_properties.h"
#include "editor/editor_theme.h"
#include "renderer/water_authoring_geometry.h"

#include <algorithm>
#include <atomic>
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
  if(state_.selection!=entity) {state_.routePoint=0;state_.propertyPage=0;state_.componentPage=0;state_.expandedScript=0;state_.scriptMenu=0;}
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
  code_.clear();state_.code=&code_;
  state_.files=&files_;state_.fileScroll=0;state_.fileScrollOffset=0;
  return files_.setRoot(path);
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
    candidates_.push_back(std::move(candidate));
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
  if(state_.editingCode) {
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
  if(state_.creatingScript) {edit.purpose=EditorTextPurpose::ScriptName;return edit;}
  if(state_.searchingCode) {edit.purpose=EditorTextPurpose::CodeSearch;edit.text=state_.codeQuery;return edit;}
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

bool EditorSession::completeTextEdit(const EditorTextEdit &edit,std::string_view text,bool accept) {
  const auto current=pendingTextEdit();
  if(edit.purpose==EditorTextPurpose::None || current.purpose!=edit.purpose ||
     current.entity!=edit.entity || current.field!=edit.field || edit.version.epoch!=sceneEpoch_) return false;
  const auto close=[&] {
    state_.editingCode=false;state_.creatingScript=false;state_.searchingCode=false;
    state_.choosingTemplate=false;
    state_.editingScriptInstance=0;state_.editingScriptEntity=0;state_.editingScriptProperty.clear();state_.editingScriptType.clear();
    state_.numericField=0;state_.numericInstance=0;state_.numericProperty.clear();state_.renameEntity=0;
    state_.editingComponentSearch=false;state_.editingMeshSearch=false;state_.editingReferenceSearch=false;
    state_.editingHierarchySearch=false;state_.editingCreationSearch=false;
    cancelPointers();
  };
  if(!accept) {close();return true;}
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
    const bool accepted=code_.replace(edit.bufferId,edit.bufferRevision,text);
    if(accepted) close();else state_.status=code_.error();
    return accepted;
  }
  if(edit.purpose==EditorTextPurpose::ScriptName) {
    if(!code_.createScript(files_,text,state_.scriptTemplate)) {state_.status=code_.error();return false;}
    state_.scriptTemplate=~0u;
    const auto root=files_.rootPath();files_.setRoot(root.c_str());
    state_.workspace=EditorWorkspace::Code;close();return true;
  }
  if(edit.purpose==EditorTextPurpose::CodeSearch) {
    state_.codeQuery=text;const auto matches=code_.find(text);
    if(auto *buffer=code_.active();buffer && !matches.empty()) buffer->firstLine=matches.front().line-1;
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
  if(state_.workspace==EditorWorkspace::Code && routing.widgetId==widgetId(EditorWidget::CodeBody)) {
    if(routing.dragging) if(auto *buffer=code_.active()) {
      const auto count=static_cast<u32>(std::count(buffer->text.begin(),buffer->text.end(),'\n'));
      const int next=static_cast<int>(buffer->firstLine)-static_cast<int>(routing.stepDelta.y/12);
      buffer->firstLine=std::min(count,static_cast<u32>(std::max(0,next)));
    }
    if(routing.tapped) state_.editingCode=code_.active()!=nullptr;
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
    if(key==widgetId(EditorWidget::TransformFold)) {
      state_.componentSelection=state_.selection;state_.expandedComponent=state_.expandedComponent=="astra.transform"?"":"astra.transform";
      state_.expandedNative=0;state_.expandedScript=0;state_.nativeMenu=0;state_.scriptMenu=0;state_.meshPicker=false;state_.propertyPage=0;return true;
    }
    if(key==widgetId(EditorWidget::MeshChoose)) {state_.meshPicker=true;state_.meshPage=0;return true;}
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
        if(state_.codeCompilerAvailable && !state_.codeBuildBusy) {
          if(files_.rootPath().empty()) state_.status="Abra um projeto antes de aplicar código";
          else if(!code_.saveAll(files_)) state_.status=code_.error();
          else {codeBuildGeneration_=code_.generation();codeBuildRequest_=files_.rootPath();state_.codeBuildBusy=true;state_.status="Compilando código do projeto";}
        }
        return true;
      case EditorWidget::CodeScene:state_.workspace=EditorWorkspace::Scene;return true;
      case EditorWidget::CodeNew:state_.choosingTemplate=true;state_.workspace=EditorWorkspace::Code;return true;
      case EditorWidget::CodeTemplateClose:state_.choosingTemplate=false;return true;
      case EditorWidget::CodeEdit:state_.editingCode=code_.active()!=nullptr;return true;
      case EditorWidget::CodeSave:state_.status=code_.save(files_)?"Código salvo":code_.error();return true;
      case EditorWidget::CodeUndo:code_.undo();return true;
      case EditorWidget::CodeRedo:code_.redo();return true;
      case EditorWidget::CodeSearch:state_.searchingCode=true;return true;
      case EditorWidget::CodeClose:
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
    const auto visible=static_cast<u32>(std::max(24.0f,layout_.filesPanel.height-40)/24);
    const auto maximum=files_.tree().size()>visible?files_.tree().size()-visible:0;
    state_.fileScrollOffset=std::min(state_.fileScrollOffset,static_cast<float>(maximum)*24);
    state_.fileScroll=static_cast<u32>(state_.fileScrollOffset/24);
    return true;
  }
  if(routing.tapped && state_.files && !history_.isOpen() && !isPlaying()) {
    const auto key=routing.widgetId;
    if(key==widgetId(EditorWidget::FilesCollapse)) {state_.filesCollapsed=!state_.filesCollapsed;return true;}
    const u32 base=widgetId(EditorWidget::FileRowBase);
    if(key>=base && key-base<files_.tree().size()) {
      const auto entry=files_.tree()[key-base];
      if(entry.directory) files_.toggle(key-base);
      else if(entry.name.ends_with(".cs") || entry.name.ends_with(".json") || entry.name.ends_with(".md")) {
        state_.code=&code_;
        if(code_.open(files_,entry.relativePath)) state_.workspace=EditorWorkspace::Code;
        else state_.status=code_.error();
      }
      else if(entry.name.ends_with(".aescene"))
        requestedScenePath_=files_.resolveFile(entry.relativePath);
      else state_.status="Arquivo de origem; importação ainda não disponível neste painel";
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
  std::vector<renderer::MapDrawState> check;
  if(!mapScene_.extract(candidate,check)) return false;
  mapScene_.hydrateMaterials(candidate);
  cancelPointers();document_=std::move(candidate);history_.clear();
  sceneEpoch_=nextSceneEpoch();
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
    library.materials.insert(library.materials.end(),source.materials.begin(),source.materials.end());
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

  ImportedSource block;
  block.guid=source;
  block.vertices.assign(model.vertices.begin(),model.vertices.end());
  block.indices.assign(model.indices.begin(),model.indices.end());
  block.materials.assign(model.materials.begin(),model.materials.end());
  block.draws.assign(model.draws.begin(),model.draws.end());
  block.nodes=model.nodes;
  block.drawNodes=model.drawNodes;
  for(usize i=0;i<model.draws.size();++i) {
    // A identidade de cada malha importada: a fonte mais a chave estável. É o
    // que sobrevive a acrescentar um objeto no editor 3D e reexportar.
    block.identities.push_back(resources::assetGuidFromSeed(
        "glb:"+source.text()+":"+(i<model.keys.size()?model.keys[i]:std::to_string(i))));
    block.names.push_back(i<model.names.size()?model.names[i]:std::string("Malha"));
  }
  // Duas malhas com a mesma chave no mesmo arquivo tornariam a identidade
  // ambígua — e um slot escolhido pela ordem da lista é o defeito que a
  // identidade existe para impedir.
  for(usize a=0;a<block.identities.size();++a)
    for(usize b=0;b<a;++b) if(block.identities[a]==block.identities[b]) {
      report.diagnostic="Duas malhas do arquivo têm o mesmo nome de nó e malha; renomeie uma delas.";
      return false;
    }

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
  const auto firstNew=[&]{
    usize count=0;
    for(usize i=0;i<slot && i<candidateSources.size();++i) count+=candidateSources[i].draws.size();
    return count;
  }();
  const auto newDrawCount=block.draws.size();
  // Onde o bloco entra é decidido pela BIBLIOTECA (substituir o que já está
  // carregado); se objetos são criados, pelo REGISTRO. São perguntas diferentes:
  // ao reabrir o projeto a fonte está no registro e não na biblioteca.
  if(slot<candidateSources.size()) candidateSources[slot]=std::move(block);
  else candidateSources.push_back(std::move(block));
  const auto candidate=flattenSources(candidateSources);

  PublishedGeometry published;
  if(!publishGeometry_(candidate.vertices,candidate.indices,candidate.draws,candidate.materials,published)) {
    report.diagnostic="O consumidor gráfico recusou a geometria importada.";return false;
  }
  // O pacote publicado é primitivas internas + biblioteca, nessa ordem. As
  // primitivas mantêm a identidade derivada da impressão digital.
  if(published.draws.size()<candidate.draws.size()) { report.diagnostic="Pacote publicado inconsistente."; return false; }
  const auto primitives=published.draws.size()-candidate.draws.size();
  std::vector<resources::AssetGuid> identities(primitives);
  identities.insert(identities.end(),candidate.identities.begin(),candidate.identities.end());
  // Pivô: as primitivas internas mantêm a convenção do pacote (centro dos
  // limites), a geometria importada usa a origem do nó.
  std::vector<float> pivots;
  pivots.reserve(published.draws.size()*3);
  for(usize i=0;i<primitives;++i)
    for(u32 axis=0;axis<3;++axis) pivots.push_back(published.draws[i].boundsCenter[axis]);
  pivots.insert(pivots.end(),candidate.pivots.begin(),candidate.pivots.end());
  if(pivots.size()!=published.draws.size()*3) { report.diagnostic="Pacote publicado inconsistente."; return false; }
  cancelPointers();
  if(!mapScene_.adoptPackage(document_,published.draws,published.materials,published.vertices,
                             published.indices,identities,packageFingerprint_,pivots)) {
    report.diagnostic="O editor recusou o pacote publicado.";return false;
  }
  importedSources_=std::move(candidateSources);

  // Reimportar NÃO cria objetos: os que já existem apontam para as mesmas
  // identidades e acabaram de ser reconciliados com a geometria nova.
  if(!report.reimported) {
    // A árvore do arquivo vira árvore de objetos. Um nó por objeto, INCLUSIVE
    // os sem malha: é o grupo vazio que segura a porta no lugar quando a
    // carroceria se move, e descartá-lo é exatamente o que achata a hierarquia.
    const auto &tree=importedSources_[slot<importedSources_.size()?slot:importedSources_.size()-1];
    history_.begin("Importar modelo");
    std::vector<EditorEntityId> created(tree.nodes.size(),kInvalidEntity);
    const float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    for(usize n=0;n<tree.nodes.size();++n) {
      const auto &node=tree.nodes[n];
      const auto parent=node.parent>=0 && static_cast<usize>(node.parent)<created.size()
          ? created[static_cast<usize>(node.parent)] : document_.root();
      if(parent==kInvalidEntity) { history_.end(); report.diagnostic="Hierarquia do modelo fora de ordem."; return false; }
      const auto id=history_.createEntity(document_,parent,EditorEntityKind::Mesh,
                                          node.name.empty()?"Objeto":node.name.c_str());
      if(!id) { history_.end(); report.diagnostic="Não foi possível criar os objetos do modelo."; return false; }
      created[n]=id;
      auto value=*document_.find(id);
      // A pose LOCAL do nó vira a transformação do objeto. Um nó cuja matriz
      // não cabe em TRS mantém a pose assada no desenho e o objeto nasce na
      // identidade; o relatório conta esses casos em vez de arredondar e
      // chamar de preservado.
      EditorTransform local;
      if(runtime::localTransformForWorld(node.localMatrix,identity,local)) value.transform=local;
      else ++report.shearedNodes;
      history_.applyValues(document_,id,value);
      ++report.objects;
    }
    // Cada desenho entra no objeto do seu nó. Um nó com várias primitivas
    // ganha filhos: slots por submesh são entrega de material, e inventar um
    // objeto por primitiva sem dizer seria pior que dizer.
    std::vector<u32> drawsPerNode(tree.nodes.size(),0);
    for(usize i=0;i<newDrawCount;++i) {
      const auto nodeIndex=i<tree.drawNodes.size()?tree.drawNodes[i]:0u;
      if(nodeIndex<drawsPerNode.size()) ++drawsPerNode[nodeIndex];
    }
    std::vector<u32> placed(tree.nodes.size(),0);
    for(usize i=0;i<newDrawCount;++i) {
      const auto drawSlot=static_cast<u32>(primitives+firstNew+i)+1;
      const auto nodeIndex=i<tree.drawNodes.size()?tree.drawNodes[i]:0u;
      if(nodeIndex>=created.size()) { history_.end(); report.diagnostic="Desenho sem nó no modelo."; return false; }
      auto target=created[nodeIndex];
      if(drawsPerNode[nodeIndex]>1) {
        const auto name=tree.names[firstNew+i]+" · "+std::to_string(placed[nodeIndex]+1);
        target=history_.createEntity(document_,created[nodeIndex],EditorEntityKind::Mesh,name.c_str());
        if(!target) { history_.end(); report.diagnostic="Não foi possível criar as partes do modelo."; return false; }
        ++report.objects;
      }
      ++placed[nodeIndex];
      auto value=*document_.find(target);
      auto *render=editMeshRenderer(value);
      if(!render) { history_.end(); report.diagnostic="Não foi possível criar a malha do objeto."; return false; }
      render->mesh=drawSlot;
      render->asset=candidate.identities[firstNew+i];
      render->material=mapScene_.materialForAsset(drawSlot-1);
      history_.applyValues(document_,target,value);
    }
    for(usize n=0;n<tree.nodes.size();++n) if(!drawsPerNode[n]) ++report.groups;
    history_.end();
  }

  resources::AssetRecord record=existing?*existing:resources::AssetRecord{};
  record.guid=source;record.type=resources::AssetType::Mesh;record.path=path;record.source=path;
  record.contentHash=Sha256::hex(bytes);
  record.importerVersion=1;
  record.importerParameters="glb";
  record.derived.clear();
  if(existing) assets_.publishImport(source,record.contentHash,record.importerVersion,record.importerParameters,{},{});
  else assets_.add(record);
  state_.status=report.reimported?"Modelo reimportado":"Modelo importado";
  return true;
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
  layout_ = buildEditorScreen(state_, editorTheme(), list_, router_);
  const u32 maximumScroll=layout_.hierarchyRowCount>layout_.hierarchyVisibleRows
      ? layout_.hierarchyRowCount-layout_.hierarchyVisibleRows : 0;
  state_.hierarchyScroll=std::min(state_.hierarchyScroll,maximumScroll);


  // Identidade, e não a pré-rotação do display: o retângulo da vista já está no
  // espaço lógico em paisagem, e o shader da interface roda tudo uma vez no fim.
  view_ = buildEditorViewport(camera_, layout_.viewport, {}, projection_);
  state_.view = &view_;

  list_.begin(state_.surface, metrics);
  router_.beginFrame();
  layout_ = buildEditorScreen(state_, editorTheme(), list_, router_);

  instances_.clear();
  buildUiInstances(list_, *font_, *icons_, kMaximumInstances, instances_);
}

} // namespace ae::editor
