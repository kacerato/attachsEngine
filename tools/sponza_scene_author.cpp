#include "editor/editor_archive.h"
#include "editor/editor_map_scene.h"
#include "editor/editor_play_scene.h"
#include "scene/environment.h"
#include "scene/light.h"
#include "scene/import_link.h"
#include "resources/gltf_package.h"
#include "resources/gltf_folder_source.h"
#include "resources/gltf_import.h"
#include "resources/import_node_map.h"
#include "resources/import_profile.h"
#include "scene/ui_canvas.h"
#include "scene/camera_look.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace ae;
using namespace ae::editor;
[[noreturn]] static void fail(const std::string &message){std::cerr<<message<<'\n';std::exit(20);}
static std::string readText(const std::filesystem::path &p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
int main(int argc,char **argv) {
  if(argc<2||argc>3) {std::cerr<<"usage: sponza_scene_author project-directory [output-directory]\n";return 1;}
  const std::filesystem::path root=std::filesystem::absolute(argv[1]);
  EditorDocument doc;
  if(!loadEditorDocument((root/"scenes/editor.aescene").string().c_str(),0,doc)){std::cerr<<"archive rejected\n";return 2;}
  std::vector<EditorEntityId> ids;doc.collectSubtree(doc.root(),ids);
  const auto sourceDir=root/"Fontes/main_sponza";
  const auto text=readText(sourceDir/"NewSponza_Main_glTF_003.gltf");
  resources::GltfFolder folder;
  folder.context=const_cast<std::filesystem::path*>(&sourceDir);
  folder.read=[](void *ctx,const std::string &relative,std::vector<u8> &out){return resources::readGltfFolderFile(*static_cast<std::filesystem::path*>(ctx),relative,out);};
  resources::GltfPackage packed;std::string error;
  if(!resources::packGltfFolder({reinterpret_cast<const u8*>(text.data()),text.size()},folder,512ull<<20,packed,error)){std::cerr<<error;return 3;}
  resources::ImportProfile profile;
  if(!resources::parseImportProfile(readText(root/".astra/imports/6c0ca0c97fca384f787f7ac68adbd5c7.profile"),profile))return 4;
  resources::GltfImport model;resources::GltfImportProgress progress;
  // Geometry-only host validation; authored textures and the device cache are never changed.
  progress.externalFile=[](void*,std::string_view,u64,std::vector<u8>&){return false;};
  auto limits=resources::applyImportProfile(resources::GltfImportLimits{},profile);
  if(!resources::importGlb(packed.glb,limits,progress,model)){std::cerr<<model.diagnostic;return 5;}
  resources::ImportNodeMap nodeMap;
  if(!resources::ImportNodeMap::deserialize(readText(root/".astra/imports/6c0ca0c97fca384f787f7ac68adbd5c7.nodes"),nodeMap)||nodeMap.nodes.size()!=model.nodes.size())return 6;
  std::vector<resources::AssetGuid> identity;std::vector<float> pivots;
  std::vector<u32> perNode(model.nodes.size());
  for(u32 i=0;i<model.draws.size();++i){const auto n=model.drawNodes[i];identity.push_back(nodeMap.nodes[n].draws.at(perNode[n]++));pivots.insert(pivots.end(),3,0);}
  auto makeLibrary=[&](EditorMapScene &library,EditorDocument &scene) {
    std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
    renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials);
    std::vector<resources::AssetGuid> identities(draws.size());std::vector<float> pivot(draws.size()*3,0);std::vector<std::string> names(draws.size());
    const auto vb=vertices.size()/renderer::MapVertexStride,ib=indices.size(),mb=materials.size();
    vertices.insert(vertices.end(),model.vertices.begin(),model.vertices.end());indices.insert(indices.end(),model.indices.begin(),model.indices.end());
    materials.insert(materials.end(),model.materials.begin(),model.materials.end());
    for(auto d:model.draws){d.vertexOffset+=vb;d.firstIndex+=ib;d.materialIndex+=mb;draws.push_back(d);}
    identities.insert(identities.end(),identity.begin(),identity.end());pivot.insert(pivot.end(),pivots.begin(),pivots.end());names.insert(names.end(),model.names.begin(),model.names.end());
    for(u32 a=6;a<identities.size();++a)for(u32 b=6;b<a;++b)if(identities[a]==identities[b])fail("Duplicate resource "+std::to_string(a)+" / "+std::to_string(b)+" "+identities[a].text());
    std::cerr<<"Library draws="<<draws.size()<<" ids="<<identities.size()<<" pivots="<<pivot.size()<<" names="<<names.size()<<"\n";
    return library.adoptPackage(scene,draws,materials,vertices,indices,identities,0,pivot,names);
  };
  EditorMapScene library;
  if(!makeLibrary(library,doc))return 7;
  // Keep the source document immutable while reconciling temporary process slots.
  auto resolved=doc;library.reconcileAssets(resolved);
  u64 total=0;
  for(auto id:ids) {
    const auto &o=*doc.find(id);
    std::cout<<id<<" parent="<<o.parent<<" "<<o.name<<" p="<<o.transform.position[0]<<","<<o.transform.position[1]<<","<<o.transform.position[2]<<" s="<<o.transform.scale[0]<<","<<o.transform.scale[1]<<","<<o.transform.scale[2];
    if(const auto *m=meshRenderer(o)){std::cout<<" mesh="<<m->mesh<<" slots="<<m->slotCount()<<" guid="<<m->asset.text();}
    if(const auto *m=meshRenderer(*resolved.find(id))){u64 tris=0;for(u32 s=0;s<m->slotCount();++s)if(m->slotMesh(s))tris+=library.asset(m->slotMesh(s)-1)->indexCount/3;total+=tris;std::cout<<" triangles="<<tris;}
    if(const auto *v=o.components.find(scene::PhysicsBody::descriptor))std::cout<<" BODY="<<int(static_cast<const scene::PhysicsBody*>(v)->motion);
    if(const auto *v=o.components.find(scene::Collider::descriptor))std::cout<<" COLLIDER="<<int(static_cast<const scene::Collider*>(v)->shape);
    if(const auto *v=o.components.find(scene::Light::descriptor)){const auto &l=*static_cast<const scene::Light*>(v);std::cout<<" LIGHT="<<int(l.kind)<<" intensity="<<l.intensity<<" unit="<<int(l.unit);}
    if(const auto *v=o.components.find(scene::Environment::descriptor)){const auto &e=*static_cast<const scene::Environment*>(v);std::cout<<" ENV diffuse="<<e.values.indirectDiffuse<<" exposure="<<e.values.exposureEv<<" sky="<<int(e.values.sky);}
    std::cout<<"\n";
  }
  std::cout<<"TOTAL_AUTHORED_TRIANGLES="<<total<<" imported_draws="<<model.draws.size()<<"\n";
  if(argc==2)return 0;
  const std::filesystem::path output=std::filesystem::absolute(argv[2]);
  if(output==root){std::cerr<<"Output must be separate from the original project\n";return 8;}
  auto apply=[&](EditorEntityId id,const EditorEntity &v){if(!doc.applyEntityValues(id,v))fail("Invalid authored object "+std::to_string(id));};
  // The import root is an organizational group. Geometry belongs to its children.
  auto group=*doc.find(2);group.components.remove(scene::Collider::descriptor);group.components.remove(scene::PhysicsBody::descriptor);group.components.remove(scene::MeshRenderer::descriptor);apply(2,group);
  u32 physicalObjects=0,shapes=0;u64 collisionTriangles=0;
  const auto sourceDrawCount=model.draws.size();
  for(auto id:ids) {
    const auto &before=*doc.find(id);const std::string name=before.name;
    const auto *mesh=meshRenderer(before);
    if(!mesh||!mesh->mesh||(before.parent!=5&&before.parent!=51)||name.starts_with("decals")||name.starts_with("lamp"))continue;
    auto v=before;v.isStatic=true;
    auto *body=static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor));if(!body)body=static_cast<scene::PhysicsBody*>(v.components.edit(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Static;
    const bool exact=name.starts_with("floor")||name.starts_with("wall")||name.starts_with("stonewall")||name.starts_with("ceiling");
    if(exact) {
      auto *c=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));c->shape=scene::ColliderShape::Mesh;c->convex=false;
      for(u32 s=0;s<mesh->slotCount();++s)if(mesh->slotMesh(s))collisionTriangles+=library.asset(mesh->slotMesh(s)-1)->indexCount/3;
      ++shapes;
    } else for(u32 s=0;s<mesh->slotCount();++s) {
      const auto guid=mesh->slotAsset(s);const auto at=std::find(identity.begin(),identity.begin()+sourceDrawCount,guid);
      if(at==identity.begin()+sourceDrawCount)fail("Unresolved visual collision source");
      const auto index=at-identity.begin();resources::CollisionMeshRecipe recipe{guid,5,.01f};recipe.identity=resources::collisionMeshGuid(recipe);
      resources::CollisionMeshBuild build;
      if(!resources::buildCollisionMesh(model.vertices,model.indices,model.draws[index],recipe,build,error))fail(error);
      auto draw=model.draws[index];draw.firstIndex=model.indices.size();draw.indexCount=build.indices.size();draw.lodLevel=0;draw.geometricError=build.resultingError;draw.lodGroupId=model.draws.size();
      model.indices.insert(model.indices.end(),build.indices.begin(),build.indices.end());model.draws.push_back(draw);identity.push_back(recipe.identity);pivots.insert(pivots.end(),3,0);model.names.push_back(name+" / colisao");profile.collisionMeshes.push_back(recipe);
      auto *c=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));c->shape=scene::ColliderShape::Mesh;c->convex=false;c->collisionMesh=recipe.identity;
      collisionTriangles+=build.indices.size()/3;++shapes;
    }
    apply(id,v);++physicalObjects;
  }
  // Find the real ground under the initial position, using authored geometry.
  const float spawnX=-9,spawnZ=.2f;float floorY=-10000;
  const auto *floorMesh=meshRenderer(*doc.find(45));float world[16];runtime::worldMatrix(doc,45,world);
  for(u32 s=0;s<floorMesh->slotCount();++s) {
    std::span<const EditorPickMesh::Triangle> triangles;float local[16];if(!library.localGeometry(floorMesh->slotMesh(s),triangles,local))continue;
    for(const auto &t:triangles){float p[3][3];for(u32 a=0;a<3;++a){float v[3];for(u32 k=0;k<3;++k)v[k]=local[12+k]+local[k]*t[a*3]+local[4+k]*t[a*3+1]+local[8+k]*t[a*3+2];for(u32 k=0;k<3;++k)p[a][k]=world[12+k]+world[k]*v[0]+world[4+k]*v[1]+world[8+k]*v[2];}
      const float d=(p[1][2]-p[2][2])*(p[0][0]-p[2][0])+(p[2][0]-p[1][0])*(p[0][2]-p[2][2]);if(std::abs(d)<1e-8f)continue;
      const float a=((p[1][2]-p[2][2])*(spawnX-p[2][0])+(p[2][0]-p[1][0])*(spawnZ-p[2][2]))/d,b=((p[2][2]-p[0][2])*(spawnX-p[2][0])+(p[0][0]-p[2][0])*(spawnZ-p[2][2]))/d,c=1-a-b;
      if(a>=-1e-5f&&b>=-1e-5f&&c>=-1e-5f){const float y=a*p[0][1]+b*p[1][1]+c*p[2][1];if(y<1)floorY=std::max(floorY,y);}
    }
  }
  if(floorY<-100)fail("No real floor under spawn");
  const auto actor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Player Sponza");auto player=*doc.find(actor);player.transform.position[0]=spawnX;player.transform.position[1]=floorY+.08f;player.transform.position[2]=spawnZ;player.isStatic=false;
  auto *character=static_cast<scene::Character*>(player.components.add(scene::Character::descriptor));character->radius=.32f;character->halfHeight=.53f;character->eyeHeight=1.65f;character->speed=3.8f;character->jumpSpeed=4.5f;character->stepHeight=.25f;character->floorSnapLength=.35f;apply(actor,player);
  const auto visual=doc.createEntity(actor,EditorEntityKind::Folder,"Corpo do player");auto body=*doc.find(visual);body.isStatic=false;body.transform.position[1]=.8f;body.transform.scale[0]=body.transform.scale[2]=.29f;body.transform.scale[1]=.72f;
  auto *render=editMeshRenderer(body);render->mesh=3;render->asset=library.assetGuid(2);render->material.enabled=true;render->material.baseColor[0]=.13f;render->material.baseColor[1]=.19f;render->material.baseColor[2]=.22f;apply(visual,body);
  const auto camera=doc.createEntity(actor,EditorEntityKind::Camera,"Camera do player");auto view=*doc.find(camera);view.isStatic=false;view.transform.position[1]=1.65f;view.transform.rotationDegrees[1]=90;
  auto *cam=editCamera(view);cam->priority=100;cam->verticalFov=70;cam->nearPlane=.08f;cam->farPlane=200;view.components.add(scene::CameraLook::descriptor);apply(camera,view);
  auto sun=*doc.find(158);assignEntityName(sun,"Sol da Sponza");sun.transform.rotationDegrees[0]=82;sun.transform.rotationDegrees[1]=-22;sun.transform.rotationDegrees[2]=0;
  auto *light=static_cast<scene::Light*>(sun.components.edit(scene::Light::descriptor));light->kind=scene::LightKind::Directional;light->unit=scene::LightUnit::Engine;light->intensity=8;light->useColorTemperature=true;light->colorTemperature=5600;apply(158,sun);
  for(auto id:{130u,131u,135u,137u,141u,151u}){auto v=*doc.find(id);auto *l=static_cast<scene::Light*>(v.components.add(scene::Light::descriptor));l->kind=scene::LightKind::Point;l->unit=scene::LightUnit::LuxLumen;l->intensity=30000;l->range=7;l->useColorTemperature=true;l->colorTemperature=3000;l->shadowMode=0;apply(id,v);}
  group=*doc.find(2);auto *environment=static_cast<scene::Environment*>(group.components.edit(scene::Environment::descriptor));auto &env=environment->values;env.exposureEv=.20f;env.indirectDiffuse=1.1f;env.indirectSpecular=1;env.contrast=1.03f;env.saturation=1.03f;env.bloomIntensity=.08f;env.autoExposure=false;
  const float zenith[3]{.20f,.32f,.46f},horizon[3]{.45f,.45f,.42f},ground[3]{.26f,.24f,.22f};std::copy(zenith,zenith+3,env.skyZenith);std::copy(horizon,horizon+3,env.skyHorizon);std::copy(ground,ground+3,env.ground);apply(2,group);
  resources::AssetRegistry registry;if(!resources::AssetRegistry::deserialize(readText(root/".astra/assets.astra"),registry))return 9;
  resources::AssetRecord uiAsset;uiAsset.guid=resources::assetGuidFromSeed("astra:sponza:player-controls:20261005");uiAsset.type=resources::AssetType::UiDocument;uiAsset.path="UI/SponzaPlayer.aeui";if(!registry.add(uiAsset))return 10;
  ui::GuiDocument hud;auto node=[&](ui::GuiKind kind,const char *name,ui::UiPoint anchorMin,ui::UiPoint anchorMax,ui::UiRect offsets){const auto id=hud.create(kind);auto n=*hud.find(id);n.name=name;n.anchorMin=anchorMin;n.anchorMax=anchorMax;n.offsets=offsets;n.background=0;n.foreground=0x708D999E;n.accent=0xC0C5DBAE;n.radius=60;n.fontSize=20;return n;};
  auto look=node(ui::GuiKind::LookArea,"Olhar",{.38f,0},{1,1},{0,0,0,0});look.control.sensitivity=1;if(!hud.update(look,error))fail(error);
  auto move=node(ui::GuiKind::Joystick,"Mover",{0,1},{0,1},{40,-272,272,-40});move.control.inputRadius=95;move.control.baseRadius=90;move.control.knobRadius=31;move.control.deadzone=.12f;if(!hud.update(move,error))fail(error);
  auto jump=node(ui::GuiKind::ActionButton,"Saltar",{1,1},{1,1},{-178,-178,-58,-58});jump.text="Saltar";jump.background=0x80363E42;jump.foreground=0xE0EEF4E8;jump.radius=60;if(!hud.update(jump,error))fail(error);
  const auto canvas=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Controles do player");auto control=*doc.find(canvas);auto *ui=static_cast<scene::UiCanvas*>(control.components.add(scene::UiCanvas::descriptor));ui->document=uiAsset.guid;ui->inputReceiver=actor;ui->inputCamera=camera;ui->movementSpace=2;apply(canvas,control);
  std::cerr<<"Collision objects="<<physicalObjects<<" shapes="<<shapes<<" triangles="<<collisionTriangles<<"\n";
  if(!makeLibrary(library,doc))fail("Geometry library rejected collision derivatives");
  library.reconcileAssets(doc);
  EditorPlayScene play;play.configureSceneGui([&](resources::AssetGuid id,ui::GuiDocument &out,std::string&){if(id!=uiAsset.guid)return false;out=hud;return true;});
  if(!play.start(doc,library)){std::cerr<<"PLAY REJECTED: "<<play.physicsError();return 12;}
  for(int i=0;i<120;++i)if(!play.advance(1./60))fail(play.frameError());
  const auto initial=play.world().graph().find(actor)->transform;
  auto &gui=play.sceneGui();const auto lease=gui.instanceFor(play.world(),canvas);auto *instance=gui.find(play.world(),lease);if(!instance)fail("No runtime HUD");
  const float eye[3]{spawnX,floorY+1.65f,spawnZ};const auto frustum=renderer::buildPerspectiveFrustum(eye,1.5707963f,0,2.1656f);gui.prepare(play.world(),frustum,{0,0,2772,1280},{0,0,2772,1280});
  instance->runtime.pointer({1,ui::UiPointerPhase::Down,{156,1124}});instance->runtime.pointer({1,ui::UiPointerPhase::Move,{156,1029}});
  for(int i=0;i<90;++i){play.submitInput({},1./60);if(!play.advance(1./60))fail(play.frameError());}
  instance->runtime.pointer({1,ui::UiPointerPhase::Up,{156,1029}});play.submitInput({},1./60);
  const auto moved=play.world().graph().find(actor)->transform;
  const float distance=std::hypot(moved.position[0]-initial.position[0],moved.position[2]-initial.position[2]);if(distance<2||moved.position[1]<floorY-.25f)fail("Player did not walk on the real floor");
  instance->runtime.pointer({2,ui::UiPointerPhase::Down,{2654,1162}});instance->runtime.pointer({2,ui::UiPointerPhase::Up,{2654,1162}});float apex=moved.position[1];
  for(int i=0;i<90;++i){play.submitInput({},1./60);if(!play.advance(1./60))fail(play.frameError());apex=std::max(apex,play.world().graph().find(actor)->transform.position[1]);}
  if(apex<moved.position[1]+.35f)fail("Jump did not drive the character");
  instance->runtime.pointer({3,ui::UiPointerPhase::Down,{1800,600}});instance->runtime.pointer({3,ui::UiPointerPhase::Move,{1940,560}});play.submitInput({},1./60);if(!play.advance(1./60))fail(play.frameError());
  const auto rotation=play.world().graph().find(camera)->transform.rotationDegrees[1];if(std::abs(rotation-90)<1)fail("Look did not rotate the assigned camera");
  const auto authored=serializeEditorDocument(doc,0);play.stop();if(serializeEditorDocument(doc,0)!=authored)fail("Play changed authoring");
  std::filesystem::create_directories(output/"scenes");std::filesystem::create_directories(output/"UI");std::filesystem::create_directories(output/".astra/imports");
  if(!saveEditorDocument((output/"scenes/editor.aescene").string().c_str(),doc,0))return 13;
  EditorDocument restored;if(!loadEditorDocument((output/"scenes/editor.aescene").string().c_str(),0,restored)||serializeEditorDocument(restored,0)!=authored)return 14;
  auto write=[&](const std::filesystem::path &path,const std::string &text){std::ofstream out(path,std::ios::binary);out<<text;if(!out)fail("Write failed: "+path.string());};
  write(output/".astra/assets.astra",registry.serialize());write(output/".astra/imports/6c0ca0c97fca384f787f7ac68adbd5c7.profile",resources::serializeImportProfile(profile));std::ostringstream uiText;hud.write(uiText);write(output/uiAsset.path,uiText.str());
  std::cout<<"ACCEPTED objects="<<doc.entityCount()<<" bodies="<<physicalObjects<<" shapes="<<shapes<<" collision_triangles="<<collisionTriangles<<" floor_y="<<floorY<<" player="<<actor<<" camera="<<camera<<" canvas="<<canvas<<" walk_distance="<<distance<<" jump_height="<<apex-moved.position[1]<<" look_yaw="<<rotation<<" visual_triangles="<<total+library.asset(2)->indexCount/3<<"\n";
  return 0;
}
