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
  AE_EXPECT_TRUE(read.read(in, 4), "v4 lido");
  AE_EXPECT_TRUE(read.textures == render.textures && read.submeshes[0].textures == render.submeshes[0].textures, "v4 preserva texturas");
  auto words = tokens(out.str());
  words.resize(words.size() - 8); // 4 tokens por slot, dois slots
  std::string v3;
  for (const auto &word : words) v3 += word + ' ';
  scene::MeshRenderer legacy;
  std::istringstream legacyIn(v3);
  AE_EXPECT_TRUE(legacy.read(legacyIn, 3), "v3 lido");
  AE_EXPECT_TRUE(legacy.textures == scene::SlotTextures{}, "v3 herda texturas");

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
  AE_EXPECT_TRUE(text.starts_with("ASTRA_MATERIAL 2") && text.find(texture.text()) != std::string::npos && text.find("none") != std::string::npos,
                 "material gravado em v2 com as texturas");

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
