#include "renderer/authoring_texture.h"

#include <filesystem>
#include <fstream>

namespace ae::renderer {
bool readAuthoringTextureLevels(const AuthoringTexture &texture, u32 from, std::vector<u8> &scratch,
                                std::span<const u8> &out) {
  if (!texture.valid() || from >= texture.levels) return false;
  if (from >= texture.firstLevel) {
    const u64 skip = texture.chainBytesFrom(texture.firstLevel) - texture.chainBytesFrom(from);
    out = std::span<const u8>(texture.mipChain).subspan(static_cast<usize>(skip));
    return true;
  }
  // Níveis [from, firstLevel) estão no arquivo, logo depois do que vem antes de
  // `from`; a cauda [firstLevel, levels) já está na memória.
  const u64 before = texture.expectedBytes() - texture.chainBytesFrom(from);
  const u64 missing = texture.chainBytesFrom(from) - texture.chainBytesFrom(texture.firstLevel);
  std::vector<u8> bytes(static_cast<usize>(missing + texture.mipChain.size()));
  const auto &file = *texture.file;
  const std::filesystem::path path(std::u8string(reinterpret_cast<const char8_t *>(file.path.data()), file.path.size()));
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  input.seekg(static_cast<std::streamoff>(file.offset + before));
  if (!input || !input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(missing))) return false;
  std::copy(texture.mipChain.begin(), texture.mipChain.end(), bytes.begin() + static_cast<std::ptrdiff_t>(missing));
  scratch = std::move(bytes);
  out = scratch;
  return true;
}
} // namespace ae::renderer
