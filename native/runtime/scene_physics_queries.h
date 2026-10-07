#pragma once
#include "runtime/game_world.h"
#include "runtime/scene_physics.h"
#include "runtime/transform_math.h"
#include "scene/physics_body.h"
#include "scene/physics_queries.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string_view>
#include <vector>

namespace ae::runtime {
// Avaliador de Play do Raio, da Varredura de forma e do Braço de mola. Roda
// depois da física em cada quadro; cada componente guarda o último resultado.
// O Braço de mola escreve a pose local dos filhos diretos quando nenhum outro
// sistema tem autoridade sobre ela.
class ScenePhysicsQueries final {
public:
  struct Result {
    bool hit=false;ObjectId collider=kInvalidObject;
    float point[3]{},normal[3]{},distance=0,fraction=0,armLength=0;
  };
  using Key=std::pair<ObjectId,u64>;
  void reset() {results_.clear();world_=0;revision_=~u64{0};ids_.clear();}
  const Result *result(ObjectId id,u64 instance) const {auto i=results_.find({id,instance});return i==results_.end()?nullptr:&i->second;}

  bool advance(GameWorld &w,const ScenePhysics &physics) {
    if(!w.running()) return false;
    sync(w);std::set<Key> seen;
    for(const auto id:ids_) {
      const auto *o=w.graph().find(id);if(!o||!w.activeInHierarchy(w.handle(id))) continue;
      for(usize k=0;k<o->components.size();++k) {
        const auto *v=o->components.at(k);const auto &type=v->type();
        if(&type!=&scene::RayCast::descriptor&&&type!=&scene::ShapeCast::descriptor&&&type!=&scene::SpringArm::descriptor) continue;
        const Key key{id,v->instanceId()};seen.insert(key);
        evaluate(w,physics,id,*v,results_[key]);
      }
    }
    for(auto i=results_.begin();i!=results_.end();) if(!seen.contains(i->first)) i=results_.erase(i); else ++i;
    return true;
  }

  // Métodos dos descritores; `update` refaz a consulta agora.
  WorldStatus command(GameWorld &w,const ScenePhysics &physics,ComponentHandle h,std::string_view method,scene::ComponentOperationValue &out) {
    const auto valid=w.validate(h.object);if(valid!=WorldStatus::Ok) return valid;
    const auto *v=w.readComponent(h);if(!v) return WorldStatus::ComponentMissing;
    sync(w);auto &r=results_[{h.object.id,h.instance}];
    if(method=="update") {evaluate(w,physics,h.object.id,*v,r);return WorldStatus::Ok;}
    using V=scene::ComponentOperationValue;
    if(method=="colliding") out=V::makeBoolean(r.hit);
    else if(method=="collider") out=V::makeObject(r.hit?r.collider:0);
    else if(method=="point") out=V::makeVector(r.point[0],r.point[1],r.point[2]);
    else if(method=="normal") out=V::makeVector(r.normal[0],r.normal[1],r.normal[2]);
    else if(method=="distance") out=V::makeNumber(r.distance);
    else if(method=="hit_length") out=V::makeNumber(r.armLength);
    else return WorldStatus::InvalidArgument;
    return WorldStatus::Ok;
  }

private:
  static QueryFilter filterFor(const GameWorld &w,ObjectId id,const scene::PhysicsQueryFilterFields &f) {
    QueryFilter filter;filter.gameplayLayerMask=f.layer?1u<<(f.layer-1):0xffffffffu;
    filter.includeSensors=f.includeSensors;filter.includeStatic=f.includeStatic;filter.includeDynamic=f.includeDynamic;
    if(f.excludeSelf) for(ObjectId p=id;p;) {
      const auto *o=w.graph().find(p);if(!o) break;
      if(o->components.find(scene::PhysicsBody::descriptor)) {filter.ignore=p;break;}
      p=o->parent;
    }
    return filter;
  }
  static void rotate(const float m[16],const float local[3],float out[3]) {
    for(u32 a=0;a<3;++a) out[a]=m[a]*local[0]+m[4+a]*local[1]+m[8+a]*local[2];
  }
  static void store(Result &r,bool hit,const QueryHit &q) {
    r.hit=hit;r.collider=hit?q.object:kInvalidObject;r.distance=hit?q.distance:0;r.fraction=hit?q.fraction:0;
    for(u32 a=0;a<3;++a) {r.point[a]=hit?q.point[a]:0;r.normal[a]=hit&&q.hasNormal?q.normal[a]:0;}
  }
  void evaluate(GameWorld &w,const ScenePhysics &physics,ObjectId id,const scene::ComponentValue &v,Result &r) {
    float m[16];
    if(!worldMatrix(w.graph(),id,m)) {r=Result{};return;}
    const float origin[3]{m[12],m[13],m[14]};
    if(&v.type()==&scene::RayCast::descriptor) {
      const auto &c=static_cast<const scene::RayCast&>(v);
      if(!c.filter.enabled) {r=Result{};return;}
      float direction[3];rotate(m,c.target,direction);QueryHit hit;
      store(r,validPhysicsQueryRay(origin,direction)&&physics.rayCast(origin,direction,filterFor(w,id,c.filter),hit),hit);
    } else if(&v.type()==&scene::ShapeCast::descriptor) {
      const auto &c=static_cast<const scene::ShapeCast&>(v);
      if(!c.filter.enabled) {r=Result{};return;}
      QueryShapeDesc shape;constexpr QueryShapeKind kinds[]{QueryShapeKind::Sphere,QueryShapeKind::Box,QueryShapeKind::Capsule,QueryShapeKind::Cylinder};
      shape.kind=kinds[static_cast<u32>(c.shape)];shape.radius=c.radius;shape.halfHeight=c.halfHeight;
      for(u32 a=0;a<3;++a) shape.halfExtent[a]=c.halfExtent[a];
      float direction[3];rotate(m,c.target,direction);QueryHit hit;
      store(r,validPhysicsQueryShape(shape)&&validPhysicsQueryRay(origin,direction)&&physics.shapeCast(shape,origin,direction,filterFor(w,id,c.filter),hit),hit);
    } else {
      const auto &c=static_cast<const scene::SpringArm&>(v);
      if(!c.filter.enabled) {r=Result{};return;}
      const float axis[3]{0,0,c.length};float direction[3];rotate(m,axis,direction);
      const float worldLength=std::sqrt(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
      if(!(worldLength>1e-6f)) {r=Result{};return;}
      QueryHit hit;bool found=false;const auto filter=filterFor(w,id,c.filter);
      if(c.radius>0) {QueryShapeDesc sphere;sphere.kind=QueryShapeKind::Sphere;sphere.radius=c.radius;found=physics.shapeCast(sphere,origin,direction,filter,hit);}
      else found=physics.rayCast(origin,direction,filter,hit);
      store(r,found,hit);
      // Comprimento livre em unidades locais: a escala do braço vale para os filhos.
      const float free=found?std::max(0.f,hit.distance-c.margin):worldLength;
      r.armLength=free*c.length/worldLength;
      for(const auto child:w.graph().childrenOf(id)) {
        const auto handle=w.handle(child);
        if(w.authorityOf(handle)!=TransformAuthority::Free) continue;
        Transform pose;if(w.localTransform(handle,pose)!=WorldStatus::Ok) continue;
        pose.position[0]=0;pose.position[1]=0;pose.position[2]=r.armLength;
        w.setLocalTransform(handle,pose);
      }
    }
  }
  void sync(GameWorld &w) {
    if(world_!=w.worldId()) {results_.clear();world_=w.worldId();revision_=~u64{0};}
    if(revision_!=w.structuralRevision()) {ids_.clear();w.graph().collectSubtree(w.graph().root(),ids_);revision_=w.structuralRevision();}
  }
  std::map<Key,Result> results_;
  u32 world_=0;u64 revision_=~u64{0};
  std::vector<ObjectId> ids_;
};
}
