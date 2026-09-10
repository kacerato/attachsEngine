// Explicit validation fixture, never packaged or seeded into user projects.
#include "editor/editor_archive.h"
#include "editor/editor_map_scene.h"
#include "editor/editor_physics_body.h"
#include "editor/editor_scene_physics.h"
#include "scene/joint.h"
#include "scene/script_behavior.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
using namespace ae;
using namespace ae::editor;
int main(int argc,char **argv) {
  if(argc!=2) {std::fprintf(stderr,"usage: component_scene_fixture output-directory\n");return 1;}
  const auto output=std::filesystem::absolute(argv[1]);
  std::filesystem::create_directories(output);
  EditorDocument doc;
  auto box=[&](const char *name,float x,float y,float z,float sx,float sy,float sz,float r,float g,float b) {
    auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,name);auto v=*doc.find(id);
    v.transform.position[0]=x;v.transform.position[1]=y;v.transform.position[2]=z;
    v.transform.scale[0]=sx;v.transform.scale[1]=sy;v.transform.scale[2]=sz;
    auto *mesh=editMeshRenderer(v);mesh->mesh=1;mesh->material.enabled=true;
    mesh->material.baseColor[0]=r;mesh->material.baseColor[1]=g;mesh->material.baseColor[2]=b;
    doc.applyEntityValues(id,v);return id;
  };
  auto physical=[&](EditorEntityId id,scene::BodyMotion motion) {
    auto v=*doc.find(id);editPhysicsBody(v)->motion=motion;editCollider(v);doc.applyEntityValues(id,v);
  };
  const auto floor=box("Piso",0,-.25f,0,16,.5f,14,.2f,.3f,.2f);physical(floor,scene::BodyMotion::Static);
  const auto mover=box("Composto",-3,3,0,1,1,1,.1f,.65f,.9f);physical(mover,scene::BodyMotion::Dynamic);
  auto v=*doc.find(mover);auto *second=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));
  second->centerX=.65f;second->halfX=.25f;second->halfY=.25f;second->halfZ=.25f;second->rotationZ=25;
  editPhysicsBody(v)->mass=2;
  auto *script=static_cast<scene::ScriptBehavior*>(v.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="validation.motion";script->source="FixtureMotion.cs";doc.applyEntityValues(mover,v);
  const auto child=doc.createEntity(mover,EditorEntityKind::Folder,"Forma filha");v=*doc.find(child);
  v.transform.position[0]=-.75f;v.transform.scale[0]=v.transform.scale[1]=v.transform.scale[2]=.5f;
  editMeshRenderer(v)->mesh=1;auto *shape=editCollider(v);shape->owner=mover;
  doc.applyEntityValues(child,v);
  const auto sensor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Sensor");v=*doc.find(sensor);
  v.transform.position[0]=-3;v.transform.position[1]=2;
  editPhysicsBody(v)->motion=scene::BodyMotion::Kinematic;editPhysicsBody(v)->sensor=true;
  shape=editCollider(v);shape->halfX=2;shape->halfY=.5f;shape->halfZ=2;
  script=static_cast<scene::ScriptBehavior*>(v.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="validation.sensor";script->source="FixtureMotion.cs";
  script->setProperty("target","object",std::to_string(mover));doc.applyEntityValues(sensor,v);
  const auto anchor=box("Ancora",3,3,0,.4f,.4f,.4f,.9f,.8f,.2f);physical(anchor,scene::BodyMotion::Static);
  const auto motor=box("Motor",3,1.5f,0,1,1,1,.9f,.3f,.15f);physical(motor,scene::BodyMotion::Dynamic);
  v=*doc.find(motor);auto *joint=static_cast<scene::Joint*>(v.components.add(scene::Joint::descriptor));
  joint->kind=scene::JointKind::Hinge;joint->connectedBody=anchor;joint->anchorA[1]=1.5f;
  joint->axisA[1]=joint->axisB[1]=0;joint->axisA[2]=joint->axisB[2]=1;
  joint->limitMin=-90;joint->limitMax=90;joint->motor=1;joint->motorVelocity=35;joint->motorForce=20;
  doc.applyEntityValues(motor,v);
  const auto camera=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Camera");v=*doc.find(camera);editCamera(v);
  v.transform.position[1]=5;v.transform.position[2]=-12;v.transform.rotationDegrees[0]=15;doc.applyEntityValues(camera,v);
  EditorScenePhysics physics;if(!physics.start(doc)) {std::fprintf(stderr,"%s\n",physics.error().c_str());return 2;}
  if(!saveEditorDocument((output/"composition.aescene").string().c_str(),doc,0)) return 3;
  std::ofstream code(output/"FixtureMotion.cs");code<<R"CS(using Astra;
using System.Numerics;
[ComponentId("validation.motion")]
public sealed class MotionProbe : Behavior {
    [PropertyId("force")] public Vector3 Force = new(0, 1, 0);
    private int ticks;
    public override void Start() { Scene.AddImpulse(ObjectId,new(0,1,0)); Scene.AddAngularImpulse(ObjectId,new(0,.3f,0)); Scene.Log(ObjectId,"VALIDATION start"); }
    public override void FixedUpdate(float dt) {
        Scene.AddForce(ObjectId,Force); Scene.AddTorque(ObjectId,new(0,.1f,0));
        if(++ticks==60) Scene.Log(ObjectId,"VALIDATION velocity="+Scene.GetBodyVelocity(ObjectId));
    }
    public override void Stop() => Scene.Log(ObjectId,"VALIDATION stop");
}
[ComponentId("validation.sensor")]
public sealed class SensorProbe : Behavior {
    [PropertyId("target")] public ObjectReference Target;
    public override void TriggerEnter(ObjectReference other) { Scene.Log(ObjectId,"VALIDATION enter="+other.ObjectId); Scene.AddImpulse(Target.ObjectId,new(0,1,0)); }
    public override void TriggerExit(ObjectReference other) => Scene.Log(ObjectId,"VALIDATION exit="+other.ObjectId);
}
)CS";
  std::printf("fixture: %u objects, %u bodies, %u joint; cube resource #1; fingerprint 0\n",doc.entityCount(),physics.bodyCount(),physics.jointCount());
  return code?0:4;
}
