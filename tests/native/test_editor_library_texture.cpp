#include "harness.h"
#include "editor/editor_session.h"
#include "renderer/authoring_geometry.h"
#include "ui_software_raster.h"
#include "tiff_fixture.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
using namespace ae;
using namespace ae::editor;
namespace {
struct TextureProject {
  std::filesystem::path root=std::filesystem::temp_directory_path()/("astra-library-texture-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  TextureProject(){std::filesystem::create_directories(root);}
  ~TextureProject(){std::error_code error;std::filesystem::remove_all(root,error);}
};
std::vector<u8> tga(u8 green) {
  std::vector<u8> bytes(18);bytes[2]=2;bytes[12]=2;bytes[14]=2;bytes[16]=32;bytes[17]=0x28;
  for(u32 i=0;i<4;++i)bytes.insert(bytes.end(),{255,green,128,static_cast<u8>(i?255:0)});
  return bytes;
}
struct TexturePublisher {
  bool failNext=false;
  std::vector<u8> vertices;std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  std::vector<renderer::SharedAuthoringTexture> textures;
  void attach(EditorSession &session) {
    renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials);
    session.importMap(draws,materials,false,vertices,indices,0);
    session.setGeometryPublisher([this](std::span<const u8> v,std::span<const u32> i,std::span<const renderer::MapDrawRecord> d,
        std::span<const renderer::MapMaterialRecord> m,std::span<const renderer::SharedAuthoringTexture> t,EditorSession::PublishedGeometry &out) {
      if(std::exchange(failNext,false))return false;
      for(const auto &texture:t)if(!texture||!texture->valid())return false;
      textures.assign(t.begin(),t.end());vertices.clear();indices.clear();draws.clear();materials.clear();
      if(!renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials))return false;
      const auto vertexBase=static_cast<u32>(vertices.size()/renderer::MapVertexStride),indexBase=static_cast<u32>(indices.size()),materialBase=static_cast<u32>(materials.size());
      vertices.insert(vertices.end(),v.begin(),v.end());indices.insert(indices.end(),i.begin(),i.end());materials.insert(materials.end(),m.begin(),m.end());
      for(auto draw:d){draw.firstIndex+=indexBase;draw.vertexOffset+=vertexBase;draw.materialIndex+=materialBase;draw.lodGroupId=static_cast<u32>(draws.size());draws.push_back(draw);}
      out={draws,materials,vertices,indices};return true;
    });
  }
};
}
AE_TEST(library_tga_texture_route_recipe_reimport_undo_and_cold_material_consumer) {
  TextureProject project;EditorSession session;TexturePublisher publisher;publisher.attach(session);
  std::vector<u8> fontBytes,atlasBytes;ui::UiFont font;ui::UiIconAtlas atlas;
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-font.aeuf",fontBytes)&&font.load(fontBytes),"owned native font");
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-icons.aeui",atlasBytes)&&atlas.load(atlasBytes),"owned native atlas");
  session.initialize(&font,&atlas);session.setSurface({0,0,655,300},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()),"open project before worker staging");
  const std::string staging=".astra/import-staging/texture";
  std::filesystem::create_directories(project.root/staging);
  const auto original=tga(128);
  AE_EXPECT_TRUE(EditorImportTransaction::write(project.root/staging/"Normal.TGA",original),"original TGA source");
  auto package=prepareLibraryPackage(project.root.string(),staging,"TGA original",{{"Normal.TGA",Sha256::hex(original),original.size(),{}}});
  AE_EXPECT_TRUE(package.accepted,package.diagnostic.c_str());std::string error;
  AE_EXPECT_TRUE(session.importLibraryPackage(package,error),error.c_str());
  const auto item=package.package.items.front();
  AE_EXPECT_TRUE(session.openLibraryItem(item.id,error),error.c_str());
  AE_EXPECT_EQ(session.takeTextureReimportPath(),item.path,"library opens real platform preparation route even before native registration");
  resources::TextureProfile profile;profile.interpretation=resources::TextureInterpretationNormal;
  profile.invertNormalGreen=true;
  resources::TextureImportLimits limits;limits.image=session.importLimits().image;
  limits.projectMaximumDimension=session.importLimits().maximumTextureDimension;
  resources::PreparedTextureImport prepared;
  AE_EXPECT_TRUE(resources::prepareTextureImport(original,profile,true,EditorMapScene::DefaultTextureSampler,limits,nullptr,prepared),prepared.diagnostic.c_str());
  AE_EXPECT_TRUE(prepared.texture->levels==2&&!prepared.texture->srgb,"normal consumer uses linear mip chain");
  session.beginTextureImportPreparation(item.path,profile);
  session.showTextureImportPreview(item.path,prepared,profile);
  AE_EXPECT_TRUE(session.screen().importTexture&&session.screen().importReady,"existing editable importer UI receives decoded TGA");
  if(const auto *path=std::getenv("ASTRA_TGA_CAPTURE")) {
    session.update();ui::UiDrawList list;ui::UiInputRouter router;
    list.begin(session.screen().surface,font.metrics(ui::UiFontWeight::Regular));router.beginFrame();buildEditorScreen(session.screen(),ui::defaultTheme(),list,router);
    std::vector<ui::UiInstance> instances;const auto built=ui::buildUiInstances(list,font,atlas,16384,instances);
    AE_EXPECT_TRUE(!built.dropped&&!built.missingGlyphRuns,"native TGA importer capture");
    test::UiSoftwareTarget target;target.resize(655,300,.12f,.13f,.14f);
    const auto *preview=session.takePreviewAtlas();
    AE_EXPECT_TRUE(preview!=nullptr,"actual decoded preview atlas");
    for(auto instance:instances) {
      if(static_cast<u32>(instance.params[2]+.5f)==static_cast<u32>(ui::UiInstanceKind::Preview)) {
        instance.params[2]=static_cast<float>(ui::UiInstanceKind::GuiImage);
        test::rasterizeUi({&instance,1},font,atlas,target,{},0,0,*preview,TexturePreviewAtlasSize);
      } else test::rasterizeUi({&instance,1},font,atlas,target);
    }
    auto *output=std::fopen(path,"wb");AE_EXPECT_TRUE(output!=nullptr,"capture output");std::fprintf(output,"P6\n655 300\n255\n");
    for(usize i=0;i<target.pixels.size();i+=4)for(u32 c=0;c<3;++c)std::fputc(static_cast<u8>(std::clamp(target.pixels[i+c],0.f,1.f)*255+.5f),output);
    AE_EXPECT_TRUE(std::fclose(output)==0,"capture flushed");
  }
  AE_EXPECT_TRUE(session.commitTextureImport(item.path,original,Sha256::hex(original),prepared,profile,error),error.c_str());
  const auto guid=session.assets().findByPath(item.path)->guid;
  AE_EXPECT_EQ(session.library().resolve(item.id,session.assets()).state,resources::LibraryResolutionState::Registered,"texture and library index published together");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"undo first conversion");
  AE_EXPECT_TRUE(!session.assets().find(guid)&&std::filesystem::exists(project.root/item.path),"source preserved when native resource detached");
  AE_EXPECT_EQ(session.library().resolve(item.id,session.assets()).state,resources::LibraryResolutionState::Pending,"index rolled back too");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"redo conversion");
  auto changedProfile=profile;changedProfile.invertNormalGreen=false;
  AE_EXPECT_TRUE(session.setTextureProfile(guid,changedProfile,error),error.c_str());
  const auto *catalog=session.library().item(item.id);
  AE_EXPECT_TRUE(catalog&&catalog->recipe==resources::serializeTextureProfile(changedProfile),"edited recipe reflected in same index transaction");
  AE_EXPECT_TRUE(session.history().undo(session.document())&&session.textureProfileFor(guid).invertNormalGreen,"profile Undo restores effective normal convention");
  const auto live=session.history().createEntity(session.document(),session.document().root(),EditorEntityKind::Mesh,"Live TGA");
  auto liveValue=*session.document().find(live);
  auto *liveRenderer=static_cast<scene::MeshRenderer *>(liveValue.components.add(scene::MeshRenderer::descriptor));
  AE_EXPECT_TRUE(liveRenderer!=nullptr,"live real mesh renderer");liveRenderer->mesh=1;
  AE_EXPECT_TRUE(session.document().applyEntityValues(live,liveValue)&&session.setSlotTexture(live,0,1,EditorSession::MaterialScope::Instance,guid,error),error.c_str());
  AE_EXPECT_TRUE(publisher.textures.size()==1&&publisher.textures.front()->mipChain[1]==127,"old live normal texels");
  const auto changed=tga(64);resources::PreparedTextureImport newPrepared;
  AE_EXPECT_TRUE(resources::prepareTextureImport(changed,profile,true,EditorMapScene::DefaultTextureSampler,limits,nullptr,newPrepared),newPrepared.diagnostic.c_str());
  AE_EXPECT_TRUE(session.commitTextureImport(item.path,changed,Sha256::hex(original),newPrepared,profile,error),error.c_str());
  AE_EXPECT_EQ(session.assets().find(guid)->contentHash,Sha256::hex(changed),"reimport keeps GUID with new source");
  AE_EXPECT_EQ(publisher.textures.front()->mipChain[1],191,"same GUID republishes new texels before committing source");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"reimport Undo restores encoded source and recipe");
  std::vector<u8> disk;
  AE_EXPECT_TRUE(EditorImportTransaction::read(project.root/item.path,disk)&&disk==original,"old TGA bytes restored from immutable authoring snapshot");
  AE_EXPECT_EQ(publisher.textures.front()->mipChain[1],127,"Undo republishes old effective texels");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"reimport Redo");
  AE_EXPECT_TRUE(EditorImportTransaction::read(project.root/item.path,disk)&&disk==changed,"new encoded source restored");
  AE_EXPECT_EQ(publisher.textures.front()->mipChain[1],191,"Redo republishes new effective texels");
  const auto stableRegistry=session.serializeAssets(),stableIndex=session.library().serialize();
  const auto rejected=tga(80);resources::PreparedTextureImport rejectedPrepared;
  AE_EXPECT_TRUE(resources::prepareTextureImport(rejected,profile,true,EditorMapScene::DefaultTextureSampler,limits,nullptr,rejectedPrepared),rejectedPrepared.diagnostic.c_str());
  publisher.failNext=true;
  AE_EXPECT_TRUE(!session.commitTextureImport(item.path,rejected,Sha256::hex(changed),rejectedPrepared,profile,error),"GPU publication failure refuses entire reimport");
  AE_EXPECT_TRUE(session.serializeAssets()==stableRegistry&&session.library().serialize()==stableIndex&&
    EditorImportTransaction::read(project.root/item.path,disk)&&disk==changed&&publisher.textures.front()->mipChain[1]==191,"failed publication restores source registry index and effective GPU candidate");
  EditorSession reopened;TexturePublisher reopenedPublisher;reopenedPublisher.attach(reopened);
  AE_EXPECT_TRUE(reopened.setProjectDirectory(project.root.string().c_str())&&reopened.loadAssets(session.serializeAssets()),"cold registry/library reopen");
  AE_EXPECT_EQ(reopened.library().resolve(item.id,reopened.assets()).state,resources::LibraryResolutionState::Registered,"cold stable texture identity");
  const auto object=reopened.history().createEntity(reopened.document(),reopened.document().root(),EditorEntityKind::Mesh,"Cubo TGA");
  auto value=*reopened.document().find(object);
  auto *render=static_cast<scene::MeshRenderer *>(value.components.add(scene::MeshRenderer::descriptor));
  AE_EXPECT_TRUE(render!=nullptr,"real primitive mesh renderer");render->mesh=1;
  AE_EXPECT_TRUE(reopened.document().applyEntityValues(object,value)&&reopened.setSlotTexture(object,0,1,EditorSession::MaterialScope::Instance,guid,error),error.c_str());
  AE_EXPECT_TRUE(reopenedPublisher.textures.size()==1&&reopenedPublisher.textures.front()->valid()&&!reopenedPublisher.textures.front()->srgb,"cold TGA binds into actual material publication as a linear normal");
  AE_EXPECT_TRUE(reopenedPublisher.textures.front()->mipChain[1]>128,"persisted green inversion affects the published texels");
  const auto registryBefore=session.serializeAssets();
  auto external=session.assets();
  AE_EXPECT_TRUE(external.publishImport(guid,Sha256::hex(changed),resources::TextureAssetImporterRevision,resources::serializeTextureProfile(changedProfile),{},{}),"simulate external metadata edit");
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(project.root/".astra/assets.astra",external.serialize()),"external recipe on disk");
  AE_EXPECT_TRUE(!session.history().undo(session.document())&&session.serializeAssets()==registryBefore,"Undo refuses changed metadata without partial source/index mutation");
  AE_EXPECT_TRUE(EditorImportTransaction::read(project.root/item.path,disk)&&disk==changed,"refused Undo leaves source intact");
}

namespace {
bool textureTap(EditorSession &session,const ui::UiFont &font,EditorWidget widget) {
  session.update();ui::UiDrawList list;ui::UiInputRouter router;
  list.begin(session.screen().surface,font.metrics(ui::UiFontWeight::Regular));router.beginFrame();
  buildEditorScreen(session.screen(),ui::defaultTheme(),list,router);
  ui::UiPoint point{-1,-1};
  for(float y=2;y<300&&point.x<0;y+=3)for(float x=2;x<655;x+=3) {
    const auto hit=router.hitTest({x,y});
    if(hit.target==ui::UiPointerTarget::Widget&&hit.widgetId==widgetId(widget)){point={x,y};break;}
  }
  if(point.x<0)return false;
  session.handlePointer({0,ui::UiPointerPhase::Down,point,1});session.handlePointer({0,ui::UiPointerPhase::Up,point,1.1});
  session.update();return true;
}
void captureTiff(EditorSession &session,const ui::UiFont &font,const ui::UiIconAtlas &atlas,std::vector<u8> &retainedPreview,const char *suffix) {
  if(const auto *path=std::getenv("ASTRA_TIFF_CAPTURE")) {
    session.update();test::UiSoftwareTarget target;target.resize(655,300,.08f,.09f,.11f);
    const auto *preview=session.takePreviewAtlas();
    if(preview)retainedPreview=*preview;
    for(auto instance:session.instances()) {
      if(static_cast<u32>(instance.params[2]+.5f)==static_cast<u32>(ui::UiInstanceKind::Preview)) {
        AE_EXPECT_TRUE(!retainedPreview.empty(),"TIFF capture owns a decoded preview atlas");
        instance.params[2]=static_cast<float>(ui::UiInstanceKind::GuiImage);
        test::rasterizeUi({&instance,1},font,atlas,target,{},0,0,retainedPreview,TexturePreviewAtlasSize);
      } else test::rasterizeUi({&instance,1},font,atlas,target);
    }
    const auto name=std::string(path)+suffix;auto *file=std::fopen(name.c_str(),"wb");
    AE_EXPECT_TRUE(file,"TIFF capture output");std::fprintf(file,"P6\n655 300\n255\n");
    for(usize i=0;i<target.pixels.size();i+=4)for(u32 c=0;c<3;++c)
      std::fputc(static_cast<u8>(std::clamp(target.pixels[i+c],0.f,1.f)*255+.5f),file);
    AE_EXPECT_TRUE(std::fclose(file)==0,"TIFF capture flushed");
  }
}
}
AE_TEST(library_tiff_source_choices_live_publication_history_and_cold_play) {
  std::vector<u8> retainedPreview;
  TextureProject project;EditorSession session;TexturePublisher publisher;publisher.attach(session);
  std::vector<u8> fontBytes,atlasBytes;ui::UiFont font;ui::UiIconAtlas atlas;
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-font.aeuf",fontBytes)&&font.load(fontBytes),"owned font");
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-icons.aeui",atlasBytes)&&atlas.load(atlasBytes),"owned icons");
  session.initialize(&font,&atlas);session.setSurface({0,0,655,300},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()),"project open");
  const std::string staging=".astra/import-staging/tiff";std::filesystem::create_directories(project.root/staging);
  const auto original=test::tiffFixture(false,16,6);
  AE_EXPECT_TRUE(EditorImportTransaction::write(project.root/staging/"Source.TIFF",original),"original 16-bit TIFF source");
  auto package=prepareLibraryPackage(project.root.string(),staging,"TIFF original",{{"Source.TIFF",Sha256::hex(original),original.size(),{}}});
  std::string error;AE_EXPECT_TRUE(package.accepted&&session.importLibraryPackage(package,error),error.c_str());
  const auto item=package.package.items.front();
  AE_EXPECT_TRUE(session.openLibraryItem(item.id,error)&&session.takeTextureReimportPath()==item.path,"TIFF library opens existing Android/host worker route");
  resources::TextureProfile profile;resources::TextureImportLimits limits;limits.image=session.importLimits().image;
  limits.projectMaximumDimension=session.importLimits().maximumTextureDimension;
  resources::PreparedTextureImport prepared;
  AE_EXPECT_TRUE(!resources::prepareTextureImport(original,profile,true,EditorMapScene::DefaultTextureSampler,limits,nullptr,prepared),"undefined extra defaults to explicit refusal");
  session.beginTextureImportPreparation(item.path,profile);session.showTextureImportPreview(item.path,prepared,profile);
  AE_EXPECT_TRUE(session.screen().importError&&session.screen().textureImportTiff&&session.screen().importPage==5,"refusal focuses recoverable source choices");
  AE_EXPECT_TRUE(!textureTap(session,font,EditorWidget::ImportAccept),"unprepared TIFF cannot publish through UI");
  captureTiff(session,font,atlas,retainedPreview,".refused.ppm");
  for(u32 step=0;step<2;++step) {
    AE_EXPECT_TRUE(textureTap(session,font,EditorWidget::TextureTiffExtra)&&session.takeTextureImportReprepare(profile),"actual touch control requests worker reprepare");
    AE_EXPECT_TRUE(!resources::prepareTextureImport(original,profile,true,EditorMapScene::DefaultTextureSampler,limits,nullptr,prepared),"16-bit precision still requires its own choice");
    session.showTextureImportPreview(item.path,prepared,profile);
    AE_EXPECT_EQ(session.screen().importPage,5u,"source page persists across failed preparation");
  }
  AE_EXPECT_TRUE(profile.tiffUndefinedExtra==2&&textureTap(session,font,EditorWidget::TextureTiffPrecision)&&session.takeTextureImportReprepare(profile),"alpha and quantization chosen in engine UI");
  AE_EXPECT_TRUE(resources::prepareTextureImport(original,profile,true,EditorMapScene::DefaultTextureSampler,limits,nullptr,prepared),prepared.diagnostic.c_str());
  AE_EXPECT_TRUE(prepared.sourceWidth==1&&prepared.sourceHeight==2&&prepared.knownLosses.size()==2&&prepared.texture->mipChain[3]==64,"source orientation, precision, alpha and losses consumed");
  session.showTextureImportPreview(item.path,prepared,profile);captureTiff(session,font,atlas,retainedPreview,".ready.ppm");
  AE_EXPECT_TRUE(textureTap(session,font,EditorWidget::ImportNextPage)&&textureTap(session,font,EditorWidget::TextureTiffLosses)&&session.screen().importPage==7,"full conversion losses are reachable before publication");
  captureTiff(session,font,atlas,retainedPreview,".losses.ppm");
  AE_EXPECT_TRUE(textureTap(session,font,EditorWidget::ImportAccept)&&session.takeImportAccept(),"ready source accepted by actual importer action");
  session.closeImportPreview();
  AE_EXPECT_TRUE(session.commitTextureImport(item.path,original,Sha256::hex(original),prepared,profile,error),error.c_str());
  const auto guid=session.assets().findByPath(item.path)->guid;
  AE_EXPECT_TRUE(session.library().item(item.id)->knownLosses.size()==2&&session.library().item(item.id)->converter=="astra-tiff-classic","conversion choices and losses published with source and registry");
  AE_EXPECT_TRUE(session.history().undo(session.document())&&!session.assets().find(guid)&&session.library().item(item.id)->knownLosses.empty(),"first import Undo detaches output and conversion losses but preserves source");
  AE_EXPECT_TRUE(session.history().redo(session.document())&&session.library().item(item.id)->knownLosses.size()==2,"Redo restores effective source interpretation");
  const auto live=session.history().createEntity(session.document(),session.document().root(),EditorEntityKind::Mesh,"TIFF live");
  auto value=*session.document().find(live);auto *render=static_cast<scene::MeshRenderer *>(value.components.add(scene::MeshRenderer::descriptor));
  AE_EXPECT_TRUE(render,"real mesh renderer");render->mesh=1;
  AE_EXPECT_TRUE(session.document().applyEntityValues(live,value)&&session.setSlotTexture(live,0,1,EditorSession::MaterialScope::Instance,guid,error),error.c_str());
  AE_EXPECT_TRUE(publisher.textures.size()==1&&publisher.textures.front()->mipChain[3]==64,"actual material publication consumes authored TIFF alpha");
  auto opaque=profile;opaque.tiffUndefinedExtra=1;
  AE_EXPECT_TRUE(session.setTextureProfile(guid,opaque,error)&&publisher.textures.front()->mipChain[3]==255,"recipe edit changes effective material texels");
  AE_EXPECT_TRUE(session.library().item(item.id)->knownLosses.front().find("ignorado")!=std::string::npos,"recipe edit updates indexed loss in same transaction");
  AE_EXPECT_TRUE(session.history().undo(session.document())&&publisher.textures.front()->mipChain[3]==64&&session.library().item(item.id)->knownLosses.front().find("alfa")!=std::string::npos,"recipe Undo restores both texels and conversion loss");
  auto changed=original;const auto pixels=changed.size()-16;changed[pixels]=changed[pixels+1]=128;
  resources::PreparedTextureImport candidate;
  AE_EXPECT_TRUE(resources::prepareTextureImport(changed,profile,true,EditorMapScene::DefaultTextureSampler,limits,nullptr,candidate),candidate.diagnostic.c_str());
  AE_EXPECT_TRUE(session.commitTextureImport(item.path,changed,Sha256::hex(original),candidate,profile,error)&&publisher.textures.front()->mipChain[0]==128,error.c_str());
  AE_EXPECT_TRUE(session.history().undo(session.document())&&publisher.textures.front()->mipChain[0]==255,"reimport Undo republishes original source pixels");
  AE_EXPECT_TRUE(session.history().redo(session.document())&&publisher.textures.front()->mipChain[0]==128,"reimport Redo republishes candidate pixels");
  const auto stableRegistry=session.serializeAssets(),stableIndex=session.library().serialize();const auto depth=session.history().undoDepth();
  publisher.failNext=true;
  AE_EXPECT_TRUE(!session.commitTextureImport(item.path,original,Sha256::hex(changed),prepared,profile,error),"GPU publication failure refuses entire TIFF operation");
  std::vector<u8> disk;
  AE_EXPECT_TRUE(session.serializeAssets()==stableRegistry&&session.library().serialize()==stableIndex&&session.history().undoDepth()==depth&&EditorImportTransaction::read(project.root/item.path,disk)&&disk==changed&&publisher.textures.front()->mipChain[0]==128,"failed publication preserves source recipe loss index history and material consumer");
  const auto archive=(project.root/"editor.aescene").string();AE_EXPECT_TRUE(session.save(archive.c_str(),0),"authored TIFF binding saved");
  EditorSession cold;TexturePublisher coldPublisher;coldPublisher.attach(cold);cold.initialize(&font,&atlas);
  AE_EXPECT_TRUE(cold.setProjectDirectory(project.root.string().c_str())&&cold.loadAssets(session.serializeAssets())&&cold.load(archive.c_str(),0),"cold source recipe and real scene binding reopened");
  AE_EXPECT_TRUE(cold.textureProfileFor(guid).tiffUndefinedExtra==2&&cold.textureProfileFor(guid).tiffQuantize16&&cold.startPlay(),"persisted interpretation enters real Play");
  cold.advanceClock(.016f);std::vector<renderer::MapDrawState> draws;
  AE_EXPECT_TRUE(cold.extractPlayMap(draws)&&!draws.empty()&&coldPublisher.textures.size()==1&&coldPublisher.textures.front()->mipChain[0]==128&&coldPublisher.textures.front()->mipChain[3]==64,"cold Play publishes quantized TIFF and real draw");
  auto external=session.assets();auto externalProfile=profile;externalProfile.tiffIgnoreColorProfile=true;
  AE_EXPECT_TRUE(external.publishImport(guid,Sha256::hex(changed),resources::TextureAssetImporterRevision,resources::serializeTextureProfile(externalProfile),{},{}),"external metadata fixture");
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(project.root/".astra/assets.astra",external.serialize()),"external recipe persisted");
  AE_EXPECT_TRUE(!session.history().undo(session.document())&&session.serializeAssets()==stableRegistry&&session.history().undoDepth()==depth,"Undo refuses external recipe without partial source/index mutation");
}
