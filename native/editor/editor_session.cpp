#include "editor/editor_creation_catalog.h"
#include "editor/editor_session.h"
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
  if(state_.selection!=entity) {state_.routePoint=0;state_.propertyPage=0;}
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
    // Uma pasta não é selecionável no viewport: ela não tem corpo, e deixar o
    // toque acertá-la selecionaria um grupo quando o usuário mirou uma peça.
    if (entity->kind == EditorEntityKind::Folder) continue;
    EditorPickCandidate candidate{};
    candidate.id = id;
    // Missing resources have no viewport silhouette. Keep their document row
    // available for repair, but never invent an invisible pick sphere.
    if(!mapScene_.bounds(document_, id, candidate.center, candidate.radius)) continue;
    candidate.selectable = entity->visible && entity->active;
    for(auto parent=document_.find(entity->parent);parent;parent=document_.find(parent->parent))
      candidate.selectable &= parent->visible && parent->active;
    candidates_.push_back(candidate);
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

bool EditorSession::handlePointer(const UiPointerEvent &event) {
  const UiPointerRouting routing = router_.route(event);
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
      else if(entry.name.ends_with(".aescene"))
        requestedScenePath_=files_.resolveFile(entry.relativePath);
      else state_.status="Arquivo de origem; importação ainda não disponível neste painel";
      return true;
    }
  }
  if(state_.draggingAsset && event.pointerId!=assetPointer_) return true;
  if(state_.draggingEntity && event.pointerId!=hierarchyPointer_) return true;
  if (isPlaying() && routing.target == UiPointerTarget::Viewport) return false;
  if(routing.tapped && state_.creationMenu) {
    const auto key=routing.widgetId;
    if(key>=widgetId(EditorWidget::CreationCategoryBase)&&key<widgetId(EditorWidget::CreationCategoryBase)+4) {
      state_.creationCategory=key-widgetId(EditorWidget::CreationCategoryBase);state_.creationPage=0;state_.creationSearch[0]=0;
      for(u32 i=0;i<editorCreationCatalog.size();++i) if(editorCreationCatalog[i].category==state_.creationCategory) {state_.creationSelection=i;break;}
      return true;
    }
    if(key>=widgetId(EditorWidget::CreationRowBase)&&key<widgetId(EditorWidget::CreationRowBase)+editorCreationCatalog.size()) {
      state_.creationSelection=key-widgetId(EditorWidget::CreationRowBase);return true;
    }
  }
  if (state_.renameEntity != kInvalidEntity || state_.editingHierarchySearch || state_.editingCreationSearch) {
    if(routing.tapped) {
      const auto key=routing.widgetId;
      auto n=std::strlen(state_.renameText);
      if(key==widgetId(EditorWidget::NameCancel)) {state_.renameEntity=kInvalidEntity;state_.editingHierarchySearch=false;state_.editingCreationSearch=false;}
      else if(key==widgetId(EditorWidget::NameClear)) state_.renameText[0]=0;
      else if(key==widgetId(EditorWidget::NameShift)) state_.renameUppercase=!state_.renameUppercase;
      else if(key==widgetId(EditorWidget::NameBackspace) && n) {
        do {--n;} while(n && (static_cast<unsigned char>(state_.renameText[n])&0xc0)==0x80);
        state_.renameText[n]=0;
      } else if(key>=widgetId(EditorWidget::NameKeyBase) && key<widgetId(EditorWidget::NameKeyBase)+40 && n+1<sizeof(state_.renameText)) {
        const char *keys=state_.renameUppercase?"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-. ":"abcdefghijklmnopqrstuvwxyz0123456789_-. ";
        state_.renameText[n]=keys[key-widgetId(EditorWidget::NameKeyBase)];state_.renameText[n+1]=0;
      } else if(key==widgetId(EditorWidget::NameApply)) {
        if(state_.editingCreationSearch) {
          std::copy(std::begin(state_.renameText),std::end(state_.renameText),state_.creationSearch);
          state_.editingCreationSearch=false;state_.creationPage=0;
          const auto query=editorSearchKey(state_.creationSearch);
          for(u32 i=0;i<editorCreationCatalog.size();++i) if(query.empty()?editorCreationCatalog[i].category==state_.creationCategory:editorSearchKey(editorCreationCatalog[i].name).find(query)!=std::string::npos) {state_.creationSelection=i;break;}
          return true;
        }
        if(state_.editingHierarchySearch) {
          std::copy(std::begin(state_.renameText),std::end(state_.renameText),state_.hierarchySearch);
          state_.editingHierarchySearch=false;state_.hierarchyScroll=0;return true;
        }
        if(const auto *entity=document_.find(state_.renameEntity)) {
          std::string name=state_.renameText;
          const auto begin=name.find_first_not_of(' '),end=name.find_last_not_of(' ');
          if(begin!=std::string::npos) {
            auto values=*entity;assignEntityName(values,name.substr(begin,end-begin+1));
            if(history_.applyValues(document_,entity->id,values)) state_.renameEntity=kInvalidEntity;
          }
        }
      }
    }
    return true;
  }
  if (state_.numericField != 0) {
    if (routing.tapped) {
      const u32 keyBase = widgetId(EditorWidget::NumericKeyBase);
      const u32 key = routing.widgetId;
      if (key == widgetId(EditorWidget::NumericCancel)) state_.numericField = 0;
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
        float number=0;
        std::istringstream input(state_.numericText);input.imbue(std::locale::classic());
        const bool parsed=static_cast<bool>(input>>number);
        input>>std::ws;
        const auto *entity=document_.find(state_.numericEntity);
        state_.numericError=true;
        if(entity && parsed && input.eof() && std::isfinite(number)) {
          auto value=*entity;
          const u32 field=state_.numericField-widgetId(EditorWidget::TransformFieldBase);
          if(setEditorPropertyValue(value,field,number) && history_.applyValues(document_,entity->id,value))
            state_.numericField=0;
        }
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
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::CreateCamera)) {
    history_.begin("Criar câmera");
    const auto id=history_.createEntity(document_,document_.root(),EditorEntityKind::Camera,"Câmera");
    if(id) {
      auto value=*document_.find(id);editorCameraPosition(camera_,value.transform.position);
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
    if(selected && selected->route.count) {
      auto value=*selected;auto &route=value.route;state_.routePoint=std::min(state_.routePoint,route.count-1);
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
    if(selected && point<selected->route.count) {
      state_.routePoint=point;
      if(event.phase==ui::UiPointerPhase::Down && !history_.isOpen()) {history_.begin("Mover ponto do rio");fieldWidget_=routing.widgetId;fieldPointer_=event.pointerId;fieldInitial_=*selected;dragEntity_=selected->id;}
      if(routing.dragging && event.phase==UiPointerPhase::Move && fieldWidget_==routing.widgetId && fieldPointer_==event.pointerId) {
        float world[16],normal[12];
        const auto ray=screenPointToRay(view_,event.position);
        if(editorWorldMatrix(document_,selected->id,world) && renderer::buildNormalMatrix(world,normal) && ray.valid && std::abs(ray.direction[1])>1e-5f) {
          const auto &p=fieldInitial_.route.points[point];const float height=world[13]+world[1]*p.position[0]+world[5]*p.position[1]+world[9]*p.position[2];
          const float distance=(height-ray.origin[1])/ray.direction[1];
          if(distance>0) {
            float offset[3];for(u32 axis=0;axis<3;++axis) offset[axis]=ray.origin[axis]+distance*ray.direction[axis]-world[12+axis];
            auto value=*selected;
            for(u32 axis=0;axis<3;++axis) value.route.points[point].position[axis]=normal[axis*4]*offset[0]+normal[axis*4+1]*offset[1]+normal[axis*4+2]*offset[2];
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
          state_.numericField=fieldWidget_;state_.numericEntity=dragEntity_;
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
  if (!isPlaying() || delta <= 0.0f || delta > 1.0f) return;
  sceneTime_ += delta;
}

void EditorSession::frameSelection() {
  const EditorEntity *entity = document_.find(state_.selection);
  if (entity == nullptr) return;
  if(entity->kind==EditorEntityKind::Folder) {frameSubtree(entity->id);return;}
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
  EditorDocument candidate;
  if(!loadEditorDocument(path,fingerprint,candidate)) return false;
  std::vector<renderer::MapDrawState> check;
  if(!mapScene_.extract(candidate,check)) return false;
  mapScene_.hydrateMaterials(candidate);
  cancelPointers();document_=std::move(candidate);history_.clear();
  sceneEpoch_=nextSceneEpoch();
  state_.selection=kInvalidEntity;state_.status="Cena restaurada";
  state_.collapsedEntities.clear();state_.hierarchyScroll=0;state_.renameEntity=kInvalidEntity;
  return true;
}

bool EditorSession::importMap(std::span<const renderer::MapDrawRecord> draws, std::span<const renderer::MapMaterialRecord> materials, bool instantiate) {
  cancelPointers();
  if (!mapScene_.import(document_, draws, materials, instantiate)) return false;
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
  EditorEntity values;values.assetId=index+1;values.material=mapScene_.materialForAsset(index);
  float parentWorld[16],world[16];EditorTransform pose;
  std::copy(worldPosition,worldPosition+3,pose.position);editorTransformMatrix(pose,world);
  if(!editorWorldMatrix(document_,parent,parentWorld) ||
     !editorLocalTransformForWorld(world,parentWorld,values.transform) || !validEditorAppearance(values)) return kInvalidEntity;
  const bool water=(mapScene_.materialFlagsForAsset(index)&renderer::MapMaterialWater)!=0;
  values.rigidBodyEnabled=(mapScene_.materialFlagsForAsset(index)&renderer::BoxAuthoringResource)!=0;
  values.waterInfinite=(mapScene_.materialFlagsForAsset(index)&renderer::MapMaterialWaterCameraGrid)!=0;
  if(mapScene_.materialFlagsForAsset(index)&renderer::WaterRouteResource) {
    values.route.count=3;
    values.route.points[0].position[2]=-12;
    values.route.points[1].position[0]=8;
    values.route.points[2].position[2]=12;
  }
  const char *role=values.route.count?"Rio":values.waterInfinite?"Oceano":water?"Superfície de água":values.rigidBodyEnabled?"Corpo rígido":"Malha";
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
  if(const auto *selected=document_.find(state_.selection)) state_.routePoint=selected->route.count?std::min(state_.routePoint,selected->route.count-1):0;
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
