#pragma once
#include "editor/editor_collider_fit.h"
#include "scene/animation.h"
#include "scene/environment.h"
#include "scene/light.h"
#include "resources/texture_asset.h"

namespace ae::test {
// Assets reais passam pela mesma importação/publicação/persistência do editor.
inline editor::EditorEntityId u07Human(editor::EditorSession &s,const std::filesystem::path &root,
                                     const std::filesystem::path &out,std::string &error) {
  auto &g=s.document();const auto actor=g.createEntity(g.root(),runtime::ObjectKind::Folder,"PlayerDynamic");
  auto e=*g.find(actor);e.transform.position[1]=2;
  auto *body=static_cast<scene::PhysicsBody*>(e.components.add(scene::PhysicsBody::descriptor));
  body->motion=scene::BodyMotion::Dynamic;body->mass=17;body->freezeRotation[0]=body->freezeRotation[2]=true;body->freezeRotation[1]=false;
  if(!g.applyEntityValues(actor,e))return 0;
  const auto import=[&](const char *name,editor::EditorEntityId parent,float scale,float x) {
    const auto bytes=read(root/(std::string("tests/native/fixtures/gltf/")+name+".glb"));
    resources::GltfImport model;editor::EditorSession::ModelImportReport report;
    if(!bytes.empty()&&resources::importGlb(bytes,{},{},model)) {
      const float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
      for(const auto &node:model.nodes){runtime::Transform pose;if(!runtime::localTransformForWorld(node.localMatrix,identity,pose)){std::fprintf(stderr,"U07_TRS_FAIL %s",node.name.c_str());for(float v:node.localMatrix)std::fprintf(stderr," %.9g",v);std::fprintf(stderr,"\n");}}
    }
    if(bytes.empty()||!resources::importGlb(bytes,{},{},model)||
       !s.commitModelImport(bytes,model,std::string("Sources/")+name+".glb","",report)||
       !s.instantiateModel(report.source,report,true)){error=model.diagnostic+" "+report.diagnostic;return editor::EditorEntityId(0);}
    const auto imported=s.selection();std::vector<editor::EditorEntityId> ids;g.collectSubtree(imported,ids);
    for(auto id:ids){auto values=*g.find(id);while(values.components.remove(scene::PhysicsBody::descriptor)){}while(values.components.remove(scene::Collider::descriptor)){}if(!g.applyEntityValues(id,values))return editor::EditorEntityId(0);}
    if(!g.reparent(imported,parent,0))return editor::EditorEntityId(0);
    auto values=*g.find(imported);values.transform.scale[0]*=scale;values.transform.scale[1]*=scale;values.transform.scale[2]*=scale;values.transform.position[0]+=x;
    if(!g.applyEntityValues(imported,values))return editor::EditorEntityId(0);
    std::filesystem::create_directories(out/"Sources");
    if(!editor::EditorImportTransaction::write(out/(std::string("Sources/")+name+".LICENSE.txt"),read(root/(std::string("tests/native/fixtures/gltf/")+name+".LICENSE.txt"))))return editor::EditorEntityId(0);
    return imported;
  };
  const auto human=import("GodotTPSPlayer",actor,1,0);if(!human)return 0;
  std::vector<editor::EditorEntityId> ids;g.collectSubtree(human,ids);u32 shapes=0;
  for(auto id:ids)if(g.find(id)->components.find(scene::SkinnedMesh::descriptor)) {
    auto values=*g.find(id);scene::Collider fitted;
    if(!editor::fitEditorCollider(s.mapScene(),g,id,fitted)){error="Não foi possível medir a malha humana deformável";return 0;}
    fitted.owner=actor;
    if(!values.components.add(scene::Collider::descriptor)||!values.components.replace(fitted))return 0;
    if(!g.applyEntityValues(id,values))return 0;
    ++shapes;
  }
  if(!shapes||!s.configureDynamicMotor(actor,editor::EditorSession::MotorCollisionPolicy::Preserve,error))return 0;
  auto authored=*g.find(actor);
  auto *motor=static_cast<scene::DynamicBodyMotor*>(authored.components.edit(scene::DynamicBodyMotor::descriptor));
  motor->control.source=scene::MotorControlSource::Ui;motor->speed=3.6f;motor->airControl=.85f;
  if(!g.applyEntityValues(actor,authored))return 0;
  // Fox usa Survey no início: um segundo rig visível, sem fingir controlador próprio.
  if(!import("Fox",g.root(),.018f,3.5f))return 0;
  s.setSelection(actor);return actor;
}
inline bool u07Environment(editor::EditorSession &s,editor::EditorEntityId floor,
                          const std::filesystem::path &root,const std::filesystem::path &out,std::string &error) {
  resources::AssetGuid textures[3];
  const char *files[]{"base.jpg","normal.png","arm.png"};
  for(u32 i=0;i<3;++i) {
    const auto bytes=read(root/"games/realism-collection/sources/concrete_floor_02"/files[i]);
    resources::TextureProfile profile;profile.maximumDimension=1024;
    profile.interpretation=i==0?resources::TextureInterpretationColor:i==1?resources::TextureInterpretationNormal:resources::TextureInterpretationData;
    resources::TextureImportLimits limits;limits.projectMaximumDimension=s.importLimits().maximumTextureDimension;limits.image=s.importLimits().image;
    resources::PreparedTextureImport prepared;
    const auto path=std::string("Textures/concrete-")+files[i];
    if(bytes.empty()||!resources::prepareTextureImport(bytes,profile,true,editor::EditorMapScene::DefaultTextureSampler,limits,nullptr,prepared)||
       !s.commitTextureImport(path,bytes,"",prepared,profile,error)){error+=" "+prepared.diagnostic;return false;}
    const auto *asset=s.assets().findByPath(path);if(!asset)return false;textures[i]=asset->guid;
  }
  auto &g=s.document();auto e=*g.find(floor);auto *mesh=runtime::editMeshRenderer(e);
  mesh->material.enabled=true;mesh->material.roughness=.8f;mesh->material.metallic=0;
  mesh->textures[0]=textures[0];mesh->textures[1]=textures[1];mesh->textures[2]=textures[2];
  mesh->occlusionTexture=textures[2];mesh->channels.occlusionSource=scene::MaterialOcclusionTexture;
  mesh->channels.occlusion=scene::MaterialChannelR;mesh->channels.roughness=scene::MaterialChannelG;mesh->channels.metallic=scene::MaterialChannelB;
  for(auto &sampling:mesh->sampling){sampling.uvSet=scene::MaterialUvWorld;sampling.wrap=scene::MaterialWrapRepeat;sampling.scale[0]=sampling.scale[1]=.5f;}
  if(!g.applyEntityValues(floor,e))return false;
  auto id=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Atmosfera de teste");e=*g.find(id);
  auto &env=static_cast<scene::Environment*>(e.components.add(scene::Environment::descriptor))->values;
  env.sky=renderer::SkyModel::PhysicalAtmosphere;env.fog=true;env.fogDensity=.004f;env.fogStart=18;env.fogHeightFalloff=.1f;
  env.physicalSkyIntensity=.8f;env.aerosolDensity=.45f;env.bloom=false;env.exposureEv=0;
  if(!g.applyEntityValues(id,e))return false;
  id=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Sol");e=*g.find(id);e.transform.rotationDegrees[0]=50;e.transform.rotationDegrees[1]=-25;
  auto *sun=static_cast<scene::Light*>(e.components.add(scene::Light::descriptor));sun->kind=scene::LightKind::Directional;sun->unit=scene::LightUnit::Engine;sun->intensity=2;
  if(!g.applyEntityValues(id,e))return false;
  return editor::EditorImportTransaction::writeText(out/"FONTES.md",
    "# Ambiente U07\n\nHumanoide de teste Godot TPS Demo: 145 juntas, oito clipes, CC-BY-3.0, Juan Linietsky e Fernando Miguel Calabró. Fonte https://github.com/godotengine/tps-demo/blob/master/player/model/player.glb ; derivação reproduzível em tools/prepare-u07-motion-assets.py. Materiais de cor explícitos, sem declarar as texturas externas do Godot como importadas. Fox: PixelMannen/tomkranis/AsoboStudio/scurest, licenças em Sources.\n\nPiso concreto PBR: Poly Haven, CC0, https://polyhaven.com/a/concrete_floor_02 .\n\nMotorAnimationDriver usa velocidade e apoio reais, mistura e fase de passada; não é root motion, retargeting nem IK. Colisão medida na malha autoral vinculada ao Body, não por osso a cada quadro.\n");
}
}
