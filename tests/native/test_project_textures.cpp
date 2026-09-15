// R4 — texturas do projeto e texturas nos materiais.
//
// Fontes geradas aqui mesmo: um PNG montado em bytes e um GLB de uma tela. O que
// se fixa é o contrato de alcance (instância, compartilhado, fonte), o arquivo
// extraído com os bytes originais e os formatos versionados.
#include "harness.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_session.h"
#include "renderer/authoring_geometry.h"
#include "renderer/material_override.h"
#include "resources/glb_images.h"
#include "resources/material_asset.h"
#include "scene/mesh_renderer.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

using namespace ae;
using namespace ae::editor;

namespace {
void putBig(std::vector<u8> &out, u32 value) {
  for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<u8>(value >> shift));
}
void putLittle(std::vector<u8> &out, u32 value) {
  for (u32 i = 0; i < 4; ++i) out.push_back(static_cast<u8>(value >> (i * 8)));
}
u32 crc(const u8 *data, usize size) {
  u32 c = 0xffffffffu;
  for (usize i = 0; i < size; ++i) {
    c ^= data[i];
    for (u32 k = 0; k < 8; ++k) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
  }
  return c ^ 0xffffffffu;
}
void pngChunk(std::vector<u8> &png, const char type[5], const std::vector<u8> &data) {
  putBig(png, static_cast<u32>(data.size()));
  std::vector<u8> body(type, type + 4);
  body.insert(body.end(), data.begin(), data.end());
  png.insert(png.end(), body.begin(), body.end());
  putBig(png, crc(body.data(), body.size()));
}
// PNG RGBA8 com zlib "stored".
std::vector<u8> png(u32 width, u32 height, u8 red) {
  std::vector<u8> out{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  std::vector<u8> header;
  putBig(header, width);
  putBig(header, height);
  header.insert(header.end(), {8, 6, 0, 0, 0});
  pngChunk(out, "IHDR", header);
  std::vector<u8> raw;
  for (u32 y = 0; y < height; ++y) {
    raw.push_back(0);
    for (u32 x = 0; x < width; ++x) raw.insert(raw.end(), {red, 64, 32, 255});
  }
  std::vector<u8> zlib{0x78, 0x01, 1, static_cast<u8>(raw.size()), static_cast<u8>(raw.size() >> 8),
                       static_cast<u8>(~raw.size()), static_cast<u8>(~raw.size() >> 8)};
  zlib.insert(zlib.end(), raw.begin(), raw.end());
  u32 a = 1, b = 0;
  for (u8 value : raw) { a = (a + value) % 65521; b = (b + a) % 65521; }
  putBig(zlib, (b << 16) | a);
  pngChunk(out, "IDAT", zlib);
  pngChunk(out, "IEND", {});
  return out;
}
// Uma tela com cor base texturizada por uma imagem PNG embutida chamada "Pintura".
std::vector<u8> texturedPanel(const std::vector<u8> &image) {
  std::vector<u8> binary;
  const float positions[12]{0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
  for (float value : positions) {
    u32 bits = 0;
    std::memcpy(&bits, &value, 4);
    putLittle(binary, bits);
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
      R"("images":[{"bufferView":2,"mimeType":"image/png","name":"Pintura.png"}],"textures":[{"source":0}],)" +
      R"("materials":[{"name":"Tela","pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],)" +
      R"("meshes":[{"name":"Tela","primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],)" +
      R"("nodes":[{"name":"Tela","mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
  while (json.size() % 4) json.push_back(' ');
  std::vector<u8> glb;
  putLittle(glb, 0x46546C67);
  putLittle(glb, 2);
  putLittle(glb, static_cast<u32>(12 + 8 + json.size() + 8 + binary.size()));
  putLittle(glb, static_cast<u32>(json.size()));
  putLittle(glb, 0x4E4F534A);
  glb.insert(glb.end(), json.begin(), json.end());
  putLittle(glb, static_cast<u32>(binary.size()));
  putLittle(glb, 0x004E4942);
  glb.insert(glb.end(), binary.begin(), binary.end());
  return glb;
}

struct Publisher {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  usize textures = 0;
  u32 rebuilds = 0;
  std::vector<u32> samplers; // flags de sampler de cada textura publicada, na ordem da biblioteca
  std::vector<renderer::SharedAuthoringTexture> published;
};

void start(EditorSession &session, Publisher &publisher) {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, vertices, indices, draws, materials);
  session.importMap(draws, materials, false, vertices, indices, 0);
  session.setGeometryPublisher([&publisher](std::span<const u8> v, std::span<const u32> i, std::span<const renderer::MapDrawRecord> d,
                                            std::span<const renderer::MapMaterialRecord> m,
                                            std::span<const renderer::SharedAuthoringTexture> t, EditorSession::PublishedGeometry &out) {
    for (const auto &texture : t) if (!texture || !texture->valid()) return false;
    ++publisher.rebuilds;
    publisher.textures = t.size();
    publisher.samplers.clear();
    for (const auto &texture : t) publisher.samplers.push_back(texture->samplerFlags);
    publisher.published.assign(t.begin(), t.end());
    publisher.vertices.clear(); publisher.indices.clear(); publisher.draws.clear(); publisher.materials.clear();
    if (!renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, publisher.vertices, publisher.indices, publisher.draws, publisher.materials))
      return false;
    const auto vertexBase = static_cast<u32>(publisher.vertices.size() / renderer::MapVertexStride);
    const auto indexBase = static_cast<u32>(publisher.indices.size());
    const auto materialBase = static_cast<u32>(publisher.materials.size());
    publisher.vertices.insert(publisher.vertices.end(), v.begin(), v.end());
    publisher.indices.insert(publisher.indices.end(), i.begin(), i.end());
    publisher.materials.insert(publisher.materials.end(), m.begin(), m.end());
    for (auto draw : d) {
      draw.firstIndex += indexBase; draw.vertexOffset += vertexBase; draw.materialIndex += materialBase;
      draw.lodGroupId = static_cast<u32>(publisher.draws.size());
      publisher.draws.push_back(draw);
    }
    out = {publisher.draws, publisher.materials, publisher.vertices, publisher.indices};
    return true;
  });
}

struct Project {
  std::filesystem::path root = std::filesystem::temp_directory_path() /
      ("astra-r4-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Project() { std::filesystem::create_directories(root); }
  ~Project() { std::error_code error; std::filesystem::remove_all(root, error); }
};

std::vector<std::string> tokens(const std::string &text) {
  std::istringstream in(text);
  std::vector<std::string> out;
  for (std::string token; in >> token;) out.push_back(token);
  return out;
}
} // namespace

AE_TEST(r4_material_v2_and_mesh_renderer_v4_round_trip_and_read_older_versions) {
  const auto texture = resources::assetGuidFromSeed("r4-textura");
  resources::MaterialAsset material;
  material.guid = resources::assetGuidFromSeed("r4-material");
  material.name = "Pintura";
  material.values.enabled = true;
  material.textures[0] = texture;
  material.textures[1] = scene::MaterialTextureNone;
  resources::MaterialAsset back;
  AE_EXPECT_TRUE(resources::MaterialAsset::deserialize(material.serialize(), back), "material v2 ida e volta");
  AE_EXPECT_TRUE(back.textures == material.textures, "texturas do material preservadas, inclusive 'sem textura'");

  // Um arquivo v1 (antes de R4) lê como v2 sem texturas trocadas.
  const std::string v1 = "ASTRA_MATERIAL 1 " + material.guid.text() + " 3 \"Velho\" 1 0.5 0.25 0.5 0 1 1 0 0 0 1\n";
  resources::MaterialAsset old;
  AE_EXPECT_TRUE(resources::MaterialAsset::deserialize(v1, old), "v1 aceito");
  AE_EXPECT_TRUE(old.textures == (std::array<resources::AssetGuid, 4>{}), "v1 herda todas as texturas");
  AE_EXPECT_EQ(old.values.baseColor[1], .5f, "valores v1 preservados");

  // Componente de malha v4: texturas por slot; v3 (sem os tokens) ainda lê.
  scene::MeshRenderer render;
  render.submeshes.resize(1);
  render.textures[2] = texture;
  render.submeshes[0].textures[3] = scene::MaterialTextureNone;
  std::ostringstream out;
  render.write(out);
  scene::MeshRenderer read;
  std::istringstream in(out.str());
  AE_EXPECT_TRUE(read.read(in, 5), "v5 lido");
  AE_EXPECT_TRUE(read.textures == render.textures && read.submeshes[0].textures == render.submeshes[0].textures, "v4 preserva texturas");
  auto words = tokens(out.str());
  words.resize(words.size() - 14); // v4: 4 tokens de textura por slot; v5: 3 de superfície; dois slots
  std::string v3;
  for (const auto &word : words) v3 += word + ' ';
  scene::MeshRenderer legacy;
  std::istringstream legacyIn(v3);
  AE_EXPECT_TRUE(legacy.read(legacyIn, 3), "v3 lido");
  AE_EXPECT_TRUE(legacy.textures == scene::SlotTextures{}, "v3 herda texturas");
  auto v4Words = tokens(out.str());
  v4Words.resize(v4Words.size() - 6); // só os tokens de superfície (v5) saem
  std::string v4;
  for (const auto &word : v4Words) v4 += word + ' ';
  scene::MeshRenderer older;
  std::istringstream olderIn(v4);
  AE_EXPECT_TRUE(older.read(olderIn, 4) && older.textures == render.textures && !older.surface.overrides(),
                 "v4 lido com texturas e sem superfície trocada");

  // Aplicação no renderer: deslocamento da biblioteca e flag do binding.
  renderer::MapMaterialRecord source{};
  std::fill(std::begin(source.textureIndices), std::end(source.textureIndices), renderer::InvalidMapTexture);
  scene::MaterialParameters value;
  value.textures[1] = 7;
  auto applied = renderer::applyMaterialOverride(source, value, 100);
  AE_EXPECT_EQ(applied.textureIndices[1], 107u, "índice deslocado pela base da biblioteca");
  AE_EXPECT_TRUE((applied.flags & renderer::MapMaterialNormalMap) != 0, "variante passa a amostrar normal");
  AE_EXPECT_EQ(applied.textureIndices[0], renderer::InvalidMapTexture, "binding não trocado fica como na fonte");
  source.flags = renderer::MapMaterialNormalMap;
  source.textureIndices[1] = 3;
  value.textures[1] = renderer::InvalidMapTexture;
  applied = renderer::applyMaterialOverride(source, value, 100);
  AE_EXPECT_TRUE(applied.textureIndices[1] == renderer::InvalidMapTexture && !(applied.flags & renderer::MapMaterialNormalMap),
                 "tirar a normal tira a flag");
  source.flags = renderer::MapMaterialMetallicRoughnessMap | renderer::MapMaterialOcclusionInMetallicRoughness;
  scene::MaterialParameters packed;
  packed.textures[2] = 5;
  applied = renderer::applyMaterialOverride(source, packed, 0);
  AE_EXPECT_TRUE(!(applied.flags & renderer::MapMaterialOcclusionInMetallicRoughness),
                 "trocar o mapa metal/rugosidade tira a oclusão empacotada da fonte");
}

AE_TEST(r4_textures_extract_from_source_and_resolve_per_instance_and_shared_scope) {
  Project project;
  EditorSession session;
  Publisher publisher;
  start(session, publisher);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  const auto image = png(4, 4, 200);
  const auto glb = texturedPanel(image);

  // Leitor de imagens embutidas: bytes originais.
  std::vector<resources::GlbEmbeddedImage> images;
  std::string diagnostic;
  AE_EXPECT_TRUE(resources::listGlbEmbeddedImages(glb, images, diagnostic), diagnostic.c_str());
  AE_EXPECT_TRUE(images.size() == 1 && images[0].bytes == image && images[0].container == resources::ImageContainer::Png,
                 "uma imagem PNG com os bytes do arquivo");

  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, model), model.diagnostic.c_str());
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.commitModelImport(glb, model, "Fontes/tela.glb", "", report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), report.diagnostic.c_str());
  const usize sourceTextures = model.textures.size();

  EditorSession::TextureExtraction extraction;
  AE_EXPECT_TRUE(session.extractSourceTextures("Fontes/tela.glb", extraction, diagnostic), diagnostic.c_str());
  AE_EXPECT_EQ(extraction.created, 1u, "uma textura extraída");
  std::vector<u8> written;
  AE_EXPECT_TRUE(EditorImportTransaction::read(project.root / "Texturas/tela/Pintura.png", written) && written == image,
                 "arquivo extraído com os bytes originais, sem recodificar");
  AE_EXPECT_EQ(session.projectTextures().size(), 1u, "registrada no projeto");
  AE_EXPECT_EQ(session.projectTextures()[0].width, 4u, "dimensões do cabeçalho");
  EditorSession::TextureExtraction again;
  AE_EXPECT_TRUE(session.extractSourceTextures("Fontes/tela.glb", again, diagnostic), diagnostic.c_str());
  AE_EXPECT_TRUE(again.created == 0 && again.reused == 1, "extrair de novo reaproveita, não duplica");
  const auto texture = extraction.textures.front();

  EditorEntityId object = kInvalidEntity;
  {
    std::vector<EditorEntityId> ids;
    session.document().collectSubtree(session.document().root(), ids);
    for (const auto id : ids)
      if (const auto *render = meshRenderer(*session.document().find(id)); render && render->slotMesh(0)) object = id;
  }
  AE_EXPECT_TRUE(object != kInvalidEntity, "instância com malha");
  const auto effective = [&](u32 binding) {
    std::vector<renderer::MapDrawState> draws;
    if (!session.extractMap(draws)) return u32{0xdeadbeef};
    for (const auto &draw : draws) if (draw.objectId == object && draw.visible) return draw.material.textures[binding];
    return u32{0xdeadbeef};
  };

  // Instância: a normal passa a ser a textura do projeto; a cor base segue a da fonte.
  const auto before = publisher.rebuilds;
  AE_EXPECT_TRUE(session.setSlotTexture(object, 0, 1, EditorSession::MaterialScope::Instance, texture, diagnostic), diagnostic.c_str());
  AE_EXPECT_EQ(publisher.rebuilds, before + 1, "textura nova na GPU custa uma publicação");
  AE_EXPECT_EQ(publisher.textures, sourceTextures + 1, "textura do projeto depois das da fonte");
  AE_EXPECT_EQ(effective(1), static_cast<u32>(sourceTextures), "normal resolvida para a textura do projeto");
  AE_EXPECT_EQ(effective(0), scene::MaterialTextureKeep, "cor base continua a da fonte");
  AE_EXPECT_TRUE(!session.setSlotTexture(object, 0, 0, EditorSession::MaterialScope::Shared, texture, diagnostic),
                 "compartilhado exige material do projeto");

  // Criar material do projeto leva a textura da instância para o recurso.
  const auto shared = session.createMaterialFromSlot(object, 0, diagnostic);
  AE_EXPECT_TRUE(shared.valid(), diagnostic.c_str());
  AE_EXPECT_TRUE(session.findMaterialAsset(shared)->textures[1] == texture, "material do projeto guarda a textura");
  AE_EXPECT_TRUE(!meshRenderer(*session.document().find(object))->textures[1].valid(), "a instância deixa de trocar sozinha");
  AE_EXPECT_EQ(effective(1), static_cast<u32>(sourceTextures), "mesma aparência depois de compartilhar");

  // Compartilhado: tirar a cor base vale para todos os usos, sem publicar de novo.
  const auto rebuilds = publisher.rebuilds;
  AE_EXPECT_TRUE(session.setSlotTexture(object, 0, 0, EditorSession::MaterialScope::Shared, scene::MaterialTextureNone, diagnostic),
                 diagnostic.c_str());
  AE_EXPECT_EQ(publisher.rebuilds, rebuilds, "sem textura nova, sem publicação");
  AE_EXPECT_EQ(effective(0), renderer::InvalidMapTexture, "cor base sem textura pelo material compartilhado");
  std::vector<u8> materialFile;
  const auto *record = session.assets().find(shared);
  AE_EXPECT_TRUE(record && EditorImportTransaction::read(project.root / record->path, materialFile), "arquivo do material");
  const std::string text(materialFile.begin(), materialFile.end());
  AE_EXPECT_TRUE(text.starts_with("ASTRA_MATERIAL 6") && text.find(texture.text()) != std::string::npos && text.find("none") != std::string::npos,
                 "material gravado na versão atual com as texturas");

  // Instância vence o compartilhado, binding a binding.
  AE_EXPECT_TRUE(session.setSlotTexture(object, 0, 0, EditorSession::MaterialScope::Instance, texture, diagnostic), diagnostic.c_str());
  AE_EXPECT_TRUE(effective(0) != renderer::InvalidMapTexture && effective(0) != scene::MaterialTextureKeep,
                 "a instância volta a ter cor base sobre o compartilhado");

  // Visto no aparelho: o vínculo com a fonte dizia "igual à fonte" com textura trocada.
  AE_EXPECT_TRUE((session.importLinkOverrides(object) & ImportOverrideMaterial) != 0,
                 "material local conta como alteração do vínculo");
  AE_EXPECT_TRUE(session.revertImportLink(object, ImportOverrideMaterial), "reverter material à fonte");
  const auto *reverted = meshRenderer(*session.document().find(object));
  AE_EXPECT_TRUE(!reverted->textures[0].valid() && !reverted->materialAsset.valid() && !reverted->material.enabled,
                 "textura, material do projeto e valores locais voltam à fonte");
  AE_EXPECT_EQ(effective(0), scene::MaterialTextureKeep, "cor base de novo a da fonte");
  AE_EXPECT_EQ(session.importLinkOverrides(object) & ImportOverrideMaterial, 0u, "vínculo volta a ser igual à fonte no material");
}

AE_TEST(r4_texture_thumbnails_are_generated_once_and_viewer_walks_mips_and_channels) {
  Project project;
  EditorSession session;
  Publisher publisher;
  start(session, publisher);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  AE_EXPECT_TRUE(!session.generatePendingTextureThumbnail() && session.takePreviewAtlas() == nullptr, "sem texturas, nada a enviar");
  const auto glb = texturedPanel(png(4, 4, 200));
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, model), model.diagnostic.c_str());
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.commitModelImport(glb, model, "Fontes/tela.glb", "", report), report.diagnostic.c_str());
  EditorSession::TextureExtraction extraction;
  std::string diagnostic;
  AE_EXPECT_TRUE(session.extractSourceTextures("Fontes/tela.glb", extraction, diagnostic), diagnostic.c_str());

  AE_EXPECT_TRUE(session.generatePendingTextureThumbnail(), "miniatura da textura nova");
  AE_EXPECT_TRUE(!session.generatePendingTextureThumbnail(), "a mesma textura não é decodificada de novo");
  const auto *atlas = session.takePreviewAtlas();
  AE_EXPECT_TRUE(atlas && atlas->size() == usize{TexturePreviewAtlasSize} * TexturePreviewAtlasSize * 4, "atlas entregue ao renderer");
  AE_EXPECT_TRUE(session.takePreviewAtlas() == nullptr, "entregue uma vez só");

  AE_EXPECT_TRUE(session.openTextureViewer(0), session.screen().textureViewerInfo.c_str());
  const auto &screen = session.screen();
  AE_EXPECT_TRUE(screen.textureViewer && screen.textureViewerLevels == 3 && !screen.textureViewerImage.isEmpty(), "4x4 tem três níveis");
  AE_EXPECT_TRUE(screen.textureViewerInfo.find("4×4") != std::string::npos && screen.textureViewerInfo.find("Texturas/tela/Pintura.png") != std::string::npos,
                 "dimensões e arquivo");
  AE_EXPECT_TRUE(!session.stepTextureViewerLevel(-1), "não há nível abaixo do zero");
  AE_EXPECT_TRUE(session.stepTextureViewerLevel(1) && screen.textureViewerLevelLabel.find("2×2") != std::string::npos,
                 screen.textureViewerLevelLabel.c_str());
  AE_EXPECT_TRUE(session.cycleTextureViewerChannel() && screen.textureViewerChannelLabel == "Vermelho", "canal seguinte");
  AE_EXPECT_TRUE(session.takePreviewAtlas() != nullptr, "trocar nível e canal reenvia o atlas");
  AE_EXPECT_TRUE(session.cycleTextureViewerZoom() && screen.textureViewerZoomLabel.starts_with("2×"), screen.textureViewerZoomLabel.c_str());
  AE_EXPECT_TRUE(session.cycleTextureViewerBackground() && screen.textureViewerBackgroundLabel == "Preto", "fundo seguinte");
  AE_EXPECT_TRUE(!session.openTextureViewer(7), "índice fora das texturas recusado");
  session.closeTextureViewer();
  AE_EXPECT_TRUE(!screen.textureViewer, "fechado");
}

namespace {
EditorEntityId firstMeshObject(EditorSession &session) {
  std::vector<EditorEntityId> ids;
  session.document().collectSubtree(session.document().root(), ids);
  for (const auto id : ids)
    if (const auto *render = meshRenderer(*session.document().find(id)); render && render->slotMesh(0)) return id;
  return kInvalidEntity;
}
u32 effectiveTexture(EditorSession &session, EditorEntityId object, u32 binding) {
  std::vector<renderer::MapDrawState> draws;
  if (!session.extractMap(draws)) return 0xdeadbeef;
  for (const auto &draw : draws) if (draw.objectId == object && draw.visible) return draw.material.textures[binding];
  return 0xdeadbeef;
}
bool resolvesToProjectTexture(u32 value) {
  return value != scene::MaterialTextureKeep && value != renderer::InvalidMapTexture && value != 0xdeadbeef;
}
} // namespace

AE_TEST(r4_reimport_keeps_texture_overrides_in_instance_and_shared_material) {
  Project project;
  EditorSession session;
  Publisher publisher;
  start(session, publisher);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  const auto glb = texturedPanel(png(4, 4, 200));
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, model), model.diagnostic.c_str());
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.commitModelImport(glb, model, "Fontes/tela.glb", "", report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), report.diagnostic.c_str());
  EditorSession::TextureExtraction extraction;
  std::string diagnostic;
  AE_EXPECT_TRUE(session.extractSourceTextures("Fontes/tela.glb", extraction, diagnostic), diagnostic.c_str());
  const auto texture = extraction.textures.front();
  auto object = firstMeshObject(session);

  // Normal no material compartilhado; emissão só nesta instância.
  AE_EXPECT_TRUE(session.setSlotTexture(object, 0, 1, EditorSession::MaterialScope::Instance, texture, diagnostic), diagnostic.c_str());
  const auto shared = session.createMaterialFromSlot(object, 0, diagnostic);
  AE_EXPECT_TRUE(shared.valid(), diagnostic.c_str());
  AE_EXPECT_TRUE(session.setSlotTexture(object, 0, 3, EditorSession::MaterialScope::Instance, texture, diagnostic), diagnostic.c_str());

  // Reimportação com a imagem da fonte alterada: mesma estrutura, outro conteúdo.
  const auto revised = texturedPanel(png(4, 4, 90));
  resources::GltfImport revisedModel;
  AE_EXPECT_TRUE(resources::importGlb(revised, {}, {}, revisedModel), revisedModel.diagnostic.c_str());
  std::vector<u8> current;
  AE_EXPECT_TRUE(EditorImportTransaction::read(project.root / "Fontes/tela.glb", current), "fonte atual");
  EditorSession::ModelImportReport again;
  AE_EXPECT_TRUE(session.commitModelImport(revised, revisedModel, "Fontes/tela.glb", Sha256::hex(current), again), again.diagnostic.c_str());
  AE_EXPECT_TRUE(again.reimported, "foi reimportação");

  object = firstMeshObject(session);
  const auto *render = meshRenderer(*session.document().find(object));
  AE_EXPECT_TRUE(render && render->textures[3] == texture, "textura da instância atravessa a reimportação");
  AE_EXPECT_TRUE(render && render->materialAsset == shared, "material compartilhado continua ligado");
  AE_EXPECT_TRUE(session.findMaterialAsset(shared)->textures[1] == texture, "textura do material compartilhado preservada");
  AE_EXPECT_TRUE(resolvesToProjectTexture(effectiveTexture(session, object, 1)), "normal resolvida depois da nova publicação");
  AE_EXPECT_TRUE(resolvesToProjectTexture(effectiveTexture(session, object, 3)), "emissão resolvida depois da nova publicação");
  AE_EXPECT_EQ(effectiveTexture(session, object, 0), scene::MaterialTextureKeep, "cor base segue a fonte revisada");
}

AE_TEST(r4_reopen_publishes_scene_textures_once_and_warns_before_deleting_a_used_texture) {
  Project project;
  const auto glb = texturedPanel(png(4, 4, 200));
  const auto scenePath = project.root / "scenes" / "editor.aescene";
  std::filesystem::create_directories(scenePath.parent_path());
  std::string registry;
  resources::AssetGuid texture;
  std::string texturePath;
  usize sourceTextures = 0;
  {
    EditorSession session;
    Publisher publisher;
    start(session, publisher);
    AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
    resources::GltfImport model;
    AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, model), model.diagnostic.c_str());
    sourceTextures = model.textures.size();
    EditorSession::ModelImportReport report;
    AE_EXPECT_TRUE(session.commitModelImport(glb, model, "Fontes/tela.glb", "", report), report.diagnostic.c_str());
    AE_EXPECT_TRUE(session.instantiateModel(report.source, report), report.diagnostic.c_str());
    EditorSession::TextureExtraction extraction;
    std::string diagnostic;
    AE_EXPECT_TRUE(session.extractSourceTextures("Fontes/tela.glb", extraction, diagnostic), diagnostic.c_str());
    texture = extraction.textures.front();
    texturePath = session.projectTextures().front().path;
    AE_EXPECT_EQ(session.textureUsersOf(texture), 0u, "extraída e ainda sem uso");
    AE_EXPECT_TRUE(session.setSlotTexture(firstMeshObject(session), 0, 0, EditorSession::MaterialScope::Instance, texture, diagnostic),
                   diagnostic.c_str());
    AE_EXPECT_EQ(session.textureUsersOf(texture), 1u, "um objeto usa a textura");
    AE_EXPECT_TRUE(session.save(scenePath.string().c_str(), 0), "cena salva");
    registry = session.serializeAssets();
  }

  // Reabertura como no shell: registro, antecipação da cena, publicação das fontes, cena.
  EditorSession session;
  Publisher publisher;
  start(session, publisher);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto reaberto");
  AE_EXPECT_TRUE(session.loadAssets(registry), "registro");
  session.anticipateSceneTextures(scenePath.string().c_str(), 0);
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(EditorImportTransaction::read(project.root / "Fontes/tela.glb", bytes), "fonte no projeto");
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(bytes, "Fontes/tela.glb", {}, report), report.diagnostic.c_str());
  AE_EXPECT_EQ(publisher.textures, sourceTextures + 1, "a textura da cena sobe junto com a fonte");
  const auto rebuilds = publisher.rebuilds;
  AE_EXPECT_TRUE(session.load(scenePath.string().c_str(), 0), "cena carregada");
  AE_EXPECT_EQ(publisher.rebuilds, rebuilds, "carregar a cena não publica de novo");
  AE_EXPECT_TRUE(resolvesToProjectTexture(effectiveTexture(session, firstMeshObject(session), 0)), "cor base da cena resolvida");

  // Apagar textura em uso: primeiro o aviso, depois volta à textura da fonte.
  EditorSession::ResourceChangeReport change;
  AE_EXPECT_TRUE(!session.deleteResource(texturePath, false, change) && change.sceneUsers == 1u, "aviso de uso antes de apagar");
  AE_EXPECT_TRUE(session.deleteResource(texturePath, true, change), "apagar mesmo assim");
  AE_EXPECT_TRUE(session.projectTextures().empty(), "textura saiu do projeto");
  AE_EXPECT_EQ(effectiveTexture(session, firstMeshObject(session), 0), scene::MaterialTextureKeep,
               "o binding volta à textura da fonte, sem textura inexistente na GPU");
}

namespace {
renderer::MaterialOverride effectiveMaterial(EditorSession &session, EditorEntityId object) {
  std::vector<renderer::MapDrawState> draws;
  if (session.extractMap(draws))
    for (const auto &draw : draws) if (draw.objectId == object && draw.visible) return draw.material;
  return {};
}
} // namespace

AE_TEST(r4_alpha_mode_cutoff_and_sides_persist_resolve_and_reach_renderer_flags) {
  // Formatos: componente v5 e material v3; material v2 lê sem superfície trocada.
  scene::MeshRenderer render;
  render.submeshes.resize(1);
  render.surface = {scene::MaterialAlphaMask, scene::MaterialSidesDouble, .3f};
  render.submeshes[0].surface.alphaMode = scene::MaterialAlphaBlend;
  std::ostringstream out;
  render.write(out);
  scene::MeshRenderer read;
  std::istringstream in(out.str());
  AE_EXPECT_TRUE(read.read(in, 5) && read.surface == render.surface && read.submeshes[0].surface == render.submeshes[0].surface,
                 "superfície por slot atravessa o arquivo");
  resources::MaterialAsset asset;
  asset.guid = resources::assetGuidFromSeed("r4-superficie");
  asset.name = "Vidro";
  asset.values.enabled = true;
  asset.surface = {scene::MaterialAlphaBlend, scene::MaterialSidesSingle, .5f};
  resources::MaterialAsset back;
  AE_EXPECT_TRUE(resources::MaterialAsset::deserialize(asset.serialize(), back) && back.surface == asset.surface, "material v3 ida e volta");
  const std::string v2 = "ASTRA_MATERIAL 2 " + asset.guid.text() + " 1 \"Velho\" 1 1 1 0.5 0 1 1 0 0 0 1 - - - -\n";
  resources::MaterialAsset old;
  AE_EXPECT_TRUE(resources::MaterialAsset::deserialize(v2, old) && !old.surface.overrides(), "material v2 herda alfa e faces");

  // Flags do renderer.
  renderer::MapMaterialRecord source{};
  std::fill(std::begin(source.textureIndices), std::end(source.textureIndices), renderer::InvalidMapTexture);
  source.flags = renderer::MapMaterialCullBackFaces;
  scene::MaterialParameters value;
  value.alphaMode = scene::MaterialAlphaMask;
  value.alphaCutoff = .25f;
  value.sides = scene::MaterialSidesDouble;
  auto applied = renderer::applyMaterialOverride(source, value);
  AE_EXPECT_TRUE((applied.flags & renderer::MapMaterialAlphaMask) && !(applied.flags & renderer::MapMaterialBlend), "recorte vai para a fila de corte");
  AE_EXPECT_EQ(applied.alphaCutoff, .25f, "corte aplicado");
  AE_EXPECT_TRUE((applied.flags & renderer::MapMaterialDoubleSided) && !(applied.flags & renderer::MapMaterialCullBackFaces), "duas faces sem culling");
  value.alphaMode = scene::MaterialAlphaBlend;
  value.sides = scene::MaterialSidesSingle;
  applied = renderer::applyMaterialOverride(source, value);
  AE_EXPECT_TRUE((applied.flags & renderer::MapMaterialBlend) && !(applied.flags & renderer::MapMaterialAlphaMask), "transparente");
  AE_EXPECT_TRUE((applied.flags & renderer::MapMaterialCullBackFaces) && !(applied.flags & renderer::MapMaterialDoubleSided), "uma face com culling");
  value.alphaMode = scene::MaterialAlphaOpaque;
  applied = renderer::applyMaterialOverride(source, value);
  AE_EXPECT_TRUE(!(applied.flags & (renderer::MapMaterialBlend | renderer::MapMaterialAlphaMask)), "opaco sai das filas de alfa");

  // Alcances na sessão.
  Project project;
  EditorSession session;
  Publisher publisher;
  start(session, publisher);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  const auto glb = texturedPanel(png(4, 4, 200));
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_TRUE((model.materials.front().flags & renderer::MapMaterialCullBackFaces) != 0, "glTF sem doubleSided pede culling");
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.commitModelImport(glb, model, "Fontes/tela.glb", "", report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), report.diagnostic.c_str());
  const auto object = firstMeshObject(session);
  std::string diagnostic;
  AE_EXPECT_EQ(effectiveMaterial(session, object).alphaMode, scene::MaterialAlphaKeep, "sem troca, a fonte decide");
  AE_EXPECT_TRUE(session.setSlotSurface(object, 0, EditorSession::MaterialScope::Instance,
                                        {scene::MaterialAlphaMask, scene::MaterialSidesKeep, .4f}, diagnostic), diagnostic.c_str());
  auto effective = effectiveMaterial(session, object);
  AE_EXPECT_TRUE(effective.alphaMode == scene::MaterialAlphaMask && effective.alphaCutoff == .4f, "recorte desta instância resolvido");
  AE_EXPECT_TRUE((session.importLinkOverrides(object) & ImportOverrideMaterial) != 0, "alfa trocado é alteração local do vínculo");

  const auto shared = session.createMaterialFromSlot(object, 0, diagnostic);
  AE_EXPECT_TRUE(shared.valid(), diagnostic.c_str());
  AE_EXPECT_TRUE(session.findMaterialAsset(shared)->surface.alphaMode == scene::MaterialAlphaMask, "material do projeto leva o recorte");
  AE_EXPECT_TRUE(!meshRenderer(*session.document().find(object))->surface.overrides(), "a instância deixa de trocar sozinha");
  AE_EXPECT_TRUE(session.setSlotSurface(object, 0, EditorSession::MaterialScope::Shared,
                                        {scene::MaterialAlphaBlend, scene::MaterialSidesSingle, .5f}, diagnostic), diagnostic.c_str());
  effective = effectiveMaterial(session, object);
  AE_EXPECT_TRUE(effective.alphaMode == scene::MaterialAlphaBlend && effective.sides == scene::MaterialSidesSingle,
                 "transparente e uma face pelo material compartilhado");
  std::vector<u8> file;
  AE_EXPECT_TRUE(EditorImportTransaction::read(project.root / session.assets().find(shared)->path, file) &&
                 std::string(file.begin(), file.end()).starts_with("ASTRA_MATERIAL 6"), "material gravado na versão atual");

  AE_EXPECT_TRUE(session.revertImportLink(object, ImportOverrideMaterial), "reverter material à fonte");
  effective = effectiveMaterial(session, object);
  AE_EXPECT_TRUE(effective.alphaMode == scene::MaterialAlphaKeep && effective.sides == scene::MaterialSidesKeep, "de volta à fonte");
}

AE_TEST(r4_sampling_uv_set_wrap_and_filter_resolve_publish_variants_and_travel_to_the_project_material) {
  // Contrato puro: dois bits por binding no registro e flags de sampler.
  renderer::MapMaterialRecord record{};
  record.textureCoordinates = 0;
  renderer::MaterialOverride value;
  value.uvSets[1] = scene::MaterialUv1;
  AE_EXPECT_EQ(renderer::applyMaterialOverride(record, value).textureCoordinates, 1u << 2, "normal passa a UV1");
  record.textureCoordinates = 0xffu;
  value = {};
  value.uvSets[3] = scene::MaterialUv0;
  AE_EXPECT_EQ(renderer::applyMaterialOverride(record, value).textureCoordinates, 0x3fu, "emissão volta a UV0 sem tocar os outros");
  const auto clampNearest = EditorMapScene::samplerFlags({scene::MaterialUvKeep, scene::MaterialWrapClamp, scene::MaterialFilterNearest});
  AE_EXPECT_TRUE((clampNearest & (renderer::AuthoringTextureRepeatU | renderer::AuthoringTextureLinearFilter | renderer::AuthoringTextureLinearMip)) == 0,
                 "limitar e mais próximo tiram repetição e filtro linear");
  AE_EXPECT_TRUE(EditorMapScene::samplerFlags({0, scene::MaterialWrapMirror, 0}) & renderer::AuthoringTextureMirrorV, "espelhar");
  AE_EXPECT_EQ(EditorMapScene::samplerFlags({}), EditorMapScene::DefaultTextureSampler, "herdar é o padrão");

  Project project;
  EditorSession session;
  Publisher publisher;
  start(session, publisher);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  const auto glb = texturedPanel(png(4, 4, 200));
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, model), model.diagnostic.c_str());
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.commitModelImport(glb, model, "Fontes/tela.glb", "", report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), report.diagnostic.c_str());
  std::string diagnostic;
  EditorSession::TextureExtraction extraction;
  AE_EXPECT_TRUE(session.extractSourceTextures("Fontes/tela.glb", extraction, diagnostic), diagnostic.c_str());
  const auto object = firstMeshObject(session);
  AE_EXPECT_TRUE(object != kInvalidEntity, "instância com malha");

  // Só o conjunto de UV, com a textura da fonte: vale e é alteração local.
  const scene::MaterialSampling uv1{scene::MaterialUv1, scene::MaterialWrapKeep, scene::MaterialFilterKeep};
  AE_EXPECT_TRUE(session.setSlotSampling(object, 0, 0, EditorSession::MaterialScope::Instance, uv1, diagnostic), diagnostic.c_str());
  AE_EXPECT_EQ(effectiveMaterial(session, object).uvSets[0], scene::MaterialUv1, "UV1 resolvido na cor base");
  AE_EXPECT_TRUE((session.importLinkOverrides(object) & ImportOverrideMaterial) != 0, "amostragem conta como material local");
  AE_EXPECT_TRUE(!session.setSlotSampling(object, 0, 0, EditorSession::MaterialScope::Instance, {3, 0, 0}, diagnostic),
                 "valor fora do contrato recusado");

  // Textura do projeto com outro sampler: uma variante publicada, índice próprio.
  AE_EXPECT_TRUE(session.setSlotTexture(object, 0, 0, EditorSession::MaterialScope::Instance, extraction.textures.front(), diagnostic),
                 diagnostic.c_str());
  const auto repeating = effectiveTexture(session, object, 0);
  AE_EXPECT_TRUE(resolvesToProjectTexture(repeating), "textura do projeto com o sampler padrão");
  const auto before = publisher.rebuilds;
  const scene::MaterialSampling clamped{scene::MaterialUv1, scene::MaterialWrapClamp, scene::MaterialFilterNearest};
  AE_EXPECT_TRUE(session.setSlotSampling(object, 0, 0, EditorSession::MaterialScope::Instance, clamped, diagnostic), diagnostic.c_str());
  AE_EXPECT_EQ(publisher.rebuilds, before + 1, "sampler novo custa uma publicação");
  const auto variant = effectiveTexture(session, object, 0);
  // Só a combinação em uso sobe: a variante pode ocupar a posição da anterior.
  AE_EXPECT_TRUE(resolvesToProjectTexture(variant) && variant < publisher.samplers.size() &&
                     publisher.samplers[variant] == EditorMapScene::samplerFlags(clamped),
                 "o binding resolve para a textura publicada com limitar e mais próximo");
  AE_EXPECT_TRUE(repeating < publisher.samplers.size() || repeating == variant, "índice anterior coerente");

  // O componente grava a amostragem e lê de volta.
  {
    const auto *render = meshRenderer(*session.document().find(object));
    std::ostringstream out;
    render->write(out);
    AE_EXPECT_TRUE(out.str().find(" 222") != std::string::npos, "token uvf gravado");
    scene::MeshRenderer back;
    std::istringstream in(out.str());
    AE_EXPECT_TRUE(back.read(in, 6) && back.sampling == render->sampling, "v6 volta igual");
  }

  // Criar material do projeto leva a amostragem; a instância deixa de trocar sozinha.
  const auto shared = session.createMaterialFromSlot(object, 0, diagnostic);
  AE_EXPECT_TRUE(shared.valid(), diagnostic.c_str());
  AE_EXPECT_TRUE(session.findMaterialAsset(shared)->sampling[0] == clamped, "material do projeto guarda a amostragem");
  AE_EXPECT_TRUE(!meshRenderer(*session.document().find(object))->sampling[0].overrides(), "instância limpa");
  AE_EXPECT_EQ(effectiveTexture(session, object, 0), variant, "mesma textura publicada pelo material compartilhado");
  resources::MaterialAsset roundTrip;
  AE_EXPECT_TRUE(resources::MaterialAsset::deserialize(session.findMaterialAsset(shared)->serialize(), roundTrip) &&
                 roundTrip.sampling[0] == clamped, "MaterialAsset v4 volta igual");

  AE_EXPECT_TRUE(session.revertImportLink(object, ImportOverrideMaterial), "reverter material à fonte");
  AE_EXPECT_EQ(effectiveMaterial(session, object).uvSets[0], scene::MaterialUvKeep, "UV volta à fonte");
}

AE_TEST(r4_uv_transform_per_binding_resolves_reaches_the_shader_entry_and_persists) {
  // Contrato puro: T·R·S do KHR_texture_transform e a entrada que o shader lê.
  scene::MaterialSampling rotated{};
  rotated.rotation = 90;
  rotated.scale[0] = 2;
  rotated.offset[0] = .5f;
  float rows[6]{};
  scene::materialUvTransformRows(rotated, rows);
  const auto near = [](float a, float b) { return std::fabs(a - b) < 1e-5f; };
  AE_EXPECT_TRUE(near(rows[0], 0) && near(rows[1], 1) && near(rows[2], .5f) && near(rows[3], -2) && near(rows[4], 0) && near(rows[5], 0),
                 "linhas (cos·sx, sin·sy, dx / -sin·sx, cos·sy, dy)");
  renderer::MaterialOverride value;
  value.uvTransformMask = 1u << 2;
  std::copy(rows, rows + 6, value.uvTransforms[2]);
  float entry[renderer::MaterialUvTransformFloats]{};
  renderer::materialUvTransformEntry(value, entry);
  AE_EXPECT_TRUE(entry[0] == 1 && entry[1] == 0 && entry[4] == 0 && entry[5] == 1, "binding sem transformação é identidade");
  AE_EXPECT_TRUE(near(entry[16 + 1], 1) && near(entry[16 + 2], .5f) && near(entry[16 + 4], -2) && entry[16 + 3] == 0,
                 "linhas do metal/rugosidade na posição do binding 2");
  scene::MaterialSampling flat{};
  flat.scale[1] = 0;
  AE_EXPECT_TRUE(!scene::validMaterialSampling(flat), "escala zero recusada");

  Project project;
  EditorSession session;
  Publisher publisher;
  start(session, publisher);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  const auto glb = texturedPanel(png(4, 4, 200));
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, model), model.diagnostic.c_str());
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.commitModelImport(glb, model, "Fontes/tela.glb", "", report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), report.diagnostic.c_str());
  const auto object = firstMeshObject(session);
  AE_EXPECT_TRUE(object != kInvalidEntity, "instância com malha");
  std::string diagnostic;

  // Normal ladrilhada 4x com deslocamento em V, na textura da fonte.
  scene::MaterialSampling tiled{};
  tiled.scale[0] = tiled.scale[1] = 4;
  tiled.offset[1] = .25f;
  const auto rebuilds = publisher.rebuilds;
  AE_EXPECT_TRUE(session.setSlotSampling(object, 0, 1, EditorSession::MaterialScope::Instance, tiled, diagnostic), diagnostic.c_str());
  AE_EXPECT_EQ(publisher.rebuilds, rebuilds, "transformação não publica textura nenhuma");
  auto effective = effectiveMaterial(session, object);
  AE_EXPECT_TRUE((effective.uvTransformMask & 2u) != 0 && effective.uvTransforms[1][0] == 4 && effective.uvTransforms[1][5] == .25f,
                 "normal com escala 4 e deslocamento V resolvidos");
  AE_EXPECT_EQ(effective.uvTransformMask & ~2u, 0u, "os outros bindings seguem sem transformação");
  AE_EXPECT_TRUE((session.importLinkOverrides(object) & ImportOverrideMaterial) != 0, "transformação conta como material local");
  {
    const auto *render = meshRenderer(*session.document().find(object));
    std::ostringstream out;
    render->write(out);
    scene::MeshRenderer back;
    std::istringstream in(out.str());
    AE_EXPECT_TRUE(back.read(in, 7) && back.sampling == render->sampling && back.sampling[1] == tiled, "v7 volta igual");
  }

  // Material do projeto leva a transformação; a instância herda dele.
  const auto shared = session.createMaterialFromSlot(object, 0, diagnostic);
  AE_EXPECT_TRUE(shared.valid(), diagnostic.c_str());
  AE_EXPECT_TRUE(session.findMaterialAsset(shared)->sampling[1] == tiled, "material do projeto guarda a transformação");
  AE_EXPECT_TRUE(!meshRenderer(*session.document().find(object))->sampling[1].transformed(), "instância limpa");
  AE_EXPECT_TRUE((effectiveMaterial(session, object).uvTransformMask & 2u) != 0, "herdada do material compartilhado");
  const auto serialized = session.findMaterialAsset(shared)->serialize();
  resources::MaterialAsset back;
  AE_EXPECT_TRUE(serialized.starts_with("ASTRA_MATERIAL 6") && resources::MaterialAsset::deserialize(serialized, back) &&
                     back.sampling[1] == tiled, "MaterialAsset v5 volta igual");

  AE_EXPECT_TRUE(session.revertImportLink(object, ImportOverrideMaterial), "reverter material à fonte");
  AE_EXPECT_EQ(effectiveMaterial(session, object).uvTransformMask, u8{0}, "sem transformação depois de reverter");
}

AE_TEST(r4_occlusion_channels_normal_and_alpha_source_resolve_reach_the_extension_and_persist) {
  // Entrada da extensão: o padrão da fonte e as escolhas, já resolvidos para o shader.
  renderer::MaterialOverride keep;
  float entry[renderer::MaterialExtensionFloats]{};
  AE_EXPECT_TRUE(!renderer::needsMaterialExtension(keep), "sem escolha, sem entrada");
  renderer::materialExtensionEntry(keep, renderer::MapMaterialOcclusionInMetallicRoughness, renderer::MaterialExtensionNoTexture, entry);
  AE_EXPECT_TRUE(entry[32] == 1 && entry[33] == 1 && entry[34] == 0 && entry[36] == 1 && entry[37] == 2 && entry[38] == 0 && entry[39] == 0,
                 "padrão glTF: oclusão no R do metal/rugosidade, rugosidade G, metal B, alfa da cor base");
  renderer::MaterialOverride chosen;
  chosen.channels.occlusionSource = scene::MaterialOcclusionTexture;
  chosen.channels.occlusionStrength = .5f;
  chosen.channels.occlusion = scene::MaterialChannelG;
  chosen.channels.roughness = scene::MaterialChannelR;
  chosen.channels.alphaSource = scene::MaterialAlphaSourceLuminance;
  chosen.channels.normalFlipY = scene::MaterialToggleOn;
  chosen.isolate = scene::MaterialIsolateOcclusion;
  AE_EXPECT_TRUE(renderer::needsMaterialExtension(chosen), "escolhas pedem entrada");
  renderer::materialExtensionEntry(chosen, 0, 7, entry);
  AE_EXPECT_TRUE(entry[32] == .5f && entry[33] == 2 && entry[34] == 1 && entry[35] == 7, "textura própria no índice 7, canal G, força 0,5");
  AE_EXPECT_TRUE(entry[36] == 0 && entry[37] == 2 && entry[38] == 2 && entry[39] == 1 && entry[40] == 1,
                 "rugosidade R, metal herdado B, luminância, Y invertido, isolando a oclusão");
  renderer::materialExtensionEntry(chosen, 0, renderer::MaterialExtensionNoTexture, entry);
  AE_EXPECT_EQ(entry[33], 0.0f, "textura própria sem textura publicada vira sem oclusão");
  scene::MaterialChannels invalid{};
  invalid.occlusionStrength = 2;
  AE_EXPECT_TRUE(!scene::validMaterialChannels(invalid), "força acima de 1 recusada");

  Project project;
  EditorSession session;
  Publisher publisher;
  start(session, publisher);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  const auto glb = texturedPanel(png(4, 4, 200));
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, model), model.diagnostic.c_str());
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.commitModelImport(glb, model, "Fontes/tela.glb", "", report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), report.diagnostic.c_str());
  std::string diagnostic;
  EditorSession::TextureExtraction extraction;
  AE_EXPECT_TRUE(session.extractSourceTextures("Fontes/tela.glb", extraction, diagnostic), diagnostic.c_str());
  const auto object = firstMeshObject(session);
  AE_EXPECT_TRUE(object != kInvalidEntity, "instância com malha");

  scene::MaterialChannels channels{};
  channels.occlusionSource = scene::MaterialOcclusionTexture;
  channels.occlusionStrength = .75f;
  channels.normalFlipY = scene::MaterialToggleOn;
  channels.alphaSource = scene::MaterialAlphaSourceOpaque;
  AE_EXPECT_TRUE(session.setSlotChannels(object, 0, EditorSession::MaterialScope::Instance, channels, diagnostic), diagnostic.c_str());
  const auto before = publisher.rebuilds;
  AE_EXPECT_TRUE(session.setSlotTexture(object, 0, scene::MaterialOcclusionTextureBinding, EditorSession::MaterialScope::Instance,
                                        extraction.textures.front(), diagnostic), diagnostic.c_str());
  AE_EXPECT_EQ(publisher.rebuilds, before + 1, "textura de oclusão nova custa uma publicação");
  auto effective = effectiveMaterial(session, object);
  AE_EXPECT_TRUE(effective.channels == channels, "canais resolvidos");
  AE_EXPECT_TRUE(resolvesToProjectTexture(effective.occlusionTexture) && effective.occlusionTexture < publisher.samplers.size(),
                 "textura de oclusão resolvida na biblioteca publicada");
  AE_EXPECT_TRUE((session.importLinkOverrides(object) & ImportOverrideMaterial) != 0, "canais contam como material local");
  {
    const auto *render = meshRenderer(*session.document().find(object));
    std::ostringstream out;
    render->write(out);
    scene::MeshRenderer back;
    std::istringstream in(out.str());
    AE_EXPECT_TRUE(back.read(in, 8) && back.channels == render->channels && back.occlusionTexture == render->occlusionTexture, "v8 volta igual");
  }

  const auto shared = session.createMaterialFromSlot(object, 0, diagnostic);
  AE_EXPECT_TRUE(shared.valid(), diagnostic.c_str());
  AE_EXPECT_TRUE(session.findMaterialAsset(shared)->channels == channels &&
                     session.findMaterialAsset(shared)->occlusionTexture == extraction.textures.front(),
                 "material do projeto leva canais e textura de oclusão");
  AE_EXPECT_TRUE(!meshRenderer(*session.document().find(object))->channels.overrides() &&
                     !meshRenderer(*session.document().find(object))->occlusionTexture.valid(), "instância limpa");
  AE_EXPECT_TRUE(effectiveMaterial(session, object).channels == channels, "herdados do material compartilhado");
  resources::MaterialAsset back;
  const auto serialized = session.findMaterialAsset(shared)->serialize();
  AE_EXPECT_TRUE(serialized.starts_with("ASTRA_MATERIAL 6") && resources::MaterialAsset::deserialize(serialized, back) &&
                     back.channels == channels && back.occlusionTexture == extraction.textures.front(), "MaterialAsset v6 volta igual");

  AE_EXPECT_TRUE(session.revertImportLink(object, ImportOverrideMaterial), "reverter material à fonte");
  effective = effectiveMaterial(session, object);
  AE_EXPECT_TRUE(effective.channels == scene::MaterialChannels{} && effective.occlusionTexture == scene::MaterialTextureKeep,
                 "canais e oclusão voltam à fonte");
}

AE_TEST(r4_texture_profile_changes_interpretation_mips_anisotropy_and_is_read_back_on_reopen) {
  Project project;
  EditorSession session;
  Publisher publisher;
  start(session, publisher);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  const auto glb = texturedPanel(png(4, 4, 200));
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, model), model.diagnostic.c_str());
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.commitModelImport(glb, model, "Fontes/tela.glb", "", report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), report.diagnostic.c_str());
  std::string diagnostic;
  EditorSession::TextureExtraction extraction;
  AE_EXPECT_TRUE(session.extractSourceTextures("Fontes/tela.glb", extraction, diagnostic), diagnostic.c_str());
  const auto object = firstMeshObject(session);
  const auto texture = extraction.textures.front();
  AE_EXPECT_TRUE(session.setSlotTexture(object, 0, 0, EditorSession::MaterialScope::Instance, texture, diagnostic), diagnostic.c_str());
  u32 index = effectiveTexture(session, object, 0);
  AE_EXPECT_TRUE(index < publisher.published.size() && publisher.published[index]->levels == 3 && publisher.published[index]->srgb &&
                     !(publisher.published[index]->samplerFlags & renderer::AuthoringTextureNoAnisotropy),
                 "padrão: cor base com mips, sRGB pelo uso e anisotropia da qualidade");

  resources::TextureProfile profile;
  profile.interpretation = resources::TextureInterpretationData;
  profile.mipmaps = false;
  profile.dilateEdges = true;
  profile.anisotropy = false;
  const auto rebuilds = publisher.rebuilds;
  AE_EXPECT_TRUE(session.setTextureProfile(texture, profile, diagnostic), diagnostic.c_str());
  AE_EXPECT_EQ(publisher.rebuilds, rebuilds + 1, "perfil novo republica as texturas");
  index = effectiveTexture(session, object, 0);
  AE_EXPECT_TRUE(index < publisher.published.size(), "binding continua resolvido");
  const auto &published = publisher.published[index];
  AE_EXPECT_TRUE(published->levels == 1 && !published->srgb && (published->samplerFlags & renderer::AuthoringTextureNoAnisotropy),
                 "sem mips, dado linear e sem anisotropia na textura publicada");
  const auto residency = session.textureResidencyOf(texture);
  AE_EXPECT_TRUE(!residency.empty() && residency.front().levels == 1 && residency.front().width == 4 && !residency.front().srgb &&
                     residency.front().bytes == 16 * 4,
                 "residência consultável por textura");
  AE_EXPECT_TRUE(std::filesystem::exists(project.root / resources::textureProfilePath(texture)), "perfil gravado no projeto");
  resources::TextureProfile invalid;
  invalid.maximumDimension = 300;
  AE_EXPECT_TRUE(!session.setTextureProfile(texture, invalid, diagnostic), "perfil fora dos passos recusado");

  // Reabrir como o shell faz: diretório do projeto e o registro gravado.
  const auto registry = session.serializeAssets();
  EditorSession reopened;
  Publisher other;
  start(reopened, other);
  AE_EXPECT_TRUE(reopened.setProjectDirectory(project.root.string().c_str()), "reabrir o projeto");
  AE_EXPECT_TRUE(reopened.loadAssets(registry), "registro");
  AE_EXPECT_TRUE(resources::sameTextureProfile(reopened.textureProfileFor(texture), profile), "perfil lido na reabertura");
}

AE_TEST(r4_texture_manager_filters_searches_and_the_inspector_lists_users) {
  Project project;
  EditorSession session;
  Publisher publisher;
  start(session, publisher);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  const auto glb = texturedPanel(png(4, 4, 200));
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, model), model.diagnostic.c_str());
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.commitModelImport(glb, model, "Fontes/tela.glb", "", report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), report.diagnostic.c_str());
  std::string diagnostic;
  EditorSession::TextureExtraction extraction;
  AE_EXPECT_TRUE(session.extractSourceTextures("Fontes/tela.glb", extraction, diagnostic), diagnostic.c_str());
  const auto object = firstMeshObject(session);
  const auto texture = extraction.textures.front();

  session.openTextureManager();
  session.update();
  AE_EXPECT_TRUE(session.screen().textureManager && session.screen().textureManagerRows.size() == 1, "gerenciador lista a textura");
  session.setTextureFilter(1);
  session.update();
  AE_EXPECT_EQ(session.screen().textureManagerRows.size(), usize{1}, "não usada aparece em Não usadas");
  AE_EXPECT_TRUE(session.setSlotTexture(object, 0, 0, EditorSession::MaterialScope::Instance, texture, diagnostic), diagnostic.c_str());
  session.update();
  AE_EXPECT_EQ(session.screen().textureManagerRows.size(), usize{0}, "usada sai de Não usadas");
  session.setTextureFilter(4);
  session.update();
  AE_EXPECT_EQ(session.screen().textureManagerRows.size(), usize{1}, "PNG sem transparência aparece em Sem alfa");
  session.setTextureFilter(0);
  session.setTextureQuery("PINTURA");
  session.update();
  AE_EXPECT_EQ(session.screen().textureManagerRows.size(), usize{1}, "busca sem diferenciar maiúsculas");
  session.setTextureQuery("tijolo");
  session.update();
  AE_EXPECT_EQ(session.screen().textureManagerRows.size(), usize{0}, "busca sem resultado");
  session.setTextureQuery("");

  // Arquivo trocado fora do editor: o conteúdo não bate mais com o registro.
  const auto path = session.assets().find(texture)->path;
  AE_EXPECT_TRUE(EditorImportTransaction::write(project.root / path, png(4, 4, 10)), "arquivo trocado no disco");
  AE_EXPECT_TRUE(session.loadAssets(session.serializeAssets()), "registro recarregado");
  session.openTextureManager();
  session.setTextureFilter(3);
  session.update();
  AE_EXPECT_EQ(session.screen().textureManagerRows.size(), usize{1}, "aparece em Alteradas");

  session.openTextureInspector(0);
  session.update();
  AE_EXPECT_TRUE(session.screen().textureInspector && session.screen().textureViewer && !session.screen().textureManager,
                 "textura em Propriedades com o visualizador");
  AE_EXPECT_TRUE(session.screen().textureUserLabels.size() == 1 && session.screen().textureUserEntities[0] == object,
                 "o objeto que usa a textura aparece e é selecionável");
  session.setSelection(session.document().root());
  session.update();
  AE_EXPECT_TRUE(!session.screen().textureInspector, "mudar a seleção devolve Propriedades ao objeto");
}
