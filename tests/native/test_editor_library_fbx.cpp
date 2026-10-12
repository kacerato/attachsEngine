#include "editor/editor_screen.h"
#include "harness.h"
#include "fbx_fixture.h"
#include "editor/editor_session.h"
#include "resources/model_import.h"
#include "resources/import_cache.h"
#include "renderer/authoring_geometry.h"
#include "scene/import_link.h"
#include "ui_software_raster.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
using namespace ae;
using namespace ae::editor;
namespace {
struct FbxProject {
  std::filesystem::path root=std::filesystem::temp_directory_path()/("astra-library-fbx-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  FbxProject(){std::filesystem::create_directories(root);}
  ~FbxProject(){std::error_code error;std::filesystem::remove_all(root,error);}
};
struct FbxPublisher {
  bool failNext=false;
  std::vector<u8> vertices;std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  usize importedIndices=0;
  usize influenceBytes=0,morphFloats=0;
  void attach(EditorSession &session) {
    renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials);
    session.importMap(draws,materials,false,vertices,indices,0);
    session.setGeometryPublisher([this,&session](std::span<const u8> v,std::span<const u32> i,std::span<const renderer::MapDrawRecord> d,
        std::span<const renderer::MapMaterialRecord> m,std::span<const renderer::SharedAuthoringTexture>,EditorSession::PublishedGeometry &out) {
      if(std::exchange(failNext,false))return false;
      influenceBytes=session.skinningPublication().influences.size();morphFloats=session.skinningPublication().morphDeltas.size();
      importedIndices=i.size();vertices.clear();indices.clear();draws.clear();materials.clear();
      if(!renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials))return false;
      const auto vb=static_cast<u32>(vertices.size()/renderer::MapVertexStride),ib=static_cast<u32>(indices.size()),mb=static_cast<u32>(materials.size());
      vertices.insert(vertices.end(),v.begin(),v.end());indices.insert(indices.end(),i.begin(),i.end());materials.insert(materials.end(),m.begin(),m.end());
      for(auto draw:d){draw.firstIndex+=ib;draw.vertexOffset+=vb;draw.materialIndex+=mb;draw.lodGroupId=static_cast<u32>(draws.size());draws.push_back(draw);}
      out={draws,materials,vertices,indices};return true;
    });
  }
};
std::vector<u8> encoded(const std::string &value){return {value.begin(),value.end()};}
std::string diskText(const std::filesystem::path &path) {
  std::vector<u8> bytes;EditorImportTransaction::read(path,bytes);return {bytes.begin(),bytes.end()};
}
bool tap(EditorSession &session,const ui::UiFont &font,u32 widget) {
  session.update();ui::UiDrawList list;ui::UiInputRouter router;
  list.begin(session.screen().surface,font.metrics(ui::UiFontWeight::Regular));router.beginFrame();buildEditorScreen(session.screen(),ui::defaultTheme(),list,router);
  ui::UiPoint point{-1,-1};
  for(float y=2;y<300&&point.x<0;y+=3)for(float x=2;x<655;x+=3) {
    const auto hit=router.hitTest({x,y});if(hit.target==ui::UiPointerTarget::Widget&&hit.widgetId==widget){point={x,y};break;}
  }
  if(point.x<0)return false;
  session.handlePointer({0,ui::UiPointerPhase::Down,point,1});session.handlePointer({0,ui::UiPointerPhase::Up,point,1.1});session.update();return true;
}
void capture(EditorSession &session,const ui::UiFont &font,const ui::UiIconAtlas &icons,const char *suffix="") {
  if(const auto *path=std::getenv("ASTRA_FBX_CAPTURE")) {
    session.update();test::UiSoftwareTarget target;target.resize(655,300,.08f,.09f,.11f);test::rasterizeUi(session.instances(),font,icons,target);
    const auto output=std::string(path)+suffix;auto *file=std::fopen(output.c_str(),"wb");AE_EXPECT_TRUE(file,"FBX capture output");std::fprintf(file,"P6\n655 300\n255\n");
    for(usize i=0;i<target.pixels.size();i+=4)for(u32 c=0;c<3;++c)std::fputc(static_cast<u8>(std::clamp(target.pixels[i+c],0.f,1.f)*255+.5f),file);
    AE_EXPECT_TRUE(std::fclose(file)==0,"FBX UI captured");
  }
}
}

AE_TEST(library_fbx_static_authoring_reimport_compound_history_failure_and_cold_play) {
  FbxProject project;EditorSession session;FbxPublisher publisher;publisher.attach(session);
  std::vector<u8> fontBytes,iconBytes;ui::UiFont font;ui::UiIconAtlas icons;
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-font.aeuf",fontBytes)&&font.load(fontBytes),"owned production font");
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-icons.aeui",iconBytes)&&icons.load(iconBytes),"owned production atlas");
  session.initialize(&font,&icons);session.setSurface({0,0,655,300},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()),"project opened before staging");
  const auto original=encoded(test::FbxTriangle);const auto hash=Sha256::hex(original);
  const std::string staging=".astra/import-staging/fbx";std::filesystem::create_directories(project.root/staging);
  AE_EXPECT_TRUE(EditorImportTransaction::write(project.root/staging/"Triangle.FBX",original),"original FBX staged");
  auto package=prepareLibraryPackage(project.root.string(),staging,"Owned static FBX",{{"Triangle.FBX",hash,original.size(),{}}});
  std::string error;AE_EXPECT_TRUE(package.accepted&&session.importLibraryPackage(package,error),error.c_str());
  const auto item=package.package.items.front();
  AE_EXPECT_TRUE(tap(session,font,widgetId(EditorWidget::LibraryItemBase)),"source entry opens through real touch router");
  AE_EXPECT_EQ(session.takeReimportPath(),item.path,"native model worker receives original FBX path");
  resources::ImportProfile profile;resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importModelSource(original,resources::applyImportProfile(session.importLimits(),profile),{},model),model.diagnostic.c_str());
  session.beginImportPreparation(item.path);session.showImportPreview(item.path,model,hash,profile);
  AE_EXPECT_TRUE(session.screen().importReady&&session.screen().workspace==EditorWorkspace::Library,"focal importer replaces catalog");
  capture(session,font,icons);AE_EXPECT_TRUE(!test::currentTestFailed(),"capture accepted");
  AE_EXPECT_TRUE(tap(session,font,widgetId(EditorWidget::ImportTabProfile))&&tap(session,font,widgetId(EditorWidget::ImportScaleUp)),"profile and actual scale editor reachable by touch");
  AE_EXPECT_TRUE(session.importProfileDraft().scale!=profile.scale,"scale control changes the worker recipe");
  AE_EXPECT_TRUE(tap(session,font,widgetId(EditorWidget::ImportScaleDown)),"restore scale with engine control");
  bool normals=false;
  // A folha lateral pagina o perfil; avança até a linha de normais existir.
  for(u32 page=0;page<10&&!normals;++page){normals=tap(session,font,widgetId(EditorWidget::ImportNormalsCycle));if(!normals)tap(session,font,widgetId(EditorWidget::ImportNextPage));}
  AE_EXPECT_TRUE(normals,"normal policy changed through engine UI");
  profile=session.importProfileDraft();AE_EXPECT_EQ(profile.normals,resources::GltfNormalsCalculate,"actual recipe carries authored normals policy");
  capture(session,font,icons,".profile.ppm");AE_EXPECT_TRUE(!test::currentTestFailed(),"profile capture accepted");
  AE_EXPECT_TRUE(tap(session,font,widgetId(EditorWidget::ImportApplyProfile))&&session.takeImportReprepare(),"UI requests real worker preparation; cannot publish old derived geometry");
  AE_EXPECT_TRUE(resources::importModelSource(original,resources::applyImportProfile(session.importLimits(),profile),{},model),model.diagnostic.c_str());
  session.showImportPreview(item.path,model,hash,profile);

  EditorSession::ModelImportReport report;
  const auto refusedDepth=session.history().undoDepth();
  const auto refusedIndex=diskText(project.root/LibraryIndexPath);
  AE_EXPECT_TRUE(!session.commitModelImport(original,model,item.path,hash,report,resources::ImportAmbiguityPolicy::Refuse,{}, {},std::string(64,'0'), {},&profile)&&
      !report.diagnostic.empty()&&!session.assets().findByPath(item.path)&&session.history().undoDepth()==refusedDepth&&
      diskText(project.root/LibraryIndexPath)==refusedIndex&&diskText(project.root/item.path)==test::FbxTriangle,
      "aggregate hash cannot publish a single-source FBX or change history/catalog/source");
  AE_EXPECT_TRUE(session.commitModelImport(original,model,item.path,hash,report,resources::ImportAmbiguityPolicy::Refuse,{}, {}, {}, {},&profile),report.diagnostic.c_str());
  const auto source=report.source;
  AE_EXPECT_TRUE(session.library().resolve(item.id,session.assets()).state==resources::LibraryResolutionState::Registered&&publisher.importedIndices==3,"real geometry and native catalog published");
  AE_EXPECT_TRUE(session.history().retainedResourceBytes()>0&&session.history().retainedResourceBytes()<=EditorHistory::kMaximumRetainedResourceBytes,"real affected IR retention is bounded");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"undo first native conversion");
  AE_EXPECT_TRUE(!session.assets().find(source)&&publisher.importedIndices==0&&diskText(project.root/item.path)==std::string(original.begin(),original.end()),"source preserved; GPU library and native identity detached");
  AE_EXPECT_TRUE(!std::filesystem::exists(project.root/resources::importNodeMapPath(source))&&!std::filesystem::exists(project.root/resources::importProfilePath(source)),"absent before metadata restored as absence");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"redo first conversion");
  session.closeImportPreview();AE_EXPECT_TRUE(session.instantiateModel(source,report),report.diagnostic.c_str());
  std::vector<EditorEntityId> ids;session.document().collectSubtree(session.document().root(),ids);EditorEntityId triangle=0;
  for(const auto id:ids)if(std::string(session.document().find(id)->name)=="Triangle")triangle=id;
  AE_EXPECT_TRUE(triangle!=0,"real editable source node instantiated");
  auto local=session.document().find(triangle)->transform;local.position[0]=17;
  AE_EXPECT_TRUE(session.history().setTransform(session.document(),triangle,local),"local transform override authored through history");
  auto changed=std::string(test::FbxTriangle);const auto position=changed.find('A',changed.find("Lcl Translation",changed.find("Model:")));
  AE_EXPECT_TRUE(position!=changed.npos,"owned fixture translation location");
  const auto value=changed.find("2,3,4",position);AE_EXPECT_TRUE(value!=changed.npos,"owned fixture translation value");changed.replace(value,5,"5,6,7");
  const auto changedBytes=encoded(changed);const auto changedHash=Sha256::hex(changedBytes);resources::GltfImport candidate;
  auto changedProfile=profile;changedProfile.normals=resources::GltfNormalsImport;
  AE_EXPECT_TRUE(resources::importModelSource(changedBytes,resources::applyImportProfile(session.importLimits(),changedProfile),{},candidate),candidate.diagnostic.c_str());
  const auto beforeRegistry=session.serializeAssets(),beforeMap=diskText(project.root/resources::importNodeMapPath(source)),beforeProfile=diskText(project.root/resources::importProfilePath(source)),beforeIndex=diskText(project.root/LibraryIndexPath);
  const auto undoDepth=session.history().undoDepth();
  AE_EXPECT_TRUE(session.commitModelImport(changedBytes,candidate,item.path,hash,report,resources::ImportAmbiguityPolicy::Refuse,{}, {}, {}, {},&changedProfile),report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.document().find(triangle)->transform.position[0]==17&&session.document().find(triangle)->transform.position[1]==6,"local override preserved while source Y updated");
  AE_EXPECT_EQ(session.history().undoDepth(),undoDepth+1,"source and reconciled instances are one history step");
  const auto afterMap=diskText(project.root/resources::importNodeMapPath(source));
  const auto revision=session.document().revision();const auto frontier=session.document().nextObjectId();
  AE_EXPECT_TRUE(session.history().undo(session.document()),"compound reimport undo");
  AE_EXPECT_TRUE(session.serializeAssets()==beforeRegistry&&diskText(project.root/resources::importNodeMapPath(source))==beforeMap&&diskText(project.root/resources::importProfilePath(source))==beforeProfile&&diskText(project.root/LibraryIndexPath)==beforeIndex,"source registry, node map, recipe and index restored together");
  AE_EXPECT_TRUE(session.document().find(triangle)->transform.position[0]==17&&session.document().find(triangle)->transform.position[1]==3&&session.document().revision()>=revision&&session.document().nextObjectId()>=frontier,"scene restored; allocator/revision monotonic");
  AE_EXPECT_TRUE(session.history().redo(session.document())&&diskText(project.root/resources::importNodeMapPath(source))==afterMap,"redo source and instances together");
  const auto stableRegistry=session.serializeAssets(),stableMap=diskText(project.root/resources::importNodeMapPath(source)),stableProfile=diskText(project.root/resources::importProfilePath(source)),stableIndex=diskText(project.root/LibraryIndexPath);
  const auto stableDepth=session.history().undoDepth();publisher.failNext=true;
  AE_EXPECT_TRUE(!session.commitModelImport(original,model,item.path,changedHash,report,resources::ImportAmbiguityPolicy::Refuse,{}, {}, {}, {},&profile),"renderer refusal rolls back import");
  AE_EXPECT_TRUE(session.serializeAssets()==stableRegistry&&diskText(project.root/resources::importNodeMapPath(source))==stableMap&&diskText(project.root/resources::importProfilePath(source))==stableProfile&&diskText(project.root/LibraryIndexPath)==stableIndex&&diskText(project.root/item.path)==changed,"refused candidate cannot escape into any authoring file");
  AE_EXPECT_TRUE(session.history().undoDepth()==stableDepth&&publisher.importedIndices==3&&session.document().find(triangle)->transform.position[1]==6,"instances, renderer and history cursor retained after failure");
  publisher.failNext=true;
  AE_EXPECT_TRUE(!session.history().undo(session.document())&&session.history().undoDepth()==stableDepth&&session.document().find(triangle)->transform.position[1]==6,"failed compound replay restores document after publisher rollback");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"undo after failed replay is still available");
  AE_EXPECT_TRUE(session.commitModelImport(changedBytes,candidate,item.path,hash,report,resources::ImportAmbiguityPolicy::Refuse,{}, {}, {}, {},&changedProfile),report.diagnostic.c_str());
  const auto archive=(project.root/"editor.aescene").string();AE_EXPECT_TRUE(session.save(archive.c_str(),0),"authored scene saved");
  EditorSession cold;FbxPublisher coldPublisher;coldPublisher.attach(cold);cold.initialize(&font,&icons);
  AE_EXPECT_TRUE(cold.setProjectDirectory(project.root.string().c_str())&&cold.loadAssets(session.serializeAssets()),"cold package and native registry loaded");
  std::vector<EditorSession::ReopenedSource> reopened(1);reopened[0].sourceName=item.path;reopened[0].hash=changedHash;
  AE_EXPECT_TRUE(resources::importModelSource(changedBytes,resources::applyImportProfile(cold.importLimits(),cold.importProfileFor(source)),{},reopened[0].model),"cold FBX decoded with persisted recipe");
  std::vector<EditorSession::ModelImportReport> reports;
  AE_EXPECT_TRUE(cold.reopenSources(reopened,reports,error)&&cold.load(archive.c_str(),0),error.c_str());
  AE_EXPECT_TRUE(cold.document().find(triangle)&&cold.document().find(triangle)->transform.position[0]==17&&cold.startPlay(),"cold authored FBX instance enters real Play");
  std::vector<renderer::MapDrawState> playDraws;cold.advanceClock(.016f);
  AE_EXPECT_TRUE(cold.extractPlayMap(playDraws)&&!playDraws.empty(),"runtime extracts actual imported static draw");
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(project.root/resources::importProfilePath(source),"external recipe"),"external metadata changed");
  AE_EXPECT_TRUE(!session.history().undo(session.document())&&session.history().undoDepth()>0&&diskText(project.root/resources::importProfilePath(source))=="external recipe","Undo preserves external metadata without changing history cursor");
}

AE_TEST(library_fbx_rig_morph_takes_meshless_editor_extraction_history_and_runtime) {
  FbxProject project;EditorSession session;FbxPublisher publisher;publisher.attach(session);
  std::vector<u8> fontBytes,iconBytes;ui::UiFont font;ui::UiIconAtlas icons;
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-font.aeuf",fontBytes)&&font.load(fontBytes),"owned font");
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-icons.aeui",iconBytes)&&icons.load(iconBytes),"owned atlas");
  session.initialize(&font,&icons);session.setSurface({0,0,655,300},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()),"project ready");
  const auto bytes=encoded(test::fbxDeformingCharacter());const auto hash=Sha256::hex(bytes);
  resources::ImportProfile profile;profile.generateLods=false;profile.optimizePolygonOrder=false;
  resources::ImportProfile parsed;const auto recipe=resources::serializeImportProfile(profile);
  AE_EXPECT_TRUE(resources::parseImportProfile(recipe,parsed)&&resources::sameImportProfile(parsed,profile),"profile12 round-trip carries actual deformation policy");
  auto legacy=recipe;const auto schema=legacy.find("\"schema\":12");AE_EXPECT_TRUE(schema!=legacy.npos,"profile12 marker");legacy.replace(schema,11,"\"schema\":11");
  AE_EXPECT_TRUE(resources::parseImportProfile(legacy,parsed)&&!parsed.fbxDeformation,"legacy recipe retains strict static contract");
  auto alternate=profile;alternate.fbxBakeRate=60;
  AE_EXPECT_TRUE(resources::importCacheKey(hash,session.importLimitsFor(profile))!=resources::importCacheKey(hash,session.importLimitsFor(alternate)),"effective bake rate changes derived cache identity");
  resources::GltfImport model;AE_EXPECT_TRUE(resources::importModelSource(bytes,session.importLimitsFor(profile),{},model),model.diagnostic.c_str());
  AE_EXPECT_TRUE(model.skins.size()==1&&model.morphs.size()==1&&model.animations.size()==1&&model.draws.size()==1,"one complete FBX family in existing native IR");
  AE_EXPECT_TRUE(model.skins[0].joints.size()==1&&model.morphs[0].targetCount==1&&model.morphs[0].vertexCount>=3,"real bind matrices and shape offsets");
  const auto morphChannel=std::find_if(model.animations[0].channels.begin(),model.animations[0].channels.end(),[](const auto &channel){return channel.path==resources::AnimationPath::Weights;});
  float weight[4]{};AE_EXPECT_TRUE(morphChannel!=model.animations[0].channels.end()&&resources::sampleAnimationChannel(*morphChannel,.5f,weight)&&std::abs(weight[0]-.5f)<.02f,"source DeformPercent sampled into native morph channel");
  const std::string staging=".astra/import-staging/rig";std::filesystem::create_directories(project.root/staging);
  AE_EXPECT_TRUE(EditorImportTransaction::write(project.root/staging/"Character.fbx",bytes),"owned original staged");
  auto package=prepareLibraryPackage(project.root.string(),staging,"Owned deforming FBX",{{"Character.fbx",hash,bytes.size(),{}}});std::string error;
  AE_EXPECT_TRUE(package.accepted&&session.importLibraryPackage(package,error),error.c_str());const auto item=package.package.items.front();
  session.beginImportPreparation(item.path);session.showImportPreview(item.path,model,hash,profile);
  AE_EXPECT_TRUE(tap(session,font,widgetId(EditorWidget::ImportTabRig)),"rig/takes destination reachable through touch");
  capture(session,font,icons,".rig.ppm");
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.commitModelImport(bytes,model,item.path,hash,report,resources::ImportAmbiguityPolicy::Refuse,{}, {}, {}, {},&profile),report.diagnostic.c_str());const auto source=report.source;
  AE_EXPECT_TRUE(publisher.influenceBytes>0&&publisher.morphFloats>0&&session.history().retainedResourceBytes()>0,"deformation publication and retained history use real buffers");
  AE_EXPECT_TRUE(session.history().undo(session.document())&&!session.assets().find(source)&&publisher.influenceBytes==0&&publisher.morphFloats==0,"first import Undo detaches all deformation resources");
  AE_EXPECT_TRUE(session.history().redo(session.document())&&session.assets().find(source),"first import Redo restores skins/morph/take");
  session.closeImportPreview();AE_EXPECT_TRUE(session.instantiateModel(source,report),report.diagnostic.c_str());
  EditorEntityId owner=0,body=0,joint=0;std::vector<EditorEntityId> ids;session.document().collectSubtree(session.document().root(),ids);
  for(const auto id:ids) {
    const auto *object=session.document().find(id);const auto *link=scene::importLink(object->components);
    if(link&&link->source==source&&link->root)owner=id;
    if(std::string_view(object->name)=="Body")body=id;
    if(std::string_view(object->name)=="Joint")joint=id;
  }
  AE_EXPECT_TRUE(owner&&body&&joint,"editable imported hierarchy and Animation/SkinnedMesh created");
  session.setSelection(owner);AE_EXPECT_TRUE(session.openLibraryItem(item.id,error),error.c_str());
  AE_EXPECT_TRUE(session.screen().importPublishedView&&session.screen().importTakeSource==source,"registered file opens its actual take subresources");
  capture(session,font,icons,".take.ppm");
  AE_EXPECT_TRUE(tap(session,font,widgetId(EditorWidget::ImportTakeExtract)),"extract/edit is usable through real touch route");
  AE_EXPECT_TRUE(session.screen().clipOpen&&session.screen().clipAsset&&!session.screen().importPanel,"validated extraction opens N01 authoring and isolated preview");
  const auto extracted=session.screen().clipGuid;AE_EXPECT_TRUE(session.animationClipAsset(extracted)&&session.seekAnimationClip(.5f,error),error.c_str());
  capture(session,font,icons,".editable.ppm");session.closeAnimationClip();
  auto edited=*session.document().find(body);auto *skin=static_cast<scene::SkinnedMesh*>(edited.components.edit(scene::SkinnedMesh::descriptor));
  AE_EXPECT_TRUE(skin&&skin->blendShapeWeights.size()==1,"actual deformable component");skin->blendShapeWeights[0]=75;
  AE_EXPECT_TRUE(session.history().applyValues(session.document(),body,edited),"local morph override authored");
  auto jointPose=session.document().find(joint)->transform;jointPose.position[0]=17;
  AE_EXPECT_TRUE(session.history().setTransform(session.document(),joint,jointPose),"local joint X authored");
  const std::string oldJoint="Model: 300, \"Model::Joint\", \"LimbNode\" { Version: 232 }";
  const std::string newJoint="Model: 300, \"Model::Joint\", \"LimbNode\" { Version: 232 Properties70: { P: \"Lcl Translation\", \"Lcl Translation\", \"\", \"A\",0,3,0 } }";
  auto revision=test::fbxDeformingCharacter();const auto jointAt=revision.find(oldJoint);AE_EXPECT_TRUE(jointAt!=revision.npos,"owned joint rest transform location");revision.replace(jointAt,oldJoint.size(),newJoint);
  const auto defaultAt=revision.find("DeformPercent: 0");AE_EXPECT_TRUE(defaultAt!=revision.npos,"owned morph default location");revision.replace(defaultAt,16,"DeformPercent: 20");
  const auto revised=encoded(revision);const auto revisedHash=Sha256::hex(revised);resources::GltfImport revisedModel;
  AE_EXPECT_TRUE(resources::importModelSource(revised,session.importLimitsFor(profile),{},revisedModel),revisedModel.diagnostic.c_str());
  const auto oldRecipe=diskText(project.root/resources::importProfilePath(source)),oldMap=diskText(project.root/resources::importNodeMapPath(source));
  const auto reimportDepth=session.history().undoDepth();
  AE_EXPECT_TRUE(session.commitModelImport(revised,revisedModel,item.path,hash,report,resources::ImportAmbiguityPolicy::Refuse,{}, {}, {}, {},&profile),report.diagnostic.c_str());
  const auto *preserved=static_cast<const scene::SkinnedMesh*>(session.document().find(body)->components.find(scene::SkinnedMesh::descriptor));
  AE_EXPECT_TRUE(preserved&&preserved->blendShapeWeights[0]==75&&session.document().find(joint)->transform.position[0]==17&&session.document().find(joint)->transform.position[1]==3,"reimport preserves local morph/joint X while inheriting source Y");
  AE_EXPECT_TRUE(session.history().undoDepth()==reimportDepth+1&&session.history().undo(session.document())&&diskText(project.root/resources::importNodeMapPath(source))==oldMap&&diskText(project.root/resources::importProfilePath(source))==oldRecipe&&session.document().find(joint)->transform.position[1]==0,"deformation reimport and scene reconciliation Undo together");
  AE_EXPECT_TRUE(session.history().redo(session.document())&&session.document().find(joint)->transform.position[1]==3,"deformation reimport Redo restores new baseline");
  const auto beforeRegistry=session.serializeAssets(),beforeIndex=diskText(project.root/LibraryIndexPath);const auto depth=session.history().undoDepth();
  publisher.failNext=true;
  AE_EXPECT_TRUE(!session.commitModelImport(bytes,model,item.path,revisedHash,report,resources::ImportAmbiguityPolicy::Refuse,{}, {}, {}, {},&profile)&&session.serializeAssets()==beforeRegistry&&diskText(project.root/LibraryIndexPath)==beforeIndex&&session.history().undoDepth()==depth,"failed deforming publication retains sources, registry, index and history");
  const auto archive=(project.root/"editor.aescene").string();AE_EXPECT_TRUE(session.save(archive.c_str(),0),"scene saved");
  EditorSession cold;FbxPublisher coldPublisher;coldPublisher.attach(cold);cold.initialize(&font,&icons);
  AE_EXPECT_TRUE(cold.setProjectDirectory(project.root.string().c_str())&&cold.loadAssets(session.serializeAssets()),"cold registry loaded");
  std::vector<EditorSession::ReopenedSource> reopened(1);reopened[0].sourceName=item.path;reopened[0].hash=revisedHash;
  AE_EXPECT_TRUE(resources::importModelSource(revised,cold.importLimitsFor(cold.importProfileFor(source)),{},reopened[0].model),"cold native FBX recipe consumed");
  std::vector<EditorSession::ModelImportReport> reports;
  AE_EXPECT_TRUE(cold.reopenSources(reopened,reports,error)&&cold.load(archive.c_str(),0)&&cold.startPlay(),error.c_str());
  std::vector<renderer::MapDrawState> draws;cold.advanceClock(10);AE_EXPECT_TRUE(cold.extractPlayMap(draws),"actual Play scheduler initialized");
  // Play deliberately caps a single resumed frame at 250 ms. Advance two
  // normal frames to inspect the take at 0.5 s rather than bypassing that
  // lifecycle safeguard with one 500 ms clock jump.
  cold.advanceClock(10.25);AE_EXPECT_TRUE(cold.extractPlayMap(draws),"first bounded Play animation step");
  cold.advanceClock(10.5);AE_EXPECT_TRUE(cold.extractPlayMap(draws),"actual Play animation sampled at half a second");
  const auto posed=std::find_if(draws.begin(),draws.end(),[&](const auto &draw){return draw.objectId==body;});
  AE_EXPECT_TRUE(posed!=draws.end()&&posed->skinPalette&&posed->morphWeights&&std::abs((*posed->skinPalette)[12]-1)<.03f&&std::abs((*posed->morphWeights)[0]-.5f)<.03f,"source take reaches renderer joint palette and morph weights after cold reopen");
  auto animationSource=test::fbxDeformingCharacter(true);animationSource.replace(animationSource.find(oldJoint),oldJoint.size(),newJoint);
  const auto meshless=encoded(animationSource);resources::GltfImport animationOnly;
  AE_EXPECT_TRUE(resources::importModelSource(meshless,session.importLimitsFor(profile),{},animationOnly)&&animationOnly.draws.empty()&&animationOnly.vertices.empty()&&!animationOnly.animations.empty(),animationOnly.diagnostic.c_str());
  const auto key=resources::importCacheKey(Sha256::hex(meshless),session.importLimitsFor(profile));std::vector<u8> cache;resources::GltfImport cached;
  AE_EXPECT_TRUE(resources::writeImportCache(animationOnly,key,cache)&&resources::readImportCache(cache,key,cached)&&cached.draws.empty()&&cached.animations.size()==animationOnly.animations.size(),"meshless cache round-trip preserves hierarchy/takes without dummy mesh");
  const std::string libraryPath="Fontes/Animation.fbx";std::filesystem::create_directories(project.root/"Fontes");
  AE_EXPECT_TRUE(EditorImportTransaction::write(project.root/libraryPath,meshless),"meshless original preserved");
  AE_EXPECT_TRUE(session.commitModelImport(meshless,animationOnly,libraryPath,Sha256::hex(meshless),report,resources::ImportAmbiguityPolicy::Refuse,{}, {}, {}, {},&profile),report.diagnostic.c_str());
  const auto meshlessSource=report.source;resources::AssetGuid meshlessClip;
  AE_EXPECT_TRUE(session.extractModelTake(meshlessSource,0,0,owner,meshlessClip,error)&&session.animationClipAsset(meshlessClip),error.c_str());
  AE_EXPECT_TRUE(session.openAnimationClip(meshlessClip,owner,error)&&session.seekAnimationClip(.5f,error),"meshless take binds compatible scene rig and opens real authoring preview");
}
