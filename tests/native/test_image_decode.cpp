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
      R"("textures":[{"source":0,"sampler":0},{"extensions":{"KHR_texture_basisu":{"source":0}}}],)" +
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

AE_TEST(m091_glb_textures_map_to_slots_with_color_space_uv_and_sampler) {
  const auto glb = texturedGlb(makePng(2, 2, checkerPixels()), kFullMaterial);
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.textures.size(), usize{2}, "a mesma imagem em sRGB (cor) e em linear (dados)");
  const auto &material = model.materials.front();
  const auto base = material.textureIndices[0], normal = material.textureIndices[1], mr = material.textureIndices[2];
  AE_EXPECT_TRUE(base < 2 && model.textures[base]->srgb, "cor base em sRGB");
  AE_EXPECT_TRUE(normal < 2 && !model.textures[normal]->srgb && normal == mr, "normal e metálico/rugosidade lineares, mesma textura");
  AE_EXPECT_TRUE(material.textureIndices[3] == renderer::InvalidMapTexture, "emissivo só em KTX2 não foi aplicado");
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
  AE_EXPECT_EQ(model.unappliedOcclusion, 1u, "oclusão declarada");
  bool ktx = false;
  for (const auto &note : model.textureNotes) ktx |= note.find("KTX2") != std::string::npos;
  AE_EXPECT_TRUE(ktx, "o motivo cita o formato sem decodificador");

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
}
