#include "editor/editor_screen.h"
#include "harness.h"
#include "editor/editor_session.h"
#include "resources/unity_material_import.h"
#include "ui_software_raster.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
using namespace ae;
using namespace ae::editor;
namespace {
struct PackageProject {
  std::filesystem::path path=std::filesystem::temp_directory_path()/("astra-library-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::string staging=".astra/import-staging/package";
  std::vector<LibrarySourceFile> files;
  PackageProject(){std::filesystem::create_directories(path/staging);}
  ~PackageProject(){std::error_code error;std::filesystem::remove_all(path,error);}
  bool add(const std::string &relative,const std::string &text) {
    std::filesystem::create_directories((path/staging/relative).parent_path());
    if(!EditorImportTransaction::writeText(path/staging/relative,text))return false;
    files.push_back({relative,Sha256::hex({reinterpret_cast<const u8*>(text.data()),text.size()}),text.size(),{}});return true;
  }
  PreparedLibraryPackage prepare(){return prepareLibraryPackage(path.string(),staging,"Pacote de autoria",files);}
};
bool capture(const char *path,const ui::UiDrawList &list,ui::UiFont &font,ui::UiIconAtlas &atlas) {
  std::vector<ui::UiInstance> instances;const auto built=ui::buildUiInstances(list,font,atlas,16384,instances);
  if(built.dropped||built.missingGlyphRuns)return false;
  test::UiSoftwareTarget target;target.resize(655,300,.12f,.13f,.14f);test::rasterizeUi(instances,font,atlas,target);
  auto *output=std::fopen(path,"wb");if(!output)return false;std::fprintf(output,"P6\n655 300\n255\n");
  for(usize i=0;i<target.pixels.size();i+=4)for(u32 c=0;c<3;++c)std::fputc(static_cast<u8>(std::clamp(target.pixels[i+c],0.f,1.f)*255+.5f),output);
  return std::fclose(output)==0;
}
}
AE_TEST(library_package_editor_import_convert_undo_cold_reload_and_mobile_route) {
  PackageProject project;
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"open project before preparing sources");
  AE_EXPECT_TRUE(project.add("Materials/Paint.mat","%YAML 1.1\nMaterial:\n  m_Name: Paint\n  m_Shader: {fileID: 46, guid: 0000000000000000f000000000000000, type: 0}\n  m_SavedProperties:\n    m_Floats:\n    - _Metallic: 0.7\n"),"material source");
  AE_EXPECT_TRUE(project.add("Materials/Paint.mat.meta","fileFormatVersion: 2\nguid: 11111111111111111111111111111111\n"),"original Unity identity");
  AE_EXPECT_TRUE(project.add("Scripts/Original.cs","// Preserved source; not compiled as an Astra component.\n"),"source code");
  AE_EXPECT_TRUE(project.add("LICENSE.txt","Original license notice\n"),"original notice");
  const auto prepared=project.prepare();AE_EXPECT_TRUE(prepared.accepted,prepared.diagnostic.c_str());
  AE_EXPECT_EQ(prepared.package.items.size(),3u,"meta preserved as a source file, not a second asset");
  std::vector<u8> fontBytes,atlasBytes;ui::UiFont font;ui::UiIconAtlas atlas;
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-font.aeuf",fontBytes)&&font.load(fontBytes),"real font with owned backing");
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-icons.aeui",atlasBytes)&&atlas.load(atlasBytes),"real icons with owned backing");
  session.initialize(&font,&atlas);session.setSurface({0,0,655,300},{});
  std::string error;AE_EXPECT_TRUE(session.importLibraryPackage(prepared,error),error.c_str());
  AE_EXPECT_EQ(session.library().packages().size(),1u,"catalog adopted after journal commit");
  for(const auto &file:prepared.files) {std::vector<u8> bytes;const auto target=project.path/"Biblioteca"/prepared.package.id.text()/file.relative;
    AE_EXPECT_TRUE(EditorImportTransaction::read(target,bytes)&&Sha256::hex(bytes)==file.sha256,"all source bytes including license/meta preserved");}
  AE_EXPECT_TRUE(!std::filesystem::exists(project.path/".astra/import-transaction/journal"),"committed package has no pending journal");
  session.update();ui::UiDrawList list;ui::UiInputRouter router;
  list.begin(session.screen().surface,font.metrics(ui::UiFontWeight::Regular));router.beginFrame();buildEditorScreen(session.screen(),ui::defaultTheme(),list,router);
  ui::UiRect material{};
  for(const auto &command:list.commands())if(command.kind==ui::UiPrimitive::Text&&list.textOf(command)=="Paint.mat")material=command.bounds;
  AE_EXPECT_TRUE(material.width>0,"package item discoverable at 655 by 300");
  if(const auto *path=std::getenv("ASTRA_LIBRARY_CAPTURE"))AE_EXPECT_TRUE(capture(path,list,font,atlas),"real native browser capture");
  const ui::UiPoint point{material.x+material.width*.5f,material.y+material.height*.5f};
  session.handlePointer({151,ui::UiPointerPhase::Down,point,1});session.handlePointer({151,ui::UiPointerPhase::Up,point,1.05});session.update();
  resources::AssetGuid item{};for(const auto &entry:prepared.package.items)if(entry.kind==resources::LibraryItemKind::Material)item=entry.id;const auto resolved=session.library().resolve(item,session.assets());
  AE_EXPECT_EQ(resolved.state,resources::LibraryResolutionState::Registered,"touch conversion atomically registers material and updates index");
  AE_EXPECT_TRUE(resolved.output&&session.findMaterialAsset(resolved.output->guid),"real inspector resource available");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"undo conversion");
  AE_EXPECT_EQ(session.library().resolve(item,session.assets()).state,resources::LibraryResolutionState::Pending,"undo restores source-only catalog state");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"redo conversion");
  AE_EXPECT_EQ(session.library().resolve(item,session.assets()).state,resources::LibraryResolutionState::Registered,"redo restores catalog and resource together");
  EditorSession reopened;AE_EXPECT_TRUE(reopened.setProjectDirectory(project.path.string().c_str()),"cold project opening loads library index");
  AE_EXPECT_TRUE(reopened.loadAssets(session.serializeAssets()),"cold native registry load");
  AE_EXPECT_EQ(reopened.library().resolve(item,reopened.assets()).state,resources::LibraryResolutionState::Registered,"cold library resolves actual native GUID");
  AE_EXPECT_TRUE(session.history().undo(session.document())&&session.history().undo(session.document()),"undo conversion then detach package");
  AE_EXPECT_TRUE(session.library().packages().empty()&&std::filesystem::exists(project.path/prepared.package.items.front().path),"detaching preserves original sources");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"package can be reattached from history");
  const auto altered=session.library().serialize()+"\n";AE_EXPECT_TRUE(EditorImportTransaction::writeText(project.path/LibraryIndexPath,altered),"external index edit");
  AE_EXPECT_TRUE(!session.history().redo(session.document()),"resource redo refuses externally changed index");
  AE_EXPECT_TRUE(session.library().resolve(item,session.assets()).state==resources::LibraryResolutionState::Pending,"failed redo leaves native and catalog states unchanged");
}
// G3a: relatório por pacote. Três materiais (completo, aproximado, sem
// conversão) aparecem como três blocos; tocar num bloco filtra a grade.
AE_TEST(library_package_material_audit_card_counts_and_filters_by_class) {
  PackageProject project;
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"open project");
  const std::string header="%YAML 1.1\nMaterial:\n  m_Shader: {fileID: 46, guid: 0000000000000000f000000000000000, type: 0}\n";
  AE_EXPECT_TRUE(project.add("Metal.mat",header+"  m_Name: Metal\n  m_ShaderKeywords: _METALLICGLOSSMAP\n  m_SavedProperties:\n    m_TexEnvs:\n    - _MetallicGlossMap:\n"
    "        m_Texture: {fileID: 2800000, guid: 11111111111111111111111111111111, type: 3}\n    m_Floats:\n    - _GlossMapScale: 0.7\n"),"complete");
  AE_EXPECT_TRUE(project.add("Glass.mat",header+"  m_Name: Glass\n  m_ShaderKeywords: _SPECULARHIGHLIGHTS_OFF\n  m_SavedProperties:\n    m_Floats:\n    - _SpecularHighlights: 0\n"),"approximated");
  AE_EXPECT_TRUE(project.add("Rock.mat",header+"  m_Name: Rock\n  m_ShaderKeywords: _PARALLAXMAP\n  m_SavedProperties:\n    m_Floats:\n    - _Parallax: 0.02\n"),"unsupported");
  AE_EXPECT_TRUE(project.add("Notes.txt","not a material\n"),"other item");
  const auto prepared=project.prepare();AE_EXPECT_TRUE(prepared.accepted,prepared.diagnostic.c_str());
  std::vector<u8> fontBytes,atlasBytes;ui::UiFont font;ui::UiIconAtlas atlas;
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-font.aeuf",fontBytes)&&font.load(fontBytes),"font");
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-icons.aeui",atlasBytes)&&atlas.load(atlasBytes),"icons");
  session.initialize(&font,&atlas);session.setSurface({0,0,655,300},{});
  std::string error;AE_EXPECT_TRUE(session.importLibraryPackage(prepared,error),error.c_str());
  const auto &state=session.screen();
  AE_EXPECT_TRUE(state.libraryMaterialAudit[0]==1&&state.libraryMaterialAudit[1]==1&&state.libraryMaterialAudit[2]==1,"one material per class");
  session.update();ui::UiDrawList list;ui::UiInputRouter router;
  list.begin(session.screen().surface,font.metrics(ui::UiFontWeight::Regular));router.beginFrame();buildEditorScreen(session.screen(),ui::defaultTheme(),list,router);
  ui::UiRect tile{};bool title=false;
  for(const auto &command:list.commands())if(command.kind==ui::UiPrimitive::Text) {
    if(list.textOf(command)=="Aproximados")tile=command.bounds;
    if(list.textOf(command)=="Materiais Unity · 3")title=true;
  }
  AE_EXPECT_TRUE(title&&tile.width>0,"audit card visible right after the package import");
  if(const auto *path=std::getenv("ASTRA_LIBRARY_AUDIT_CAPTURE"))AE_EXPECT_TRUE(capture(path,list,font,atlas),"audit card capture");
  const ui::UiPoint point{tile.x+tile.width*.5f,tile.y+tile.height*.5f};
  session.handlePointer({152,ui::UiPointerPhase::Down,point,1});session.handlePointer({152,ui::UiPointerPhase::Up,point,1.05});session.update();
  AE_EXPECT_EQ(state.libraryAuditFilter,EditorScreenState::LibraryAuditApproximated,"tile filters by class");
  AE_EXPECT_TRUE(state.libraryRows.size()==1&&state.libraryRows[0].name=="Glass.mat"&&state.libraryRows[0].audit==EditorScreenState::LibraryAuditApproximated,"grid shows only the approximated material");
  resources::AssetGuid glass{};for(const auto &entry:prepared.package.items)if(entry.path.ends_with("Glass.mat"))glass=entry.id;
  const auto *item=session.library().item(glass);
  AE_EXPECT_TRUE(item&&item->conversion!=resources::LibraryConversionState::Unsupported&&item->knownLosses.size()==1&&
    item->knownLosses[0].starts_with(resources::UnityMaterialApproximationPrefix),"approximation recorded with its reason");
  session.handlePointer({153,ui::UiPointerPhase::Down,point,2});session.handlePointer({153,ui::UiPointerPhase::Up,point,2.05});session.update();
  AE_EXPECT_EQ(state.libraryAuditFilter,EditorScreenState::LibraryAuditNone,"same tile clears the filter");
}
AE_TEST(library_package_rejects_changed_staging_and_recovers_metadata_with_sources) {
  PackageProject project;EditorSession session;
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"open project before preparing sources");
  AE_EXPECT_TRUE(project.add("README.txt","Original notice"),"source");
  const auto prepared=project.prepare();AE_EXPECT_TRUE(prepared.accepted,prepared.diagnostic.c_str());
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(project.path/project.staging/"README.txt","Changed source payload"),"changed private staging");
  std::string error;AE_EXPECT_TRUE(!session.importLibraryPackage(prepared,error),"prepared hash cannot authorize altered staging");
  AE_EXPECT_TRUE(session.library().packages().empty()&&!std::filesystem::exists(project.path/prepared.manifestPath),"refusal publishes nothing");
  const auto root=project.path.string();std::filesystem::create_directories(project.path/".astra/imports");
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(project.path/"existing.material","before")&&
    EditorImportTransaction::writeText(project.path/LibraryIndexPath,"old index")&&
    EditorImportTransaction::writeText(project.path/".astra/assets.astra","old registry"),"prior files");
  EditorImportTransaction transaction(root);
  AE_EXPECT_TRUE(transaction.begin("existing.material",Sha256::hex({reinterpret_cast<const u8*>("before"),6}),error,LibraryIndexPath),"prepared shared journal");
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(project.path/"existing.material","partial")&&
    EditorImportTransaction::writeText(project.path/LibraryIndexPath,"partial index")&&
    EditorImportTransaction::writeText(project.path/".astra/assets.astra","partial registry"),"simulate interrupted writes before committed marker");
  AE_EXPECT_TRUE(EditorImportTransaction::recover(root,error),error.c_str());
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(EditorImportTransaction::read(project.path/LibraryIndexPath,bytes)&&std::string(bytes.begin(),bytes.end())=="old index","index restored by existing journal recovery");
  AE_EXPECT_TRUE(EditorImportTransaction::read(project.path/"existing.material",bytes)&&std::string(bytes.begin(),bytes.end())=="before","native resource restored too");
  AE_EXPECT_TRUE(EditorImportTransaction::read(project.path/".astra/assets.astra",bytes)&&std::string(bytes.begin(),bytes.end())=="old registry","registry restored too");
}
AE_TEST(library_package_revisions_isolate_original_unity_texture_identities) {
  PackageProject project;
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"open project before preparing sources");
  const std::vector<u8> png{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,104,112,80,248,15,0,4,4,1,224,242,235,128,249,0,0,0,0,73,69,78,68,174,66,96,130};
  const std::string image(png.begin(),png.end());
  AE_EXPECT_TRUE(project.add("Color.png",image)&&project.add("Color.png.meta","guid: 11111111111111111111111111111111\n"),"texture and original identity");
  AE_EXPECT_TRUE(project.add("Paint.mat","Material:\n  m_Name: Paint\n  m_Shader: {fileID: 46, guid: 0000000000000000f000000000000000, type: 0}\n  m_SavedProperties:\n    m_TexEnvs:\n    - _MainTex:\n        m_Texture: {fileID: 2800000, guid: 11111111111111111111111111111111, type: 3}\n"),"material uses original texture GUID");
  const auto first=project.prepare();AE_EXPECT_TRUE(first.accepted,first.diagnostic.c_str());
  std::string error;
  AE_EXPECT_TRUE(session.importLibraryPackage(first,error),error.c_str());
  resources::AssetGuid firstMaterial;
  for(const auto &item:first.package.items)if(item.kind==resources::LibraryItemKind::Material)firstMaterial=session.importUnityMaterial(item.path,error);
  AE_EXPECT_TRUE(firstMaterial.valid(),error.c_str());const auto firstTexture=session.findMaterialAsset(firstMaterial)->textures[0];
  const auto firstHash=session.assets().find(firstTexture)->contentHash;
  // PNG permits bytes after IEND; a new source revision remains decodable while
  // differing byte-for-byte and retaining the same original Unity GUID.
  project.files.clear();
  AE_EXPECT_TRUE(project.add("Color.png",image+"revision-2")&&project.add("Color.png.meta","guid: 11111111111111111111111111111111\n"),"second source revision");
  // Publication moves staging into its immutable package namespace.
  std::vector<u8> material;AE_EXPECT_TRUE(EditorImportTransaction::read(project.path/"Biblioteca"/first.package.id.text()/"Paint.mat",material),"preserved material");
  AE_EXPECT_TRUE(project.add("Paint.mat",std::string(material.begin(),material.end())),"same source reference");
  const auto second=project.prepare();AE_EXPECT_TRUE(second.accepted&&second.package.id!=first.package.id,second.diagnostic.c_str());
  AE_EXPECT_TRUE(session.importLibraryPackage(second,error),error.c_str());resources::AssetGuid secondMaterial;
  for(const auto &item:second.package.items)if(item.kind==resources::LibraryItemKind::Material)secondMaterial=session.importUnityMaterial(item.path,error);
  AE_EXPECT_TRUE(secondMaterial.valid(),error.c_str());const auto secondTexture=session.findMaterialAsset(secondMaterial)->textures[0];
  AE_EXPECT_TRUE(firstTexture!=secondTexture&&firstHash!=session.assets().find(secondTexture)->contentHash,"native identities and hashes isolated by package revision");
  AE_EXPECT_TRUE(session.assets().find(firstTexture)->source.starts_with("Biblioteca/"+first.package.id.text()+"/")&&
    session.assets().find(secondTexture)->source.starts_with("Biblioteca/"+second.package.id.text()+"/"),"each material resolves only its own preserved source namespace");
  for(const auto &package:session.library().packages())for(const auto &item:package.items)
    AE_EXPECT_EQ(session.library().resolve(item.id,session.assets()).state,resources::LibraryResolutionState::Registered,"material and texture recipes round-trip through index");
}
