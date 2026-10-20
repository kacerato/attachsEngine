#include "editor/editor_terrain_recovery.h"
#include "harness.h"
#include <chrono>
#include <fstream>

using namespace ae;
namespace {
struct RecoveryFixture {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      std::filesystem::path(std::u8string(u8"astra-recuperação-地形-")) /
      std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count());
  RecoveryFixture() { std::filesystem::create_directories(root); }
  ~RecoveryFixture() {
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
  }
  std::string project() const {
    const auto text = root.u8string();
    return {reinterpret_cast<const char *>(text.data()), text.size()};
  }
  bool glb(const resources::TerrainRecipe &recipe, std::vector<u8> &bytes,
           std::string &error) {
    resources::TerrainMesh mesh;
    return resources::generateTerrain(recipe, mesh, error) &&
           resources::writeTerrainGlb(recipe, mesh, {}, bytes, error);
  }
};
} // namespace

AE_TEST(terrain_recovery_unicode_checkpoint_delta_and_original_preserved) {
  RecoveryFixture f;
  resources::TerrainRecipe first;
  first.cells = 8;
  for(u32 i=0;i<16;++i)
    first.strokes.push_back({resources::TerrainTool::Raise,float(i%4),0,float(i/4),4,.1f,0});
  auto second = first;
  second.seed += 1;
  std::string error;
  std::vector<u8> original, next;
  AE_EXPECT_TRUE(f.glb(first, original, error) && f.glb(second, next, error),
                 error.c_str());
  const auto a = Sha256::hex(original), b = Sha256::hex(next);
  auto source = f.root / "original.glb";
  AE_EXPECT_TRUE(editor::EditorImportTransaction::write(source, original),
                 "create original terrain source");
  AE_EXPECT_TRUE(
      editor::writeTerrainRecovery(f.project(), a, first, {}, error) &&
          editor::writeTerrainRecovery(f.project(), b, second, a, error),
      error.c_str());
  std::filesystem::path objects, ref;
  AE_EXPECT_TRUE(editor::terrainRecoveryPaths(f.project(), b, objects, ref),
                 "Unicode project yields safe recovery paths");
  std::vector<u8> pointer, object;
  AE_EXPECT_TRUE(editor::EditorImportTransaction::read(ref, pointer, 64),
                 "atomic revision reference exists");
  const std::string id(pointer.begin(), pointer.end());
  AE_EXPECT_TRUE(editor::EditorImportTransaction::read(
                     objects / (id + ".aethist"), object),
                 "delta object exists");
  AE_EXPECT_TRUE(object.size() > 5 && object[4] == 'D',
                 "small recipe revision uses an actual binary delta");
  resources::TerrainRecipe loaded;
  AE_EXPECT_TRUE(editor::readTerrainRecovery(f.project(), b, loaded, error),
                 "follow reference and delta base");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(loaded),
               resources::serializeTerrainRecipe(second),
               "recover exact revised recipe");
  AE_EXPECT_TRUE(editor::readTerrainRecovery(f.project(), a, loaded, error),
                 "previous checkpoint remains recoverable");
  std::vector<u8> disk;
  AE_EXPECT_TRUE(editor::EditorImportTransaction::read(source, disk) &&
                     disk == original,
                 "history writes never alter or delete original GLB");
}

AE_TEST(terrain_recovery_substitution_corruption_and_missing_source) {
  RecoveryFixture f;
  resources::TerrainRecipe wanted;
  wanted.cells = 8;
  auto substituted = wanted;
  substituted.seed += 7;
  std::string error;
  std::vector<u8> original, impostor;
  AE_EXPECT_TRUE(f.glb(wanted, original, error) &&
                     f.glb(substituted, impostor, error),
                 error.c_str());
  auto hash = Sha256::hex(original);
  auto source = f.root / "terrain.glb";
  AE_EXPECT_TRUE(
      editor::writeTerrainRecovery(f.project(), hash, wanted, {}, error),
      error.c_str());
  AE_EXPECT_TRUE(editor::EditorImportTransaction::write(source, impostor),
                 "substitute a valid but different terrain GLB");
  resources::TerrainRecipe loaded;
  bool recovered = false;
  AE_EXPECT_TRUE(editor::readTerrainAuthoring(f.project(), "terrain.glb", hash,
                                              loaded, error, recovered) &&
                     recovered && loaded.seed == wanted.seed,
                 "valid substituted GLB rejected by hash, checkpoint restores "
                 "intended recipe");
  const std::vector<u8> truncated{'g', 'l', 'T', 'F'};
  AE_EXPECT_TRUE(editor::EditorImportTransaction::write(source, truncated),
                 "truncate terrain GLB");
  AE_EXPECT_TRUE(editor::readTerrainAuthoring(f.project(), "terrain.glb", hash,
                                              loaded, error, recovered) &&
                     recovered && loaded.seed == wanted.seed,
                 "truncated GLB restores from checkpoint");
  std::filesystem::remove(source);
  AE_EXPECT_TRUE(editor::readTerrainAuthoring(f.project(), "terrain.glb", hash,
                                              loaded, error, recovered) &&
                     recovered,
                 "missing source restores authoring recipe");
  AE_EXPECT_TRUE(!std::filesystem::exists(source),
                 "recipe recovery does not claim or fabricate GLB restoration");
  loaded.seed = 9876;
  const auto before = resources::serializeTerrainRecipe(loaded);
  AE_EXPECT_TRUE(!editor::readTerrainAuthoring(f.project(), "missing.glb",
                                               std::string(64, '0'), loaded,
                                               error, recovered) &&
                     !recovered,
                 "missing source and checkpoint report failure");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(loaded), before,
               "failed recovery preserves caller output");
  AE_EXPECT_TRUE(!editor::readTerrainAuthoring(f.project(), "missing.glb",
                                               "../bad", loaded, error,
                                               recovered),
                 "invalid revision hash rejected");
}

AE_TEST(terrain_recovery_interrupted_reference_and_symlink_protection) {
  RecoveryFixture f;
  resources::TerrainRecipe recipe;
  recipe.cells = 8;
  std::string error;
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(f.glb(recipe, bytes, error), error.c_str());
  const auto hash = Sha256::hex(bytes);
  std::filesystem::path objects, ref;
  AE_EXPECT_TRUE(editor::terrainRecoveryPaths(f.project(), hash, objects, ref),
                 "reference path");
  std::filesystem::create_directories(ref.parent_path());
  auto tmp = ref;
  tmp += ".aether-tmp";
  {
    std::ofstream partial(tmp, std::ios::binary);
    partial << "partial reference";
  }
  resources::TerrainRecipe loaded;
  AE_EXPECT_TRUE(!editor::readTerrainRecovery(f.project(), hash, loaded, error),
                 "interrupted temp reference never becomes visible revision");
  AE_EXPECT_TRUE(
      editor::writeTerrainRecovery(f.project(), hash, recipe, {}, error),
      "retry publishes a complete reference atomically");
  AE_EXPECT_TRUE(
      editor::readTerrainRecovery(f.project(), hash, loaded, error) &&
          resources::serializeTerrainRecipe(loaded) ==
              resources::serializeTerrainRecipe(recipe),
      "completed retry can be reopened");
  auto other = recipe;
  other.seed += 2;
  AE_EXPECT_TRUE(f.glb(other, bytes, error), error.c_str());
  std::filesystem::path otherObjects, otherRef;
  const auto otherHash = Sha256::hex(bytes);
  AE_EXPECT_TRUE(editor::terrainRecoveryPaths(f.project(), otherHash,
                                              otherObjects, otherRef),
                 "second ref");
  auto otherTmp = otherRef;
  otherTmp += ".aether-tmp";
  auto protectedFile = f.root / "protected.bin";
  {
    std::ofstream target(protectedFile, std::ios::binary);
    target << "untouched";
  }
  std::error_code ec;
  std::filesystem::create_symlink(protectedFile, otherTmp, ec);
  if (ec) {
    std::fprintf(stderr, "symlink recovery test unavailable: %s\n",
                 ec.message().c_str());
    return;
  }
  AE_EXPECT_TRUE(
      !editor::writeTerrainRecovery(f.project(), otherHash, other, hash, error),
      "symlinked atomic reference temporary refused");
  std::vector<u8> protectedBytes;
  AE_EXPECT_TRUE(
      editor::EditorImportTransaction::read(protectedFile, protectedBytes),
      "protected target readable");
  AE_EXPECT_EQ(std::string(protectedBytes.begin(), protectedBytes.end()),
               "untouched", "symlink target never truncated");
}
