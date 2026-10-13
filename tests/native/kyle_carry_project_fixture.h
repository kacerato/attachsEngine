#pragma once
#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "renderer/authoring_geometry.h"
#include "renderer/primitive_geometry.h"
#include "runtime/transform_math.h"
#include "scene/collider.h"
#include "scene/environment.h"
#include "scene/light.h"
#include "scene/physics_body.h"
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace ae::test {
// Projeto privado "Kyle - Carregar caixa": so o Robot Kyle do UMotion Pro
// (GLB gerado por tools/animation-studio/import_character.py), palco, luz e a
// interacao de carregar com as duas maos feita pelos componentes universais
// (Objeto interagivel + Sistema de interacao). Nenhum clipe: o corpo fica na
// pose de repouso e so o IK move bracos, tronco e pelve.
inline int writeKyleCarryProject(const std::filesystem::path &glb,const std::filesystem::path &out) {
  using namespace editor;
  const auto read=[](const std::filesystem::path &p){std::vector<u8> b;if(FILE *f=std::fopen(p.string().c_str(),"rb")){int c;while((c=std::fgetc(f))!=EOF)b.push_back(static_cast<u8>(c));std::fclose(f);}return b;};
  if(std::filesystem::exists(out))return 181;
  std::filesystem::create_directories(out/"scenes");std::filesystem::create_directories(out/"Sources");
  EditorSession session;std::string error;
  if(!session.setProjectDirectory(out.generic_string().c_str()))return 182;
  std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  const auto library=[&]{vertices.clear();indices.clear();draws.clear();materials.clear();
    return renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials)&&
           renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials);};
  if(!library()||!session.importMap(draws,materials,false,vertices,indices,0))return 183;
  session.setGeometryPublisher([&](std::span<const u8> v,std::span<const u32> i,std::span<const renderer::MapDrawRecord> d,
      std::span<const renderer::MapMaterialRecord> m,std::span<const renderer::SharedAuthoringTexture>,EditorSession::PublishedGeometry &published) {
    if(!library())return false;
    const auto vb=static_cast<u32>(vertices.size()/renderer::MapVertexStride),ib=static_cast<u32>(indices.size()),mb=static_cast<u32>(materials.size());
    vertices.insert(vertices.end(),v.begin(),v.end());indices.insert(indices.end(),i.begin(),i.end());materials.insert(materials.end(),m.begin(),m.end());
    for(auto draw:d){draw.firstIndex+=ib;draw.vertexOffset+=vb;draw.materialIndex+=mb;draws.push_back(draw);}
    published={draws,materials,vertices,indices};return true;
  });
  auto &graph=session.document();
  const auto bytes=read(glb);
  if(bytes.empty()||!EditorImportTransaction::write(out/"Sources/RobotKyle.glb",bytes))return 184;
  EditorSession::ModelImportReport report;
  if(!session.importModel(bytes,"Sources/RobotKyle.glb",{},report)){std::fprintf(stderr,"IMPORT: %s\n",report.diagnostic.c_str());return 185;}
  const auto imported=session.selection();
  const auto kyle=graph.createEntity(graph.root(),runtime::ObjectKind::Folder,"Robot Kyle");
  if(!kyle||!graph.reparent(imported,kyle,0))return 186;
  {
    // Palco com colisao: o apoio dos pes pisa nele quando o corpo agacha.
    const auto floor=graph.createEntity(graph.root(),runtime::ObjectKind::Mesh,"Palco");auto entity=*graph.find(floor);
    if(!runtime::configurePrimitive(entity,scene::PrimitiveType::Cube,{1,session.mapScene().assetGuid(0),session.mapScene().materialForAsset(0)}))return 187;
    entity.transform.position[1]=-.1f;entity.transform.scale[0]=6;entity.transform.scale[1]=.2f;entity.transform.scale[2]=6;
    auto *mesh=runtime::editMeshRenderer(entity);mesh->material.enabled=true;
    mesh->material.baseColor[0]=.23f;mesh->material.baseColor[1]=.25f;mesh->material.baseColor[2]=.28f;mesh->material.roughness=.85f;
    if(!entity.components.find(scene::PhysicsBody::descriptor)&&!entity.components.add(scene::PhysicsBody::descriptor))return 188;
    if(!entity.components.find(scene::Collider::descriptor)&&!entity.components.add(scene::Collider::descriptor))return 188;
    if(!graph.applyEntityValues(floor,entity))return 189;
    const auto sun=graph.createEntity(graph.root(),runtime::ObjectKind::Folder,"Luz do palco");entity=*graph.find(sun);
    entity.transform.rotationDegrees[0]=45;entity.transform.rotationDegrees[1]=-25;
    auto *light=static_cast<scene::Light*>(entity.components.add(scene::Light::descriptor));
    light->kind=scene::LightKind::Directional;light->unit=scene::LightUnit::Engine;light->intensity=2;
    if(!graph.applyEntityValues(sun,entity))return 190;
    const auto atmosphere=graph.createEntity(graph.root(),runtime::ObjectKind::Folder,"Atmosfera");entity=*graph.find(atmosphere);
    auto &env=static_cast<scene::Environment*>(entity.components.add(scene::Environment::descriptor))->values;
    env.sky=renderer::SkyModel::Atmosphere;env.bloom=false;const renderer::SceneEnvironment daylight;
    std::copy_n(daylight.skyZenith,3,env.skyZenith);std::copy_n(daylight.skyHorizon,3,env.skyHorizon);std::copy_n(daylight.ground,3,env.ground);
    if(!graph.applyEntityValues(atmosphere,entity))return 191;
  }
  // Interacao: mesa, caixa com duas pegadas, ponto de carregar e componentes.
  if(!session.history().begin("Carregar caixa"))return 192;
  const auto group=session.history().createEntity(graph,graph.root(),EditorEntityKind::Folder,"Carregar caixa");
  if(!group||!session.addCarrySetupInTransaction(kyle,group,error)){std::fprintf(stderr,"CARRY: %s\n",error.c_str());session.history().cancel(graph);return 193;}
  session.history().end();
  // Vista lateral: o corpo de perfil, a mesa na frente.
  float hips[16]{};std::vector<EditorEntityId> ids;graph.collectSubtree(kyle,ids);
  for(auto id:ids)if(std::string_view(graph.find(id)->name)=="Hip")runtime::worldMatrix(graph,id,hips);
  const float eye[]{hips[12]+3.2f,1.3f,hips[14]+.6f};session.setCameraPose(eye,-1.45f,.08f);session.saveSceneView("Kyle de perfil");
  if(!session.save((out/"scenes/editor.aescene").generic_string().c_str(),0))return 194;
  const std::string descriptor="{\"format\":\"ASTRA-PROJECT-1\",\"resourceSource\":\"independent\",\"project\":{\"name\":\"Kyle - Carregar caixa\",\"template\":\"empty\",\"scenes\":1,\"assets\":2},\"mainScene\":\"scenes/editor.aescene\",\"editorScene\":\"scenes/editor.aescene\"}";
  if(!EditorImportTransaction::writeText(out/"project.json",descriptor)||
     !EditorImportTransaction::writeText(out/"LEIA-ME.md","# Kyle - Carregar caixa\n\nRobot Kyle (UMotion Pro, fonte do proprietario) sem clipe. A caixa e pega com as duas maos pelos componentes Objeto interagivel e Sistema de interacao (Final IK 2.2, Interaction Pick Up 2-Handed, como referencia). De Play.\n"))return 195;
  // Play no host: as duas maos chegam as pegadas e a caixa sobe ao ponto.
  if(!session.startPlay())return 196;
  std::vector<renderer::MapDrawState> frames;
  for(u32 i=0;i<240;++i){session.advanceClock(10+float(i)/60.f);session.extractPlayMap(frames);}
  const auto &g=session.playScene().document();
  const auto find=[&](const char *name){std::vector<runtime::ObjectId> all;g.collectSubtree(g.root(),all);for(auto id:all)if(std::string_view(g.find(id)->name)==name)return id;return runtime::ObjectId{};};
  const auto gap=[&](const char *a,const char *b){float m[16],n[16];runtime::worldMatrix(g,find(a),m);runtime::worldMatrix(g,find(b),n);return std::hypot(m[12]-n[12],m[13]-n[13],m[14]-n[14]);};
  const float left=gap("Left_Wrist_Joint_01","Pegada esq."),right=gap("Right_Wrist_Joint_01","Pegada dir."),hold=gap("Caixa","Ponto de carregar");
  std::printf("KYLE_CARRY left=%.3f right=%.3f hold=%.3f parent=%s\n",left,right,hold,std::string(g.find(g.find(find("Caixa"))->parent)->name).c_str());
  return left<.04f&&right<.04f&&hold<.04f?0:197;
}
} // namespace ae::test
