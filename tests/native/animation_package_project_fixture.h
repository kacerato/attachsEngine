#pragma once
#include "scene/animation.h"
#include "scene/light.h"
#include "scene/environment.h"

namespace ae::test {
// Opt-in, private acceptance project. Source packages are not bundled in the
// public APK: callers supply the GLBs produced by import_character.py.
inline int writeAnimationPackageProject(const std::filesystem::path &library,
                                       const std::filesystem::path &out) {
  using namespace editor;
  if(std::filesystem::exists(out))return 181;
  std::filesystem::create_directories(out/"scenes");
  EditorSession session;std::string error;
  if(!session.setProjectDirectory(out.generic_string().c_str()))return 182;
  std::vector<u8> vertices;std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  if(!renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials)||
     !session.importMap(draws,materials,false,vertices,indices,0))return 183;
  const auto publishGeometry=[&](std::span<const u8> v,std::span<const u32> i,
      std::span<const renderer::MapDrawRecord> d,std::span<const renderer::MapMaterialRecord> m,
      std::span<const renderer::SharedAuthoringTexture>,EditorSession::PublishedGeometry &published) {
    vertices.clear();indices.clear();draws.clear();materials.clear();
    if(!renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials))return false;
    const auto vb=static_cast<u32>(vertices.size()/renderer::MapVertexStride);
    const auto ib=static_cast<u32>(indices.size()),mb=static_cast<u32>(materials.size());
    vertices.insert(vertices.end(),v.begin(),v.end());indices.insert(indices.end(),i.begin(),i.end());
    materials.insert(materials.end(),m.begin(),m.end());
    for(auto draw:d){draw.firstIndex+=ib;draw.vertexOffset+=vb;draw.materialIndex+=mb;draws.push_back(draw);}
    published={draws,materials,vertices,indices};return true;
  };
  session.setGeometryPublisher(publishGeometry);
  auto &graph=session.document();
  const char *names[]{"RobotKyle","Viking","VikingWalk","VikingRun"};
  u32 clipCount=0;
  for(u32 index=0;index<4;++index) {
    const auto bytes=read(library/(std::string(names[index])+".glb"));
    // importModel publishes an already present project source; the shell's
    // file importer normally performs this copy before calling the session.
    std::filesystem::create_directories(out/"Sources");
    if(bytes.empty()||!EditorImportTransaction::write(out/(std::string("Sources/")+names[index]+".glb"),bytes))return 184;
    EditorSession::ModelImportReport report;
    if(bytes.empty()||!session.importModel(bytes,std::string("Sources/")+names[index]+".glb",{},report)) {
      std::fprintf(stderr,"IMPORT %s: %s\n",names[index],report.diagnostic.c_str());return 184;
    }
    const auto imported=session.selection();
    const auto slot=graph.createEntity(graph.root(),runtime::ObjectKind::Folder,names[index]);
    if(!slot||!graph.reparent(imported,slot,0))return 185;
    auto entity=*graph.find(slot);entity.transform.position[0]=(float(index)-1.5f)*2.2f;
    if(!graph.applyEntityValues(slot,entity))return 186;
    // Copy the IDs: extraction republishes the library and invalidates views.
    std::vector<resources::AssetGuid> clips;
    for(const auto &entry:session.mapScene().clipCatalog())
      if(entry.source==report.source)clips.push_back(entry.clip);
    for(auto source:clips) {
      resources::AssetGuid authored;
      if(!session.extractAnimationClip(source,imported,authored,error)) {
        std::fprintf(stderr,"EXTRACT %s: %s\n",names[index],error.c_str());return 187;
      }
      ++clipCount;
    }
    if(index==0) {
      std::vector<EditorEntityId> nodes;graph.collectSubtree(imported,nodes);EditorEntityId arm=0;
      for(auto id:nodes)if(std::string_view(graph.find(id)->name)=="Right_Upper_Arm_Joint_01")arm=id;
      resources::AssetGuid authored;u64 track=0;
      if(!arm||!session.createAnimationClip(imported,2,authored,error,resources::AnimationRotationMode::Euler)||
         !session.addAnimationClipTrack(authored,session.animationClipAsset(authored)->revision,imported,arm,
             resources::AnimationPath::Rotation,resources::AnimationRotationMode::Euler,track,error)||
         !session.openAnimationClip(authored,imported,error)||!session.seekAnimationClip(1,error))return 188;
      const auto &transform=graph.find(arm)->transform;
      const float pose[]{transform.rotationDegrees[0],transform.rotationDegrees[1],transform.rotationDegrees[2]+35};
      if(!session.previewAnimationClipPose(track,pose,error)||!session.recordAnimationClipPose(error))return 189;
      session.closeAnimationClip();
      auto actor=*graph.find(imported);
      auto *animation=static_cast<scene::Animation*>(actor.components.add(scene::Animation::descriptor));
      if(!animation)return 190;
      animation->clip=authored;animation->appendClip(authored);animation->playAutomatically=true;
      if(!graph.applyEntityValues(imported,actor))return 191;
      ++clipCount;
    }
    const auto retained=read(out/(std::string("Sources/")+names[index]+".glb"));
    if(retained!=bytes)return 192;
    // Audit accompanies the private project; no original Unity package is copied.
    const auto audit=read(library/(std::string(names[index])+".import.json"));
    if(!EditorImportTransaction::write(out/(std::string("Sources/")+names[index]+".import.json"),audit))return 193;
  }
  const auto floor=graph.createEntity(graph.root(),runtime::ObjectKind::Mesh,"Palco");auto entity=*graph.find(floor);
  if(!runtime::configurePrimitive(entity,scene::PrimitiveType::Cube,
      {1,session.mapScene().assetGuid(0),session.mapScene().materialForAsset(0)}))return 194;
  entity.transform.position[1]=-.1f;entity.transform.scale[0]=12;entity.transform.scale[1]=.2f;entity.transform.scale[2]=5;
  auto *mesh=runtime::editMeshRenderer(entity);mesh->material.enabled=true;
  mesh->material.baseColor[0]=.23f;mesh->material.baseColor[1]=.25f;mesh->material.baseColor[2]=.28f;
  mesh->material.roughness=.85f;
  if(!graph.applyEntityValues(floor,entity))return 195;
  const auto sun=graph.createEntity(graph.root(),runtime::ObjectKind::Folder,"Luz do palco");entity=*graph.find(sun);
  entity.transform.rotationDegrees[0]=45;entity.transform.rotationDegrees[1]=-25;
  auto *light=static_cast<scene::Light*>(entity.components.add(scene::Light::descriptor));
  light->kind=scene::LightKind::Directional;light->unit=scene::LightUnit::Engine;light->intensity=2;
  if(!graph.applyEntityValues(sun,entity))return 196;
  const auto environment=graph.createEntity(graph.root(),runtime::ObjectKind::Folder,"Atmosfera");entity=*graph.find(environment);
  auto &env=static_cast<scene::Environment*>(entity.components.add(scene::Environment::descriptor))->values;
  env.sky=renderer::SkyModel::PhysicalAtmosphere;env.physicalSkyIntensity=.6f;env.bloom=false;
  if(!graph.applyEntityValues(environment,entity))return 197;
  const float eye[]{0,2,-9};session.setCameraPose(eye,0,.14f);session.saveSceneView("Palco de autoria");
  if(!session.save((out/"scenes/editor.aescene").generic_string().c_str(),0))return 198;
  const std::string descriptor="{\"format\":\"ASTRA-PROJECT-1\",\"resourceSource\":\"independent\",\"project\":{\"name\":\"Animation Studio - Pacotes\",\"template\":\"empty\",\"scenes\":1,\"assets\":8},\"mainScene\":\"scenes/editor.aescene\",\"editorScene\":\"scenes/editor.aescene\"}";
  if(!EditorImportTransaction::writeText(out/"project.json",descriptor)||
     !EditorImportTransaction::writeText(out/"LEIA-ME.md",
       "# Biblioteca privada de autoria\n\nKyle: UMotion. Viking, Idle, Walk e Run: FinalIK. Fontes e materiais fornecidos pelo proprietario para testes. Quatro rigs originais separados; Walk e Run nao sao retargeting para Idle. Clipes extraidos sao recursos editaveis independentes, preservando os GLBs. Kyle demonstra autoria de um osso real, nao animacao original do pacote. Nao inclui scripts, controllers ou shaders Unity.\n"))return 199;
  if(clipCount!=4)return 200;
  EditorSession reopened;reopened.setGeometryPublisher(publishGeometry);
  const auto registry=read(out/".astra/assets.astra");
  if(!reopened.setProjectDirectory(out.generic_string().c_str())||
     !reopened.loadAssets(std::string_view(reinterpret_cast<const char*>(registry.data()),registry.size())))return 201;
  std::vector<EditorSession::ReopenedSource> sources;
  for(const auto *name:names) {
    const auto bytes=read(out/(std::string("Sources/")+name+".glb"));resources::GltfImport model;
    if(!resources::importGlb(bytes,{},{},model))return 202;
    sources.push_back({std::move(model),Sha256::hex(bytes),std::string("Sources/")+name+".glb"});
  }
  std::vector<EditorSession::ModelImportReport> reports;
  if(!reopened.reopenSources(sources,reports,error)||
     !reopened.load((out/"scenes/editor.aescene").generic_string().c_str(),0))return 203;
  for(const auto &record:session.assets().records())if(const auto *clip=session.animationClipAsset(record.guid)) {
    const auto *restored=reopened.animationClipAsset(record.guid);
    if(!restored||restored->serialize()!=clip->serialize())return 204;
  }
  runtime::SceneGraph playing=reopened.document();runtime::SceneAnimator player;
  player.begin(playing,reopened.mapScene());
  if(!player.advance(.4,{})||!player.advance(.2,{}))return 205;
  player.reset();
  std::printf("ANIMATION_PACKAGE_PROJECT sources=4 authored_clips=%u preserved=true path=%s\n",clipCount,out.generic_string().c_str());
  return 0;
}
}
