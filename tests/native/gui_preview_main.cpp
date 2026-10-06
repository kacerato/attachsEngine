#include "editor/editor_session.h"
#include "core/sha256.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_archive.h"
#include "editor/editor_import_transaction.h"
#include "scene/script_behavior.h"
#include "scene/collision_recipe.h"
#include "scene/component_preset.h"
#include "scene/camera.h"
#include "scene/camera_look.h"
#include "scene/camera_follow.h"
#include "scene/character.h"
#include "resources/gltf_import.h"
#include "runtime/primitive_object.h"
#include "renderer/primitive_geometry.h"
#include "ui_software_raster.h"
#include "convex_bake_fixture.h"
#include "collider_occlusion_fixture.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
using namespace ae;
static std::vector<u8> read(const std::filesystem::path &path) {
  std::ifstream s(path,std::ios::binary|std::ios::ate);if(!s)return{};
  const auto size=s.tellg();if(size<0)return{};std::vector<u8> out(static_cast<usize>(size));s.seekg(0);
  s.read(reinterpret_cast<char*>(out.data()),size);return out;
}
static editor::EditorEntityId bodyOwnerFixture(editor::EditorSession &s,bool rendered) {
  auto &g=s.document();const auto root=g.createEntity(g.root(),runtime::ObjectKind::Folder,"SharedBody");auto object=*g.find(root);
  auto *body=static_cast<scene::PhysicsBody*>(object.components.add(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Static;
  if(!g.applyEntityValues(root,object))return 0;
  const auto branch=g.createEntity(root,runtime::ObjectKind::Folder,"Branch");
  for(u32 n=0;n<2;++n) {
    const auto id=g.createEntity(n?root:branch,runtime::ObjectKind::Mesh,n?"RightCylinder":"LeftBox");object=*g.find(id);
    object.components.add(scene::Collider::descriptor);
    if(rendered) {
      const u32 slot=n?3:0;
      if(!runtime::configurePrimitive(object,n?scene::PrimitiveType::Cylinder:scene::PrimitiveType::Cube,
          {slot+1,s.mapScene().assetGuid(slot),s.mapScene().materialForAsset(slot)}))return 0;
      while(object.components.remove(scene::PhysicsBody::descriptor)){}
      // configurePrimitive authors its own collider. Keep the deliberately
      // repeated UID 1 and remove the additional self-owned shape, otherwise
      // the acceptance fixture would contain an orphan physics component.
      for(usize i=0;i<object.components.size();) {
        const auto *value=object.components.at(i);
        if(&value->type()==&scene::Collider::descriptor && value->instanceId()!=1)
          object.components.removeInstance(value->instanceId());
        else ++i;
      }
    }
    object.transform.position[0]=n?1.5f:-1.5f;
    auto *c=static_cast<scene::Collider*>(object.components.edit(scene::Collider::descriptor));c->owner=root;c->shape=n?scene::ColliderShape::Cylinder:scene::ColliderShape::Box;
    if(rendered&&n)c->halfHeight=1;
    if(!g.applyEntityValues(id,object))return 0;
  }
  return root;
}
int main(int argc,char **argv) {
  if(argc<2)return 1;
  const std::filesystem::path root=AETHER_REPOSITORY_ROOT;
  if(std::string_view(argv[1])=="verify-collision-recipe-project") {
    if(argc!=4)return 130;
    const auto project=std::filesystem::absolute(argv[2]);
    const auto text=[](const std::filesystem::path &p){auto bytes=read(p);return std::string(bytes.begin(),bytes.end());};
    editor::EditorDocument actual,original;
    if(!editor::deserializeEditorDocument(text(project/"scenes/editor.aescene"),0,actual)||!editor::deserializeEditorDocument(text(argv[3]),0,original))return 131;
    editor::EditorEntityId owner=0;std::vector<editor::EditorEntityId> ids;actual.collectSubtree(actual.root(),ids);
    for(auto id:ids)if(scene::collisionRecipe(actual.find(id)->components)){if(owner)return 132;owner=id;}
    const auto *oldObject=original.find(owner),*object=actual.find(owner);
    const auto *oldRecipe=oldObject?scene::collisionRecipe(oldObject->components):nullptr,*recipe=object?scene::collisionRecipe(object->components):nullptr;
    if(!recipe||!oldRecipe||recipe->geometryHash==oldRecipe->geometryHash||recipe->sources!=oldRecipe->sources||recipe->parts.size()!=oldRecipe->parts.size()||recipe->settings.maximumParts!=oldRecipe->settings.maximumParts||recipe->settings.voxelResolution!=oldRecipe->settings.voxelResolution)return 133;
    auto restored=*object;
    for(const auto &p:oldRecipe->parts) {
      const auto *old=oldObject->components.findInstance(p.collider),*now=object->components.findInstance(p.collider);
      if(!old){if(now)return 134;continue;}if(!now||&now->type()!=&scene::Collider::descriptor)return 134;
      const auto *c=static_cast<const scene::Collider*>(old);
      for(const auto &field:scene::componentDelta(*old,*now))if(field.differs&&(field.address.id!="collision_mesh"||c->collisionMesh!=p.baseline.collisionMesh))return 135;
      if(!restored.components.replaceInstance(p.collider,*old))return 136;
    }
    if(!restored.components.replaceInstance(oldRecipe->instanceId(),*oldRecipe)||!actual.applyEntityValues(owner,restored)||editor::serializeEditorDocument(actual,0)!=editor::serializeEditorDocument(original,0))return 137;
    // Re-read the unmodified Android archive, reopen every saved source through
    // the native publisher, then query the actual Jolt shape by collider UID.
    if(!editor::deserializeEditorDocument(text(project/"scenes/editor.aescene"),0,actual))return 138;
    editor::EditorSession s;test::ConvexCpuLibrary library;std::string error;
    if(!library.connect(s)||!s.setProjectDirectory(project.generic_string().c_str())||!s.loadAssets(text(project/".astra/assets.astra")))return 139;
    std::vector<editor::EditorSession::ReopenedSource> sources;
    for(const auto &record:s.assets().records())if(record.type==resources::AssetType::Mesh){auto bytes=read(project/editor::EditorImportTransaction::fromUtf8(record.path));resources::GltfImport model;if(Sha256::hex(bytes)!=record.contentHash||!resources::importGlb(bytes,{},{},model))return 140;sources.push_back({std::move(model),record.contentHash,record.path});}
    std::vector<editor::EditorSession::ModelImportReport> reports;if(!s.reopenSources(sources,reports,error))return 141;
    runtime::GameWorld world;runtime::ScenePhysics physics;runtime::QueryHit hit;test::ConvexMapGeometry geometry(s.mapScene());
    const float from[]{-2.2f,3,0},direction[]{0,-6,0},disabled[]{2,3,0};
    if(!world.load(actual)||!physics.start(world,&geometry)||!physics.rayCast(from,direction,{},hit)||hit.object!=owner||hit.colliderInstance!=oldRecipe->parts.front().collider||physics.rayCast(disabled,direction,{},hit))return 142;
    std::printf("COLLISION_RECIPE_DEVICE: Android archive reopened with source hashes; new geometry and Jolt hit retain Collider UID; local fields/disabled part, sources, hierarchy, Body and motor byte-exactly preserved.\n");return 0;
  }
  if(std::string_view(argv[1])=="verify-u11-project" || std::string_view(argv[1])=="verify-u11-vertex") {
    const bool vertex=std::string_view(argv[1])=="verify-u11-vertex";
    const editor::EditorEntityId object=vertex?6:4;const u64 uid=vertex?3:1;
    if(argc!=4)return 120;
    const auto project=std::filesystem::absolute(argv[2]);
    const auto text=[](const std::filesystem::path &p){auto b=read(p);return std::string(b.begin(),b.end());};
    editor::EditorDocument actual,original;
    if(!editor::deserializeEditorDocument(text(project/"scenes/editor.aescene"),0,actual)||
       !editor::deserializeEditorDocument(text(argv[3]),0,original))return 121;
    editor::EditorSession s;test::ConvexCpuLibrary library;std::string error;
    if(!library.connect(s)||!s.setProjectDirectory(project.generic_string().c_str())||!s.loadAssets(text(project/".astra/assets.astra")))return 122;
    std::vector<editor::EditorSession::ReopenedSource> sources;
    for(const auto &r:s.assets().records())if(r.type==resources::AssetType::Mesh){
      auto bytes=read(project/editor::EditorImportTransaction::fromUtf8(r.path));resources::GltfImport model;
      if(Sha256::hex(bytes)!=r.contentHash||!resources::importGlb(bytes,{},{},model))return 123;
      sources.push_back({std::move(model),r.contentHash,r.path});
    }
    std::vector<editor::EditorSession::ModelImportReport> reports;
    if(!s.reopenSources(sources,reports,error))return 124;
    runtime::GameWorld world;runtime::ScenePhysics physics;runtime::QueryHit hit;test::ConvexMapGeometry geometry(s.mapScene());
    const float origin[]{vertex?-1.57322f:-.5f,3,vertex?2.8232f:-.4f},direction[]{0,-6,0};
    if(!world.load(actual)||!physics.start(world,&geometry)||!physics.rayCast(origin,direction,{},hit)||hit.colliderObject!=object||hit.colliderInstance!=uid||hit.object!=2 || (vertex&&hit.point[1]<1.04f))return 125;
    auto changed=*actual.find(object);const auto *old=editor::inspectedCollider(original,object,uid);
    if(!old||!changed.components.replaceInstance(uid,*old)||!actual.applyEntityValues(object,changed)||
       editor::serializeEditorDocument(actual,0)!=editor::serializeEditorDocument(original,0))return 126;
    std::printf("U11_DEVICE_PROJECT: native archive/resource reopen + actual edited Jolt hit %.4f + only collider %u:%llu changed; all other entities/visuals/UID/owner preserved\n",hit.point[1],unsigned(object),static_cast<unsigned long long>(uid));return 0;
  }
  if(std::string_view(argv[1])=="write-body-owners"||std::string_view(argv[1])=="write-query-owners"||std::string_view(argv[1])=="write-occlusion"||std::string_view(argv[1])=="write-collider-handles"||std::string_view(argv[1])=="write-u11-integral"||std::string_view(argv[1])=="write-u11-scale") {
    const bool queryProbe=std::string_view(argv[1])=="write-query-owners";
    const bool occlusion=std::string_view(argv[1])=="write-occlusion";
    const bool integral=std::string_view(argv[1])=="write-u11-integral",scale=std::string_view(argv[1])=="write-u11-scale";
    const bool handles=std::string_view(argv[1])=="write-collider-handles"||integral||scale;
    if(argc!=3)return 100;
    const auto out=std::filesystem::absolute(argv[2]);std::error_code ec;if(std::filesystem::exists(out,ec))return 101;
    std::filesystem::create_directories(out/"scenes",ec);if(ec)return 102;
    std::filesystem::create_directories(out/".astra",ec);if(ec)return 102;
    editor::EditorSession s;if(!s.setProjectDirectory(out.generic_string().c_str()))return 103;
    test::ConvexCpuLibrary library;if(!library.connect(s)||!bodyOwnerFixture(s,true))return 104;
    if(occlusion&&!test::addOcclusionWall(s,4))return 112;
    if(handles) {
      auto &document=s.document();
      for(u32 n=0;n<2;++n) {
        const auto id=document.createEntity(2,runtime::ObjectKind::Mesh,n?"Capsule":"Sphere");auto value=*document.find(id);
        const u32 slot=n?2:1;
        if(!runtime::configurePrimitive(value,n?scene::PrimitiveType::Capsule:scene::PrimitiveType::Sphere,
            {slot+1,s.mapScene().assetGuid(slot),s.mapScene().materialForAsset(slot)}))return 117;
        while(value.components.remove(scene::PhysicsBody::descriptor)){}
        auto *collider=static_cast<scene::Collider*>(value.components.edit(scene::Collider::descriptor));
        if(!collider)return 117;
        collider->owner=2;value.transform.position[0]=n?1.5f:-1.5f;value.transform.position[2]=3;
        if(!document.applyEntityValues(id,value))return 117;
      }
    }
    if(queryProbe) {
      auto body=*s.document().find(2);auto *script=static_cast<scene::ScriptBehavior*>(body.components.add(scene::ScriptBehavior::descriptor));
      script->scriptType="example.physics.compound-query-probe";script->source="Scripts/CompoundQueryProbe.cs";
      if(!s.document().applyEntityValues(2,body))return 113;
      std::filesystem::create_directories(out/"Scripts",ec);if(ec)return 114;
      std::filesystem::copy_file(root/"examples/physics/CompoundQueryProbe.cs",out/"Scripts/CompoundQueryProbe.cs",ec);if(ec)return 115;
    }
    auto &g=s.document();const auto camera=g.createEntity(g.root(),runtime::ObjectKind::Camera,"Camera");auto object=*g.find(camera);
    object.components.add(scene::Camera::descriptor);object.transform.position[1]=2;object.transform.position[2]=-8;object.transform.rotationDegrees[0]=14;
    if(!g.applyEntityValues(camera,object))return 105;
    if(occlusion){const float eye[]{0,2,-8};s.setCameraPose(eye,0,.24f);if(!s.saveSceneView("Occlusion"))return 116;}
    if(handles) {
      const float eye[]{0,2,-8},curved[]{0,1,.8f};s.setCameraPose(eye,0,.24f);
      if(!s.saveSceneView("Handles"))return 116;
      s.setCameraPose(curved,0,.2f);if(!s.saveSceneView("Curvas"))return 116;
    }
    if(integral||scale){
      const auto floor=g.createEntity(g.root(),runtime::ObjectKind::Mesh,"Ground");auto ground=*g.find(floor);
      if(!runtime::configurePrimitive(ground,scene::PrimitiveType::Cube,{1,s.mapScene().assetGuid(0),s.mapScene().materialForAsset(0)}))return 119;
      static_cast<scene::PhysicsBody*>(ground.components.edit(scene::PhysicsBody::descriptor))->motion=scene::BodyMotion::Static;
      ground.transform.scale[0]=ground.transform.scale[2]=80;ground.transform.scale[1]=.2f;ground.transform.position[1]=-1.1f;
      if(!g.applyEntityValues(floor,ground))return 119;
      auto owner=*g.find(2);auto *body=static_cast<scene::PhysicsBody*>(owner.components.edit(scene::PhysicsBody::descriptor));
      if(integral){body->motion=scene::BodyMotion::Dynamic;body->freezeRotation[0]=body->freezeRotation[1]=body->freezeRotation[2]=true;owner.components.add(scene::DynamicBodyMotor::descriptor);}
      if(!g.applyEntityValues(2,owner))return 119;
      if(integral){const auto character=g.createEntity(g.root(),runtime::ObjectKind::Folder,"IndependentCharacter");auto e=*g.find(character);
        e.transform.position[0]=7;e.transform.position[1]=2;e.components.add(scene::Character::descriptor);
        if(!g.applyEntityValues(character,e))return 119;}
      if(scale)for(u32 n=0;n<252;++n){const auto id=g.createEntity(2,runtime::ObjectKind::Folder,"ScalePart"+std::to_string(n+1));auto e=*g.find(id);
        auto *c=static_cast<scene::Collider*>(e.components.add(scene::Collider::descriptor));c->owner=2;c->shape=scene::ColliderShape::Cylinder;c->centerX=6+float(n%16)*2;c->centerZ=float(n/16)*2;
        if(!g.applyEntityValues(id,e))return 119;}
    }
    const auto data=editor::serializeEditorDocument(g,0);editor::EditorDocument reopened;
    if(!editor::deserializeEditorDocument(data,0,reopened))return 106;
    runtime::GameWorld world;runtime::ScenePhysics physics;runtime::QueryHit hit;
    const float left[]{-1.5f,3,0},right[]{1.5f,3,0},delta[]{0,-6,0};
    if(!world.load(reopened)||!physics.start(world)||!physics.rayCast(left,delta,{},hit)||hit.object!=2||hit.colliderInstance!=1||hit.colliderObject!=4||
       !physics.rayCast(right,delta,{},hit)||hit.object!=2||hit.colliderInstance!=1||hit.colliderObject!=5)return 112;
    if(handles)for(u32 n=0;n<2;++n) {
      const auto *shape=g.find(n?7:6);const auto *c=shape?shape->components.find(scene::Collider::descriptor):nullptr;
      const float curved[]{n?1.5f:-1.5f,3,3};
      if(!c||!physics.rayCast(curved,delta,{},hit)||hit.object!=2||hit.colliderObject!=(n?7u:6u)||hit.colliderInstance!=c->instanceId())return 118;
    }
    physics.stop();
    std::string descriptor=R"({"format":"ASTRA-PROJECT-1","resourceSource":"independent","project":{"name":"UI Body Owners v1","template":"empty","scenes":1,"assets":1},"mainScene":"scenes/editor.aescene","editorScene":"scenes/editor.aescene"})";
    if(queryProbe)descriptor.replace(descriptor.find("UI Body Owners v1"),std::string_view("UI Body Owners v1").size(),"API Query Owners v1");
    if(occlusion)descriptor.replace(descriptor.find("UI Body Owners v1"),std::string_view("UI Body Owners v1").size(),"UI Occlusion v1");
    if(handles)descriptor.replace(descriptor.find("UI Body Owners v1"),std::string_view("UI Body Owners v1").size(),"UI Collider Handles v1");
    if(integral||scale)descriptor.replace(descriptor.find("UI Collider Handles v1"),std::string_view("UI Collider Handles v1").size(),scale?"U11 Scale 256":"U11 Integral");
    if(!editor::EditorImportTransaction::writeText(out/"project.json",descriptor)||!editor::EditorImportTransaction::writeText(out/"scenes/editor.aescene",data)||
        !editor::EditorImportTransaction::writeText(out/".astra/assets.astra",s.serializeAssets()))return 107;
    std::printf("Body owners project: %s; root body, two child colliders with local UID 1\n",out.generic_string().c_str());return 0;
  }
  if(std::string_view(argv[1])=="write-controls"||std::string_view(argv[1])=="write-conversion"||std::string_view(argv[1])=="write-dynamic"||std::string_view(argv[1])=="write-object-motor"||std::string_view(argv[1])=="write-convex"||std::string_view(argv[1])=="write-convex-hierarchy") {
    const bool hierarchyMotor=std::string_view(argv[1])=="write-convex-hierarchy";
    const bool convexMotor=std::string_view(argv[1])=="write-convex"||hierarchyMotor;
    const bool conversion=std::string_view(argv[1])=="write-conversion";
    const bool objectMotor=std::string_view(argv[1])=="write-object-motor";
    const bool dynamic=std::string_view(argv[1])=="write-dynamic"||objectMotor||convexMotor;
    if(argc!=3)return 50;
    const auto out=std::filesystem::absolute(argv[2]);std::error_code ec;if(std::filesystem::exists(out,ec))return 51;
    std::filesystem::create_directories(out/"scenes",ec);std::filesystem::create_directories(out/"UI",ec);std::filesystem::create_directories(out/"Scripts",ec);if(ec)return 52;
    test::ConvexCpuLibrary convexLibrary;
    editor::EditorSession session;if(!session.setProjectDirectory(out.generic_string().c_str()))return 53;
    std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
    if(!renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials)||!session.importMap(draws,materials,false,vertices,indices,0))return 54;
    if(convexMotor&&!convexLibrary.connect(session))return 54;
    auto &d=session.gui().document();std::string error;
    const auto look=d.create(ui::GuiKind::LookArea);auto n=*d.find(look);n.name="Olhar";n.anchorMin={.5f,0};n.anchorMax={1,1};n.offsets={0,0,-24,-24};n.control.sensitivity=1;if(!d.update(n,error))return 55;
    const auto move=d.create(ui::GuiKind::Joystick);n=*d.find(move);n.name="Mover";n.anchorMin=n.anchorMax={0,1};n.offsets={32,-240,232,-40};n.control.inputRadius=80;n.foreground=0x604A555B;n.accent=0xFFB8E86B;if(!d.update(n,error))return 55;
    const auto jump=d.create(ui::GuiKind::ActionButton);n=*d.find(jump);n.name="Saltar";n.text="SALTAR";n.anchorMin=n.anchorMax={1,1};n.offsets={-190,-160,-50,-60};n.background=0x904A555B;n.radius=50;if(!d.update(n,error))return 55;
    const auto label=d.create(ui::GuiKind::Text);n=*d.find(label);n.name="Instrucoes";n.text="Mover / olhar / saltar | controles autorados";n.offsets={32,32,1900,76};n.fontSize=23;if(!d.update(n,error))return 55;
    if(dynamic){auto id=d.create(ui::GuiKind::ActionButton);n=*d.find(id);n.name="Impulso";n.text="IMPULSO";n.control.action="Impulso";n.anchorMin=n.anchorMax={1,1};n.offsets={-390,-160,-240,-60};n.background=0x904A555B;n.radius=12;if(!d.update(n,error))return 55;}
    if(!session.gui().save())return 55;
    resources::AssetRegistry registry;if(!resources::AssetRegistry::deserialize(session.serializeAssets(),registry))return 56;const auto *source=registry.findByPath("UI/main.aeui");if(!source)return 56;
    auto &g=session.document();editor::EditorEntityId actor=0,visual=0;editor::EditorEntity object;
    if(dynamic){auto actions=g.inputActions();runtime::InputAction impulse;impulse.id="Impulso";impulse.kind=runtime::ActionKind::Button;impulse.deadzone=0;if(!actions.add(impulse)||!g.setInputActions(actions))return 57;}
    if(convexMotor) {
      actor=visual=hierarchyMotor?test::convexFixtureHierarchy(session,error):test::convexFixtureObject(session,error);if(!actor){std::fprintf(stderr,"%s\n",error.c_str());return 57;}
      object=*g.find(actor);editor::assignEntityName(object,hierarchyMotor?"CompoundPlayer":"ConcavePlayer");object.transform.position[1]=2;
      if(!g.applyEntityValues(actor,object))return 57;
    } else if(objectMotor) {
      const float position[]{0,2,0};actor=visual=session.instantiateAsset(0,g.root(),position);if(!actor)return 57;
      object=*g.find(actor);editor::assignEntityName(object,"PlayerDynamic");object.transform.scale[0]=2.7f;object.transform.scale[1]=.4f;object.transform.scale[2]=1.1f;object.transform.rotationDegrees[1]=35;
      while(object.components.remove(scene::Collider::descriptor)){}while(object.components.remove(scene::PhysicsBody::descriptor)){}
      if(!g.applyEntityValues(actor,object)||!session.configureDynamicMotor(actor,editor::EditorSession::MotorCollisionPolicy::FitPrimitive,error)){std::fprintf(stderr,"%s\n",error.c_str());return 57;}
    } else if(conversion) {
      const float position[]{0,1,0};visual=session.instantiateAsset(3,g.root(),position);if(!visual)return 57;
      object=*g.find(visual);editor::assignEntityName(object,"DynamicCylinder");
      auto *body=static_cast<scene::PhysicsBody*>(object.components.edit(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Dynamic;body->mass=17;if(!g.applyEntityValues(visual,object))return 57;
    } else {
      for(u32 i=0;i<editor::editorCreationCatalog.size();++i) if(editor::editorCreationCatalog[i].id==(dynamic?"physics.motor_cylinder":"physics.character_cylinder")) actor=session.createRecipe(i,g.root());
      if(!actor||g.childrenOf(actor).size()!=1)return 57;
      visual=g.childrenOf(actor)[0];
      object=*g.find(actor);editor::assignEntityName(object,dynamic?"PlayerDynamic":"PlayerCharacter");if(!g.applyEntityValues(actor,object))return 57;
    }
    const auto floor=g.createEntity(g.root(),runtime::ObjectKind::Mesh,"Ground");object=*g.find(floor);
    if(!runtime::configurePrimitive(object,scene::PrimitiveType::Cube,{1,session.mapScene().assetGuid(0),session.mapScene().materialForAsset(0)}))return 57;
    object.transform.position[1]=-.5f;object.transform.scale[0]=object.transform.scale[2]=30;object.transform.scale[1]=1;if(!g.applyEntityValues(floor,object))return 57;
    const auto camera=g.createEntity(g.root(),runtime::ObjectKind::Camera,"Camera");object=*g.find(camera);object.components.add(scene::Camera::descriptor);object.components.add(scene::CameraLook::descriptor);
    auto *follow=static_cast<scene::CameraFollow*>(object.components.add(scene::CameraFollow::descriptor));follow->target=actor?actor:visual;follow->offset[1]=3;follow->offset[2]=-8;object.transform.position[1]=3;object.transform.position[2]=-8;object.transform.rotationDegrees[0]=12;if(!g.applyEntityValues(camera,object))return 57;
    const auto hud=g.createEntity(g.root(),runtime::ObjectKind::Folder,"HUD");object=*g.find(hud);auto *canvas=static_cast<scene::UiCanvas*>(object.components.add(scene::UiCanvas::descriptor));canvas->document=source->guid;canvas->inputReceiver=actor;canvas->inputCamera=camera;canvas->movementSpace=2;
    auto *script=static_cast<scene::ScriptBehavior*>(object.components.add(scene::ScriptBehavior::descriptor));script->scriptType="example.gui.authored-controls";script->source="Scripts/GuiAuthoredControls.cs";if(!g.applyEntityValues(hud,object))return 57;
    const auto archive=editor::serializeEditorDocument(g,0);editor::EditorDocument reopened;if(!editor::deserializeEditorDocument(archive,0,reopened))return 58;
    const std::string projectName=hierarchyMotor?"UI Hierarchy Sources v1":convexMotor?"UI Convex Parts v1":objectMotor?"UI Object Motor v2":dynamic?"UI Dynamic Motor v1":conversion?"UI Character Conversion v3":"UI Authored Controls v3";
    const std::string descriptor="{\"format\":\"ASTRA-PROJECT-1\",\"resourceSource\":\"independent\",\"project\":{\"name\":\""+projectName+"\",\"template\":\"empty\",\"scenes\":1,\"assets\":1},\"mainScene\":\"scenes/editor.aescene\",\"editorScene\":\"scenes/editor.aescene\"}";
    if(!editor::EditorImportTransaction::writeText(out/"project.json",descriptor)||!editor::EditorImportTransaction::writeText(out/"scenes/editor.aescene",archive)||!editor::EditorImportTransaction::write(out/"Scripts/GuiAuthoredControls.cs",read(root/"examples/ui/GuiAuthoredControls.cs")))return 59;
    std::printf("Authored controls project: %s\n",out.generic_string().c_str());return 0;
  }
  if(std::string_view(argv[1])=="write-instances") {
    if(argc!=3)return 30;
    const auto out=std::filesystem::absolute(argv[2]);std::error_code ec;
    if(std::filesystem::exists(out,ec))return 31;
    for(const auto *folder:{"Scripts","scenes","UI","images"})std::filesystem::create_directories(out/folder,ec);
    if(ec)return 32;
    editor::EditorSession session;if(!session.setProjectDirectory(out.generic_string().c_str()))return 33;
    std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
    if(!renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials)||!session.importMap(draws,materials,false,vertices,indices,0))return 34;
    auto &ui=session.gui().document();std::string error;auto image=ui.create(ui::GuiKind::Image);auto n=*ui.find(image);
    n.name="open_image";n.image="images/banner.png";n.offsets={40,40,320,160};n.interaction.clickable=true;
    n.motion.enabled=true;n.motion.duration=.4f;n.motion.to={16,0,1,.75f};n.actions={{ui::GuiEventKind::Click,ui::GuiClickAction::PlayAnimation,0,0}};
    if(!ui.update(n,error))return 35;
    auto label=ui.create(ui::GuiKind::Text);n=*ui.find(label);n.name="status";n.text="Entre em Play";n.offsets={40,180,460,225};n.fontSize=24;
    if(!ui.update(n,error)||!session.gui().save())return 35;
    resources::AssetRegistry registry;if(!resources::AssetRegistry::deserialize(session.serializeAssets(),registry))return 36;
    const auto *record=registry.findByPath("UI/main.aeui");if(!record)return 36;
    auto &g=session.document();
    const auto cylinder=g.createEntity(g.root(),runtime::ObjectKind::Mesh,"DynamicCylinder");auto object=*g.find(cylinder);
    if(!runtime::configurePrimitive(object,scene::PrimitiveType::Cylinder,{4,session.mapScene().assetGuid(3),session.mapScene().materialForAsset(3)}))return 37;
    auto *body=static_cast<scene::PhysicsBody*>(object.components.edit(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Dynamic;body->gravityFactor=0;body->angularY=.35f;
    if(!g.applyEntityValues(cylinder,object))return 37;
    for(u32 j=0;j<2;++j) {
      const auto id=g.createEntity(j?cylinder:g.root(),runtime::ObjectKind::Folder,j?"WorldPanel":"HUD");object=*g.find(id);
      auto *c=static_cast<scene::UiCanvas*>(object.components.add(scene::UiCanvas::descriptor));c->document=record->guid;c->mode=j;c->resolution[0]=800;c->resolution[1]=400;c->unitsPerPixel=.005f;
      if(j){c->offset[1]=1.8f;c->offset[2]=-.65f;c->occlusion=false;}
      else {auto *script=static_cast<scene::ScriptBehavior*>(object.components.add(scene::ScriptBehavior::descriptor));script->scriptType="example.gui.canvas-instances";script->source="Scripts/GuiCanvasInstances.cs";}
      if(!g.applyEntityValues(id,object))return 38;
    }
    const auto camera=g.createEntity(g.root(),runtime::ObjectKind::Camera,"Camera");object=*g.find(camera);object.components.add(scene::Camera::descriptor);object.transform.position[2]=-7;
    if(!g.applyEntityValues(camera,object))return 38;
    const auto data=editor::serializeEditorDocument(g,0);editor::EditorDocument reopened;if(!editor::deserializeEditorDocument(data,0,reopened))return 39;
    const std::string descriptor="{\"format\":\"ASTRA-PROJECT-1\",\"resourceSource\":\"independent\",\"project\":{\"name\":\"UI Canvas Instances\",\"template\":\"empty\",\"scenes\":1,\"assets\":1},\"mainScene\":\"scenes/editor.aescene\",\"editorScene\":\"scenes/editor.aescene\"}";
    if(!editor::EditorImportTransaction::writeText(out/"project.json",descriptor)||!editor::EditorImportTransaction::writeText(out/"scenes/editor.aescene",data)||!editor::EditorImportTransaction::write(out/"Scripts/GuiCanvasInstances.cs",read(root/"examples/ui/GuiCanvasInstances.cs"))||!editor::EditorImportTransaction::write(out/"images/banner.png",read(root/"examples/ui/images/banner.png")))return 40;
    std::printf("Scene-owned UI project: %s\n",out.generic_string().c_str());return 0;
  }
  if(std::string_view(argv[1])=="verify-images") {
    if(argc!=4)return 25;
    const auto project=std::filesystem::absolute(argv[2]);
    editor::EditorSession session;
    if(!session.setProjectDirectory(project.generic_string().c_str()))return 26;
    usize count=0;
    for(const auto &file:std::filesystem::directory_iterator(project/argv[3])) {
      if(file.path().extension()!=".png")continue;
      auto &doc=session.gui().document();doc=ui::GuiDocument{};
      const auto id=doc.create(ui::GuiKind::Image);auto node=*doc.find(id);
      node.image=file.path().lexically_relative(project).generic_string();std::string error;
      if(!doc.update(node,error))return 27;
      session.refreshGuiImages();const auto *image=session.guiImages().find(node.image);
      if(!image || !image->error.empty() || !image->width || !image->height) {
        std::fprintf(stderr,"IMAGE %s: %s\n",node.image.c_str(),image?image->error.c_str():"not loaded");return 28;
      }
      ++count;
    }
    std::printf("PASS native image directory: decoded=%zu\n",count);return count?0:29;
  }
  if(std::string_view(argv[1])=="verify-library") {
    if(argc!=3)return 19;
    const auto project=std::filesystem::absolute(argv[2]);
    editor::EditorSession session;
    if(!session.setProjectDirectory(project.generic_string().c_str()))return 20;
    usize pages=0,images=0,models=0;
    for(const auto &file:std::filesystem::directory_iterator(project/"UI")) {
      if(!file.path().filename().string().starts_with("page-"))continue;
      std::ifstream input(file.path());std::string error;
      if(!session.gui().document().read(input,error)) {std::fprintf(stderr,"%s: %s\n",file.path().string().c_str(),error.c_str());return 21;}
      session.refreshGuiImages();
      for(const auto &[path,image]:session.guiImages().entries()) {
        if(!image.error.empty() || !image.width || !image.height) {std::fprintf(stderr,"IMAGE %s: %s\n",path.c_str(),image.error.c_str());return 22;}
        ++images;
      }
      ++pages;
    }
    std::printf("Native pages: %zu; decoded images: %zu\n",pages,images);std::fflush(stdout);
    for(const auto &file:std::filesystem::directory_iterator(project/"models/synty")) {
      if(file.path().extension()!=".glb")continue;
      resources::GltfImport model;
      if(!resources::importGlb(read(file.path()),{}, {},model) || model.draws.empty() || model.textures.empty() || model.skippedTextures) {
        std::fprintf(stderr,"GLB %s: %s omitted=%u\n",file.path().string().c_str(),model.diagnostic.c_str(),model.skippedTextures);return 23;
      }
      ++models;
      if(models%50==0) {std::printf("Native textured models: %zu\n",models);std::fflush(stdout);}
    }
    std::printf("PASS native library: pages=%zu decoded_images=%zu textured_models=%zu\n",pages,images,models);
    return pages==22 && images==520 && models==520?0:24;
  }
  if(std::string_view(argv[1])=="verify-storage") {
    if(argc!=3)return 13;
    const auto project=std::filesystem::absolute(argv[2]).generic_string();
    editor::EditorSession session;
    if(!session.setProjectDirectory(project.c_str()) || session.gui().document().nodes().size()!=6)return 14;
    auto node=*session.gui().document().find(session.gui().document().findByName("start"));node.text="Persistiu";
    std::string error;
    if(!session.gui().document().update(node,error) || !session.gui().setResource("UI/custom.aeui") || !session.gui().save())return 15;
    editor::EditorSession reopened;
    if(!reopened.setProjectDirectory(project.c_str()))return 16;
    const auto *loaded=reopened.gui().document().find(reopened.gui().document().findByName("start"));
    if(!loaded || loaded->text!="Persistiu" || (reopened.gui().setResource("../outside.aeui") && reopened.gui().save()))return 17;
    std::printf("PASS real EditorSession storage: custom resource, save, project reopen, unsafe path rejection\n");return 0;
  }
  if(std::string_view(argv[1])=="write-project") {
    if(argc<3 || argc>4)return 6;
    const auto out=std::filesystem::absolute(argv[2]);std::error_code ec;
    if(std::filesystem::exists(out,ec)) {std::fprintf(stderr,"Use a new output directory\n");return 7;}
    for(const auto *folder:{"Scripts","scenes","UI","images"}) std::filesystem::create_directories(out/folder,ec);
    if(ec)return 8;
    editor::EditorDocument doc;
    const bool states=argc>3 && std::string_view(argv[3])=="states";
    const bool behavior=states || (argc>3 && std::string_view(argv[3])=="behavior");
    const char *source=states?"Scripts/GuiStatesActions.cs":behavior?"Scripts/GuiImageActions.cs":"Scripts/GuiMenu.cs";
    const auto id=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"GuiMenu");auto object=*doc.find(id);
    auto *script=static_cast<scene::ScriptBehavior *>(object.components.add(scene::ScriptBehavior::descriptor));
    if(!script)return 9;
    script->scriptType=states?"example.gui.states-actions":behavior?"example.gui.image-actions":"example.gui.menu";
    script->source=source;
    if(!doc.applyEntityValues(id,object))return 10;
    if(argc>3 && std::string_view(argv[3])=="expansion") {
      const auto camera=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"UICamera");auto entity=*doc.find(camera);entity.transform.position[2]=-6;
      if(!entity.components.add(scene::Camera::descriptor) || !doc.applyEntityValues(camera,entity))return 18;
    }
    const auto scene=editor::serializeEditorDocument(doc,0);editor::EditorDocument reopened;
    if(!editor::deserializeEditorDocument(scene,0,reopened))return 11;
    std::ostringstream descriptor;
    descriptor<<"{\"format\":\"ASTRA-PROJECT-1\",\"resourceSource\":\"independent\",\"project\":{\"name\":"<<std::quoted(out.filename().string())<<",\"template\":\"empty\",\"scenes\":1,\"assets\":0},\"mainScene\":\"scenes/editor.aescene\",\"editorScene\":\"scenes/editor.aescene\"}";
    if(!editor::EditorImportTransaction::writeText(out/"project.json",descriptor.str()) ||
       !editor::EditorImportTransaction::writeText(out/"scenes/editor.aescene",scene) ||
       !editor::EditorImportTransaction::write(out/source,read(root/"examples/ui"/std::filesystem::path(source).filename())) ||
       !editor::EditorImportTransaction::write(out/"UI/main.aeui",read(root/(states?"examples/ui/states-actions.aeui":behavior?"examples/ui/behavior.aeui":"examples/ui/main.aeui")))) return 12;
    if(behavior && !editor::EditorImportTransaction::write(out/"images/banner.png",read(root/"examples/ui/images/banner.png")))return 12;
    std::printf("GUI project written: %s\n",out.generic_string().c_str());return 0;
  }
  const u32 width=argc>2?static_cast<u32>(std::atoi(argv[2])):1280,height=argc>3?static_cast<u32>(std::atoi(argv[3])):720;
  const auto fontBytes=read(root/"assets/astra-visual/ui/astra-ui-font.aeuf");
  const auto iconBytes=read(root/"assets/astra-visual/ui/astra-ui-icons.aeui");
  ui::UiFont font;ui::UiIconAtlas icons;
  if(!font.load(fontBytes) || !icons.load(iconBytes)) return 2;
  const bool hierarchy=argc>5&&std::string_view(argv[5])=="convex-hierarchy";
  const bool surfaces=argc>5&&std::string_view(argv[5])=="collider-surface";
  const bool owners=argc>5&&std::string_view(argv[5])=="collider-owners";
  const bool convex=argc>5&&(std::string_view(argv[5])=="convex"||hierarchy);
  test::ConvexCpuLibrary convexLibrary;
  editor::EditorSession session;session.initialize(&font,&icons);session.setSurface({0,0,float(width),float(height)},{});if(!convex&&!surfaces&&!owners)session.openGui();
  if(!session.gui().immediate().setFont(read(root/"assets/astra-visual/ui/gui-inter.ttf"))) return 5;
  if(convex) {
    const auto project=std::filesystem::absolute(argv[4]);std::error_code ec;
    if(std::filesystem::exists(project,ec))return 70;
    std::filesystem::create_directories(project/"scenes",ec);if(ec)return 71;
    if(!session.setProjectDirectory(project.generic_string().c_str())||!convexLibrary.connect(session))return 72;
    std::string error;if(!(hierarchy?test::convexFixtureHierarchy(session,error):test::convexFixtureObject(session,error))){std::fprintf(stderr,"%s\n",error.c_str());return 73;}
    session.frameSelection();session.update();
    for(auto widget:{editor::EditorWidget::InspectorMenu,editor::EditorWidget::MotorSetupOpen,editor::EditorWidget::MotorSetupDecompose})
      if(!test::tapConvexWidget(session,font,widget))return 74;
    if(hierarchy) {
      for(auto widget:{editor::EditorWidget::MotorBakeSourcesOpen,editor::EditorWidget::MotorBakeSourcesHierarchy})if(!test::tapConvexWidget(session,font,widget))return 86;
      if(session.selectedMotorDecompositionSources().size()!=2)return 87;
      if(!test::tapConvexWidget(session,font,editor::EditorWidget::MotorBakeSourceBase)||session.selectedMotorDecompositionSources().size()!=1)return 88;
      if(!test::tapConvexWidget(session,font,editor::EditorWidget::MotorBakeSourceBase)||session.selectedMotorDecompositionSources().size()!=2)return 89;
      if(height<600){if(!test::tapConvexWidget(session,font,editor::EditorWidget::MotorBakeSourceNext))return 90;
        const auto second=static_cast<editor::EditorWidget>(editor::widgetId(editor::EditorWidget::MotorBakeSourceBase)+1);
        if(!test::tapConvexWidget(session,font,second)||!test::tapConvexWidget(session,font,second)||!test::tapConvexWidget(session,font,editor::EditorWidget::MotorBakeSourcePrevious))return 91;}
      test::UiSoftwareTarget sources;sources.resize(width,height,.1f,.1f,.1f);session.update();test::rasterizeUi(session.instances(),font,icons,sources);
      std::ofstream capture(std::string(argv[1])+"-sources.ppm",std::ios::binary);capture<<"P6\n"<<width<<' '<<height<<"\n255\n";
      for(usize i=0;i<sources.pixels.size();i+=4)for(usize c=0;c<3;++c)capture.put(static_cast<char>(std::clamp(sources.pixels[i+c],0.f,1.f)*255+.5f));
      if(!capture||!test::tapConvexWidget(session,font,editor::EditorWidget::MotorBakeSourcesDone))return 92;
    }
    for(auto widget:{hierarchy?editor::EditorWidget::MotorBakeBudgetMedium:editor::EditorWidget::MotorBakeBudgetLow,editor::EditorWidget::MotorBakeStart})if(!test::tapConvexWidget(session,font,widget))return 74;
    if(!test::waitConvexBake(session)){std::fprintf(stderr,"%s\n",session.motorDecompositionProgress().error.c_str());return 75;}
    if(!test::tapConvexWidget(session,font,editor::EditorWidget::MotorBakePartBase)||session.screen().motorBakeEnabled[0])return 76;
    if(!test::tapConvexWidget(session,font,editor::EditorWidget::MotorBakePartBase)||!session.screen().motorBakeEnabled[0])return 77;
    const usize pageSize=session.screen().motorBakePartsPerPage();
    for(usize first=0;first<session.screen().motorBakeEnabled.size();first+=pageSize){
      const usize last=std::min(first+pageSize,session.screen().motorBakeEnabled.size())-1;
      const auto widget=static_cast<editor::EditorWidget>(editor::widgetId(editor::EditorWidget::MotorBakePartBase)+static_cast<u32>(last));
      if(!test::tapConvexWidget(session,font,widget)||session.screen().motorBakeEnabled[last])return 82;
      if(!test::tapConvexWidget(session,font,widget)||!session.screen().motorBakeEnabled[last])return 83;
      if(first+pageSize<session.screen().motorBakeEnabled.size()&&!test::tapConvexWidget(session,font,editor::EditorWidget::MotorBakeNext))return 84;
    }
    while(session.screen().motorBakePage)if(!test::tapConvexWidget(session,font,editor::EditorWidget::MotorBakePrevious))return 85;
    std::printf("Native decomposition preview: %zu parts; toggles reached through pointer routing\n",session.screen().motorBakePreview.size());
  }
  if(surfaces) {
    auto &g=session.document();const auto id=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Compound · superfícies");auto object=*g.find(id);
    object.components.add(scene::PhysicsBody::descriptor);
    auto *first=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));first->centerX=-1.5f;
    auto *second=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));second->centerX=1.5f;const auto instance=second->instanceId();
    if(!g.applyEntityValues(id,object))return 93;
    session.setSelection(id);session.frameAll();const float eye[]{0,2,-8};session.setCameraPose(eye,0,.24f);session.update();
    if(!test::tapConvexWidget(session,font,editor::EditorWidget::ToolSelect)||!test::tapConvexWidget(session,font,static_cast<editor::EditorWidget>(editor::widgetId(editor::EditorWidget::ComponentFoldBase)+1)))return 94;
    const auto original=editor::serializeEditorDocument(g,0);const auto undo=session.history().undoDepth();
    const float at[]{1.5f,0,0};const auto point=editor::projectWorldToScreen(session.view(),at);
    if(!point.valid)return 95;
    session.handlePointer({92,ui::UiPointerPhase::Down,point.screen,0});session.handlePointer({92,ui::UiPointerPhase::Up,point.screen,.02});session.update();
    if(session.selection()!=id || session.screen().expandedNative!=instance || session.history().undoDepth()!=undo || editor::serializeEditorDocument(g,0)!=original)return 96;
    std::printf("Surface pointer capture: owner=%u collider=%llu, authoring/history unchanged\n",id,static_cast<unsigned long long>(instance));
  }
  if(owners) {
    const auto id=bodyOwnerFixture(session,false);if(!id)return 108;
    session.setSelection(id);session.frameAll();const float eye[]{0,2,-8};session.setCameraPose(eye,0,.24f);session.update();
    if(!test::tapConvexWidget(session,font,editor::EditorWidget::ToolSelect)||!test::tapConvexWidget(session,font,editor::EditorWidget::ComponentFoldBase))return 109;
    const auto original=editor::serializeEditorDocument(session.document(),0);const auto undo=session.history().undoDepth();
    const float at[]{1.5f,0,0};const auto point=editor::projectWorldToScreen(session.view(),at);
    if(!point.valid)return 110;
    session.handlePointer({95,ui::UiPointerPhase::Down,point.screen,0});session.handlePointer({95,ui::UiPointerPhase::Up,point.screen,.02});session.update();
    const auto *object=session.document().find(session.selection());
    if(!object||std::string_view(object->name)!="RightCylinder"||session.screen().expandedNative!=1||session.history().undoDepth()!=undo||editor::serializeEditorDocument(session.document(),0)!=original)return 111;
    std::printf("Body owners pointer capture: root=%u authoring=%u collider=1; archive/history unchanged\n",id,session.selection());
  }
  if(argc>4&&!convex&&!surfaces&&!owners) {
    const auto folder=std::filesystem::path(argv[4]).parent_path();
    const auto project=folder.filename()=="UI"?folder.parent_path():folder;
    if(!session.setProjectDirectory(project.string().c_str()))return 14;
    std::ifstream input(argv[4]);std::string error;
    if(!session.gui().document().read(input,error)) {std::fprintf(stderr,"%s\n",error.c_str());return 3;}
    session.gui().select(session.gui().document().findByName(argc>6?argv[6]:"volume"));
  }
  if(argc>5 && std::string_view(argv[5])=="preview") session.gui().setPreview(true);
  if(argc>5 && std::string_view(argv[5])=="scene") {
    const auto project=std::filesystem::path(argv[4]).parent_path().parent_path();
    const auto registryBytes=read(project/".astra/assets.astra");
    if(registryBytes.empty() || !session.loadAssets(std::string_view(reinterpret_cast<const char*>(registryBytes.data()),registryBytes.size())))return 44;
    std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
    if(!renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials)||!session.importMap(draws,materials,false,vertices,indices,0))return 43;
    if(!session.load((project/"scenes/editor.aescene").string().c_str(),0))return 41;
    session.refreshGuiTree();editor::EditorGuiTarget selection;
    const auto wanted=session.gui().document().findByName(argc>6?argv[6]:"open_image");
    for(const auto &row:session.screen().guiRows)if(row.target.node==wanted){selection=row.target;break;}
    // load preserves the existing workspace; selecting UI returns to Scene.
    if(!selection.valid()) {
      editor::EditorGuiTree tree;resources::AssetRegistry registry;resources::AssetRegistry::deserialize(session.serializeAssets(),registry);
      tree.rebuild(session.document(),registry,[&](resources::AssetGuid id,ui::GuiDocument &doc,std::string &error){return session.loadGuiDocument(id,doc,error);},session.gui().resource(),session.gui().document());
      for(const auto &row:tree.rows())if(row.target.node==wanted){selection=row.target;break;}
    }
    if(!session.selectGuiElement(selection))return 42;
  }
  session.update();session.update();
  test::UiSoftwareTarget target;target.resize(width,height,0.1f,0.1f,0.1f);
  const auto &im=session.immediateGui();
  const auto &guiImages=session.guiImages();
  test::rasterizeUi(session.instances(),font,icons,target,im.atlas(),im.atlasWidth(),im.atlasHeight(),guiImages.pixels(),guiImages.size());
  std::ofstream output(argv[1],std::ios::binary);output<<"P6\n"<<width<<' '<<height<<"\n255\n";
  for(usize i=0;i<target.pixels.size();i+=4) for(usize c=0;c<3;++c) output.put(static_cast<char>(std::clamp(target.pixels[i+c],0.0f,1.0f)*255+0.5f));
  if(convex) {
    if(!test::tapConvexWidget(session,font,editor::EditorWidget::MotorSetupApply))return 78;
    if(session.screen().motorSetupTarget){std::fprintf(stderr,"%s\n",session.screen().motorSetupError.c_str());return 79;}
    const auto project=std::filesystem::absolute(argv[4]);
    const auto archive=editor::serializeEditorDocument(session.document(),0);
    const std::string descriptor=R"({"format":"ASTRA-PROJECT-1","resourceSource":"independent","project":{"name":"Convex Parts Acceptance","template":"empty","scenes":1,"assets":2},"mainScene":"scenes/editor.aescene","editorScene":"scenes/editor.aescene"})";
    if(!editor::EditorImportTransaction::writeText(project/"scenes/editor.aescene",archive)||!editor::EditorImportTransaction::writeText(project/"project.json",descriptor))return 80;
    test::UiSoftwareTarget inspector;inspector.resize(width,height,.1f,.1f,.1f);session.update();
    test::rasterizeUi(session.instances(),font,icons,inspector);
    std::ofstream extra(std::string(argv[1])+"-inspector.ppm",std::ios::binary);extra<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(usize i=0;i<inspector.pixels.size();i+=4)for(usize c=0;c<3;++c)extra.put(static_cast<char>(std::clamp(inspector.pixels[i+c],0.f,1.f)*255+.5f));
    if(!extra)return 81;
  }
  std::printf("%ux%u elements=%u instances=%u rejected_imgui=%u canvas=%.0fx%.0f\n",width,height,
    static_cast<u32>(session.gui().document().nodes().size()),static_cast<u32>(session.instances().size()),im.rejectedCommands(),
    session.gui().canvas().width,session.gui().canvas().height);
  return output?0:4;
}
