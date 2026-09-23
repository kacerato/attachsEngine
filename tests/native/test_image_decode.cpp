// M09.1 — imagens embutidas e texturas do perfil de importação.
//
// Os PNG são montados aqui mesmo (zlib com blocos armazenados, sem compressão):
// nada é baixado e nenhum arquivo pessoal entra no teste. JPEG é exercitado
// pelos arquivos reais do probe `--reimport-glb`.
#include "harness.h"
#include "editor/editor_session.h"
#include "renderer/authoring_geometry.h"
#include "resources/gltf_import.h"
#include "resources/image_decode.h"
#include "resources/texture_asset.h"
#include "resources/texture_budget.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

using namespace ae;

namespace {
void putBigEndian(std::vector<u8> &out, u32 value) {
  for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<u8>(value >> shift));
}
void putLittle32(std::vector<u8> &out, u32 value) {
  for (u32 i = 0; i < 4; ++i) out.push_back(static_cast<u8>(value >> (i * 8)));
}
u32 crc32(const u8 *data, usize size) {
  static const auto table = [] {
    std::array<u32, 256> values{};
    for (u32 n = 0; n < 256; ++n) {
      u32 c = n;
      for (u32 k = 0; k < 8; ++k) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
      values[n] = c;
    }
    return values;
  }();
  u32 c = 0xffffffffu;
  for (usize i = 0; i < size; ++i) c = table[(c ^ data[i]) & 0xff] ^ (c >> 8);
  return c ^ 0xffffffffu;
}
void chunk(std::vector<u8> &png, const char type[5], const std::vector<u8> &data) {
  putBigEndian(png, static_cast<u32>(data.size()));
  std::vector<u8> body(type, type + 4);
  body.insert(body.end(), data.begin(), data.end());
  png.insert(png.end(), body.begin(), body.end());
  putBigEndian(png, crc32(body.data(), body.size()));
}
// PNG RGBA8 com zlib "stored": válido para qualquer decodificador.
std::vector<u8> makePng(u32 width, u32 height, const std::vector<u8> &rgba) {
  std::vector<u8> png{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  std::vector<u8> header;
  putBigEndian(header, width);
  putBigEndian(header, height);
  header.insert(header.end(), {8, 6, 0, 0, 0});
  chunk(png, "IHDR", header);
  std::vector<u8> raw;
  for (u32 y = 0; y < height; ++y) {
    raw.push_back(0);
    raw.insert(raw.end(), rgba.begin() + y * width * 4, rgba.begin() + (y + 1) * width * 4);
  }
  std::vector<u8> zlib{0x78, 0x01};
  usize offset = 0;
  do {
    const u16 length = static_cast<u16>(std::min<usize>(65535, raw.size() - offset));
    const bool last = offset + length == raw.size();
    zlib.push_back(last ? 1 : 0);
    zlib.push_back(static_cast<u8>(length));
    zlib.push_back(static_cast<u8>(length >> 8));
    zlib.push_back(static_cast<u8>(~length));
    zlib.push_back(static_cast<u8>(static_cast<u16>(~length) >> 8));
    zlib.insert(zlib.end(), raw.begin() + offset, raw.begin() + offset + length);
    offset += length;
  } while (offset < raw.size());
  u32 a = 1, b = 0;
  for (u8 value : raw) { a = (a + value) % 65521; b = (b + a) % 65521; }
  putBigEndian(zlib, (b << 16) | a);
  chunk(png, "IDAT", zlib);
  chunk(png, "IEND", {});
  return png;
}
// Tabuleiro 2x2: vermelho puro e preto nas diagonais.
std::vector<u8> checkerPixels() {
  return {255, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 0, 0, 255};
}

// GLB de uma tela com uma imagem PNG embutida e o material recebido.
std::vector<u8> texturedGlb(const std::vector<u8> &image, const std::string &material) {
  std::vector<u8> binary;
  const float positions[12]{0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
  for (float value : positions) {
    u32 bits = 0;
    std::memcpy(&bits, &value, 4);
    putLittle32(binary, bits);
  }
  for (u16 value : {u16{0}, u16{1}, u16{2}, u16{0}, u16{2}, u16{3}}) {
    binary.push_back(static_cast<u8>(value));
    binary.push_back(static_cast<u8>(value >> 8));
  }
  const usize imageOffset = binary.size();
  binary.insert(binary.end(), image.begin(), image.end());
  while (binary.size() % 4) binary.push_back(0);
  std::string json =
      std::string(R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":)") + std::to_string(binary.size()) + R"(}],)" +
      R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":12},)" +
      R"({"buffer":0,"byteOffset":)" + std::to_string(imageOffset) + R"(,"byteLength":)" + std::to_string(image.size()) + R"(}],)" +
      R"("accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":6,"type":"SCALAR"}],)" +
      R"("images":[{"bufferView":2,"mimeType":"image/png"}],)" +
      R"("samplers":[{"magFilter":9728,"minFilter":9728,"wrapS":33071,"wrapT":33648}],)" +
      // A segunda textura só existe numa extensão sem decodificador neste perfil.
      // Era KHR_texture_basisu até a Entrega 4 ligar o transcodificador KTX2.
      R"("textures":[{"source":0,"sampler":0},{"extensions":{"EXT_texture_webp":{"source":0}}}],)" +
      R"("materials":[)" + material + R"(],)" +
      R"("meshes":[{"name":"Tela","primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],)" +
      R"("nodes":[{"name":"Tela","mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
  while (json.size() % 4) json.push_back(' ');
  std::vector<u8> glb;
  putLittle32(glb, 0x46546C67);
  putLittle32(glb, 2);
  putLittle32(glb, static_cast<u32>(12 + 8 + json.size() + 8 + binary.size()));
  putLittle32(glb, static_cast<u32>(json.size()));
  putLittle32(glb, 0x4E4F534A);
  glb.insert(glb.end(), json.begin(), json.end());
  putLittle32(glb, static_cast<u32>(binary.size()));
  putLittle32(glb, 0x004E4942);
  glb.insert(glb.end(), binary.begin(), binary.end());
  return glb;
}

const char *kFullMaterial =
    R"({"name":"Tela","pbrMetallicRoughness":{"baseColorTexture":{"index":0,"extensions":{"KHR_texture_transform":{"scale":[2,2]}}},)"
    R"("metallicRoughnessTexture":{"index":0}},"normalTexture":{"index":0,"scale":0.5,"texCoord":1},)"
    R"("emissiveTexture":{"index":1},"occlusionTexture":{"index":0}})";
} // namespace

AE_TEST(m091_png_decodes_to_rgba_and_refuses_oversized_headers_before_allocating) {
  const auto png = makePng(2, 2, checkerPixels());
  AE_EXPECT_TRUE(resources::detectImageContainer(png) == resources::ImageContainer::Png, "assinatura PNG");
  resources::DecodedImage image;
  std::string diagnostic;
  AE_EXPECT_TRUE(resources::decodeImageRgba8(png, {}, image, diagnostic), diagnostic.c_str());
  AE_EXPECT_EQ(image.width, 2u, "largura");
  AE_EXPECT_TRUE(image.rgba == checkerPixels(), "pixels exatos, linha 0 no topo");

  // Cabeçalho dizendo 9000x9000 com corpo minúsculo: a recusa vem do tamanho
  // declarado, antes de qualquer alocação de pixels.
  auto huge = makePng(1, 1, {1, 2, 3, 4});
  const u8 big[4]{0, 0, 0x23, 0x28}; // 9000
  std::memcpy(huge.data() + 16, big, 4);
  std::memcpy(huge.data() + 20, big, 4);
  u32 crc = crc32(huge.data() + 12, 17);
  for (int i = 0; i < 4; ++i) huge[29 + i] = static_cast<u8>(crc >> (24 - 8 * i));
  AE_EXPECT_TRUE(!resources::decodeImageRgba8(huge, {}, image, diagnostic), "9000x9000 recusada");
  AE_EXPECT_TRUE(diagnostic.find("limite") != std::string::npos, "com o motivo do limite");
  AE_EXPECT_TRUE(image.rgba.empty(), "saída vazia na falha");

  const std::vector<u8> gif{'G', 'I', 'F', '8', '9', 'a'};
  AE_EXPECT_TRUE(!resources::decodeImageRgba8(gif, {}, image, diagnostic), "formato fora do perfil recusado");
}

AE_TEST(m091_mip_chain_averages_color_in_linear_space_and_data_directly) {
  resources::DecodedImage image;
  image.width = image.height = 2;
  image.rgba = checkerPixels();
  std::vector<u8> chain;
  u32 levels = 0;
  AE_EXPECT_TRUE(resources::buildMipChain(image, true, chain, levels), "cadeia sRGB");
  AE_EXPECT_EQ(levels, 2u, "2x2 e 1x1");
  AE_EXPECT_EQ(chain.size(), usize{20}, "16 + 4 bytes");
  // Metade da energia em linear é 0,5; em sRGB, ~188 — não 128.
  AE_EXPECT_TRUE(chain[16] >= 187 && chain[16] <= 188, "média de cor feita em espaço linear");
  AE_EXPECT_EQ(chain[19], u8{255}, "alfa médio direto");
  AE_EXPECT_TRUE(resources::buildMipChain(image, false, chain, levels), "cadeia de dados");
  AE_EXPECT_EQ(chain[16], u8{128}, "mapa de dados: média direta");
}

AE_TEST(texture_cook_keeps_npot_energy_and_renormalizes_normal_mips) {
  resources::DecodedImage npot{3, 1, {0,0,0,255, 30,0,0,255, 90,0,0,255}};
  std::vector<u8> chain; u32 levels = 0;
  AE_EXPECT_TRUE(resources::buildMipChain(npot, false, chain, levels), "NPOT data mip");
  AE_EXPECT_TRUE(levels == 2 && chain[12] == 40, "3 -> 1 includes every source texel");

  // +X, +Y, +X, +Y: normalized average is approximately (.707,.707,0),
  // encoded near (218,218,128), rather than the raw byte average (191,191,128).
  resources::DecodedImage normals{2, 2, {
    255,128,128,255, 128,255,128,255,
    255,128,128,255, 128,255,128,255}};
  AE_EXPECT_TRUE(resources::buildNormalMipChain(normals, chain, levels), "normal mip");
  AE_EXPECT_TRUE(levels == 2 && chain[16] >= 217 && chain[16] <= 219 &&
                 chain[17] >= 217 && chain[17] <= 219, "normal mip renormalized");
}

AE_TEST(texture_asset_recipe_cooks_and_cache_round_trips) {
  const auto png = makePng(2, 2, {255,128,128,255, 128,255,128,128,
                                  255,128,128,255, 128,255,128,255});
  resources::TextureProfile settings;
  settings.interpretation = resources::TextureInterpretationNormal;
  settings.invertNormalGreen = true;
  settings.anisotropy = false;
  resources::TextureImportLimits limits;
  resources::PreparedTextureImport prepared;
  AE_EXPECT_TRUE(resources::prepareTextureImport(
      png, settings, true, renderer::AuthoringTextureLinearFilter, limits, nullptr, prepared),
      prepared.diagnostic.c_str());
  AE_EXPECT_TRUE(prepared.valid() && !prepared.texture->srgb && prepared.texture->levels == 2,
                 "normal is linear with full mip chain");
  AE_EXPECT_TRUE((prepared.texture->samplerFlags & renderer::AuthoringTextureNoAnisotropy) != 0,
                 "anisotropy setting reaches cooked sampler");
  AE_EXPECT_TRUE(prepared.sourceHasAlpha, "alpha metadata comes from source");
  std::vector<u8> cache;
  AE_EXPECT_TRUE(resources::writeTextureAssetCache(prepared, cache), "cache writes");
  resources::PreparedTextureImport reopened;
  AE_EXPECT_TRUE(resources::readTextureAssetCache(cache, prepared.cacheKey, limits, reopened),
                 "cache reads with expected recipe key");
  AE_EXPECT_TRUE(reopened.valid() && reopened.texture->mipChain == prepared.texture->mipChain &&
                 reopened.texture->samplerFlags == prepared.texture->samplerFlags,
                 "cache preserves cooked payload and sampler");
  cache.back() ^= 1u;
  AE_EXPECT_TRUE(!resources::readTextureAssetCache(cache, prepared.cacheKey, limits, reopened),
                 "checksum rejects modified cache");

  const auto larger = makePng(4, 4, std::vector<u8>(4u * 4u * 4u, 128u));
  resources::TextureImportLimits tight = limits;
  // RGBA source 64 + reserved chain 84 + first generated mip 16 coexist.
  // The previous preflight counted only source+chain (148) and accepted 150.
  tight.maximumWorkingBytes = 150;
  AE_EXPECT_TRUE(!resources::prepareTextureImport(
      larger, {}, true, renderer::AuthoringTextureLinearFilter, tight, nullptr, reopened),
      "working budget includes the current, next and reserved mip chain");
}

AE_TEST(texture_alpha_coverage_selects_closest_representable_mip_mask) {
  resources::TextureProfile profile;
  profile.preserveAlphaCoverage = true;
  resources::TextureImportLimits limits;
  resources::PreparedTextureImport cooked;
  const auto cook = [&](const std::vector<u8> &pixels, float cutoff,
                        resources::PreparedTextureImport &result) {
    profile.alphaCoverageCutoff = cutoff;
    return resources::prepareTextureImport(makePng(4, 4, pixels), profile, true, 0,
                                           limits, nullptr, result);
  };
  std::vector<u8> sparse(4u * 4u * 4u, 255u);
  for (u32 pixel = 0; pixel < 16; ++pixel)
    sparse[pixel * 4u + 3u] = (pixel % 4u % 2u == 0u && pixel / 4u % 2u == 0u) ? 255u : 0u;
  AE_EXPECT_TRUE(cook(sparse, .1f, cooked), cooked.diagnostic.c_str());
  // Cada bloco 2x2 tem um texel visível. A média alfa 64 passaria no corte
  // 0,1 e cobriria 100%; zero é a máscara discreta mais próxima de 25%.
  for (usize at = 4u * 4u * 4u + 3u; at < 4u * 4u * 4u + 16u; at += 4u)
    AE_EXPECT_EQ(cooked.texture->mipChain[at], u8{0}, "mip esparso escolhe cobertura zero");

  std::vector<u8> dense(4u * 4u * 4u, 255u);
  for (u32 pixel = 0; pixel < 16; ++pixel)
    dense[pixel * 4u + 3u] = (pixel % 4u % 2u == 1u && pixel / 4u % 2u == 1u) ? 0u : 255u;
  AE_EXPECT_TRUE(cook(dense, .9f, cooked), cooked.diagnostic.c_str());
  // Três texels opacos por bloco ficam em alfa 191; expandir todos é mais
  // próximo da cobertura original de 75% que manter todos invisíveis.
  for (usize at = 4u * 4u * 4u + 3u; at < 4u * 4u * 4u + 16u; at += 4u)
    AE_EXPECT_TRUE(cooked.texture->mipChain[at] >= 230u, "mip denso amplia cobertura");

  profile.interpretation = resources::TextureInterpretationData;
  AE_EXPECT_TRUE(resources::prepareTextureImport(makePng(4, 4, dense), profile, true, 0,
                                                  limits, nullptr, cooked), cooked.diagnostic.c_str());
  AE_EXPECT_EQ(cooked.texture->mipChain[4u * 4u * 4u + 3u], u8{191},
               "preservação de cobertura não modifica mapa de dados");
  std::atomic<bool> cancel{true};
  AE_EXPECT_TRUE(!resources::prepareTextureImport(makePng(4, 4, dense), profile, true, 0,
                                                   limits, &cancel, cooked) && !cooked.valid(),
                 "cancelamento não entrega derivado parcial");
}

AE_TEST(m091_glb_textures_map_to_slots_with_color_space_uv_and_sampler) {
  const auto glb = texturedGlb(makePng(2, 2, checkerPixels()), kFullMaterial);
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.textures.size(), usize{3},
               "a mesma imagem usa receitas distintas para cor, dados e normal");
  const auto &material = model.materials.front();
  const auto base = material.textureIndices[0], normal = material.textureIndices[1], mr = material.textureIndices[2];
  AE_EXPECT_TRUE(base < 3 && model.textures[base]->srgb, "cor base em sRGB");
  AE_EXPECT_TRUE(normal < 3 && mr < 3 && !model.textures[normal]->srgb && !model.textures[mr]->srgb && normal != mr,
                 "normal renormalizada e metálico/rugosidade linear mantêm receitas separadas");
  AE_EXPECT_TRUE(material.textureIndices[3] == renderer::InvalidMapTexture, "emissivo só em WebP não foi aplicado");
  AE_EXPECT_TRUE((material.flags & renderer::MapMaterialNormalMap) && (material.flags & renderer::MapMaterialMetallicRoughnessMap),
                 "flags dos mapas que o shader usa");
  AE_EXPECT_TRUE(!(material.flags & renderer::MapMaterialEmissiveMap), "sem flag de mapa não aplicado");
  AE_EXPECT_EQ(material.normalScale, .5f, "escala da normal");
  AE_EXPECT_EQ(material.textureCoordinates, u32{1u << 2}, "normal em TEXCOORD_1, o resto em TEXCOORD_0");
  const auto flags = model.textures[base]->samplerFlags;
  AE_EXPECT_TRUE(!(flags & renderer::AuthoringTextureLinearFilter), "magFilter NEAREST");
  AE_EXPECT_TRUE(!(flags & renderer::AuthoringTextureLinearMip), "minFilter sem mipmap linear");
  AE_EXPECT_TRUE(!(flags & renderer::AuthoringTextureRepeatU) && !(flags & renderer::AuthoringTextureMirrorU), "wrapS CLAMP");
  AE_EXPECT_TRUE(flags & renderer::AuthoringTextureMirrorV, "wrapT MIRRORED_REPEAT");
  AE_EXPECT_TRUE(model.textures[base]->valid() && model.textures[base]->levels == 2, "mips completos");
  AE_EXPECT_EQ(model.skippedTextures, 1u, "uma referência não aplicada");
  AE_EXPECT_EQ(model.unappliedTextureTransforms, 1u, "transformação de UV declarada");
  // R4: a oclusão deste material está na MESMA textura e UV do metálico/rugosidade.
  AE_EXPECT_EQ(model.appliedOcclusion, 1u, "oclusão empacotada no metálico/rugosidade é aplicada (ORM)");
  AE_EXPECT_EQ(model.unappliedOcclusion, 0u, "nenhuma oclusão perdida");
  AE_EXPECT_TRUE((material.flags & renderer::MapMaterialOcclusionInMetallicRoughness) != 0, "flag que o shader lê");
  {
    // Oclusão numa textura diferente da de metálico/rugosidade não tem slot: declarada.
    const auto separate = texturedGlb(makePng(2, 2, checkerPixels()),
        R"({"pbrMetallicRoughness":{"metallicRoughnessTexture":{"index":0}},"occlusionTexture":{"index":1}})");
    resources::GltfImport other;
    AE_EXPECT_TRUE(resources::importGlb(separate, {}, {}, other), other.diagnostic.c_str());
    AE_EXPECT_TRUE(other.unappliedOcclusion == 1u && other.appliedOcclusion == 0u, "oclusão separada declarada como não aplicada");
    AE_EXPECT_TRUE(!(other.materials.front().flags & renderer::MapMaterialOcclusionInMetallicRoughness), "sem flag de oclusão");
    // Força diferente de 1 também não cabe na leitura direta do canal.
    const auto weak = texturedGlb(makePng(2, 2, checkerPixels()),
        R"({"pbrMetallicRoughness":{"metallicRoughnessTexture":{"index":0}},"occlusionTexture":{"index":0,"strength":0.5}})");
    resources::GltfImport faint;
    AE_EXPECT_TRUE(resources::importGlb(weak, {}, {}, faint), faint.diagnostic.c_str());
    AE_EXPECT_EQ(faint.unappliedOcclusion, 1u, "força 0,5 declarada como não aplicada");
  }
  bool webp = false;
  for (const auto &note : model.textureNotes) webp |= note.find("WebP") != std::string::npos;
  AE_EXPECT_TRUE(webp, "o motivo cita o formato sem decodificador");

  resources::GltfImportLimits tight;
  tight.maximumTextureBytes = 8;
  resources::GltfImport starved;
  AE_EXPECT_TRUE(resources::importGlb(glb, tight, {}, starved), "orçamento esgotado não derruba a geometria");
  AE_EXPECT_TRUE(starved.textures.empty() && starved.skippedTextures >= 3, "texturas ficam de fora, contadas");
  bool budget = false;
  for (const auto &note : starved.textureNotes) budget |= note.find("Orçamento") != std::string::npos;
  AE_EXPECT_TRUE(budget, "e o motivo é o orçamento");
}

AE_TEST(m091_normal_mapped_primitive_without_tangent_gets_a_valid_tangent_basis) {
  const auto glb = texturedGlb(makePng(2, 2, checkerPixels()), kFullMaterial);
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.generatedTangentPrimitives, 1u, "a primitiva sem TANGENT com mapa normal ganhou tangentes");
  const usize vertices = model.vertices.size() / renderer::MapVertexStride;
  AE_EXPECT_EQ(vertices, usize{4}, "quatro vértices");
  for (usize v = 0; v < vertices; ++v) {
    i16 normal[4]{}, tangent[4]{};
    std::memcpy(normal, model.vertices.data() + v * renderer::MapVertexStride + 12, 8);
    std::memcpy(tangent, model.vertices.data() + v * renderer::MapVertexStride + 20, 8);
    const float t[3]{tangent[0] / 32767.0f, tangent[1] / 32767.0f, tangent[2] / 32767.0f};
    const float n[3]{normal[0] / 32767.0f, normal[1] / 32767.0f, normal[2] / 32767.0f};
    AE_EXPECT_TRUE(std::fabs(std::hypot(t[0], t[1], t[2]) - 1.0f) < 1e-3f, "tangente unitária, nunca zero");
    AE_EXPECT_TRUE(std::fabs(t[0] * n[0] + t[1] * n[1] + t[2] * n[2]) < 1e-3f, "perpendicular à normal");
    AE_EXPECT_TRUE(tangent[3] == 32767 || tangent[3] == -32767, "sinal de orientação definido");
  }

  // Sem mapa normal não há o que gerar: o vértice fica como veio.
  resources::GltfImport plain;
  AE_EXPECT_TRUE(resources::importGlb(texturedGlb(makePng(2, 2, checkerPixels()),
      R"({"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}})"), {}, {}, plain), plain.diagnostic.c_str());
  AE_EXPECT_EQ(plain.generatedTangentPrimitives, 0u, "sem mapa normal, nenhuma tangente gerada");
}

AE_TEST(m091_large_textures_are_reduced_to_the_resident_limit_instead_of_dropped) {
  std::vector<u8> pixels;
  for (u32 i = 0; i < 16; ++i) pixels.insert(pixels.end(), {200, 100, 50, 255});
  const auto glb = texturedGlb(makePng(4, 4, pixels), R"({"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}})");
  resources::GltfImportLimits limits;
  limits.maximumTextureDimension = 2;
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, limits, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.textures.size(), usize{1}, "a textura entrou");
  AE_EXPECT_EQ(model.textures.front()->width, 2u, "com o maior lado no limite residente");
  AE_EXPECT_EQ(model.textures.front()->levels, 2u, "e os mips restantes (2x2, 1x1)");
  AE_EXPECT_TRUE(model.textures.front()->valid(), "cadeia consistente depois do corte");
  AE_EXPECT_EQ(model.reducedTextures, 1u, "a redução é contada");
  AE_EXPECT_EQ(model.textureBytes, u64{20}, "o orçamento vale sobre o que reside");
  AE_EXPECT_EQ(model.skippedTextures, 0u, "nada foi descartado");
}

AE_TEST(m091_budget_lowers_resolution_for_every_texture_before_dropping_any) {
  std::vector<u8> pixels;
  for (u32 i = 0; i < 16; ++i) pixels.insert(pixels.end(), {10, 200, 90, 255});
  // A mesma imagem como cor (sRGB) e como metálico/rugosidade (linear) vira duas
  // texturas de 84 bytes em 4x4; o orçamento só comporta as duas em 2x2 (20 cada).
  const auto glb = texturedGlb(makePng(4, 4, pixels),
      R"({"pbrMetallicRoughness":{"baseColorTexture":{"index":0},"metallicRoughnessTexture":{"index":0}}})");
  resources::GltfImportLimits limits;
  limits.maximumTextureBytes = 50;
  limits.minimumTextureDimension = 1;
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, limits, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.textures.size(), usize{2}, "as duas texturas entraram");
  AE_EXPECT_EQ(model.skippedTextures, 0u, "nenhuma foi descartada pela ordem do arquivo");
  AE_EXPECT_EQ(model.residentTextureDimension, 2u, "o limite do arquivo caiu pela metade");
  AE_EXPECT_EQ(model.reducedTextures, 2u, "e valeu para todas");
  AE_EXPECT_EQ(model.textureBytes, u64{40}, "dentro do orçamento");

  // Com piso acima do necessário, a redução para no piso e o orçamento descarta.
  limits.minimumTextureDimension = 4;
  limits.maximumTextureBytes = 100; // cabe uma de 84 bytes, não as duas
  resources::GltfImport floored;
  AE_EXPECT_TRUE(resources::importGlb(glb, limits, {}, floored), floored.diagnostic.c_str());
  AE_EXPECT_EQ(floored.residentTextureDimension, 4u, "o piso segura a resolução");
  AE_EXPECT_EQ(floored.skippedTextures, 1u, "e só então uma textura fica de fora");
}

AE_TEST(m091_publication_offsets_texture_indices_per_source) {
  editor::EditorSession session;
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, vertices, indices, draws, materials);
  session.importMap(draws, materials, false, vertices, indices, 0);
  std::vector<renderer::MapMaterialRecord> publishedMaterials;
  usize publishedTextures = 0;
  std::vector<u8> ownedVertices;
  std::vector<u32> ownedIndices;
  std::vector<renderer::MapDrawRecord> ownedDraws;
  std::vector<renderer::MapMaterialRecord> ownedMaterials;
  session.setGeometryPublisher([&](std::span<const u8> v, std::span<const u32> i, std::span<const renderer::MapDrawRecord> d,
                                   std::span<const renderer::MapMaterialRecord> m,
                                   std::span<const renderer::SharedAuthoringTexture> textures,
                                   editor::EditorSession::PublishedGeometry &out) {
    publishedMaterials.assign(m.begin(), m.end());
    publishedTextures = textures.size();
    for (const auto &texture : textures) if (!texture || !texture->valid()) return false;
    ownedVertices.clear(); ownedIndices.clear(); ownedDraws.clear(); ownedMaterials.clear();
    if (!renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, ownedVertices, ownedIndices, ownedDraws, ownedMaterials)) return false;
    const auto vertexBase = static_cast<u32>(ownedVertices.size() / renderer::MapVertexStride);
    const auto indexBase = static_cast<u32>(ownedIndices.size());
    const auto materialBase = static_cast<u32>(ownedMaterials.size());
    ownedVertices.insert(ownedVertices.end(), v.begin(), v.end());
    ownedIndices.insert(ownedIndices.end(), i.begin(), i.end());
    ownedMaterials.insert(ownedMaterials.end(), m.begin(), m.end());
    for (auto draw : d) {
      draw.firstIndex += indexBase; draw.vertexOffset += vertexBase; draw.materialIndex += materialBase;
      draw.lodGroupId = static_cast<u32>(ownedDraws.size());
      ownedDraws.push_back(draw);
    }
    out = {ownedDraws, ownedMaterials, ownedVertices, ownedIndices};
    return true;
  });
  const auto glb = texturedGlb(makePng(2, 2, checkerPixels()), kFullMaterial);
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, model), model.diagnostic.c_str());
  editor::EditorSession::ModelImportReport report;
  // O registro exige hash de conteúdo no formato SHA-256 (64 hexadecimais).
  AE_EXPECT_TRUE(session.publishModel(model, std::string(64, 'a'), "Fontes/a.glb", report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.publishModel(model, std::string(64, 'b'), "Fontes/b.glb", report), report.diagnostic.c_str());
  AE_EXPECT_EQ(publishedTextures, usize{4}, "as texturas das duas fontes chegam na mesma publicação");
  // Cada fonte tem 1 material + o neutro; o da segunda fonte é o terceiro.
  AE_EXPECT_TRUE(publishedMaterials.size() >= 3, "materiais das duas fontes");
  const auto first = publishedMaterials[0].textureIndices[0], second = publishedMaterials[2].textureIndices[0];
  AE_EXPECT_TRUE(first < 2 && second == first + 2, "índices da segunda fonte deslocados pelo bloco da primeira");

  // R2: a publicação passa pelo orçamento agregado e deixa o relatório.
  AE_EXPECT_TRUE(session.textureResidency().textures >= 1u, "texturas contadas na publicação");
  AE_EXPECT_TRUE(session.textureResidency().withinBudget(), "o teto padrão comporta o teste");
  // Teto zero com piso padrão (256 px): nada abaixo do piso, e o relatório diz que não coube.
  session.setImportTextureBudget(0);
  AE_EXPECT_TRUE(session.publishModel(model, std::string(64, 'c'), "Fontes/c.glb", report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(!session.textureResidency().withinBudget(), "acima do teto é informado, não escondido");
  AE_EXPECT_EQ(session.textureResidency().reducedTextures, 0u, "o piso protege texturas pequenas");
}

namespace {
// Cadeia RGBA8 (ou ASTC 4x4) completa; cada byte guarda o número do nível, para
// o teste ver qual fatia sobrou depois da redução.
renderer::SharedAuthoringTexture chainTexture(u32 width, u32 height, u32 format = renderer::AuthoringTextureRgba8) {
  auto texture = std::make_shared<renderer::AuthoringTexture>();
  texture->width = width;
  texture->height = height;
  texture->format = format;
  for (u32 w = width, h = height;; w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1) {
    const u64 bytes = format == renderer::AuthoringTextureAstc4x4 ? u64{(w + 3) / 4} * ((h + 3) / 4) * 16 : u64{w} * h * 4;
    texture->mipChain.insert(texture->mipChain.end(), static_cast<usize>(bytes), static_cast<u8>(texture->levels));
    ++texture->levels;
    if (w == 1 && h == 1) break;
  }
  return texture;
}
} // namespace

AE_TEST(r2_texture_budget_reduces_residency_across_sources_without_touching_originals) {
  const auto big = chainTexture(1024, 1024), mid = chainTexture(512, 512);
  AE_EXPECT_TRUE(big->valid() && mid->valid(), "cadeias de teste válidas");
  const u64 bigBytes = big->mipChain.size(), midBytes = mid->mipChain.size();

  // A mesma textura duas vezes na lista conta uma vez e é reduzida uma vez.
  std::vector<renderer::SharedAuthoringTexture> list{big, mid, big};
  auto report = resources::applyTextureBudget(list, 3'000'000, 256);
  AE_EXPECT_EQ(report.textures, 2u, "duas texturas distintas");
  AE_EXPECT_EQ(report.requestedBytes, bigBytes + midBytes, "compartilhada contada uma vez");
  AE_EXPECT_EQ(report.reducedTextures, 1u, "só a maior precisou cair");
  AE_EXPECT_EQ(report.droppedLevels, 1u, "um nível basta");
  AE_EXPECT_TRUE(report.withinBudget() && report.residentBytes <= 3'000'000u, "cabe no teto");
  AE_EXPECT_TRUE(list[0] == list[2] && list[0] != big, "as duas entradas apontam para a mesma cópia reduzida");
  AE_EXPECT_EQ(list[0]->width, 512u, "resolução residente");
  AE_EXPECT_TRUE(list[0]->valid() && list[0]->mipChain.front() == 1, "a cadeia começa no antigo nível 1");
  AE_EXPECT_TRUE(list[1] == mid, "a que cabia não foi copiada");
  AE_EXPECT_TRUE(big->width == 1024 && big->mipChain.size() == bigBytes, "original intacta");

  // Teto impossível: tudo desce até o piso e o relatório admite que não coube.
  std::vector<renderer::SharedAuthoringTexture> floor{big, mid};
  report = resources::applyTextureBudget(floor, 1000, 256);
  AE_EXPECT_EQ(floor[0]->width, 256u, "maior para no piso");
  AE_EXPECT_EQ(floor[1]->width, 256u, "menor para no piso");
  AE_EXPECT_EQ(report.droppedLevels, 3u, "2 níveis da maior + 1 da menor");
  AE_EXPECT_TRUE(!report.withinBudget(), "acima do teto é dito");

  // ASTC 4x4: o nível de cima tem tamanho em blocos, não em pixels.
  std::vector<renderer::SharedAuthoringTexture> astc{chainTexture(8, 8, renderer::AuthoringTextureAstc4x4)};
  report = resources::applyTextureBudget(astc, 60, 1);
  AE_EXPECT_EQ(report.requestedBytes, u64{112}, "64 + 16 + 16 + 16");
  AE_EXPECT_TRUE(report.withinBudget() && astc[0]->width == 4 && astc[0]->valid(), "ASTC reduzido em blocos inteiros");
}
