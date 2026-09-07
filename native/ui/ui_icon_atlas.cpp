#include "ui/ui_icon_atlas.h"

namespace ae::ui {
namespace {

constexpr u32 kIconMagic = 0x49554541;  // 'AEUI'
constexpr u32 kIconVersion = 1;
constexpr u32 kHeaderBytes = 20;
constexpr u32 kRectBytes = 8;
constexpr u32 kMaximumAtlasSide = 8192;

u32 readU32(std::span<const u8> bytes, usize offset) noexcept {
  return static_cast<u32>(bytes[offset]) | static_cast<u32>(bytes[offset + 1]) << 8 |
         static_cast<u32>(bytes[offset + 2]) << 16 | static_cast<u32>(bytes[offset + 3]) << 24;
}

u16 readU16(std::span<const u8> bytes, usize offset) noexcept {
  return static_cast<u16>(static_cast<u16>(bytes[offset]) |
                          static_cast<u16>(bytes[offset + 1]) << 8);
}

} // namespace

void UiIconAtlas::unload() noexcept {
  ready_ = false;
  width_ = 0;
  height_ = 0;
  rects_.clear();
  pixels_ = {};
}

bool UiIconAtlas::load(std::span<const u8> bytes) {
  unload();
  if (bytes.size() < kHeaderBytes) return false;
  if (readU32(bytes, 0) != kIconMagic || readU32(bytes, 4) != kIconVersion) return false;

  const u32 width = readU32(bytes, 8);
  const u32 height = readU32(bytes, 12);
  const u32 count = readU32(bytes, 16);
  if (width == 0 || height == 0 || width > kMaximumAtlasSide || height > kMaximumAtlasSide)
    return false;
  // O binário e o enum saem da mesma execução da ferramenta. Uma contagem
  // diferente significa que um dos dois ficou para trás, e seguir em frente
  // desenharia ícones deslocados por um -- o tipo de defeito que passa
  // despercebido porque quase todo ícone continua sendo *um* ícone.
  if (count != kUiIconCount) return false;

  const u64 rectBytes = static_cast<u64>(count) * kRectBytes;
  const u64 pixelBytes = static_cast<u64>(width) * height * 4;
  if (bytes.size() != kHeaderBytes + rectBytes + pixelBytes) return false;

  rects_.resize(count);
  usize cursor = kHeaderBytes;
  for (u32 index = 0; index < count; ++index) {
    const u32 x = readU16(bytes, cursor);
    const u32 y = readU16(bytes, cursor + 2);
    const u32 rectWidth = readU16(bytes, cursor + 4);
    const u32 rectHeight = readU16(bytes, cursor + 6);
    cursor += kRectBytes;
    if (rectWidth == 0 || rectHeight == 0 || x + rectWidth > width || y + rectHeight > height) {
      unload();
      return false;
    }
    rects_[index] = {static_cast<float>(x), static_cast<float>(y),
                     static_cast<float>(rectWidth), static_cast<float>(rectHeight)};
  }

  width_ = width;
  height_ = height;
  pixels_ = bytes.subspan(static_cast<usize>(kHeaderBytes + rectBytes),
                          static_cast<usize>(pixelBytes));
  ready_ = true;
  return true;
}

UiRect UiIconAtlas::rectOf(UiIcon icon) const noexcept {
  const u32 index = static_cast<u32>(icon);
  if (!ready_ || index == 0 || index > rects_.size()) return UiRect{};
  return rects_[index - 1];
}

} // namespace ae::ui
