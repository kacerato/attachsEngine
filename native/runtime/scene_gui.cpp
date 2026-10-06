#include "runtime/scene_gui.h"
#include <algorithm>
#include <cstring>

namespace ae::runtime {
namespace {
bool shown(const SceneGraph &graph,ObjectId object) {
  for(const auto *n=graph.find(object);n;n=graph.find(n->parent))if(!n->active||!n->visible)return false;
  return true;
}
}
ui::GuiCanvas uiCanvasPresentation(const scene::UiCanvas &c) {
  ui::GuiCanvas out;out.mode=static_cast<ui::GuiCanvasMode>(c.mode);
  out.resolution={c.resolution[0],c.resolution[1]};out.unitsPerPixel=c.unitsPerPixel;out.occlusion=c.occlusion;
  std::copy_n(c.offset,3,out.position);std::copy_n(c.rotation,3,out.rotation);return out;
}
namespace {
bool samePresentation(const scene::UiCanvas &a,const scene::UiCanvas &b) {
  return a.mode==b.mode&&a.occlusion==b.occlusion&&a.unitsPerPixel==b.unitsPerPixel&&
    std::equal(a.offset,a.offset+3,b.offset)&&std::equal(a.rotation,a.rotation+3,b.rotation)&&std::equal(a.resolution,a.resolution+2,b.resolution);
}
}
void SceneGui::reset(){cancelPointers();routes_.clear();receivers_.clear();instances_.clear();documents_.clear();failures_.clear();world_=0;graphRevision_=~u64{0};diagnostic_.clear();inputDiagnostic_.clear();++revision_;}
void SceneGui::invalidateResources(){cancelPointers();instances_.clear();documents_.clear();failures_.clear();graphRevision_=~u64{0};++revision_;}
void SceneGui::cancelPointers(){for(auto &i:instances_)i->runtime.cancelPointers();for(auto &r:receivers_){r.input.reset();r.pendingJump.clear();}}
bool SceneGui::captures(u32 pointer,ui::UiPointerDevice device) const {return std::any_of(routes_.begin(),routes_.end(),[&](const auto &r){return r.pointer==pointer&&r.device==device;});}
void SceneGui::setImages(const ui::GuiImageAtlas *images){images_=images;for(auto &i:instances_)i->runtime.setImages(images);}
SceneGui::Instance *SceneGui::find(GameWorld &world,u64 id) {
  if(!world.running() || world.worldId()!=world_)return nullptr;
  for(auto &i:instances_)if(i->id==id && world.validate(i->owner.object)==WorldStatus::Ok && world.componentTypeId(i->owner)==scene::UiCanvas::descriptor.id)return i.get();
  return nullptr;
}
u64 SceneGui::instanceFor(GameWorld &world,ObjectId object,u64 component){reconcile(world);for(auto &i:instances_)if(i->owner.object.id==object&&(!component||component==i->owner.instance))return i->id;return 0;}
bool SceneGui::reconcile(GameWorld &world) {
  if(!world.running()){reset();return false;}
  if(world_!=world.worldId()){reset();world_=world.worldId();}
  const auto &graph=world.graph();
  if(graphRevision_==graph.revision())return diagnostic_.empty();
  graphRevision_=graph.revision();diagnostic_.clear();
  std::vector<std::pair<ObjectId,u64>> alive;
  std::vector<ObjectId> objects;graph.collectSubtree(graph.root(),objects);
  for(auto object:objects){const auto *node=graph.find(object);for(u32 slot=0;slot<node->components.size();++slot) {const auto *value=node->components.at(slot);
    if(value->type().id!=scene::UiCanvas::descriptor.id)continue;
    const auto &c=static_cast<const scene::UiCanvas&>(*value);ComponentHandle owner{world.handle(object),c.instanceId()};
    if(world.validate(owner.object)!=WorldStatus::Ok || world.componentTypeId(owner).empty())continue;
    alive.emplace_back(object,c.instanceId());
    auto at=std::find_if(instances_.begin(),instances_.end(),[&](const auto &i){return i->owner.object.id==object&&i->owner.instance==c.instanceId();});
    if(at!=instances_.end() && (*at)->asset!=c.document){(*at)->runtime.cancelPointers();instances_.erase(at);at=instances_.end();++revision_;}
    if(!c.valid()){if(at!=instances_.end()){(*at)->runtime.cancelPointers();instances_.erase(at);++revision_;}diagnostic_="Propriedades inválidas de Canvas UI: objeto "+std::to_string(object);continue;}
    if(!c.document.valid()){if(at!=instances_.end()){instances_.erase(at);++revision_;}diagnostic_="Canvas UI sem documento: objeto "+std::to_string(object);continue;}
    if(at==instances_.end()) {
      if(instances_.size()>=kMaximumInstances){diagnostic_="Limite de 64 instâncias UI excedido";continue;}
      auto asset=documents_.find(c.document);
      if(asset==documents_.end()) {
        const auto failure=failures_.find(c.document);
        if(failure!=failures_.end()){diagnostic_=failure->second;continue;}
        ui::GuiDocument document;std::string error;
        if(!loader_||!loader_(c.document,document,error)||!document.validate(error)) {
          diagnostic_=error.empty()?"Documento UI não disponível":error;failures_[c.document]=diagnostic_;continue;
        }
        asset=documents_.emplace(c.document,std::make_shared<const ui::GuiDocument>(std::move(document))).first;
      }
      auto entry=std::make_unique<Instance>();entry->id=nextId_++;entry->owner=owner;entry->asset=c.document;entry->config=c;
      entry->runtime.load(*asset->second);std::string error;entry->runtime.document().setCanvas(uiCanvasPresentation(c),error);entry->runtime.setImages(images_);
      instances_.push_back(std::move(entry));at=std::prev(instances_.end());++revision_;
    }
    auto &i=**at;
    if(i.config.inputReceiver!=c.inputReceiver||i.config.inputCamera!=c.inputCamera||i.config.movementSpace!=c.movementSpace)i.runtime.cancelPointers();
    if(!samePresentation(i.config,c)){i.runtime.cancelPointers();std::string error;i.runtime.document().setCanvas(uiCanvasPresentation(c),error);i.prepared=false;}
    i.config=c;i.enabled=c.enabled&&shown(graph,object);
    if(!i.enabled){i.runtime.cancelPointers();i.prepared=false;}
  }}
  const auto removed=std::erase_if(instances_,[&](const auto &i){return std::find(alive.begin(),alive.end(),std::pair{i->owner.object.id,i->owner.instance})==alive.end() || world.validate(i->owner.object)!=WorldStatus::Ok || world.componentTypeId(i->owner)!=scene::UiCanvas::descriptor.id;});
  if(removed)++revision_;
  // Stable component identity breaks ties; same asset never aliases input state.
  std::stable_sort(instances_.begin(),instances_.end(),[](const auto &a,const auto &b){return a->config.order<b->config.order || (a->config.order==b->config.order&&a->id<b->id);});
  for(auto i=documents_.begin();i!=documents_.end();) {
    if(std::none_of(instances_.begin(),instances_.end(),[&](const auto &e){return e->asset==i->first;}))i=documents_.erase(i);else ++i;
  }
  return diagnostic_.empty();
}
void SceneGui::advance(GameWorld &world,double seconds){reconcile(world);for(auto &i:instances_)if(i->enabled&&find(world,i->id))i->runtime.advance(seconds);}
void SceneGui::prepare(GameWorld &world,const renderer::PerspectiveFrustum &camera,const ui::UiRect &viewport,const ui::UiRect &surface) {
  reconcile(world);
  for(auto &i:instances_) {
    i->prepared=false;if(!i->enabled||!find(world,i->id))continue;
    const auto &canvas=i->runtime.document().canvas();
    if(canvas.mode==ui::GuiCanvasMode::World) {
      if(!worldMatrix(world.poseGraph(),i->owner.object.id,i->hostMatrix)||!i->frame.configure(canvas,camera,viewport,surface,i->hostMatrix)){i->runtime.cancelPointers();diagnostic_="Matriz/câmera inválida para Canvas UI "+std::to_string(i->owner.object.id);continue;}
      i->runtime.layout({0,0,canvas.resolution.x,canvas.resolution.y});
    } else {if(viewport.isEmpty()){i->runtime.cancelPointers();continue;}i->runtime.layout(viewport);}
    i->viewport=viewport;i->prepared=true;
  }
}
void SceneGui::draw(ui::UiDrawList &out,const ui::UiFontMetrics &metrics) {
  for(auto &i:instances_)if(i->prepared&&i->enabled) {
    const auto &c=i->runtime.document().canvas();
    if(c.mode==ui::GuiCanvasMode::Screen)i->runtime.draw(out);
    else {scratch_.begin({0,0,c.resolution.x,c.resolution.y},metrics);i->runtime.draw(scratch_);scratch_.projectRange(0,i->frame.projection(),c.occlusion,i->viewport);out.append(scratch_);}
  }
}
bool SceneGui::pointer(GameWorld &world,const ui::UiPointerEvent &e,const Occlusion &occluded) {
  auto dispatch=[&](Instance &i,bool captured) {
    if(!i.enabled||!i.prepared||!find(world,i.id)){if(captured)i.runtime.cancelPointers();return captured;}
    // Projection may extend under editor/tool surfaces; clipped-away pixels
    // cannot start a gesture. Existing captures still own their full gesture.
    if(!captured&&!i.viewport.contains(e.position))return false;
    auto event=e;float distance=0;
    if(i.runtime.document().canvas().mode==ui::GuiCanvasMode::World) {
      if(!i.frame.map(e.position,event.position,distance)){if(captured){event.phase=ui::UiPointerPhase::Cancel;i.runtime.pointer(event);}return captured;}
      if(!captured && i.runtime.document().canvas().occlusion&&occluded&&occluded(e.position,distance))return false;
    }
    return i.runtime.pointer(event);
  };
  auto route=std::find_if(routes_.begin(),routes_.end(),[&](const auto &r){return r.pointer==e.pointerId&&r.device==e.device;});
  if(route!=routes_.end()) {
    auto *owner=find(world,route->instance);
    if(e.phase!=ui::UiPointerPhase::Down || (owner&&owner->runtime.captures(e.pointerId,e.device))) {
      if(owner)dispatch(*owner,true);
      if(e.phase==ui::UiPointerPhase::Up||e.phase==ui::UiPointerPhase::Cancel)routes_.erase(route);
      return true;
    }
    // A fresh down begins a new gesture after focus loss/device reconnect.
    routes_.erase(route);
  }
  if(e.phase!=ui::UiPointerPhase::Down)return false;
  if(routes_.size()>=kMaximumInstances*32){diagnostic_="Quota de capturas UI excedida";return true;}
  for(auto at=instances_.rbegin();at!=instances_.rend();++at)if(dispatch(**at,false)) {
    if((*at)->runtime.captures(e.pointerId,e.device))routes_.push_back({e.pointerId,e.device,(*at)->id});
    return true;
  }
  return false;
}
}
