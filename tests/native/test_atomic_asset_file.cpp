#include "harness.h"
#include "platform/atomic_asset_file.h"
#include <cstring>
#include <filesystem>
#include <chrono>

namespace {
struct Bytes {
  const char *value;
  int remaining;
  bool fail = false;
};
int readBytes(void *context, void *buffer, size_t capacity) {
  auto &bytes = *static_cast<Bytes *>(context);
  if (bytes.fail) return -1;
  const int size = static_cast<int>(capacity) < bytes.remaining ? static_cast<int>(capacity) : bytes.remaining;
  std::memcpy(buffer, bytes.value, size);
  bytes.value += size; bytes.remaining -= size;
  return size;
}
struct Fixture {
  std::string path = (std::filesystem::temp_directory_path() /
      ("aether-asset-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))).string();
  ~Fixture() { std::remove(path.c_str()); std::remove((path + ".aether-tmp").c_str()); }
  bool write(const char *value, int length, int expected, bool fail = false) {
    Bytes bytes{value, length, fail};
    return ae::platform::replaceAssetFile(path.c_str(), expected, readBytes, &bytes);
  }
  bool matches(const char *value) {
    FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    char buffer[32]{};
    const size_t read = std::fread(buffer, 1, sizeof(buffer), file);
    std::fclose(file);
    return read == std::strlen(value) && std::memcmp(buffer, value, read) == 0;
  }
};
}
AE_TEST(asset_atomic_same_size_update_replaces_contents) {
  Fixture f;
  AE_EXPECT_TRUE(f.write("old", 3, 3), "initial");
  AE_EXPECT_TRUE(f.write("new", 3, 3), "replace same size");
  AE_EXPECT_TRUE(f.matches("new"), "must not retain stale bytes");
}
AE_TEST(asset_atomic_truncated_read_preserves_old_file) {
  Fixture f;
  AE_EXPECT_TRUE(f.write("old", 3, 3), "initial");
  AE_EXPECT_TRUE(!f.write("ne", 2, 3), "short read rejected");
  AE_EXPECT_TRUE(f.matches("old"), "preserve valid file");
  AE_EXPECT_TRUE(!std::filesystem::exists(f.path + ".aether-tmp"), "temporary cleaned");
}
AE_TEST(asset_atomic_read_failure_preserves_old_file) {
  Fixture f;
  AE_EXPECT_TRUE(f.write("old", 3, 3), "initial");
  AE_EXPECT_TRUE(!f.write("new", 3, 3, true), "read failure");
  AE_EXPECT_TRUE(f.matches("old"), "preserve valid file");
}
AE_TEST(asset_atomic_oversized_input_is_rejected) {
  Fixture f;
  AE_EXPECT_TRUE(!f.write("long", 4, 3), "length checked");
  AE_EXPECT_TRUE(!std::filesystem::exists(f.path), "no incomplete destination");
}
AE_TEST(asset_build_id_requires_exact_lowercase_sha256) {
  const char *id = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  AE_EXPECT_TRUE(ae::platform::validAssetBuildId(id, 64), "valid hash");
  AE_EXPECT_TRUE(!ae::platform::validAssetBuildId(id, 63), "short hash");
  AE_EXPECT_TRUE(!ae::platform::validAssetBuildId(nullptr, 64), "null hash");
  AE_EXPECT_TRUE(!ae::platform::validAssetBuildId("z123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", 64), "nonhex");
}
