#include "harness.h"
#include "editor/editor_session.h"
#include "editor/editor_archive.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_component_visuals.h"
#include "editor/editor_import_transaction.h"
#include "renderer/primitive_geometry.h"
#include "runtime/primitive_object.h"
#include "scene/component_reflection.h"
#include <filesystem>
#include <chrono>
using namespace ae;
namespace {
bool near(float a,float b,float e=.005f){return std::abs(a-b)<e;}
template<class T> u64 field(runtime::SceneGraph &g,runtime::ObjectId id){auto v=*g.find(id);auto *c=v.components.add(T::descriptor);auto instance=c->instanceId();g.applyEntityValues(id,v);return instance;}
runtime::ObjectId body(editor::EditorDocument &g,float x,float mass=1){auto id=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Body");auto v=*g.find(id);v.transform.position[0]=x;auto *b=static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor));b->motion=scene::BodyMotion::Dynamic;b->mass=mass;b->gravityFactor=0;b->linearDamping=b->angularDamping=0;v.components.add(scene::Collider::descriptor);g.applyEntityValues(id,v);return id;}
void configure(runtime::SceneGraph &g,runtime::ObjectId id,const scene::ComponentType &type,void (*edit)(scene::PhysicsFieldProperties&)){auto v=*g.find(id);auto *c=static_cast<scene::PhysicsFieldProperties*>(v.components.edit(type));edit(*c);g.applyEntityValues(id,v);}
scene::ScriptSceneAccess access;
scene::ScriptRuntimeApi api(){scene::ScriptRuntimeApi a;a.start=[](const u8*,int,const u8*,int,const scene::ScriptSceneAccess *s){access=*s;return s->available()?0:1;};a.update=[](float){return 0;};a.fixedUpdate=a.lateUpdate=a.update;a.stop=[]{};a.copyDiagnostics=[](u8*,int){return 0;};a.trigger=[](u64,u64,u32){return 0;};a.contact=[](u64,u64,u32,const float*){return 0;};a.timer=[](u64,u64,u32){return 0;};a.lifecycle=[](u32,u32){return 0;};return a;}
}

AE_TEST(fields50_creation_archive_undo_and_real_visuals){
 AE_EXPECT_TRUE(scene::auditComponentContracts().empty(),"all fields have real consumers");
 editor::EditorSession session;auto &g=session.document();const char *recipes[]{"field.gravity","field.wind","field.drag","field.radial"};runtime::ObjectId last=0;
 for(const char *name:recipes){u32 recipe=0;AE_EXPECT_TRUE(editor::findCreationRecipe(name,&recipe),"creation entry");last=session.createRecipe(recipe,g.root());AE_EXPECT_TRUE(last,"real editor creation");auto v=*g.find(last);const scene::ComponentValue *component=nullptr;for(usize n=0;n<v.components.size();++n)if(scene::physicsFieldKind(*v.components.at(n))>=0)component=v.components.at(n);AE_EXPECT_TRUE(component,"created field type");if(!component)continue;
  const auto &type=component->type();const auto instance=component->instanceId();
  AE_EXPECT_TRUE(scene::setComponentTriple(v.components,type.id,"offset",{1,2,3},instance)==scene::ComponentPropertyStatus::Applied,"atomic local center");
  AE_EXPECT_TRUE(scene::setComponentTriple(v.components,type.id,"half_extents",{0,4,5},instance)!=scene::ComponentPropertyStatus::Applied,"singular volume rejected atomically");
  AE_EXPECT_TRUE(g.applyEntityValues(last,v),"authoring mutation");const auto payload=editor::serializeEditorDocument(g,456);editor::EditorDocument copy;AE_EXPECT_TRUE(editor::deserializeEditorDocument(payload,456,copy),"four types persist and reopen");
  const auto *saved=static_cast<const scene::PhysicsFieldProperties*>(copy.find(last)->components.find(type));AE_EXPECT_TRUE(saved&&near(saved->offset[2],3)&&near(saved->halfExtents[0],3),"no partial vector write");
  const auto visuals=editor::collectComponentVisuals(copy,last,1);AE_EXPECT_TRUE(!visuals.empty()&&visuals.back().segments.size()>=12,"volume gizmo and actual atlas icon");
 }
 AE_EXPECT_TRUE(session.history().undo(g)&&!g.exists(last),"undo creation");AE_EXPECT_TRUE(session.history().redo(g)&&g.exists(last),"redo creation");
}

AE_TEST(fields50_hierarchy_volume_falloff_layers_and_center){
 runtime::SceneGraph g;auto parent=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Parent"),id=g.createEntity(parent,runtime::ObjectKind::Folder,"Field");auto instance=field<scene::GravityField>(g,id);runtime::Transform t;t.position[0]=10;t.rotationDegrees[2]=90;t.scale[0]=2;t.scale[1]=3;g.setTransform(parent,t);
 configure(g,id,scene::GravityField::descriptor,[](auto &c){c.offset[0]=1;c.shape=1;c.radius=2;c.falloff=1;c.affectedLayer=3;});
 const auto *c=g.find(id)->components.findInstance(instance);runtime::PhysicsFieldFrame frame;AE_EXPECT_TRUE(runtime::makePhysicsFieldFrame(g,id,*c,frame),"full hierarchy and nonuniform scale");
 auto center=runtime::samplePhysicsField(frame,frame.center,2);AE_EXPECT_TRUE(near(center.weight,1)&&near(center.acceleration[0],9.81f),"gravity orientation excludes scale amplification");
 float halfway[3];for(u32 a=0;a<3;++a)halfway[a]=frame.center[a]+frame.world[a];auto value=runtime::samplePhysicsField(frame,halfway,2);AE_EXPECT_TRUE(near(value.weight,.5f),"sphere local distance honors scale");AE_EXPECT_TRUE(runtime::samplePhysicsField(frame,halfway,1).weight==0,"layer filter does not change geometry");
 float outside[3];for(u32 a=0;a<3;++a)outside[a]=frame.center[a]+frame.world[a]*3;AE_EXPECT_TRUE(!(runtime::samplePhysicsField(frame,outside,2).flags&1),"outside ellipsoid");
 configure(g,id,scene::GravityField::descriptor,[](auto &c){c.shape=0;c.falloff=2;});runtime::makePhysicsFieldFrame(g,id,*g.find(id)->components.findInstance(instance),frame);AE_EXPECT_TRUE(near(runtime::samplePhysicsField(frame,halfway,2).weight,20.f/27),"smoothstep box falloff");
 configure(g,id,scene::GravityField::descriptor,[](auto &c){c.enabled=false;});runtime::makePhysicsFieldFrame(g,id,*g.find(id)->components.findInstance(instance),frame);value=runtime::samplePhysicsField(frame,frame.center,2);AE_EXPECT_TRUE(value.flags==1&&value.weight==0,"disabled remains geometrically inspectable");
}

AE_TEST(fields50_real_solver_gravity_wind_drag_radial_and_sleep){
 // Two overlapping zero-gravity replacements cancel world gravity once.
 {editor::EditorDocument g;auto b=body(g,0);auto v=*g.find(b);static_cast<scene::PhysicsBody*>(v.components.edit(scene::PhysicsBody::descriptor))->gravityFactor=1;g.applyEntityValues(b,v);
  for(int n=0;n<2;++n){auto id=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Zero gravity");field<scene::GravityField>(g,id);configure(g,id,scene::GravityField::descriptor,[](auto &c){c.vector[1]=0;});}
  runtime::GameWorld w;runtime::ScenePhysics physics;AE_EXPECT_TRUE(w.load(g)&&physics.start(w)&&physics.advance(1,w),"real gravity replacement");float velocity[3];physics.getBodyVelocity(b,velocity);AE_EXPECT_TRUE(near(velocity[1],0),"no double cancellation");}
 // Wind is a force coupling: mass 2 responds more slowly than mass 1.
 {editor::EditorDocument g;auto a=body(g,-10,1),b=body(g,10,2),id=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Wind");field<scene::WindField>(g,id);configure(g,id,scene::WindField::descriptor,[](auto &c){c.halfExtents[0]=100;c.vector[0]=4;c.coefficient=2;});runtime::GameWorld w;runtime::ScenePhysics physics;AE_EXPECT_TRUE(w.load(g)&&physics.start(w),"wind world");for(int n=0;n<60;++n)AE_EXPECT_TRUE(physics.advance(1.0/60,w),"fixed integration");float va[3],vb[3];physics.getBodyVelocity(a,va);physics.getBodyVelocity(b,vb);AE_EXPECT_TRUE(near(va[0],4*(1-std::exp(-2.f)))&&near(vb[0],4*(1-std::exp(-1.f))),"mass-dependent analytical wind");}
 {editor::EditorDocument g;auto b=body(g,0),id=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Drag");auto v=*g.find(b);auto *settings=static_cast<scene::PhysicsBody*>(v.components.edit(scene::PhysicsBody::descriptor));settings->velocityX=2;settings->angularY=2;g.applyEntityValues(b,v);field<scene::DragField>(g,id);configure(g,id,scene::DragField::descriptor,[](auto &c){c.linearDrag=1000;c.angularDrag=1000;});runtime::GameWorld w;runtime::ScenePhysics physics;AE_EXPECT_TRUE(w.load(g)&&physics.start(w)&&physics.advance(1.0/60,w),"strong drag");AetherBodyStateV1 state;AE_EXPECT_TRUE(physics.bodyCommand(w,w.handle(b),w.findComponent(w.handle(b),"astra.physics.body").instance,0,{},{},state)==runtime::WorldStatus::Ok,"actual angular snapshot");AE_EXPECT_TRUE(state.linear.x>=0&&state.linear.x<.001f&&state.angular.y>=0&&state.angular.y<.001f,"strong damping is finite and does not reverse");}
 {editor::EditorDocument g;auto b=body(g,1),id=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Radial");auto instance=field<scene::RadialField>(g,id);configure(g,id,scene::RadialField::descriptor,[](auto &c){c.acceleration=-6;c.tangentialAcceleration=3;c.wakeBodies=false;});runtime::GameWorld w;runtime::ScenePhysics physics;AE_EXPECT_TRUE(w.load(g)&&physics.start(w),"radial world");AetherBodyStateV1 state;auto component=w.findComponent(w.handle(b),"astra.physics.body");physics.bodyCommand(w,w.handle(b),component.instance,3,{},{},state);AE_EXPECT_TRUE(physics.advance(1.0/60,w),"sleep step");physics.bodyCommand(w,w.handle(b),component.instance,0,{},{},state);AE_EXPECT_TRUE(state.flags&8,"wake disabled preserves sleeping body");
  AE_EXPECT_TRUE(w.setProperty({w.handle(id),instance},"wake_bodies",true)==runtime::WorldStatus::Ok,"live field edit without solver rebuild");AE_EXPECT_TRUE(physics.advance(1.0/60,w),"wake step");physics.bodyCommand(w,w.handle(b),component.instance,0,{},{},state);AE_EXPECT_TRUE(state.flags&1&&near(state.linear.x,-.1f)&&near(state.linear.z,-.05f),"radial and tangent reach actual solver");}
}

AE_TEST(fields50_script_abi_membership_identity_and_live_edits){
 editor::EditorDocument g;auto a=body(g,-1,1),b=body(g,1,2),id=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Field");(void)a;auto second=*g.find(b);second.layer=1;g.applyEntityValues(b,second);auto instance=field<scene::WindField>(g,id);auto v=*g.find(id);auto *s=static_cast<scene::ScriptBehavior*>(v.components.add(scene::ScriptBehavior::descriptor));s->scriptType="Fields50";s->source="Fields50.cs";g.applyEntityValues(id,v);
 editor::EditorMapScene map;editor::EditorPlayScene play;play.setScriptRuntime(api(),"/test");AE_EXPECT_TRUE(play.start(g,map),"real ScriptBridge and native physics");const auto handle=play.world().handle(id);float point[3]{};scene::ScriptFieldState result;
 auto query=[&](u32 op){return access.fieldQuery(access.context,id,handle.world,handle.generation,instance,op,point,0,&result);};
 AE_EXPECT_TRUE(access.version==scene::ScriptSceneAccess{}.version&&access.available()&&query(0)&&near(result.windVelocity[0],5),"ABI34 sample uses actual model");AE_EXPECT_TRUE(query(2)&&result.affectedBodies==2&&near(result.affectedMass,3),"current solver membership and mass");
 AE_EXPECT_TRUE(play.world().setProperty({handle,instance},"affected_layer",u32{1})==runtime::WorldStatus::Ok&&query(2)&&result.affectedBodies==1&&near(result.affectedMass,1),"statistics use actual gameplay layer");play.world().setProperty({handle,instance},"affected_layer",u32{0});
 AE_EXPECT_TRUE(play.world().setTriple({handle,instance},"vector",{8,0,0})==runtime::WorldStatus::Ok&&query(0)&&near(result.windVelocity[0],8),"live reflected tuple affects query");
 result.reserved=1;AE_EXPECT_TRUE(!query(0)&&access.lastStatus(access.context)==u32(runtime::WorldStatus::InvalidArgument),"reserved layout rejected");result={};
 AE_EXPECT_TRUE(!access.fieldQuery(access.context,id,handle.world+1,handle.generation,instance,0,point,0,&result),"foreign world");AE_EXPECT_TRUE(!access.fieldQuery(access.context,id,handle.world,handle.generation+1,instance,0,point,0,&result),"stale generation");
 AE_EXPECT_TRUE(play.world().setProperty({handle,instance},"enabled",false)==runtime::WorldStatus::Ok&&query(1)&&(result.flags&1)&&query(2)&&result.affectedBodies==0,"disabled: inspect geometry, no effect/membership");
 AE_EXPECT_TRUE(play.world().removeComponent({handle,instance})==runtime::WorldStatus::Ok&&query(1),"removal is queued until safe point");play.world().flush();AE_EXPECT_TRUE(!query(1)&&access.lastStatus(access.context)==u32(runtime::WorldStatus::ComponentMissing),"removed field identity rejected after flush");
 const auto callback=access;play.stop();AE_EXPECT_TRUE(!callback.fieldQuery(callback.context,id,handle.world,handle.generation,instance,0,point,0,&result)&&callback.lastStatus(callback.context)==u32(runtime::WorldStatus::NotRunning),"callback after stop rejected");
}

// Fixture exporter is defined below; it uses actual editor resources and archive.
int writeFieldsProject(const char *directory){
 namespace fs=std::filesystem;std::error_code ec;const auto root=fs::absolute(editor::EditorImportTransaction::fromUtf8(directory),ec);
 if(ec||(fs::exists(root,ec)&&!fs::is_empty(root,ec)))return 2;
 fs::create_directories(root/"scenes",ec);if(ec)return 2;
 fs::create_directories(root/"Scripts",ec);if(ec)return 2;
 std::vector<u8> source;if(!editor::EditorImportTransaction::read(fs::path(AETHER_REPOSITORY_ROOT)/"tests/fixtures/physics/Fields50Probe.cs",source)||!editor::EditorImportTransaction::write(root/"Scripts/Fields50Probe.cs",source))return 2;
 editor::EditorSession session;if(!session.setProjectDirectory(root.generic_string().c_str()))return 2;
 std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
 if(!renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials)||!session.importMap(draws,materials,false,vertices,indices,0))return 2;
 auto &g=session.document();const scene::ComponentType *types[]{&scene::GravityField::descriptor,&scene::WindField::descriptor,&scene::DragField::descriptor,&scene::RadialField::descriptor};
 const char *names[]{"Gravidade","Vento","Arrasto","Radial"};
 for(u32 n=0;n<4;++n){auto id=g.createEntity(g.root(),runtime::ObjectKind::Folder,names[n]);auto v=*g.find(id);v.transform.position[0]=float(n)*8-12;v.components.add(*types[n]);if(n==1){auto *script=static_cast<scene::ScriptBehavior*>(v.components.add(scene::ScriptBehavior::descriptor));script->scriptType="acceptance.fields50";script->source="Scripts/Fields50Probe.cs";}if(!g.applyEntityValues(id,v))return 2;auto b=body(g,float(n)*8-12+(n==3?1:0));v=*g.find(b);std::snprintf(v.name,sizeof(v.name),n==1?"%s":"Corpo: %s",n==1?"Body":names[n]);if(!runtime::configurePrimitive(v,scene::PrimitiveType::Sphere,{2,session.mapScene().assetGuid(1),session.mapScene().materialForAsset(1)}))return 2;auto *settings=static_cast<scene::PhysicsBody*>(v.components.edit(scene::PhysicsBody::descriptor));settings->motion=scene::BodyMotion::Dynamic;settings->gravityFactor=0;settings->velocityX=n==2?2:0;if(!g.applyEntityValues(b,v))return 2;}
 auto camera=g.createEntity(g.root(),runtime::ObjectKind::Camera,"Camera");auto v=*g.find(camera);v.components.add(scene::Camera::descriptor);v.transform.position[1]=8;v.transform.position[2]=-24;v.transform.rotationDegrees[0]=18;if(!g.applyEntityValues(camera,v))return 2;
 auto light=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Sol");v=*g.find(light);auto *sun=static_cast<scene::Light*>(v.components.add(scene::Light::descriptor));sun->kind=scene::LightKind::Directional;sun->intensity=10000;v.transform.rotationDegrees[0]=-45;if(!g.applyEntityValues(light,v))return 2;
 const auto data=editor::serializeEditorDocument(g,0);editor::EditorDocument reopened;if(!editor::deserializeEditorDocument(data,0,reopened)||!editor::EditorImportTransaction::writeText(root/"scenes/editor.aescene",data))return 2;
 const std::string descriptor="{\"format\":\"ASTRA-PROJECT-1\",\"resourceSource\":\"independent\",\"project\":{\"name\":\"Fields50-20261001\",\"template\":\"empty\",\"scenes\":1,\"assets\":0},\"mainScene\":\"scenes/editor.aescene\",\"editorScene\":\"scenes/editor.aescene\"}";
 if(!editor::EditorImportTransaction::writeText(root/"project.json",descriptor))return 2;
 std::printf("Fields50 editable fixture: %s\n",root.generic_string().c_str());return 0;
}

int benchmarkFields(){
 editor::EditorDocument g;
 for(u32 n=0;n<96;++n)body(g,float(n)*3);
 for(u32 n=0;n<32;++n){auto id=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Wind benchmark");field<scene::WindField>(g,id);configure(g,id,scene::WindField::descriptor,[](auto &c){c.halfExtents[0]=1000;c.coefficient=.05f;});}
 runtime::GameWorld w;runtime::ScenePhysics physics;if(!w.load(g)||!physics.start(w))return 1;
 for(u32 n=0;n<5;++n)if(!physics.advance(1.0/60,w))return 1;
 std::array<double,60> milliseconds{};double sum=0;
 for(auto &value:milliseconds){const auto start=std::chrono::steady_clock::now();if(!physics.advance(1.0/60,w))return 1;value=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();sum+=value;}
 std::sort(milliseconds.begin(),milliseconds.end());
 std::printf("Host Debug: 96 dynamic bodies, 32 overlapping fields, 60 fixed steps; full fields+solver+sync mean=%.3f ms p95=%.3f ms max=%.3f ms\n",sum/60,milliseconds[56],milliseconds.back());return 0;
}
