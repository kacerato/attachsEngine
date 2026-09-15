#include "resources/glb_images.h"
#include "resources/json_reader.h"

#include <cstring>

namespace ae::resources {
namespace {
constexpr u32 kGlbMagic = 0x46546C67, kJsonChunk = 0x4E4F534A, kBinaryChunk = 0x004E4942;
constexpr u32 kMaximumImages = 4096;

u32 readU32(std::span<const u8> bytes, usize at) {
  u32 value = 0;
  std::memcpy(&value, bytes.data() + at, sizeof value);
  return value;
}
} // namespace

bool listGlbEmbeddedImages(std::span<const u8> glb, std::vector<GlbEmbeddedImage> &out, std::string &diagnostic) {
  out.clear();
  diagnostic.clear();
  const auto fail = [&](const char *reason) {
    out.clear();
    diagnostic = reason;
    return false;
  };
  if (glb.size() < 20 || readU32(glb, 0) != kGlbMagic || readU32(glb, 4) != 2) return fail("O arquivo não é um GLB 2.0.");
  const u64 total = readU32(glb, 8);
  if (total > glb.size() || total < 20) return fail("Tamanho do GLB inconsistente.");
  const u64 jsonLength = readU32(glb, 12);
  if (readU32(glb, 16) != kJsonChunk || 20 + jsonLength > total) return fail("Bloco JSON do GLB inválido.");
  const std::string_view jsonText(reinterpret_cast<const char *>(glb.data() + 20), static_cast<usize>(jsonLength));
  std::span<const u8> binary;
  u64 at = 20 + jsonLength;
  if (at + 8 <= total) {
    const u64 length = readU32(glb, static_cast<usize>(at));
    if (readU32(glb, static_cast<usize>(at + 4)) == kBinaryChunk && at + 8 + length <= total)
      binary = glb.subspan(static_cast<usize>(at + 8), static_cast<usize>(length));
  }
  JsonDocument document;
  if (!JsonDocument::parse(jsonText, document) || !document.root() || document.root()->kind != JsonDocument::Kind::Object)
    return fail("JSON do GLB ilegível.");
  const auto &root = *document.root();
  const auto *images = document.member(root, "images");
  if (!images) return true;
  const auto *views = document.member(root, "bufferViews");
  if (images->kind != JsonDocument::Kind::Array || (views && views->kind != JsonDocument::Kind::Array))
    return fail("Lista de imagens do GLB inválida.");
  for (u32 i = 0; i < images->childCount && i < kMaximumImages; ++i) {
    const auto *image = document.child(*images, i);
    if (!image || image->kind != JsonDocument::Kind::Object || !views) continue;
    const auto viewIndex = document.index(*image, "bufferView");
    if (viewIndex < 0 || viewIndex >= views->childCount) continue;
    const auto *view = document.child(*views, static_cast<u32>(viewIndex));
    if (!view || view->kind != JsonDocument::Kind::Object || document.index(*view, "buffer") != 0) continue;
    const double offset = document.number(*view, "byteOffset", 0);
    const auto length = document.index(*view, "byteLength");
    if (offset < 0 || length <= 0 || offset + static_cast<double>(length) > static_cast<double>(binary.size())) continue;
    GlbEmbeddedImage entry;
    entry.index = i;
    entry.name = std::string(document.string(*image, "name"));
    const auto bytes = binary.subspan(static_cast<usize>(offset), static_cast<usize>(length));
    entry.container = detectImageContainer(bytes);
    entry.bytes.assign(bytes.begin(), bytes.end());
    out.push_back(std::move(entry));
  }
  return true;
}
} // namespace ae::resources
