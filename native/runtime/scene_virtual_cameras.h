#pragma once
#include "runtime/component_operations.h"
#include "runtime/game_world.h"
#include "runtime/scene_physics.h"
#include "runtime/transform_math.h"
#include "scene/camera.h"
#include "scene/physics_body.h"
#include "scene/virtual_camera.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

namespace ae::runtime {
// Avaliador de Play das Câmeras virtuais e dos Cérebros (bloco G).
//
// Ordem no quadro: depois da física, de LateUpdate, dos percursos, tweens,
// constraints e de Acompanhar alvo, para ler as poses finais dos alvos.
// 1. Cada câmera virtual ativa calcula a pose CRUA (posição + rotação) e a
//    grava no próprio objeto, como a CinemachineCamera faz com o transform.
//    Desoclusão, inclinação e tremor são CORREÇÕES: entram só no estado
//    entregue ao Cérebro e nunca realimentam o objeto.
// 2. Cada Cérebro escolhe a câmera de maior prioridade (empate: a ativada por
//    último; depois o menor ID), mistura do estado anterior para o novo e
//    escreve pose e lente na Câmera do próprio objeto.
class SceneVirtualCameras final {
public:
  struct CameraState {
    float position[3]{};
    float rotation[4]{0,0,0,1};
    float verticalFov=60,orthographicHalfHeight=5,nearPlane=.1f,farPlane=2000;
  };
  struct Brain {
    ObjectId live=kInvalidObject,blendFrom=kInvalidObject;
    bool blending=false,hasOutput=false;
    scene::CameraBlendStyle style=scene::CameraBlendStyle::Cut;
    float elapsed=0,duration=0;
    CameraState frozen,output;
    float progress() const noexcept {return blending&&duration>0?std::min(1.f,elapsed/duration):1.f;}
  };
  using Key=std::pair<ObjectId,u64>;

  void reset() {rigs_.clear();brains_.clear();ids_.clear();revision_=~u64{0};world_=0;stamp_=0;started_=false;}
  void setEvents(ComponentEventQueue *events) noexcept {events_=events;}
  const Brain *brain(ObjectId id) const {for(const auto &[key,b]:brains_) if(key.first==id) return &b;return nullptr;}
  bool live(ObjectId id) const {for(const auto &[key,b]:brains_) if(b.live==id&&id) return true;return false;}
  const CameraState *state(ObjectId id) const {for(const auto &[key,r]:rigs_) if(key.first==id&&r.valid) return &r.state;return nullptr;}

  // `look`: eixo da ação Olhar neste quadro (delta normalizado pela tela).
  bool advance(GameWorld &w,const ScenePhysics *physics,const float look[2]) {
    if(!w.running()) return false;
    sync(w);
    bool unscaled=false;
    for(const auto id:ids_) if(const auto *o=w.graph().find(id))
      if(const auto *b=o->components.find(scene::CameraBrain::descriptor);b&&scene::cameraBrain(*b).enabled&&w.activeInHierarchy(w.handle(id)))
        {unscaled=scene::cameraBrain(*b).ignoreTimeScale;break;}
    const float delta=unscaled?w.clock().unscaledDelta():w.clock().delta();
    if(!std::isfinite(delta)||delta<0) return false;

    std::map<Key,Rig> seenRigs;
    for(const auto id:ids_) {
      const auto *o=w.graph().find(id);if(!o) continue;
      for(usize k=0;k<o->components.size();++k) {
        const auto *v=o->components.at(k);
        if(&v->type()!=&scene::VirtualCamera::descriptor) continue;
        const Key key{id,v->instanceId()};
        auto node=rigs_.extract(key);Rig rig=node.empty()?Rig{}:std::move(node.mapped());
        const scene::VirtualCamera c=scene::virtualCamera(*v);  // cópia: a órbita escreve no componente
        const bool eligible=c.enabled&&c.valid()&&w.activeInHierarchy(w.handle(id));
        if(eligible&&!rig.eligible) rig.stamp=started_?++stamp_:0;
        if(!eligible) rig.previousValid=false;
        rig.eligible=eligible;
        if(eligible) evaluate(w,physics,id,key,c,rig,delta,look);
        seenRigs.emplace(key,std::move(rig));
      }
    }
    rigs_=std::move(seenRigs);

    std::map<Key,Brain> seenBrains;
    for(const auto id:ids_) {
      const auto *o=w.graph().find(id);if(!o) continue;
      const auto *value=o->components.find(scene::CameraBrain::descriptor);if(!value) continue;
      const Key key{id,value->instanceId()};
      auto node=brains_.extract(key);Brain b=node.empty()?Brain{}:std::move(node.mapped());
      const auto settings=scene::cameraBrain(*value);
      if(settings.enabled&&settings.valid()&&w.activeInHierarchy(w.handle(id))&&!driveBrain(w,id,key,settings,b,delta)) return false;
      seenBrains.emplace(key,std::move(b));
    }
    brains_=std::move(seenBrains);
    started_=true;
    return true;
  }

  // Métodos dos descritores.
  WorldStatus command(GameWorld &w,ComponentHandle h,std::string_view method,scene::ComponentOperationValue &out) {
    const auto valid=w.validate(h.object);if(valid!=WorldStatus::Ok) return valid;
    const auto *v=w.readComponent(h);if(!v) return WorldStatus::ComponentMissing;
    using V=scene::ComponentOperationValue;
    if(&v->type()==&scene::CameraBrain::descriptor) {
      const auto i=brains_.find({h.object.id,h.instance});
      if(method=="live_camera") out=V::makeObject(i==brains_.end()?0:i->second.live);
      else if(method=="blending") out=V::makeBoolean(i!=brains_.end()&&i->second.blending);
      else return WorldStatus::InvalidArgument;
      return WorldStatus::Ok;
    }
    auto &rig=rigs_[{h.object.id,h.instance}];
    if(method=="prioritize") rig.stamp=++stamp_;
    else if(method=="snap") rig.previousValid=false;
    else if(method=="is_live") out=V::makeBoolean(live(h.object.id));
    else return WorldStatus::InvalidArgument;
    return WorldStatus::Ok;
  }

  // Matemática compartilhada com os testes.
  static void multiply(const float a[4],const float b[4],float out[4]) {
    const float r[4]{a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
                     a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};
    std::copy(r,r+4,out);
  }
  static void axisAngle(u32 axis,float radians,float out[4]) {out[0]=out[1]=out[2]=0;out[axis]=std::sin(radians*.5f);out[3]=std::cos(radians*.5f);}
  // Yaw em Y, pitch em X (positivo olha para baixo), roll em Z local: +Z é a frente.
  static void yawPitchRoll(float yaw,float pitch,float roll,float out[4]) {
    float y[4],p[4],r[4],t[4];axisAngle(1,yaw,y);axisAngle(0,pitch,p);axisAngle(2,roll,r);
    multiply(y,p,t);multiply(t,r,out);
  }
  static void rotate(const float q[4],const float v[3],float out[3]) {
    const float t[3]{2*(q[1]*v[2]-q[2]*v[1]),2*(q[2]*v[0]-q[0]*v[2]),2*(q[0]*v[1]-q[1]*v[0])};
    const float r[3]{v[0]+q[3]*t[0]+q[1]*t[2]-q[2]*t[1],v[1]+q[3]*t[1]+q[2]*t[0]-q[0]*t[2],v[2]+q[3]*t[2]+q[0]*t[1]-q[1]*t[0]};
    std::copy(r,r+3,out);
  }
  static void slerp(const float a[4],const float b[4],float t,float out[4]) {
    float d=a[0]*b[0]+a[1]*b[1]+a[2]*b[2]+a[3]*b[3];float s=1;
    if(d<0) {d=-d;s=-1;}
    float wa=1-t,wb=t;
    if(d<.9995f) {const float angle=std::acos(std::min(1.f,d)),sine=std::sin(angle);wa=std::sin((1-t)*angle)/sine;wb=std::sin(t*angle)/sine;}
    float n=0;for(u32 i=0;i<4;++i) {out[i]=wa*a[i]+s*wb*b[i];n+=out[i]*out[i];}
    n=std::sqrt(n);for(u32 i=0;i<4;++i) out[i]/=n;
  }
  static void lookRotation(const float forward[3],float roll,float out[4]) {
    const float n=std::sqrt(forward[0]*forward[0]+forward[1]*forward[1]+forward[2]*forward[2]);
    yawPitchRoll(std::atan2(forward[0],forward[2]),std::asin(std::clamp(-forward[1]/n,-1.f,1.f)),roll,out);
  }
  // Rolagem em torno da frente, medida contra a orientação sem rolagem (up do mundo).
  static float rollOf(const float q[4]) {
    const float z[3]{0,0,1},y[3]{0,1,0},x[3]{1,0,0};float f[3],u[3],base[4],up0[3],right0[3];
    rotate(q,z,f);rotate(q,y,u);lookRotation(f,0,base);rotate(base,y,up0);rotate(base,x,right0);
    return std::atan2(-(u[0]*right0[0]+u[1]*right0[1]+u[2]*right0[2]),u[0]*up0[0]+u[1]*up0[1]+u[2]*up0[2]);
  }
  // Mistura de rotação sem rolagem espúria: a direção de visão gira pelo arco
  // entre as duas e a rolagem é interpolada à parte (SlerpWithReferenceUp da
  // Cinemachine). Um slerp direto entre olhares de guinadas e inclinações
  // diferentes entorta o horizonte no meio da transição.
  static void blendRotation(const float a[4],const float b[4],float t,float out[4]) {
    const float z[3]{0,0,1};float fa[3],fb[3];rotate(a,z,fa);rotate(b,z,fb);
    const float d=std::clamp(fa[0]*fb[0]+fa[1]*fb[1]+fa[2]*fb[2],-1.f,1.f);
    if(d<-.9999f) {slerp(a,b,t,out);return;}                     // opostas: o arco não é único
    float f[3];
    if(d>.9999f) for(u32 i=0;i<3;++i) f[i]=fa[i]+(fb[i]-fa[i])*t;
    else {const float angle=std::acos(d),s=std::sin(angle),wa=std::sin((1-t)*angle)/s,wb=std::sin(t*angle)/s;for(u32 i=0;i<3;++i) f[i]=wa*fa[i]+wb*fb[i];}
    const float ra=rollOf(a),rb=rollOf(b);
    lookRotation(f,ra+std::remainder(rb-ra,6.28318530718f)*t,out);
  }
  // Pose de mundo na convenção do transform (Euler Rz·Ry·Rx), escala preservada.
  static bool poseTransform(const float position[3],const float q[4],const float scale[3],Transform &out) {
    const float x=q[0],y=q[1],z=q[2],s=q[3];
    const float m[16]{1-2*(y*y+z*z),2*(x*y+s*z),2*(x*z-s*y),0, 2*(x*y-s*z),1-2*(x*x+z*z),2*(y*z+s*x),0,
                      2*(x*z+s*y),2*(y*z-s*x),1-2*(x*x+y*y),0, position[0],position[1],position[2],1};
    const float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    if(!localTransformForWorld(m,identity,out)) return false;
    std::copy(scale,scale+3,out.scale);return true;
  }

private:
  struct Rig {
    bool eligible=false,previousValid=false,valid=false;
    u64 stamp=0;
    float pivot[3]{},rotation[4]{0,0,0,1},collision=-1,noiseTime=0;
    CameraState state;
  };
  static float smoothing(float delta,float seconds) {return seconds<=0?1.f:static_cast<float>(1.0-std::exp(-static_cast<double>(delta)/seconds));}
  static ObjectId bodyOwner(const GameWorld &w,ObjectId id) {
    for(ObjectId p=id;p;) {const auto *o=w.graph().find(p);if(!o) break;if(o->components.find(scene::PhysicsBody::descriptor)) return p;p=o->parent;}
    return kInvalidObject;
  }
  bool targetPose(GameWorld &w,u64 target,Transform &out) const {
    if(!target) return false;
    const auto handle=w.handle(static_cast<ObjectId>(target));
    return w.activeInHierarchy(handle)&&w.worldTransform(handle,out)==WorldStatus::Ok;
  }
  void evaluate(GameWorld &w,const ScenePhysics *physics,ObjectId id,const Key &key,const scene::VirtualCamera &c,Rig &rig,float delta,const float look[2]) {
    const auto handle=w.handle(id);
    Transform own{};if(w.worldTransform(handle,own)!=WorldStatus::Ok) {rig.valid=false;return;}
    float ownRotation[4];transformRotationQuaternion(own,ownRotation);
    bool pathWriter=false;
    if(const auto *o=w.graph().find(id)) for(usize k=0;k<o->components.size();++k) {
      const auto *v=o->components.at(k);if(v->type().id!="astra.path.follow") continue;
      for(const auto &p:v->type().booleans) if(p.id=="enabled") pathWriter|=p.read(*v);
    }
    const bool snap=!rig.previousValid;
    Transform tracked{},aimed{};
    const bool hasTracked=targetPose(w,c.trackingTarget,tracked);
    const bool hasAim=c.lookAtTarget?targetPose(w,c.lookAtTarget,aimed):(aimed=tracked,hasTracked);

    // Órbita: a ação Olhar gira os ângulos enquanto a câmera está ao vivo; o
    // valor fica no componente, onde scripts e tweens também o leem.
    float yaw=c.orbitYaw,pitch=c.orbitPitch;
    if(c.position==scene::VirtualCameraPosition::Orbit&&c.orbitInput&&look&&live(id)&&(look[0]!=0||look[1]!=0)) {
      yaw=std::remainder(yaw+look[0]*c.orbitYawSensitivity,360.f);
      pitch=std::clamp(pitch+look[1]*c.orbitPitchSensitivity,c.orbitPitchMin,c.orbitPitchMax);
      const ComponentHandle component{handle,key.second};
      w.setTweenNumber(component,"orbit_yaw",yaw);w.setTweenNumber(component,"orbit_pitch",pitch);
    }
    pitch=std::clamp(pitch,c.orbitPitchMin,c.orbitPitchMax);

    float position[3]{own.position[0],own.position[1],own.position[2]};
    const bool drivesPosition=c.position!=scene::VirtualCameraPosition::Authored&&hasTracked&&!pathWriter;
    if(drivesPosition) {
      const float a=snap?1.f:smoothing(delta,c.positionDamping);
      for(u32 i=0;i<3;++i) rig.pivot[i]+=(tracked.position[i]-rig.pivot[i])*a;
      float offset[3];
      if(c.position==scene::VirtualCameraPosition::Follow) {
        std::copy(c.followOffset,c.followOffset+3,offset);
        if(c.binding==scene::VirtualCameraBinding::TargetYaw) {
          float q[4],f[3];const float z[3]{0,0,1};transformRotationQuaternion(tracked,q);rotate(q,z,f);
          float yawOnly[4];axisAngle(1,std::atan2(f[0],f[2]),yawOnly);rotate(yawOnly,c.followOffset,offset);
        }
      } else {
        float q[4],f[3];const float z[3]{0,0,1};
        yawPitchRoll(yaw*.0174532925f,pitch*.0174532925f,0,q);rotate(q,z,f);
        for(u32 i=0;i<3;++i) offset[i]=-c.orbitRadius*f[i];
      }
      for(u32 i=0;i<3;++i) position[i]=rig.pivot[i]+offset[i];
    } else std::copy(position,position+3,rig.pivot);

    float rotation[4]{ownRotation[0],ownRotation[1],ownRotation[2],ownRotation[3]};
    float aimPoint[3]{};bool aiming=false;
    if(c.rotation==scene::VirtualCameraRotation::LookAt&&hasAim) {
      for(u32 i=0;i<3;++i) aimPoint[i]=aimed.position[i]+c.aimOffset[i];
      const float forward[3]{aimPoint[0]-position[0],aimPoint[1]-position[1],aimPoint[2]-position[2]};
      if(forward[0]*forward[0]+forward[1]*forward[1]+forward[2]*forward[2]>1e-8f) {lookRotation(forward,0,rotation);aiming=true;}
      else std::copy(rig.rotation,rig.rotation+4,rotation);
    } else if(c.rotation==scene::VirtualCameraRotation::TargetRotation&&hasTracked) transformRotationQuaternion(tracked,rotation);
    const bool drivesRotation=c.rotation!=scene::VirtualCameraRotation::Authored&&(c.rotation==scene::VirtualCameraRotation::LookAt?hasAim:hasTracked);
    if(drivesRotation&&!snap&&c.rotationDamping>0) slerp(rig.rotation,rotation,smoothing(delta,c.rotationDamping),rotation);
    std::copy(rotation,rotation+4,rig.rotation);

    // Pose crua no objeto: só o que este componente decide, sem correções.
    if((drivesPosition||drivesRotation)&&w.authorityOf(handle)==TransformAuthority::Free) {
      Transform raw;if(poseTransform(position,rotation,own.scale,raw)) w.setWorldTransform(handle,raw);
    }

    // Correção 1: desoclusão ao longo da linha alvo → câmera.
    float corrected[3]{position[0],position[1],position[2]};
    if(!aiming&&hasAim) for(u32 i=0;i<3;++i) aimPoint[i]=aimed.position[i]+c.aimOffset[i];
    if(c.avoidObstacles&&hasAim&&physics) {
      float direction[3]{position[0]-aimPoint[0],position[1]-aimPoint[1],position[2]-aimPoint[2]};
      const float length=std::sqrt(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
      float allowed=length;
      if(length>c.minimumDistance+1e-4f) {
        for(float &d:direction) d/=length;
        const float start[3]{aimPoint[0]+direction[0]*c.minimumDistance,aimPoint[1]+direction[1]*c.minimumDistance,aimPoint[2]+direction[2]*c.minimumDistance};
        const float span=length-c.minimumDistance;
        const float cast[3]{direction[0]*span,direction[1]*span,direction[2]*span};
        QueryFilter filter;filter.gameplayLayerMask=c.collisionLayer?1u<<(c.collisionLayer-1):0xffffffffu;
        filter.ignore=bodyOwner(w,c.lookAtTarget?static_cast<ObjectId>(c.lookAtTarget):static_cast<ObjectId>(c.trackingTarget));
        QueryHit hit;bool found=false;
        if(c.cameraRadius>0) {QueryShapeDesc sphere;sphere.kind=QueryShapeKind::Sphere;sphere.radius=c.cameraRadius;found=physics->shapeCast(sphere,start,cast,filter,hit);}
        else found=physics->rayCast(start,cast,filter,hit);
        if(found) allowed=c.minimumDistance+std::clamp(hit.fraction,0.f,1.f)*span;
      } else for(float &d:direction) d=length>1e-6f?d/length:0;
      if(snap||rig.collision<0||allowed<rig.collision) rig.collision=allowed;
      else rig.collision+=(allowed-rig.collision)*smoothing(delta,c.collisionDamping);
      rig.collision=std::min(rig.collision,length);
      for(u32 i=0;i<3;++i) corrected[i]=aimPoint[i]+direction[i]*rig.collision;
    } else rig.collision=-1;

    // Correção 2: inclinação holandesa e tremor (Perlin) em torno da pose.
    float finalRotation[4];std::copy(rotation,rotation+4,finalRotation);
    if(c.dutch!=0) {float roll[4];axisAngle(2,c.dutch*.0174532925f,roll);multiply(finalRotation,roll,finalRotation);}
    rig.noiseTime+=delta*c.noiseFrequency;
    if(c.noiseAmplitude>0||c.noisePositionAmplitude>0) {
      const float seed=static_cast<float>(key.second%977)*7.31f;
      const auto channel=[&](u32 k){return scene::cameraNoise(rig.noiseTime+seed+static_cast<float>(k)*31.7f);};
      if(c.noiseAmplitude>0) {
        float shake[4];const float r=c.noiseAmplitude*.0174532925f;
        yawPitchRoll(channel(1)*r,channel(0)*r,channel(2)*r,shake);multiply(finalRotation,shake,finalRotation);
      }
      if(c.noisePositionAmplitude>0) {
        const float local[3]{channel(3)*c.noisePositionAmplitude,channel(4)*c.noisePositionAmplitude,channel(5)*c.noisePositionAmplitude};
        float offset[3];rotate(rotation,local,offset);for(u32 i=0;i<3;++i) corrected[i]+=offset[i];
      }
    }
    std::copy(corrected,corrected+3,rig.state.position);std::copy(finalRotation,finalRotation+4,rig.state.rotation);
    rig.state.verticalFov=c.verticalFov;rig.state.orthographicHalfHeight=c.orthographicHalfHeight;
    rig.state.nearPlane=c.nearPlane;rig.state.farPlane=c.farPlane;
    rig.valid=true;rig.previousValid=true;
  }

  static void mix(const CameraState &a,const CameraState &b,float t,CameraState &out) {
    CameraState r;
    for(u32 i=0;i<3;++i) r.position[i]=a.position[i]+(b.position[i]-a.position[i])*t;
    blendRotation(a.rotation,b.rotation,t,r.rotation);
    r.verticalFov=a.verticalFov+(b.verticalFov-a.verticalFov)*t;
    r.orthographicHalfHeight=a.orthographicHalfHeight+(b.orthographicHalfHeight-a.orthographicHalfHeight)*t;
    r.nearPlane=a.nearPlane+(b.nearPlane-a.nearPlane)*t;r.farPlane=a.farPlane+(b.farPlane-a.farPlane)*t;
    out=r;
  }
  const Rig *rigOf(ObjectId id) const {
    const Rig *best=nullptr;
    for(const auto &[key,r]:rigs_) if(key.first==id&&r.eligible&&r.valid) {best=&r;break;}
    return best;
  }
  void emit(GameWorld &w,ObjectId object,const scene::ComponentType &type,std::string_view event,std::initializer_list<scene::ComponentOperationValue> values) {
    if(!events_||!object) return;
    const auto *o=w.graph().find(object);const auto *v=o?o->components.find(type):nullptr;
    if(v) events_->emit(w,object,v->instanceId(),type,event,std::span(values.begin(),values.size()));
  }
  bool driveBrain(GameWorld &w,ObjectId id,const Key &key,const scene::CameraBrain &settings,Brain &b,float delta) {
    using V=scene::ComponentOperationValue;
    // Candidata: maior prioridade, depois a ativada por último, depois o menor ID.
    ObjectId best=kInvalidObject;float bestPriority=0;u64 bestStamp=0;const scene::VirtualCamera *bestCamera=nullptr;
    for(const auto &[rigKey,r]:rigs_) {
      if(!r.eligible||!r.valid) continue;
      const auto *o=w.graph().find(rigKey.first);const auto *v=o?o->components.findInstance(rigKey.second):nullptr;if(!v) continue;
      const auto &c=scene::virtualCamera(*v);
      const bool better=!best||c.priority>bestPriority||(c.priority==bestPriority&&(r.stamp>bestStamp||(r.stamp==bestStamp&&rigKey.first<best)));
      if(better) {best=rigKey.first;bestPriority=c.priority;bestStamp=r.stamp;bestCamera=&c;}
    }
    if(best!=b.live) {
      const ObjectId outgoing=b.live;
      if(!best) {
        emit(w,outgoing,scene::VirtualCamera::descriptor,"deactivated",{V::makeObject(0)});
        b.live=kInvalidObject;b.blending=false;b.blendFrom=kInvalidObject;
      } else {
        auto style=bestCamera->blendStyle;float duration=bestCamera->blendTime;
        if(style==scene::CameraBlendStyle::BrainDefault) {style=settings.defaultBlend;duration=settings.defaultBlendTime;}
        const bool cut=!b.hasOutput||style==scene::CameraBlendStyle::Cut||duration<=0;
        if(cut) {b.blending=false;b.blendFrom=kInvalidObject;}
        else {
          // Interrompida no meio, a transição parte do quadro mostrado agora.
          const auto *from=b.blending?nullptr:rigOf(outgoing);
          b.blendFrom=from?outgoing:kInvalidObject;b.frozen=b.output;
          b.blending=true;b.elapsed=0;b.duration=duration;b.style=style;
        }
        b.live=best;
        emit(w,id,scene::CameraBrain::descriptor,"camera_activated",{V::makeObject(best),V::makeObject(outgoing)});
        if(cut) emit(w,id,scene::CameraBrain::descriptor,"camera_cut",{V::makeObject(best)});
        emit(w,best,scene::VirtualCamera::descriptor,"activated",{V::makeObject(outgoing)});
        if(outgoing) emit(w,outgoing,scene::VirtualCamera::descriptor,"deactivated",{V::makeObject(best)});
      }
    }
    const Rig *target=rigOf(b.live);
    if(!target) return true;
    CameraState output=target->state;
    if(b.blending) {
      b.elapsed+=delta;
      const auto *from=b.blendFrom?rigOf(b.blendFrom):nullptr;
      if(b.blendFrom&&!from) {b.blendFrom=kInvalidObject;}            // a câmera de origem saiu: congela o que mostrava
      if(from) b.frozen=from->state;
      mix(b.frozen,target->state,scene::cameraBlendWeight(b.style,b.elapsed/b.duration),output);
      if(b.elapsed>=b.duration) {
        b.blending=false;b.blendFrom=kInvalidObject;output=target->state;
        emit(w,id,scene::CameraBrain::descriptor,"blend_finished",{V::makeObject(b.live)});
      }
    }
    b.output=output;b.hasOutput=true;
    return apply(w,id,key,output);
  }
  bool apply(GameWorld &w,ObjectId id,const Key &,const CameraState &s) {
    const auto handle=w.handle(id);
    if(w.authorityOf(handle)!=TransformAuthority::Free) return true;
    Transform current{},pose{};
    if(w.worldTransform(handle,current)!=WorldStatus::Ok||!poseTransform(s.position,s.rotation,current.scale,pose)) return true;
    if(w.setWorldTransform(handle,pose)!=WorldStatus::Ok) return false;
    const auto camera=w.findComponent(handle,"astra.camera");
    const auto *value=w.readComponent(camera);if(!value) return true;
    const auto &lens=static_cast<const scene::Camera &>(*value);
    if(lens.verticalFov!=s.verticalFov) w.setTweenNumber(camera,"vertical_fov",s.verticalFov);
    if(lens.orthographicHalfHeight!=s.orthographicHalfHeight) w.setTweenNumber(camera,"orthographic_half_height",s.orthographicHalfHeight);
    // Próximo/distante mantêm far > near em cada escrita: a ordem depende do sentido.
    const auto setPlane=[&](const char *property,float v){w.setProperty(camera,property,scene::ComponentPropertyValue{v});};
    if(s.nearPlane<lens.farPlane) {if(lens.nearPlane!=s.nearPlane) setPlane("near_plane",s.nearPlane);if(lens.farPlane!=s.farPlane) setPlane("far_plane",s.farPlane);}
    else {if(lens.farPlane!=s.farPlane) setPlane("far_plane",s.farPlane);if(lens.nearPlane!=s.nearPlane) setPlane("near_plane",s.nearPlane);}
    return true;
  }
  void sync(GameWorld &w) {
    if(world_!=w.worldId()) {rigs_.clear();brains_.clear();world_=w.worldId();revision_=~u64{0};stamp_=0;started_=false;}
    if(revision_==w.structuralRevision()) return;
    ids_.clear();std::vector<ObjectId> all;w.graph().collectSubtree(w.graph().root(),all);
    for(const auto id:all) if(const auto *o=w.graph().find(id))
      if(o->components.find(scene::VirtualCamera::descriptor)||o->components.find(scene::CameraBrain::descriptor)) ids_.push_back(id);
    revision_=w.structuralRevision();
  }
  std::map<Key,Rig> rigs_;
  std::map<Key,Brain> brains_;
  std::vector<ObjectId> ids_;
  ComponentEventQueue *events_=nullptr;
  u64 revision_=~u64{0},stamp_=0;
  u32 world_=0;
  bool started_=false;
};
}
