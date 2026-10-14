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
#include "editor/editor_umotion_import.h"
#include "runtime/scene_animation.h"
#include "resources/unity_yaml.h"
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace ae::test {
// Projeto privado "Kyle - Carregar caixa": so o Robot Kyle do UMotion Pro
// (GLB gerado por tools/animation-studio/import_character.py), palco, luz e a
// interacao de carregar com as duas maos feita pelos componentes universais
// (Objeto interagivel + Sistema de interacao). Nenhum clipe: o corpo fica na
// pose de repouso e so o IK move bracos, tronco e pelve.
// Palco do Kyle (projeto novo): biblioteca de primitivas como o shell, o GLB
// em Sources/, palco com colisao, luz e atmosfera. `kyle` e a pasta do modelo.
struct KyleStage {
  std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  editor::EditorEntityId kyle=0;
  // Mesma biblioteca do shell: so as primitivas antes das fontes. Outra ordem
  // desloca os slots de malha do modelo e ele abre trocado no aparelho.
  bool library() {vertices.clear();indices.clear();draws.clear();materials.clear();
    return renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials);}
  int open(const std::filesystem::path &glb,const std::filesystem::path &out,editor::EditorSession &session) {
    using namespace editor;
    const auto read=[](const std::filesystem::path &p){std::vector<u8> b;if(FILE *f=std::fopen(p.string().c_str(),"rb")){int c;while((c=std::fgetc(f))!=EOF)b.push_back(static_cast<u8>(c));std::fclose(f);}return b;};

  if(std::filesystem::exists(out))return 181;
  std::filesystem::create_directories(out/"scenes");std::filesystem::create_directories(out/"Sources");
  if(!session.setProjectDirectory(out.generic_string().c_str()))return 182;
  if(!library()||!session.importMap(draws,materials,false,vertices,indices,0))return 183;
  session.setGeometryPublisher([this](std::span<const u8> v,std::span<const u32> i,std::span<const renderer::MapDrawRecord> d,
      std::span<const renderer::MapMaterialRecord> m,std::span<const renderer::SharedAuthoringTexture>,EditorSession::PublishedGeometry &published) {
    if(!library())return false;
    const auto vb=static_cast<u32>(vertices.size()/renderer::MapVertexStride),ib=static_cast<u32>(indices.size()),mb=static_cast<u32>(materials.size());
    vertices.insert(vertices.end(),v.begin(),v.end());indices.insert(indices.end(),i.begin(),i.end());materials.insert(materials.end(),m.begin(),m.end());
    for(auto draw:d){draw.firstIndex+=ib;draw.vertexOffset+=vb;draw.materialIndex+=mb;draws.push_back(draw);}
    published={draws,materials,vertices,indices};return true;
  });
  auto &graph=session.document();
  const auto bytes=read(glb);
  // A extensao da fonte decide o importador (GLB convertido ou o FBX do pacote).
  const std::string source="Sources/RobotKyle"+glb.extension().string();
  if(bytes.empty()||!EditorImportTransaction::write(out/source,bytes))return 184;
  EditorSession::ModelImportReport report;
  if(!session.importModel(bytes,source,{},report)){std::fprintf(stderr,"IMPORT: %s\n",report.diagnostic.c_str());return 185;}
  const auto imported=session.selection();
  kyle=graph.createEntity(graph.root(),runtime::ObjectKind::Folder,"Robot Kyle");
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
    return 0;
  }
};
inline int writeKyleCarryProject(const std::filesystem::path &glb,const std::filesystem::path &out) {
  using namespace editor;
  EditorSession session;std::string error;KyleStage stage;
  if(const int code=stage.open(glb,out,session))return code;
  auto &graph=session.document();const auto kyle=stage.kyle;
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
  // Registro das fontes: e o que o shell grava; sem ele o projeto reabre com a
  // fonte ausente e o Kyle invisivel.
  std::filesystem::create_directories(out/".astra");
  if(!EditorImportTransaction::writeText(out/".astra/assets.astra",session.assets().serialize()))return 194;
  const std::string descriptor="{\"format\":\"ASTRA-PROJECT-1\",\"resourceSource\":\"independent\",\"project\":{\"name\":\"Kyle - Carregar caixa\",\"template\":\"empty\",\"scenes\":1,\"assets\":2},\"mainScene\":\"scenes/editor.aescene\",\"editorScene\":\"scenes/editor.aescene\"}";
  if(!EditorImportTransaction::writeText(out/"project.json",descriptor)||
     !EditorImportTransaction::writeText(out/"LEIA-ME.md","# Kyle - Carregar caixa\n\nRobot Kyle (UMotion Pro, fonte do proprietario) sem clipe. A caixa e pega com as duas maos pelos componentes Objeto interagivel e Sistema de interacao (Final IK 2.2, Interaction Pick Up 2-Handed, como referencia). De Play.\n"))return 195;
  // Play no host: as duas maos chegam as pegadas e a caixa sobe ao ponto.
  if(!session.startPlay())return 196;
  std::vector<renderer::MapDrawState> frames;
  const auto &g=session.playScene().document();
  const auto trace=std::getenv("AE_KYLE_TRACE")!=nullptr;
  // Corpo inteiro coerente: o peito desce com o quadril (sem esticar), o
  // tornozelo nao sai do chao e o joelho dobra para a frente, nao para o lado.
  const auto bone=[&](const char *n){std::vector<runtime::ObjectId> all;g.collectSubtree(g.root(),all);for(auto id:all)if(std::string_view(g.find(id)->name)==n){float m[16];runtime::worldMatrix(g,id,m);return std::array<float,3>{m[12],m[13],m[14]};}return std::array<float,3>{};};
  float hipRest=0,ribsRest=0,ankleRest[3]{},kneeRestX=0,maxStretch=0,maxAnkle=0,maxKneeSide=0,minHip=10;
  for(u32 i=0;i<240;++i){session.advanceClock(10+float(i)/60.f);session.extractPlayMap(frames);
    {const auto h=bone("Hip"),r=bone("Ribs"),a=bone("Left_Ankle_Joint_01"),k=bone("Left_Knee_Joint_01");
     if(i==0){hipRest=h[1];ribsRest=r[1];std::copy(a.begin(),a.end(),ankleRest);kneeRestX=k[0];}
     minHip=std::min(minHip,h[1]);maxStretch=std::max(maxStretch,std::abs((r[1]-h[1])-(ribsRest-hipRest)));
     maxAnkle=std::max(maxAnkle,std::hypot(a[0]-ankleRest[0],a[1]-ankleRest[1],a[2]-ankleRest[2]));maxKneeSide=std::max(maxKneeSide,std::abs(k[0]-kneeRestX));}
    if(trace&&i%6==0) {
      const auto at=[&](const char *n){std::vector<runtime::ObjectId> all;g.collectSubtree(g.root(),all);for(auto id:all)if(std::string_view(g.find(id)->name)==n){float m[16];runtime::worldMatrix(g,id,m);return std::array<float,6>{m[12],m[13],m[14],m[8],m[9],m[10]};}return std::array<float,6>{};};
      const auto h=at("Hip"),r=at("Ribs"),hd=at("Head"),la=at("Left_Ankle_Joint_01"),ra=at("Right_Ankle_Joint_01"),lk=at("Left_Knee_Joint_01");
      std::printf("f%03u hip %.3f %.3f %.3f ribs %.3f %.3f %.3f head y %.3f | Lank %.3f %.3f %.3f z(%.2f %.2f %.2f) Rank z(%.2f %.2f %.2f) Lknee %.3f %.3f %.3f\n",i,h[0],h[1],h[2],r[0],r[1],r[2],hd[1],la[0],la[1],la[2],la[3],la[4],la[5],ra[3],ra[4],ra[5],lk[0],lk[1],lk[2]);
    }}
  const auto find=[&](const char *name){std::vector<runtime::ObjectId> all;g.collectSubtree(g.root(),all);for(auto id:all)if(std::string_view(g.find(id)->name)==name)return id;return runtime::ObjectId{};};
  const auto gap=[&](const char *a,const char *b){float m[16],n[16];runtime::worldMatrix(g,find(a),m);runtime::worldMatrix(g,find(b),n);return std::hypot(m[12]-n[12],m[13]-n[13],m[14]-n[14]);};
  const float left=gap("Left_Wrist_Joint_01","Pegada esq."),right=gap("Right_Wrist_Joint_01","Pegada dir."),hold=gap("Caixa","Ponto de carregar");
  std::printf("KYLE_CARRY left=%.3f right=%.3f hold=%.3f parent=%s\n",left,right,hold,std::string(g.find(g.find(find("Caixa"))->parent)->name).c_str());
  std::printf("KYLE_BODY crouch=%.3f stretch=%.4f ankle=%.4f knee_side=%.3f\n",hipRest-minHip,maxStretch,maxAnkle,maxKneeSide);
  if(hipRest-minHip<.05f||maxStretch>.01f||maxAnkle>.02f||maxKneeSide>.06f)return 198;
  return left<.04f&&right<.04f&&hold<.04f?0:197;
}

// Projeto "Kyle - UMotion": o projeto UMotion Pro (.asset) importado no Kyle
// pela engine (clipes, alvos de IK, plano rotativo, Filho de). Validacao no
// Play contra a curva da caixa que o proprio UMotion exportou (Root/Cube no
// .anim): distancias da caixa ao Root e ao Hip, independentes de eixos.
inline int writeKyleUMotionProject(const std::filesystem::path &glb,const std::filesystem::path &asset,const std::filesystem::path &exported,const std::filesystem::path &out) {
  using namespace editor;
  const auto read=[](const std::filesystem::path &p){std::vector<u8> b;if(FILE *f=std::fopen(p.string().c_str(),"rb")){int c;while((c=std::fgetc(f))!=EOF)b.push_back(static_cast<u8>(c));std::fclose(f);}return b;};
  EditorSession session;std::string error;KyleStage stage;
  if(const int code=stage.open(glb,out,session))return code;
  auto &graph=session.document();
  const auto source=read(asset);const std::string relative="Sources/"+asset.filename().string();
  if(source.empty()||!EditorImportTransaction::write(out/relative,source))return 200;
  UMotionImportReport report;
  if(!session.importUMotionProject(relative,stage.kyle,report,error)){std::fprintf(stderr,"UMOTION: %s\n",error.c_str());return 201;}
  std::printf("UMOTION clips=%zu bones=%u helpers=%u ik=%u childOf=%u tracks=%u properties=%u rest=%.5f%%\n",report.clips.size(),report.bones,report.helpers,report.ikChains,report.childOf,report.tracks,report.propertyTracks,report.restError*100);
  {
    // Enquadrar a raiz do personagem: a camera fica a poucos metros (o modelo
    // tem a armadura em escala 0,01 e a malha em centimetros).
    session.setSelection(stage.kyle);session.frameSelection();
    std::printf("FRAME distance=%.3f target=%.3f %.3f %.3f\n",session.camera().distance,session.camera().target[0],session.camera().target[1],session.camera().target[2]);
    std::vector<EditorEntityId> all;graph.collectSubtree(stage.kyle,all);
    for(auto id:all){float c[3],r;if(session.mapScene().bounds(graph,id,c,r))std::printf("  BOUNDS %s r=%.3f c=%.3f %.3f %.3f\n",graph.find(id)->name,r,c[0],c[1],c[2]);}
  }
  float hips[16]{};std::vector<EditorEntityId> ids;graph.collectSubtree(stage.kyle,ids);
  for(auto id:ids)if(std::string_view(graph.find(id)->name)=="Hip")runtime::worldMatrix(graph,id,hips);
  if(std::getenv("AE_KYLE_TRACE"))for(auto id:ids)if(std::string_view(graph.find(id)->name).starts_with("Cube")) {
    float m[16];runtime::worldMatrix(graph,id,m);const auto *e=graph.find(id);
    const auto *mesh=runtime::meshRenderer(*e);
    float c[3];float r=0;const bool b=session.mapScene().bounds(graph,id,c,r);
    std::printf("CUBE %s scale %.4f %.4f %.4f local %.4f parent %s mesh=%u bounds=%d r=%.4f at %.3f %.3f %.3f\n",e->name,std::hypot(m[0],m[1],m[2]),std::hypot(m[4],m[5],m[6]),std::hypot(m[8],m[9],m[10]),
                e->transform.scale[0],graph.find(e->parent)->name,mesh?mesh->mesh:0u,int(b),r,m[12],m[13],m[14]);
  }
  const float eye[]{hips[12]+3.2f,1.3f,hips[14]+.6f};session.setCameraPose(eye,-1.45f,.08f);session.saveSceneView("Kyle de perfil");
  if(!session.save((out/"scenes/editor.aescene").generic_string().c_str(),0))return 202;
  std::filesystem::create_directories(out/".astra");
  if(!EditorImportTransaction::writeText(out/".astra/assets.astra",session.assets().serialize()))return 202;
  const std::string descriptor="{\"format\":\"ASTRA-PROJECT-1\",\"resourceSource\":\"independent\",\"project\":{\"name\":\"Kyle - UMotion\",\"template\":\"empty\",\"scenes\":1,\"assets\":3},\"mainScene\":\"scenes/editor.aescene\",\"editorScene\":\"scenes/editor.aescene\"}";
  if(!EditorImportTransaction::writeText(out/"project.json",descriptor)||
     !EditorImportTransaction::writeText(out/"LEIA-ME.md","# Kyle - UMotion\n\nRobot Kyle e o projeto UMotion Pro de exemplo (fontes do proprietario) importados pela Astra: clipes FK, alvos de IK em Plano rotativo e Filho de animado da caixa. De Play.\n"))return 203;
  // Curva Root/Cube do .anim exportado pelo UMotion.
  std::vector<std::array<float,10>> cube; // tempo, valor xyz, entrada xyz, saida xyz
  {
    const auto text=read(exported);resources::UnityYamlDocument doc;std::string e;
    if(text.empty()||!resources::readUnityYaml(std::string_view(reinterpret_cast<const char*>(text.data()),text.size()),doc,e))return 204;
    for(const auto &object:doc.objects)if(const auto *curves=object.value.get("m_PositionCurves"))for(const auto &entry:curves->elements) {
      const auto *path=entry.get("path");if(!path||path->scalar!="Root/Cube")continue;
      for(const auto &k:entry.get("curve")->get("m_Curve")->elements) {
        std::array<float,10> row{};double d=0;k.get("time")->number(d);row[0]=float(d);
        const char *fields[3]{"value","inSlope","outSlope"};
        for(int f=0;f<3;++f)for(int c=0;c<3;++c){const char *axis[3]{"x","y","z"};k.get(fields[f])->get(axis[c])->number(d);row[1+f*3+c]=float(d);}
        cube.push_back(row);
      }
    }
    if(cube.size()<2)return 205;
  }
  const auto unityCube=[&](float t,float out[3]) {
    if(t<=cube.front()[0]){for(int c=0;c<3;++c)out[c]=cube.front()[1+c];return;}
    if(t>=cube.back()[0]){for(int c=0;c<3;++c)out[c]=cube.back()[1+c];return;}
    usize i=0;while(cube[i+1][0]<t)++i;const auto &a=cube[i],&b=cube[i+1];
    const float dt=b[0]-a[0],u=(t-a[0])/dt,u2=u*u,u3=u2*u;
    for(int c=0;c<3;++c)out[c]=(2*u3-3*u2+1)*a[1+c]+(u3-2*u2+u)*dt*a[7+c]+(-2*u3+3*u2)*b[1+c]+(u3-u2)*dt*b[4+c];
  };
  const float hipU[3]{0.029f,-0.0026f,0};
  for(const auto &record:session.assets().records())if(const auto *clip=session.animationClipAsset(record.guid)) {
    resources::AnimationClip compiled;std::string diag;const bool ok=clip->compile(compiled,&diag);
    runtime::AnimationClipView view;const bool found=session.mapScene().findClip(record.guid,view);
    if(found&&view.source) {std::vector<runtime::ObjectId> targets;runtime::resolveAnimationTargets(graph,stage.kyle,*view.source,targets);
      u32 resolved=0;for(auto t:targets)resolved+=t!=0;std::printf("FIND %d targets %zu resolved %u\n",int(found),targets.size(),resolved);}
    else std::printf("FIND %d\n",int(found));
    std::printf("CLIP %s compile=%d channels=%zu duration=%.3f diag=%s\n",clip->name.c_str(),int(ok),compiled.channels.size(),clip->duration,diag.c_str());
  }
  {std::vector<runtime::ObjectId> all;graph.collectSubtree(graph.root(),all);
   for(auto id:all)if(const auto *a=graph.find(id)->components.find(scene::Animation::descriptor)){const auto &an=static_cast<const scene::Animation&>(*a);std::printf("ANIMATION on %s clips=%zu auto=%d\n",std::string(graph.find(id)->name).c_str(),an.clips.size(),int(an.playAutomatically));}}
  {
    runtime::SceneGraph playing=graph;runtime::SceneAnimator player;player.begin(playing,session.mapScene());
    const bool a=player.advance(.9,{}),b=player.advance(.4,{});
    std::vector<runtime::ObjectId> all;playing.collectSubtree(playing.root(),all);
    for(auto id:all)if(std::string_view(playing.find(id)->name)=="IK_Handle_Right_Hand"){float m[16];runtime::worldMatrix(playing,id,m);std::printf("DIRECT advance=%d%d handle %.3f %.3f %.3f diag=%s\n",int(a),int(b),m[12],m[13],m[14],std::string(player.compositionDiagnostic()).c_str());}
  }
  if(!session.startPlay())return 206;
  std::vector<renderer::MapDrawState> frames;const auto &g=session.playScene().document();
  const auto at=[&](const char *n){std::vector<runtime::ObjectId> all;g.collectSubtree(g.root(),all);for(auto id:all)if(std::string_view(g.find(id)->name)==n){float m[16];runtime::worldMatrix(g,id,m);return std::array<float,3>{m[12],m[13],m[14]};}return std::array<float,3>{};};
  float worstRoot=0,worstHip=0,worstStep=0;int worstFrame=0,stepFrame=0;std::array<float,3> lastCube{},lastUnity{};
  for(int i=0;i<180;++i) {
    session.advanceClock(10+(double(i)+.001)/60.0);session.extractPlayMap(frames);
    const auto c=at("Cube"),r=at("Root"),h=at("Hip");float u[3];unityCube(float(i)/60.f,u);
    const float dRoot=std::hypot(c[0]-r[0],c[1]-r[1],c[2]-r[2]),dHip=std::hypot(c[0]-h[0],c[1]-h[1],c[2]-h[2]);
    const float uRoot=std::hypot(u[0],u[1],u[2]),uHip=std::hypot(u[0]-hipU[0],u[1]-hipU[1],u[2]-hipU[2]);
    // So nos instantes das chaves exportadas: entre elas a curva de referencia
    // foi reduzida pelo exportador (tolerancia propria) e nao e verdade exata.
    const bool keyed=std::any_of(cube.begin(),cube.end(),[&](const auto &k){return std::fabs(k[0]*60-float(i))<1e-3f;});
    const float e1=keyed?std::fabs(dRoot-uRoot):0.f,e2=keyed?std::fabs(dHip-uHip):0.f;
    if(std::max(e1,e2)>.001f&&std::getenv("AE_KYLE_ERRORS")) {
      const auto wr=at("Right_Wrist_Joint_01"),hr=at("IK_Handle_Right_Hand"),wl=at("Left_Wrist_Joint_01"),hl=at("IK_Handle_Left_Hand");
      std::printf("ERR f%03d root %.4f hip %.4f | reachR %.4f reachL %.4f\n",i,e1,e2,std::hypot(wr[0]-hr[0],wr[1]-hr[1],wr[2]-hr[2]),std::hypot(wl[0]-hl[0],wl[1]-hl[1],wl[2]-hl[2]));
    }
    if(std::max(e1,e2)>std::max(worstRoot,worstHip))worstFrame=i;
    worstRoot=std::max(worstRoot,e1);worstHip=std::max(worstHip,e2);
    if(std::getenv("AE_KYLE_TRACE")&&i%10==0) {
      const auto w=at("Right_Wrist_Joint_01"),hd=at("IK_Handle_Right_Hand");
      std::vector<runtime::ObjectId> all;g.collectSubtree(g.root(),all);u64 parent=0;
      for(auto id:all)if(std::string_view(g.find(id)->name)=="Cube")if(const auto *c=g.find(id)->components.find(scene::ChildOf::descriptor))parent=static_cast<const scene::ChildOf&>(*c).parent;
      std::printf("  wrist %.3f %.3f %.3f handle %.3f %.3f %.3f root %.3f %.3f %.3f childof.parent=%llu diag=%zu\n",w[0],w[1],w[2],hd[0],hd[1],hd[2],r[0],r[1],r[2],(unsigned long long)parent,(size_t)0);
    }
    if(std::getenv("AE_KYLE_TRACE")&&i%10==0)std::printf("f%03d cube-root astra %.4f unity %.4f | cube-hip astra %.4f unity %.4f\n",i,dRoot,uRoot,dHip,uHip);
    {
      // Deslocamento quadro a quadro: a caixa anda o mesmo que a curva da Unity
      // (a raiz nao se move neste clipe, entao o passo no mundo e o passo local).
      const std::array<float,3> cu{c[0]-r[0],c[1]-r[1],c[2]-r[2]};
      if(i&&keyed) {
        const float da=std::hypot(cu[0]-lastCube[0],cu[1]-lastCube[1],cu[2]-lastCube[2]),du=std::hypot(u[0]-lastUnity[0],u[1]-lastUnity[1],u[2]-lastUnity[2]);
        if(std::fabs(da-du)>worstStep){worstStep=std::fabs(da-du);stepFrame=i;}
        if(std::getenv("AE_KYLE_STEP")&&std::fabs(da-du)>.003f)std::printf("STEP f%03d astra %.4f unity %.4f\n",i,da,du);
      }
      lastCube=cu;lastUnity={u[0],u[1],u[2]};
    }
    if(const char *dump=std::getenv("AE_KYLE_DUMP");dump&&i==std::atoi(dump)) {
      // Posicao e eixos de mundo de cada objeto do personagem neste quadro.
      std::vector<runtime::ObjectId> all;g.collectSubtree(g.root(),all);
      for(auto id:all){float m[16];runtime::worldMatrix(g,id,m);
        std::printf("DUMP %s %.5f %.5f %.5f | %.4f %.4f %.4f | %.4f %.4f %.4f\n",g.find(id)->name,m[12],m[13],m[14],m[0],m[1],m[2],m[4],m[5],m[6]);}
    }
    if(std::getenv("AE_KYLE_SWITCH")&&((i>=26&&i<=34)||(i>=56&&i<=66)||(i>=84&&i<=92)||(i>=110&&i<=126)))
      std::printf("s%03d cube-root astra %.4f unity %.4f keyed=%d\n",i,dRoot,uRoot,int(keyed));
  }
  std::printf("UMOTION_CUBE worst root=%.4f m hip=%.4f m frame=%d step=%.4f m frame=%d\n",worstRoot,worstHip,worstFrame,worstStep,stepFrame);
  {
    // Quadro a quadro em varios ciclos: saltos da caixa e das maos entre
    // quadros vizinhos e deriva da pose de um ciclo para o seguinte.
    const char *names[]{"Cube","Right_Wrist_Joint_01","Left_Wrist_Joint_01","IK_Handle_Right_Hand","IK_Handle_Left_Hand"};
    std::vector<std::array<std::array<float,3>,5>> first(180);
    std::array<float,5> worstJump{},worstDrift{};std::array<int,5> jumpFrame{},driftFrame{};
    std::array<std::array<float,3>,5> previous{};
    const double start=12345.0;
    for(int i=0;i<180*8;++i) {
      session.advanceClock(start+(double(i)+.001)/60.0);session.extractPlayMap(frames);
      for(int n=0;n<5;++n) {
        const auto v=at(names[n]);
        if(i) {const float d=std::hypot(v[0]-previous[n][0],v[1]-previous[n][1],v[2]-previous[n][2]);if(d>worstJump[n]){worstJump[n]=d;jumpFrame[n]=i;}}
        if(i<180)first[i][n]=v;
        else {const auto &f=first[i%180][n];const float d=std::hypot(v[0]-f[0],v[1]-f[1],v[2]-f[2]);if(d>worstDrift[n]){worstDrift[n]=d;driftFrame[n]=i;}}
        previous[n]=v;
      }
      if(std::getenv("AE_KYLE_FRAMES")&&i%10==0) {
        const auto c=at("Cube"),wr=at("Right_Wrist_Joint_01"),hr=at("IK_Handle_Right_Hand"),wl=at("Left_Wrist_Joint_01"),hl=at("IK_Handle_Left_Hand");
        std::printf("F%04d c%03d cube %.3f %.3f %.3f | R reach %.3f handle %.3f %.3f %.3f | L reach %.3f handle %.3f %.3f %.3f\n",i,i%180,c[0],c[1],c[2],
          std::hypot(wr[0]-hr[0],wr[1]-hr[1],wr[2]-hr[2]),hr[0],hr[1],hr[2],std::hypot(wl[0]-hl[0],wl[1]-hl[1],wl[2]-hl[2]),hl[0],hl[1],hl[2]);
      }
    }
    for(int n=0;n<5;++n)std::printf("LOOPS %s jump %.4f m @%d (cycle frame %d) drift %.4f m @%d\n",names[n],worstJump[n],jumpFrame[n],jumpFrame[n]%180,worstDrift[n],driftFrame[n]);
    // Cada ciclo repete o primeiro: nada se acumula de um quadro para o outro.
    if(*std::max_element(worstDrift.begin(),worstDrift.end())>.002f)return 208;
  }
  // Projeto Humanoid: a curva exportada passou pelo retargeting por musculos da
  // Unity (torcao distribuida no antebraco), que nao faz parte do rig UMotion.
  // Fora das maos a caixa coincide em decimos de milimetro; presa a mao, ate 1 cm.
  return worstRoot<.01f&&worstHip<.01f?0:207;
}
} // namespace ae::test
