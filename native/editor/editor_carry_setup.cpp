// Cena de exemplo "Carregar com as duas maos" (modelo de cena) e o
// preenchimento universal de ossos humanoides nos componentes de corpo. Referencia: Final IK 2.2, demo "Interaction
// Pick Up 2-Handed" (PickUp2Handed.cs): duas maos na mesma InteractionObject,
// objeto sobe a um ponto de carregar no personagem e as maos seguem presas.
// Aqui tudo e componente: Objeto interagivel (duas pegadas + ponto de carregar)
// e Sistema de interacao (duas maos, pelve e peito para agachar e inclinar).
#include "editor/editor_session.h"
#include "editor/editor_humanoid_bones.h"
#include "runtime/transform_math.h"
#include "scene/collider.h"
#include "scene/ik_constraints.h"
#include "scene/ik_effects.h"
#include "scene/physics_body.h"
#include <algorithm>
#include <cmath>

namespace ae::editor {
namespace {
struct Vec3 {float x=0,y=0,z=0;};
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 mul(Vec3 a,float s){return {a.x*s,a.y*s,a.z*s};}
float len(Vec3 a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
Vec3 flat(Vec3 a){a.y=0;const float n=len(a);return n>1e-5f?mul(a,1/n):Vec3{};}
bool position(const runtime::SceneGraph &g,EditorEntityId id,Vec3 &out) {
  float m[16];if(!id||!runtime::worldMatrix(g,id,m))return false;
  out={m[12],m[13],m[14]};return true;
}
} // namespace

// Universal: ao adicionar um componente que pede ossos, preenche o que estiver
// vazio com os ossos humanoides da subarvore (nada e sobrescrito).
// Altura do osso do pe acima da raiz do personagem, na pose atual: e onde o
// tornozelo fica quando o pe esta no chao. Com Desvio do pe 0, o tornozelo ia
// ao chao e a malha do pe afundava.
float footRestHeight(const EditorDocument &document,EditorEntityId foot) {
  EditorEntityId top=foot;for(const auto *o=document.find(foot);o&&o->parent!=document.root();o=document.find(o->parent))top=o->parent;
  float f[16],r[16];
  if(top==foot||!runtime::worldMatrix(document,foot,f)||!runtime::worldMatrix(document,top,r))return 0;
  return std::clamp(f[13]-r[13],0.f,.5f);
}
bool prefillHumanoidReferences(const EditorDocument &document,EditorEntityId owner,scene::ComponentValue &value) {
  if(&value.type()==&scene::FootGrounding::descriptor) {
    auto &c=static_cast<scene::FootGrounding&>(value);
    if(c.footOffset==0)c.footOffset=footRestHeight(document,owner);
    return true;
  }
  const bool interaction=&value.type()==&scene::InteractionSystem::descriptor,full=&value.type()==&scene::FullBodyIk::descriptor,
             grounder=&value.type()==&scene::GrounderBody::descriptor;
  if(!interaction&&!full&&!grounder)return false;
  const auto bones=findHumanoidBones(document,owner);
  const auto fill=[](u64 &slot,EditorEntityId id){if(!slot&&id)slot=id;};
  if(interaction) {
    auto &c=static_cast<scene::InteractionSystem&>(value);fill(c.pelvis,bones.hips);fill(c.chest,bones.chest);
    u32 count=0;if(bones.hand[0])for(auto id=bones.hand[0];id&&id!=bones.upperArm[0];id=document.find(id)->parent)++count;
    if(count>=2&&count<=8)c.limbBones=static_cast<float>(count);
  } else if(full) {
    auto &c=static_cast<scene::FullBodyIk&>(value);fill(c.pelvis,bones.hips);fill(c.chest,bones.chest);
    const EditorEntityId tips[4]{bones.hand[0],bones.hand[1],bones.foot[0],bones.foot[1]};
    for(u32 i=0;i<4;++i)fill(c.tips[i],tips[i]);
  } else fill(static_cast<scene::GrounderBody&>(value).pelvis,bones.hips);
  return bones.hips||bones.arms();
}

bool EditorSession::addCarrySetupInTransaction(EditorEntityId character,EditorEntityId group,std::string &error) {
  const auto bones=findHumanoidBones(document_,character);
  if(!bones.arms()||!bones.hips) {error="Personagem sem mãos, antebraços, braços e quadril reconhecíveis pelo nome";return false;}
  // Cubo autoral quando a biblioteca o traz; senao o cubo primitivo, que o
  // shell publica sempre no indice 0 (as primitivas vem primeiro).
  u32 slot=boxAssetSlot();
  if(!slot&&mapScene_.assetCount())slot=1;
  if(!slot) {error="Sem cubo na biblioteca";return false;}
  // Medidas da pose atual: tudo proporcional ao braco do personagem.
  Vec3 root,hips,chest,hand[2],fore[2],upper[2];
  if(!position(document_,character,root)||!position(document_,bones.hips,hips)) {error="Pose do personagem inválida";return false;}
  for(u32 s=0;s<2;++s)if(!position(document_,bones.hand[s],hand[s])||!position(document_,bones.forearm[s],fore[s])||!position(document_,bones.upperArm[s],upper[s])) {error="Pose do braço inválida";return false;}
  if(!position(document_,bones.chest,chest))chest=mul(add(upper[0],upper[1]),.5f);
  const float arm=(len(sub(fore[0],upper[0]))+len(sub(hand[0],fore[0]))+len(sub(fore[1],upper[1]))+len(sub(hand[1],fore[1])))*.5f;
  const float leg=std::max(.2f,hips.y-root.y);
  if(arm<.05f) {error="Braços curtos demais para carregar";return false;}
  float rootWorld[16];runtime::worldMatrix(document_,character,rootWorld);
  // Frente pelo esqueleto (esquerda x cima): a raiz importada nem sempre olha
  // para onde o corpo olha.
  Vec3 side=flat(sub(upper[0],upper[1])),forward{-side.z,0,side.x};
  if(len(side)<.5f) {forward=flat({rootWorld[8],rootWorld[9],rootWorld[10]});if(len(forward)<.5f)forward={0,0,1};side={forward.z,0,-forward.x};}
  const float span=len(sub(upper[0],upper[1]));
  const float scale=arm/.55f;
  // Mesa na frente, caixa sobre ela; ponto de carregar na frente da barriga.
  const float boxWidth=std::max(.15f,span*.8f),boxHeight=.26f*scale,boxDepth=.28f*scale;
  const float tableTop=root.y+leg*.78f,ahead=arm*.75f+.05f*scale;
  const Vec3 tableCenter=add({root.x,root.y+(tableTop-root.y)*.5f,root.z},mul(forward,ahead));
  const Vec3 boxCenter=add({root.x,tableTop+boxHeight*.5f,root.z},mul(forward,ahead));
  const Vec3 holdWorld=add({root.x,hips.y+(chest.y-hips.y)*.55f,root.z},mul(forward,arm*.55f));
  const float yaw=std::atan2(forward.x,forward.z)*57.2957795f;
  const auto cube=[&](EditorEntityId parent,const char *name,Vec3 at,Vec3 size,const float color[3],bool collider)->EditorEntityId {
    const float origin[3]{0,0,0};EditorAssetInstantiation options;options.name=name;
    options.scale[0]=size.x;options.scale[1]=size.y;options.scale[2]=size.z;
    const auto id=instantiateAssetInTransaction(slot-1,parent,origin,&options);if(!id)return kInvalidEntity;
    auto values=*document_.find(id);
    values.transform.position[0]=at.x;values.transform.position[1]=at.y;values.transform.position[2]=at.z;
    values.transform.rotationDegrees[0]=values.transform.rotationDegrees[2]=0;values.transform.rotationDegrees[1]=parent==group?yaw:0;
    values.transform.scale[0]=size.x;values.transform.scale[1]=size.y;values.transform.scale[2]=size.z;
    if(auto *render=editMeshRenderer(values)){render->material.enabled=true;std::copy(color,color+3,render->material.baseColor);render->material.roughness=.85f;render->material.metallic=0;}
    if(collider) {
      if(!values.components.find(scene::PhysicsBody::descriptor)&&!values.components.add(scene::PhysicsBody::descriptor))return kInvalidEntity;
      if(!values.components.find(scene::Collider::descriptor)&&!values.components.add(scene::Collider::descriptor))return kInvalidEntity;
    } else {
      values.components.remove(scene::Collider::descriptor);values.components.remove(scene::PhysicsBody::descriptor);
    }
    return history_.applyValues(document_,id,values)?id:kInvalidEntity;
  };
  const auto folder=[&](EditorEntityId parent,const char *name,Vec3 local,float rotationY)->EditorEntityId {
    const auto id=history_.createEntity(document_,parent,EditorEntityKind::Folder,name);if(!id)return kInvalidEntity;
    auto values=*document_.find(id);
    values.transform.position[0]=local.x;values.transform.position[1]=local.y;values.transform.position[2]=local.z;
    values.transform.rotationDegrees[1]=rotationY;
    return history_.applyValues(document_,id,values)?id:kInvalidEntity;
  };
  const float tableColor[3]{.45f,.40f,.34f},boxColor[3]{.80f,.52f,.22f};
  // Sem chao com colisao na cena, o apoio dos pes nao tem onde pisar: cria um.
  bool ground=false;
  {std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
   for(const auto id:ids)if(const auto *e=document_.find(id);e&&e->components.find(scene::Collider::descriptor))ground=true;}
  if(!ground&&!cube(group,"Chão de apoio",{root.x,root.y-.05f,root.z},{6,.1f,6},tableColor,true))return false;
  if(!cube(group,"Mesa",tableCenter,{boxWidth+.3f*scale,tableTop-root.y,.5f*scale},tableColor,true))return false;
  // Caixa: pasta (o objeto pego) com o visual e as duas pegadas nas laterais.
  const auto box=folder(group,"Caixa",boxCenter,yaw);if(!box)return false;
  if(!cube(box,"Caixa · visual",{0,0,0},{boxWidth,boxHeight,boxDepth},boxColor,false))return false;
  // Pegadas no espaco da caixa (girada pelo yaw): +X local e o lado esquerdo
  // quando o personagem olha para +Z; o lado real vem dos ombros.
  const float sideSign=(side.x*std::cos(yaw/57.2957795f)-side.z*std::sin(yaw/57.2957795f))>=0?1.f:-1.f;
  const auto gripLeft=folder(box,"Pegada esq.",{sideSign*boxWidth*.5f,0,0},0);
  const auto gripRight=folder(box,"Pegada dir.",{-sideSign*boxWidth*.5f,0,0},0);
  if(!gripLeft||!gripRight)return false;
  // Ponto de carregar no personagem (local a raiz dele).
  const auto hold=history_.createEntity(document_,character,EditorEntityKind::Folder,"Ponto de carregar");if(!hold)return false;
  {
    auto values=*document_.find(hold);
    float world[16]{1,0,0,0,0,1,0,0,0,0,1,0,holdWorld.x,holdWorld.y,holdWorld.z,1};runtime::Transform local;
    if(!runtime::localTransformForWorld(world,rootWorld,local))return false;
    std::copy(local.position,local.position+3,values.transform.position);
    if(!history_.applyValues(document_,hold,values))return false;
  }
  {
    auto values=*document_.find(box);
    auto *c=static_cast<scene::InteractionObject*>(values.components.add(scene::InteractionObject::descriptor));if(!c)return false;
    c->grip=gripLeft;c->secondGrip=gripRight;c->holdPoint=hold;c->pickUp=true;c->reachTime=.9f;c->liftTime=.7f;c->releaseTime=.5f;
    if(!history_.applyValues(document_,box,values))return false;
  }
  {
    auto values=*document_.find(character);
    auto *c=static_cast<scene::InteractionSystem*>(values.components.edit(scene::InteractionSystem::descriptor));
    if(!c)c=static_cast<scene::InteractionSystem*>(values.components.add(scene::InteractionSystem::descriptor));
    if(!c)return false;
    c->autoEffector=bones.hand[0];c->autoSecond=bones.hand[1];c->autoObject=box;
    c->pelvis=bones.hips;c->chest=bones.chest;c->crouch=std::min(leg*.4f,.5f*scale);c->lean=.6f;
    // Ossos de cada braco ate o ombro (mao -> antebraco -> braco).
    u32 count=0;for(auto id=bones.hand[0];id&&id!=bones.upperArm[0];id=document_.find(id)->parent)++count;
    c->limbBones=static_cast<float>(std::clamp(count,2u,8u));
    if(!history_.applyValues(document_,character,values))return false;
  }
  // Pes apoiados: com o corpo agachado, os joelhos dobram em vez de os pes afundarem.
  for(u32 s=0;s<2;++s)if(bones.foot[s]) {
    auto values=*document_.find(bones.foot[s]);
    if(values.components.find(scene::FootGrounding::descriptor))continue;
    auto *c=static_cast<scene::FootGrounding*>(values.components.add(scene::FootGrounding::descriptor));if(!c)return false;
    c->lockFoot=true;c->footOffset=footRestHeight(document_,bones.foot[s]);
    if(!history_.applyValues(document_,bones.foot[s],values))return false;
  }
  return true;
}


} // namespace ae::editor
