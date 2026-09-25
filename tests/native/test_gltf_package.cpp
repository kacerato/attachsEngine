// M08/M09 Entrega 4, trilha 3 — dependências externas de glTF empacotadas num GLB.
#include "harness.h"
#include "core/sha256.h"
#include "resources/gltf_import.h"
#include "resources/gltf_folder_source.h"
#include "resources/gltf_package.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace ae;

namespace {
std::vector<u8> fromBase64(std::string_view text) {
  std::vector<u8> out;
  u32 accumulator = 0;
  int bits = 0;
  for (const char c : text) {
    int value = -1;
    if (c >= 'A' && c <= 'Z') value = c - 'A';
    else if (c >= 'a' && c <= 'z') value = c - 'a' + 26;
    else if (c >= '0' && c <= '9') value = c - '0' + 52;
    else if (c == '+') value = 62;
    else if (c == '/') value = 63;
    if (value < 0) continue;
    accumulator = (accumulator << 6) | static_cast<u32>(value);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<u8>(accumulator >> bits));
    }
  }
  return out;
}

std::string toBase64(std::span<const u8> bytes) {
  static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (usize i = 0; i < bytes.size(); i += 3) {
    const u32 chunk = (u32{bytes[i]} << 16) | (i + 1 < bytes.size() ? u32{bytes[i + 1]} << 8 : 0) |
                      (i + 2 < bytes.size() ? u32{bytes[i + 2]} : 0);
    out.push_back(table[(chunk >> 18) & 63]);
    out.push_back(table[(chunk >> 12) & 63]);
    out.push_back(i + 1 < bytes.size() ? table[(chunk >> 6) & 63] : '=');
    out.push_back(i + 2 < bytes.size() ? table[chunk & 63] : '=');
  }
  return out;
}

// PNG 1x1 RGBA, o de uso comum em exemplos de data URI.
const std::vector<u8> kPixel =
    fromBase64("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==");

std::vector<u8> quadBin() {
  const float positions[12]{0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
  const float uvs[8]{0, 0, 1, 0, 1, 1, 0, 1};
  const u16 indices[6]{0, 1, 2, 0, 2, 3};
  std::vector<u8> bin(92, 0);
  std::memcpy(bin.data(), positions, 48);
  std::memcpy(bin.data() + 48, uvs, 32);
  std::memcpy(bin.data() + 80, indices, 12);
  return bin;
}

std::string gltfText(const std::string &bufferUri, const std::string &imageUri) {
  return std::string(R"({"asset":{"version":"2.0"},"buffers":[{"uri":")") + bufferUri + R"(","byteLength":92}],)" +
         R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":32},)" +
         R"({"buffer":0,"byteOffset":80,"byteLength":12}],)" +
         R"("accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},)" +
         R"({"bufferView":1,"componentType":5126,"count":4,"type":"VEC2"},{"bufferView":2,"componentType":5123,"count":6,"type":"SCALAR"}],)" +
         R"("images":[{"uri":")" + imageUri + R"("}],"textures":[{"source":0}],)" +
         R"("materials":[{"name":"Grade \"A\"","pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],)" +
         R"("meshes":[{"name":"Tela","primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}],)" +
         R"("nodes":[{"name":"Tela","mesh":0,"translation":[0.25,-1.5e-3,3]}],"scenes":[{"nodes":[0]}],"scene":0})";
}

std::span<const u8> bytesOf(const std::string &text) {
  return {reinterpret_cast<const u8 *>(text.data()), text.size()};
}
} // namespace

// Sonda de arquivos reais: o mesmo empacotamento do seletor, com os nomes dos
// arquivos como o provedor os declararia (só o nome, sem pasta).
int probePackGltf(int count, char **paths) {
  const auto read = [](const char *path, std::vector<u8> &bytes) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return true;
  };
  const auto baseName = [](std::string path) {
    const auto slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
  };
  std::vector<u8> main;
  if (!read(paths[0], main)) { std::fprintf(stderr, "READ %s\n", paths[0]); return 2; }
  std::vector<std::vector<u8>> storage(static_cast<usize>(count - 1));
  std::vector<resources::GltfPackageFile> files;
  for (int i = 1; i < count; ++i) {
    if (!read(paths[i], storage[static_cast<usize>(i - 1)])) { std::fprintf(stderr, "READ %s\n", paths[i]); return 2; }
    files.push_back({baseName(paths[i]), storage[static_cast<usize>(i - 1)]});
  }
  using Clock = std::chrono::steady_clock;
  const auto start = Clock::now();
  resources::GltfPackage package;
  std::string diagnostic;
  if (!resources::packGltf(main, files, 256ull << 20, package, diagnostic)) {
    std::printf("%s: RECUSADO: %s\n", paths[0], diagnostic.c_str());
    return 1;
  }
  const auto packed = Clock::now();
  resources::GltfImport model;
  const bool imported = resources::importGlb(package.glb, {}, {}, model);
  const auto done = Clock::now();
  const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
  std::printf("%s: companions=%d dependencies=%zu unused=%u packed_mb=%.1f pack_ms=%.1f import=%d import_ms=%.1f draws=%zu textures=%zu skipped_textures=%u\n",
              paths[0], count - 1, package.dependencies.size(), package.unusedFiles,
              static_cast<double>(package.glb.size()) / 1048576.0, ms(start, packed), imported ? 1 : 0, ms(packed, done),
              model.draws.size(), model.textures.size(), model.skippedTextures);
  if (!imported) std::printf("  import: %s\n", model.diagnostic.c_str());
  return imported ? 0 : 1;
}

AE_TEST(m09e4_gltf_with_companion_bin_and_image_packs_into_a_self_contained_glb) {
  AE_EXPECT_TRUE(kPixel.size() > 60, "PNG de 1 pixel decodificado do base64");
  const auto bin = quadBin();
  const std::string text = gltfText("cena.bin", "texturas/grade%20a.png");
  const std::string note = "não é referenciado";
  const std::vector<resources::GltfPackageFile> files{
      {"cena.bin", bin}, {"grade a.png", kPixel}, {"leia-me.txt", bytesOf(note)}};
  AE_EXPECT_TRUE(resources::gltfNeedsPackage(bytesOf(text)), ".gltf de texto sempre precisa de empacotamento");
  resources::GltfPackage package;
  std::string diagnostic;
  AE_EXPECT_TRUE(resources::packGltf(bytesOf(text), files, 64u << 20, package, diagnostic), diagnostic.c_str());
  AE_EXPECT_EQ(package.dependencies.size(), usize{2}, "o .bin e a imagem");
  AE_EXPECT_EQ(package.unusedFiles, 1u, "o arquivo escolhido e não referenciado é contado");
  AE_EXPECT_TRUE(package.dependencies.size() == 2 && package.dependencies[0].file == "cena.bin" &&
                     package.dependencies[0].sha256 == Sha256::hex(bin) && package.dependencies[1].file == "grade a.png",
                 "cada dependência com o arquivo que casou e o hash do conteúdo");
  AE_EXPECT_TRUE(!resources::gltfNeedsPackage(package.glb), "o GLB empacotado não depende de mais nada");

  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(package.glb, {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.draws.size(), usize{1}, "a malha veio do .bin");
  AE_EXPECT_EQ(model.textures.size(), usize{1}, "a imagem veio do arquivo ao lado");
  AE_EXPECT_EQ(model.skippedTextures, 0u, "nada ficou de fora");
  AE_EXPECT_TRUE(!model.materialNames.empty() && model.materialNames.front() == "Grade \"A\"", "texto com escape preservado");
  AE_EXPECT_TRUE(!model.nodes.empty() && model.nodes.front().localMatrix[12] == 0.25f &&
                     model.nodes.front().localMatrix[13] == -1.5e-3f && model.nodes.front().localMatrix[14] == 3.0f,
                 "números reescritos sem perder precisão");

  const auto manifest = resources::serializeGltfManifest("cena.gltf", bytesOf(text), package);
  AE_EXPECT_TRUE(manifest.starts_with("ASTRA_GLTF_DEPS 1\n") && manifest.find("\"grade a.png\"") != std::string::npos &&
                     manifest.find(Sha256::hex(bin)) != std::string::npos && manifest.find("packed ") != std::string::npos,
                 "manifesto com origem, tamanho e hash de cada byte");
}

AE_TEST(gltf_sampler_preserves_independent_min_mag_and_mip_modes) {
  struct Case { u32 minFilter; u32 magFilter; bool minLinear; bool magLinear; bool mipEnabled; bool mipLinear; };
  constexpr Case cases[]{
      {9728,9729,false,true,false,false}, {9729,9728,true,false,false,false},
      {9984,9729,false,true,true,false},  {9985,9729,true,true,true,false},
      {9986,9729,false,true,true,true},   {9987,9729,true,true,true,true},
  };
  const auto bin=quadBin();
  const std::vector<resources::GltfPackageFile> files{{"cena.bin",bin},{"grade.png",kPixel}};
  for(const auto &expected:cases) {
    auto text=gltfText("cena.bin","grade.png");
    const std::string texture=R"("textures":[{"source":0}])";
    const auto at=text.find(texture);
    AE_EXPECT_TRUE(at!=std::string::npos,"texture do fixture");
    text.replace(at,texture.size(),R"("textures":[{"source":0,"sampler":0}],"samplers":[{"minFilter":)"+
        std::to_string(expected.minFilter)+R"(,"magFilter":)"+std::to_string(expected.magFilter)+"}]");
    resources::GltfPackage package;std::string diagnostic;
    AE_EXPECT_TRUE(resources::packGltf(bytesOf(text),files,64u<<20,package,diagnostic),diagnostic.c_str());
    resources::GltfImport model;
    AE_EXPECT_TRUE(resources::importGlb(package.glb,{},{},model),model.diagnostic.c_str());
    AE_EXPECT_EQ(model.textures.size(),usize{1},"uma textura");
    const auto actual=renderer::decodeTextureSampler(model.textures.front()->samplerFlags);
    AE_EXPECT_EQ(actual.minLinear,expected.minLinear,"minificação");
    AE_EXPECT_EQ(actual.magLinear,expected.magLinear,"magnificação");
    AE_EXPECT_EQ(actual.mipEnabled,expected.mipEnabled,"uso de mip");
    AE_EXPECT_EQ(actual.mipLinear,expected.mipLinear,"filtro entre mips");
  }

  // Flags sem marcador são assets anteriores: continuam com filtro único e
  // cadeia de mips aberta, exatamente como o backend os interpretava.
  const auto oldNearest=renderer::decodeTextureSampler(0);
  AE_EXPECT_TRUE(!oldNearest.minLinear&&!oldNearest.magLinear&&oldNearest.mipEnabled&&!oldNearest.mipLinear,
                 "flags legadas nearest preservadas");
  const auto oldLinear=renderer::decodeTextureSampler(3);
  AE_EXPECT_TRUE(oldLinear.minLinear&&oldLinear.magLinear&&oldLinear.mipEnabled&&oldLinear.mipLinear,
                 "flags legadas lineares preservadas");
}

AE_TEST(m09e4_gltf_dependencies_never_reach_network_parent_folders_or_ambiguous_files) {
  const auto bin = quadBin();
  resources::GltfPackage package;
  std::string diagnostic;

  const std::string missing = gltfText("cena.bin", "grade.png");
  const std::vector<resources::GltfPackageFile> onlyImage{{"grade.png", kPixel}};
  AE_EXPECT_TRUE(!resources::packGltf(bytesOf(missing), onlyImage, 64u << 20, package, diagnostic) &&
                     diagnostic.find("cena.bin") != std::string::npos && package.glb.empty(),
                 "dependência que falta é listada pelo nome");

  const std::vector<resources::GltfPackageFile> both{{"cena.bin", bin}, {"grade.png", kPixel}};
  const std::string network = gltfText("https://exemplo.com/cena.bin", "grade.png");
  AE_EXPECT_TRUE(!resources::packGltf(bytesOf(network), both, 64u << 20, package, diagnostic) &&
                     diagnostic.find("rede") != std::string::npos,
                 "URI de rede recusada, sem acesso implícito");
  const std::string parent = gltfText("cena.bin", "../fora/grade.png");
  AE_EXPECT_TRUE(!resources::packGltf(bytesOf(parent), both, 64u << 20, package, diagnostic) &&
                     diagnostic.find("sai da pasta") != std::string::npos,
                 "segmento .. recusado");
  const std::string absolute = gltfText("/sdcard/cena.bin", "grade.png");
  AE_EXPECT_TRUE(!resources::packGltf(bytesOf(absolute), both, 64u << 20, package, diagnostic), "caminho absoluto recusado");
  const std::vector<resources::GltfPackageFile> twice{{"cena.bin", bin}, {"cena.bin", bin}, {"grade.png", kPixel}};
  const std::string normal = gltfText("cena.bin", "grade.png");
  AE_EXPECT_TRUE(!resources::packGltf(bytesOf(normal), twice, 64u << 20, package, diagnostic) &&
                     diagnostic.find("ambígua") != std::string::npos,
                 "dois arquivos com o mesmo nome tornam a dependência ambígua");
  AE_EXPECT_TRUE(!resources::packGltf(bytesOf(normal), both, 64, package, diagnostic) &&
                     diagnostic.find("limite") != std::string::npos,
                 "limite de bytes vale para o conjunto");

  // Sem arquivos ao lado: tudo em data URI.
  const std::string embedded = gltfText("data:application/octet-stream;base64," + toBase64(bin),
                                        "data:image/png;base64," + toBase64(kPixel));
  AE_EXPECT_TRUE(resources::packGltf(bytesOf(embedded), {}, 64u << 20, package, diagnostic), diagnostic.c_str());
  AE_EXPECT_TRUE(package.dependencies.size() == 2 && package.dependencies[0].dataUri && package.dependencies[1].dataUri,
                 "data URIs decodificadas e registradas");
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(package.glb, {}, {}, model) && model.textures.size() == 1, model.diagnostic.c_str());
}

// S0 — fonte em pasta: o principal e os arquivos dele no disco, casados pelo
// caminho relativo. Buffers entram no GLB de trabalho; a imagem é lida do disco
// pelo importador, e nada fora da pasta é alcançável.
namespace {
std::filesystem::path writeFolderSource(const std::string &name, const std::string &text, bool withImage) {
  const auto root = std::filesystem::temp_directory_path() / ("astra-gltf-folder-" + name);
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root / "texturas", error);
  const auto write = [](const std::filesystem::path &path, std::span<const u8> bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  };
  write(root / "cena.gltf", bytesOf(text));
  write(root / "cena.bin", quadBin());
  if (withImage) write(root / "texturas" / "grade a.png", kPixel);
  write(root / "grade a.png", kPixel); // mesmo nome, outra pasta: não pode ser confundido
  return root;
}
std::vector<u8> readAll(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
} // namespace

AE_TEST(s0_gltf_folder_source_reads_buffers_into_glb_and_images_from_disk) {
  const std::string text = gltfText("cena.bin", "texturas/grade%20a.png");
  const auto root = writeFolderSource("ok", text, true);
  std::vector<resources::GltfFolderDependency> dependencies;
  std::string diagnostic;
  AE_EXPECT_TRUE(resources::listGltfFolderDependencies(bytesOf(text), dependencies, diagnostic), diagnostic.c_str());
  AE_EXPECT_TRUE(dependencies.size() == 2 && dependencies[0].relative == "cena.bin" && !dependencies[0].image &&
                     dependencies[1].relative == "texturas/grade a.png" && dependencies[1].image,
                 "buffer e imagem pelo caminho relativo decodificado");
  const auto main = readAll(root / "cena.gltf");
  resources::GltfImport model;
  std::vector<u8> packed;
  AE_EXPECT_TRUE(resources::gltfIsFolderSource(main), ".gltf com URI é fonte em pasta");
  AE_EXPECT_TRUE(resources::importGltfFolder(main, root, 64u << 20, {}, {}, model, packed), model.diagnostic.c_str());
  AE_EXPECT_TRUE(model.draws.size() == 1 && model.textures.size() == 1 && model.skippedTextures == 0,
                 "geometria do .bin e textura lida da subpasta");
  AE_EXPECT_TRUE(packed.size() < 92 + 4096, "o GLB de trabalho leva o buffer, não a imagem");
  std::error_code error;
  std::filesystem::remove_all(root, error);
}

AE_TEST(s0_gltf_folder_source_refuses_escape_and_reports_missing_files_by_path) {
  std::string relative, refusal;
  AE_EXPECT_TRUE(!resources::gltfRelativeUri("../fora.png", relative, refusal) && !refusal.empty(), "`..` recusado");
  AE_EXPECT_TRUE(!resources::gltfRelativeUri("file:///sdcard/x.png", relative, refusal), "esquema recusado");
  AE_EXPECT_TRUE(!resources::gltfRelativeUri("/abs.png", relative, refusal), "absoluto recusado");
  AE_EXPECT_TRUE(resources::gltfRelativeUri("./a/./b%20c.png", relative, refusal) && relative == "a/b c.png",
                 "normalizado e decodificado");
  // A imagem existe só na raiz com o mesmo nome: pelo caminho, está faltando.
  const std::string text = gltfText("cena.bin", "texturas/grade%20a.png");
  const auto root = writeFolderSource("missing", text, false);
  const auto main = readAll(root / "cena.gltf");
  resources::GltfImport model;
  std::vector<u8> packed;
  AE_EXPECT_TRUE(resources::importGltfFolder(main, root, 64u << 20, {}, {}, model, packed), model.diagnostic.c_str());
  AE_EXPECT_TRUE(model.textures.empty() && model.skippedTextures == 1, "textura ausente fica de fora, sem trocar de arquivo");
  bool noted = false;
  for (const auto &note : model.textureNotes) noted |= note.find("texturas/grade%20a.png") != std::string::npos;
  AE_EXPECT_TRUE(noted, "o motivo cita a URI que faltou");
  const std::string escaping = gltfText("../cena.bin", "texturas/grade%20a.png");
  AE_EXPECT_TRUE(!resources::importGltfFolder(bytesOf(escaping), root, 64u << 20, {}, {}, model, packed) &&
                     model.diagnostic.find("sai da pasta") != std::string::npos,
                 "buffer fora da pasta recusado antes de ler");
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(!resources::readGltfFolderFile(root, "../astra-gltf-folder-ok/cena.gltf", bytes), "leitor fecha a pasta");
  std::error_code error;
  std::filesystem::remove_all(root, error);
}

// Sonda de uma fonte real em pasta (o Sponza), com os limites padrão da importação.
int probeImportGltfFolder(const char *mainPath, unsigned compression, bool lods) {
  const std::filesystem::path path(mainPath);
  const auto main = readAll(path);
  using Clock = std::chrono::steady_clock;
  const auto start = Clock::now();
  resources::GltfImport model;
  std::vector<u8> packed;
  resources::GltfImportLimits limits;
  limits.textureCompression = static_cast<u8>(compression);
  // S3: o preset de fonte nova (LOD + ordem de polígonos).
  limits.generateLods = limits.optimizePolygonOrder = lods;
  // Luzes e câmeras do arquivo entram no relatório da sonda (S4).
  limits.importLights = limits.importCameras = true;
  const bool imported = resources::importGltfFolder(main, path.parent_path(), 512ull << 20, limits, {}, model, packed);
  const auto ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
  u64 triangles = 0;
  for (const auto &draw : model.draws) triangles += draw.indexCount / 3;
  std::printf("%s: import=%d ms=%.0f packed_mb=%.1f draws=%zu triangles=%llu nodes=%zu textures=%zu skipped_textures=%u "
              "texture_mb=%.1f resident_dim=%u reduced=%u compressed=%u lights=%zu cameras=%zu\n",
              mainPath, imported ? 1 : 0, ms, static_cast<double>(packed.size()) / 1048576.0, model.draws.size(),
              static_cast<unsigned long long>(triangles), model.nodes.size(), model.textures.size(), model.skippedTextures,
              static_cast<double>(model.textureBytes) / 1048576.0, model.residentTextureDimension, model.reducedTextures,
              model.compressedTextures, model.lights.size(), model.cameras.size());
  if (!imported) std::printf("  import: %s\n", model.diagnostic.c_str());
  for (const auto &note : model.textureNotes) std::printf("  textura: %s\n", note.c_str());
  for (const auto &note : model.notes) std::printf("  nota: %s\n", note.c_str());
  if (lods)
    std::printf("  lod: draws=%u levels=%u small=%u deformed=%u source_tris=%llu lod_tris=%llu budget=%d acmr=%.3f->%.3f\n",
                model.lodDraws, model.lodLevels, model.lodSkippedSmall, model.lodSkippedDeformed,
                static_cast<unsigned long long>(model.lodSourceTriangles), static_cast<unsigned long long>(model.lodTriangles),
                model.lodBudgetReached ? 1 : 0, model.acmrBefore, model.acmrAfter);
  for (const auto &light : model.lights)
    std::printf("  luz: tipo=%u intensidade=%.3f cor=%.2f,%.2f,%.2f alcance=%.1f%s nó=%u\n", light.kind, light.intensity,
                light.color[0], light.color[1], light.color[2], light.range, light.rangeDeclared ? "" : " (sem alcance)", light.node);
  for (const auto &camera : model.cameras)
    std::printf("  câmera: nó=%u fov=%.1f near=%.2f far=%.1f\n", camera.node, camera.verticalFovDegrees, camera.nearPlane,
                camera.farPlane);
  // Materiais translúcidos: o que o renderer recebe (flags, textura de cor, fator).
  for (usize m = 0; m < model.materials.size(); ++m) {
    const auto &material = model.materials[m];
    if (!(material.flags & renderer::MapMaterialBlend)) continue;
    const u32 texture = material.textureIndices[0];
    const bool present = texture != renderer::InvalidMapTexture && texture < model.textures.size() && model.textures[texture];
    std::printf("  blend: %s flags=0x%x base_texture=%u presente=%d fator=%.2f,%.2f,%.2f,%.2f uv=%u\n",
                m < model.materialNames.size() ? model.materialNames[m].c_str() : "?", material.flags, texture, present ? 1 : 0,
                material.baseColorFactor[0], material.baseColorFactor[1], material.baseColorFactor[2],
                material.baseColorFactor[3], material.textureCoordinates);
    if (present) {
      const auto &t = *model.textures[texture];
      std::printf("         textura %ux%u níveis=%u formato=%u srgb=%d\n", t.width, t.height, t.levels, t.format, t.srgb ? 1 : 0);
    }
  }
  return imported ? 0 : 1;
}
