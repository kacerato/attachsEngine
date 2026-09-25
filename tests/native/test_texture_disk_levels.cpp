// Bloco C (S2): níveis de cima de uma textura ficam só no derivado em disco.
// A leitura por nível devolve exatamente os bytes da cadeia inteira, o cache lê
// no modo parcial e regrava a cadeia completa, e o orçamento corta níveis de uma
// textura parcial sem perder o conteúdo.
#include "harness.h"
#include "renderer/authoring_texture.h"
#include "renderer/texture_level_reader.h"
#include "resources/gltf_import.h"
#include "resources/import_cache.h"
#include "resources/texture_budget.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <vector>

using namespace ae;
using namespace ae::renderer;

namespace {
AuthoringTexture fullTexture(u32 size) {
  AuthoringTexture t;
  t.width = t.height = size;
  u32 levels = 1;
  for (u32 s = size; s > 1; s /= 2) ++levels;
  t.levels = levels;
  t.mipChain.resize(t.expectedBytes());
  for (usize i = 0; i < t.mipChain.size(); ++i) t.mipChain[i] = static_cast<u8>((i * 131u) ^ (i >> 7));
  return t;
}
std::filesystem::path scratchDirectory() {
  auto dir = std::filesystem::temp_directory_path() / "astra-disk-levels";
  std::filesystem::create_directories(dir);
  return dir;
}
} // namespace

AE_TEST(c_texture_levels_read_from_memory_and_from_the_derived_file) {
  const auto full = fullTexture(64);  // 7 níveis
  // Arquivo com um prefixo qualquer e a cadeia inteira depois dele.
  const auto path = scratchDirectory() / "cadeia.bin";
  {
    std::ofstream out(path, std::ios::binary);
    const std::vector<char> prefix(100, 'x');
    out.write(prefix.data(), prefix.size());
    out.write(reinterpret_cast<const char *>(full.mipChain.data()), static_cast<std::streamsize>(full.mipChain.size()));
  }
  AuthoringTexture partial = full;
  partial.firstLevel = 3;
  partial.file = std::make_shared<AuthoringTextureFile>(AuthoringTextureFile{path.string(), 100});
  partial.mipChain.assign(full.mipChain.end() - static_cast<std::ptrdiff_t>(full.chainBytesFrom(3)), full.mipChain.end());
  AE_EXPECT_TRUE(partial.valid() && partial.partial(), "textura parcial válida só com a cauda");
  std::vector<u8> scratch;
  std::span<const u8> bytes;
  AE_EXPECT_TRUE(readAuthoringTextureLevels(partial, 0, scratch, bytes) &&
                 std::equal(bytes.begin(), bytes.end(), full.mipChain.begin(), full.mipChain.end()),
                 "do nível 0: topo do arquivo + cauda da memória = cadeia inteira");
  const auto fromTwo = full.expectedBytes() - full.chainBytesFrom(2);
  AE_EXPECT_TRUE(readAuthoringTextureLevels(partial, 2, scratch, bytes) && bytes.size() == full.chainBytesFrom(2) &&
                 std::equal(bytes.begin(), bytes.end(), full.mipChain.begin() + static_cast<std::ptrdiff_t>(fromTwo)),
                 "do nível 2: só o que falta do arquivo");
  AE_EXPECT_TRUE(readAuthoringTextureLevels(partial, 5, scratch, bytes) && bytes.data() >= partial.mipChain.data() &&
                 bytes.size() == full.chainBytesFrom(5), "da cauda: direto da memória, sem arquivo");
  auto missing = partial;
  missing.file = std::make_shared<AuthoringTextureFile>(AuthoringTextureFile{(scratchDirectory() / "nao-existe.bin").string(), 0});
  std::span<const u8> untouched;
  AE_EXPECT_TRUE(!readAuthoringTextureLevels(missing, 0, scratch, untouched) && untouched.empty(),
                 "derivado ausente: falha fechada");
  AE_EXPECT_TRUE(readAuthoringTextureLevels(missing, 4, scratch, bytes), "a cauda continua servindo sem o arquivo");
}

AE_TEST(c_import_cache_reads_textures_partially_and_writes_them_back_whole) {
  // Qualquer modelo coerente serve; a textura não precisa de material.
  resources::GltfImport model;
  model.nodes.emplace_back();
  model.draws.emplace_back();
  model.draws[0].indexCount = 3;
  model.indices = {0, 1, 2};
  model.vertices.resize(3 * MapVertexStride);
  model.drawNodes = {0};
  model.drawSkins = {-1};
  model.drawMorphs = {-1};
  model.names = {"a"};
  model.keys = {"a"};
  const auto full = std::make_shared<AuthoringTexture>(fullTexture(512));  // 10 níveis
  model.textures = {full};
  model.textureBytes = full->expectedBytes();
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(resources::writeImportCache(model, "k", bytes), "derivado gravado");
  const auto path = scratchDirectory() / "derivado.aeic";
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  resources::ImportCachePartialTextures partial;
  partial.path = path.string();
  partial.keepDimension = 64;
  resources::GltfImport read;
  AE_EXPECT_TRUE(resources::readImportCache(bytes, "k", read, &partial), "leitura parcial");
  const auto &texture = *read.textures.at(0);
  AE_EXPECT_EQ(texture.firstLevel, 3u, "512, 256 e 128 ficam no arquivo; 64 para baixo na memória");
  AE_EXPECT_EQ(texture.mipChain.size(), full->chainBytesFrom(3), "RAM só com a cauda");
  std::vector<u8> scratch;
  std::span<const u8> levels;
  AE_EXPECT_TRUE(readAuthoringTextureLevels(texture, 0, scratch, levels) &&
                 std::equal(levels.begin(), levels.end(), full->mipChain.begin(), full->mipChain.end()),
                 "a cadeia reconstruída é a gravada");
  std::vector<u8> again;
  AE_EXPECT_TRUE(resources::writeImportCache(read, "k", again) && again == bytes, "regravar do parcial dá o mesmo derivado");
  resources::GltfImport whole;
  AE_EXPECT_TRUE(resources::readImportCache(bytes, "k", whole) && !whole.textures.at(0)->partial(),
                 "sem o modo parcial, a leitura de antes");
  // Logo depois de gravar (importação nova): as texturas do próprio modelo
  // viram parciais pelos deslocamentos que a escrita devolveu, sem reler.
  std::vector<u64> offsets;
  AE_EXPECT_TRUE(resources::writeImportCache(model, "k", bytes, &offsets) && offsets.size() == 1, "deslocamentos devolvidos");
  auto fresh = model;
  AE_EXPECT_TRUE(resources::makeTexturesPartial(fresh, offsets, path.string(), 64), "texturas convertidas");
  const auto &converted = *fresh.textures.at(0);
  AE_EXPECT_TRUE(converted.firstLevel == 3 && converted.mipChain.size() == full->chainBytesFrom(3), "só a cauda fica");
  AE_EXPECT_TRUE(readAuthoringTextureLevels(converted, 0, scratch, levels) &&
                 std::equal(levels.begin(), levels.end(), full->mipChain.begin(), full->mipChain.end()),
                 "o topo vem do derivado gravado");
  AE_EXPECT_TRUE(!resources::makeTexturesPartial(fresh, {}, path.string()), "listas que não casam recusam");
}

AE_TEST(c_texture_level_reader_reads_off_the_render_thread_and_forgets_on_clear) {
  const auto full = fullTexture(256);
  const auto path = scratchDirectory() / "leitor.bin";
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(full.mipChain.data()), static_cast<std::streamsize>(full.mipChain.size()));
  }
  auto partial = std::make_shared<AuthoringTexture>(full);
  partial->firstLevel = 4;
  partial->file = std::make_shared<AuthoringTextureFile>(AuthoringTextureFile{path.string(), 0});
  partial->mipChain.assign(full.mipChain.end() - static_cast<std::ptrdiff_t>(full.chainBytesFrom(4)), full.mipChain.end());
  TextureLevelReader reader;
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(reader.take(7, 1, bytes) == TextureLevelReader::Status::Unknown, "nada pedido ainda");
  reader.request(7, partial, 1);
  reader.request(7, partial, 1);  // repetido não duplica
  auto status = TextureLevelReader::Status::Pending;
  for (u32 tries = 0; tries < 2000 && status == TextureLevelReader::Status::Pending; ++tries) {
    status = reader.take(7, 1, bytes);
    if (status == TextureLevelReader::Status::Pending) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const auto skip = full.expectedBytes() - full.chainBytesFrom(1);
  AE_EXPECT_TRUE(status == TextureLevelReader::Status::Ready && bytes.size() == full.chainBytesFrom(1) &&
                 std::equal(bytes.begin(), bytes.end(), full.mipChain.begin() + static_cast<std::ptrdiff_t>(skip)),
                 "bytes do nível 1 em diante, lidos em segundo plano");
  AE_EXPECT_TRUE(reader.take(7, 1, bytes) == TextureLevelReader::Status::Unknown, "entregue uma vez");
  auto missing = std::make_shared<AuthoringTexture>(*partial);
  missing->file = std::make_shared<AuthoringTextureFile>(AuthoringTextureFile{(scratchDirectory() / "sumiu.bin").string(), 0});
  reader.request(8, missing, 0);
  status = TextureLevelReader::Status::Pending;
  for (u32 tries = 0; tries < 2000 && status == TextureLevelReader::Status::Pending; ++tries) {
    status = reader.take(8, 0, bytes);
    if (status == TextureLevelReader::Status::Pending) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  AE_EXPECT_TRUE(status == TextureLevelReader::Status::Failed, "derivado ausente vira falha, não bytes inventados");
  reader.request(9, partial, 0);
  reader.clear();
  AE_EXPECT_TRUE(reader.take(9, 0, bytes) == TextureLevelReader::Status::Unknown && reader.outstanding() == 0,
                 "trocar a biblioteca descarta pedidos e resultados");
}

AE_TEST(c_texture_budget_drops_levels_of_a_partial_texture_without_losing_content) {
  const auto full = fullTexture(128);  // 8 níveis
  const auto path = scratchDirectory() / "orcamento.bin";
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(full.mipChain.data()), static_cast<std::streamsize>(full.mipChain.size()));
  }
  auto partial = std::make_shared<AuthoringTexture>(full);
  partial->firstLevel = 2;
  partial->file = std::make_shared<AuthoringTextureFile>(AuthoringTextureFile{path.string(), 0});
  partial->mipChain.assign(full.mipChain.end() - static_cast<std::ptrdiff_t>(full.chainBytesFrom(2)), full.mipChain.end());
  std::vector<SharedAuthoringTexture> textures{partial};
  const auto report = resources::applyTextureBudget(textures, full.chainBytesFrom(1), 1);
  AE_EXPECT_TRUE(report.withinBudget() && report.droppedLevels == 1, "um nível a menos cabe");
  const auto &reduced = *textures[0];
  AE_EXPECT_TRUE(reduced.width == 64 && reduced.firstLevel == 1 && reduced.partial(), "segue parcial, um nível abaixo");
  std::vector<u8> scratch;
  std::span<const u8> levels;
  const auto skip = full.expectedBytes() - full.chainBytesFrom(1);
  AE_EXPECT_TRUE(readAuthoringTextureLevels(reduced, 0, scratch, levels) && levels.size() == full.chainBytesFrom(1) &&
                 std::equal(levels.begin(), levels.end(), full.mipChain.begin() + static_cast<std::ptrdiff_t>(skip)),
                 "o novo nível 0 é o antigo nível 1, lido do arquivo");
}
