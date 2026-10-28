#include "editor/editor_bundled_packages.h"
#include "editor/editor_screen.h"
#include "editor/editor_session.h"
#include "harness.h"
#include "core/sha256.h"
#include <chrono>
using namespace ae;
using namespace ae::editor;
namespace {
std::string hex(const std::string &text){return Sha256::hex({reinterpret_cast<const u8*>(text.data()),text.size()});}
struct Folder {
  std::filesystem::path path=std::filesystem::temp_directory_path()/("astra-bundled-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Folder(){std::filesystem::create_directories(path);}
  ~Folder(){std::error_code error;std::filesystem::remove_all(path,error);}
};
// Índice de dois pacotes como o build gera: assets neutros e caminho original.
std::string index(const std::vector<std::pair<std::string,std::string>> &files) {
  std::string text="ASTRA_BUNDLED_PACKAGES_1\npackage\tPacote de autoria\tp0\n";
  for(usize i=0;i<files.size();++i)
    text+="file\tf"+std::to_string(i)+".bin\t"+std::to_string(files[i].second.size())+"\t"+hex(files[i].second)+"\t"+files[i].first+"\n";
  return text+"package\tEfeitos\tp1\nfile\tf0.bin\t3\t"+hex("abc")+"\tAssets/a.txt\n";
}
}

AE_TEST(bundled_package_index_parses_and_rejects_unsafe_entries) {
  std::vector<BundledPackage> packages;std::string diagnostic;
  AE_EXPECT_TRUE(parseBundledPackageIndex(index({{"Assets/Fx/Fogo.prefab","fogo"},{"Assets/Fx/Fogo.prefab.meta","guid: 1"}}),packages,diagnostic),diagnostic.c_str());
  AE_EXPECT_EQ(packages.size(),usize{2},"two packages");
  AE_EXPECT_TRUE(packages[0].files.size()==2&&packages[0].files[0].asset=="bundled-packages/p0/f0.bin"&&packages[0].bytes==11,"assets under the package directory");
  AE_EXPECT_TRUE(packages[0].indexHash.size()==64&&packages[0].indexHash!=packages[1].indexHash,"one marker hash per package");
  for(const char *bad:{"Assets/../x","/abs","C:/x","Assets//x",".astra/x"}) {
    AE_EXPECT_TRUE(!parseBundledPackageIndex(index({{bad,"x"}}),packages,diagnostic)&&packages.empty(),bad);
  }
  AE_EXPECT_TRUE(!parseBundledPackageIndex(index({{"a","x"},{"a","y"}}),packages,diagnostic),"repeated path refused");
  AE_EXPECT_TRUE(!parseBundledPackageIndex("ASTRA_BUNDLED_PACKAGES_2\n",packages,diagnostic),"unknown header refused");
  AE_EXPECT_TRUE(!parseBundledPackageIndex("ASTRA_BUNDLED_PACKAGES_1\npackage\tA\tp0\n",packages,diagnostic),"empty package refused");
}

AE_TEST(bundled_package_marker_copy_and_library_catalog) {
  const std::vector<std::pair<std::string,std::string>> sources{
    {"Assets/Fx/Fogo.prefab","%YAML 1.1\nGameObject:\n  m_Name: Fogo\n"},
    {"Assets/Fx/Fogo.prefab.meta","fileFormatVersion: 2\nguid: 0123456789abcdef0123456789abcdef\n"}};
  std::vector<BundledPackage> packages;std::string diagnostic;
  AE_EXPECT_TRUE(parseBundledPackageIndex(index(sources),packages,diagnostic),diagnostic.c_str());
  const auto &package=packages[0];
  Folder extracted,project;
  AE_EXPECT_TRUE(!bundledPackageExtracted(extracted.path,package),"empty folder is not extracted");
  for(const auto &[relative,text]:sources) {
    std::filesystem::create_directories((extracted.path/relative).parent_path());
    AE_EXPECT_TRUE(EditorImportTransaction::writeText(extracted.path/relative,text),"source");
  }
  AE_EXPECT_TRUE(!bundledPackageExtracted(extracted.path,package),"no marker yet");
  AE_EXPECT_TRUE(writeBundledPackageMarker(extracted.path,package)&&bundledPackageExtracted(extracted.path,package),"marker makes it complete");
  AE_EXPECT_TRUE(!bundledPackageExtracted(extracted.path,packages[1]),"another package's marker does not match");
  const std::string relative=".astra/import-staging/library-bundled";std::atomic<u64> done{0};
  AE_EXPECT_TRUE(copyBundledPackageToStaging(extracted.path,project.path/relative,package,{},done,diagnostic),diagnostic.c_str());
  AE_EXPECT_EQ(done.load(),package.bytes,"progress counts every byte");
  std::vector<LibrarySourceFile> files;for(const auto &file:package.files)files.push_back({file.relative,file.sha256,file.bytes,{}});
  const auto prepared=prepareLibraryPackage(project.path.string(),relative,package.name,files);
  AE_EXPECT_TRUE(prepared.accepted&&prepared.package.name=="Pacote de autoria"&&prepared.package.items.size()==1,prepared.diagnostic.c_str());
  // Uma fonte alterada depois da extração não passa pela cópia.
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(extracted.path/sources[0].first,"mudou"),"tamper");
  AE_EXPECT_TRUE(!copyBundledPackageToStaging(extracted.path,project.path/(relative+"-2"),package,{},done,diagnostic),"changed source refused");
}

AE_TEST(bundled_package_card_requests_import_or_opens_the_imported_package) {
  Folder project;
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"open project");
  std::vector<u8> fontBytes,atlasBytes;ui::UiFont font;ui::UiIconAtlas atlas;
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-font.aeuf",fontBytes)&&font.load(fontBytes),"font");
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-icons.aeui",atlasBytes)&&atlas.load(atlasBytes),"icons");
  session.initialize(&font,&atlas);session.setSurface({0,0,1280,720},{});
  // Importa um pacote com o mesmo nome do primeiro incluído: a Biblioteca abre nele.
  const std::string staging=".astra/import-staging/package",text="notas\n";
  std::filesystem::create_directories(project.path/staging);
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(project.path/staging/"Notas.txt",text),"source");
  const auto prepared=prepareLibraryPackage(project.path.string(),staging,"Pacote de autoria",{{"Notas.txt",hex(text),text.size(),{}}});
  std::string error;AE_EXPECT_TRUE(prepared.accepted&&session.importLibraryPackage(prepared,error),error.c_str());
  session.setBundledPackages({{"Pacote de autoria",6,1},{"Efeitos",3000000,40}});
  const auto frame=[&](ui::UiDrawList &list) {
    session.update();ui::UiInputRouter router;list.begin(session.screen().surface,font.metrics(ui::UiFontWeight::Regular));router.beginFrame();
    buildEditorScreen(session.screen(),ui::defaultTheme(),list,router);
  };
  const auto find=[&](ui::UiDrawList &list,std::string_view label){
    for(const auto &command:list.commands())if(command.kind==ui::UiPrimitive::Text&&list.textOf(command)==label)return command.bounds;
    return ui::UiRect{};
  };
  const auto tap=[&](ui::UiRect at,u32 id){
    const ui::UiPoint point{at.x+at.width*.5f,at.y+at.height*.5f};
    session.handlePointer({id,ui::UiPointerPhase::Down,point,double(id)});session.handlePointer({id,ui::UiPointerPhase::Up,point,double(id)+.05});session.update();
  };
  ui::UiDrawList list;frame(list);
  AE_EXPECT_TRUE(find(list,"Incluídos com a Astra").width==0,"card hidden while the Library shows one package");
  // Ícone de pacotes no cabeçalho: volta para todos os pacotes.
  ui::UiRect packages{};
  for(const auto &command:list.commands())
    if(command.kind==ui::UiPrimitive::Image&&command.image==static_cast<ui::UiImageId>(ui::UiIcon::AssetsSourcePackage)&&command.bounds.y<80)packages=command.bounds;
  AE_EXPECT_TRUE(packages.width>0,"packages icon in the header");
  tap(packages,201);
  ui::UiDrawList all;frame(all);
  AE_EXPECT_TRUE(find(all,"Incluídos com a Astra").width>0,"card visible on all packages");
  AE_EXPECT_TRUE(find(all,"Importado").width>0&&find(all,"Importar").width>0,"imported badge and import action");
  u32 index=99;AE_EXPECT_TRUE(!session.consumeBundledPackageRequest(index),"no request before a tap");
  tap(find(all,"Importar"),202);
  AE_EXPECT_TRUE(session.consumeBundledPackageRequest(index)&&index==1,"tap asks the shell to import the second package");
  AE_EXPECT_TRUE(!session.consumeBundledPackageRequest(index),"request consumed once");
  tap(find(all,"Importado"),203);
  AE_EXPECT_TRUE(!session.consumeBundledPackageRequest(index)&&session.screen().libraryPackage==prepared.package.id,"imported package opens filtered");
}
