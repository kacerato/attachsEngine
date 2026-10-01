#pragma once
#include "editor/editor_document.h"
#include "scene/physics_body.h"
#include "scene/collider.h"
#include "scene/joint.h"
#include <array>

namespace ae::test {
// One authored scene shared by integrated physics acceptance and the real UI
// preview. No fake backend or scripted transform animation supplies its motion.
struct MechanismsFixture {
  editor::EditorDocument document;
  std::array<editor::EditorEntityId,5> anchors{},bodies{};
  bool create() {
    constexpr scene::JointKind kinds[]{scene::JointKind::Fixed,scene::JointKind::Cone,
      scene::JointKind::SwingTwist,scene::JointKind::SixDOF,scene::JointKind::Spring};
    constexpr const char *names[]{"Fixa","Cone","Swing / Twist","Configurável 6DOF","Mola"};
    for(u32 i=0;i<5;++i) {
      anchors[i]=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Suporte");
      auto anchor=*document.find(anchors[i]);anchor.transform.position[0]=float(i)*6;anchor.transform.position[1]=8;
      auto *support=static_cast<scene::PhysicsBody*>(anchor.components.add(scene::PhysicsBody::descriptor));support->motion=scene::BodyMotion::Static;
      auto *shape=static_cast<scene::Collider*>(anchor.components.add(scene::Collider::descriptor));shape->halfX=shape->halfY=shape->halfZ=.2f;
      if(!document.applyEntityValues(anchors[i],anchor))return false;
      bodies[i]=document.createEntity(document.root(),editor::EditorEntityKind::Folder,names[i]);
      auto body=*document.find(bodies[i]);body.transform.position[0]=float(i)*6;body.transform.position[1]=6;
      auto *physical=static_cast<scene::PhysicsBody*>(body.components.add(scene::PhysicsBody::descriptor));physical->motion=scene::BodyMotion::Dynamic;physical->allowSleep=false;
      shape=static_cast<scene::Collider*>(body.components.add(scene::Collider::descriptor));shape->shape=scene::ColliderShape::Cylinder;shape->radius=.25f;shape->halfHeight=.5f;
      auto *joint=static_cast<scene::Joint*>(body.components.add(scene::Joint::descriptor));joint->kind=kinds[i];joint->connectedBody=anchors[i];
      joint->axisA[0]=joint->axisB[0]=1;joint->axisA[1]=joint->axisB[1]=0;
      joint->normalA[0]=joint->normalB[0]=0;joint->normalA[1]=joint->normalB[1]=1;
      if(i==4) {joint->limitMin=joint->limitMax=2;joint->frequency=3;}
      else joint->anchorB[1]=-2;
      if(i==3) {joint->axes[0].motion=1;joint->axes[0].motor=2;joint->axes[0].position=.75f;joint->axes[0].force=100;}
      if(!document.applyEntityValues(bodies[i],body))return false;
    }
    return true;
  }
};
}
