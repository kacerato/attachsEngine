#include "harness.h"
#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_animation_clip_preview.h"
#include "editor/editor_animation_authoring.h"
#include "editor/editor_theme.h"
#include "resources/animation_clip_asset.h"
#include "resources/animation_binding_path.h"
#include "scene/skinned_mesh.h"
#include "runtime/transform_math.h"
#include "core/sha256.h"
#include "renderer/authoring_geometry.h"
#include "skinned_glb_fixture.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cmath>
#include "ui_software_raster.h"
#include <cstdlib>
#include <thread>
#include <cstring>
using namespace ae;
namespace {
resources::AnimationClipAsset fixture(std::string &error) {
  using namespace resources;AnimationClip clip;clip.name="Mecanismo editável";clip.duration=2;
  AnimationChannel p;p.node=1;p.times={0,2};p.values={0,0,0,4,0,0};
  AnimationChannel q;q.node=1;q.path=AnimationPath::Rotation;q.times={0,2};q.values={0,0,0,1,0,.8660254f,0,.5f};
  AnimationChannel w;w.node=1;w.path=AnimationPath::Weights;w.weightCount=2;w.times={0,2};w.values={.1f,.2f,.9f,.6f};
  clip.channels={p,q,w};const AnimationClipBinding bindings[]{{0,{},"Esquerda/Junta","Junta"},{0,{},"Direita/Junta","Junta"}};
  AnimationClipAsset result;authorAnimationClip(clip,assetGuidFromSeed("authored-test"),{},{},{},bindings,result,error);return result;
}
struct TemporaryProject {
  std::filesystem::path path;
  TemporaryProject() {
    path=std::filesystem::absolute("build")/("animation-clip-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;std::filesystem::create_directories(path,ec);
  }
  ~TemporaryProject() {std::error_code ec;std::filesystem::remove_all(path,ec);}
};
std::vector<u8> productionUiBytes(const char *path) {
  std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};
}
ui::UiPoint clipActionPosition(editor::EditorSession &session,const ui::UiFont &font,u32 code) {
  using namespace editor;using namespace ui;
  UiDrawList list;list.begin(session.screen().surface,font.metrics(UiFontWeight::Regular));
  UiInputRouter router;router.beginFrame();buildEditorScreen(session.screen(),editorTheme(),list,router);
  UiPoint point{-1,-1};
  for(float y=2;y<394&&point.x<0;y+=3)for(float x=2;x<853;x+=3) {
    const auto hit=router.hitTest({x,y});if(hit.target==UiPointerTarget::Widget&&hit.widgetId==clip_widget::id(code)) {point={x,y};break;}
  }
  return point;
}
void tapClipAction(editor::EditorSession &session,const ui::UiFont &font,u32 code) {
  using namespace ui;const auto point=clipActionPosition(session,font,code);
  AE_EXPECT_TRUE(point.x>=0,"clip action is drawn and reachable through the production router");
  session.handlePointer({0,UiPointerPhase::Down,point,1});session.handlePointer({0,UiPointerPhase::Up,point,1.1});session.update();
}
void captureClipSurface(editor::EditorSession &session,const ui::UiFont &font,const ui::UiIconAtlas &icons,const char *name) {
  if(const auto *directory=std::getenv("AE_CLIP_CAPTURE_DIR")) {
    std::fprintf(stderr,"CAPTURE %s icons=%ux%u bytes=%zu pointer=%p font=%zu instances=%zu\n",name,icons.width(),icons.height(),icons.pixels().size(),static_cast<const void*>(icons.pixels().data()),font.atlasPixels().size(),session.instances().size());
    AE_EXPECT_EQ(icons.pixels().size(),usize(icons.width())*icons.height()*4,"capture atlas has whole RGBA storage");
    for(const auto &instance:session.instances())if(static_cast<u32>(instance.params[2])==static_cast<u32>(ui::UiInstanceKind::Icon)) {
      AE_EXPECT_TRUE(std::isfinite(instance.bounds[2])&&std::isfinite(instance.bounds[3])&&instance.bounds[2]>0&&instance.bounds[3]>0,"capture icon dimensions must be nondegenerate");
      for(float v:instance.atlas)AE_EXPECT_TRUE(std::isfinite(v),"capture icon UV must be finite");
    }
    const std::filesystem::path output(directory);std::error_code ec;std::filesystem::create_directories(output,ec);
    const auto width=static_cast<u32>(session.screen().surface.width),height=static_cast<u32>(session.screen().surface.height);
    test::UiSoftwareTarget target;target.resize(width,height,.08f,.09f,.11f);test::rasterizeUi(session.instances(),font,icons,target);
    std::ofstream file(output/name,std::ios::binary);file<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(usize i=0;i<target.pixels.size();i+=4)for(u32 c=0;c<3;++c)file.put(static_cast<char>(std::clamp(target.pixels[i+c]*255.f,0.f,255.f)));
  }
}
}
namespace {
// Private package fixtures are opt-in and are never copied into the repository
// or required by CI. When supplied, this is a real engine import/sampler/skin
// acceptance case, not a Blender-only assertion.
void animation_character_package_native_import_sampling_and_deformation() {
  using namespace resources;
  const auto directory=std::filesystem::path(std::getenv("AE_ANIMATION_CHARACTER_LIBRARY"));
  struct Library final:runtime::AnimationLibrary {
    runtime::SourceAnimations source;
    bool findClip(const AssetGuid &guid,runtime::AnimationClipView &out)const override {
      for(usize i=0;i<source.clipIds.size();++i)if(source.clipIds[i]==guid) {out={&source.clips[i],&source,source.clips[i].name};return true;}
      return false;
    }
  };
  for(const auto *name:{"RobotKyle","Viking","VikingWalk","VikingRun"}) {
    const auto path=directory/(std::string(name)+".glb");std::vector<u8> bytes;
    AE_EXPECT_TRUE(editor::EditorImportTransaction::read(path,bytes),"generated package GLB is present");
    const auto hash=Sha256::hex(bytes);GltfImport model;
    AE_EXPECT_TRUE(importGlb(bytes,{}, {},model),model.diagnostic.c_str());
    AE_EXPECT_TRUE(model.skins.size()==1&&!model.textures.empty()&&model.skippedSkins==0&&model.skippedTextures==0&&model.skippedAnimations==0&&model.unsupportedAnimationChannels==0,"native import preserves skin, textures and every supported animation channel");
    const bool robot=std::string_view(name)=="RobotKyle";
    AE_EXPECT_EQ(model.skins.front().joints.size(),robot?49u:22u,"all package joints survive the native importer");
    AE_EXPECT_EQ(model.animations.size(),robot?0u:1u,"expected source clip count");
    runtime::SceneGraph graph;std::vector<runtime::ObjectId> objects;Library library;
    library.source.source=assetGuidFromSeed(name);library.source.clips=model.animations;
    const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    for(const auto &node:model.nodes) {
      const auto id=graph.createEntity(graph.root(),runtime::ObjectKind::Folder,node.name);runtime::Transform pose;
      AE_EXPECT_TRUE(id&&runtime::localTransformForWorld(node.localMatrix,identity,pose)&&graph.setTransform(id,pose),"native node TRS is representable without loss");objects.push_back(id);
    }
    for(usize i=0;i<model.nodes.size();++i) {
      const auto &node=model.nodes[i];if(node.parent>=0)AE_EXPECT_TRUE(graph.reparent(objects[i],objects[node.parent],0),"retain imported hierarchy");
      std::string path=animationBindingSegment(node.name);
      for(auto parent=node.parent;parent>=0;parent=model.nodes[parent].parent)path=animationBindingSegment(model.nodes[parent].name)+"/"+path;
      library.source.nodes.push_back({});library.source.nodeNames.push_back(node.name);library.source.nodePaths.push_back(path);
    }
    for(usize i=0;i<model.animations.size();++i)library.source.clipIds.push_back(assetGuidFromSeed(std::string(name)+":"+std::to_string(i)));
    std::vector<runtime::ObjectId> targets;runtime::resolveAnimationTargets(graph,graph.root(),library.source,targets);
    for(const auto &clip:model.animations)for(const auto &channel:clip.channels)AE_EXPECT_TRUE(channel.node<targets.size()&&targets[channel.node]==objects[channel.node],"every animated node resolves to the actual imported hierarchy");
    runtime::SceneAnimator animator;animator.begin(graph,library);std::vector<std::vector<float>> initial(model.draws.size());bool changed=false;u32 poses=0,skinnedDraws=0;
    for(u32 frame=0;frame<=(robot?0u:60u);++frame) {
      if(!robot) {runtime::SceneAnimator::ExternalSample sample;sample.owner=graph.root();sample.clip=library.source.clipIds[0];sample.time=model.animations[0].duration*frame/60;sample.weight=1;
        animator.setExternalSamples({sample});AE_EXPECT_TRUE(animator.advance(0,{})&&animator.compositionDiagnostic().empty(),"real runtime sampler evaluates the package clip");}
      for(usize d=0;d<model.draws.size();++d) {
        if(model.drawSkins[d]<0)continue;
        const auto &draw=model.draws[d];const auto &skin=model.skins[model.drawSkins[d]];
        float drawWorld[16];AE_EXPECT_TRUE(runtime::worldMatrix(graph,objects[model.drawNodes[d]],drawWorld),"mesh world transform");
        std::vector<float> worlds(skin.joints.size()*16),palette;
        for(usize j=0;j<skin.joints.size();++j)AE_EXPECT_TRUE(runtime::worldMatrix(graph,objects[skin.joints[j]],worlds.data()+j*16),"joint world transform");
        AE_EXPECT_TRUE(computeSkinPalette(skin,drawWorld,worlds,palette),"actual bind matrices form a usable palette");
        std::vector<float> positions;std::vector<u8> influences;
        for(u32 i=0;i<draw.indexCount;++i) {
          const usize vertex=model.indices[draw.firstIndex+i]+draw.vertexOffset;float xyz[3];std::memcpy(xyz,model.vertices.data()+vertex*renderer::MapVertexStride,sizeof xyz);
          positions.insert(positions.end(),xyz,xyz+3);influences.insert(influences.end(),model.skinInfluences.begin()+vertex*SkinInfluenceStride,model.skinInfluences.begin()+(vertex+1)*SkinInfluenceStride);
        }
        AE_EXPECT_TRUE(deformPositions(positions,influences,palette,4,nullptr,{}),"production CPU skin deforms source geometry");
        for(float v:positions)AE_EXPECT_TRUE(std::isfinite(v),"deformation stays finite over the whole clip");
        if(!frame) {initial[d]=positions;++skinnedDraws;}
        else for(usize i=0;i<positions.size()&&i<initial[d].size();++i)if(std::abs(positions[i]-initial[d][i])>1e-5)changed=true;
        ++poses;
      }
    }
    AE_EXPECT_TRUE(skinnedDraws&& (robot||changed),"animated character visibly changes geometry; static Kyle remains a real skin");
    std::vector<u8> after;AE_EXPECT_TRUE(editor::EditorImportTransaction::read(path,after)&&Sha256::hex(after)==hash,"engine acceptance does not rewrite the supplied GLB");
    std::printf("CHARACTER %s joints=%zu clips=%zu textures=%zu deformed_samples=%u source_preserved=true\n",name,model.skins[0].joints.size(),model.animations.size(),model.textures.size(),poses);
  }
}
struct RegisterCharacterPackageAcceptance {
  RegisterCharacterPackageAcceptance() {if(std::getenv("AE_ANIMATION_CHARACTER_LIBRARY"))test::registry().push_back({"animation_character_package_native_import_sampling_and_deformation",animation_character_package_native_import_sampling_and_deformation});}
} registerCharacterPackageAcceptance;
}
AE_TEST(animation_clip_authoring_api_uses_real_resources_without_opening_a_panel) {
  using namespace editor;using namespace resources;
  TemporaryProject project;EditorSession session;std::string error;
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"authoring project");
  const auto owner=session.document().createEntity(session.document().root(),runtime::ObjectKind::Folder,"Rotor");
  const auto child=session.document().createEntity(owner,runtime::ObjectKind::Folder,"A/B");
  AnimationAuthoringScope scope(session);const auto &api=scope.access();auto *context=api.context;
  AssetGuid guid;AE_EXPECT_TRUE(api.create(context,owner,2,0,&guid)==1,"API creates a registered clip with no panel");
  AE_EXPECT_TRUE(!session.screen().clipOpen&&api.count(context,0)==1,"catalog does not depend on the clip window");
  const auto original=*session.animationClipAsset(guid);const auto track=original.tracks.front().id;
  const auto draft=api.begin(context,guid,original.revision),stale=api.begin(context,guid,original.revision);
  AE_EXPECT_TRUE(draft&&stale&&draft!=stale,"independent revision-bound drafts");
  const auto before=original.serialize();AnimationAuthorCommand command;
  command.operation=static_cast<u32>(AnimationAuthorOperation::PutPose);command.track=track;command.first=.5;
  const float pose[]{3,0,0};
  AE_EXPECT_TRUE(api.apply(context,draft,&command,pose,3,nullptr,0,nullptr)==1,"same native pose operation through the editor ABI");
  const std::string path=animationBindingSegment("A/B"),name="A/B";u64 childTrack=0;
  AE_EXPECT_TRUE(api.addTrack(context,draft,{},reinterpret_cast<const u8*>(path.data()),static_cast<int>(path.size()),
      reinterpret_cast<const u8*>(name.data()),static_cast<int>(name.size()),0,0,pose,3,&childTrack)==1,"typed portable binding adds an actual child channel");
  const int bytes=api.snapshot(context,draft,nullptr,0);std::string text(static_cast<usize>(bytes),'\0');
  AE_EXPECT_TRUE(bytes>0&&api.snapshot(context,draft,reinterpret_cast<u8*>(text.data()),bytes)==bytes,"versioned complete snapshot");
  AnimationClipAsset snapshot;AE_EXPECT_TRUE(AnimationClipAsset::deserialize(text,snapshot,&error),error.c_str());
  const auto key=snapshot.track(track)->curves[0].keys[1].id;
  const AnimationAuthorAddress selected[]{ {track,0,0,key} };
  const auto clipboard=api.copy(context,draft,selected,1);
  AE_EXPECT_TRUE(clipboard&&api.paste(context,draft,clipboard,.25f,0)==1,"owned typed clipboard pastes into the draft");
  const auto depth=session.history().undoDepth();
  AE_EXPECT_TRUE(session.animationClipAsset(guid)->serialize()==before&&session.history().undoDepth()==depth,"drafts do not leak writes to resource/runtime/history");
  float sampled[3]{};double value=0,derivative=0;
  AE_EXPECT_TRUE(api.sample(context,draft,track,.25f,sampled,3)==3&&sampled[0]==3,"draft samples the runtime channel");
  AE_EXPECT_TRUE(api.sampleCurve(context,draft,track,0,.25,&value,&derivative)==1&&value==3,"curve evaluation uses shared native math");
  int foreign=1;std::thread wrongThread([&]{foreign=api.count(context,0);});wrongThread.join();
  AE_EXPECT_TRUE(foreign==-1,"authoring refuses access from another thread");
  {AnimationAuthoringScope other(session);const auto &isolated=other.access();
    AE_EXPECT_TRUE(isolated.commit(isolated.context,draft)==0,"tokens cannot escape their command invocation");}
  AE_EXPECT_TRUE(api.commit(context,draft)==1&&session.history().undoDepth()==depth+1,"one commit journals all draft edits as one history operation");
  AE_EXPECT_TRUE(api.commit(context,stale)==0&&api.cancel(context,stale)==1,"stale draft cannot overwrite the committed revision");
  AE_EXPECT_TRUE(api.snapshot(context,draft,nullptr,0)==-1,"committed draft is retired");
  runtime::SceneAnimator animator;animator.begin(session.document(),session.mapScene());
  runtime::SceneAnimator::ExternalSample sample;sample.owner=owner;sample.clip=guid;sample.time=.25f;sample.weight=1;
  animator.setExternalSamples({sample});AE_EXPECT_TRUE(animator.advance(0,{}),"published resource reaches the real compositor");
  AE_EXPECT_TRUE(session.document().find(owner)->transform.position[0]==3&&session.document().find(child)->transform.position[0]==3,"API-authored root and escaped child binding both drive transforms");
  animator.reset();
  AE_EXPECT_TRUE(session.history().undo(session.document())&&session.animationClipAsset(guid)->tracks.size()==original.tracks.size(),"Undo restores the authored topology");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"Redo restores the API commit");
  EditorSession reopened;AE_EXPECT_TRUE(reopened.setProjectDirectory(project.path.string().c_str())&&reopened.loadAssets(session.serializeAssets()),"API publication cold reopens");
  AE_EXPECT_TRUE(reopened.animationClipAsset(guid)->serialize()==session.animationClipAsset(guid)->serialize(),"every authored field survives persistence");
  // Reusing a pathname after undo must not revive old GUID/revision handles.
  AssetGuid first,recreated;AE_EXPECT_TRUE(session.createAnimationClip(owner,1,first,error),error.c_str());
  const auto firstPath=session.assets().find(first)->path;
  AE_EXPECT_TRUE(session.history().undo(session.document()),"remove newly created clip by Undo");
  AE_EXPECT_TRUE(session.createAnimationClip(owner,1,recreated,error)&&first!=recreated&&session.assets().find(recreated)->path==firstPath,"recreated resource has a fresh identity at the same path");
  AE_EXPECT_TRUE(api.begin(context,first,1)==0,"old resource GUID never resolves to its replacement");
}
AE_TEST(animation_editor_tools_use_contextual_menu_and_reject_stale_catalogs) {
  using namespace editor;using namespace ui;
  TemporaryProject project;EditorSession session;UiFont font;UiIconAtlas icons;
  const auto fontBytes=productionUiBytes("assets/astra-visual/ui/astra-ui-font.aeuf"),iconBytes=productionUiBytes("assets/astra-visual/ui/astra-ui-icons.aeui");
  AE_EXPECT_TRUE(font.load(fontBytes)&&icons.load(iconBytes),"production editor identity");
  session.initialize(&font,&icons);session.setSurface({0,0,853,394},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"tool project");
  {std::ofstream source(project.path/"Tools.cs");source<<"// Ferramentas de autoria\n";}
  AE_EXPECT_TRUE(session.openCodeFile("Tools.cs"),"real IDE navigation");session.setCodeCompilerAvailable(true);session.update();
  const auto tap=[&](EditorWidget widget) {
    UiDrawList list;list.begin(session.screen().surface,font.metrics(UiFontWeight::Regular));UiInputRouter router;router.beginFrame();
    buildEditorScreen(session.screen(),editorTheme(),list,router);UiPoint point{-1,-1};
    for(float y=2;y<session.screen().surface.height&&point.x<0;y+=3)for(float x=2;x<session.screen().surface.width;x+=3) {
      const auto hit=router.hitTest({x,y});if(hit.target==UiPointerTarget::Widget&&hit.widgetId==widgetId(widget)) {point={x,y};break;}
    }
    AE_EXPECT_TRUE(point.x>=0,"editor tool action is reachable through the production router");
    session.handlePointer({0,UiPointerPhase::Down,point,1});session.handlePointer({0,UiPointerPhase::Up,point,1.1});session.update();
  };
  tap(EditorWidget::CodeMenu);tap(EditorWidget::CodeTools);
  const auto catalog=session.takeEditorToolRequest();
  AE_EXPECT_TRUE(catalog.root==session.codeProjectRoot()&&catalog.id.empty()&&session.screen().codeToolsBusy,"menu requests only the canonical applied project catalog");
  session.completeEditorToolRequest(catalog,true,"ASTRA_EDITOR_COMMANDS 1 2 \"Tools::Rotor\" \"Autorar mecanismo\" \"Tools::Lifetime\" \"Vida do comando\"");session.update();
  AE_EXPECT_TRUE(session.screen().codeToolCatalog.size()==2&&!session.screen().codeToolsBusy,"versioned catalog populates the real surface");
  captureClipSurface(session,font,icons,"editor-tools-menu.ppm");
  session.setSurface({0,0,655,300},{});session.update();captureClipSurface(session,font,icons,"editor-tools-menu-small.ppm");
  tap(EditorWidget::CodeToolBase);const auto run=session.takeEditorToolRequest();
  AE_EXPECT_TRUE(run.id=="Tools::Rotor"&&run.epoch==session.sceneVersion().epoch&&!session.screen().codeMenu,"selection queues the stable tool identity in this scene context");
  session.completeEditorToolRequest(run,true,"Ferramenta concluída");
  AE_EXPECT_TRUE(!session.screen().codeToolsBusy,"execution report releases the UI");
  AE_EXPECT_TRUE(session.requestEditorTools(),"refresh");const auto invalid=session.takeEditorToolRequest();
  session.completeEditorToolRequest(invalid,true,"ASTRA_EDITOR_COMMANDS 1 2 \"same\" \"A\" \"same\" \"B\"");
  AE_EXPECT_TRUE(session.screen().codeToolCatalog.empty()&&!session.requestEditorTools("same"),"duplicate or stale tools never become executable");
}
AE_TEST(animation_clip_editor_publication_history_conflict_and_cold_reopen) {
  TemporaryProject project;editor::EditorSession session;std::string error;
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"open isolated project");
  auto value=fixture(error);const std::string path="Clipes/Mecanismo.aeclip";
  AE_EXPECT_TRUE(session.createAnimationClipAsset(value,path,error),error.c_str());
  AE_EXPECT_TRUE(session.assets().find(value.guid)&&session.animationClipAsset(value.guid),"registered resource and authoring data");
  runtime::AnimationClipView view;
  AE_EXPECT_TRUE(session.mapScene().findClip(value.guid,view)&&view.clip->channels.size()==3,"same resource available to runtime consumer");
  const auto track=value.tracks[0].id;u64 inserted=0;
  AE_EXPECT_TRUE(session.editAnimationClipAsset(value.guid,1,[&](auto &c){
    auto *t=c.track(track);t->sourceOverride=true;return resources::splitAnimationCurve(t->curves[0],.5f,c.nextId,inserted);
  },error),error.c_str());
  const auto after=session.animationClipAsset(value.guid)->serialize();const auto frontier=session.animationClipAsset(value.guid)->nextId;
  AE_EXPECT_EQ(session.history().undoDepth(),2u,"creation and edit are two resource transactions");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"undo republishes resource and registry");
  AE_EXPECT_EQ(session.animationClipAsset(value.guid)->revision,3u,"history revisions are monotonic");
  AE_EXPECT_EQ(session.animationClipAsset(value.guid)->nextId,frontier,"undo never recycles IDs");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"redo republishes");
  auto expected=*session.animationClipAsset(value.guid);expected.revision=2;
  AE_EXPECT_EQ(expected.serialize(),after,"redo restores every authoring field");
  const auto registry=session.serializeAssets();editor::EditorSession reopened;
  AE_EXPECT_TRUE(reopened.setProjectDirectory(project.path.string().c_str())&&reopened.loadAssets(registry),"cold reopen from registered disk resource");
  AE_EXPECT_TRUE(reopened.animationClipAsset(value.guid)&&reopened.mapScene().findClip(value.guid,view),"resource available after cold reopen");
  AE_EXPECT_EQ(reopened.animationClipAsset(value.guid)->nextId,frontier,"allocator frontier persists");
  const auto prior=session.animationClipAsset(value.guid)->serialize();const auto registryBefore=session.serializeAssets();
  const auto file=project.path/path;std::ofstream(file,std::ios::app)<<" external";
  AE_EXPECT_TRUE(!session.editAnimationClipAsset(value.guid,4,[](auto &c){c.name="Não deve publicar";return true;},error),"external file conflicts block edits");
  AE_EXPECT_EQ(session.animationClipAsset(value.guid)->serialize(),prior,"memory preserved on publication conflict");
  AE_EXPECT_EQ(session.serializeAssets(),registryBefore,"registry preserved on publication conflict");
  AE_EXPECT_TRUE(!session.history().undo(session.document()),"history also refuses conflict");
  AE_EXPECT_EQ(session.history().undoDepth(),2u,"failed undo does not consume cursor");
}
AE_TEST(animation_clip_creation_history_is_atomic_monotonic_and_checks_resource_users) {
  TemporaryProject project;editor::EditorSession session;std::string error;
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"project");
  const auto owner=session.document().createEntity(session.document().root(),runtime::ObjectKind::Folder,"Rotor");
  resources::AssetGuid guid;AE_EXPECT_TRUE(session.createAnimationClip(owner,2,guid,error),error.c_str());
  const auto record=*session.assets().find(guid);const auto before=session.animationClipAsset(guid)->serialize();
  AE_EXPECT_TRUE(session.history().undo(session.document())&&!session.animationClipAsset(guid)&&!session.assets().find(guid)&&!std::filesystem::exists(project.path/record.path),"undo creation removes file, registry and runtime library together");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"redo creation restores resource");
  auto restored=*session.animationClipAsset(guid);AE_EXPECT_EQ(restored.revision,2u,"redo never reuses the old publication revision");restored.revision=1;
  AE_EXPECT_EQ(restored.serialize(),before,"redo restores bindings, key IDs and all data");
  resources::AssetRecord dependent;dependent.guid=resources::assetGuidFromSeed("clip-user");dependent.type=resources::AssetType::AnimatorController;
  dependent.path="Teste.controller";dependent.contentHash=std::string(64,'a');dependent.dependencies={guid};
  auto registry=session.assets();AE_EXPECT_TRUE(registry.add(dependent)&&session.loadAssets(registry.serialize()),"registered dependency");
  AE_EXPECT_TRUE(!session.history().undo(session.document())&&session.history().undoDepth()==1&&session.animationClipAsset(guid),"referenced resource refuses creation undo without consuming history");
  registry.remove(dependent.guid);AE_EXPECT_TRUE(session.loadAssets(registry.serialize()),"reload resolved dependency registry");
  AE_EXPECT_TRUE(session.history().undo(session.document()),session.screen().status.c_str());
  std::ofstream(project.path/record.path)<<"external";
  AE_EXPECT_TRUE(!session.history().redo(session.document())&&session.history().redoDepth()==1,"redo never overwrites an unrelated file at the old path");
}
AE_TEST(animation_clip_editor_runtime_exact_paths_quaternion_and_morph_consumer) {
  TemporaryProject project;editor::EditorSession session;std::string error;
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"project");auto asset=fixture(error);
  AE_EXPECT_TRUE(session.createAnimationClipAsset(asset,"Clipes/Teste.aeclip",error),error.c_str());
  auto &graph=session.document();const auto owner=graph.createEntity(graph.root(),runtime::ObjectKind::Folder,"Mecanismo");
  const auto left=graph.createEntity(owner,runtime::ObjectKind::Folder,"Esquerda"),right=graph.createEntity(owner,runtime::ObjectKind::Folder,"Direita");
  const auto a=graph.createEntity(left,runtime::ObjectKind::Folder,"Junta"),b=graph.createEntity(right,runtime::ObjectKind::Folder,"Junta");
  auto *skin=static_cast<scene::SkinnedMesh*>(graph.editComponents(b)->add(scene::SkinnedMesh::descriptor));skin->blendShapeWeights={0,0};
  runtime::AnimationClipView clip;AE_EXPECT_TRUE(session.mapScene().findClip(asset.guid,clip),"resolve clip");
  std::vector<runtime::ObjectId> targets;runtime::resolveAnimationTargets(graph,owner,*clip.source,targets);
  AE_EXPECT_TRUE(targets.size()==2&&targets[0]==a&&targets[1]==b,"duplicate short names in separate branches bind exactly");
  runtime::SceneAnimator runtime;runtime.begin(graph,session.mapScene());
  runtime::SceneAnimator::ExternalSample sample;sample.owner=owner;sample.clip=asset.guid;sample.time=1;sample.weight=1;
  runtime.setExternalSamples({sample});AE_EXPECT_TRUE(runtime.advance(0,{}),"real compositor accepts authored channels");
  AE_EXPECT_TRUE(std::abs(graph.find(a)->transform.position[0])<1e-5&&std::abs(graph.find(b)->transform.position[0]-2)<1e-5,"only intended object moves");
  AE_EXPECT_TRUE(std::abs(graph.find(b)->transform.rotationDegrees[1]-60)<1e-3,"quaternion SLERP consumed as transform");
  const auto *liveSkin=static_cast<const scene::SkinnedMesh*>(graph.find(b)->components.find(scene::SkinnedMesh::descriptor));
  AE_EXPECT_TRUE(liveSkin->blendShapeWeights.size()==2&&std::abs(liveSkin->blendShapeWeights[0]-50)<1e-4&&std::abs(liveSkin->blendShapeWeights[1]-40)<1e-4,"morph curves reach actual component state");
  graph.createEntity(right,runtime::ObjectKind::Folder,"Junta");runtime::resolveAnimationTargets(graph,owner,*clip.source,targets);
  AE_EXPECT_EQ(targets[1],runtime::kInvalidObject,"ambiguous siblings never bind arbitrarily");
  graph.setName(right,"Outro");runtime::resolveAnimationTargets(graph,owner,*clip.source,targets);
  AE_EXPECT_EQ(targets[1],runtime::kInvalidObject,"missing explicit path never falls back to a short name");
  runtime::SourceAnimations escaped;escaped.nodes={{},{}};
  escaped.nodePaths={resources::animationBindingSegment("A/B")+"/"+resources::animationBindingSegment(".."),resources::animationBindingSegment("50%")};
  const auto slash=graph.createEntity(owner,runtime::ObjectKind::Folder,"A/B"),dots=graph.createEntity(slash,runtime::ObjectKind::Folder,".."),percent=graph.createEntity(owner,runtime::ObjectKind::Folder,"50%");
  runtime::resolveAnimationTargets(graph,owner,escaped,targets);
  AE_EXPECT_TRUE(targets[0]==dots&&targets[1]==percent,"reserved object names remain literal scene names, not path traversal or ambiguity");
}
AE_TEST(animation_clip_preview_seek_cancel_and_failure_never_modify_source_document) {
  editor::EditorDocument original;std::string error;auto asset=fixture(error);
  const auto owner=original.createEntity(original.root(),runtime::ObjectKind::Folder,"Mecanismo");
  const auto left=original.createEntity(owner,runtime::ObjectKind::Folder,"Esquerda"),right=original.createEntity(owner,runtime::ObjectKind::Folder,"Direita");
  original.createEntity(left,runtime::ObjectKind::Folder,"Junta");
  const auto target=original.createEntity(right,runtime::ObjectKind::Folder,"Junta");
  auto *skin=static_cast<scene::SkinnedMesh*>(original.editComponents(target)->add(scene::SkinnedMesh::descriptor));skin->blendShapeWeights={7,8};
  const auto before=editor::serializeEditorDocument(original,0);editor::AnimationClipPreview preview;
  AE_EXPECT_TRUE(preview.begin(original,owner,asset,error),error.c_str());
  AE_EXPECT_TRUE(preview.seek(1,error),error.c_str());
  AE_EXPECT_TRUE(std::abs(preview.scene().find(target)->transform.position[0]-2)<1e-5,"preview uses real compositor");
  AE_EXPECT_TRUE(preview.seek(0,error)&&preview.seek(2,error)&&preview.seek(.5f,error),"scrub backward/forward deterministically");
  AE_EXPECT_TRUE(std::abs(preview.scene().find(target)->transform.position[0]-1)<1e-5,"scrub has no incremental drift");
  const auto pose=preview.scene().find(target)->transform;
  AE_EXPECT_TRUE(!preview.seek(std::numeric_limits<float>::quiet_NaN(),error),"invalid seek");
  AE_EXPECT_EQ(preview.scene().find(target)->transform.position[0],pose.position[0],"invalid seek preserves last pose");
  auto bad=asset;bad.bindings[1].path="Ausente/Junta";
  AE_EXPECT_TRUE(!preview.begin(original,owner,bad,error)&&preview.active(),"failed binding retains current preview");
  AE_EXPECT_EQ(preview.time(),.5f,"failed begin retains time");
  AE_EXPECT_EQ(editor::serializeEditorDocument(original,0),before,"no source state, morph weight, ID or component changed");
  preview.cancel();AE_EXPECT_TRUE(!preview.active(),"cancel destroys evaluated scene");
  AE_EXPECT_EQ(editor::serializeEditorDocument(original,0),before,"cancel needs no fragile source restoration");
}

AE_TEST(animation_clip_extracts_imported_rig_without_resampling_or_source_mutation) {
  using namespace editor;using namespace resources;
  TemporaryProject project;EditorSession session;std::string error;
  ui::UiFont font;ui::UiIconAtlas icons;
  const auto fontBytes=productionUiBytes("assets/astra-visual/ui/astra-ui-font.aeuf"),iconBytes=productionUiBytes("assets/astra-visual/ui/astra-ui-icons.aeui");
  AE_EXPECT_TRUE(font.load(fontBytes)&&icons.load(iconBytes),"production font and atlas");
  session.initialize(&font,&icons);session.setSurface({0,0,853,394},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"isolated registered project");
  // Host-side geometry consumer owns the same packet the shell would upload.
  // Real import, node instantiation, clip library and compositor are exercised;
  // this does not assert GPU rendering or physical-device acceptance.
  std::vector<u8> vertices;std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials),"existing primitive packet");
  AE_EXPECT_TRUE(session.importMap(draws,materials,false,vertices,indices,0),"host geometry initialized");
  session.setGeometryPublisher([&](std::span<const u8> v,std::span<const u32> i,std::span<const renderer::MapDrawRecord> d,
                                  std::span<const renderer::MapMaterialRecord> m,std::span<const renderer::SharedAuthoringTexture>,EditorSession::PublishedGeometry &out) {
    vertices.clear();indices.clear();draws.clear();materials.clear();
    if(!renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials))return false;
    const auto vertexBase=static_cast<u32>(vertices.size()/renderer::MapVertexStride),indexBase=static_cast<u32>(indices.size()),materialBase=static_cast<u32>(materials.size());
    vertices.insert(vertices.end(),v.begin(),v.end());indices.insert(indices.end(),i.begin(),i.end());materials.insert(materials.end(),m.begin(),m.end());
    for(auto draw:d) {draw.firstIndex+=indexBase;draw.vertexOffset+=vertexBase;draw.materialIndex+=materialBase;draws.push_back(draw);}
    out={draws,materials,vertices,indices};return true;
  });
  const auto sourceBytes=test::skinnedAnimatedGlb();
  std::filesystem::create_directories(project.path/"Fontes");
  AE_EXPECT_TRUE(EditorImportTransaction::write(project.path/"Fontes/Rig.glb",sourceBytes),"original source file");
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(sourceBytes,"Fontes/Rig.glb",{},report),report.diagnostic.c_str());
  const auto catalog=session.mapScene().clipCatalog();AE_EXPECT_EQ(catalog.size(),1u,"imported Wave clip discovered");
  runtime::AnimationClipView original;AE_EXPECT_TRUE(session.mapScene().findClip(catalog.front().clip,original),"source clip available");
  const auto originalClip=*original.clip;const auto sourceRecord=*session.assets().find(report.source);
  const auto sourceScene=serializeEditorDocument(session.document(),0);
  const auto depth=session.history().undoDepth();AssetGuid extracted;
  AE_EXPECT_TRUE(session.extractAnimationClip(catalog.front().clip,session.document().root(),extracted,error),error.c_str());
  const auto *asset=session.animationClipAsset(extracted);
  AE_EXPECT_TRUE(asset&&asset->source==report.source&&asset->sourceClip==catalog.front().clip&&asset->sourceHash==sourceRecord.contentHash,"source revision and clip identity retained");
  AE_EXPECT_EQ(asset->bindings.size(),2u,"unused geometry nodes are compacted out of editable bindings");
  AE_EXPECT_EQ(session.history().undoDepth(),depth+1,"one creation transaction");
  runtime::AnimationClipView copy;AE_EXPECT_TRUE(session.mapScene().findClip(extracted,copy),"compiled authored consumer published");
  for(usize c=0;c<originalClip.channels.size();++c)for(u32 frame=0;frame<=200;++frame) {
    float a[MaximumMorphTargets]{},b[MaximumMorphTargets]{};const float time=frame*.005f;
    AE_EXPECT_TRUE(sampleAnimationChannel(originalClip.channels[c],time,a)&&sampleAnimationChannel(copy.clip->channels[c],time,b),"both samplers produce poses");
    for(u32 component=0;component<originalClip.channels[c].components();++component)
      AE_EXPECT_TRUE(std::abs(a[component]-b[component])<2e-5,"Step, quaternion SLERP and cubic values are preserved without baking");
  }
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),0),sourceScene,"extraction does not change imported scene pose or components");
  std::vector<u8> retained;AE_EXPECT_TRUE(EditorImportTransaction::read(project.path/"Fontes/Rig.glb",retained)&&retained==sourceBytes,"source bytes unchanged");
  AE_EXPECT_EQ(session.assets().find(report.source)->contentHash,sourceRecord.contentHash,"source registration unchanged");
  AE_EXPECT_TRUE(session.openAnimationClip(extracted,session.document().root(),error)&&session.seekAnimationClip(.5f,error),error.c_str());
  session.update();tapClipAction(session,font,clip_widget::Choose);tapClipAction(session,font,clip_widget::ImportedCatalog);
  AE_EXPECT_TRUE(session.screen().clipImportedPicker&&session.screen().clipSourceEntries.size()==1,"source catalog comes from the actual imported animation library");
  AE_EXPECT_TRUE(session.screen().clipSourceEntries.front().name.find("Fontes/Rig.glb")!=std::string::npos,"source is visible to distinguish identically named clips");
  captureClipSurface(session,font,icons,"clip-imported-picker.ppm");
  tapClipAction(session,font,clip_widget::Choice);
  const auto extractedViaUi=session.screen().clipGuid;
  AE_EXPECT_TRUE(extractedViaUi!=extracted&&session.animationClipAsset(extractedViaUi)&&session.animationClipAsset(extractedViaUi)->sourceClip==catalog.front().clip,"picker extracts and opens a real independent resource");
  session.closeAnimationClip();
  AE_EXPECT_TRUE(session.history().undo(session.document())&&!session.animationClipAsset(extractedViaUi),"undo UI extraction retires the copy without changing its source");
  // Library publication invalidates views; reacquire before inspecting channels.
  AE_EXPECT_TRUE(session.mapScene().findClip(extracted,copy)&&session.openAnimationClip(extracted,session.document().root(),error)&&session.seekAnimationClip(.5f,error),error.c_str());
  asset=session.animationClipAsset(extracted);
  const auto rotation=std::find_if(copy.clip->channels.begin(),copy.clip->channels.end(),[](const auto &c){return c.path==AnimationPath::Rotation;});
  std::vector<runtime::ObjectId> targets;runtime::resolveAnimationTargets(session.document(),session.document().root(),*copy.source,targets);
  const auto arm=targets[rotation->node];u64 track=0;
  AE_EXPECT_TRUE(session.addAnimationClipTrack(extracted,asset->revision,session.document().root(),arm,AnimationPath::Translation,AnimationRotationMode::Quaternion,track,error),error.c_str());
  AE_EXPECT_TRUE(session.animationClipAsset(extracted)->track(track),"another property uses existing imported node binding");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"undo added property");session.closeAnimationClip();
  const auto missing=session.document().createEntity(session.document().root(),runtime::ObjectKind::Folder,"Sem rig");
  const auto count=session.assets().size();AssetGuid refused;
  AE_EXPECT_TRUE(!session.extractAnimationClip(catalog.front().clip,missing,refused,error)&&session.assets().size()==count,"missing used node refuses without creating a resource");
  AE_EXPECT_TRUE(session.history().undo(session.document())&&!session.animationClipAsset(extracted),"undo extraction removes file, registration and runtime entry");
  AE_EXPECT_TRUE(session.history().redo(session.document())&&session.animationClipAsset(extracted),"redo restores the same resource identity");
  // The shell loads the project registry explicitly before source restoration.
  // Read the committed disk registry, rather than an in-memory serialization.
  std::vector<u8> registered;
  AE_EXPECT_TRUE(EditorImportTransaction::read(project.path/".astra/assets.astra",registered),"journal committed project registry");
  EditorSession reopened;AE_EXPECT_TRUE(reopened.setProjectDirectory(project.path.string().c_str())&&
    reopened.loadAssets({reinterpret_cast<const char*>(registered.data()),registered.size()}),"cold resource loading follows the shell contract");
  AE_EXPECT_TRUE(reopened.animationClipAsset(extracted)&&reopened.animationClipAsset(extracted)->sourceHash==sourceRecord.contentHash,"extracted source relationship persists across sessions");
}

AE_TEST(animation_clip_multiple_selection_uses_real_surface_and_atomic_history) {
  using namespace editor;using namespace ui;using namespace resources;
  TemporaryProject project;EditorSession session;UiFont font;UiIconAtlas icons;std::string error;
  const auto fontBytes=productionUiBytes("assets/astra-visual/ui/astra-ui-font.aeuf"),iconBytes=productionUiBytes("assets/astra-visual/ui/astra-ui-icons.aeui");
  AE_EXPECT_TRUE(font.load(fontBytes)&&icons.load(iconBytes),"production visual resources");
  session.initialize(&font,&icons);session.setSurface({0,0,853,394},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"isolated project");
  const auto owner=session.document().createEntity(session.document().root(),EditorEntityKind::Folder,"Palco");
  AssetGuid guid;AE_EXPECT_TRUE(session.createAnimationClip(owner,2,guid,error),error.c_str());
  const auto rotation=session.animationClipAsset(guid)->tracks[1].id;
  AE_EXPECT_TRUE(session.editAnimationClipAsset(guid,1,[&](auto &candidate) {
    const float a[]{0,.38268343f,0,.92387953f},b[]{0,.70710678f,0,.70710678f};
    return candidate.putPose(rotation,.5f,a,error)&&candidate.putPose(rotation,1,b,error);
  },error)&&session.openAnimationClip(guid,owner,error),error.c_str());session.update();
  const auto sceneBefore=serializeEditorDocument(session.document(),0);
  tapClipAction(session,font,clip_widget::Selection);
  tapClipAction(session,font,clip_widget::RowsNext);
  const auto canvas=session.layout().clipCanvas;
  const UiPoint start{canvas.x+canvas.width*.2f,canvas.y+5},end{canvas.x+canvas.width*.55f,canvas.y+25};
  session.handlePointer({0,UiPointerPhase::Down,start,1});session.handlePointer({0,UiPointerPhase::Move,end,1.1});session.update();
  AE_EXPECT_TRUE(session.screen().clipSelecting&&session.screen().clipSelection.size()==8,"box selects two complete quaternion poses");
  AE_EXPECT_TRUE(session.layout().clipCanvas.y==canvas.y,"selection feedback does not move the canvas during the gesture");
  captureClipSurface(session,font,icons,"clip-selection-box.ppm");
  session.handlePointer({0,UiPointerPhase::Up,end,1.2});session.update();
  const auto selection=session.screen().clipSelection;const auto frontier=session.animationClipAsset(guid)->nextId;
  captureClipSurface(session,font,icons,"clip-selection.ppm");
  const auto depth=session.history().undoDepth();
  tapClipAction(session,font,clip_widget::SelectionOffset);auto edit=session.pendingTextEdit();
  AE_EXPECT_TRUE(session.completeTextEdit(edit,"0.25",true),"typed group offset commits through numeric editor");session.update();
  const auto *asset=session.animationClipAsset(guid);
  for(const auto &curve:asset->track(rotation)->curves)AE_EXPECT_TRUE(curve.keys[1].time==.75f&&curve.keys[2].time==1.25f,"every quaternion component retains synchronization");
  AE_EXPECT_TRUE(asset->nextId==frontier&&session.screen().clipSelection==selection&&session.history().undoDepth()==depth+1,"one persistent transaction preserves selected IDs");
  tapClipAction(session,font,clip_widget::Undo);tapClipAction(session,font,clip_widget::Redo);
  const auto unchanged=session.animationClipAsset(guid)->serialize();const auto undoDepth=session.history().undoDepth();
  tapClipAction(session,font,clip_widget::SelectionOffset);edit=session.pendingTextEdit();
  AE_EXPECT_TRUE(!session.completeTextEdit(edit,"1",true),"collision refuses the whole selection");session.update();
  AE_EXPECT_TRUE(session.screen().clipDiagnostic.find("Seleção ultrapassa")!=std::string::npos,"numeric refusal retains the resource's actionable reason");
  AE_EXPECT_TRUE(session.animationClipAsset(guid)->serialize()==unchanged&&session.history().undoDepth()==undoDepth,"failed transform does not write resource or history");
  AE_EXPECT_TRUE(session.completeTextEdit(edit,"",false),"cancel the refused numeric field before another operation");session.update();
  AE_EXPECT_TRUE(session.seekAnimationClip(.25f,error),error.c_str());session.update();
  tapClipAction(session,font,clip_widget::SelectionScale);edit=session.pendingTextEdit();
  AE_EXPECT_TRUE(session.completeTextEdit(edit,"1.1",true),"scale around playhead updates the selected time interval");session.update();
  asset=session.animationClipAsset(guid);AE_EXPECT_TRUE(std::abs(asset->track(rotation)->curves[0].keys[1].time-.8f)<1e-6,"scale uses the nonzero captured playhead pivot");
  const auto beforeDrag=asset->serialize();const auto dragDepth=session.history().undoDepth();
  const auto shifted=session.layout().clipCanvas;
  const UiPoint key{shifted.x+shifted.width*.8f/2,shifted.y+13};
  session.handlePointer({0,UiPointerPhase::Down,key,2});session.handlePointer({0,UiPointerPhase::Move,{key.x+10,key.y},2.1});session.update();
  session.handlePointer({0,UiPointerPhase::Cancel,{key.x+10,key.y},2.2});session.update();
  AE_EXPECT_TRUE(session.animationClipAsset(guid)->serialize()==beforeDrag&&session.history().undoDepth()==dragDepth,"cancel group drag publishes nothing");
  tapClipAction(session,font,clip_widget::Edits);captureClipSurface(session,font,icons,"clip-edit-menu.ppm");tapClipAction(session,font,clip_widget::CopyKeys);
  AE_EXPECT_TRUE(session.screen().clipClipboard&&session.screen().clipClipboard->size()==8,"copy owns both synchronized poses");
  AE_EXPECT_TRUE(session.seekAnimationClip(.1f,error),error.c_str());session.update();
  tapClipAction(session,font,clip_widget::Edits);tapClipAction(session,font,clip_widget::PasteReplace);
  for(const auto &curve:session.animationClipAsset(guid)->track(rotation)->curves)AE_EXPECT_EQ(curve.keys.size(),6u,"native menu pastes both poses through the journal");
  AE_EXPECT_EQ(session.screen().clipSelection.size(),8u,"pasted keys become the stable active selection");
  tapClipAction(session,font,clip_widget::Edits);tapClipAction(session,font,clip_widget::CutKeys);
  for(const auto &curve:session.animationClipAsset(guid)->track(rotation)->curves)AE_EXPECT_EQ(curve.keys.size(),4u,"cut publishes clipboard only after atomic key removal");
  tapClipAction(session,font,clip_widget::Undo);tapClipAction(session,font,clip_widget::Undo);
  for(const auto &curve:session.animationClipAsset(guid)->track(rotation)->curves)AE_EXPECT_EQ(curve.keys.size(),4u,"undo cut and paste restores pre-paste topology");
  // Select the original pair again: undo reconciles retired pasted IDs rather
  // than allowing them to select newly allocated keys at the same time.
  AE_EXPECT_TRUE(session.screen().clipSelection.empty(),"retired clipboard IDs leave no stale active selection");
  const auto restoredCanvas=session.layout().clipCanvas;
  const UiPoint boxStart{restoredCanvas.x+restoredCanvas.width*.35f,restoredCanvas.y+5},boxEnd{restoredCanvas.x+restoredCanvas.width*.72f,restoredCanvas.y+25};
  session.handlePointer({0,UiPointerPhase::Down,boxStart,3});session.handlePointer({0,UiPointerPhase::Move,boxEnd,3.1});session.handlePointer({0,UiPointerPhase::Up,boxEnd,3.2});session.update();
  AE_EXPECT_EQ(session.screen().clipSelection.size(),8u,"range reselects the original persistent poses after history");
  tapClipAction(session,font,clip_widget::DeleteKey);
  for(const auto &curve:session.animationClipAsset(guid)->track(rotation)->curves)AE_EXPECT_EQ(curve.keys.size(),2u,"delete retires both selected poses atomically");
  tapClipAction(session,font,clip_widget::Undo);
  for(const auto &curve:session.animationClipAsset(guid)->track(rotation)->curves)AE_EXPECT_EQ(curve.keys.size(),4u,"undo restores grouped persistent keys");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),0),sceneBefore,"source scene pose never changes");
  std::vector<u8> registered;AE_EXPECT_TRUE(EditorImportTransaction::read(project.path/".astra/assets.astra",registered),"persisted registry");
  EditorSession reopened;AE_EXPECT_TRUE(reopened.setProjectDirectory(project.path.string().c_str())&&reopened.loadAssets({reinterpret_cast<const char*>(registered.data()),registered.size()}),"cold resource load");
  AE_EXPECT_TRUE(reopened.animationClipAsset(guid)->track(rotation)->curves[0].keys[1].time==session.animationClipAsset(guid)->track(rotation)->curves[0].keys[1].time,"multi-key changes survive reloading");
  session.setSurface({0,0,655,300},{});session.update();
  const auto smallCanvas=session.layout().clipCanvas;
  const UiPoint smallKey{smallCanvas.x+smallCanvas.width*.8f/2,smallCanvas.y+13};
  session.handlePointer({0,UiPointerPhase::Down,smallKey,4});session.handlePointer({0,UiPointerPhase::Up,smallKey,4.1});session.update();
  AE_EXPECT_TRUE(!session.screen().clipSelection.empty(),"small surface selects a complete pose");
  tapClipAction(session,font,clip_widget::Edits);
  for(const auto action:{clip_widget::CopyKeys,clip_widget::CutKeys,clip_widget::PasteReplace,
      clip_widget::PasteInsertTracks,clip_widget::PasteInsertAll,clip_widget::Duration,
      clip_widget::PreviousFrame,clip_widget::NextFrame,clip_widget::BakeOpen,clip_widget::EditsClose}) {
    const auto point=clipActionPosition(session,font,action);
    AE_EXPECT_TRUE(point.x>=0&&point.x<655&&point.y>=0&&point.y<300,"every contextual operation remains reachable in a smaller landscape surface");
  }
  captureClipSurface(session,font,icons,"clip-edit-menu-small.ppm");
  tapClipAction(session,font,clip_widget::BakeOpen);
  for(const auto action:{clip_widget::BakeRate,clip_widget::BakeTolerance,clip_widget::BakeMode,
      clip_widget::BakeReduction,clip_widget::BakeApply,clip_widget::BakeClose})
    AE_EXPECT_TRUE(clipActionPosition(session,font,action).x>=0,"all bake actions remain reachable on 655 by 300");
  captureClipSurface(session,font,icons,"clip-bake-small.ppm");
}

AE_TEST(animation_clip_mobile_surface_edits_preview_history_and_cancel_are_connected) {
  using namespace editor;using namespace ui;
  TemporaryProject project;EditorSession session;UiFont font;UiIconAtlas icons;std::string error;
  const auto read=[](const char *path){std::ifstream file(path,std::ios::binary);return std::vector<u8>(std::istreambuf_iterator<char>(file),{});};
  // The production font/atlas borrow their pixel storage. Keep the bytes alive
  // through raster capture, just as the Android shell keeps its asset buffers.
  const auto fontBytes=read("assets/astra-visual/ui/astra-ui-font.aeuf"),iconBytes=read("assets/astra-visual/ui/astra-ui-icons.aeui");
  AE_EXPECT_TRUE(font.load(fontBytes)&&icons.load(iconBytes),"production font and icon atlas");
  session.initialize(&font,&icons);session.setSurface({0,0,853,394},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"isolated project");
  auto &doc=session.document();const auto owner=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Mecanismo");
  const auto rotor=doc.createEntity(owner,EditorEntityKind::Folder,"Rotor/A");
  resources::AssetGuid guid;AE_EXPECT_TRUE(session.createAnimationClip(owner,2,guid,error)&&session.openAnimationClip(guid,owner,error),error.c_str());
  const auto sceneBefore=serializeEditorDocument(doc,0);session.update();
  const auto location=[&](u32 code) {
    UiDrawList list;list.begin(session.screen().surface,font.metrics(UiFontWeight::Regular));UiInputRouter router;router.beginFrame();
    buildEditorScreen(session.screen(),editorTheme(),list,router);UiPoint point{-1,-1};
    for(float y=2;y<394&&point.x<0;y+=3)for(float x=2;x<853;x+=3) {
      const auto hit=router.hitTest({x,y});if(hit.target==UiPointerTarget::Widget&&hit.widgetId==clip_widget::id(code)) {point={x,y};break;}
    }
    return point;
  };
  const auto tap=[&](u32 code) {
    const auto point=location(code);AE_EXPECT_TRUE(point.x>=0,"clip action is drawn and reachable through the production router");
    session.handlePointer({0,UiPointerPhase::Down,point,1});session.handlePointer({0,UiPointerPhase::Up,point,1.1});session.update();
  };
  const auto capture=[&](const char *name) {
    if(const auto *directory=std::getenv("AE_CLIP_CAPTURE_DIR")) {
      const std::filesystem::path output(directory);std::error_code ec;std::filesystem::create_directories(output,ec);
      test::UiSoftwareTarget target;target.resize(853,394,.08f,.09f,.11f);test::rasterizeUi(session.instances(),font,icons,target);
      std::ofstream file(output/name,std::ios::binary);file<<"P6\n853 394\n255\n";
      for(usize i=0;i<target.pixels.size();i+=4)for(u32 c=0;c<3;++c)file.put(static_cast<char>(std::clamp(target.pixels[i+c]*255.f,0.f,255.f)));
    }
  };
  AE_EXPECT_TRUE(session.layout().viewport.height>130&&session.layout().clipCanvas.width>500,"useful landscape preview and editing surface");
  tap(clip_widget::Curves);AE_EXPECT_TRUE(session.screen().clipCurves,"curve route switches actual canvas");
  AE_EXPECT_TRUE(session.seekAnimationClip(.5f,error),error.c_str());tap(clip_widget::AddKey);
  const auto inserted=session.screen().clipKey;AE_EXPECT_TRUE(inserted&&session.animationClipAsset(guid)->tracks[0].curves[0].keys.size()==3,"native UI creates a persistent key");
  tap(clip_widget::KeyValue);const auto edit=session.pendingTextEdit();AE_EXPECT_TRUE(edit.purpose==EditorTextPurpose::Number,"real numeric editor opened");
  AE_EXPECT_TRUE(session.completeTextEdit(edit,"3.25",true),"numeric field commits to same authored clip");session.update();
  AE_EXPECT_TRUE(std::abs(session.evaluatedEditorScene().find(owner)->transform.position[0]-3.25f)<1e-5,"authored key affects evaluated preview pose");
  AE_EXPECT_EQ(serializeEditorDocument(doc,0),sceneBefore,"preview never changes saved source pose");
  capture("clip-selected-key.ppm");
  tap(clip_widget::Tangent);capture("clip-tangents.ppm");tap(clip_widget::TangentLink);tap(clip_widget::SlopeOut);
  auto numeric=session.pendingTextEdit();AE_EXPECT_TRUE(session.completeTextEdit(numeric,"1.5",true),"free slopes are editable through real keyboard flow");session.update();
  const auto &sloped=session.animationClipAsset(guid)->tracks[0].curves[0].keys[1];
  AE_EXPECT_TRUE(!sloped.broken&&sloped.inSlope==1.5f&&sloped.outSlope==1.5f,"linked tangent edit updates both effective slopes");
  tap(clip_widget::WeightOut);numeric=session.pendingTextEdit();
  const bool weightSaved=session.completeTextEdit(numeric,"0.55",true);
  AE_EXPECT_TRUE(weightSaved,("weighted handle fraction edited · "+session.screen().clipDiagnostic+" · "+session.screen().status).c_str());session.update();
  tap(clip_widget::TangentClose);
  const auto handleDepth=session.history().undoDepth();const auto handleOriginal=session.animationClipAsset(guid)->serialize();
  const auto handle=location(clip_widget::HandleOut);
  AE_EXPECT_TRUE(handle.x>=0,"weighted handle is drawn, visible and touchable");
  session.handlePointer({0,UiPointerPhase::Down,handle,1.3});session.handlePointer({0,UiPointerPhase::Move,{handle.x+15,handle.y+8},1.4});session.update();
  session.handlePointer({0,UiPointerPhase::Cancel,{handle.x+15,handle.y+8},1.5});session.update();
  AE_EXPECT_TRUE(session.history().undoDepth()==handleDepth&&session.animationClipAsset(guid)->serialize()==handleOriginal,"cancel weighted handle drag restores resource and history");
  const auto depth=session.history().undoDepth();const auto original=session.animationClipAsset(guid)->serialize();
  const auto canvas=session.layout().clipCanvas;const float x=canvas.x+canvas.width*.25f;
  const float y=canvas.bottom()-(3.25f-session.screen().clipMinimum)/(session.screen().clipMaximum-session.screen().clipMinimum)*canvas.height;
  session.handlePointer({0,UiPointerPhase::Down,{x,y},2});session.handlePointer({0,UiPointerPhase::Move,{x+50,y-10},2.1});session.update();
  session.handlePointer({0,UiPointerPhase::Cancel,{x+50,y-10},2.2});session.update();
  AE_EXPECT_TRUE(session.animationClipAsset(guid)->serialize()==original&&session.history().undoDepth()==depth,"cancelled pointer draft publishes neither disk nor history");
  AE_EXPECT_EQ(session.screen().clipKey,inserted,"cancel preserves the selected key for the next edit");
  tap(clip_widget::DeleteKey);AE_EXPECT_EQ(session.animationClipAsset(guid)->tracks[0].curves[0].keys.size(),2u,"delete affects resource");
  tap(clip_widget::Undo);AE_EXPECT_EQ(session.animationClipAsset(guid)->tracks[0].curves[0].keys.size(),3u,"undo republishes resource and preview");
  tap(clip_widget::Redo);AE_EXPECT_EQ(session.animationClipAsset(guid)->tracks[0].curves[0].keys.size(),2u,"redo republishes");
  const auto beforeBake=*session.animationClipAsset(guid);const auto bakeDepth=session.history().undoDepth();
  tap(clip_widget::Edits);tap(clip_widget::BakeOpen);
  tap(clip_widget::BakeRate);auto bakeField=session.pendingTextEdit();
  AE_EXPECT_TRUE(session.completeTextEdit(bakeField,"24",true),"bake sampling rate is editable through the real keyboard");session.update();
  tap(clip_widget::BakeTolerance);bakeField=session.pendingTextEdit();
  AE_EXPECT_TRUE(session.completeTextEdit(bakeField,"0.001",true),"bake tolerance is an effective setting");session.update();
  capture("clip-bake-before.ppm");tap(clip_widget::BakeApply);capture("clip-bake-result.ppm");
  AE_EXPECT_TRUE(session.screen().clipBakeHasReport&&session.screen().clipBakeReport.verifiedSamples>0&&session.screen().clipBakeReport.maximumError<=.001,
      session.screen().clipDiagnostic.c_str());
  AE_EXPECT_TRUE(session.history().undoDepth()==bakeDepth+1,"one bake publishes one resource history entry");
  tap(clip_widget::Undo);auto expectedBakeUndo=beforeBake;const auto *undone=session.animationClipAsset(guid);
  AE_EXPECT_TRUE(undone->revision>beforeBake.revision&&undone->nextId>=beforeBake.nextId,"undo preserves monotonic revision and allocator frontier");
  expectedBakeUndo.revision=undone->revision;expectedBakeUndo.nextId=undone->nextId;
  AE_EXPECT_EQ(undone->serialize(),expectedBakeUndo.serialize(),"undo restores every original curve and binding while retaining revision/frontier");
  AE_EXPECT_TRUE(!session.screen().clipBakeHasReport,"undo invalidates the old bake report");
  tap(clip_widget::Redo);AE_EXPECT_TRUE(session.animationClipAsset(guid)->tracks[0].sourceOverride,"redo restores authored baked output");
  tap(clip_widget::Undo);tap(clip_widget::BakeClose);
  // Raster evidence is produced by the executable UI and production bitmaps.
  // The blank scene area is not Vulkan or physical-device evidence.
  if(std::getenv("AE_CLIP_CAPTURE_DIR")) {
    tap(clip_widget::Undo);
    tap(clip_widget::Keys);capture("clip-keys.ppm");
    tap(clip_widget::Curves);capture("clip-curves.ppm");
  }
  tap(clip_widget::New);tap(clip_widget::NewEuler);
  const auto eulerGuid=session.screen().clipGuid;const auto *euler=session.animationClipAsset(eulerGuid);
  AE_EXPECT_TRUE(eulerGuid!=guid&&euler&&euler->tracks[1].rotationMode==resources::AnimationRotationMode::Euler,"mode creation is reachable through real UI");
  AE_EXPECT_TRUE(session.editAnimationClipAsset(eulerGuid,euler->revision,[&](auto &candidate) {
    auto key=candidate.tracks[1].curves[1].keys.back();key.value=720;u64 id=0;
    return candidate.putKey(candidate.tracks[1].id,1,key,id,error);
  },error)&&session.seekAnimationClip(.25f,error),error.c_str());session.update();
  AE_EXPECT_TRUE(std::abs(session.evaluatedEditorScene().find(owner)->transform.rotationDegrees[1]-90)<.01,"Euler turns reach actual preview compositor");
  tap(clip_widget::New);tap(clip_widget::NewProgressive);
  const auto *progress=session.animationClipAsset(session.screen().clipGuid);
  AE_EXPECT_TRUE(progress&&progress->tracks[1].rotationMode==resources::AnimationRotationMode::ProgressiveQuaternion&&progress->tracks[1].curves.size()==5,"progressive mode creates pose and speed data");
  const auto progressiveGuid=progress->guid;
  tap(clip_widget::Name);auto name=session.pendingTextEdit();
  AE_EXPECT_TRUE(session.completeTextEdit(name,"Mecanismo universal",true),"rename commits through the real text flow");session.update();
  AE_EXPECT_EQ(session.animationClipAsset(progressiveGuid)->name,std::string("Mecanismo universal"),"name persists in resource and catalog");
  tap(clip_widget::Targets);capture("clip-targets.ppm");tap(clip_widget::TargetChoice);capture("clip-properties.ppm");tap(clip_widget::PropertyChoice+4);
  const auto childTrack=session.screen().clipTrack;
  AE_EXPECT_TRUE(session.screen().clipTarget==rotor&&session.animationClipAsset(progressiveGuid)->tracks.size()==4,"hierarchy picker authors a reusable child property, not a player preset");
  AE_EXPECT_TRUE(session.seekAnimationClip(1,error),error.c_str());session.update();tap(clip_widget::Pose);tap(clip_widget::PoseValue+1);
  numeric=session.pendingTextEdit();AE_EXPECT_TRUE(session.completeTextEdit(numeric,"90",true),"pose editor writes a grouped rotation key");session.update();
  AE_EXPECT_TRUE(std::abs(session.evaluatedEditorScene().find(rotor)->transform.rotationDegrees[1]-90)<.01,"full pose flows through curve publication and actual preview");
  AE_EXPECT_EQ(serializeEditorDocument(doc,0),sceneBefore,"pose authoring never overwrites the original scene transform");
  capture("clip-pose.ppm");
  tap(clip_widget::RemoveTrack);AE_EXPECT_TRUE(!session.animationClipAsset(progressiveGuid)->track(childTrack),"remove property retires the track");
  tap(clip_widget::Undo);AE_EXPECT_TRUE(session.animationClipAsset(progressiveGuid)->track(childTrack),"undo restores track identity and grouped pose");
  tap(clip_widget::Close);AE_EXPECT_TRUE(!session.screen().clipOpen&&serializeEditorDocument(doc,0)==sceneBefore,"close restores document display without mutation");
}

AE_TEST(animation_clip_conversion_consolidation_ui_api_publication_and_reopen) {
  using namespace editor;using namespace ui;using namespace resources;
  TemporaryProject project;EditorSession session;UiFont font;UiIconAtlas icons;std::string error;
  const auto fontBytes=productionUiBytes("assets/astra-visual/ui/astra-ui-font.aeuf"),iconBytes=productionUiBytes("assets/astra-visual/ui/astra-ui-icons.aeui");
  AE_EXPECT_TRUE(font.load(fontBytes)&&icons.load(iconBytes),"production UI assets");
  session.initialize(&font,&icons);session.setSurface({0,0,853,394},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"project");
  const auto owner=session.document().createEntity(session.document().root(),runtime::ObjectKind::Folder,"Rotor");
  AssetGuid guid;AE_EXPECT_TRUE(session.createAnimationClip(owner,2,guid,error,AnimationRotationMode::Euler),error.c_str());
  const u64 rotation=session.animationClipAsset(guid)->tracks[1].id;
  AE_EXPECT_TRUE(session.editAnimationClipAsset(guid,1,[&](auto &a){const float p[]{20,720,35};
    if(!a.putPose(rotation,2,p,error))return false;
    AnimationBakeSettings s;s.rotation=0;AnimationBakeReport r;return bakeAnimationClipTrack(a,rotation,s,r,error);},error),error.c_str());
  AE_EXPECT_TRUE(session.openAnimationClip(guid,owner,error),error.c_str());session.update();
  while(session.screen().clipRow+session.layout().clipVisibleRows<=3)tapClipAction(session,font,clip_widget::RowsNext);
  tapClipAction(session,font,clip_widget::Row+3-session.screen().clipRow);
  tapClipAction(session,font,clip_widget::Edits);tapClipAction(session,font,clip_widget::BakeOpen);
  tapClipAction(session,font,clip_widget::BakeMode);tapClipAction(session,font,clip_widget::BakeMode);tapClipAction(session,font,clip_widget::BakeMode);
  AE_EXPECT_TRUE(session.screen().clipBakeReferenceShown&&clipActionPosition(session,font,clip_widget::BakeApply).x<0,"Euler cannot silently choose a branch");
  tapClipAction(session,font,clip_widget::BakeSeedUse);tapClipAction(session,font,clip_widget::BakeSeedX);
  auto field=session.pendingTextEdit();AE_EXPECT_TRUE(session.completeTextEdit(field,"360",true),"explicit XYZ seed through real numeric input");session.update();
  captureClipSurface(session,font,icons,"clip-euler-branch.ppm");
  const auto depth=session.history().undoDepth();tapClipAction(session,font,clip_widget::BakeApply);
  AE_EXPECT_TRUE(session.screen().clipBakeHasReport&&session.animationClipAsset(guid)->track(rotation)->rotationMode==AnimationRotationMode::Euler&&session.history().undoDepth()==depth+1,"conversion publishes one transaction");
  tapClipAction(session,font,clip_widget::Undo);tapClipAction(session,font,clip_widget::Redo);
  const auto original=session.animationClipAsset(guid)->serialize();
  tapClipAction(session,font,clip_widget::BakeTarget);tapClipAction(session,font,clip_widget::BakeReference);
  captureClipSurface(session,font,icons,"clip-consolidate-before.ppm");tapClipAction(session,font,clip_widget::BakeApply);
  const auto created=session.screen().clipGuid;
  AE_EXPECT_TRUE(created!=guid&&session.animationClipAsset(created)&&session.animationClipAsset(guid)->serialize()==original&&session.screen().clipBakeHasReport,"new clip and source preservation");
  captureClipSurface(session,font,icons,"clip-consolidate-result.ppm");session.closeAnimationClip();
  AE_EXPECT_TRUE(session.history().undo(session.document())&&!session.animationClipAsset(created),"Undo removes file, registry and runtime library");
  AE_EXPECT_TRUE(session.history().redo(session.document())&&session.animationClipAsset(created),"Redo recreates a whole registered resource");
  EditorSession reopened;AE_EXPECT_TRUE(reopened.setProjectDirectory(project.path.string().c_str())&&reopened.loadAssets(session.serializeAssets())&&reopened.animationClipAsset(created),"cold reopen");
  runtime::SceneAnimator player;player.begin(session.document(),session.mapScene());runtime::SceneAnimator::ExternalSample sample;sample.owner=owner;sample.clip=created;sample.time=.5;sample.weight=1;
  player.setExternalSamples({sample});AE_EXPECT_TRUE(player.advance(0,{}),"consolidation is consumed by the production runtime");player.reset();
  AnimationAuthoringScope scope(session);const auto &api=scope.access();const auto draft=api.begin(api.context,guid,0);
  AnimationAuthorBakeRequest request;AnimationAuthorBakeReport report;AssetGuid apiCreated;
  const std::string name="API consolidada";
  AE_EXPECT_TRUE(api.version==5&&api.consolidate(api.context,draft,reinterpret_cast<const u8*>(name.data()),name.size(),&request,&apiCreated,&report)==1&&report.verifiedSamples&&apiCreated!=guid,"ABI 4 publication stays available in ABI 5 without a visible panel");
  const auto before=session.animationClipAsset(guid)->serialize();request.settings.rotation=1;
  const auto quaternionDraft=api.begin(api.context,apiCreated,0);
  AE_EXPECT_TRUE(!api.bakeAdvanced(api.context,quaternionDraft,rotation,&request,&report),"missing Euler reference rejected through ABI");
  AE_EXPECT_EQ(session.animationClipAsset(guid)->serialize(),before,"failed conversion does not publish");
  session.setSurface({0,0,655,300},{});AE_EXPECT_TRUE(session.openAnimationClip(guid,owner,error),error.c_str());session.update();
  tapClipAction(session,font,clip_widget::Edits);tapClipAction(session,font,clip_widget::BakeOpen);tapClipAction(session,font,clip_widget::BakeTarget);
  captureClipSurface(session,font,icons,"clip-consolidate-small.ppm");
  for(const auto code:{clip_widget::BakeTarget,clip_widget::BakeApply,clip_widget::BakeClose})AE_EXPECT_TRUE(clipActionPosition(session,font,code).x>=0,"touch controls fit 655x300");
}
AE_TEST(animation_clip_pose_draft_manual_auto_record_and_lifecycle) {
  using namespace editor;using namespace resources;using namespace ui;
  TemporaryProject project;EditorSession session;UiFont font;UiIconAtlas icons;std::string error;
  const auto fontBytes=productionUiBytes("assets/astra-visual/ui/astra-ui-font.aeuf"),iconBytes=productionUiBytes("assets/astra-visual/ui/astra-ui-icons.aeui");
  AE_EXPECT_TRUE(font.load(fontBytes)&&icons.load(iconBytes),"production assets");
  session.initialize(&font,&icons);session.setSurface({0,0,853,394},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"project");
  const auto owner=session.document().createEntity(session.document().root(),runtime::ObjectKind::Folder,"Mecanismo");
  AssetGuid guid;AE_EXPECT_TRUE(session.createAnimationClip(owner,2,guid,error)&&session.openAnimationClip(guid,owner,error)&&session.seekAnimationClip(1,error),error.c_str());session.update();
  const auto track=session.screen().clipTrack;const auto depth=session.history().undoDepth();
  const auto before=session.animationClipAsset(guid)->serialize(),sceneBefore=serializeEditorDocument(session.document(),0);
  const float first[]{3,0,0},second[]{3,4,0};
  AE_EXPECT_TRUE(session.previewAnimationClipPose(track,first,error)&&session.previewAnimationClipPose(track,second,error),error.c_str());
  AE_EXPECT_TRUE(session.screen().clipPosePending&&session.evaluatedEditorScene().find(owner)->transform.position[1]==4,"pending pose is actually composed");
  AE_EXPECT_TRUE(session.animationClipAsset(guid)->serialize()==before&&session.history().undoDepth()==depth&&serializeEditorDocument(session.document(),0)==sceneBefore,"draft never writes project, source pose or history");
  AE_EXPECT_TRUE(!session.seekAnimationClip(-1,error)&&session.screen().clipPosePending,"invalid seek preserves pending pose");
  session.cancelAnimationClipPose();session.update();
  AE_EXPECT_TRUE(!session.screen().clipPosePending&&session.evaluatedEditorScene().find(owner)->transform.position[0]==0,"cancel recomposes published pose");
  tapClipAction(session,font,clip_widget::Pose);tapClipAction(session,font,clip_widget::PoseAutoKey);
  AE_EXPECT_TRUE(!session.screen().clipPoseAutoKey,"manual mode is reachable");
  tapClipAction(session,font,clip_widget::PoseValue);auto field=session.pendingTextEdit();
  AE_EXPECT_TRUE(session.completeTextEdit(field,"5",true),"numeric editor can experiment without recording");session.update();
  AE_EXPECT_TRUE(session.screen().clipPosePending&&session.animationClipAsset(guid)->serialize()==before,"manual edit keeps source bytes unchanged");
  captureClipSurface(session,font,icons,"clip-pose-pending.ppm");
  tapClipAction(session,font,clip_widget::PoseValue+1);field=session.pendingTextEdit();
  AE_EXPECT_TRUE(session.completeTextEdit(field,"6",true),"a second component belongs to the same draft");session.update();
  tapClipAction(session,font,clip_widget::PoseRecord);
  AE_EXPECT_TRUE(!session.screen().clipPosePending&&session.history().undoDepth()==depth+1,"manual record publishes one complete pose transaction");
  AE_EXPECT_TRUE(session.evaluatedEditorScene().find(owner)->transform.position[0]==5&&session.evaluatedEditorScene().find(owner)->transform.position[1]==6,"record includes all pending components");
  tapClipAction(session,font,clip_widget::Undo);AE_EXPECT_TRUE(session.evaluatedEditorScene().find(owner)->transform.position[0]==0,"undo recomposes before recording");
  tapClipAction(session,font,clip_widget::Redo);AE_EXPECT_TRUE(session.evaluatedEditorScene().find(owner)->transform.position[1]==6,"redo restores grouped pose");
  tapClipAction(session,font,clip_widget::PoseAutoKey);tapClipAction(session,font,clip_widget::PoseValue+2);field=session.pendingTextEdit();
  AE_EXPECT_TRUE(session.completeTextEdit(field,"7",true),"auto-key records through the same transaction");session.update();
  AE_EXPECT_TRUE(!session.screen().clipPosePending&&session.history().undoDepth()==depth+2&&session.evaluatedEditorScene().find(owner)->transform.position[2]==7,"auto-key publishes one pose");
  AE_EXPECT_TRUE(session.previewAnimationClipPose(track,first,error)&&session.seekAnimationClip(0,error)&&!session.screen().clipPosePending,"valid seek discards only unrecorded changes");
  AE_EXPECT_TRUE(session.previewAnimationClipPose(track,second,error),error.c_str());session.closeAnimationClip();
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),0),sceneBefore,"close never copies preview pose into scene");
  AE_EXPECT_TRUE(session.openAnimationClip(guid,owner,error)&&!session.screen().clipPosePending,error.c_str());
  session.setSurface({0,0,655,300},{});session.update();tapClipAction(session,font,clip_widget::Pose);
  AE_EXPECT_TRUE(clipActionPosition(session,font,clip_widget::PoseAutoKey).x>=0,"auto-key remains reachable on a small landscape surface");
  AE_EXPECT_TRUE(session.previewAnimationClipPose(track,first,error),error.c_str());session.update();
  for(const auto action:{clip_widget::PoseRecord,clip_widget::PoseCancel})AE_EXPECT_TRUE(clipActionPosition(session,font,action).x>=0,"record and cancel fit a small surface");
  captureClipSurface(session,font,icons,"clip-pose-pending-small.ppm");
  tapClipAction(session,font,clip_widget::Keys);AE_EXPECT_TRUE(!session.screen().clipPosePending,"timeline navigation cannot publish a pose draft accidentally");
  session.closeAnimationClip();EditorSession reopened;
  AE_EXPECT_TRUE(reopened.setProjectDirectory(project.path.string().c_str())&&reopened.loadAssets(session.serializeAssets()),"cold project reopen");
  const auto *persisted=reopened.animationClipAsset(guid);
  AE_EXPECT_TRUE(persisted&&persisted->track(track)->curves[2].keys[1].value==7,"recorded keys survive reopening");
}
AE_TEST(animation_clip_pose_gizmo_records_one_gesture_and_cancels_without_source_mutation) {
  using namespace editor;using namespace resources;using namespace ui;
  TemporaryProject project;EditorSession session;UiFont font;UiIconAtlas icons;std::string error;
  const auto fontBytes=productionUiBytes("assets/astra-visual/ui/astra-ui-font.aeuf"),iconBytes=productionUiBytes("assets/astra-visual/ui/astra-ui-icons.aeui");
  AE_EXPECT_TRUE(font.load(fontBytes)&&icons.load(iconBytes),"assets");
  session.initialize(&font,&icons);session.setSurface({0,0,853,394},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"project");
  const auto owner=session.document().createEntity(session.document().root(),runtime::ObjectKind::Folder,"Rotor");
  AssetGuid guid;AE_EXPECT_TRUE(session.createAnimationClip(owner,2,guid,error)&&session.openAnimationClip(guid,owner,error)&&session.seekAnimationClip(1,error),error.c_str());session.update();
  const auto sceneBefore=serializeEditorDocument(session.document(),0);const auto depth=session.history().undoDepth();
  tapClipAction(session,font,clip_widget::Pose);tapClipAction(session,font,clip_widget::PoseIsolate);
  const auto point=clipActionPosition(session,font,clip_widget::PoseGizmo);
  AE_EXPECT_TRUE(point.x>=0,"translation axis is visible and reachable through the real router");
  session.handlePointer({17,UiPointerPhase::Down,point,1});
  const auto moved=UiPoint{point.x+35,point.y};session.handlePointer({17,UiPointerPhase::Move,moved,1.1});session.update();
  AE_EXPECT_TRUE(session.screen().clipPosePending&&session.history().undoDepth()==depth,"drag only updates an isolated pose draft");
  session.handlePointer({17,UiPointerPhase::Move,{point.x+45,point.y},1.2});session.update();
  session.handlePointer({17,UiPointerPhase::Up,{point.x+45,point.y},1.3});session.update();
  AE_EXPECT_TRUE(!session.screen().clipPosePending&&session.history().undoDepth()==depth+1,"one whole drag records exactly one transaction");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),0),sceneBefore,"pose gizmo never changes document transforms");
  tapClipAction(session,font,clip_widget::Undo);
  AE_EXPECT_TRUE(std::abs(session.evaluatedEditorScene().find(owner)->transform.position[0])<1e-6,"gizmo undo restores actual evaluated pose");
  tapClipAction(session,font,clip_widget::PoseAutoKey);
  const float pending[]{.25f,.15f,0};AE_EXPECT_TRUE(session.previewAnimationClipPose(session.screen().clipTrack,pending,error),error.c_str());session.update();
  const auto start=clipActionPosition(session,font,clip_widget::PoseGizmo);
  AE_EXPECT_TRUE(start.x>=0,"pending pose has a reachable gizmo");
  session.handlePointer({18,UiPointerPhase::Down,start,2});session.handlePointer({18,UiPointerPhase::Move,{start.x+45,start.y},2.1});session.update();
  session.handlePointer({18,UiPointerPhase::Cancel,{start.x+45,start.y},2.2});session.update();
  AE_EXPECT_TRUE(session.screen().clipPosePending&&std::abs(session.screen().clipPoseValues[0]-.25f)<1e-6&&std::abs(session.screen().clipPoseValues[1]-.15f)<1e-6,"cancel restores the pre-gesture draft, including another edited component");
  captureClipSurface(session,font,icons,"clip-pose-gizmo.ppm");
  tapClipAction(session,font,clip_widget::PoseCancel);
  tapClipAction(session,font,clip_widget::Layers);tapClipAction(session,font,clip_widget::LayerAdd);
  tapClipAction(session,font,clip_widget::Targets);tapClipAction(session,font,clip_widget::TargetUse);tapClipAction(session,font,clip_widget::PropertyChoice);
  const float raw[]{4,0,0};AE_EXPECT_TRUE(session.previewAnimationClipPose(session.screen().clipTrack,raw,error)&&session.recordAnimationClipPose(error),error.c_str());session.update();
  tapClipAction(session,font,clip_widget::Layers);
  tapClipAction(session,font,clip_widget::LayerWeight);auto field=session.pendingTextEdit();AE_EXPECT_TRUE(session.completeTextEdit(field,"0.5",true),"layer weight");session.update();
  tapClipAction(session,font,clip_widget::LayerClose);
  AE_EXPECT_TRUE(session.evaluatedEditorScene().find(owner)->transform.position[0]==4,"isolated layer shows the actual editable value");
  tapClipAction(session,font,clip_widget::Pose);
  AE_EXPECT_TRUE(session.evaluatedEditorScene().find(owner)->transform.position[0]==2,"leaving Pose restores composition without a source revision change");
  tapClipAction(session,font,clip_widget::Pose);
  AE_EXPECT_TRUE(session.evaluatedEditorScene().find(owner)->transform.position[0]==4,"returning to Pose restores isolated property inspection");
  tapClipAction(session,font,clip_widget::PoseIsolate);
  AE_EXPECT_TRUE(session.evaluatedEditorScene().find(owner)->transform.position[0]==2&&session.screen().clipPoseValues[0]==4,"composition inspection restores the weighted result without rewriting authored values");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),0),sceneBefore,"layer isolation also leaves source scene untouched");
  // A bone/property under a rotated parent must receive the inverse-parent
  // displacement, rather than treating world X as local X.
  const auto parent=session.document().createEntity(session.document().root(),runtime::ObjectKind::Folder,"Pai girado");
  auto parentValues=*session.document().find(parent);parentValues.transform.rotationDegrees[1]=90;
  AE_EXPECT_TRUE(session.document().applyEntityValues(parent,parentValues)&&session.document().reparent(owner,parent,0),"real rotated hierarchy");
  session.update();tapClipAction(session,font,clip_widget::PoseIsolate);tapClipAction(session,font,clip_widget::PoseAutoKey);
  const float eye[]{0,3,-12};session.setCameraPose(eye,0,.2f);session.update();
  const auto rotatedPoint=clipActionPosition(session,font,clip_widget::PoseGizmo);
  AE_EXPECT_TRUE(rotatedPoint.x>=0,"world X handle remains reachable with rotated parent");
  const auto rotatedSource=serializeEditorDocument(session.document(),0);
  const float localX=session.screen().clipPoseValues[0],localZ=session.screen().clipPoseValues[2];
  session.handlePointer({19,UiPointerPhase::Down,rotatedPoint,3});
  session.handlePointer({19,UiPointerPhase::Move,{rotatedPoint.x+35,rotatedPoint.y},3.1});session.update();
  session.handlePointer({19,UiPointerPhase::Up,{rotatedPoint.x+35,rotatedPoint.y},3.2});session.update();
  AE_EXPECT_TRUE(std::abs(session.screen().clipPoseValues[0]-localX)<1e-4&&std::abs(session.screen().clipPoseValues[2]-localZ)>.1f,"world drag is converted into the correct local axis");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),0),rotatedSource,"rotated-parent authoring also preserves source transforms");
}
AE_TEST(animation_clip_layers_mobile_api_runtime_history_and_cold_reopen) {
  using namespace editor;using namespace resources;using namespace ui;
  TemporaryProject project;EditorSession session;UiFont font;UiIconAtlas icons;std::string error;
  const auto fontBytes=productionUiBytes("assets/astra-visual/ui/astra-ui-font.aeuf"),iconBytes=productionUiBytes("assets/astra-visual/ui/astra-ui-icons.aeui");
  AE_EXPECT_TRUE(font.load(fontBytes)&&icons.load(iconBytes),"production graphics assets");
  session.initialize(&font,&icons);session.setSurface({0,0,853,394},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"isolated project");
  const auto owner=session.document().createEntity(session.document().root(),runtime::ObjectKind::Folder,"Mecanismo");
  AssetGuid guid;AE_EXPECT_TRUE(session.createAnimationClip(owner,2,guid,error)&&session.openAnimationClip(guid,owner,error),error.c_str());session.update();
  const auto source=serializeEditorDocument(session.document(),0);
  tapClipAction(session,font,clip_widget::Layers);tapClipAction(session,font,clip_widget::LayerAdd);
  const auto layer=session.screen().clipLayer;AE_EXPECT_TRUE(layer&&session.screen().clipTrack==0,"new layer is empty, not a fake duplicate");
  captureClipSurface(session,font,icons,"clip-layers-empty.ppm");
  tapClipAction(session,font,clip_widget::Targets);tapClipAction(session,font,clip_widget::TargetUse);tapClipAction(session,font,clip_widget::PropertyChoice);
  const auto track=session.screen().clipTrack;AE_EXPECT_TRUE(track&&session.animationClipAsset(guid)->track(track)->layer==layer,"object/property workflow creates a sparse layer channel");
  AE_EXPECT_TRUE(session.seekAnimationClip(1,error),error.c_str());session.update();tapClipAction(session,font,clip_widget::Pose);
  tapClipAction(session,font,clip_widget::PoseValue);auto field=session.pendingTextEdit();
  AE_EXPECT_TRUE(session.completeTextEdit(field,"4",true),"layer pose uses native numeric authoring");session.update();
  tapClipAction(session,font,clip_widget::Layers);tapClipAction(session,font,clip_widget::LayerWeight);field=session.pendingTextEdit();
  AE_EXPECT_TRUE(session.completeTextEdit(field,"0.5",true),"effective layer weight");session.update();
  AE_EXPECT_TRUE(std::abs(session.evaluatedEditorScene().find(owner)->transform.position[0]-2)<1e-5&&session.screen().clipPoseValues[0]==4,"fields stay raw while preview composes");
  tapClipAction(session,font,clip_widget::LayerName);field=session.pendingTextEdit();AE_EXPECT_TRUE(session.completeTextEdit(field,"Correção",true),"rename layer independently of clip");session.update();
  AE_EXPECT_TRUE(session.animationClipAsset(guid)->layer(layer)->name=="Correção","name persisted in layer metadata");
  tapClipAction(session,font,clip_widget::LayerMute);AE_EXPECT_TRUE(session.evaluatedEditorScene().find(owner)->transform.position[0]==0,"mute changes evaluated scene");
  tapClipAction(session,font,clip_widget::LayerMute);tapClipAction(session,font,clip_widget::LayerSolo);
  AE_EXPECT_TRUE(session.animationClipAsset(guid)->layer(layer)->solo,"solo is authored data");tapClipAction(session,font,clip_widget::LayerSolo);
  tapClipAction(session,font,clip_widget::LayerReference);AE_EXPECT_TRUE(session.evaluatedEditorScene().find(owner)->transform.position[0]==0,"reference at playhead removes offset");
  tapClipAction(session,font,clip_widget::LayerReferenceTime);field=session.pendingTextEdit();AE_EXPECT_TRUE(session.completeTextEdit(field,"0",true),"reference time is editable");session.update();
  AE_EXPECT_TRUE(session.evaluatedEditorScene().find(owner)->transform.position[0]==2,"reference at zero uses real layer pose");
  tapClipAction(session,font,clip_widget::LayerDuplicate);const auto duplicate=session.screen().clipLayer;
  AE_EXPECT_TRUE(duplicate!=layer&&session.animationClipAsset(guid)->layers.size()==3,"duplicate owns a distinct layer");
  tapClipAction(session,font,clip_widget::LayerUp);tapClipAction(session,font,clip_widget::LayerDown);
  tapClipAction(session,font,clip_widget::LayerRemove);AE_EXPECT_TRUE(!session.animationClipAsset(guid)->layer(duplicate),"remove retires full layer");
  tapClipAction(session,font,clip_widget::Undo);AE_EXPECT_TRUE(session.animationClipAsset(guid)->layer(duplicate),"undo restores full layer");tapClipAction(session,font,clip_widget::Redo);
  tapClipAction(session,font,clip_widget::LayerNext); // select correction from permanent base
  AE_EXPECT_TRUE(session.screen().clipLayer==layer,"layer navigation reconciles retired IDs after history");
  session.setSurface({0,0,655,300},{});session.update();
  for(const auto action:{clip_widget::LayerName,clip_widget::LayerAdd,clip_widget::LayerDuplicate,clip_widget::LayerRemove,
       clip_widget::LayerBlend,clip_widget::LayerWeight,clip_widget::LayerReference,clip_widget::LayerReferenceTime,
       clip_widget::LayerMute,clip_widget::LayerSolo,clip_widget::LayerCopy,clip_widget::LayerClose})
    AE_EXPECT_TRUE(clipActionPosition(session,font,action).x>=0,"layer actions reachable on a small landscape screen");
  for(u32 component=0;component<3;++component) AE_EXPECT_TRUE(clipActionPosition(session,font,clip_widget::PoseValue+component).x>=0,"XYZ remain reachable in the low landscape pose strip");
  captureClipSurface(session,font,icons,"clip-layers-small.ppm");
  tapClipAction(session,font,clip_widget::LayerChoose);tapClipAction(session,font,clip_widget::LayerChoice+1);
  AE_EXPECT_TRUE(session.screen().clipLayer==layer,"direct layer list uses actual identities");
  tapClipAction(session,font,clip_widget::LayerCopy);AE_EXPECT_TRUE(session.evaluatedEditorScene().find(owner)->transform.position[0]==0,"copying the base replaces real layer keys");tapClipAction(session,font,clip_widget::Undo);
  tapClipAction(session,font,clip_widget::LayerBlend);AE_EXPECT_TRUE(session.animationClipAsset(guid)->layer(layer)->blend==AnimationAuthorBlend::Override,"blend edit changes authoring model");
  AE_EXPECT_TRUE(clipActionPosition(session,font,clip_widget::LayerReference).x<0,"override has no decorative reference control");
  tapClipAction(session,font,clip_widget::LayerBlend);

  AE_EXPECT_EQ(serializeEditorDocument(session.document(),0),source,"all edits leave source scene pose unchanged");
  session.closeAnimationClip();AnimationAuthoringScope scope(session);const auto &api=scope.access();const auto draft=api.begin(api.context,guid,0);
  AE_EXPECT_TRUE(api.version==5&&draft,"layer API remains independent of visible panel in ABI 5");float raw[3],composed[3];
  AE_EXPECT_TRUE(api.sample(api.context,draft,track,1,raw,3)==3&&raw[0]==4&&api.sampleComposed(api.context,draft,track,1,composed,3)==3&&composed[0]==2,"raw/composed API contract");
  AE_EXPECT_TRUE(api.cancel(api.context,draft)==1,"cancel draft");
  runtime::SceneAnimator player;player.begin(session.document(),session.mapScene());runtime::SceneAnimator::ExternalSample sample;sample.owner=owner;sample.clip=guid;sample.time=1;sample.weight=1;
  player.setExternalSamples({sample});AE_EXPECT_TRUE(player.advance(0,{})&&session.document().find(owner)->transform.position[0]==2,"same compiled composition consumed by real runtime");player.reset();
  EditorSession reopened;AE_EXPECT_TRUE(reopened.setProjectDirectory(project.path.string().c_str())&&reopened.loadAssets(session.serializeAssets()),"cold reopening registered layer resource");
  AE_EXPECT_EQ(reopened.animationClipAsset(guid)->serialize(),session.animationClipAsset(guid)->serialize(),"all metadata, keys, IDs and ordering persist");
}
