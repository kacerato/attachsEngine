#include "renderer/reflection_probes.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace ae::renderer {
namespace {
void hash(u64 &key, u64 value) noexcept { key = (key ^ value) * 1099511628211ull; }
void hashFloat(u64 &key, float value) noexcept { hash(key, std::bit_cast<u32>(value)); }
} // namespace

u64 reflectionProbeCaptureKey(const SceneReflectionProbe &probe) noexcept {
  u64 key = 1469598103934665603ull;
  for (float v : probe.position) hashFloat(key, v);
  hash(key, probe.resolution);
  hash(key, probe.hdr);
  hashFloat(key, probe.nearClip);
  hashFloat(key, probe.farClip);
  hash(key, probe.cullingMask);
  hash(key, probe.solidBackground);
  for (float v : probe.background) hashFloat(key, v);
  hash(key, static_cast<u64>(probe.type));
  return key;
}

bool skyReflectionNeedsCapture(std::span<const float> current, std::span<const float> captured,
                               bool captureValid) noexcept {
  if (!captureValid || current.size() != captured.size()) return true;
  for (usize i = 0; i < current.size(); ++i) {
    if (!std::isfinite(current[i])) return false; // céu inválido: mantém a última captura
    if (std::abs(current[i] - captured[i]) > 1e-3f * std::max(1.0f, std::abs(captured[i]))) return true;
  }
  return false;
}

u32 reflectionProbeBaseMip(u32 resolution) noexcept {
  const u32 octahedron = std::min<u32>(std::max<u32>(resolution, 16u) * 2u, ReflectionProbeAtlasSize);
  u32 mip = 0;
  while ((ReflectionProbeAtlasSize >> mip) > octahedron && mip + 1 < ReflectionProbeAtlasMips) ++mip;
  return mip;
}

u32 selectFrameReflectionProbes(std::span<const SceneReflectionProbe> probes, const float camera[3],
                                std::span<u32> outIndices) noexcept {
  std::vector<u32> order(probes.size());
  for (u32 i = 0; i < order.size(); ++i) order[i] = i;
  const auto distance2 = [&](const SceneReflectionProbe &p) {
    float d = 0;
    for (u32 k = 0; k < 3; ++k) {
      const float center = (p.boxMin[k] + p.boxMax[k]) * 0.5f;
      d += (center - camera[k]) * (center - camera[k]);
    }
    return d;
  };
  std::stable_sort(order.begin(), order.end(), [&](u32 a, u32 b) {
    const auto &pa = probes[a], &pb = probes[b];
    const bool ca = pa.contains(camera), cb = pb.contains(camera);
    if (ca != cb) return ca;
    if (pa.importance != pb.importance) return pa.importance > pb.importance;
    return distance2(pa) < distance2(pb);
  });
  const u32 count = static_cast<u32>(std::min<usize>({order.size(), outIndices.size(), MaximumFrameReflectionProbes}));
  for (u32 i = 0; i < count; ++i) outIndices[i] = order[i];
  return count;
}

float reflectionProbeInfluence(const SceneReflectionProbe &probe, const float point[3]) noexcept {
  float inside = 1e30f;
  for (u32 k = 0; k < 3; ++k) {
    const float d = std::min(point[k] - probe.boxMin[k], probe.boxMax[k] - point[k]);
    if (d < 0) return 0;
    inside = std::min(inside, d);
  }
  if (probe.blendDistance <= 1e-6f) return 1;
  return std::clamp(inside / probe.blendDistance, 0.0f, 1.0f);
}

ReflectionProbeBlend blendReflectionProbes(std::span<const SceneReflectionProbe> frame, const float center[3],
                                           ReflectionProbeUsage usage) noexcept {
  ReflectionProbeBlend result;
  if (usage == ReflectionProbeUsage::Off) return result;
  // Candidatas: as que alcançam o centro. Ordem da Unity: maior Importance
  // primeiro; empate fica com a caixa menor (a mais específica).
  struct Candidate { u32 layer; float weight; };
  std::array<Candidate, MaximumFrameReflectionProbes> candidates{};
  u32 count = 0;
  for (u32 i = 0; i < frame.size() && i < MaximumFrameReflectionProbes; ++i) {
    const float w = reflectionProbeInfluence(frame[i], center);
    if (w > 0) candidates[count++] = {i, w};
  }
  if (!count) return result;
  std::sort(candidates.begin(), candidates.begin() + count, [&](const Candidate &a, const Candidate &b) {
    const auto &pa = frame[a.layer], &pb = frame[b.layer];
    if (pa.importance != pb.importance) return pa.importance > pb.importance;
    return pa.volume() < pb.volume();
  });
  const Candidate &top = candidates[0];
  result.first = top.layer;
  if (usage == ReflectionProbeUsage::Simple) {
    result.firstWeight = 1;
    return result;
  }
  // A mais importante entra com o próprio peso; a segunda (ou o céu) completa.
  // Em "Misturar sondas" sem segunda sonda, a única fica inteira: a Unity só
  // mistura o céu quando a opção diz "e céu".
  result.firstWeight = top.weight;
  if (count > 1) {
    result.second = candidates[1].layer;
    const float rest = 1.0f - top.weight;
    if (usage == ReflectionProbeUsage::BlendProbes) {
      result.secondWeight = rest;
    } else {
      result.secondWeight = rest * candidates[1].weight;
    }
  } else if (usage == ReflectionProbeUsage::BlendProbes) {
    result.firstWeight = 1;
  }
  return result;
}

u32 encodeReflectionProbeBlend(const ReflectionProbeBlend &blend) noexcept {
  const auto quantize = [](float w) { return static_cast<u32>(std::lround(std::clamp(w, 0.0f, 1.0f) * 255.0f)); };
  const u32 first = std::min<u32>(blend.first, 15u), second = std::min<u32>(blend.second, 15u);
  return first | (second << 4) | (quantize(blend.firstWeight) << 8) | (quantize(blend.secondWeight) << 16);
}

ReflectionProbeBlend decodeReflectionProbeBlend(u32 word) noexcept {
  ReflectionProbeBlend blend;
  blend.first = word & 15u;
  blend.second = (word >> 4) & 15u;
  blend.firstWeight = static_cast<float>((word >> 8) & 255u) / 255.0f;
  blend.secondWeight = static_cast<float>((word >> 16) & 255u) / 255.0f;
  return blend;
}

void boxProjectReflection(const float position[3], const float direction[3], const float boxMin[3],
                          const float boxMax[3], const float capture[3], float out[3]) noexcept {
  for (u32 k = 0; k < 3; ++k) out[k] = direction[k];
  for (u32 k = 0; k < 3; ++k) if (position[k] < boxMin[k] || position[k] > boxMax[k]) return;
  // Distância até a parede da caixa ao longo do raio refletido; o ponto de
  // saída, visto do centro de captura, é a direção que a sonda viu.
  float t = 1e30f;
  for (u32 k = 0; k < 3; ++k) {
    if (std::abs(direction[k]) < 1e-8f) continue;
    const float plane = direction[k] > 0 ? boxMax[k] : boxMin[k];
    t = std::min(t, (plane - position[k]) / direction[k]);
  }
  if (!(t < 1e29f)) return;
  for (u32 k = 0; k < 3; ++k) out[k] = position[k] + direction[k] * t - capture[k];
}

void octahedralEncode(const float d[3], float uv[2]) noexcept {
  const float l1 = std::abs(d[0]) + std::abs(d[1]) + std::abs(d[2]);
  float x = d[0] / std::max(l1, 1e-8f), z = d[2] / std::max(l1, 1e-8f);
  if (d[1] < 0) {
    const float fx = (1 - std::abs(z)) * (x >= 0 ? 1.f : -1.f);
    const float fz = (1 - std::abs(x)) * (z >= 0 ? 1.f : -1.f);
    x = fx; z = fz;
  }
  uv[0] = x * 0.5f + 0.5f;
  uv[1] = z * 0.5f + 0.5f;
}

void octahedralDecode(const float uv[2], float d[3]) noexcept {
  float x = uv[0] * 2 - 1, z = uv[1] * 2 - 1;
  const float y = 1 - std::abs(x) - std::abs(z);
  if (y < 0) {
    const float fx = (1 - std::abs(z)) * (x >= 0 ? 1.f : -1.f);
    const float fz = (1 - std::abs(x)) * (z >= 0 ? 1.f : -1.f);
    x = fx; z = fz;
  }
  const float length = std::sqrt(x * x + y * y + z * z);
  d[0] = x / length; d[1] = y / length; d[2] = z / length;
}

u16 floatToHalf(float value) noexcept {
  const u32 bits = std::bit_cast<u32>(value);
  const u32 sign = (bits >> 16) & 0x8000u;
  const i32 exponent = static_cast<i32>((bits >> 23) & 0xffu) - 127 + 15;
  u32 mantissa = bits & 0x7fffffu;
  if (((bits >> 23) & 0xffu) == 0xffu) return static_cast<u16>(sign | 0x7c00u | (mantissa ? 0x200u : 0u));
  if (exponent >= 31) return static_cast<u16>(sign | 0x7bffu); // satura no maior finito
  if (exponent <= 0) {
    if (exponent < -10) return static_cast<u16>(sign);
    mantissa |= 0x800000u;
    const u32 shift = static_cast<u32>(14 - exponent);
    u32 half = mantissa >> shift;
    if ((mantissa >> (shift - 1)) & 1u) ++half;
    return static_cast<u16>(sign | half);
  }
  u32 half = sign | (static_cast<u32>(exponent) << 10) | (mantissa >> 13);
  if (mantissa & 0x1000u) ++half; // arredonda para o mais próximo
  return static_cast<u16>(half);
}

float halfToFloat(u16 value) noexcept {
  const u32 sign = (static_cast<u32>(value) & 0x8000u) << 16;
  const u32 exponent = (value >> 10) & 0x1fu;
  u32 mantissa = value & 0x3ffu;
  if (exponent == 0) {
    if (!mantissa) return std::bit_cast<float>(sign);
    float result = static_cast<float>(mantissa) / 1024.0f * std::ldexp(1.0f, -14);
    return sign ? -result : result;
  }
  if (exponent == 31) return std::bit_cast<float>(sign | 0x7f800000u | (mantissa << 13));
  return std::bit_cast<float>(sign | ((exponent + 112) << 23) | (mantissa << 13));
}

usize ReflectionProbeTexture::expectedTexels() const noexcept {
  usize total = 0;
  for (u32 mip = baseMip; mip < mips; ++mip) total += static_cast<usize>(mipSide(mip)) * mipSide(mip) * 4;
  return total;
}

bool ReflectionProbeTexture::valid(std::string *error) const {
  const auto fail = [&](const char *message) { if (error) *error = message; return false; };
  if (size != ReflectionProbeAtlasSize || mips != ReflectionProbeAtlasMips) return fail("tamanho do atlas inesperado");
  if (baseMip >= mips) return fail("mip base fora da cadeia");
  if (texels.size() != expectedTexels()) return fail("quantidade de texels não confere");
  return true;
}

std::vector<u8> ReflectionProbeTexture::serialize() const {
  std::vector<u8> out;
  const auto put32 = [&](u32 v) { for (u32 i = 0; i < 4; ++i) out.push_back(static_cast<u8>(v >> (i * 8))); };
  out.insert(out.end(), {'A', 'E', 'R', 'P'});
  put32(Version);
  put32(static_cast<u32>(guid.high));
  put32(static_cast<u32>(guid.high >> 32));
  put32(static_cast<u32>(guid.low));
  put32(static_cast<u32>(guid.low >> 32));
  put32(static_cast<u32>(sourceHash));
  put32(static_cast<u32>(sourceHash >> 32));
  u32 seconds;
  std::memcpy(&seconds, &bakeSeconds, 4);
  put32(seconds);
  put32(size);
  put32(baseMip);
  put32(mips);
  put32(hdr ? 1u : 0u);
  if (hdr) {
    for (u16 t : texels) { out.push_back(static_cast<u8>(t)); out.push_back(static_cast<u8>(t >> 8)); }
  } else {
    // Sem HDR a sonda é capturada em faixa baixa (0..1) e cabe em 8 bits.
    for (u16 t : texels) out.push_back(static_cast<u8>(std::lround(std::clamp(halfToFloat(t), 0.0f, 1.0f) * 255.0f)));
  }
  return out;
}

bool ReflectionProbeTexture::deserialize(std::span<const u8> bytes, ReflectionProbeTexture &out, std::string *error) {
  const auto fail = [&](const char *message) { if (error) *error = message; return false; };
  if (bytes.size() < 56 || bytes.size() > MaximumBytes) return fail("tamanho de arquivo inválido");
  if (std::memcmp(bytes.data(), "AERP", 4) != 0) return fail("assinatura AERP ausente");
  usize at = 4;
  const auto get32 = [&]() {
    u32 v = 0;
    for (u32 i = 0; i < 4; ++i) v |= static_cast<u32>(bytes[at + i]) << (i * 8);
    at += 4;
    return v;
  };
  ReflectionProbeTexture value;
  if (get32() != Version) return fail("versão de sonda desconhecida");
  const u64 h0 = get32(), h1 = get32(), l0 = get32(), l1 = get32();
  value.guid = {h0 | (h1 << 32), l0 | (l1 << 32)};
  const u64 s0 = get32(), s1 = get32();
  value.sourceHash = s0 | (s1 << 32);
  const u32 seconds = get32();
  std::memcpy(&value.bakeSeconds, &seconds, 4);
  value.size = get32();
  value.baseMip = get32();
  value.mips = get32();
  value.hdr = get32() != 0;
  if (value.size != ReflectionProbeAtlasSize || value.mips != ReflectionProbeAtlasMips || value.baseMip >= value.mips)
    return fail("cabeçalho de sonda inválido");
  const usize count = value.expectedTexels();
  const usize payload = value.hdr ? count * 2 : count;
  if (bytes.size() - at != payload) return fail("dados da sonda truncados");
  value.texels.resize(count);
  for (usize i = 0; i < count; ++i) {
    if (value.hdr) value.texels[i] = static_cast<u16>(bytes[at + i * 2] | (bytes[at + i * 2 + 1] << 8));
    else value.texels[i] = floatToHalf(static_cast<float>(bytes[at + i]) / 255.0f);
  }
  out = std::move(value);
  return true;
}

} // namespace ae::renderer
