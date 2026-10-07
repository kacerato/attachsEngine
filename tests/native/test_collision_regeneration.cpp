#include "convex_bake_fixture.h"
#include "harness.h"
#include "scene/collision_recipe.h"
#include "scene/prefab_link.h"
#include "ui_software_raster.h"
#include "skinned_glb_fixture.h"
#include <fstream>
using namespace ae;
namespace {
std::vector<u8> closedSkinCube() {
  std::vector<u8> binary;
  const auto word=[&](u32 v){for(u32 k=0;k<4;++k)binary.push_back(static_cast<u8>(v>>(k*8)));};
  const auto scalar=[&](float value){word(std::bit_cast<u32>(value));};
  for(float z:{-1.f,1.f})for(float y:{-1.f,1.f})for(float x:{-1.f,1.f})for(float v:{x,y,z})scalar(v);
  binary.insert(binary.end(),32,0); // all JOINTS_0 refer to the actual bone
  for(u32 i=0;i<8;++i)for(float v:{1.f,0.f,0.f,0.f})scalar(v);
  for(u32 i:{0,1,5,0,5,4,2,6,7,2,7,3,0,4,6,0,6,2,1,3,7,1,7,5,0,2,3,0,3,1,4,5,7,4,7,6})word(i);
  for(float v:{1.f,0.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,0.f,1.f})scalar(v);
  std::string json=R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"name":"ClosedRig","children":[1,2]},{"name":"Bone"},{"name":"ClosedSkin","mesh":0,"skin":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":1,"WEIGHTS_0":2},"indices":3}]}],"skins":[{"joints":[1],"inverseBindMatrices":4,"skeleton":1}],"buffers":[{"byteLength":464}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":96},{"buffer":0,"byteOffset":96,"byteLength":32},{"buffer":0,"byteOffset":128,"byteLength":128},{"buffer":0,"byteOffset":256,"byteLength":144},{"buffer":0,"byteOffset":400,"byteLength":64}],"accessors":[{"bufferView":0,"componentType":5126,"count":8,"type":"VEC3","min":[-1,-1,-1],"max":[1,1,1]},{"bufferView":1,"componentType":5121,"count":8,"type":"VEC4"},{"bufferView":2,"componentType":5126,"count":8,"type":"VEC4"},{"bufferView":3,"componentType":5125,"count":36,"type":"SCALAR"},{"bufferView":4,"componentType":5126,"count":1,"type":"MAT4"}]})";
  while(json.size()%4)json+=' ';
  auto payload=std::move(binary);binary.clear();word(0x46546c67);word(2);word(static_cast<u32>(28+json.size()+payload.size()));word(static_cast<u32>(json.size()));word(0x4e4f534a);binary.insert(binary.end(),json.begin(),json.end());word(static_cast<u32>(payload.size()));word(0x004e4942);binary.insert(binary.end(),payload.begin(),payload.end());return binary;
}
struct RecipeFixture {
  std::filesystem::path root =
      std::filesystem::path(AETHER_REPOSITORY_ROOT) / "build" /
      ("collision-recipe-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  test::ConvexCpuLibrary library;
  editor::EditorSession session;
  ui::UiFont font;
  ui::UiIconAtlas icons;
  // Both loaders retain views of the binary pixel data. Keep the backing files
  // alive for routing AND software rendering, as the Android asset owner does.
  std::vector<u8> fontBytes, iconBytes;
  editor::EditorEntityId owner = 0, left = 0, right = 0;
  bool setup() {
    const auto read = [](const char *file) {
      std::ifstream in(std::filesystem::path(AETHER_REPOSITORY_ROOT) /
                           "assets/astra-visual/ui" / file,
                       std::ios::binary);
      return std::vector<u8>(std::istreambuf_iterator<char>(in), {});
    };
    fontBytes = read("astra-ui-font.aeuf");
    iconBytes = read("astra-ui-icons.aeui");
    if (!font.load(fontBytes) || !icons.load(iconBytes))
      return false;
    std::filesystem::create_directories(root);
    if (!session.setProjectDirectory(root.generic_string().c_str()) ||
        !library.connect(session))
      return false;
    session.initialize(&font, &icons);
    session.setSurface({0, 0, 1280, 720}, {});
    auto &g = session.document();
    owner = g.createEntity(g.root(), runtime::ObjectKind::Folder, "RecipeBody");
    for (u32 i = 0; i < 2; ++i) {
      const auto id = g.createEntity(owner, runtime::ObjectKind::Mesh,
                                     i ? "RightSource" : "LeftSource");
      auto e = *g.find(id);
      if (!runtime::configurePrimitive(
              e, scene::PrimitiveType::Cube,
              {1, session.mapScene().assetGuid(0),
               session.mapScene().materialForAsset(0)}))
        return false;
      e.components.remove(scene::PhysicsBody::descriptor);
      e.components.remove(scene::Collider::descriptor);
      e.transform.position[0] = i ? 2 : -2;
      if (!g.applyEntityValues(id, e))
        return false;
      (i ? right : left) = id;
    }
    const editor::EditorSession::MotorBakeSource selected[]{{left, 0},
                                                            {right, 0}};
    std::string error;
    if (!session.selectMotorDecompositionSources(owner, selected, error))
      return false;
    resources::ConvexBakeSettings settings;
    settings.maximumParts = 4;
    settings.voxelResolution = 10000;
    settings.timeBudgetSeconds = 30;
    if (!session.beginMotorDecomposition(owner, settings, error) ||
        !test::waitConvexBake(session) ||
        !session.applyMotorDecomposition(error)) {
      std::fprintf(stderr, "recipe setup: %s %s\n", error.c_str(),
                   session.motorDecompositionProgress().error.c_str());
      return false;
    }
    return true;
  }
  bool regenerate(std::string &error) {
    return session.beginMotorRegeneration(owner, error) &&
           test::waitConvexBake(session);
  }
};
bool cast(runtime::ScenePhysics &physics, float x, runtime::QueryHit &hit) {
  const float p[]{x, 3, 0}, d[]{0, -6, 0};
  return physics.rayCast(p, d, {}, hit);
}
void capture(editor::EditorSession &s, const ui::UiFont &font,
             const ui::UiIconAtlas &icons, const std::filesystem::path &path) {
  s.update();
  test::UiSoftwareTarget target;
  const auto size = s.screen().surface;
  target.resize(static_cast<u32>(size.width), static_cast<u32>(size.height),
                .1f, .1f, .1f);
  const auto &im = s.immediateGui();
  const auto &images = s.guiImages();
  test::rasterizeUi(s.instances(), font, icons, target, im.atlas(),
                    im.atlasWidth(), im.atlasHeight(), images.pixels(),
                    images.size());
  std::ofstream out(path, std::ios::binary);
  out << "P6\n"
      << static_cast<u32>(size.width) << ' ' << static_cast<u32>(size.height)
      << "\n255\n";
  for (usize i = 0; i < target.pixels.size(); i += 4)
    for (u32 c = 0; c < 3; ++c)
      out.put(static_cast<char>(
          std::clamp(target.pixels[i + c], 0.f, 1.f) * 255 + .5f));
}
} // namespace
AE_TEST(collision_recipe_authored_skin_and_morph_snapshot_consumers) {
  RecipeFixture f;AE_EXPECT_TRUE(f.setup(),"loaded resource and authoring session");
  auto &s=f.session;auto &g=s.document();std::string error;
  const auto import=[&](std::vector<u8> bytes,const char *path) {
    resources::GltfImport model;editor::EditorSession::ModelImportReport report;
    if(!resources::importGlb(bytes,{},{},model)||!s.commitModelImport(bytes,model,path,"",report)||!s.instantiateModel(report.source,report,false))return editor::EditorEntityId{};
    return s.selection();
  };
  const auto rig=import(test::skinnedAnimatedGlb(),"Sources/skin.glb");
  AE_EXPECT_TRUE(rig,"real skin source publication and instantiation");
  std::vector<editor::EditorEntityId> ids;g.collectSubtree(rig,ids);editor::EditorEntityId skinId=0;
  for(auto id:ids)if(g.find(id)->components.find(scene::SkinnedMesh::descriptor))skinId=id;
  AE_EXPECT_TRUE(skinId,"real imported Skin with scene bone references");
  auto skinned=*g.find(skinId);const auto *render=runtime::meshRenderer(skinned);
  const auto mesh=s.mapScene().assetSlot(render->slotAsset(0));
  std::vector<editor::EditorPickMesh::Triangle> initial,moved;
  AE_EXPECT_TRUE(s.mapScene().authoredGeometry(g,skinId,mesh,initial,error),error.c_str());
  const auto *skin=static_cast<const scene::SkinnedMesh*>(skinned.components.find(scene::SkinnedMesh::descriptor));
  AE_EXPECT_TRUE(!skin->bones.empty(),"bound bones");
  auto bone=*g.find(static_cast<editor::EditorEntityId>(skin->bones.front()));bone.transform.position[0]+=.7f;
  AE_EXPECT_TRUE(g.applyEntityValues(bone.id,bone)&&s.mapScene().authoredGeometry(g,skinId,mesh,moved,error)&&initial!=moved,"bone transforms change CPU snapshot, using renderer palette");
  static_cast<scene::SkinnedMesh*>(skinned.components.edit(scene::SkinnedMesh::descriptor))->bones[0]=0;
  AE_EXPECT_TRUE(g.applyEntityValues(skinId,skinned)&&!s.mapScene().authoredGeometry(g,skinId,mesh,moved,error)&&error.find("ossos")!=std::string::npos,"missing bones refuse snapshot instead of bind fallback");
  const auto closedRoot=import(closedSkinCube(),"Sources/closed-skin.glb");AE_EXPECT_TRUE(closedRoot,"real closed skinned GLB");
  ids.clear();g.collectSubtree(closedRoot,ids);editor::EditorEntityId closedMesh=0;
  for(auto id:ids)if(g.find(id)->components.find(scene::SkinnedMesh::descriptor))closedMesh=id;
  AE_EXPECT_TRUE(closedMesh,"closed deformable mesh");
  const auto *closedSkin=static_cast<const scene::SkinnedMesh*>(g.find(closedMesh)->components.find(scene::SkinnedMesh::descriptor));
  auto closedBone=*g.find(static_cast<editor::EditorEntityId>(closedSkin->bones.front()));closedBone.transform.scale[0]=2;
  AE_EXPECT_TRUE(g.applyEntityValues(closedBone.id,closedBone),"bone stretch authors actual skin pose");
  resources::ConvexBakeSettings skinSettings;skinSettings.maximumParts=4;skinSettings.voxelResolution=10000;skinSettings.timeBudgetSeconds=30;
  const editor::EditorSession::MotorBakeSource closedSources[]{{closedMesh,0}};
  AE_EXPECT_TRUE(s.selectMotorDecompositionSources(closedRoot,closedSources,error)&&s.beginMotorDecomposition(closedRoot,skinSettings,error)&&test::waitConvexBake(s)&&s.applyMotorDecomposition(error),error.c_str());
  runtime::GameWorld skinWorld;runtime::ScenePhysics skinPhysics;test::ConvexMapGeometry skinGeometry(s.mapScene());runtime::QueryHit skinHit;
  const float extended[]{1.5f,3,0},down[]{0,-6,0};
  AE_EXPECT_TRUE(skinWorld.load(g)&&skinPhysics.start(skinWorld,&skinGeometry)&&skinPhysics.rayCast(extended,down,{},skinHit)&&skinHit.object==closedRoot,"actual Jolt shape reaches deformed skin outside rest bounds");
  std::ifstream input(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"tests/native/fixtures/gltf/AnimatedMorphCube.glb",std::ios::binary);
  const auto morphRoot=import(std::vector<u8>(std::istreambuf_iterator<char>(input),{}),"Sources/morph.glb");
  AE_EXPECT_TRUE(morphRoot,"real closed morph cube");
  ids.clear();g.collectSubtree(morphRoot,ids);editor::EditorEntityId morphId=0;
  for(auto id:ids)if(g.find(id)->components.find(scene::SkinnedMesh::descriptor))morphId=id;
  AE_EXPECT_TRUE(morphId,"imported morph deformer");
  auto object=*g.find(morphId);auto *deformer=static_cast<scene::SkinnedMesh*>(object.components.edit(scene::SkinnedMesh::descriptor));
  AE_EXPECT_TRUE(!deformer->blendShapeWeights.empty(),"morph weights available");
  for(auto &weight:deformer->blendShapeWeights)weight=50;
  AE_EXPECT_TRUE(g.applyEntityValues(morphId,object),"authored morph pose");
  const editor::EditorSession::MotorBakeSource sources[]{{morphId,0}};
  AE_EXPECT_TRUE(s.selectMotorDecompositionSources(morphRoot,sources,error),error.c_str());
  resources::ConvexBakeSettings settings;settings.maximumParts=4;settings.voxelResolution=10000;settings.timeBudgetSeconds=30;
  const auto before=editor::serializeEditorDocument(g,0);
  AE_EXPECT_TRUE(s.beginMotorDecomposition(morphRoot,settings,error)&&test::waitConvexBake(s),error.c_str());
  AE_EXPECT_TRUE(editor::serializeEditorDocument(g,0)==before,"CPU pose baking does not write bones or scene");
  AE_EXPECT_TRUE(s.applyMotorDecomposition(error),error.c_str());
  const auto *recipe=scene::collisionRecipe(g.find(morphRoot)->components);
  AE_EXPECT_TRUE(recipe&&recipe->settings.pose==resources::ConvexBakePose::Authored,"pose choice survives recipe publication");
  const auto posedHash=recipe->geometryHash;
  const auto *record=s.assets().find(recipe->bakeSource);
  AE_EXPECT_TRUE(record,"immutable posed GLB registered");
  std::ifstream baked(f.root/record->path,std::ios::binary);std::string bytes(std::istreambuf_iterator<char>(baked),{});
  AE_EXPECT_TRUE(bytes.find("\"pose\":\"authored\"")!=std::string::npos,"actual GLB provenance identifies snapshot policy");
  AE_EXPECT_TRUE(s.beginMotorRegeneration(morphRoot,error)&&test::waitConvexBake(s)&&s.motorDecompositionProgress().stage.find("reutilizada")!=std::string::npos,"identical recipe reuses actual decomposed hulls");
  s.cancelMotorDecomposition();settings.pose=resources::ConvexBakePose::Rest;
  AE_EXPECT_TRUE(s.beginMotorDecomposition(morphRoot,settings,error)&&test::waitConvexBake(s)&&s.motorDecompositionProgress().stage.find("reutilizada")==std::string::npos,"explicit bind/rest selection invalidates cooking cache");
  AE_EXPECT_TRUE(s.confirmMotorDecompositionMapping(error)&&s.applyMotorDecomposition(error),error.c_str());
  AE_EXPECT_TRUE(scene::collisionRecipe(g.find(morphRoot)->components)->geometryHash!=posedHash,"morph geometry really affects derived collision");
  const auto restHash=scene::collisionRecipe(g.find(morphRoot)->components)->geometryHash;
  const auto beforeAnimation=editor::serializeEditorDocument(g,0);
  settings.pose=resources::ConvexBakePose::Animation;settings.animationTime=.35f;
  AE_EXPECT_TRUE(s.beginMotorDecomposition(morphRoot,settings,error)&&test::waitConvexBake(s),error.c_str());
  AE_EXPECT_TRUE(editor::serializeEditorDocument(g,0)==beforeAnimation,"runtime animation sampler operates on isolated authoring copy");
  AE_EXPECT_TRUE(s.confirmMotorDecompositionMapping(error)&&s.applyMotorDecomposition(error),error.c_str());
  AE_EXPECT_TRUE(scene::collisionRecipe(g.find(morphRoot)->components)->geometryHash!=restHash&&scene::collisionRecipe(g.find(morphRoot)->components)->settings.animationTime==.35f,"sampled animation deforms collision and persists exact instant");
  auto device=g;
  AE_EXPECT_TRUE(device.destroyEntity(f.owner)&&device.destroyEntity(rig)&&device.destroyEntity(closedRoot),"isolated pose acceptance scene");
  auto named=*device.find(morphRoot);editor::assignEntityName(named,"PoseBody");
  AE_EXPECT_TRUE(device.applyEntityValues(morphRoot,named),"stable acceptance actor name");
  std::filesystem::create_directories(f.root/"scenes");
  AE_EXPECT_TRUE(editor::EditorImportTransaction::writeText(f.root/"scenes/editor.aescene",editor::serializeEditorDocument(device,0))&&
    editor::EditorImportTransaction::writeText(f.root/"project.json",R"({"format":"ASTRA-PROJECT-1","resourceSource":"independent","project":{"name":"Physical Pose 20261006","template":"empty","scenes":1,"assets":2},"mainScene":"scenes/editor.aescene","editorScene":"scenes/editor.aescene"})"),"editable animation snapshot acceptance project from native serializer");
}
AE_TEST(collision_recipe_solver_rebuild_mass_inertia_and_authored_velocity) {
  RecipeFixture f;AE_EXPECT_TRUE(f.setup(),"physical recipe");auto &s=f.session;auto &g=s.document();
  auto authored=*g.find(f.owner);auto *body=runtime::editPhysicsBody(authored);body->mass=2;body->gravityFactor=0;
  for(auto &locked:body->freezeRotation)locked=false;
  AE_EXPECT_TRUE(g.applyEntityValues(f.owner,authored),"free dynamic rotation");
  runtime::GameWorld world;runtime::ScenePhysics physics;test::ConvexMapGeometry geometry(s.mapScene());
  AE_EXPECT_TRUE(world.load(g)&&physics.start(world,&geometry),physics.error().c_str());
  const auto uid=body->instanceId();AetherBodyStateV1 state;
  AE_EXPECT_TRUE(physics.bodyCommand(world,world.handle(f.owner),uid,6,{3,0,0},{0,0,2},state)==runtime::WorldStatus::Ok,"actual solver velocities");
  auto revised=*world.graph().find(f.owner);runtime::editPhysicsBody(revised)->mass=4;
  for(auto &scale:revised.transform.scale)scale=2;
  AE_EXPECT_TRUE(world.poseGraph().applyEntityValues(f.owner,revised)&&physics.rebuild(world,&geometry),physics.error().c_str());
  AE_EXPECT_TRUE(physics.bodyCommand(world,world.handle(f.owner),uid,0,{},{},state)==runtime::WorldStatus::Ok&&std::abs(state.linear.x-1.5f)<.001f&&std::abs(state.angular.z-.25f)<.001f,"same UID conserves linear/angular momentum through actual mass and shape inertia change");
  revised=*world.graph().find(f.owner);auto *changed=runtime::editPhysicsBody(revised);changed->velocityX=7;changed->angularZ=5;
  AE_EXPECT_TRUE(world.poseGraph().applyEntityValues(f.owner,revised)&&physics.rebuild(world,&geometry)&&physics.bodyCommand(world,world.handle(f.owner),uid,0,{},{},state)==runtime::WorldStatus::Ok&&state.linear.x==7&&state.angular.z==5,"explicit authored velocity edits take precedence independently");
  revised=*world.graph().find(f.owner);changed=runtime::editPhysicsBody(revised);changed->motion=scene::BodyMotion::Kinematic;
  revised.components.remove(scene::DynamicBodyMotor::descriptor);
  AE_EXPECT_TRUE(world.poseGraph().applyEntityValues(f.owner,revised)&&physics.rebuild(world,&geometry),"authority transition does not restore dynamic inertia on kinematic body");
}
AE_TEST(collision_recipe_first_authoring_linked_prefab_atomic_dependencies) {
  RecipeFixture f;AE_EXPECT_TRUE(f.setup(),"fixture");auto &s=f.session;auto &g=s.document();std::string error;
  auto bare=*g.find(f.owner);
  while(bare.components.remove(scene::Collider::descriptor)){}
  bare.components.remove(scene::CollisionRecipe::descriptor);bare.components.remove(scene::DynamicBodyMotor::descriptor);bare.components.remove(scene::PhysicsBody::descriptor);
  AE_EXPECT_TRUE(g.applyEntityValues(f.owner,bare),"unconfigured visual hierarchy");
  const auto asset=s.createPrefab(f.owner,error);AE_EXPECT_TRUE(asset.valid(),error.c_str());
  const auto instance=s.instantiatePrefab(asset,g.root(),error);AE_EXPECT_TRUE(instance,error.c_str());
  const auto unchanged=runtime::serializePrefabObject(*g.find(f.owner));
  std::vector<editor::EditorEntityId> children;g.collectSubtree(instance,children);
  std::vector<editor::EditorSession::MotorBakeSource> sources;
  for(auto id:children)if(runtime::meshRenderer(*g.find(id)))sources.push_back({id,0});
  resources::ConvexBakeSettings settings;settings.maximumParts=4;settings.voxelResolution=10000;settings.timeBudgetSeconds=30;
  AE_EXPECT_TRUE(s.selectMotorDecompositionSources(instance,sources,error)&&s.beginMotorDecomposition(instance,settings,error)&&test::waitConvexBake(s)&&s.applyMotorDecomposition(error),error.c_str());
  AE_EXPECT_TRUE(scene::prefabLink(g.find(instance)->components)&&runtime::serializePrefabObject(*g.find(f.owner))==unchanged,"first-time authoring retains link and source independence");
  const auto applied=editor::serializeEditorDocument(g,0);
  AE_EXPECT_TRUE(s.history().undo(g)&&!scene::collisionRecipe(g.find(instance)->components)&&s.history().redo(g)&&editor::serializeEditorDocument(g,0)==applied,"single transaction on linked instance");
  editor::PrefabOverrideView view;AE_EXPECT_TRUE(s.inspectPrefabOverrides(instance,view,error),error.c_str());
  const auto uid=scene::collisionRecipe(g.find(instance)->components)->instanceId();usize row=view.rows.size();
  for(usize i=0;i<view.rows.size();++i)if(view.rows[i].component==uid)row=i;
  AE_EXPECT_TRUE(row<view.rows.size()&&view.rows[row].applyable,"recipe publication includes newly required Body/Motor, with preflight");
  AE_EXPECT_TRUE(s.applyPrefabOverride(view,row,false,error),error.c_str());
  const auto third=s.instantiatePrefab(asset,g.root(),error);AE_EXPECT_TRUE(third,error.c_str());
  for(auto id:{f.owner,third})AE_EXPECT_TRUE(scene::collisionRecipe(g.find(id)->components)&&runtime::physicsBody(*g.find(id))&&g.find(id)->components.find(scene::DynamicBodyMotor::descriptor),"published initial physical revision is structurally complete");
  runtime::GameWorld world;runtime::ScenePhysics physics;test::ConvexMapGeometry geometry(s.mapScene());
  AE_EXPECT_TRUE(world.load(g)&&physics.start(world,&geometry),physics.error().c_str());
}
AE_TEST(collision_recipe_advanced_settings_pointer_persistence_and_migration) {
  RecipeFixture f;AE_EXPECT_TRUE(f.setup(),"fixture");auto &s=f.session;std::string error;
  s.setSelection(f.owner);s.setSurface({0,0,800,400},{});const auto before=editor::serializeEditorDocument(s.document(),0);
  AE_EXPECT_TRUE(test::tapConvexWidget(s,f.font,editor::EditorWidget::InspectorMenu)&&test::tapConvexWidget(s,f.font,editor::EditorWidget::MotorSetupOpen)&&test::tapConvexWidget(s,f.font,editor::EditorWidget::MotorBakeSourcesOpen),"contextual sources reached by actual pointer");
  s.frameSelection();s.update();const float leftCenter[]{-2,0,0};const auto point=editor::projectWorldToScreen(s.view(),leftCenter);
  AE_EXPECT_TRUE(point.valid,"source projects into viewport");
  const auto selectedCount=s.selectedMotorDecompositionSources().size();
  s.handlePointer({71,ui::UiPointerPhase::Down,point.screen,0});s.handlePointer({71,ui::UiPointerPhase::Up,point.screen,.02});s.update();
  AE_EXPECT_TRUE(s.selection()==f.owner&&s.selectedMotorDecompositionSources().size()==selectedCount-1,"viewport touch changes source slot without retargeting Body");
  s.handlePointer({72,ui::UiPointerPhase::Down,point.screen,0});s.handlePointer({72,ui::UiPointerPhase::Up,point.screen,.02});s.update();
  AE_EXPECT_TRUE(s.selectedMotorDecompositionSources().size()==selectedCount,"second tap restores source");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,f.font,editor::EditorWidget::MotorBakeSourcesDone)&&test::tapConvexWidget(s,f.font,editor::EditorWidget::MotorBakeSettingsOpen),"contextual settings reached by actual pointer");
  capture(s,f.font,f.icons,f.root/"advanced-compact-1.ppm");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,f.font,editor::EditorWidget::MotorBakeSettingsNumberBase),"part budget number reachable");
  auto edit=s.pendingTextEdit();AE_EXPECT_TRUE(!s.completeTextEdit(edit,"2.5",true),"non-integral part count refuses atomically");
  AE_EXPECT_TRUE(s.completeTextEdit(edit,"6",true)&&s.screen().motorBakeDraft.maximumParts==6,"valid numeric expression changes actual draft");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,f.font,editor::EditorWidget::MotorBakeSettingsNext),"advanced pagination");
  capture(s,f.font,f.icons,f.root/"advanced-compact-2.ppm");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,f.font,static_cast<editor::EditorWidget>(editor::widgetId(editor::EditorWidget::MotorBakeSettingsNumberBase)+3)),"volume error reachable");
  AE_EXPECT_TRUE(s.completeTextEdit(s.pendingTextEdit(),"0,75",true),"decimal IME setting");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,f.font,editor::EditorWidget::MotorBakeSettingsNext),"time budget page");
  capture(s,f.font,f.icons,f.root/"advanced-compact-3.ppm");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,f.font,editor::EditorWidget::MotorBakePoseRest)&&test::tapConvexWidget(s,f.font,editor::EditorWidget::MotorBakeSettingsDone),"explicit pose and finish");
  AE_EXPECT_TRUE(editor::serializeEditorDocument(s.document(),0)==before,"draft has no implicit scene/history mutation");
  AE_EXPECT_TRUE(test::tapConvexWidget(s,f.font,editor::EditorWidget::MotorBakeRegenerate)&&test::waitConvexBake(s)&&s.confirmMotorDecompositionMapping(error)&&s.applyMotorDecomposition(error),error.c_str());
  const auto *recipe=scene::collisionRecipe(s.document().find(f.owner)->components);
  AE_EXPECT_TRUE(recipe&&recipe->settings.maximumParts==6&&recipe->settings.volumeErrorPercent==.75f&&recipe->settings.pose==resources::ConvexBakePose::Rest,"UI settings used by bake and saved in actual recipe");
  std::ostringstream out;recipe->write(out);scene::CollisionRecipe restored;std::istringstream in(out.str());
  AE_EXPECT_TRUE(restored.read(in,2)&&restored.settings==recipe->settings,"recipe v2 round trip");
  // A v1 empty recipe never captured deformed geometry; migrate explicitly to Rest.
  std::istringstream legacy("16 100000 32 1 60 - \"\" 0 0");
  AE_EXPECT_TRUE(restored.read(legacy,1)&&restored.settings.pose==resources::ConvexBakePose::Rest,"legacy rest recipes retain interpretation");
  std::ifstream oldArchive(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"docs/validacao/ui-universal-2026-10-06/collision-regeneration/device-before-regeneration.aescene",std::ios::binary);
  const std::string legacyText(std::istreambuf_iterator<char>(oldArchive),{});editor::EditorDocument migrated;
  AE_EXPECT_TRUE(editor::deserializeEditorDocument(legacyText,0,migrated),"actual nonempty v1 device archive migrates without rewriting source");
  std::vector<editor::EditorEntityId> migratedIds;migrated.collectSubtree(migrated.root(),migratedIds);u32 legacyRecipes=0;
  for(auto id:migratedIds)if(const auto *old=scene::collisionRecipe(migrated.find(id)->components)) {
    AE_EXPECT_TRUE(!old->parts.empty()&&old->settings.pose==resources::ConvexBakePose::Rest&&old->settings.animationTime==0,"legacy geometry, part baselines and undeformed policy retained");++legacyRecipes;
  }
  AE_EXPECT_TRUE(legacyRecipes==1,"historical project contains the expected physical recipe");
  s.setSurface({0,0,1500,700},{});s.setSelection(f.owner);
  AE_EXPECT_TRUE(test::tapConvexWidget(s,f.font,editor::EditorWidget::InspectorMenu)&&test::tapConvexWidget(s,f.font,editor::EditorWidget::MotorSetupOpen)&&test::tapConvexWidget(s,f.font,editor::EditorWidget::MotorBakeSettingsOpen),"expanded settings");
  capture(s,f.font,f.icons,f.root/"advanced-expanded.ppm");
}
AE_TEST(collision_recipe_regeneration_history_reopen_and_real_solver) {
  RecipeFixture f;
  AE_EXPECT_TRUE(f.setup(),
                 "real builtin source meshes, worker, publication and recipe");
  auto &s = f.session;
  auto &g = s.document();
  std::string error;
  const auto recipe = *scene::collisionRecipe(g.find(f.owner)->components);
  AE_EXPECT_TRUE(recipe.sources.size() == 2 && recipe.parts.size() == 2 &&
                     recipe.settings.maximumParts == 4,
                 "persistent exact settings and source instances");
  const auto a = recipe.parts[0].collider, b = recipe.parts[1].collider;
  auto authored = *g.find(f.owner);
  auto *part =
      static_cast<scene::Collider *>(authored.components.editInstance(a));
  part->centerX = .3f;
  part->rotationY = 12;
  part->hullTolerance = .005f;
  AE_EXPECT_TRUE(authored.components.removeInstance(b) &&
                     g.applyEntityValues(f.owner, authored),
                 "local pose/cooking edits and local component removal");
  auto source = *g.find(f.left);
  source.transform.position[0] -= .2f;
  AE_EXPECT_TRUE(g.applyEntityValues(f.left, source), "source change");
  const auto before = editor::serializeEditorDocument(g, 0);
  const auto depth = s.history().undoDepth();
  AE_EXPECT_TRUE(f.regenerate(error), error.c_str());
  AE_EXPECT_TRUE(editor::serializeEditorDocument(g, 0) == before &&
                     s.history().undoDepth() == depth,
                 "worker and correspondence do not mutate authoring");
  AE_EXPECT_TRUE(!s.screen().motorBakeMappingConfirmed &&
                     !s.applyMotorDecomposition(error),
                 "spatial proposal needs explicit confirmation");
  AE_EXPECT_TRUE(s.confirmMotorDecompositionMapping(error) &&
                     s.applyMotorDecomposition(error),
                 error.c_str());
  const auto *object = g.find(f.owner);
  const auto *after =
      static_cast<const scene::Collider *>(object->components.findInstance(a));
  AE_EXPECT_TRUE(after && after->centerX == .3f && after->rotationY == 12 &&
                     after->hullTolerance == .005f &&
                     !object->components.findInstance(b),
                 "stable UID, effective fields and tombstone preserved");
  const auto *newRecipe = scene::collisionRecipe(object->components);
  AE_EXPECT_TRUE(newRecipe->parts.size() == 2 &&
                     newRecipe->parts[0].baseline.centerX == 0,
                 "new base is distinct from the local override");
  std::ostringstream oldBody, newBody;
  runtime::physicsBody(authored)->write(oldBody);
  runtime::physicsBody(*object)->write(newBody);
  AE_EXPECT_TRUE(oldBody.str() == newBody.str(),
                 "Body unchanged by geometry authoring");
  test::ConvexMapGeometry geometry(s.mapScene());
  runtime::GameWorld world;
  runtime::ScenePhysics physics;
  runtime::QueryHit hit;
  AE_EXPECT_TRUE(world.load(g) && physics.start(world, &geometry) &&
                     cast(physics, -1.9f, hit) && hit.colliderInstance == a &&
                     !cast(physics, 2, hit),
                 "actual regenerated Jolt shape and preserved deletion");
  physics.stop();
  const auto saved = editor::serializeEditorDocument(g, 0);
  AE_EXPECT_TRUE(s.history().undoDepth() == depth + 1 && s.history().undo(g) &&
                     editor::serializeEditorDocument(g, 0) == before &&
                     s.history().redo(g) &&
                     editor::serializeEditorDocument(g, 0) == saved,
                 "single byte-exact Undo/Redo");
  editor::EditorDocument reopened;
  AE_EXPECT_TRUE(editor::deserializeEditorDocument(saved, 0, reopened),
                 "recipe with typed source identities round-trips");
  editor::EditorSession fresh;
  test::ConvexCpuLibrary library;
  AE_EXPECT_TRUE(
      library.connect(fresh) &&
          fresh.setProjectDirectory(f.root.generic_string().c_str()) &&
          fresh.loadAssets(s.serializeAssets()),
      "fresh asset registry and source library");
  fresh.document() = reopened;
  std::vector<editor::EditorSession::ReopenedSource> savedSources;
  for (const auto &record : s.assets().records())
    if (record.type == resources::AssetType::Mesh) {
      std::vector<u8> bytes;
      resources::GltfImport model;
      AE_EXPECT_TRUE(
          editor::EditorImportTransaction::read(
              f.root / editor::EditorImportTransaction::fromUtf8(record.path),
              bytes) &&
              resources::importGlb(bytes, {}, {}, model),
          "read actual published geometry");
      savedSources.push_back(
          {std::move(model), record.contentHash, record.path});
    }
  std::vector<editor::EditorSession::ModelImportReport> reports;
  AE_EXPECT_TRUE(fresh.reopenSources(savedSources, reports, error),
                 error.c_str());
  fresh.setSelection(f.owner);
  AE_EXPECT_TRUE(fresh.beginMotorRegeneration(f.owner, error) &&
                     test::waitConvexBake(fresh) &&
                     fresh.confirmMotorDecompositionMapping(error) &&
                     fresh.applyMotorDecomposition(error),
                 error.c_str());
  const auto *twice = static_cast<const scene::Collider *>(
      fresh.document().find(f.owner)->components.findInstance(a));
  AE_EXPECT_TRUE(
      twice && twice->centerX == .3f &&
          !fresh.document().find(f.owner)->components.findInstance(b),
      "second regeneration after reopen retains override and tombstone");
}
AE_TEST(collision_recipe_prefab_instance_references_and_independence) {
  RecipeFixture f;
  AE_EXPECT_TRUE(f.setup(), "physical recipe fixture");
  auto &s = f.session;
  auto &g = s.document();
  std::string error;
  const auto asset = s.createPrefab(f.owner, error);
  AE_EXPECT_TRUE(asset.valid(), error.c_str());
  const auto instance = s.instantiatePrefab(asset, g.root(), error);
  AE_EXPECT_TRUE(instance, error.c_str());
  const auto *recipe = scene::collisionRecipe(g.find(instance)->components);
  AE_EXPECT_TRUE(recipe && recipe->sources[0].object != f.left &&
                     recipe->sources[1].object != f.right &&
                     g.isDescendantOf(static_cast<editor::EditorEntityId>(
                                          recipe->sources[0].object),
                                      instance),
                 "source references remapped into the cloned prefab");
  const auto uid = recipe->parts.front().collider;
  auto child = *g.find(
      static_cast<editor::EditorEntityId>(recipe->sources.front().object));
  child.transform.position[0] -= .15f;
  AE_EXPECT_TRUE(g.applyEntityValues(child.id, child), "instance source only");
  auto object = *g.find(instance);
  static_cast<scene::Collider *>(object.components.editInstance(uid))->centerY =
      .2f;
  AE_EXPECT_TRUE(g.applyEntityValues(instance, object),
                 "local prefab collider override");
  const auto untouched = runtime::serializePrefabObject(*g.find(f.owner));
  AE_EXPECT_TRUE(s.beginMotorRegeneration(instance, error) &&
                     test::waitConvexBake(s) &&
                     s.confirmMotorDecompositionMapping(error) &&
                     s.applyMotorDecomposition(error),
                 error.c_str());
  AE_EXPECT_TRUE(runtime::serializePrefabObject(*g.find(f.owner)) ==
                         untouched &&
                     scene::prefabLink(g.find(instance)->components) &&
                     static_cast<const scene::Collider *>(
                         g.find(instance)->components.findInstance(uid))
                             ->centerY == .2f,
                 "regeneration preserves prefab link and isolates shared "
                 "source/other instance");
  editor::PrefabOverrideView view;
  AE_EXPECT_TRUE(s.inspectPrefabOverrides(instance, view, error),
                 error.c_str());
  bool recipeOverride = false;
  for (const auto &row : view.rows)
    recipeOverride |=
        row.component ==
        scene::collisionRecipe(g.find(instance)->components)->instanceId();
  AE_EXPECT_TRUE(recipeOverride,
                 "recipe baseline change is an actual atomic prefab override");
  usize recipeRow = view.rows.size();
  for (usize i = 0; i < view.rows.size(); ++i)
    if (view.rows[i].component ==
        scene::collisionRecipe(g.find(instance)->components)->instanceId())
      recipeRow = i;
  AE_EXPECT_TRUE(
      recipeRow < view.rows.size() && view.rows[recipeRow].applyable &&
          view.rows[recipeRow].applyReason.find("Revisão física") !=
              std::string::npos,
      "editor explicitly discloses recipe and physical parts as one revision");
  AE_EXPECT_TRUE(s.applyPrefabOverride(view, recipeRow, false, error),
                 error.c_str());
  const auto third = s.instantiatePrefab(asset, g.root(), error);
  AE_EXPECT_TRUE(third, error.c_str());
  for (const auto id : {f.owner, third}) {
    const auto *published = scene::collisionRecipe(g.find(id)->components);
    AE_EXPECT_TRUE(
        published && published->geometryHash ==
                         scene::collisionRecipe(g.find(instance)->components)
                             ->geometryHash,
        "new and inherited instances receive the same published recipe");
    for (const auto &p : published->parts) {
      const auto *c = static_cast<const scene::Collider *>(
          g.find(id)->components.findInstance(p.collider));
      AE_EXPECT_TRUE(c && c->collisionMesh == p.baseline.collisionMesh,
                     "recipe publication also transfers concrete physical "
                     "meshes instead of corrupting baseline comparison");
    }
    AE_EXPECT_TRUE(static_cast<const scene::Collider *>(
                       g.find(id)->components.findInstance(uid))
                           ->centerY == .2f,
                   "explicit physical revision carries authored part fields");
  }
}
AE_TEST(collision_recipe_mapping_staleness_missing_source_and_publish_refusal) {
  RecipeFixture f;
  AE_EXPECT_TRUE(f.setup(), "physical recipe fixture");
  auto &s = f.session;
  auto &g = s.document();
  std::string error;
  AE_EXPECT_TRUE(f.regenerate(error), error.c_str());
  const auto a = s.screen().motorBakePartMapping[0],
             b = s.screen().motorBakePartMapping[1];
  AE_EXPECT_TRUE(a && b && a != b &&
                     !s.setMotorDecompositionPartMapping(0, b, error),
                 "bijection rejects duplicate previous component");
  AE_EXPECT_TRUE(s.setMotorDecompositionPartMapping(0, 0, error) &&
                     s.setMotorDecompositionPartMapping(0, a, error) &&
                     s.confirmMotorDecompositionMapping(error),
                 error.c_str());
  const auto before = editor::serializeEditorDocument(g, 0);
  const auto registry = s.serializeAssets();
  f.library.refuse = true;
  // Force a distinct revision so the actual publisher must run.
  auto child = *g.find(f.left);
  child.transform.position[0] -= .1f;
  AE_EXPECT_TRUE(g.applyEntityValues(f.left, child) &&
                     !s.applyMotorDecomposition(error),
                 "stale Apply fails before publishing");
  s.cancelMotorDecomposition();
  const auto changed = editor::serializeEditorDocument(g, 0);
  AE_EXPECT_TRUE(f.regenerate(error) &&
                     s.confirmMotorDecompositionMapping(error) &&
                     !s.applyMotorDecomposition(error),
                 "real publisher refusal rolls back");
  AE_EXPECT_TRUE(editor::serializeEditorDocument(g, 0) == changed &&
                     s.serializeAssets() == registry,
                 "scene, recipe and asset journal remain unchanged on refusal");
  f.library.refuse = false;
  auto missing = *g.find(f.owner);
  auto *r = static_cast<scene::CollisionRecipe *>(
      missing.components.edit(scene::CollisionRecipe::descriptor));
  r->sources[0].object = 0;
  AE_EXPECT_TRUE(g.applyEntityValues(f.owner, missing),
                 "missing authoring source remains a diagnosable draft");
  s.cancelMotorDecomposition();
  AE_EXPECT_TRUE(!s.beginMotorRegeneration(f.owner, error) && !error.empty(),
                 "missing source does not cook visual fallback");
  (void)before;
}
AE_TEST(collision_recipe_real_glb_reimport_and_physical_mesh_override) {
  RecipeFixture f;
  AE_EXPECT_TRUE(f.setup(), "published physical recipe");
  auto &s = f.session;
  auto &g = s.document();
  std::string error;
  const auto uid =
      scene::collisionRecipe(g.find(f.owner)->components)->parts[0].collider;
  const auto cube = s.mapScene().assetGuid(0);
  const auto publishCube = [&](float width, const std::string &expected,
                               editor::EditorSession::ModelImportReport
                                   &report) {
    std::span<const editor::EditorPickMesh::Triangle> triangles;
    float transform[16];
    if (!s.mapScene().localGeometry(s.mapScene().assetSlot(cube), triangles,
                                    transform))
      return false;
    resources::ConvexBakePart part;
    for (const auto &t : triangles)
      for (u32 v = 0; v < 3; ++v) {
        part.indices.push_back(static_cast<u32>(part.vertices.size()));
        part.vertices.push_back({t[v * 3] * width, t[v * 3 + 1], t[v * 3 + 2]});
      }
    std::vector<u8> bytes;
    resources::GltfImport model;
    return resources::writeCollisionTopologyGlb(part, std::string(64, '0'),
                                                bytes, error) &&
           resources::importGlb(bytes, {}, {}, model) &&
           s.commitModelImport(bytes, model, "Sources/reimported-cube.glb",
                               expected, report);
  };
  editor::EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(publishCube(1, {}, report) &&
                     s.instantiateModel(report.source, report, false),
                 report.diagnostic.c_str());
  const auto imported = s.selection();
  const auto asset = runtime::meshRenderer(*g.find(imported))->slotAsset(0);
  auto child = *g.find(f.left);
  auto *render = runtime::editMeshRenderer(child);
  *render->editSlotAsset(0) = asset;
  *render->editSlotMesh(0) = s.mapScene().assetSlot(asset);
  AE_EXPECT_TRUE(
      g.applyEntityValues(f.left, child) && g.destroyEntity(imported),
      "source object keeps identity and uses actual imported GLB mesh");
  AE_EXPECT_TRUE(f.regenerate(error) &&
                     s.confirmMotorDecompositionMapping(error) &&
                     s.applyMotorDecomposition(error),
                 error.c_str());
  auto edited = *g.find(f.owner);
  static_cast<scene::Collider *>(edited.components.editInstance(uid))->centerZ =
      .15f;
  AE_EXPECT_TRUE(g.applyEntityValues(f.owner, edited),
                 "local pose before source reimport");
  const auto before = runtime::serializePrefabObject(*g.find(f.owner));
  const auto oldSource = report.source;
  const auto oldHash = s.assets().find(oldSource)->contentHash;
  AE_EXPECT_TRUE(publishCube(1.4f, oldHash, report) && report.reimported &&
                     report.source == oldSource &&
                     s.mapScene().assetSlot(asset),
                 report.diagnostic.c_str());
  AE_EXPECT_TRUE(runtime::serializePrefabObject(*g.find(f.owner)) == before,
                 "source reimport does not silently replace physics or recipe");
  AE_EXPECT_TRUE(f.regenerate(error) &&
                     s.confirmMotorDecompositionMapping(error) &&
                     s.applyMotorDecomposition(error),
                 error.c_str());
  const auto *collider = static_cast<const scene::Collider *>(
      g.find(f.owner)->components.findInstance(uid));
  const auto &base =
      scene::collisionRecipe(g.find(f.owner)->components)->parts[0];
  AE_EXPECT_TRUE(collider && collider->centerZ == .15f &&
                     base.high[0] - base.low[0] > 1.3f,
                 "new source bounds consumed while local pose and UID survive");
  test::ConvexMapGeometry geometry(s.mapScene());
  runtime::GameWorld world;
  runtime::ScenePhysics physics;
  runtime::QueryHit hit;
  AE_EXPECT_TRUE(world.load(g) && physics.start(world, &geometry) &&
                     cast(physics, -1.35f, hit) && hit.colliderInstance == uid,
                 "actual solver sees enlarged regenerated GLB");
  physics.stop();
  edited = *g.find(f.owner);
  auto *local =
      static_cast<scene::Collider *>(edited.components.editInstance(uid));
  local->collisionMesh = cube;
  local->centerX = -2;
  local->convex = false;
  local->enabled = false;
  AE_EXPECT_TRUE(g.applyEntityValues(f.owner, edited) && f.regenerate(error) &&
                     s.confirmMotorDecompositionMapping(error),
                 error.c_str());
  const auto registry = s.serializeAssets(),
             invalid = editor::serializeEditorDocument(g, 0);
  AE_EXPECT_TRUE(!s.applyMotorDecomposition(error) && !error.empty() &&
                     s.serializeAssets() == registry &&
                     editor::serializeEditorDocument(g, 0) == invalid,
                 "disabled invalid dynamic mesh override cannot conceal an "
                 "unsafe publication");
  s.cancelMotorDecomposition();
  edited = *g.find(f.owner);
  local = static_cast<scene::Collider *>(edited.components.editInstance(uid));
  local->convex = true;
  local->enabled = true;
  AE_EXPECT_TRUE(g.applyEntityValues(f.owner, edited) && f.regenerate(error) &&
                     s.confirmMotorDecompositionMapping(error) &&
                     s.applyMotorDecomposition(error),
                 error.c_str());
  collider = static_cast<const scene::Collider *>(
      g.find(f.owner)->components.findInstance(uid));
  AE_EXPECT_TRUE(
      collider && collider->collisionMesh == cube && collider->centerX == -2 &&
          collider->centerZ == .15f,
      "separate physical mesh resource and its pose remain authored overrides");
  runtime::GameWorld manual;
  AE_EXPECT_TRUE(manual.load(g) && physics.start(manual, &geometry) &&
                     cast(physics, -2, hit) && hit.colliderInstance == uid &&
                     !cast(physics, -1.35f, hit),
                 "solver consumes manual physical mesh after regeneration");
}
AE_TEST(collision_recipe_compact_review_pointer_and_visual_capture) {
  RecipeFixture f;
  AE_EXPECT_TRUE(f.setup(), "executable native UI fixture");
  auto &s = f.session;
  auto &g = s.document();
  std::string error;
  auto child = *g.find(f.left);
  child.transform.position[0] -= .2f;
  AE_EXPECT_TRUE(g.applyEntityValues(f.left, child), "changed source");
  s.setSurface({0, 0, 800, 400}, {});
  s.setSelection(f.owner);
  s.frameAll();
  std::filesystem::create_directories(f.root / "scenes");
  const std::string descriptor =
      R"({"format":"ASTRA-PROJECT-1","resourceSource":"independent","project":{"name":"Collision Recipe U05U09","template":"empty","scenes":1,"assets":2},"mainScene":"scenes/editor.aescene","editorScene":"scenes/editor.aescene"})";
  AE_EXPECT_TRUE(editor::EditorImportTransaction::writeText(
                     f.root / "project.json", descriptor) &&
                     editor::EditorImportTransaction::writeText(
                         f.root / "scenes/editor.aescene",
                         editor::serializeEditorDocument(g, 0)),
                 "isolated editable device acceptance project");
  AE_EXPECT_TRUE(
      test::tapConvexWidget(s, f.font, editor::EditorWidget::InspectorMenu) &&
          test::tapConvexWidget(s, f.font,
                                editor::EditorWidget::MotorSetupOpen),
      "real contextual object route");
  capture(s, f.font, f.icons, f.root / "start-compact.ppm");
  AE_EXPECT_TRUE(test::tapConvexWidget(
                     s, f.font, editor::EditorWidget::MotorBakeRegenerate) &&
                     test::waitConvexBake(s),
                 "real route enters regeneration with persisted custom budget");
  AE_EXPECT_TRUE(s.screen().motorBakeBudget == 3,
                 "custom persisted settings do not falsely select a preset");
  capture(s, f.font, f.icons, f.root / "review-compact.ppm");
  AE_EXPECT_TRUE(
      test::tapConvexWidget(s, f.font,
                            editor::EditorWidget::MotorBakeMappingBase) &&
          test::tapConvexWidget(s, f.font,
                                editor::EditorWidget::MotorBakeConfirmMapping),
      "compact mapping and confirmation are reachable");
  capture(s, f.font, f.icons, f.root / "confirmed-compact.ppm");
  AE_EXPECT_TRUE(
      test::tapConvexWidget(s, f.font, editor::EditorWidget::MotorBakeNext),
      "all candidate parts accessible");
  AE_EXPECT_TRUE(s.screen().motorBakePage == 1,
                 "actual pointer event selects the second candidate page");
  capture(s, f.font, f.icons, f.root / "next-compact.ppm");
  s.setSurface({0, 0, 1500, 700}, {});
  AE_EXPECT_TRUE(test::tapConvexWidget(s, f.font,
                                     editor::EditorWidget::MotorBakePrevious) &&
                     test::tapConvexWidget(s, f.font,
                                          editor::EditorWidget::MotorBakeNext),
                 "full Inspector chrome still exposes every candidate page");
  AE_EXPECT_TRUE(s.screen().motorBakePage == 1,
                 "noncompact review does not hide the second candidate");
  capture(s, f.font, f.icons, f.root / "next-full.ppm");
  std::printf("Collision recipe UI captures: %s\n",
              f.root.generic_string().c_str());
}
