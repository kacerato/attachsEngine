#include "editor/editor_screen.h"
#include "harness.h"
#include "unity_fixture.h"
#include "fbx_fixture.h"
#include "editor/editor_session.h"
#include "resources/model_import.h"
#include "resources/unity_animation_import.h"
#include "resources/unity_controller_import.h"
#include "resources/unity_import_recipe.h"
#include "renderer/authoring_geometry.h"
#include "scene/mesh_renderer.h"
#include "ui_software_raster.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
using namespace ae;
using namespace ae::editor;
using namespace ae::resources;
namespace {
std::string digest(std::string_view value){return Sha256::hex({reinterpret_cast<const u8*>(value.data()),value.size()});}
std::string contents(const std::filesystem::path &path) {std::vector<u8> bytes;EditorImportTransaction::read(path,bytes);return {bytes.begin(),bytes.end()};}
struct UnityProject {
  std::filesystem::path root=std::filesystem::temp_directory_path()/("astra-unity-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::vector<LibrarySourceFile> files;
  UnityProject(){std::filesystem::create_directories(root/".astra/import-staging/unity");}
  ~UnityProject(){std::error_code ec;std::filesystem::remove_all(root,ec);}
  bool add(const std::string &path,const std::string &text) {
    const auto target=root/".astra/import-staging/unity"/path;std::filesystem::create_directories(target.parent_path());
    if(!EditorImportTransaction::writeText(target,text))return false;
    files.push_back({path,digest(text),text.size(),{}});return true;
  }
};
struct PublishedUnityGeometry {
  std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  void attach(EditorSession &session) {
    renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials);
    session.importMap(draws,materials,false,vertices,indices,0);
    session.setGeometryPublisher([this](std::span<const u8> v,std::span<const u32> i,std::span<const renderer::MapDrawRecord> d,
        std::span<const renderer::MapMaterialRecord> m,std::span<const renderer::SharedAuthoringTexture>,EditorSession::PublishedGeometry &out) {
      vertices.clear();indices.clear();draws.clear();materials.clear();
      if(!renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials))return false;
      const auto vb=static_cast<u32>(vertices.size()/renderer::MapVertexStride),ib=static_cast<u32>(indices.size()),mb=static_cast<u32>(materials.size());
      vertices.insert(vertices.end(),v.begin(),v.end());indices.insert(indices.end(),i.begin(),i.end());materials.insert(materials.end(),m.begin(),m.end());
      for(auto draw:d){draw.firstIndex+=ib;draw.vertexOffset+=vb;draw.materialIndex+=mb;draw.lodGroupId=static_cast<u32>(draws.size());draws.push_back(draw);}
      out={draws,materials,vertices,indices};return true;
    });
  }
};
bool tapUnity(EditorSession &session,const ui::UiFont &font,EditorWidget widget) {
  session.update();ui::UiDrawList list;ui::UiInputRouter router;
  list.begin(session.screen().surface,font.metrics(ui::UiFontWeight::Regular));router.beginFrame();buildEditorScreen(session.screen(),ui::defaultTheme(),list,router);
  for(float y=2;y<300;y+=3)for(float x=2;x<655;x+=3) {
    const auto hit=router.hitTest({x,y});if(hit.target!=ui::UiPointerTarget::Widget||hit.widgetId!=widgetId(widget))continue;
    session.handlePointer({21,ui::UiPointerPhase::Down,{x,y},1});session.handlePointer({21,ui::UiPointerPhase::Up,{x,y},1.1});session.update();return true;
  }
  return false;
}
bool captureUnity(EditorSession &session,const ui::UiFont &font,const ui::UiIconAtlas &icons) {
  const auto *path=std::getenv("ASTRA_UNITY_LIBRARY_CAPTURE");if(!path)return true;
  session.update();test::UiSoftwareTarget image;image.resize(655,300,.08f,.09f,.11f);test::rasterizeUi(session.instances(),font,icons,image);
  auto *file=std::fopen(path,"wb");if(!file)return false;std::fprintf(file,"P6\n655 300\n255\n");
  for(usize i=0;i<image.pixels.size();i+=4)for(u32 c=0;c<3;++c)std::fputc(static_cast<u8>(std::clamp(image.pixels[i+c],0.f,1.f)*255+.5f),file);
  return std::fclose(file)==0;
}
}
AE_TEST(library_unity_exact_references_native_prefab_clip_controller_and_transactional_reimport) {
  UnityProject project;EditorSession session;PublishedUnityGeometry publisher;publisher.attach(session);
  std::vector<u8> fontBytes,iconBytes;ui::UiFont font;ui::UiIconAtlas icons;
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-font.aeuf",fontBytes)&&font.load(fontBytes),"production font owned for complete UI lifetime");
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-icons.aeui",iconBytes)&&icons.load(iconBytes),"production icon atlas with owned backing");
  session.initialize(&font,&icons);session.setSurface({0,0,655,300},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()),"open project");
  const std::string meshGuid="11111111111111111111111111111111",matGuid="22222222222222222222222222222222",clipGuid="33333333333333333333333333333333";
  AE_EXPECT_TRUE(project.add("Triangle.fbx",test::FbxTriangle)&&project.add("Triangle.fbx.meta","guid: "+meshGuid+"\nModelImporter:\n  fileIDToRecycleName:\n    4300000: Triangle\n"),"mesh source and declared fileID");
  AE_EXPECT_TRUE(project.add("Paint.mat","%YAML 1.1\n--- !u!21 &2100000\nMaterial:\n  m_Name: Paint\n  m_Shader: {fileID: 46, guid: 0000000000000000f000000000000000, type: 0}\n  m_SavedProperties:\n    m_Floats:\n    - _Metallic: 0.4\n")&&project.add("Paint.mat.meta","guid: "+matGuid+"\n"),"real Standard material source");
  AE_EXPECT_TRUE(project.add("Slide.anim",test::unityClip())&&project.add("Slide.anim.meta","guid: "+clipGuid+"\n"),"generic clip source");
  AE_EXPECT_TRUE(project.add("Triangle.prefab",test::unityPrefab(meshGuid,matGuid))&&project.add("Triangle.prefab.meta","guid: 44444444444444444444444444444444\n"),"prefab has fileID beyond double precision");
  AE_EXPECT_TRUE(project.add("Motion.controller",test::unityController(clipGuid))&&project.add("Motion.controller.meta","guid: 55555555555555555555555555555555\n"),"native-compatible controller source");
  auto package=prepareLibraryPackage(project.root.string(),".astra/import-staging/unity","Unity native family",project.files);std::string error;
  AE_EXPECT_TRUE(package.accepted&&session.importLibraryPackage(package,error),error.c_str());
  const auto item=[&](std::string_view filename)->LibraryItem {for(const auto &entry:package.package.items)if(entry.path.ends_with(filename))return entry;return {};};
  const auto prefabItem=item("Triangle.prefab"),clipItem=item("Slide.anim"),controllerItem=item("Motion.controller"),modelItem=item("Triangle.fbx"),materialItem=item("Paint.mat");
  const auto initialRegistry=session.serializeAssets(),initialIndex=contents(project.root/LibraryIndexPath);const auto initialDepth=session.history().undoDepth();
  AE_EXPECT_TRUE(!session.importUnityAsset(prefabItem.path,0,UnityReimportPolicy::RefuseConflict,error).valid()&&error.find("Converta primeiro")!=error.npos,"missing converted dependency explicitly refused");
  AE_EXPECT_TRUE(session.serializeAssets()==initialRegistry&&contents(project.root/LibraryIndexPath)==initialIndex&&session.history().undoDepth()==initialDepth,"dependency refusal publishes no candidate");
  const std::string meshSource=test::FbxTriangle;std::vector<u8> mesh(meshSource.begin(),meshSource.end());GltfImport converted;ImportProfile profile;
  AE_EXPECT_TRUE(importModelSource(mesh,applyImportProfile(session.importLimits(),profile),{},converted),converted.diagnostic.c_str());
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.commitModelImport(mesh,converted,modelItem.path,digest(test::FbxTriangle),report,ImportAmbiguityPolicy::Refuse,{}, {}, {}, {},&profile),report.diagnostic.c_str());
  const auto material=session.importUnityMaterial(materialItem.path,error);AE_EXPECT_TRUE(material.valid(),error.c_str());
  AE_EXPECT_TRUE(session.openLibraryItem(prefabItem.id,error),error.c_str());
  const auto prefabResolution=session.library().resolve(prefabItem.id,session.assets());
  AE_EXPECT_TRUE(prefabResolution.output&&prefabResolution.state==LibraryResolutionState::Registered,"Library command publishes native prefab");
  const auto prefab=prefabResolution.output->guid;runtime::Prefab resource;
  AE_EXPECT_TRUE(session.loadPrefab(prefab,resource,error),error.c_str());
  AE_EXPECT_TRUE(resource.graph().find(resource.root())->transform.position[2]==-3,"source coordinates reflected into native graph");
  AE_EXPECT_TRUE(captureUnity(session,font,icons),"real library source/provenance/actions at655x300");
  AE_EXPECT_TRUE(tapUnity(session,font,EditorWidget::LibraryInstantiate),"Instantiate touch action creates actual linked scene object");
  const auto owner=session.screen().selection,other=session.instantiatePrefab(prefab,session.document().root(),error);
  AE_EXPECT_TRUE(owner&&other&&owner!=other,"same converted resource instantiates twice through engine");
  const auto *render=meshRenderer(*session.document().find(owner));
  AE_EXPECT_TRUE(render&&render->slotMaterialAsset(0)==material,"prefab uses shared native material and published mesh");
  const auto clip=session.importUnityAsset(clipItem.path,owner,UnityReimportPolicy::RefuseConflict,error);AE_EXPECT_TRUE(clip.valid(),error.c_str());
  AE_EXPECT_TRUE(session.openAnimationClip(clip,owner,error),error.c_str());
  const auto track=session.animationClipAsset(clip)->tracks.front().id,key=session.animationClipAsset(clip)->tracks.front().curves.front().keys.back().id;
  auto revision=session.animationClipAsset(clip)->revision;
  AE_EXPECT_TRUE(session.editAnimationClipAsset(clip,revision,[&](auto &value){value.track(track)->curves.front().keys.back().value=7;return true;},error),error.c_str());
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(project.root/clipItem.path,test::unityClip(4)),"change source endpoint");
  const auto priorRegistry=session.serializeAssets(),priorIndex=contents(project.root/LibraryIndexPath),priorNative=contents(project.root/session.assets().find(clip)->path),priorRecipe=session.assets().find(clip)->importerParameters;const auto priorDepth=session.history().undoDepth();
  AE_EXPECT_TRUE(!session.importUnityAsset(clipItem.path,owner,UnityReimportPolicy::RefuseConflict,error).valid(),"overlapping local/source edit requires explicit policy");
  AE_EXPECT_TRUE(priorRegistry==session.serializeAssets()&&priorIndex==contents(project.root/LibraryIndexPath)&&priorNative==contents(project.root/session.assets().find(clip)->path)&&priorDepth==session.history().undoDepth(),"conflict keeps native/recipe/index/history together");
  AE_EXPECT_TRUE(session.importUnityAsset(clipItem.path,owner,UnityReimportPolicy::KeepLocal,error)==clip,error.c_str());
  AE_EXPECT_TRUE(session.animationClipAsset(clip)->track(track)->curves.front().keys.back().value==7&&session.animationClipAsset(clip)->track(track)->curves.front().keys.back().id==key,"local curve override and key identity preserved");
  AE_EXPECT_TRUE(session.history().undo(session.document())&&session.assets().find(clip)->importerParameters==priorRecipe,"Undo restores prior source recipe with native content");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"Redo replays source recipe and native output together");
  AE_EXPECT_TRUE(session.importUnityAsset(clipItem.path,owner,UnityReimportPolicy::RefuseConflict,error)==clip,"unchanged source after override has no false conflict");
  revision=session.animationClipAsset(clip)->revision;
  AE_EXPECT_TRUE(session.editAnimationClipAsset(clip,revision,[&](auto &value){value.track(track)->curves.front().keys.back().value=8;return true;},error),error.c_str());
  AE_EXPECT_TRUE(session.history().undo(session.document())&&session.animationClipAsset(clip)->track(track)->curves.front().keys.back().value==7,"Undo authoring restores content with monotonic revision");
  AE_EXPECT_TRUE(session.history().undo(session.document())&&session.history().redo(session.document()),"Unity import Undo/Redo remains valid after an authoring Undo changed revision/frontier");
  AE_EXPECT_TRUE(session.openLibraryItem(controllerItem.id,error),error.c_str());
  const auto controller=session.library().resolve(controllerItem.id,session.assets());
  AE_EXPECT_TRUE(controller.output&&controller.output->type==AssetType::AnimatorController,"controller converted by Library action");
  const auto controllerId=controller.output->guid;const auto controllerPath=controller.output->path;
  AnimatorControllerAsset graph;AE_EXPECT_TRUE(AnimatorControllerAsset::deserialize(contents(project.root/controllerPath),graph),"native graph resource");
  AE_EXPECT_EQ(graph.graph.layers.front().states.front().motions.front().clip,clip,"controller motion references authored clip GUID");
  AE_EXPECT_TRUE(graph.graph.layers.front().states.front().loop&&graph.graph.layers.front().weight==1,"loop metadata and effective base layer weight");
  const auto stateId=graph.graph.layers.front().states.front().id;
  AE_EXPECT_TRUE(session.importUnityAsset(controllerItem.path,owner,UnityReimportPolicy::RefuseConflict,error)==controllerId,error.c_str());
  AE_EXPECT_TRUE(AnimatorControllerAsset::deserialize(contents(project.root/controllerPath),graph)&&graph.graph.layers.front().states.front().id==stateId,"controller reimport keeps exact fileID identity");
  EditorActionRequest add;add.version=session.sceneVersion();add.action=EditorAction::AddComponent;add.entity=owner;add.componentType=scene::Animator::descriptor.id;
  AE_EXPECT_EQ(session.dispatch(add).status,EditorActionStatus::Applied,"Animator created by the same component command used by engine UI");
  session.setSelection(owner);AE_EXPECT_TRUE(session.openLibraryItem(controllerItem.id,error),error.c_str());
  AE_EXPECT_TRUE(tapUnity(session,font,EditorWidget::LibraryControllerAssign),"Library bind command assigns actual Animator instance");
  const auto archive=(project.root/"editor.aescene").string();AE_EXPECT_TRUE(session.save(archive.c_str(),0),"linked instances and authored Animator saved");
  const auto finalRegistry=session.serializeAssets();EditorSession reopened;PublishedUnityGeometry reopenedPublisher;reopenedPublisher.attach(reopened);
  AE_EXPECT_TRUE(reopened.setProjectDirectory(project.root.string().c_str())&&reopened.loadAssets(finalRegistry),"cold registry/index/resource load");
  std::vector<EditorSession::ReopenedSource> sources(1);sources.front().sourceName=modelItem.path;sources.front().hash=digest(meshSource);
  AE_EXPECT_TRUE(importModelSource(mesh,reopened.importLimitsFor(profile),{},sources.front().model),"cold source decoded by real importer");
  std::vector<EditorSession::ModelImportReport> reopenedReports;
  AE_EXPECT_TRUE(reopened.reopenSources(sources,reopenedReports,error)&&reopened.load(archive.c_str(),0),error.c_str());
  AE_EXPECT_TRUE(reopened.animationClipAsset(clip)&&reopened.loadPrefab(prefab,resource,error),"converted resources usable after reopening");
  AE_EXPECT_TRUE(reopened.library().resolve(controllerItem.id,reopened.assets()).state==LibraryResolutionState::Registered,"cold controller provenance resolves");
  AE_EXPECT_TRUE(reopened.startPlay(),"native converted controller starts in cold Play");
  std::vector<renderer::MapDrawState> live;reopened.advanceClock(10);AE_EXPECT_TRUE(reopened.extractPlayMap(live),"Play initialized");
  reopened.advanceClock(10.25);AE_EXPECT_TRUE(reopened.extractPlayMap(live),"first bounded frame");
  reopened.advanceClock(10.5);AE_EXPECT_TRUE(reopened.extractPlayMap(live),"second bounded frame");
  AE_EXPECT_TRUE(std::abs(reopened.playScene().document().find(owner)->transform.position[0]-3.5f)<.03f&&
      reopened.playScene().document().find(other)->transform.position[0]==1,"controller consumes converted curve at0.5s while second resource instance stays independent");
  const auto beforeFailure=session.serializeAssets(),beforeFailureIndex=contents(project.root/LibraryIndexPath);const auto failureDepth=session.history().undoDepth();
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(project.root/controllerItem.path,test::unityController("ffffffffffffffffffffffffffffffff")),"missing source dependency revision");
  AE_EXPECT_TRUE(!session.importUnityAsset(controllerItem.path,owner,UnityReimportPolicy::ReceiveSource,error).valid(),"explicit policy cannot authorize missing dependency");
  AE_EXPECT_TRUE(beforeFailure==session.serializeAssets()&&beforeFailureIndex==contents(project.root/LibraryIndexPath)&&failureDepth==session.history().undoDepth(),"dependency failure keeps all published resources and history");
}
