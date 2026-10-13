// Componentes dos modelos de cena: liga os nos do modelo com os componentes
// reais (os mesmos da autoria manual). Chamado dentro da transacao do modelo.
#include "editor/editor_scene_template.h"
#include "editor/editor_document.h"
#include "scene/ik_constraints.h"
#include "scene/ik_effects.h"
#include "scene/character.h"
#include "scene/physics_body.h"
#include "scene/collider.h"
#include "scene/camera.h"
#include "scene/camera_follow.h"
#include <functional>

namespace ae::editor {
bool applySceneTemplateComponents(const SceneTemplate &model,std::span<const EditorEntityId> created,const EditorDocument &document,
                                  const std::function<bool(EditorEntityId,const EditorEntity &)> &apply) {
  const auto node=[&](i32 index)->EditorEntityId {return index>=0&&static_cast<usize>(index)<created.size()?created[static_cast<usize>(index)]:kInvalidEntity;};
  // O cubo autoral nasce com colisor. Nos modelos com componentes, so os nos
  // marcados como colisor estatico mantem colisao: cubo visual de osso com
  // colisor entregaria a pose do osso para a fisica e o IK nao poderia mover.
  for(usize n=0;n<model.nodes.size()&&n<created.size();++n) {
    if(!model.nodes[n].mesh||!created[n]||!document.find(created[n]))continue;
    bool keep=false;for(const auto &spec:model.components)keep=keep||(spec.node==static_cast<i32>(n)&&spec.kind==SceneTemplateComponentKind::StaticCollider);
    if(keep)continue;
    auto values=*document.find(created[n]);bool changed=false;
    if(values.components.find(scene::Collider::descriptor)) {values.components.remove(scene::Collider::descriptor);changed=true;}
    if(values.components.find(scene::PhysicsBody::descriptor)) {values.components.remove(scene::PhysicsBody::descriptor);changed=true;}
    if(values.rigidBodyEnabled) {values.rigidBodyEnabled=false;changed=true;}
    if(changed&&!apply(created[n],values))return false;
  }
  for(const auto &spec:model.components) {
    const auto id=node(spec.node);if(!id||!document.find(id))return false;
    auto values=*document.find(id);
    using K=SceneTemplateComponentKind;
    switch(spec.kind) {
    case K::StaticCollider: {
      if(!values.components.find(scene::PhysicsBody::descriptor)&&!values.components.add(scene::PhysicsBody::descriptor))return false;
      if(!values.components.find(scene::Collider::descriptor)&&!values.components.add(scene::Collider::descriptor))return false;
      break;
    }
    case K::Character: {
      auto *c=static_cast<scene::Character*>(values.components.add(scene::Character::descriptor));if(!c)return false;
      c->speed=4;c->acceleration=12;c->braking=16;c->airControl=.4f;
      break;
    }
    case K::FollowCamera: {
      if(!values.components.find(scene::Camera::descriptor)&&!values.components.add(scene::Camera::descriptor))return false;
      auto *f=static_cast<scene::CameraFollow*>(values.components.add(scene::CameraFollow::descriptor));if(!f)return false;
      f->target=node(spec.a);
      break;
    }
    case K::TwoBoneIk: {
      auto *c=static_cast<scene::TwoBoneIk*>(values.components.add(scene::TwoBoneIk::descriptor));if(!c)return false;
      c->target=node(spec.a);break;
    }
    case K::ChainIk: {
      auto *c=static_cast<scene::ChainIk*>(values.components.add(scene::ChainIk::descriptor));if(!c)return false;
      c->target=node(spec.a);c->chainLength=spec.value>0?spec.value:3;c->solver=1;break;
    }
    case K::LookAt: {
      auto *c=static_cast<scene::LookAtIk*>(values.components.add(scene::LookAtIk::descriptor));if(!c)return false;
      c->target=node(spec.a);c->chainLength=spec.value>0?spec.value:3;break;
    }
    case K::FootGrounding: {
      auto *c=static_cast<scene::FootGrounding*>(values.components.add(scene::FootGrounding::descriptor));if(!c)return false;
      c->lockFoot=true;break;
    }
    case K::Grounder: {
      auto *c=static_cast<scene::GrounderBody*>(values.components.add(scene::GrounderBody::descriptor));if(!c)return false;
      c->pelvis=node(spec.a);break;
    }
    case K::Inertia: if(!values.components.add(scene::Inertia::descriptor))return false;break;
    case K::RotationCone: {
      auto *c=static_cast<scene::RotationLimit*>(values.components.add(scene::RotationLimit::descriptor));if(!c)return false;
      c->kind=2;c->cone=spec.value>0?spec.value:45;break;
    }
    case K::InteractionObject: {
      auto *c=static_cast<scene::InteractionObject*>(values.components.add(scene::InteractionObject::descriptor));if(!c)return false;
      c->pickUp=true;c->reachTime=.8f;break;
    }
    case K::InteractionSystem: {
      auto *c=static_cast<scene::InteractionSystem*>(values.components.add(scene::InteractionSystem::descriptor));if(!c)return false;
      c->autoEffector=node(spec.a);c->autoObject=node(spec.b);break;
    }
    }
    if(!apply(id,values))return false;
  }
  return true;
}
} // namespace ae::editor
