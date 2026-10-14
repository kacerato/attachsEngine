// Humanoid da Unity (músculos -> rotações): endereços das curvas, a pose de
// referência em casos de direção conhecida e o redirecionamento para um alvo.
#include "harness.h"
#include "core/sha256.h"
#include "editor/editor_animation_clip_preview.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_library.h"
#include "editor/editor_session.h"
#include "renderer/authoring_geometry.h"
#include "resources/animation_curve.h"
#include "resources/model_import.h"
#include "resources/unity_humanoid.h"
#include "resources/unity_import_recipe.h"
#include "runtime/transform_math.h"
#include <chrono>
#include <cmath>
#include <cstdlib>

using namespace ae;
using namespace ae::resources;

namespace {
using Muscles=std::array<std::array<float,3>,UnityHumanBoneCount>;
std::array<double,3> axisY(const std::array<double,4> &q) {
  // Rotaciona (0,1,0) pelo quaternion (x,y,z,w).
  const double x=q[0],y=q[1],z=q[2],w=q[3];
  return {2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x)};
}
UnityHumanoidPose pose(const Muscles &muscles) {
  const float rootT[3]{0,1,0},rootQ[4]{0,0,0,1};UnityHumanoidPose out;
  unityHumanoidReferencePose(muscles,rootT,rootQ,out);return out;
}
AnimationCurve constant(float value,float duration) {
  AnimationCurve curve;
  for(const float time:{0.f,duration}) {AnimationCurveKey key;key.id=curve.keys.size()+1;key.time=time;key.value=value;key.incoming=key.outgoing=AnimationTangentMode::Linear;curve.keys.push_back(key);}
  return curve;
}
}

AE_TEST(unity_humanoid_attributes_address_muscles_body_and_ignored_ik) {
  const auto arm=unityHumanoidAttribute("Left Arm Down-Up");
  AE_EXPECT_TRUE(arm.kind==UnityHumanoidAttribute::Kind::Muscle&&arm.bone==13&&arm.axis==2,"Left Arm Down-Up is LeftUpperArm, swing axis");
  const auto twist=unityHumanoidAttribute("Right Forearm Twist In-Out");
  AE_EXPECT_TRUE(twist.kind==UnityHumanoidAttribute::Kind::Muscle&&twist.bone==16&&twist.axis==0,"forearm twist is axis 0");
  const auto finger=unityHumanoidAttribute("Left Thumb 1 Stretched");
  AE_EXPECT_TRUE(finger.kind==UnityHumanoidAttribute::Kind::Muscle&&finger.bone==24&&finger.axis==2,"legacy finger name resolves");
  AE_EXPECT_TRUE(unityHumanoidAttribute("RootQ.w").kind==UnityHumanoidAttribute::Kind::RootQ&&unityHumanoidAttribute("RootQ.w").axis==3,"body rotation");
  AE_EXPECT_TRUE(unityHumanoidAttribute("RootT.y").kind==UnityHumanoidAttribute::Kind::RootT,"body position");
  AE_EXPECT_TRUE(unityHumanoidAttribute("LeftFootT.x").kind==UnityHumanoidAttribute::Kind::Ignored,"IK goals stay out of the pose");
  AE_EXPECT_TRUE(unityHumanoidAttribute("Spine Wiggle").kind==UnityHumanoidAttribute::Kind::None,"unknown attribute is reported");
  AE_EXPECT_EQ(unityHumanBoneIndex("Left Index Proximal"),27,"finger human name");
  AE_EXPECT_EQ(unityHumanBoneIndex("UpperChest"),54,"upper chest keeps index 54");
}

AE_TEST(unity_humanoid_reference_pose_has_known_directions) {
  Muscles neutral{};const auto rest=pose(neutral);
  const auto leftArm=axisY(rest.global[13]),rightArm=axisY(rest.global[14]),leftLeg=axisY(rest.global[1]);
  AE_EXPECT_TRUE(leftArm[0]>.3&&leftArm[1]<0,"neutral left arm goes out to +X and down");
  AE_EXPECT_TRUE(std::abs(leftArm[0]+rightArm[0])<1e-3&&std::abs(leftArm[1]-rightArm[1])<1e-3,"arms are mirror images");
  AE_EXPECT_TRUE(leftLeg[1]<-.9,"neutral leg points down");
  AE_EXPECT_TRUE(std::abs(rest.hips[1]-1)<.15&&std::abs(rest.hips[0])<1e-3,"hips near human height, centred");
  Muscles raised{};raised[13][2]=1;
  const auto up=axisY(pose(raised).global[13]);
  AE_EXPECT_TRUE(up[1]>leftArm[1]+.5,"Arm Down-Up +1 raises the arm");
  if(std::getenv("AE_UMOTION_TRACE")) {
    Muscles front{};front[13][1]=1;const auto f=axisY(pose(front).global[13]);
    const auto foot=axisY(rest.global[5]),toes=axisY(rest.global[19]);
    std::fprintf(stderr,"  braço frente(+1) (%.2f %.2f %.2f) neutro (%.2f %.2f %.2f) pé (%.2f %.2f %.2f) dedos (%.2f %.2f %.2f)\n",
                 f[0],f[1],f[2],leftArm[0],leftArm[1],leftArm[2],foot[0],foot[1],foot[2],toes[0],toes[1],toes[2]);
    Muscles elbow{};elbow[13][2]=-1;elbow[15][2]=-1;const auto fore=axisY(pose(elbow).global[15]);
    Muscles knee{};knee[3][2]=-1;const auto shin=axisY(pose(knee).global[3]);
    Muscles thigh{};thigh[1][2]=1;const auto leg=axisY(pose(thigh).global[1]);
    Muscles spine{};spine[7][2]=1;const auto back=axisY(pose(spine).global[7]);
    std::fprintf(stderr,"  antebraço dobrado (%.2f %.2f %.2f) canela dobrada (%.2f %.2f %.2f) coxa frente(+1) (%.2f %.2f %.2f) coluna frente(+1) (%.2f %.2f %.2f)\n",
                 fore[0],fore[1],fore[2],shin[0],shin[1],shin[2],leg[0],leg[1],leg[2],back[0],back[1],back[2]);
  }
  Muscles bent{};bent[3][2]=-1;
  const auto knee=axisY(pose(bent).global[3]),straight=axisY(rest.global[3]);
  AE_EXPECT_TRUE(knee[1]>straight[1]+.5,"Lower Leg Stretch -1 folds the knee");
}

AE_TEST(unity_humanoid_retarget_to_the_reference_skeleton_returns_its_rotations) {
  UnityHumanoidCurves curves;
  curves.muscles[13][2]=constant(.6f,1);curves.present[13][2]=true;
  curves.muscles[15][2]=constant(-.4f,1);curves.present[15][2]=true;
  curves.muscles[7][0]=constant(.3f,1);curves.present[7][0]=true;curves.muscleCurves=3;
  const auto target=unityHumanoidReferenceSkeleton();
  AnimationClipAsset clip;clip.name="Referência";clip.guid=assetGuidFromSeed("unity-humanoid-reference");clip.displayRate=30;
  std::string error;UnityHumanoidReport report;
  AE_EXPECT_TRUE(appendUnityHumanoidTracks(curves,target,1,30,clip,report,error),error.c_str());
  AE_EXPECT_EQ(report.samples,31u,"30 Hz from 0 to 1 s");
  Muscles muscles{};muscles[13][2]=.6f;muscles[15][2]=-.4f;muscles[7][0]=.3f;
  const auto expected=pose(muscles);
  // Cada osso humano recebe a rotação local da referência (sinal livre).
  u32 checked=0;
  for(const auto &binding:clip.bindings)for(const auto &track:clip.tracks)if(track.binding==binding.id&&track.path==AnimationPath::Rotation) {
    const auto bone=unityHumanBoneIndex(binding.path);AE_EXPECT_TRUE(bone>=0,"binding is a human bone");if(bone<0)continue;
    AnimationCurveSample x,y,z,w;
    AE_EXPECT_TRUE(sampleAnimationCurve(track.curves[0],.5,x)&&sampleAnimationCurve(track.curves[1],.5,y)&&
                   sampleAnimationCurve(track.curves[2],.5,z)&&sampleAnimationCurve(track.curves[3],.5,w),"rotation samples");
    const auto &q=expected.local[static_cast<u32>(bone)];
    const double dot=std::abs(x.value*q[0]+y.value*q[1]+z.value*q[2]+w.value*q[3]);
    if(bone)AE_EXPECT_TRUE(dot>.9999,("local rotation matches the reference: "+binding.path).c_str());
    ++checked;
  }
  AE_EXPECT_EQ(checked,UnityHumanBoneCount,"one rotation track per human bone");
  AE_EXPECT_TRUE(clip.valid(&error),error.c_str());
}

// Pacote real (opcional): AE_UMOTION_EXAMPLES aponta para a pasta UMotionExamples
// do UMotion Pro. O Kyle entra pelo FBX e pelo .meta (Avatar), o PickUp.anim
// Humanoid é convertido pelo mesmo comando da Biblioteca, e a caixa (curvas
// próprias do clipe, feitas presas às mãos no UMotion) prova a pose: enquanto
// ela sobe, as mãos ficam junto dela.
AE_TEST(unity_humanoid_kyle_pickup_from_the_umotion_package_follows_the_box) {
  const char *examples=std::getenv("AE_UMOTION_EXAMPLES");
  if(!examples){std::fprintf(stderr,"  AE_UMOTION_EXAMPLES ausente: Kyle/PickUp nao verificados\n");return;}
  namespace fs=std::filesystem;using namespace ae::editor;
  const fs::path source(examples);
  const auto project=fs::temp_directory_path()/("astra-humanoid-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup {fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{project};
  fs::create_directories(project/".astra/import-staging/unity");
  std::vector<LibrarySourceFile> files;
  const auto add=[&](const fs::path &from,const std::string &relative) {
    std::vector<u8> bytes;if(!EditorImportTransaction::read(from,bytes))return false;
    const auto target=project/".astra/import-staging/unity"/relative;fs::create_directories(target.parent_path());
    if(!EditorImportTransaction::write(target,bytes))return false;
    files.push_back({relative,Sha256::hex(bytes),bytes.size(),{}});return true;
  };
  const auto model=source/"Miscellaneous/Robot Kyle/Model/Robot Kyle.fbx",clip=source/"Exported Animations/PickUp.anim";
  EditorSession session;
  std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials);
  session.importMap(draws,materials,false,vertices,indices,0);
  session.setGeometryPublisher([&](std::span<const u8> v,std::span<const u32> i,std::span<const renderer::MapDrawRecord> d,
      std::span<const renderer::MapMaterialRecord> m,std::span<const renderer::SharedAuthoringTexture>,EditorSession::PublishedGeometry &out) {
    vertices.clear();indices.clear();draws.clear();materials.clear();
    if(!renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials))return false;
    const auto vb=static_cast<u32>(vertices.size()/renderer::MapVertexStride),ib=static_cast<u32>(indices.size()),mb=static_cast<u32>(materials.size());
    vertices.insert(vertices.end(),v.begin(),v.end());indices.insert(indices.end(),i.begin(),i.end());materials.insert(materials.end(),m.begin(),m.end());
    for(auto draw:d){draw.firstIndex+=ib;draw.vertexOffset+=vb;draw.materialIndex+=mb;draw.lodGroupId=static_cast<u32>(draws.size());draws.push_back(draw);}
    out={draws,materials,vertices,indices};return true;
  });
  // Abrir o projeto limpa a pasta de preparo: o pacote é copiado depois.
  AE_EXPECT_TRUE(session.setProjectDirectory(project.string().c_str()),"project");
  AE_EXPECT_TRUE(add(model,"Kyle/Robot Kyle.fbx")&&add(fs::path(model.string()+".meta"),"Kyle/Robot Kyle.fbx.meta")&&
                 add(clip,"Kyle/PickUp.anim")&&add(fs::path(clip.string()+".meta"),"Kyle/PickUp.anim.meta"),"package files");
  std::string error;auto package=prepareLibraryPackage(project.string(),".astra/import-staging/unity","UMotion",files);
  AE_EXPECT_TRUE(package.accepted,package.diagnostic.c_str());
  AE_EXPECT_TRUE(package.accepted&&session.importLibraryPackage(package,error),error.c_str());
  LibraryItem modelItem,clipItem;
  for(const auto &item:package.package.items){if(item.path.ends_with("Robot Kyle.fbx"))modelItem=item;if(item.path.ends_with("PickUp.anim"))clipItem=item;}
  std::vector<u8> bytes;AE_EXPECT_TRUE(EditorImportTransaction::read(model,bytes),"FBX bytes");
  GltfImport converted;ImportProfile profile;
  AE_EXPECT_TRUE(importModelSource(bytes,applyImportProfile(session.importLimits(),profile),{},converted),converted.diagnostic.c_str());
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.commitModelImport(bytes,converted,modelItem.path,Sha256::hex(bytes),report,ImportAmbiguityPolicy::Refuse,{},{},{},{},&profile),report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source,report),report.diagnostic.c_str());
  const auto owner=session.selection();auto &document=session.document();
  const auto named=[&](std::string_view name) {
    std::vector<EditorEntityId> ids;document.collectSubtree(owner,ids);
    for(const auto id:ids)if(const auto *e=document.find(id);e&&name==e->name)return id;
    return EditorEntityId{};
  };
  // A caixa do exemplo é filha do Root (o quadril) na cena do UMotion.
  const auto hipsBone=named("Root");AE_EXPECT_TRUE(hipsBone!=0,"Kyle hips bone");
  const auto box=document.createEntity(hipsBone,EditorEntityKind::Folder,"Cube");
  const auto converted_clip=session.importUnityAsset(clipItem.path,owner,UnityReimportPolicy::RefuseConflict,error);
  AE_EXPECT_TRUE(converted_clip.valid(),error.c_str());if(!converted_clip.valid())return;
  const auto *asset=session.animationClipAsset(converted_clip);AE_EXPECT_TRUE(asset!=nullptr,"converted clip resource");if(!asset)return;
  editor::AnimationClipPreview preview;AE_EXPECT_TRUE(preview.begin(document,owner,*asset,error),error.c_str());
  const auto leftHand=named("Left_Wrist_Joint_01"),rightHand=named("Right_Wrist_Joint_01"),leftFoot=named("Left_Ankle_Joint_01"),rightFoot=named("Right_Ankle_Joint_01"),head=named("Head");
  AE_EXPECT_TRUE(leftHand&&rightHand&&leftFoot&&rightFoot&&head,"Kyle bones by Avatar names");
  const auto at=[&](EditorEntityId id){float m[16];runtime::worldMatrix(preview.scene(),id,m);return std::array<float,3>{m[12],m[13],m[14]};};
  float ownerMatrix[16];runtime::worldMatrix(document,owner,ownerMatrix);
  AE_EXPECT_TRUE(preview.seek(0,error),error.c_str());
  const auto feet=std::min(at(leftFoot)[1],at(rightFoot)[1])-ownerMatrix[13];
  std::fprintf(stderr,"  t=0: pes y=%.3f cabeca y=%.3f quadril y=%.3f\n",feet,at(head)[1]-ownerMatrix[13],at(hipsBone)[1]-ownerMatrix[13]);
  AE_EXPECT_TRUE(feet>-.05f&&feet<.2f,"feet on the ground at the start");
  AE_EXPECT_TRUE(at(head)[1]>at(hipsBone)[1]+.4f,"head well above the hips");
  if(std::getenv("AE_UMOTION_TRACE")) {
    float q[4];runtime::transformRotationQuaternion(document.find(hipsBone)->transform,q);
    std::fprintf(stderr,"  Root descanso Astra (%.4f %.4f %.4f %.4f) pos (%.4f %.4f %.4f)\n",q[0],q[1],q[2],q[3],
                 document.find(hipsBone)->transform.position[0],document.find(hipsBone)->transform.position[1],document.find(hipsBone)->transform.position[2]);
    const auto *parent=document.find(document.find(hipsBone)->parent);
    std::fprintf(stderr,"  pai do Root: %s escala %.4f\n",parent?parent->name:"?",parent?parent->transform.scale[0]:0.f);
    for(const char *bone:{"Ribs","Left_Shoulder_Joint_01","Left_Upper_Arm_Joint_01","Left_Forearm_Joint_01","Left_Wrist_Joint_01","Left_Thigh_Joint_01"})
      if(const auto id=named(bone)) {
        float r[4];runtime::transformRotationQuaternion(document.find(id)->transform,r);const auto *e=document.find(id);
        std::fprintf(stderr,"  %s descanso (%.4f %.4f %.4f %.4f) pos (%.4f %.4f %.4f)\n",bone,r[0],r[1],r[2],r[3],e->transform.position[0],e->transform.position[1],e->transform.position[2]);
      }
  }
  if(std::getenv("AE_UMOTION_TRACE"))for(float t=0;t<=asset->duration+1e-4f;t+=.1f) {
    preview.seek(std::min(t,asset->duration),error);const auto l=at(leftHand),r=at(rightHand),c=at(box),h=at(hipsBone);
    std::fprintf(stderr,"  t=%.1f caixa(%.2f %.2f %.2f) maoE(%.2f %.2f %.2f) maoD(%.2f %.2f %.2f) quadril(%.2f %.2f %.2f)\n",
                 t,c[0],c[1],c[2],l[0],l[1],l[2],r[0],r[1],r[2],h[0],h[1],h[2]);
  }
  // Distância da caixa à mão mais próxima enquanto ela é carregada (0,7 a 2,2 s:
  // passa da mão direita para a esquerda). A referência é a reconstrução do
  // HumanPoseHandler, não a Unity; o limite mede a conversão atual (média
  // 0,135 m, pior 0,342 m) e falha se ela regredir.
  const auto distance=[](std::array<float,3> a,std::array<float,3> b){return std::sqrt((a[0]-b[0])*(a[0]-b[0])+(a[1]-b[1])*(a[1]-b[1])+(a[2]-b[2])*(a[2]-b[2]));};
  double sum=0,worst=0,best=1e9;u32 n=0;
  for(float t=.7f;t<=2.2f;t+=.05f) {
    preview.seek(t,error);const auto c=at(box);
    const double nearest=std::min(distance(at(leftHand),c),distance(at(rightHand),c));
    sum+=nearest;worst=std::max(worst,nearest);best=std::min(best,nearest);++n;
  }
  std::fprintf(stderr,"  caixa-mão mais próxima em 0,7..2,2 s: média %.3f m, pior %.3f m, melhor %.3f m\n",sum/n,worst,best);
  AE_EXPECT_TRUE(sum/n<.16,"the box stays near a hand while it is carried");
  AE_EXPECT_TRUE(worst<.38,"no carried frame leaves the box far from both hands");
  AE_EXPECT_TRUE(best<.08,"a hand reaches the box");
}
