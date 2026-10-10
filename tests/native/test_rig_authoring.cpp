#include "harness.h"
#include "resources/rig_evaluation.h"
#include "runtime/rig_instance.h"
#include "runtime/transform_math.h"
#include "core/rotation_math.h"
#include <cmath>

using namespace ae;
using namespace ae::resources;
namespace {
std::array<float,4> orientation(float x,float y,float z) {
  const float degrees[]{x,y,z};std::array<float,4> q;rotationQuaternionXYZ(degrees,q.data());return q;
}
bool near(float a,float b,float epsilon=1e-4f){return std::abs(a-b)<epsilon;}
RigAsset arm(std::string_view key,float length,bool destination) {
  RigAsset value;value.guid=assetGuidFromSeed(key);value.name=key;value.nextId=7;
  RigJoint root;root.id=1;root.name=destination?"BaseB":"MotorA";
  RigJoint shoulder;shoulder.id=2;shoulder.parent=1;shoulder.name=destination?"EixoB":"AcoplamentoA";shoulder.path=shoulder.name;shoulder.bind.position={length,0,0};
  RigJoint tool;tool.id=3;tool.parent=2;tool.name=destination?"FerramentaB":"GarraA";tool.path=shoulder.path+'/'+tool.name;tool.bind.position={length,0,0};
  if(destination){root.bind.position={10,0,0};root.bind.rotation=orientation(0,90,0);shoulder.bind.rotation=orientation(0,0,45);}
  root.reference=root.bind;shoulder.reference=shoulder.bind;tool.reference=tool.bind;value.joints={root,shoulder,tool};
  for(u32 i=0;i<3;++i){RigControl c;c.id=4+i;c.joint=1+i;c.name=value.joints[i].name;c.channels=RigAll;value.controls.push_back(c);}
  return value;
}
AnimationClip motion() {
  AnimationClip clip;clip.name="Braço";clip.duration=1;
  AnimationChannel position;position.node=0;position.times={0,1};position.values={0,0,0,1,0,0};
  AnimationChannel rotation;rotation.node=1;rotation.path=AnimationPath::Rotation;rotation.times={0,1};rotation.values={0,0,0,1,0,0,.707106781f,.707106781f};
  clip.channels={position,rotation};return clip;
}
}
AE_TEST(rig_retarget_rest_compensation_runtime_bake_and_resource_roundtrip) {
  auto source=arm("Mecanismo origem",1,false),target=arm("Mecanismo destino",2,true);std::string error;
  RigAsset restored;AE_EXPECT_TRUE(RigAsset::deserialize(source.serialize(),restored,&error)&&restored.serialize()==source.serialize(),error.c_str());
  RetargetProfile profile;profile.guid=assetGuidFromSeed("Perfil mecânico");profile.name="A para B";profile.source=source.guid;profile.target=target.guid;
  profile.sourceRevision=source.revision;profile.targetRevision=target.revision;profile.motionScale=2;profile.facing=orientation(0,90,0);
  for(u64 id=1;id<=3;++id){RetargetJoint j;j.source=j.target=id;if(id==1)j.position=RetargetPosition::ScaledOffset;profile.mapping.push_back(j);}
  RetargetProfile cold;AE_EXPECT_TRUE(RetargetProfile::deserialize(profile.serialize(),cold,&error)&&cold.serialize()==profile.serialize(),error.c_str());
  CompiledRetarget program;AE_EXPECT_TRUE(compileRetarget(source,target,cold,program,error),error.c_str());
  const u64 nodeJoints[]{1,2,3};CompiledRigAnimation clip;
  AE_EXPECT_TRUE(compileRigAnimation(program.source,motion(),nodeJoints,false,clip,error),error.c_str());
  std::vector<RigTransform> input(3),output(3);RetargetWorkspace scratch;scratch.prepare(program);
  AE_EXPECT_TRUE(sampleRigAnimation(program.source,clip,.5f,input)&&evaluateRetarget(program,input,output,scratch),"sample and transfer valid TRS");
  AE_EXPECT_TRUE(near(output[0].position[0],10)&&near(output[0].position[2],-1)&&near(output[1].position[0],2)&&near(output[2].position[0],2),"root offset rotates/scales; destination limb lengths survive");
  const auto delta=multiplyRigRotation(multiplyRigRotation(profile.facing,orientation(0,0,45)),inverseRigRotation(profile.facing));
  const auto expected=multiplyRigRotation(delta,program.target.joints[1].referenceWorld.rotation);
  AE_EXPECT_TRUE(rotationDistanceDegrees(expected.data(),scratch.targetWorld[1].rotation.data())<.001,"different parent orientation and reference roll are compensated");
  AE_EXPECT_TRUE(scratch.maximumAngularError<.001&&scratch.maximumPositionError<1e-4&&scratch.limitedJoints==0,"unconstrained reference-relative transfer has quantified zero residual");
  AnimationClip baked;AE_EXPECT_TRUE(bakeRetarget(program,clip,{},baked,error),error.c_str());
  CompiledRigAnimation bakedClip;AE_EXPECT_TRUE(compileRigAnimation(program.target,baked,nodeJoints,false,bakedClip,error),error.c_str());
  std::vector<RigTransform> bakedPose(3);
  for(float time:{0.f,.25f,.5f,.75f,1.f}) {
    AE_EXPECT_TRUE(sampleRigAnimation(program.source,clip,time,input)&&evaluateRetarget(program,input,output,scratch)&&sampleRigAnimation(program.target,bakedClip,time,bakedPose),"bake and realtime sample");
    for(u32 j=0;j<3;++j)AE_EXPECT_TRUE(near(output[j].position[0],bakedPose[j].position[0])&&near(output[j].position[2],bakedPose[j].position[2])&&rotationDistanceDegrees(output[j].rotation.data(),bakedPose[j].rotation.data())<.001,"independent baked clip agrees with realtime at sampled instants");
  }
  runtime::SceneGraph scene;const auto root=scene.createEntity(scene.root(),runtime::ObjectKind::Folder,target.joints[0].name);
  const auto shoulder=scene.createEntity(root,runtime::ObjectKind::Folder,target.joints[1].name),tool=scene.createEntity(shoulder,runtime::ObjectKind::Folder,target.joints[2].name);
  const runtime::ObjectId objects[]{root,shoulder,tool};
  for(u32 i=0;i<3;++i){runtime::Transform transform;AE_EXPECT_TRUE(runtime::rigTransformToScene(target.joints[i].bind,transform,transform)&&scene.setTransform(objects[i],transform),"set actual target reference hierarchy");}
  runtime::RigBindings bindings;AE_EXPECT_TRUE(runtime::bindRigToScene(target,program.target,scene,root,bindings),"portable exact path binding");
  runtime::RetargetInstance instance;AE_EXPECT_TRUE(instance.configure(program,clip,bindings,error)&&instance.evaluate(scene,.5f,error),error.c_str());
  float world[16];AE_EXPECT_TRUE(runtime::worldMatrix(scene,tool,world)&&std::isfinite(world[12]),"runtime updates real articulated scene hierarchy");
  const auto before=scene.find(root)->transform;AE_EXPECT_TRUE(scene.destroyEntity(tool)&&!instance.evaluate(scene,.8f,error)&&scene.find(root)->transform.position[2]==before.position[2],"missing destination rejects whole frame before mutating surviving joints");
  runtime::RigBindings missing;AE_EXPECT_TRUE(!runtime::bindRigToScene(target,program.target,scene,root,missing)&&missing.issues.size()==1&&missing.objects[0]==root&&missing.objects[1]==shoulder&&!missing.objects[2],"removal invalidates only the affected binding");
  const auto old=target.serialize();AE_EXPECT_TRUE(!target.edit(target.revision,[](auto &candidate,auto &){candidate.joints[0].parent=3;return true;},error)&&target.serialize()==old,"cycle refused atomically");
  ++target.revision;AE_EXPECT_TRUE(!compileRetarget(source,target,profile,program,error),"stale rig revision requires explicit profile revalidation");
}
AE_TEST(rig_mirror_asymmetric_pose_masks_control_spaces_limits_and_missing_bindings) {
  RigAsset rig;rig.guid=assetGuidFromSeed("Garra assimétrica");rig.name="Garra";rig.nextId=7;
  RigJoint root;root.id=1;root.name="Centro";
  RigJoint left;left.id=2;left.parent=1;left.name=left.path="Lado A";left.bind.position={-1,0,0};
  RigJoint right;right.id=3;right.parent=1;right.name=right.path="Lado B";right.bind.position={2,0,0};right.bind.rotation=orientation(0,0,30);
  root.reference=root.bind;left.reference=left.bind;right.reference=right.bind;rig.joints={root,left,right};
  for(u32 i=0;i<3;++i){RigControl c;c.id=4+i;c.joint=1+i;c.name=rig.joints[i].name;c.channels=RigAll;rig.controls.push_back(c);}
  rig.mirror={{5,6},{4,4}};CompiledRig compiled;std::string error;AE_EXPECT_TRUE(compileRig(rig,compiled,error),error.c_str());
  std::vector<RigTransform> pose{root.reference,left.reference,right.reference},mirrored(3);pose[1].position[1]=.5f;pose[1].rotation=orientation(0,0,20);
  AE_EXPECT_TRUE(mirrorRigPose(rig,compiled,pose,mirrored,RigAll,true,error),error.c_str());
  AE_EXPECT_TRUE(near(mirrored[2].position[0],2)&&near(mirrored[2].position[1],.5f)&&rotationDistanceDegrees(mirrored[2].rotation.data(),orientation(0,0,10).data())<.001,"reference mirror preserves asymmetric length and compensates side rest rotation");
  std::vector<RigTransform> twice(3);AE_EXPECT_TRUE(mirrorRigPose(rig,compiled,mirrored,twice,RigAll,true,error),error.c_str());
  for(u32 i=0;i<3;++i)AE_EXPECT_TRUE(near(twice[i].position[0],pose[i].position[0])&&near(twice[i].position[1],pose[i].position[1])&&rotationDistanceDegrees(twice[i].rotation.data(),pose[i].rotation.data())<.001,"bidirectional reference mirror is involutive without active limits");
  rig.mirrorOffset=1;AE_EXPECT_TRUE(compileRig(rig,compiled,error)&&mirrorRigPose(rig,compiled,pose,mirrored,RigPosition,false,error)&&near(mirrored[2].position[0],3),"absolute mirror respects plane offset and selected channels");
  rig.controls[1].limit.kind=RigLimitKind::Hinge;rig.controls[1].limit.axis={0,0,1};rig.controls[1].limit.minimum=-25;rig.controls[1].limit.maximum=25;
  AE_EXPECT_TRUE(compileRig(rig,compiled,error),error.c_str());RigTransform value;value.rotation=orientation(40,0,80);
  AE_EXPECT_TRUE(applyRigControl(rig,compiled,5,value,pose,mirrored,error),error.c_str());
  AE_EXPECT_TRUE(rotationDistanceDegrees(mirrored[1].rotation.data(),orientation(0,0,25).data())<.001&&mirrored[1].position==left.reference.position,"hinge removes swing and clamps signed twist in reference space");
  RigPose saved;saved.guid=assetGuidFromSeed("Pose garra");saved.rig=rig.guid;saved.rigRevision=rig.revision;saved.name="Fechada";saved.entries={{2,RigRotation,mirrored[1]}};
  RigPose reopened;AE_EXPECT_TRUE(RigPose::deserialize(saved.serialize(),reopened,&error)&&blendRigPose(compiled,reopened,.5f,pose,twice,error),error.c_str());
  AE_EXPECT_TRUE(rotationDistanceDegrees(twice[1].rotation.data(),orientation(0,0,22.5f).data())<.001&&twice[1].position==pose[1].position,"weighted portable pose applies only authored channel mask");
  saved.entries[0].joint=999;const auto unchanged=twice[1].rotation;
  AE_EXPECT_TRUE(!blendRigPose(compiled,saved,1,pose,twice,error)&&twice[1].rotation==unchanged,"unknown stable joint refuses pose without partial writes");
}
