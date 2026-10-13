#include "runtime/scene_root_motion.h"
#include "runtime/scene_physics.h"
#include "runtime/transform_math.h"
#include "runtime/ik_solvers.h"
#include "scene/root_motion.h"
#include <cmath>

namespace ae::runtime {
namespace {
constexpr float kDegrees=180.f/3.14159265f;
float wrapDegrees(float a) {while(a>180)a-=360;while(a<-180)a+=360;return a;}
// Yaw (graus) do eixo frontal de uma matriz, no plano XZ.
bool yawOf(const float m[16],u32 axis,float &yaw) {
  float f[3];ikAxisWorld(m,axis,f);
  if(std::abs(f[0])+std::abs(f[2])<1e-5f)return false;
  yaw=std::atan2(f[0],f[2])*kDegrees;return true;
}
void rotateAroundY(float m[16],float degrees,const float pivot[3]) {
  const float a=degrees/kDegrees,c=std::cos(a),s=std::sin(a);
  for(u32 col=0;col<3;++col) {const float x=m[col*4],z=m[col*4+2];m[col*4]=c*x+s*z;m[col*4+2]=-s*x+c*z;}
  const float x=m[12]-pivot[0],z=m[14]-pivot[2];m[12]=pivot[0]+c*x+s*z;m[14]=pivot[2]-s*x+c*z;
}
} // namespace

bool SceneRootMotion::advance(GameWorld &world,float dt,ScenePhysics *physics) {
  diagnostics_.clear();reports_.clear();
  if(!world.running())return false;
  std::vector<ObjectId> ids;world.graph().collectSubtree(world.graph().root(),ids);
  for(const auto owner:ids) {
    const auto *o=world.graph().find(owner);if(!o||!world.graph().activeInHierarchy(owner))continue;
    const auto *c=static_cast<const scene::RootMotion*>(o->components.find(scene::RootMotion::descriptor));
    if(!c||!c->enabled)continue;
    const ObjectId bone=static_cast<ObjectId>(c->bone);
    if(!bone||!world.graph().find(bone)||!world.graph().isDescendantOf(bone,owner)) {diagnostics_.push_back({owner,"Root motion sem osso raiz dentro do personagem"});continue;}
    float ownerWorld[16],boneWorld[16];Transform rel;
    if(!worldMatrix(world.graph(),owner,ownerWorld)||!worldMatrix(world.graph(),bone,boneWorld)||!localTransformForWorld(boneWorld,ownerWorld,rel))continue;
    float relMatrix[16];transformMatrix(rel,relMatrix);
    float yaw=0;const bool hasYaw=yawOf(relMatrix,c->axis,yaw);
    auto &s=states_[owner];
    if(!s.started) {
      for(u32 i=0;i<3;++i)s.position[i]=s.rest[i]=rel.position[i];
      s.yaw=s.restYaw=hasYaw?yaw:0;s.started=true;
      reports_.push_back({owner,Route::Transform});continue;
    }
    // 1) Passo do osso no espaco do personagem; salto grande = laco do clipe.
    float delta[3]{rel.position[0]-s.position[0],rel.position[1]-s.position[1],rel.position[2]-s.position[2]};
    float turn=hasYaw?wrapDegrees(yaw-s.yaw):0;
    Report report;report.owner=owner;
    // Resultado do pedido anterior: deslocamento realizado e residuo.
    if(s.pending) {
      float requested=0,residual=0;
      for(u32 i=0;i<3;++i) {report.realized[i]=ownerWorld[12+i]-s.origin[i];requested+=s.requested[i]*s.requested[i];
        const float d=s.requested[i]-report.realized[i];residual+=d*d;}
      report.residual=std::sqrt(residual);requested=std::sqrt(requested);
      report.blocked=requested>1e-4f&&report.residual>requested*.5f;
      if(report.blocked)diagnostics_.push_back({owner,"Root motion bloqueado pelo mundo: o personagem andou menos que a animação"});
      s.pending=false;
    }
    if(std::sqrt(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2])>c->loopJump) {for(u32 i=0;i<3;++i)delta[i]=s.step[i];report.loop=true;}
    if(std::abs(turn)>c->loopTurn) {turn=s.turn;report.loop=true;}
    for(u32 i=0;i<3;++i) {s.position[i]=rel.position[i];s.step[i]=delta[i];}
    if(hasYaw)s.yaw=yaw;
    s.turn=turn;
    float applied[3]{c->horizontal?delta[0]*c->weight:0,c->vertical?delta[1]*c->weight:0,c->horizontal?delta[2]*c->weight:0};
    const float appliedTurn=c->rotation?turn*c->weight:0;
    // Passo no mundo pelo eixo do personagem (escala incluida).
    const float worldStep[3]{ownerWorld[0]*applied[0]+ownerWorld[4]*applied[1]+ownerWorld[8]*applied[2],
                             ownerWorld[1]*applied[0]+ownerWorld[5]*applied[1]+ownerWorld[9]*applied[2],
                             ownerWorld[2]*applied[0]+ownerWorld[6]*applied[1]+ownerWorld[10]*applied[2]};
    for(u32 i=0;i<3;++i)report.delta[i]=worldStep[i];
    report.turn=appliedTurn;
    // 2) Rota: Personagem, corpo dinamico ou transformacao livre.
    const bool owned=physics&&physics->ownsWorld(world);
    if(owned&&physics->hasCharacter(owner)) {
      report.route=Route::Character;
      // O passo vira velocidade pedida ao motor: a colisao decide o realizado.
      const float inv=dt>1e-6f?1/dt:0;
      const float velocity[3]{worldStep[0]*inv,worldStep[1]*inv,worldStep[2]*inv};
      float ownerYaw=0;yawOf(ownerWorld,2,ownerYaw);
      const bool moved=physics->requestCharacterVelocity(owner,velocity,c->vertical);
      for(u32 i=0;i<3;++i) {s.requested[i]=worldStep[i];s.origin[i]=ownerWorld[12+i];}
      s.pending=moved;
      const bool turned=appliedTurn==0||physics->setCharacterYaw(owner,(ownerYaw+appliedTurn)/kDegrees);
      if(!moved||!turned)diagnostics_.push_back({owner,"Personagem recusou o passo do root motion"});
    } else if(float probe[3];owned&&physics->getBodyVelocity(owner,probe)) {
      report.route=Route::Body;
      // Velocidade no plano vem da animacao; a vertical continua da fisica.
      float velocity[3];physics->getBodyVelocity(owner,velocity);
      const float inv=dt>1e-6f?1/dt:0;
      if(c->horizontal) {velocity[0]=worldStep[0]*inv;velocity[2]=worldStep[2]*inv;}
      if(c->vertical)velocity[1]=worldStep[1]*inv;
      if(!physics->setBodyVelocity(owner,velocity))diagnostics_.push_back({owner,"Corpo recusou a velocidade do root motion"});
      for(u32 i=0;i<3;++i)report.delta[i]=worldStep[i];
    } else {
      report.route=Route::Transform;
      float moved[16];for(u32 i=0;i<16;++i)moved[i]=ownerWorld[i];
      for(u32 i=0;i<3;++i)moved[12+i]+=worldStep[i];
      if(appliedTurn!=0) {const float pivot[3]{moved[12],moved[13],moved[14]};rotateAroundY(moved,appliedTurn,pivot);}
      float parent[16];Transform local;
      if(!parentWorldMatrix(world.graph(),owner,parent)||!localTransformForWorld(moved,parent,local)||
         world.setLocalTransform(world.handle(owner),local)!=WorldStatus::Ok) {
        report.route=Route::Blocked;diagnostics_.push_back({owner,"Transformação do personagem pertence a outro sistema"});
      }
    }
    // 3) O osso volta ao lugar nos eixos aplicados (a animacao nao anda duas vezes).
    if(c->keepBoneInPlace&&report.route!=Route::Blocked) {
      float ownerNow[16];if(!worldMatrix(world.graph(),owner,ownerNow))continue;
      Transform placed=rel;
      if(c->horizontal) {placed.position[0]=s.rest[0]+(rel.position[0]-s.rest[0])*(1-c->weight);placed.position[2]=s.rest[2]+(rel.position[2]-s.rest[2])*(1-c->weight);}
      if(c->vertical)placed.position[1]=s.rest[1]+(rel.position[1]-s.rest[1])*(1-c->weight);
      float placedMatrix[16];transformMatrix(placed,placedMatrix);
      if(c->rotation&&hasYaw) {const float pivot[3]{placedMatrix[12],placedMatrix[13],placedMatrix[14]};rotateAroundY(placedMatrix,-wrapDegrees(yaw-s.restYaw)*c->weight,pivot);}
      float desired[16],parent[16];Transform local;multiplyMatrix(ownerNow,placedMatrix,desired);
      if(parentWorldMatrix(world.graph(),bone,parent)&&localTransformForWorld(desired,parent,local))world.setLocalTransform(world.handle(bone),local);
    }
    reports_.push_back(report);
  }
  for(auto it=states_.begin();it!=states_.end();) {
    const auto *o=world.graph().find(it->first);
    if(!o||!o->components.find(scene::RootMotion::descriptor))it=states_.erase(it);else ++it;
  }
  return true;
}
} // namespace ae::runtime
