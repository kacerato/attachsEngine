#include "harness.h"
#include "resources/terrain_history_store.h"
#include <chrono>
#include <fstream>
using ae::resources::TerrainHistoryStore;
namespace {
struct HistoryFixture {
  std::filesystem::path dir =
      std::filesystem::temp_directory_path() /
      ("astra-history-test-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  ~HistoryFixture() {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
  }
};
} // namespace
AE_TEST(terrain_history_exact_binary_delta_chain_and_gc) {
  HistoryFixture f;
  TerrainHistoryStore store(f.dir, {4096, 2});
  std::string a(2048, 'a');
  a[50] = '\0';
  std::string b = a;
  b[100] = 'b';
  auto c = b;
  c.insert(500, "inserted");
  auto d = c;
  d.erase(10, 12);
  std::string ia, ib, ic, id, out;
  AE_EXPECT_TRUE(store.put(a, {}, ia) && store.put(b, ia, ib) &&
                     store.put(c, ib, ic) && store.put(d, ic, id),
                 "write checkpoints and bounded deltas");
  AE_EXPECT_TRUE(std::filesystem::file_size(f.dir / (ib + ".aethist")) < 200,
                 "small edit stores exact delta");
  AE_EXPECT_TRUE(std::filesystem::file_size(f.dir / (id + ".aethist")) > 2000,
                 "depth limit forces full checkpoint");
  AE_EXPECT_TRUE(store.read(ic, out) && out == c && store.read(id, out) &&
                     out == d,
                 "exact embedded null insertion/deletion restoration");
  std::size_t removed = 99;
  AE_EXPECT_TRUE(store.collect({id}, {ic}, removed) && removed == 0,
                 "live roots retain delta ancestors and protected roots");
  AE_EXPECT_TRUE(store.collect({id}, {}, removed) && removed == 3,
                 "unreferenced chain is collected");
  AE_EXPECT_TRUE(store.read(id, out) && out == d,
                 "protected checkpoint survives");
}
AE_TEST(terrain_history_corruption_interruption_and_fail_closed_gc) {
  HistoryFixture f;
  TerrainHistoryStore store(f.dir);
  std::string id, second, out = "unchanged";
  AE_EXPECT_TRUE(store.put("original", {}, id) &&
                     store.put("other", {}, second),
                 "write snapshots");
  AE_EXPECT_EQ(
      id, "199e4a0bc2e0bfb2ea054222b14bf083095a34159a03c0ca5684aba3d7cb4f5e",
      "known SHA-256 envelope identity");
  auto object = f.dir / (id + ".aethist");
  {
    std::ofstream w(object, std::ios::binary | std::ios::trunc);
    w << "ATH1F";
  }
  AE_EXPECT_TRUE(!store.read(id, out) && out == "unchanged",
                 "truncation fails without changing destination");
  std::size_t count;
  AE_EXPECT_TRUE(!store.collect({id}, {}, count) &&
                     std::filesystem::exists(f.dir / (second + ".aethist")),
                 "invalid root prevents all sweep writes");
  {
    std::ofstream w(f.dir / (second + ".aethist.aether-tmp"));
    w << "partial";
  }
  {
    std::ofstream w(f.dir / "user.glb");
    w << "asset";
  }
  {
    std::ofstream w(f.dir / "unrelated.tmp");
    w << "keep";
  }
  AE_EXPECT_TRUE(store.recover(count) && count == 1,
                 "only owned interrupted writes removed");
  AE_EXPECT_TRUE(std::filesystem::exists(f.dir / "user.glb") &&
                     std::filesystem::exists(f.dir / "unrelated.tmp"),
                 "user assets and unrelated temp files untouched");
  AE_EXPECT_TRUE(!store.read("../user.glb", out), "reject path traversal");
  AE_EXPECT_TRUE(!store.put("original", {}, id),
                 "corrupt immutable object never overwritten");
  auto secondPath = f.dir / (second + ".aethist");
  {
    std::fstream w(secondPath, std::ios::binary | std::ios::in | std::ios::out);
    w.seekp(13);
    w.put('X');
  }
  AE_EXPECT_TRUE(!store.read(second, out),
                 "same-size checksum corruption rejected");
}
AE_TEST(terrain_history_missing_delta_base_and_limits) {
  HistoryFixture f;
  TerrainHistoryStore store(f.dir, {1024, 4});
  std::string a(512, 'a'), b = a;
  b[20] = 'b';
  std::string ia, ib, out;
  AE_EXPECT_TRUE(store.put(a, {}, ia) && store.put(b, ia, ib), "write delta");
  std::filesystem::remove(f.dir / (ia + ".aethist"));
  AE_EXPECT_TRUE(!store.read(ib, out), "missing delta ancestor rejected");
  std::size_t count;
  AE_EXPECT_TRUE(!store.collect({ib}, {}, count) &&
                     std::filesystem::exists(f.dir / (ib + ".aethist")),
                 "broken dependency aborts collection");
  AE_EXPECT_TRUE(!store.put(std::string(1025, 'x'), {}, out),
                 "bounded snapshot size");
  AE_EXPECT_TRUE(!store.put(a, std::string(64, '0'), out),
                 "missing parent cannot silently become checkpoint");
}
AE_TEST(terrain_history_refuses_symlinked_store) {
  HistoryFixture f;
  auto real = f.dir / "real";
  std::filesystem::create_directories(real);
  std::error_code ec;
  std::filesystem::create_directory_symlink(real, f.dir / "alias", ec);
  if (ec) {
    std::fprintf(stderr, "symlink test unavailable: %s\n",
                 ec.message().c_str());
    return;
  }
  TerrainHistoryStore alias(f.dir / "alias" / "objects");
  std::string id;
  AE_EXPECT_TRUE(!alias.put("bytes", {}, id),
                 "symlinked ancestor is refused before creating directories");
  AE_EXPECT_TRUE(!std::filesystem::exists(real / "objects"),
                 "no directory created through alias");
  TerrainHistoryStore store(real);
  AE_EXPECT_TRUE(store.put("bytes", {}, id), "real object write");
  auto object = real / (id + ".aethist");
  auto target = f.dir / "protected.bin";
  std::filesystem::rename(object, target);
  std::filesystem::create_symlink(target, object, ec);
  AE_EXPECT_TRUE(!ec, "create object symlink");
  std::string out;
  AE_EXPECT_TRUE(!store.read(id, out),
                 "symlink object cannot become a live root");
  std::size_t n;
  AE_EXPECT_TRUE(!store.collect({id}, {}, n),
                 "symlink root prevents collection");
  AE_EXPECT_TRUE(store.collect({}, {}, n) && n == 0 &&
                     std::filesystem::exists(target),
                 "sweep skips symlinks and preserves target");
}
#ifdef TERRAIN_HISTORY_STANDALONE
int main() {
  for (auto &t : ae::test::registry()) {
    ae::test::currentTestFailed() = false;
    t.fn();
    if (ae::test::currentTestFailed())
      return 1;
  }
  return 0;
}
#endif
