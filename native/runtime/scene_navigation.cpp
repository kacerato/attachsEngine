#include "runtime/scene_navigation.h"

#include "runtime/scene_components.h"
#include "runtime/scene_physics.h"
#include "runtime/transform_math.h"
#include "scene/dynamic_body_motor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace ae::runtime {
namespace {
constexpr float Pi=3.14159265358979f;
u8 navArea(scene::NavArea area) {
  switch(area) {
    case scene::NavArea::NotWalkable: return navigation::AreaNotWalkable;
    case scene::NavArea::Jump: return navigation::AreaJump;
    case scene::NavArea::Difficult: return navigation::AreaDifficult;
    default: return navigation::AreaWalkable;
  }
}
void transformPoint(const float m[16],const float p[3],float out[3]) {
  for(u32 k=0;k<3;++k) out[k]=m[k]*p[0]+m[4+k]*p[1]+m[8+k]*p[2]+m[12+k];
}
bool descendantOf(const SceneGraph &graph,ObjectId id,ObjectId ancestor) {
  for(u32 guard=0;id&&guard<4096;++guard) {if(id==ancestor) return true;const auto *o=graph.find(id);id=o?o->parent:kInvalidObject;}
  return false;
}
// Modificador efetivo: o do próprio objeto, ou o do ancestral mais próximo que
// se aplica aos filhos.
const scene::NavModifier *effectiveModifier(const SceneGraph &graph,ObjectId id) {
  bool self=true;
  for(u32 guard=0;id&&guard<4096;++guard) {
    const auto *o=graph.find(id);if(!o) return nullptr;
    if(const auto *m=o->components.find(scene::NavModifier::descriptor)) {
      const auto &modifier=scene::navModifier(*m);
      if(self||modifier.applyToChildren) return &modifier;
    }
    self=false;id=o->parent;
  }
  return nullptr;
}
float wrapDegrees(float d) {d=std::fmod(d+180.f,360.f);if(d<0) d+=360.f;return d-180.f;}
float approachYaw(float current,float target,float maxStep) {
  const float delta=wrapDegrees(target-current);
  return current+std::clamp(delta,-maxStep,maxStep);
}
bool linksEqual(const std::vector<navigation::OffMeshLink> &a,const std::vector<navigation::OffMeshLink> &b) {
  if(a.size()!=b.size()) return false;
  for(usize i=0;i<a.size();++i)
    if(std::memcmp(a[i].start,b[i].start,sizeof(a[i].start))||std::memcmp(a[i].end,b[i].end,sizeof(a[i].end))||
       a[i].radius!=b[i].radius||a[i].bidirectional!=b[i].bidirectional||a[i].area!=b[i].area||a[i].id!=b[i].id) return false;
  return true;
}
navigation::AgentSettings agentSettings(const scene::NavAgent &a) {
  navigation::AgentSettings s;
  s.radius=a.radius;s.height=a.height;s.speed=a.speed;s.acceleration=a.acceleration;
  s.avoidance=static_cast<u32>(a.avoidance);s.separation=a.avoidance==scene::NavAvoidance::None?0.f:2.f;
  s.filter.include=static_cast<u16>(navigation::FlagWalk|(a.useJump?navigation::FlagJump:0)|(a.useDifficult?navigation::FlagDifficult:0));
  s.filter.jumpCost=a.jumpCost;s.filter.difficultCost=a.difficultCost;
  return s;
}
bool sameSettings(const navigation::AgentSettings &a,const navigation::AgentSettings &b) {
  return a.radius==b.radius&&a.height==b.height&&a.speed==b.speed&&a.acceleration==b.acceleration&&
         a.avoidance==b.avoidance&&a.separation==b.separation&&a.filter==b.filter;
}
bool insideBounds(const navigation::NavMeshData &d,const float p[3],float margin) {
  for(u32 k=0;k<3;++k) if(p[k]<d.boundsMin[k]-margin||p[k]>d.boundsMax[k]+margin) return false;
  return true;
}
} // namespace

navigation::BakeSettings navigationBakeSettings(const scene::NavSurface &s) {
  navigation::BakeSettings b;
  b.agentRadius=s.agentRadius;b.agentHeight=s.agentHeight;b.agentMaxClimb=s.agentMaxClimb;b.agentMaxSlope=s.agentMaxSlope;
  b.cellSize=s.cellSize;b.cellHeight=s.cellHeight;b.minRegionArea=s.minRegionArea;
  b.edgeMaxLength=s.edgeMaxLength;b.edgeMaxError=s.edgeMaxError;
  b.detailSampleDistance=s.detailSampleDistance;b.detailSampleMaxError=s.detailSampleMaxError;
  b.tileSize=static_cast<u32>(s.tileSize);
  return b;
}

bool collectNavigationGeometry(const GameWorld &world,const ScenePhysics &physics,ObjectId surfaceId,
                               navigation::BakeGeometry &out,std::string &error) {
  out={};
  const auto &graph=world.graph();
  const auto *owner=graph.find(surfaceId);
  const auto *component=owner?owner->components.find(scene::NavSurface::descriptor):nullptr;
  if(!component) {error="Objeto sem Superfície de navegação";return false;}
  const auto &surface=scene::navSurface(*component);
  std::vector<float> vertices;std::vector<ObjectId> objects;std::vector<u32> layers;
  if(!physics.collectStaticTriangles(vertices,objects,layers)) {error="Física da cena indisponível para o bake";return false;}
  if(surface.collect==scene::NavCollect::Volume) {
    float m[16];if(!worldMatrix(graph,surfaceId,m)) {error="Pose da superfície inválida";return false;}
    out.hasVolume=true;
    for(u32 k=0;k<3;++k) {out.volumeMin[k]=std::numeric_limits<float>::max();out.volumeMax[k]=-std::numeric_limits<float>::max();}
    for(u32 corner=0;corner<8;++corner) {
      const float local[3]{surface.volumeCenter[0]+(corner&1?.5f:-.5f)*surface.volumeSize[0],
                           surface.volumeCenter[1]+(corner&2?.5f:-.5f)*surface.volumeSize[1],
                           surface.volumeCenter[2]+(corner&4?.5f:-.5f)*surface.volumeSize[2]};
      float p[3];transformPoint(m,local,p);
      for(u32 k=0;k<3;++k) {out.volumeMin[k]=std::min(out.volumeMin[k],p[k]);out.volumeMax[k]=std::max(out.volumeMax[k],p[k]);}
    }
  }
  for(usize t=0;t<objects.size();++t) {
    const ObjectId object=objects[t];
    if(surface.layer&&layers[t]!=surface.layer-1) continue;
    if(surface.collect==scene::NavCollect::Children&&!descendantOf(graph,object,surfaceId)) continue;
    const auto *o=graph.find(object);
    // Obstáculos e agentes não são chão assado: recortam ou andam em execução.
    if(o&&(o->components.find(scene::NavObstacle::descriptor)||o->components.find(scene::NavAgent::descriptor))) continue;
    u8 area=navigation::AreaWalkable;
    if(const auto *modifier=effectiveModifier(graph,object)) {
      if(modifier->mode==scene::NavModifierMode::Ignore) continue;
      area=navArea(modifier->area);
    }
    const u32 base=static_cast<u32>(out.vertices.size()/3);
    out.vertices.insert(out.vertices.end(),vertices.begin()+static_cast<std::ptrdiff_t>(t*9),vertices.begin()+static_cast<std::ptrdiff_t>(t*9+9));
    out.indices.insert(out.indices.end(),{base,base+1,base+2});
    out.areas.push_back(area);
  }
  return true;
}

SceneNavigation::SceneNavigation()=default;
SceneNavigation::~SceneNavigation()=default;

void SceneNavigation::setMeshes(std::span<const navigation::NavMeshData> meshes) {
  meshes_.clear();
  for(const auto &m:meshes) meshes_.push_back(std::make_shared<const navigation::NavMeshData>(m));
  surfaces_.clear();agents_.clear();obstacles_.clear();
}
void SceneNavigation::reset() {
  surfaces_.clear();agents_.clear();obstacles_.clear();ids_.clear();revision_=~u64{0};world_=0;diagnostic_.clear();
}

void SceneNavigation::sync(GameWorld &w) {
  if(world_!=w.worldId()) {surfaces_.clear();agents_.clear();obstacles_.clear();world_=w.worldId();revision_=~u64{0};}
  if(revision_==w.structuralRevision()) return;
  ids_.clear();std::vector<ObjectId> all;w.graph().collectSubtree(w.graph().root(),all);
  for(const auto id:all) if(const auto *o=w.graph().find(id))
    if(o->components.find(scene::NavSurface::descriptor)||o->components.find(scene::NavAgent::descriptor)||
       o->components.find(scene::NavObstacle::descriptor)||o->components.find(scene::NavLink::descriptor)) ids_.push_back(id);
  revision_=w.structuralRevision();
}

SceneNavigation::Surface *SceneNavigation::surfaceOf(ObjectId id) {
  for(auto &[key,s]:surfaces_) if(key.first==id) return s.get();
  return nullptr;
}

SceneNavigation::Surface *SceneNavigation::surfaceFor(GameWorld &,const scene::NavAgent &settings,const float position[3]) {
  if(settings.surface) return surfaceOf(static_cast<ObjectId>(settings.surface));
  Surface *best=nullptr;float bestDistance=std::numeric_limits<float>::max();
  const float extents[3]{settings.radius*2+.5f,std::max(settings.height,2.f),settings.radius*2+.5f};
  for(auto &[key,s]:surfaces_) {
    float nearest[3];
    if(!s->world.nearest(position,extents,nearest)) continue;
    const float dx=nearest[0]-position[0],dy=nearest[1]-position[1],dz=nearest[2]-position[2];
    const float d=dx*dx+dy*dy+dz*dz;
    if(d<bestDistance) {bestDistance=d;best=s.get();}
  }
  return best;
}

void SceneNavigation::unbind(Agent &agent) {
  if(agent.crowd>=0) {
    const auto found=surfaces_.find(agent.surface);
    if(found!=surfaces_.end()) found->second->world.removeAgent(agent.crowd);
  }
  agent.crowd=-1;agent.onLink=false;
}

void SceneNavigation::emit(GameWorld &w,const Key &key,std::string_view event) {
  if(events_) events_->emit(w,key.first,key.second,scene::NavAgent::descriptor,event,{});
}

void SceneNavigation::updateLinks(GameWorld &w) {
  std::map<Key,std::vector<navigation::OffMeshLink>> per;
  for(const auto id:ids_) {
    const auto *o=w.graph().find(id);if(!o||!w.activeInHierarchy(w.handle(id))) continue;
    float m[16];bool hasMatrix=false;
    for(usize k=0;k<o->components.size();++k) {
      const auto *v=o->components.at(k);
      if(&v->type()!=&scene::NavLink::descriptor) continue;
      const auto &l=scene::navLink(*v);if(!l.enabled||!l.valid()) continue;
      if(!hasMatrix&&!(hasMatrix=worldMatrix(w.graph(),id,m))) break;
      navigation::OffMeshLink link;transformPoint(m,l.start,link.start);transformPoint(m,l.end,link.end);
      link.radius=l.radius;link.bidirectional=l.bidirectional;link.area=navArea(l.area);link.id=static_cast<u32>(id);
      for(auto &[key,s]:surfaces_)
        if(const auto *d=s->world.data();d&&insideBounds(*d,link.start,link.radius)) per[key].push_back(link);
    }
  }
  for(auto &[key,s]:surfaces_) {
    auto &next=per[key];
    if(linksEqual(next,s->links)) continue;
    s->world.setLinks(next);s->links=std::move(next);
  }
}

void SceneNavigation::updateObstacles(GameWorld &w,float seconds) {
  std::map<Key,Obstacle> seen;
  const auto release=[&](Obstacle &o) {
    for(const auto &[surface,ref]:o.refs) {const auto s=surfaces_.find(surface);if(s!=surfaces_.end()) s->second->world.removeObstacle(ref);}
    o.refs.clear();
  };
  for(const auto id:ids_) {
    const auto *object=w.graph().find(id);if(!object) continue;
    const auto *v=object->components.find(scene::NavObstacle::descriptor);if(!v) continue;
    const Key key{id,v->instanceId()};
    auto node=obstacles_.extract(key);Obstacle o=node.empty()?Obstacle{}:std::move(node.mapped());
    const auto settings=scene::navObstacle(*v);
    // Superfícies recarregadas perderam os recortes: o obstáculo é recolocado.
    for(const auto &[surface,ref]:o.refs) if(!surfaces_.count(surface)) {o.refs.clear();o.placed=false;break;}
    Transform t;float m[16];
    if(!settings.enabled||!settings.valid()||!w.activeInHierarchy(w.handle(id))||
       w.worldTransform(w.handle(id),t)!=WorldStatus::Ok||!worldMatrix(w.graph(),id,m)) {release(o);o.placed=false;seen.emplace(key,std::move(o));continue;}
    float center[3];transformPoint(m,settings.center,center);
    const float yaw=t.rotationDegrees[1]*Pi/180.f;
    const float sx=std::abs(t.scale[0]),sy=std::abs(t.scale[1]),sz=std::abs(t.scale[2]);
    float half[3]{settings.size[0]*sx*.5f,settings.size[1]*sy*.5f,settings.size[2]*sz*.5f};
    const float radius=settings.radius*std::max(sx,sz),height=settings.height*sy;
    u64 shape=static_cast<u64>(settings.shape)+1;
    for(const float f:{half[0],half[1],half[2],radius,height}) {u32 bits;std::memcpy(&bits,&f,4);shape=shape*1099511628211ull^bits;}
    const float dx=center[0]-o.pose[0],dy=center[1]-o.pose[1],dz=center[2]-o.pose[2];
    const bool moved=o.placed&&(std::sqrt(dx*dx+dy*dy+dz*dz)>settings.moveThreshold||std::abs(wrapDegrees((yaw-o.pose[3])*180.f/Pi))>2.f||shape!=o.shape);
    const auto place=[&]{
      release(o);bool complete=true;
      for(auto &[surfaceKey,s]:surfaces_) {
        const auto *d=s->world.data();if(!d) continue;
        const float reach=settings.shape==scene::NavObstacleShape::Box?std::sqrt(half[0]*half[0]+half[2]*half[2]):radius;
        if(!insideBounds(*d,center,reach+std::max(half[1],height))) continue;
        u32 ref=0;
        if(settings.shape==scene::NavObstacleShape::Box) ref=s->world.addBoxObstacle(center,half,yaw);
        else {const float base[3]{center[0],center[1]-height*.5f,center[2]};ref=s->world.addCylinderObstacle(base,radius,height);}
        if(ref) o.refs.emplace_back(surfaceKey,ref);else complete=false;
      }
      return complete;
    };
    if(!o.placed||(moved&&!settings.carveOnlyStationary)) {
      o.placed=place();o.moving=false;o.still=0;
    } else if(moved) {
      release(o);o.moving=true;o.still=0;
    } else if(o.moving) {
      o.still+=seconds;
      if(o.still>=settings.stationaryTime) {place();o.moving=false;}
    }
    if(!o.moving) {o.pose[0]=center[0];o.pose[1]=center[1];o.pose[2]=center[2];o.pose[3]=yaw;o.shape=shape;}
    else if(moved) {o.pose[0]=center[0];o.pose[1]=center[1];o.pose[2]=center[2];o.pose[3]=yaw;o.shape=shape;}
    seen.emplace(key,std::move(o));
  }
  for(auto &[key,o]:obstacles_) release(o);
  obstacles_=std::move(seen);
  for(auto &[key,s]:surfaces_) {bool upToDate=true;s->world.updateObstacles(upToDate);}
}

bool SceneNavigation::advance(GameWorld &w,float seconds) {
  if(!w.running()||!std::isfinite(seconds)||seconds<0) return false;
  sync(w);
  // Superfícies: uma malha carregada por instância ativa com recurso.
  std::map<Key,std::unique_ptr<Surface>> live;
  for(const auto id:ids_) {
    const auto *o=w.graph().find(id);if(!o) continue;
    const auto *v=o->components.find(scene::NavSurface::descriptor);if(!v) continue;
    const Key key{id,v->instanceId()};
    const auto &s=scene::navSurface(*v);
    auto node=surfaces_.extract(key);
    if(!s.enabled||!s.data.valid()||!w.activeInHierarchy(w.handle(id))) continue;
    if(!node.empty()&&node.mapped()->guid==s.data) {live.insert(std::move(node));continue;}
    const auto mesh=std::find_if(meshes_.begin(),meshes_.end(),[&](const auto &m){return m->guid==s.data;});
    if(mesh==meshes_.end()) {diagnostic_="Superfície \""+std::string(o->name)+"\" sem malha assada carregada; faça o bake";continue;}
    auto surface=std::make_unique<Surface>();surface->key=key;surface->guid=s.data;
    std::string error;
    if(!surface->world.load(**mesh,{},128,error)) {diagnostic_="Malha de \""+std::string(o->name)+"\" recusada: "+error;continue;}
    live.emplace(key,std::move(surface));
  }
  // Agentes ligados a uma superfície que saiu perderam o índice da multidão.
  for(auto &[key,a]:agents_) if(!live.count(a.surface)) {a.crowd=-1;a.onLink=false;}
  surfaces_=std::move(live);
  updateLinks(w);
  updateObstacles(w,seconds);

  std::map<Key,Agent> seen;
  std::vector<std::pair<Key,scene::NavAgent>> active;
  for(const auto id:ids_) {
    const auto *o=w.graph().find(id);if(!o) continue;
    const auto *v=o->components.find(scene::NavAgent::descriptor);if(!v) continue;
    const Key key{id,v->instanceId()};
    auto node=agents_.extract(key);Agent a=node.empty()?Agent{}:std::move(node.mapped());
    const auto settings=scene::navAgent(*v);
    const auto handle=w.handle(id);
    if(!settings.enabled||!settings.valid()||!w.activeInHierarchy(handle)) {
      unbind(a);if(physics_) physics_->releaseMotorControl(id,MotorControlSource::Ai);
      seen.emplace(key,std::move(a));continue;
    }
    Transform t;if(w.worldTransform(handle,t)!=WorldStatus::Ok) {seen.emplace(key,std::move(a));continue;}
    auto it=seen.emplace(key,std::move(a)).first;auto &agent=it->second;
    if(agent.crowd<0) {
      Surface *s=surfaceFor(w,settings,t.position);
      if(!s) {if(!surfaces_.empty()) diagnostic_="Agente \""+std::string(o->name)+"\" fora de qualquer malha de navegação";continue;}
      agent.surface=s->key;agent.settings=agentSettings(settings);
      agent.crowd=s->world.addAgent(t.position,agent.settings);
      if(agent.crowd<0) {diagnostic_="Multidão cheia ou agente fora da malha: "+std::string(o->name);continue;}
      agent.drive=physics_&&physics_->hasCharacter(id)?Drive::Character:physics_&&physics_->hasDynamicMotor(id)?Drive::DynamicMotor:
                  physics_&&physics_->hasMovingBody(id)?Drive::Body:Drive::Pose;
      if(agent.hasDestination&&!agent.stopped&&!s->world.setDestination(agent.crowd,agent.destination)) {agent.failed=true;emit(w,key,"path_failed");}
    }
    auto &world=surfaces_.at(agent.surface)->world;
    const auto desired=agentSettings(settings);
    if(!sameSettings(desired,agent.settings)&&world.updateAgent(agent.crowd,desired)) agent.settings=desired;
    // A física moveu o objeto no passo anterior; um script pode ter movido a pose.
    navigation::AgentState state;
    if(world.agentState(agent.crowd,state)) {
      float feet[3]{t.position[0],t.position[1],t.position[2]};
      if(agent.drive==Drive::Pose) feet[1]-=settings.baseOffset;
      const float dx=feet[0]-state.position[0],dz=feet[2]-state.position[2];
      if(agent.drive!=Drive::Pose||dx*dx+dz*dz>.25f) world.syncAgent(agent.crowd,feet);
    }
    if(settings.target&&!agent.scriptOverride) {
      Transform target;
      if(w.worldTransform(w.handle(static_cast<ObjectId>(settings.target)),target)==WorldStatus::Ok) {
        const float dx=target.position[0]-agent.chasedAt[0],dy=target.position[1]-agent.chasedAt[1],dz=target.position[2]-agent.chasedAt[2];
        if(!agent.chasing||std::sqrt(dx*dx+dy*dy+dz*dz)>settings.repathDistance) {
          std::memcpy(agent.chasedAt,target.position,sizeof(agent.chasedAt));agent.chasing=true;
          std::memcpy(agent.destination,target.position,sizeof(agent.destination));agent.hasDestination=true;
          agent.reached=false;agent.failed=false;agent.stopped=false;
          if(!world.setDestination(agent.crowd,agent.destination)) {agent.failed=true;emit(w,key,"path_failed");}
        }
      }
    }
    // Frear ao chegar: a velocidade máxima cai linearmente na distância em que
    // a aceleração consegue parar, terminando na distância de parada.
    if(navigation::AgentState now;world.agentState(agent.crowd,now)&&now.hasPath&&!now.onLink) {
      float speed=settings.speed;
      if(settings.autoBraking) {
        const float remaining=world.remainingDistance(agent.crowd);
        const float brake=std::max(settings.radius*2,settings.acceleration>0?settings.speed*settings.speed/(2*settings.acceleration):settings.radius*2);
        speed=settings.speed*std::clamp((remaining-settings.stoppingDistance)/brake,.08f,1.f);
      }
      world.setAgentSpeed(agent.crowd,speed);
    }
    active.emplace_back(key,settings);
  }
  for(auto &[key,a]:agents_) unbind(a);   // componentes removidos
  agents_=std::move(seen);
  for(auto &[key,s]:surfaces_) s->world.advance(seconds);
  for(const auto &[key,settings]:active) {
    auto found=agents_.find(key);
    if(found!=agents_.end()&&found->second.crowd>=0) driveAgent(w,key,found->second,settings,seconds);
  }
  return true;
}

void SceneNavigation::driveAgent(GameWorld &w,const Key &key,Agent &a,const scene::NavAgent &settings,float seconds) {
  const ObjectId id=key.first;
  const auto surface=surfaces_.find(a.surface);
  if(surface==surfaces_.end()) return;
  auto &world=surface->second->world;
  navigation::AgentState state;
  if(!world.agentState(a.crowd,state)) return;
  if(state.status==navigation::PathStatus::Invalid&&a.hasDestination&&!a.failed) {a.failed=true;emit(w,key,"path_failed");}
  if(state.onLink&&!a.onLink) emit(w,key,"link_entered");
  a.onLink=state.onLink;
  float velocity[3]{state.velocity[0],state.velocity[1],state.velocity[2]};
  if(state.hasPath&&!state.pending&&!state.onLink) {
    const float remaining=world.remainingDistance(a.crowd);
    // Distância de parada zero ainda tolera o resto que um motor físico deixa.
    if(remaining<=std::max(settings.stoppingDistance,std::min(.25f,settings.radius*.5f))) {
      world.stopAgent(a.crowd);velocity[0]=velocity[1]=velocity[2]=0;
      if(!a.reached) {a.reached=true;emit(w,key,"destination_reached");}
    } else if(!settings.autoBraking) {
      const float h=std::hypot(state.desiredVelocity[0],state.desiredVelocity[2]);
      if(h>1e-3f) {velocity[0]=state.desiredVelocity[0]/h*settings.speed;velocity[2]=state.desiredVelocity[2]/h*settings.speed;}
    }
  }
  if(a.stopped) velocity[0]=velocity[1]=velocity[2]=0;
  const auto handle=w.handle(id);
  Transform t;if(w.worldTransform(handle,t)!=WorldStatus::Ok) return;
  const float horizontal=std::hypot(velocity[0],velocity[2]);
  float yaw=t.rotationDegrees[1];
  if(settings.updateRotation&&horizontal>.05f)
    yaw=approachYaw(yaw,std::atan2(velocity[0],velocity[2])*180.f/Pi,settings.angularSpeed*seconds);
  const auto *object=w.graph().find(id);
  switch(a.drive) {
    case Drive::Character:
    case Drive::DynamicMotor: {
      if(!physics_||!object) return;
      float speed=0;
      if(a.drive==Drive::Character) {if(const auto *c=characterComponent(*object)) speed=c->speed;}
      else if(const auto *m=object->components.find(scene::DynamicBodyMotor::descriptor)) speed=static_cast<const scene::DynamicBodyMotor &>(*m).speed;
      if(state.onLink&&a.drive==Drive::Character) {
        physics_->submitMotorControl(id,MotorControlSource::Ai,0,0,0);
        physics_->teleportCharacter(id,state.position,w);
        break;
      }
      float right=speed>0?velocity[0]/speed:0,forward=speed>0?velocity[2]/speed:0;
      const float magnitude=std::hypot(right,forward);
      if(magnitude>1) {right/=magnitude;forward/=magnitude;}
      physics_->submitMotorControl(id,MotorControlSource::Ai,right,forward,0);
      if(a.drive==Drive::Character&&settings.updateRotation) physics_->setCharacterYaw(id,yaw*Pi/180.f);
      break;
    }
    case Drive::Body: {
      if(!physics_) return;
      float current[3]{};physics_->getBodyVelocity(id,current);
      float next[3]{velocity[0],current[1],velocity[2]};
      if(state.onLink&&seconds>0) for(u32 k=0;k<3;++k) next[k]=(state.position[k]-t.position[k])/seconds;
      physics_->setBodyVelocity(id,next);
      break;
    }
    case Drive::Pose: {
      t.position[0]=state.position[0];t.position[1]=state.position[1]+settings.baseOffset;t.position[2]=state.position[2];
      if(settings.updateRotation) t.rotationDegrees[1]=yaw;
      if(w.setWorldTransform(handle,t)==WorldStatus::TransformOwnedByPhysics)
        diagnostic_="Agente \""+std::string(object?object->name:"")+"\" não move a própria pose: um filho tem corpo físico; mova o corpo para o agente ou tire-o do filho";
      break;
    }
    case Drive::None: break;
  }
}

WorldStatus SceneNavigation::command(GameWorld &w,ComponentHandle h,std::string_view method,
                                     std::span<const scene::ComponentOperationValue> arguments,scene::ComponentOperationValue &out) {
  using V=scene::ComponentOperationValue;
  const auto valid=w.validate(h.object);if(valid!=WorldStatus::Ok) return valid;
  const auto *v=w.readComponent(h);if(!v) return WorldStatus::ComponentMissing;
  const Key key{h.object.id,h.instance};
  if(&v->type()==&scene::NavSurface::descriptor) {
    const auto s=surfaces_.find(key);
    if(method=="is_ready") {out=V::makeBoolean(s!=surfaces_.end()&&s->second->world.loaded());return WorldStatus::Ok;}
    if(method=="polygon_count") {out=V::makeInteger(s==surfaces_.end()?0:s->second->world.stats().polygons);return WorldStatus::Ok;}
    return WorldStatus::InvalidArgument;
  }
  if(&v->type()==&scene::NavObstacle::descriptor) {
    if(method!="is_carving") return WorldStatus::InvalidArgument;
    const auto o=obstacles_.find(key);
    out=V::makeBoolean(o!=obstacles_.end()&&!o->second.refs.empty()&&!o->second.moving);return WorldStatus::Ok;
  }
  if(&v->type()!=&scene::NavAgent::descriptor) return WorldStatus::ComponentMissing;
  auto &a=agents_[key];
  const auto settings=scene::navAgent(*v);
  auto surface=surfaces_.find(a.surface);
  auto *world=a.crowd>=0&&surface!=surfaces_.end()?&surface->second->world:nullptr;
  const auto point=[&](float p[3]) {
    if(arguments.empty()||arguments[0].valueKind()!=scene::ComponentValueKind::Vector3) return false;
    for(u32 k=0;k<3;++k) p[k]=arguments[0].vector[k];
    return std::isfinite(p[0])&&std::isfinite(p[1])&&std::isfinite(p[2]);
  };
  if(method=="set_destination") {
    float p[3];if(!point(p)) return WorldStatus::InvalidArgument;
    std::memcpy(a.destination,p,sizeof(p));a.hasDestination=true;a.stopped=false;a.reached=false;a.failed=false;a.scriptOverride=true;
    // Sem malha ainda (Start antes do primeiro quadro): o pedido fica guardado.
    if(!world) {out=V::makeBoolean(true);return WorldStatus::Ok;}
    const bool ok=world->setDestination(a.crowd,p);
    if(!ok) {a.failed=true;emit(w,key,"path_failed");}
    out=V::makeBoolean(ok);return WorldStatus::Ok;
  }
  if(method=="stop") {a.stopped=true;if(world) world->stopAgent(a.crowd);return WorldStatus::Ok;}
  if(method=="resume") {
    a.stopped=false;a.reached=false;
    if(world&&a.hasDestination&&!world->setDestination(a.crowd,a.destination)) {a.failed=true;emit(w,key,"path_failed");}
    return WorldStatus::Ok;
  }
  if(method=="warp") {
    float p[3];if(!point(p)) return WorldStatus::InvalidArgument;
    if(!world) {out=V::makeBoolean(false);return WorldStatus::Ok;}
    float nearest[3];const float extents[3]{2,4,2};
    if(!world->nearest(p,extents,nearest)) {out=V::makeBoolean(false);return WorldStatus::Ok;}
    const auto handle=w.handle(h.object.id);
    if(a.drive==Drive::Character) {if(!physics_||!physics_->teleportCharacter(h.object.id,nearest,w)) return WorldStatus::Rejected;}
    else if(a.drive==Drive::Pose) {
      Transform t;if(w.worldTransform(handle,t)!=WorldStatus::Ok) return WorldStatus::Rejected;
      t.position[0]=nearest[0];t.position[1]=nearest[1]+settings.baseOffset;t.position[2]=nearest[2];
      if(w.setWorldTransform(handle,t)!=WorldStatus::Ok) return WorldStatus::Rejected;
    } else return WorldStatus::Rejected;   // corpo dinâmico: a física decide a pose
    world->syncAgent(a.crowd,nearest);
    if(a.hasDestination&&!a.stopped) world->setDestination(a.crowd,a.destination);
    out=V::makeBoolean(true);return WorldStatus::Ok;
  }
  navigation::AgentState state;
  const bool known=world&&world->agentState(a.crowd,state);
  if(method=="remaining_distance") {out=V::makeNumber(known&&state.hasPath?world->remainingDistance(a.crowd):std::numeric_limits<double>::infinity());return WorldStatus::Ok;}
  if(method=="path_status") {out=V::makeInteger(a.failed?3:known?static_cast<i64>(state.status):0);return WorldStatus::Ok;}
  if(method=="has_path") {out=V::makeBoolean(known&&state.hasPath);return WorldStatus::Ok;}
  if(method=="is_on_link") {out=V::makeBoolean(known&&state.onLink);return WorldStatus::Ok;}
  if(method=="velocity") {out=known?V::makeVector(state.velocity[0],state.velocity[1],state.velocity[2]):V::makeVector(0,0,0);return WorldStatus::Ok;}
  return WorldStatus::InvalidArgument;
}

const navigation::NavWorld *SceneNavigation::surfaceWorld(ObjectId surface) const {
  for(const auto &[key,s]:surfaces_) if(key.first==surface) return &s->world;
  return nullptr;
}

std::vector<SceneNavigation::AgentView> SceneNavigation::agents() const {
  std::vector<AgentView> out;
  for(const auto &[key,a]:agents_) {
    const auto s=surfaces_.find(a.surface);if(a.crowd<0||s==surfaces_.end()) continue;
    navigation::AgentState state;if(!s->second->world.agentState(a.crowd,state)) continue;
    AgentView view;view.id=key.first;view.drive=a.drive;view.hasPath=state.hasPath;view.onLink=state.onLink;view.status=state.status;
    std::memcpy(view.position,state.position,sizeof(view.position));std::memcpy(view.target,state.target,sizeof(view.target));
    if(state.hasPath) {
      std::vector<float> corners;navigation::AgentFilter filter=a.settings.filter;
      s->second->world.findPath(state.position,state.target,filter,corners);
      view.corners=std::move(corners);
    }
    out.push_back(std::move(view));
  }
  return out;
}

} // namespace ae::runtime
