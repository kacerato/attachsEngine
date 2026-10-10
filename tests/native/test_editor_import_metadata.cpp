#include "harness.h"
#include "core/base.h"
#include "editor/editor_import_transaction.h"
#include <array>
#include <chrono>
using namespace ae;
using namespace ae::editor;
namespace {
struct MetadataProject {
  std::filesystem::path root=std::filesystem::temp_directory_path()/("astra-metadata-journal-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  MetadataProject(){std::filesystem::create_directories(root/".astra/imports");}
  ~MetadataProject(){std::error_code error;std::filesystem::remove_all(root,error);}
  bool write(const std::string &path,const std::string &text){return EditorImportTransaction::writeText(root/path,text);}
  bool equals(const std::string &path,const std::string &text){std::vector<u8> bytes;return EditorImportTransaction::read(root/path,bytes)&&std::string(bytes.begin(),bytes.end())==text;}
};
std::string digest(const std::string &text){return Sha256::hex({reinterpret_cast<const u8 *>(text.data()),text.size()});}
}
AE_TEST(import_metadata_journal_publishes_node_map_library_and_profile_and_recovers_interruption) {
  MetadataProject project;
  const std::string index=".astra/imports/library-index.astra",profile=".astra/imports/123456789abcdef0123456789abcdef0.profile",map=".astra/imports/123456789abcdef0123456789abcdef0.nodes";
  AE_EXPECT_TRUE(project.write("mesh.fbx","old source")&&project.write(".astra/assets.astra","old registry")&&project.write(map,"old map")&&project.write(index,"old index")&&project.write(profile,"old profile"),"complete previous version");
  const std::array metadata{
    EditorImportTransaction::Metadata{EditorImportTransaction::MetadataKind::NodeMap,map,digest("old map")},
    EditorImportTransaction::Metadata{EditorImportTransaction::MetadataKind::LibraryIndex,index,digest("old index")},
    EditorImportTransaction::Metadata{EditorImportTransaction::MetadataKind::SourceProfile,profile,digest("old profile")}};
  std::string error;EditorImportTransaction pending(project.root.string());
  AE_EXPECT_TRUE(pending.begin("mesh.fbx",digest("old source"),error,{}, {},metadata),error.c_str());
  AE_EXPECT_TRUE(project.write("mesh.fbx","partial source")&&project.write(".astra/assets.astra","partial registry")&&project.write(map,"partial map")&&project.write(index,"partial index")&&project.write(profile,"partial profile"),"simulate interruption during publication");
  AE_EXPECT_TRUE(EditorImportTransaction::recover(project.root.string(),error),error.c_str());
  AE_EXPECT_TRUE(project.equals("mesh.fbx","old source")&&project.equals(".astra/assets.astra","old registry")&&project.equals(map,"old map")&&project.equals(index,"old index")&&project.equals(profile,"old profile"),"one recovery restores all five files");
  EditorImportTransaction publish(project.root.string());
  AE_EXPECT_TRUE(publish.begin("mesh.fbx",digest("old source"),error,{}, {},metadata),error.c_str());
  const std::string next="new source";const std::array<std::optional<std::string>,3> values{"new map","new index","new profile"};
  AE_EXPECT_TRUE(publish.commit({reinterpret_cast<const u8*>(next.data()),next.size()},"new registry",{},&error,values),error.c_str());
  AE_EXPECT_TRUE(project.equals("mesh.fbx",next)&&project.equals(".astra/assets.astra","new registry")&&project.equals(map,"new map")&&project.equals(index,"new index")&&project.equals(profile,"new profile"),"one commit publishes all five files");
  AE_EXPECT_TRUE(EditorImportTransaction::recover(project.root.string(),error)&&!std::filesystem::exists(project.root/".astra/import-transaction/journal"),"committed operation leaves no recovery journal");
  EditorImportTransaction detach(project.root.string());
  const std::array previous{EditorImportTransaction::Metadata{EditorImportTransaction::MetadataKind::NodeMap,map,digest("new map")},
    EditorImportTransaction::Metadata{EditorImportTransaction::MetadataKind::LibraryIndex,index,digest("new index")},
    EditorImportTransaction::Metadata{EditorImportTransaction::MetadataKind::SourceProfile,profile,digest("new profile")}};
  AE_EXPECT_TRUE(detach.begin("mesh.fbx",digest(next),error,{}, {},previous),error.c_str());
  const std::array<std::optional<std::string>,3> detached{std::nullopt,"source-only index",std::nullopt};
  AE_EXPECT_TRUE(detach.commit({reinterpret_cast<const u8*>(next.data()),next.size()},"detached registry",{},&error,detached)&&
    !std::filesystem::exists(project.root/profile)&&!std::filesystem::exists(project.root/map)&&project.equals(index,"source-only index"),"history can restore original absence of a typed profile without an invalid empty file");
}
AE_TEST(import_metadata_journal_refuses_untyped_paths_duplicates_and_changed_metadata) {
  MetadataProject project;std::string error;
  const std::string index=".astra/imports/library-index.astra";
  AE_EXPECT_TRUE(project.write("mesh.fbx","source")&&project.write(index,"external"),"baseline source/index");
  const auto attempt=[&](const std::vector<EditorImportTransaction::Metadata> &entries) {
    EditorImportTransaction transaction(project.root.string());
    return transaction.begin("mesh.fbx",digest("source"),error,{}, {},entries);
  };
  using Kind=EditorImportTransaction::MetadataKind;
  AE_EXPECT_TRUE(!attempt({{Kind::LibraryIndex,index,digest("stale")}}),"changed index cannot be overwritten");
  AE_EXPECT_TRUE(!attempt({{Kind::LibraryIndex,".astra/assets.astra",{}}}),"whitelist forbids registry alias");
  AE_EXPECT_TRUE(!attempt({{Kind::SourceProfile,".astra/imports/../../outside.profile",{}}}),"typed profile rejects traversal");
  AE_EXPECT_TRUE(!attempt({{Kind::SourceProfile,".astra/imports/00000000000000000000000000000000.profile",{}}}),"invalid resource identity refused");
  AE_EXPECT_TRUE(!attempt({{Kind::LibraryIndex,index,digest("external")},{Kind::LibraryIndex,index,digest("external")}}),"duplicate target cannot own two backups");
  AE_EXPECT_TRUE(project.equals("mesh.fbx","source")&&project.equals(index,"external")&&!std::filesystem::exists(project.root/".astra/import-transaction/journal"),"all refusals leave authoring files and journal state intact");
  EditorImportTransaction late(project.root.string());
  const std::array metadata{EditorImportTransaction::Metadata{Kind::LibraryIndex,index,digest("external")}};
  AE_EXPECT_TRUE(late.begin("mesh.fbx",digest("source"),error,{}, {},metadata),error.c_str());
  AE_EXPECT_TRUE(project.write(index,"external after begin"),"simulate external metadata edit while GPU candidate is prepared");
  const std::string source="new source";const std::array<std::optional<std::string>,1> values{"new index"};
  AE_EXPECT_TRUE(!late.commit({reinterpret_cast<const u8*>(source.data()),source.size()},"new registry",{},&error,values),"precommit conflict refused before the first mutation");
  AE_EXPECT_TRUE(late.rollback()&&project.equals("mesh.fbx","source")&&project.equals(index,"external after begin")&&
    !std::filesystem::exists(project.root/".astra/assets.astra")&&!std::filesystem::exists(project.root/".astra/import-transaction/journal"),"caller rollback after pure refusal preserves external edits and clears only internal backups");
  EditorImportTransaction registryConflict(project.root.string());
  AE_EXPECT_TRUE(registryConflict.begin("mesh.fbx",digest("source"),error),error.c_str());
  AE_EXPECT_TRUE(project.write(".astra/assets.astra","external registry after begin"),"simulate external registry edit in prepare/commit window");
  AE_EXPECT_TRUE(!registryConflict.commit({reinterpret_cast<const u8*>(source.data()),source.size()},"candidate registry",{},&error),"registry conflict refused before source write");
  AE_EXPECT_TRUE(registryConflict.rollback()&&project.equals("mesh.fbx","source")&&project.equals(".astra/assets.astra","external registry after begin"),"pure refusal and caller rollback preserve external registry");

}
