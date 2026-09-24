// M08.2 — identidade de nós e reimportação sem perda.
//
// Fontes geradas aqui mesmo, sem nome de carro nem de projeto: um "Veiculo"
// genérico com peças de mesmo nome, e um conjunto que não é veículo. O que se
// fixa é o contrato — base, fonte nova e edição local —, não um arquivo.
#include "editor/editor_import_transaction.h"
#include "harness.h"
#include "editor/editor_archive.h"
#include "editor/editor_import_reconcile.h"
#include "editor/editor_session.h"
#include "renderer/authoring_geometry.h"
#include "resources/import_node_map.h"
#include "resources/import_cache.h"
#include "resources/import_profile.h"
#include <fstream>
#include "scene/import_link.h"
#include "scene/script_behavior.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

using namespace ae;
using namespace ae::editor;

namespace {
void putU32(std::vector<u8> &out, u32 value) {
  for (u32 i = 0; i < 4; ++i) out.push_back(static_cast<u8>(value >> (i * 8)));
}

// Três malhas sobre o mesmo buffer: "Chapa" (quadrado), "Roda" (triângulo) e
// "Duplo" (as duas como primitivas de um nó só). Geometrias diferentes dão
// assinaturas diferentes, que é o que desempata peças de mesmo nome.
std::vector<u8> glb(const std::string &nodes, const std::string &roots) {
  std::vector<u8> binary;
  const float positions[12]{0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
  for (float value : positions) {
    u32 bits = 0;
    std::memcpy(&bits, &value, 4);
    putU32(binary, bits);
  }
  const u16 quad[6]{0, 1, 2, 0, 2, 3}, tri[3]{0, 1, 2};
  for (u16 value : quad) { binary.push_back(static_cast<u8>(value)); binary.push_back(static_cast<u8>(value >> 8)); }
  for (u16 value : tri) { binary.push_back(static_cast<u8>(value)); binary.push_back(static_cast<u8>(value >> 8)); }
  while (binary.size() % 4) binary.push_back(0);
  std::string json =
      std::string(R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":)") + std::to_string(binary.size()) + R"(}],)" +
      R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":12},{"buffer":0,"byteOffset":60,"byteLength":6}],)" +
      R"("accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},)" +
      R"({"bufferView":1,"componentType":5123,"count":6,"type":"SCALAR"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],)" +
      R"("meshes":[{"name":"Chapa","primitives":[{"attributes":{"POSITION":0},"indices":1}]},)" +
      R"({"name":"Roda","primitives":[{"attributes":{"POSITION":0},"indices":2}]},)" +
      R"({"name":"Duplo","primitives":[{"attributes":{"POSITION":0},"indices":1},{"attributes":{"POSITION":0},"indices":2}]},)" +
      R"({"name":"Trio","primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0},{"attributes":{"POSITION":0},"indices":2,"material":1},{"attributes":{"POSITION":0},"indices":1,"material":2}]}],)" +
      R"("materials":[{"name":"Pintura","pbrMetallicRoughness":{"baseColorFactor":[1,0,0,1]}},)" +
      R"({"name":"Vidro","pbrMetallicRoughness":{"baseColorFactor":[0,0,1,1]}},)" +
      R"({"name":"Borracha","pbrMetallicRoughness":{"baseColorFactor":[0.1,0.1,0.1,1]}}],)" +
      R"("nodes":[)" + nodes + R"(],"scenes":[{"nodes":[)" + roots + R"(]}],"scene":0})";
  while (json.size() % 4) json.push_back(' ');
  std::vector<u8> out;
  putU32(out, 0x46546C67);
  putU32(out, 2);
  putU32(out, static_cast<u32>(12 + 8 + json.size() + 8 + binary.size()));
  putU32(out, static_cast<u32>(json.size()));
  putU32(out, 0x4E4F534A);
  out.insert(out.end(), json.begin(), json.end());
  putU32(out, static_cast<u32>(binary.size()));
  putU32(out, 0x004E4942);
  out.insert(out.end(), binary.begin(), binary.end());
  return out;
}

// Revisão 1: carroceria e duas rodas de MESMO nome, em lados opostos.
std::vector<u8> vehicleV1() {
  return glb(R"({"name":"Veiculo","children":[1,2,3]},)"
             R"({"name":"Carroceria","mesh":0},)"
             R"({"name":"Roda","mesh":1,"translation":[1,0,0]},)"
             R"({"name":"Roda","mesh":1,"translation":[-1,0,0]})",
             "0");
}
// Revisão 2: a carroceria sobe na fonte e entra uma porta nova. As rodas
// homônimas continuam onde estavam.
std::vector<u8> vehicleV2() {
  return glb(R"({"name":"Veiculo","children":[1,2,3,4]},)"
             R"({"name":"Carroceria","mesh":0,"translation":[0,0.5,0]},)"
             R"({"name":"Roda","mesh":1,"translation":[1,0,0]},)"
             R"({"name":"Roda","mesh":1,"translation":[-1,0,0]},)"
             R"({"name":"Porta","mesh":0,"translation":[0,0,2]})",
             "0");
}
// Revisão ambígua: as duas rodas mudaram de lugar, e nada no arquivo diz qual
// é qual.
std::vector<u8> vehicleAmbiguous() {
  return glb(R"({"name":"Veiculo","children":[1,2,3]},)"
             R"({"name":"Carroceria","mesh":0},)"
             R"({"name":"Roda","mesh":1,"translation":[2,0,0]},)"
             R"({"name":"Roda","mesh":1,"translation":[-2,0,0]})",
             "0");
}
// Revisão sem as rodas.
std::vector<u8> vehicleWithoutWheels() {
  return glb(R"({"name":"Veiculo","children":[1]},)"
             R"({"name":"Carroceria","mesh":0})",
             "0");
}

struct FakeRenderer {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  u32 rebuilds = 0;
  bool publish(std::span<const u8> extraVertices, std::span<const u32> extraIndices,
               std::span<const renderer::MapDrawRecord> extraDraws,
               std::span<const renderer::MapMaterialRecord> extraMaterials, EditorSession::PublishedGeometry &out) {
    ++rebuilds;
    vertices.clear(); indices.clear(); draws.clear(); materials.clear();
    if (!renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, vertices, indices, draws, materials)) return false;
    const auto vertexBase = static_cast<u32>(vertices.size() / renderer::MapVertexStride);
    const auto indexBase = static_cast<u32>(indices.size());
    const auto materialBase = static_cast<u32>(materials.size());
    vertices.insert(vertices.end(), extraVertices.begin(), extraVertices.end());
    indices.insert(indices.end(), extraIndices.begin(), extraIndices.end());
    materials.insert(materials.end(), extraMaterials.begin(), extraMaterials.end());
    for (const auto &source : extraDraws) {
      auto draw = source;
      draw.firstIndex += indexBase; draw.vertexOffset += vertexBase; draw.materialIndex += materialBase;
      draw.lodGroupId = static_cast<u32>(draws.size());
      draws.push_back(draw);
    }
    out = {draws, materials, vertices, indices};
    return true;
  }
};

void start(EditorSession &session, FakeRenderer &renderer) {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, vertices, indices, draws, materials);
  session.importMap(draws, materials, false, vertices, indices, 0);
  session.setGeometryPublisher([&renderer](std::span<const u8> v, std::span<const u32> i,
                                           std::span<const renderer::MapDrawRecord> d,
                                           std::span<const renderer::MapMaterialRecord> m,
                                           std::span<const renderer::SharedAuthoringTexture>,
                                           EditorSession::PublishedGeometry &out) { return renderer.publish(v, i, d, m, out); });
}

struct Project {
  std::filesystem::path root = std::filesystem::temp_directory_path() /
      ("astra-m082-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Project() { std::filesystem::create_directories(root); }
  ~Project() { std::error_code error; std::filesystem::remove_all(root, error); }
};

const char *kSource = "Fontes/veiculo.glb";

bool commit(EditorSession &session, const Project &project, const std::vector<u8> &bytes,
            EditorSession::ModelImportReport &report,
            resources::ImportAmbiguityPolicy policy = resources::ImportAmbiguityPolicy::Refuse) {
  resources::GltfImport model;
  if (!resources::importGlb(bytes, {}, {}, model)) { report.diagnostic = model.diagnostic; return false; }
  std::vector<u8> current;
  const bool exists = EditorImportTransaction::read(project.root / kSource, current);
  return session.commitModelImport(bytes, model, kSource, exists ? Sha256::hex(current) : std::string(), report, policy);
}

std::vector<EditorEntityId> children(const EditorDocument &document, EditorEntityId id) {
  const auto span = document.childrenOf(id);
  return {span.begin(), span.end()};
}

EditorEntityId childNamed(const EditorDocument &document, EditorEntityId parent, const char *name, u32 occurrence = 0) {
  for (const auto id : children(document, parent))
    if (std::strcmp(document.find(id)->name, name) == 0 && occurrence-- == 0) return id;
  return kInvalidEntity;
}

const scene::ImportLink *linkOf(const EditorDocument &document, EditorEntityId id) {
  const auto *entity = document.find(id);
  return entity ? scene::importLink(entity->components) : nullptr;
}

void attachScript(EditorSession &session, EditorEntityId id, const char *type) {
  auto values = *session.document().find(id);
  auto *script = static_cast<scene::ScriptBehavior *>(values.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType = type;
  session.history().begin("Script");
  session.history().applyValues(session.document(), id, values);
  session.history().end();
}

void move(EditorSession &session, EditorEntityId id, u32 axis, float value) {
  auto values = *session.document().find(id);
  values.transform.position[axis] = value;
  session.history().begin("Mover");
  session.history().applyValues(session.document(), id, values);
  session.history().end();
}

u32 linkedCount(const EditorDocument &document) {
  std::vector<EditorEntityId> ids;
  document.collectSubtree(document.root(), ids);
  u32 count = 0;
  for (const auto id : ids) if (linkOf(document, id)) ++count;
  return count;
}
} // namespace

// Perfil com escala e teto de textura; o resto fica no padrão do formato.
static resources::ImportProfile profileOf(float scale, u32 maximumTextureDimension) {
  resources::ImportProfile profile;
  profile.scale = scale;
  profile.maximumTextureDimension = maximumTextureDimension;
  return profile;
}

AE_TEST(m082_node_map_starts_from_legacy_keys_and_round_trips) {
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(vehicleV1(), {}, {}, model), model.diagnostic.c_str());
  const auto source = resources::assetGuidFromSeed("fonte:Fontes/x.glb");
  resources::ImportNodeMap map, again;
  resources::ImportMatchReport report;
  std::string diagnostic;
  AE_EXPECT_TRUE(resources::buildImportNodeMap(model, source, "h1", nullptr, resources::ImportAmbiguityPolicy::Refuse, map, report, diagnostic),
                 diagnostic.c_str());
  AE_EXPECT_EQ(map.revision, 1u, "primeira revisão");
  AE_EXPECT_EQ(map.nodes.size(), 4u, "um registro por nó, inclusive o grupo sem malha");
  // As identidades de desenho são as da chave legada: cenas gravadas antes do
  // mapa continuam apontando para as mesmas malhas.
  AE_EXPECT_TRUE(map.nodes[1].draws.front() == resources::assetGuidFromSeed("glb:" + source.text() + ":" + model.keys[0]),
                 "desenho mantém a identidade legada");
  AE_EXPECT_TRUE(map.nodes[2].id != map.nodes[3].id, "nós homônimos têm identidades distintas");
  AE_EXPECT_TRUE(resources::ImportNodeMap::deserialize(map.serialize(), again), "mapa relido");
  AE_EXPECT_EQ(again.serialize(), map.serialize(), "ida e volta sem perda");
  // Deterministico: a mesma fonte sem mapa gera o mesmo mapa.
  AE_EXPECT_TRUE(resources::buildImportNodeMap(model, source, "h1", nullptr, resources::ImportAmbiguityPolicy::Refuse, again, report, diagnostic),
                 "de novo");
  AE_EXPECT_EQ(again.serialize(), map.serialize(), "mesma fonte, mesmo mapa");
}

AE_TEST(m082_matching_uses_authored_ids_renames_and_reparenting) {
  const auto source = resources::assetGuidFromSeed("fonte:Fontes/conjunto.glb");
  std::string diagnostic;
  const auto build = [&](const std::vector<u8> &bytes, const resources::ImportNodeMap *previous, resources::ImportNodeMap &out,
                         resources::ImportMatchReport &report, const char *hash) {
    resources::GltfImport model;
    return resources::importGlb(bytes, {}, {}, model) &&
           resources::buildImportNodeMap(model, source, hash, previous, resources::ImportAmbiguityPolicy::Refuse, out, report, diagnostic);
  };
  // Um conjunto que não é veículo: prateleira com suportes.
  resources::ImportNodeMap first, second;
  resources::ImportMatchReport report;
  AE_EXPECT_TRUE(build(glb(R"({"name":"Estante","children":[1,2]},{"name":"Tabua","mesh":0,"extras":{"uuid":"tabua-1"}},{"name":"Suporte","mesh":1,"translation":[0,1,0]})", "0"),
                       nullptr, first, report, "a"), diagnostic.c_str());
  // Renomeados os dois, e o suporte trocou de pai para a tábua.
  AE_EXPECT_TRUE(build(glb(R"({"name":"Estante","children":[1]},{"name":"Prancha","mesh":0,"extras":{"uuid":"tabua-1"},"children":[2]},{"name":"Suporte","mesh":1,"translation":[0,1,0]})", "0"),
                       &first, second, report, "b"), diagnostic.c_str());
  AE_EXPECT_EQ(report.byAuthoredId, 1u, "o id do autor reconhece a tábua renomeada");
  AE_EXPECT_TRUE(second.nodes[1].id == first.nodes[1].id, "a tábua mantém a identidade");
  AE_EXPECT_TRUE(second.nodes[2].id == first.nodes[2].id, "o suporte com pai novo mantém a identidade");
  AE_EXPECT_TRUE(second.nodes[2].parent == first.nodes[1].id, "e o pai novo está registrado");
  AE_EXPECT_EQ(report.added + report.removed, 0u, "nada nasceu nem sumiu");
  AE_EXPECT_EQ(second.revision, 2u, "revisão avança com conteúdo novo");

  // Renomeação sem id: mesma geometria e pose, únicas entre irmãos.
  resources::ImportNodeMap third;
  AE_EXPECT_TRUE(build(glb(R"({"name":"Estante","children":[1]},{"name":"Prancha","mesh":0,"extras":{"uuid":"tabua-1"},"children":[2]},{"name":"Apoio","mesh":1,"translation":[0,1,0]})", "0"),
                       &second, third, report, "c"), diagnostic.c_str());
  AE_EXPECT_EQ(report.renamed, 1u, "renomeação reconhecida por evidência estrutural");
  AE_EXPECT_TRUE(third.nodes[2].id == first.nodes[2].id, "identidade preservada em três revisões");
}

AE_TEST(m082_ambiguous_repeated_names_are_never_matched_silently) {
  Project project;
  EditorSession session;
  FakeRenderer renderer;
  start(session, renderer);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(commit(session, project, vehicleV1(), report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), report.diagnostic.c_str());
  const auto root = children(session.document(), session.document().root()).front();
  const auto left = childNamed(session.document(), root, "Roda", 0);
  const auto leftNode = linkOf(session.document(), left)->node;
  std::vector<u8> before;
  EditorImportTransaction::read(project.root / kSource, before);

  AE_EXPECT_TRUE(!commit(session, project, vehicleAmbiguous(), report), "ambiguidade recusada sem escolha");
  AE_EXPECT_EQ(report.match.ambiguities.size(), 1u, "um grupo ambíguo relatado");
  AE_EXPECT_EQ(report.match.ambiguities.front().name, std::string("Roda"), "com o nome do grupo");
  std::vector<u8> after;
  EditorImportTransaction::read(project.root / kSource, after);
  AE_EXPECT_TRUE(after == before, "a fonte do projeto não mudou");
  AE_EXPECT_TRUE(linkOf(session.document(), left)->revision == 1u, "a cena não foi reconciliada");

  // A prévia mostra a mesma ambiguidade e trava a publicação até a escolha.
  resources::GltfImport model;
  resources::importGlb(vehicleAmbiguous(), {}, {}, model);
  session.showImportPreview(kSource, model, Sha256::hex(vehicleAmbiguous()));
  AE_EXPECT_EQ(session.importAmbiguityPolicy() == resources::ImportAmbiguityPolicy::Refuse, true, "sem escolha, recusa");

  AE_EXPECT_TRUE(commit(session, project, vehicleAmbiguous(), report, resources::ImportAmbiguityPolicy::MatchInOrder),
                 report.diagnostic.c_str());
  AE_EXPECT_TRUE(linkOf(session.document(), left)->node == leftNode, "pela ordem, por escolha explícita");
  AE_EXPECT_EQ(session.document().find(left)->transform.position[0], 2.f, "e a roda recebeu a pose nova");
  AE_EXPECT_EQ(children(session.document(), root).size(), 3u, "sem duplicar");
}

AE_TEST(r3_import_profile_scales_roots_persists_per_source_and_changes_the_cache_key) {
  // Arquivo do perfil: escreve, lê e recusa o que não conhece.
  const auto profile = profileOf(10.0f, 512);
  resources::ImportProfile parsed;
  AE_EXPECT_TRUE(resources::parseImportProfile(resources::serializeImportProfile(profile), parsed), "ida e volta");
  AE_EXPECT_TRUE(resources::sameImportProfile(parsed, profile), "mesmo perfil");
  AE_EXPECT_TRUE(!resources::parseImportProfile(R"({"schema":3,"scale":3,"maximumTextureDimension":512,"normals":0,"normalWeighting":0,"tangents":0,"importCameras":false})", parsed), "escala fora dos passos");
  AE_EXPECT_TRUE(!resources::parseImportProfile(R"({"schema":9,"scale":1,"maximumTextureDimension":512,"normals":0,"normalWeighting":0,"tangents":0,"importCameras":false})", parsed), "schema desconhecido");
  AE_EXPECT_TRUE(!resources::parseImportProfile(R"({"schema":3,"scale":1,"maximumTextureDimension":300,"normals":0,"normalWeighting":0,"tangents":0,"importCameras":false})", parsed), "textura fora dos passos");
  // Campo do schema atual ausente é recusa, não padrão silencioso.
  AE_EXPECT_TRUE(!resources::parseImportProfile(R"({"schema":3,"scale":1,"maximumTextureDimension":512})", parsed), "perfil truncado");
  // Modo desconhecido de normal ou tangente também falha fechado.
  AE_EXPECT_TRUE(!resources::parseImportProfile(R"({"schema":3,"scale":1,"maximumTextureDimension":512,"normals":7,"normalWeighting":0,"tangents":0,"importCameras":false})", parsed), "modo de normal inválido");
  AE_EXPECT_TRUE(!resources::parseImportProfile("{", parsed), "JSON quebrado");

  // O perfil nunca sobe acima do teto do aparelho.
  resources::GltfImportLimits device;
  device.maximumTextureDimension = 1024;
  AE_EXPECT_EQ(resources::applyImportProfile(device, profileOf(1.0f, 2048)).maximumTextureDimension, 1024u, "teto do aparelho vence");

  // Escala só nas raízes; os filhos herdam pela hierarquia.
  resources::GltfImport plain, scaled;
  AE_EXPECT_TRUE(resources::importGlb(vehicleV2(), {}, {}, plain), plain.diagnostic.c_str());
  AE_EXPECT_TRUE(resources::importGlb(vehicleV2(), resources::applyImportProfile({}, profile), {}, scaled), scaled.diagnostic.c_str());
  AE_EXPECT_EQ(scaled.nodes[0].localMatrix[0], 10.f * plain.nodes[0].localMatrix[0], "raiz escalada");
  AE_EXPECT_EQ(scaled.nodes[1].localMatrix[13], plain.nodes[1].localMatrix[13], "filho intacto");
  // Geometria derivada também muda a saída do importador para os mesmos bytes,
  // então precisa entrar na chave: um perfil novo não pode reusar o derivado do
  // perfil antigo.
  resources::ImportProfile recalculated = profile;
  recalculated.normals = resources::GltfNormalsCalculate;
  resources::ImportProfile withCameras = profile;
  withCameras.importCameras = true;
  AE_EXPECT_TRUE(resources::importCacheKey("abc", resources::applyImportProfile({}, profile)) !=
                     resources::importCacheKey("abc", resources::applyImportProfile({}, withCameras)),
                 "importar cameras muda a saida e por isso muda a chave");
  AE_EXPECT_TRUE(resources::importCacheKey("abc", resources::applyImportProfile({}, profile)) !=
                     resources::importCacheKey("abc", resources::applyImportProfile({}, recalculated)),
                 "normais recalculadas mudam a chave do cache");
  AE_EXPECT_TRUE(resources::importCacheKey("abc", {}) != resources::importCacheKey("abc", resources::applyImportProfile({}, profile)),
                 "perfil diferente, derivado diferente");

  // Resolução: fonte -> padrão do projeto -> embutido.
  Project project;
  EditorSession session;
  FakeRenderer renderer;
  start(session, renderer);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  // Fonte nova sem padrão do projeto: embutido + o preset de importação nova (ASTC 6x6, S1).
  resources::ImportProfile preset;
  preset.textureCompression = 6;
  AE_EXPECT_TRUE(resources::sameImportProfile(session.importProfileForPath(kSource), preset), "sem arquivos: preset de fonte nova");
  AE_EXPECT_TRUE(session.saveProjectImportProfile(profileOf(0.01f, 1024)), "padrão do projeto gravado");
  AE_EXPECT_EQ(session.importProfileForPath(kSource).maximumTextureDimension, 1024u, "fonte nova usa o padrão");
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(commit(session, project, vehicleV1(), report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.saveImportProfile(report.source, profile), "perfil da fonte gravado");
  AE_EXPECT_TRUE(resources::sameImportProfile(session.importProfileForPath(kSource), profile), "fonte registrada usa o próprio perfil");
  {
    std::ofstream corrupt(project.root / resources::importProfilePath(report.source), std::ios::binary | std::ios::trunc);
    corrupt << "{\"schema\":1";
  }
  AE_EXPECT_EQ(session.importProfileForPath(kSource).maximumTextureDimension, 1024u, "perfil ilegível cai no padrão do projeto");

  // O painel abre com o perfil resolvido e recebe saídas estruturadas.
  session.beginImportPreparation(kSource);
  AE_EXPECT_EQ(session.screen().importTextureDimension, 1024u, "painel abre com o perfil resolvido");
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(vehicleV2(), {}, {}, model), model.diagnostic.c_str());
  session.showImportPreview(kSource, model, Sha256::hex(vehicleV2()), {});
  AE_EXPECT_EQ(session.screen().importNodes.size(), model.nodes.size(), "uma linha por nó");
  AE_EXPECT_EQ(session.screen().importNodes[1].depth, 1u, "profundidade do filho");
  AE_EXPECT_TRUE(session.screen().importHasExtent, "tamanho aproximado calculado");
}

AE_TEST(m082_reimport_preserves_local_edits_across_two_instances_and_reopen) {
  Project project;
  EditorSession session;
  FakeRenderer renderer;
  start(session, renderer);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(commit(session, project, vehicleV1(), report), report.diagnostic.c_str());
  const auto source = report.source;
  AE_EXPECT_TRUE(session.instantiateModel(source, report), "instância A");
  AE_EXPECT_TRUE(session.instantiateModel(source, report), "instância B");
  const auto roots = children(session.document(), session.document().root());
  AE_EXPECT_EQ(roots.size(), 2u, "duas instâncias");
  const auto a = roots[0], b = roots[1];
  AE_EXPECT_TRUE(linkOf(session.document(), a)->instance != linkOf(session.document(), b)->instance, "instâncias independentes");

  // Em A: a segunda roda deslocada e um script na carroceria.
  const auto aWheel = childNamed(session.document(), a, "Roda", 1);
  const auto aBody = childNamed(session.document(), a, "Carroceria");
  move(session, aWheel, 0, 5);
  attachScript(session, aBody, "Jogo.Porta");
  AE_EXPECT_TRUE(session.importLinkOverrides(aWheel) == ImportOverridePosition, "a roda deslocada aparece como alteração local");
  AE_EXPECT_EQ(session.importLinkOverrides(aBody), 0u, "script não é alteração de campo da fonte");
  const auto sceneBefore = serializeEditorDocument(session.document(), 0);

  AE_EXPECT_TRUE(commit(session, project, vehicleV2(), report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(report.reimported, "reimportação");
  AE_EXPECT_EQ(report.reconcile.created, 2u, "a porta nova entrou nas duas instâncias");
  AE_EXPECT_EQ(report.reconcile.conflicts, 0u, "sem conflito");
  auto &document = session.document();
  AE_EXPECT_EQ(document.find(aWheel)->transform.position[0], 5.f, "o ajuste local da roda sobreviveu");
  AE_EXPECT_EQ(document.find(childNamed(document, a, "Roda", 0))->transform.position[0], 1.f, "a roda homônima não trocou de lugar");
  AE_EXPECT_EQ(document.find(aBody)->transform.position[1], .5f, "a carroceria recebeu a pose nova da fonte");
  AE_EXPECT_TRUE(scene::scriptBehavior(document.find(aBody)->components.find(scene::ScriptBehavior::descriptor)) != nullptr,
                 "e manteve o script");
  const auto bBody = childNamed(document, b, "Carroceria");
  AE_EXPECT_EQ(document.find(bBody)->transform.position[1], .5f, "B também atualizou");
  AE_EXPECT_EQ(document.find(childNamed(document, b, "Roda", 1))->transform.position[0], -1.f, "B não herdou o ajuste de A");
  AE_EXPECT_EQ(children(document, a).size(), 4u, "A: carroceria, duas rodas e porta, sem duplicar");
  AE_EXPECT_EQ(children(document, b).size(), 4u, "B igual");
  AE_EXPECT_TRUE(childNamed(document, a, "Porta") != kInvalidEntity, "porta presente em A");
  const auto *doorMesh = meshRenderer(*document.find(childNamed(document, a, "Porta")));
  AE_EXPECT_TRUE(doorMesh && doorMesh->mesh > 0, "a porta nova tem malha resolvida");
  AE_EXPECT_TRUE(std::filesystem::exists(project.root / resources::importNodeMapPath(source)), "mapa gravado na transação");
  AE_EXPECT_EQ(session.importNodeMap(source)->revision, 2u, "mapa na revisão 2");
  std::vector<renderer::MapDrawState> states;
  AE_EXPECT_TRUE(session.extractMap(states), "cena reconciliada extrai");

  // Salvar e reabrir: vínculos, ajustes e mapa voltam iguais.
  const auto scenePath = project.root / "cena.aescene";
  AE_EXPECT_TRUE(session.save(scenePath.string().c_str(), 0), "salvo");
  const auto registry = session.serializeAssets();
  EditorSession reopened;
  FakeRenderer second;
  start(reopened, second);
  AE_EXPECT_TRUE(reopened.setProjectDirectory(project.root.string().c_str()), "projeto reaberto");
  AE_EXPECT_TRUE(reopened.loadAssets(registry), "registro");
  std::vector<u8> bytes;
  EditorImportTransaction::read(project.root / kSource, bytes);
  EditorSession::ModelImportReport rehydrate;
  AE_EXPECT_TRUE(reopened.importModel(bytes, kSource, {}, rehydrate), rehydrate.diagnostic.c_str());
  AE_EXPECT_TRUE(rehydrate.match.sameContent, "mesma fonte, mesmo mapa");
  AE_EXPECT_TRUE(reopened.load(scenePath.string().c_str(), 0), "cena reaberta");
  AE_EXPECT_EQ(linkedCount(reopened.document()), 10u, "dois grupos de cinco objetos, todos vinculados");
  AE_EXPECT_EQ(serializeEditorDocument(reopened.document(), 0), serializeEditorDocument(session.document(), 0),
               "a reabertura não reconciliou nada de novo");

  // Cena salva ANTES da reimportação, aberta depois: a base no vínculo
  // permite reconciliar na abertura.
  const auto oldScene = project.root / "antiga.aescene";
  EditorImportTransaction::writeText(oldScene, sceneBefore);
  EditorSession late;
  FakeRenderer third;
  start(late, third);
  AE_EXPECT_TRUE(late.setProjectDirectory(project.root.string().c_str()), "projeto");
  AE_EXPECT_TRUE(late.loadAssets(registry), "registro");
  AE_EXPECT_TRUE(late.importModel(bytes, kSource, {}, rehydrate), rehydrate.diagnostic.c_str());
  AE_EXPECT_TRUE(late.load(oldScene.string().c_str(), 0), "cena antiga aberta");
  const auto lateRoot = children(late.document(), late.document().root()).front();
  AE_EXPECT_EQ(children(late.document(), lateRoot).size(), 4u, "a porta entrou na abertura");
  AE_EXPECT_EQ(late.document().find(childNamed(late.document(), lateRoot, "Roda", 1))->transform.position[0], 5.f,
               "o ajuste local continua");
  AE_EXPECT_EQ(late.document().find(childNamed(late.document(), lateRoot, "Carroceria"))->transform.position[1], .5f,
               "e a pose nova chegou");
}

AE_TEST(m082_removed_nodes_keep_local_data_as_orphans_and_deleted_parts_stay_deleted) {
  Project project;
  EditorSession session;
  FakeRenderer renderer;
  start(session, renderer);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(commit(session, project, vehicleV1(), report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), "instância");
  auto &document = session.document();
  const auto root = children(document, document.root()).front();
  const auto scripted = childNamed(document, root, "Roda", 1);
  attachScript(session, scripted, "Jogo.Roda");

  AE_EXPECT_TRUE(commit(session, project, vehicleWithoutWheels(), report), report.diagnostic.c_str());
  AE_EXPECT_EQ(report.reconcile.removed, 1u, "a roda intocada saiu");
  AE_EXPECT_EQ(report.reconcile.orphaned, 1u, "a roda com script ficou");
  const auto *orphan = linkOf(document, scripted);
  AE_EXPECT_TRUE(orphan && orphan->orphan, "marcada como órfã");
  AE_EXPECT_EQ(session.importLinkOverrides(scripted), 0u, "órfão não finge ter base");
  AE_EXPECT_TRUE(session.resolveImportOrphan(scripted, true), "manter como independente");
  AE_EXPECT_TRUE(linkOf(document, scripted)->unlinked, "independente, com os dados");
  AE_EXPECT_TRUE(document.find(scripted)->components.find(scene::ScriptBehavior::descriptor) != nullptr, "script preservado");

  // Apagar localmente a carroceria e reimportar com ela na fonte: continua
  // apagada, porque a instância já a conhecia.
  EditorSession other;
  FakeRenderer otherRenderer;
  start(other, otherRenderer);
  Project second;
  AE_EXPECT_TRUE(other.setProjectDirectory(second.root.string().c_str()), "projeto 2");
  AE_EXPECT_TRUE(commit(other, second, vehicleV1(), report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(other.instantiateModel(report.source, report), "instância");
  const auto otherRoot = children(other.document(), other.document().root()).front();
  AE_EXPECT_TRUE(other.history().destroyEntity(other.document(), childNamed(other.document(), otherRoot, "Carroceria")), "apagada");
  AE_EXPECT_TRUE(commit(other, second, vehicleV2(), report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(childNamed(other.document(), otherRoot, "Carroceria") == kInvalidEntity, "não ressuscitou");
  AE_EXPECT_TRUE(childNamed(other.document(), otherRoot, "Porta") != kInvalidEntity, "mas a porta nova entrou");
  AE_EXPECT_TRUE(report.reconcile.keptDeleted >= 1u, "remoção local contada");
  AE_EXPECT_TRUE(other.history().undo(other.document()), "a reimportação é um passo de desfazer");
  AE_EXPECT_TRUE(childNamed(other.document(), otherRoot, "Porta") == kInvalidEntity, "desfeita, a porta sai");
}

AE_TEST(m082_revert_unlink_duplicate_and_legacy_adoption) {
  Project project;
  EditorSession session;
  FakeRenderer renderer;
  start(session, renderer);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(commit(session, project, vehicleV1(), report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), "instância");
  auto &document = session.document();
  const auto root = children(document, document.root()).front();
  const auto wheel = childNamed(document, root, "Roda");
  move(session, wheel, 1, 3);
  AE_EXPECT_TRUE(session.revertImportLink(wheel, ImportOverrideAll), "reverter");
  AE_EXPECT_EQ(document.find(wheel)->transform.position[1], 0.f, "voltou à base");
  AE_EXPECT_EQ(session.importLinkOverrides(wheel), 0u, "sem alterações");
  AE_EXPECT_TRUE(session.history().undo(document), "reverter também se desfaz");
  AE_EXPECT_EQ(document.find(wheel)->transform.position[1], 3.f, "ajuste de volta");

  // Duplicar a instância inteira cria outra instância; duplicar uma peça cria
  // um objeto independente.
  const auto copy = session.history().duplicateEntity(document, root);
  AE_EXPECT_TRUE(copy != kInvalidEntity, "cópia");
  AE_EXPECT_TRUE(linkOf(document, copy)->instance != linkOf(document, root)->instance, "cópia é outra instância");
  const auto copyWheel = childNamed(document, copy, "Roda");
  AE_EXPECT_TRUE(linkOf(document, copyWheel)->instance == linkOf(document, copy)->instance, "com os filhos na instância nova");
  const auto part = session.history().duplicateEntity(document, wheel);
  AE_EXPECT_TRUE(linkOf(document, part)->unlinked, "peça duplicada é independente");
  AE_EXPECT_EQ(session.importLinkOverrides(part), 0u, "e não aparece como vinculada");

  AE_EXPECT_EQ(session.unlinkImport(copy), 4u, "desvincular a cópia inteira");
  AE_EXPECT_TRUE(linkOf(document, copyWheel)->unlinked, "filhos incluídos");
  AE_EXPECT_TRUE(!linkOf(document, wheel)->unlinked, "a original continua vinculada");

  // Cena anterior ao vínculo: tira os vínculos da instância original e
  // reimporta. A prova (identidade da malha + nomes dos pais) religa.
  std::vector<EditorEntityId> subtree;
  document.collectSubtree(root, subtree);
  for (const auto id : subtree) {
    // A peça duplicada fica com a marca de independente: uma peça solta SEM
    // marca, com a mesma malha da roda, tornaria a instância ambígua — e aí a
    // adoção corretamente não religa nada.
    if (id == part) continue;
    auto values = *document.find(id);
    values.components.remove(scene::ImportLink::descriptor);
    document.applyEntityValues(id, values);
  }
  AE_EXPECT_TRUE(commit(session, project, vehicleV2(), report), report.diagnostic.c_str());
  AE_EXPECT_EQ(report.reconcile.adopted, 4u, "grupo, carroceria e as duas rodas religados");
  AE_EXPECT_TRUE(childNamed(document, root, "Porta") != kInvalidEntity, "e a instância antiga recebeu a porta");
  AE_EXPECT_EQ(document.find(wheel)->transform.position[1], 3.f, "o ajuste anterior virou alteração local preservada");
  AE_EXPECT_TRUE(childNamed(document, copy, "Porta") == kInvalidEntity, "a cópia desvinculada não recebe nada");
}

// Fontes sintéticas da conferência no aparelho: `--write-m082-fixtures pasta`.
// Nada de modelo pessoal: o mesmo "Veiculo" genérico dos testes.
std::vector<u8> panelThree();
std::vector<u8> panelTwo();
int writeM082Fixtures(const char *directory) {
  const auto root = EditorImportTransaction::fromUtf8(directory);
  std::error_code error;
  std::filesystem::create_directories(root, error);
  const bool ok = !error && EditorImportTransaction::write(root / "m082-v1.glb", vehicleV1()) &&
                  EditorImportTransaction::write(root / "m082-v2.glb", vehicleV2()) &&
                  EditorImportTransaction::write(root / "m082-ambiguo.glb", vehicleAmbiguous()) &&
                  EditorImportTransaction::write(root / "m082-sem-rodas.glb", vehicleWithoutWheels()) &&
                  EditorImportTransaction::write(root / "m08e2-painel-tres-materiais.glb", panelThree()) &&
                  EditorImportTransaction::write(root / "m08e2-painel-duas-primitivas.glb", panelTwo());
  std::printf("%s\n", ok ? "fixtures gravadas" : "falha ao gravar fixtures");
  return ok ? 0 : 1;
}

// Probe com arquivo real, fora da suíte: `aether_tests --reimport-glb caminho`.
// Publica pela transação, cria duas instâncias, desloca uma peça de uma delas,
// republica o MESMO conteúdo (nada pode mudar), força a correspondência geral
// como se fosse outra revisão (mede ambiguidade e tempo) e reabre o projeto.
int probeReimportGlb(const char *path) {
  using Clock = std::chrono::steady_clock;
  const auto ms = [](Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
  };
  std::vector<u8> bytes;
  if (!EditorImportTransaction::read(EditorImportTransaction::fromUtf8(path), bytes)) { std::fprintf(stderr, "READ %s\n", path); return 2; }
  resources::GltfImport model;
  const auto parseStart = Clock::now();
  // AETHER_PROBE_ASTC=1 mede como num aparelho que amostra ASTC 4x4.
  resources::GltfImportLimits limits;
  limits.astc4x4 = std::getenv("AETHER_PROBE_ASTC") != nullptr;
  if (!resources::importGlb(bytes, limits, {}, model)) { std::fprintf(stderr, "PARSE: %s\n", model.diagnostic.c_str()); return 1; }
  const auto parseEnd = Clock::now();
  std::printf("%s: parse_ms=%.1f textures=%zu texture_mb=%.1f reduced=%u generated_tangents=%u draco=%u meshopt_views=%u ktx2=%u mirrored=%u baked_transforms=%u astc=%u skipped_textures=%u unapplied_transforms=%u unapplied_occlusion=%u\n",
              path, ms(parseStart, parseEnd), model.textures.size(), static_cast<double>(model.textureBytes) / 1048576.0,
              model.reducedTextures, model.generatedTangentPrimitives, model.dracoPrimitives, model.meshoptViews,
              model.ktx2Images, model.mirroredNodes, model.bakedTextureTransforms, model.astcTextures, model.skippedTextures,model.unappliedTextureTransforms, model.unappliedOcclusion);
  for (const auto &note : model.textureNotes) std::printf("  nota: %s\n", note.c_str());
  Project project;
  EditorSession session;
  FakeRenderer renderer;
  start(session, renderer);
  if (!session.setProjectDirectory(project.root.string().c_str())) return 1;
  EditorSession::ModelImportReport report;
  const auto t0 = Clock::now();
  if (!session.commitModelImport(bytes, model, kSource, "", report)) { std::fprintf(stderr, "COMMIT: %s\n", report.diagnostic.c_str()); return 1; }
  const auto t1 = Clock::now();
  const auto source = report.source;
  if (!session.instantiateModel(source, report) || !session.instantiateModel(source, report)) { std::fprintf(stderr, "INSTANCE: %s\n", report.diagnostic.c_str()); return 1; }
  auto &document = session.document();
  const auto roots = children(document, document.root());
  std::vector<EditorEntityId> subtree;
  document.collectSubtree(roots.front(), subtree);
  EditorEntityId moved = kInvalidEntity;
  for (const auto id : subtree) if (id != roots.front() && meshRenderer(*document.find(id))) { moved = id; break; }
  if (moved) move(session, moved, 1, document.find(moved)->transform.position[1] + 0.25f);
  const auto linked = linkedCount(document);

  EditorSession::ModelImportReport again;
  const auto t2 = Clock::now();
  if (!session.commitModelImport(bytes, model, kSource, Sha256::hex(bytes), again)) { std::fprintf(stderr, "REIMPORT: %s\n", again.diagnostic.c_str()); return 1; }
  const auto t3 = Clock::now();

  resources::ImportNodeMap forced;
  resources::ImportMatchReport match;
  std::string diagnostic;
  const auto t4 = Clock::now();
  resources::buildImportNodeMap(model, source, "outra-revisao", session.importNodeMap(source), resources::ImportAmbiguityPolicy::Refuse,
                                forced, match, diagnostic);
  const auto t5 = Clock::now();

  const auto scenePath = project.root / "cena.aescene";
  const auto registry = session.serializeAssets();
  bool reopenedOk = session.save(scenePath.string().c_str(), 0);
  EditorSession reopened;
  FakeRenderer second;
  start(reopened, second);
  EditorSession::ModelImportReport rehydrate;
  reopenedOk = reopenedOk && reopened.setProjectDirectory(project.root.string().c_str()) && reopened.loadAssets(registry) &&
               reopened.importModel(bytes, kSource, {}, rehydrate) && reopened.load(scenePath.string().c_str(), 0);
  const bool identical = reopenedOk && serializeEditorDocument(reopened.document(), 0) == serializeEditorDocument(session.document(), 0);
  std::printf("%s: nodes=%zu draws=%zu map_revision=%u linked=%u moved_override=%u publish_ms=%.1f "
              "same_content=%d reimport_changed=%d reimport_ms=%.1f forced_match: authored=%u structure=%u renamed=%u "
              "reparented=%u ambiguities=%zu match_ms=%.1f reopen_identical=%d reopen_linked=%u\n",
              path, model.nodes.size(), model.draws.size(), session.importNodeMap(source)->revision, linked,
              moved ? session.importLinkOverrides(moved) : 0u, ms(t0, t1), again.match.sameContent ? 1 : 0,
              again.reconcile.changed() ? 1 : 0, ms(t2, t3), match.byAuthoredId, match.byStructure, match.renamed,
              match.reparented, match.ambiguities.size(), ms(t4, t5), identical ? 1 : 0, linkedCount(reopened.document()));
  return identical && again.match.sameContent && !again.reconcile.changed() ? 0 : 1;
}

AE_TEST(m082_interrupted_import_restores_the_node_map_with_source_and_registry) {
  Project project;
  std::filesystem::create_directories(project.root / "Fontes");
  std::filesystem::create_directories(project.root / ".astra/imports");
  const std::string mapPath = ".astra/imports/mapa.nodes";
  EditorImportTransaction::writeText(project.root / kSource, "fonte antiga");
  EditorImportTransaction::writeText(project.root / ".astra/assets.astra", "registro antigo");
  EditorImportTransaction::writeText(project.root / mapPath, "mapa antigo");
  std::vector<u8> bytes;
  EditorImportTransaction::read(project.root / kSource, bytes);
  std::string diagnostic;
  {
    EditorImportTransaction transaction(project.root.string());
    AE_EXPECT_TRUE(transaction.begin(kSource, Sha256::hex(bytes), diagnostic, mapPath), diagnostic.c_str());
    EditorImportTransaction::writeText(project.root / mapPath, "mapa pela metade");
    EditorImportTransaction::writeText(project.root / kSource, "fonte pela metade");
  }
  AE_EXPECT_TRUE(EditorImportTransaction::recover(project.root.string(), diagnostic), diagnostic.c_str());
  EditorImportTransaction::read(project.root / mapPath, bytes);
  AE_EXPECT_EQ(std::string(bytes.begin(), bytes.end()), std::string("mapa antigo"), "mapa restaurado");
  EditorImportTransaction::read(project.root / kSource, bytes);
  AE_EXPECT_EQ(std::string(bytes.begin(), bytes.end()), std::string("fonte antiga"), "fonte restaurada");

  // Journal do formato anterior continua recuperável.
  EditorImportTransaction::writeText(project.root / ".astra/import-transaction/journal",
                                     "ASTRA_IMPORT_1 committed \"Fontes/veiculo.glb\" 1 1\n");
  AE_EXPECT_TRUE(EditorImportTransaction::recover(project.root.string(), diagnostic), "journal v1 aceito");
  AE_EXPECT_TRUE(!std::filesystem::exists(project.root / ".astra/import-transaction/journal"), "e assentado");
}

// ---------------------------------------------------------------------------
// Entrega 2 — submeshes: um nó com várias primitivas é UM objeto com slots.

// Um painel cujo nó carrega três primitivas com três materiais (a malha "Trio").
std::vector<u8> panelThree() { return glb(R"({"name":"Painel","mesh":3})", "0"); }
// Revisão em que o painel passa a ter duas primitivas (a malha "Duplo").
std::vector<u8> panelTwo() { return glb(R"({"name":"Painel","mesh":2})", "0"); }

namespace {
std::vector<u8> projectSource(const Project &project) {
  std::vector<u8> bytes;
  EditorImportTransaction::read(project.root / kSource, bytes);
  return bytes;
}
} // namespace

AE_TEST(m08e2_node_with_three_primitives_is_one_object_with_three_slots) {
  Project project;
  EditorSession session;
  FakeRenderer renderer;
  start(session, renderer);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(commit(session, project, panelThree(), report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), report.diagnostic.c_str());
  auto &document = session.document();
  const auto roots = children(document, document.root());
  AE_EXPECT_EQ(roots.size(), 1u, "um objeto para o nó");
  const auto panel = roots.front();
  AE_EXPECT_EQ(children(document, panel).size(), 0u, "nenhum filho inventado por primitiva");
  AE_EXPECT_EQ(report.objects, 1u, "o relatório conta um objeto");
  const auto *render = meshRenderer(*document.find(panel));
  AE_EXPECT_TRUE(render && render->slotCount() == 3u, "três slots");
  for (u32 slot = 0; render && slot < render->slotCount(); ++slot) {
    AE_EXPECT_TRUE(render->slotMesh(slot) > 0, "slot resolvido no pacote");
    for (u32 other = 0; other < slot; ++other)
      AE_EXPECT_TRUE(render->slotAsset(slot) != render->slotAsset(other), "identidade própria por slot");
  }

  std::vector<renderer::MapDrawState> states;
  AE_EXPECT_TRUE(session.extractMap(states), "extração");
  u32 draws = 0;
  for (const auto &state : states) if (state.objectId == panel && state.visible) ++draws;
  AE_EXPECT_EQ(draws, 3u, "três desenhos, todos do mesmo objeto");
  u32 candidates = 0;
  for (const auto &candidate : session.pickCandidates()) if (candidate.id == panel && candidate.geometry()) ++candidates;
  AE_EXPECT_EQ(candidates, 3u, "cada primitiva é tocável e seleciona o mesmo objeto");

  const auto *link = linkOf(document, panel);
  AE_EXPECT_TRUE(link && link->baseSlots().size() == 3u, "a base do vínculo conhece os três slots");
  AE_EXPECT_EQ(session.importLinkOverrides(panel), 0u, "igual à fonte");

  EditorDocument loaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(document, 0), 0, loaded), "cena relida");
  const auto *again = meshRenderer(*loaded.find(panel));
  AE_EXPECT_TRUE(again && again->slotCount() == 3u, "slots voltam do arquivo");
  for (u32 slot = 0; again && render && slot < 3; ++slot)
    AE_EXPECT_TRUE(again->slotAsset(slot) == render->slotAsset(slot), "com as mesmas identidades");
  const auto *againLink = scene::importLink(loaded.find(panel)->components);
  AE_EXPECT_TRUE(againLink && againLink->baseSlots().size() == 3u, "e a base dos slots também");
}

AE_TEST(m08e2_mesh_renderer_records_from_before_slots_still_load) {
  // Registro v2 (sem slots): malha 5, sem substituição, onze números, sem id.
  scene::MeshRenderer render;
  std::istringstream in("5 1 0 1 1 1 0.5 0 1 1 0 0 0 1 -");
  in.imbue(std::locale::classic());
  AE_EXPECT_TRUE(render.read(in, 2), "v2 lido");
  AE_EXPECT_EQ(render.slotCount(), 1u, "um slot");
  AE_EXPECT_EQ(render.mesh, 5u, "no lugar de sempre");
  scene::ImportLink link;
  std::istringstream old("aa000000000000000000000000000001 bb000000000000000000000000000002 cc000000000000000000000000000003 -1 1 0 0 \"Painel\" 0 0 0 0 0 0 1 1 1 - - 3 0");
  AE_EXPECT_TRUE(link.read(old, 1), "vínculo v1 lido");
  AE_EXPECT_TRUE(link.baseSubmeshes.empty(), "v1 não tinha slots na base");
}

AE_TEST(m08e2_legacy_parts_become_slots_only_when_nothing_is_lost) {
  const auto buildLegacyScene = [](bool scriptOnPart, std::string &scene, std::string &registry, Project &project) {
    EditorSession session;
    FakeRenderer renderer;
    start(session, renderer);
    session.setProjectDirectory(project.root.string().c_str());
    EditorSession::ModelImportReport report;
    if (!commit(session, project, panelThree(), report) || !session.instantiateModel(report.source, report)) return false;
    auto &document = session.document();
    const auto panel = children(document, document.root()).front();
    const auto &node = session.importNodeMap(report.source)->nodes.front();
    const auto revision = session.importNodeMap(report.source)->revision;
    // A representação anterior: o objeto do nó sem malha e uma parte por
    // primitiva, cada uma com vínculo `primitive`.
    auto values = *document.find(panel);
    const auto instance = linkOf(document, panel)->instance;
    values.components.remove(scene::MeshRenderer::descriptor);
    setImportLinkBase(*scene::editImportLink(values.components), node, values.transform, -1, revision, false);
    scene::editImportLink(values.components)->root = true;
    document.applyEntityValues(panel, values);
    for (u32 p = 0; p < 3; ++p) {
      const auto part = document.createEntity(panel, EditorEntityKind::Mesh, "Painel · " + std::to_string(p + 1));
      auto partValues = *document.find(part);
      auto *render = editMeshRenderer(partValues);
      render->asset = node.draws[p];
      if (p == 1) { render->material.enabled = true; render->material.baseColor[0] = .25f; }
      auto *link = scene::editImportLink(partValues.components);
      link->source = report.source;
      link->instance = instance;
      setImportLinkBase(*link, node, EditorTransform{}, static_cast<i32>(p), revision);
      link->baseName = partValues.name;
      if (scriptOnPart && p == 2) {
        auto *script = static_cast<scene::ScriptBehavior *>(partValues.components.add(scene::ScriptBehavior::descriptor));
        script->scriptType = "Jogo.Luz";
      }
      document.applyEntityValues(part, partValues);
    }
    scene = serializeEditorDocument(document, 0);
    registry = session.serializeAssets();
    return true;
  };
  const auto reopen = [](Project &project, const std::string &scene, const std::string &registry, EditorSession &session,
                         FakeRenderer &renderer) {
    start(session, renderer);
    session.setProjectDirectory(project.root.string().c_str());
    session.loadAssets(registry);
    EditorSession::ModelImportReport report;
    const auto bytes = projectSource(project);
    const auto path = project.root / "legado.aescene";
    EditorImportTransaction::writeText(path, scene);
    return session.importModel(bytes, kSource, {}, report) && session.load(path.string().c_str(), 0);
  };

  {
    Project project;
    std::string scene, registry;
    AE_EXPECT_TRUE(buildLegacyScene(false, scene, registry, project), "cena legada montada");
    EditorSession session;
    FakeRenderer renderer;
    AE_EXPECT_TRUE(reopen(project, scene, registry, session, renderer), "cena legada aberta");
    auto &document = session.document();
    const auto panel = children(document, document.root()).front();
    AE_EXPECT_EQ(children(document, panel).size(), 0u, "as partes viraram slots");
    const auto *render = meshRenderer(*document.find(panel));
    AE_EXPECT_TRUE(render && render->slotCount() == 3u, "três slots no objeto do nó");
    AE_EXPECT_TRUE(render && render->slotMaterial(1).enabled && render->slotMaterial(1).baseColor[0] == .25f,
                   "o material local da segunda parte foi preservado no slot");
    // A base migrou junto: malha, pose e pai não aparecem como alteração. O
    // material local da segunda parte é alteração real e aparece (R4).
    AE_EXPECT_EQ(session.importLinkOverrides(panel), static_cast<u32>(ImportOverrideMaterial),
                 "a base migrou junto: só o material local aparece como alteração");
    std::vector<renderer::MapDrawState> states;
    AE_EXPECT_TRUE(session.extractMap(states), "extração depois da migração");
  }
  {
    Project project;
    std::string scene, registry;
    AE_EXPECT_TRUE(buildLegacyScene(true, scene, registry, project), "cena legada com script");
    EditorSession session;
    FakeRenderer renderer;
    AE_EXPECT_TRUE(reopen(project, scene, registry, session, renderer), "aberta");
    const auto panel = children(session.document(), session.document().root()).front();
    AE_EXPECT_EQ(children(session.document(), panel).size(), 3u, "com script numa parte, nada é consolidado");
    AE_EXPECT_TRUE(meshRenderer(*session.document().find(panel)) == nullptr, "e o objeto do nó continua sem malha");
  }
}

AE_TEST(m08e2_reimport_changes_slot_count_without_dropping_local_material) {
  Project project;
  EditorSession session;
  FakeRenderer renderer;
  start(session, renderer);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(commit(session, project, panelThree(), report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), "A");
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), "B");
  auto &document = session.document();
  const auto roots = children(document, document.root());
  const auto paint = [&](EditorEntityId id, u32 slot) {
    auto values = *document.find(id);
    auto *material = editMeshRenderer(values)->editSlotMaterial(slot);
    material->enabled = true;
    material->baseColor[1] = .1f;
    session.history().begin("Material");
    session.history().applyValues(document, id, values);
    session.history().end();
  };
  paint(roots[0], 0); // A: slot que continua existindo
  paint(roots[1], 2); // B: slot que a fonte nova não tem
  AE_EXPECT_TRUE(commit(session, project, panelTwo(), report), report.diagnostic.c_str());
  const auto *a = meshRenderer(*document.find(roots[0]));
  const auto *b = meshRenderer(*document.find(roots[1]));
  AE_EXPECT_TRUE(a && a->slotCount() == 2u, "A seguiu a fonte: dois slots");
  AE_EXPECT_TRUE(a && a->slotMaterial(0).enabled && a->slotMaterial(0).baseColor[1] == .1f, "com o material local do slot 0");
  AE_EXPECT_TRUE(b && b->slotCount() == 3u, "B manteve o slot com material local");
  AE_EXPECT_EQ(report.reconcile.conflicts, 1u, "e isso foi relatado como conflito");
  std::vector<renderer::MapDrawState> states;
  AE_EXPECT_TRUE(session.extractMap(states), "extração");
}

AE_TEST(m08e2_slot_material_scope_is_instance_or_shared_resource_and_persists) {
  Project project;
  EditorSession session;
  FakeRenderer renderer;
  start(session, renderer);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(commit(session, project, panelThree(), report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), "A");
  AE_EXPECT_TRUE(session.instantiateModel(report.source, report), "B");
  const auto roots = children(session.document(), session.document().root());
  const auto a = roots[0], b = roots[1];
  const auto drawn = [](EditorSession &from, EditorEntityId id, u32 slot) {
    std::vector<renderer::MapDrawState> states;
    from.extractMap(states);
    const auto *render = meshRenderer(*from.document().find(id));
    for (const auto &state : states)
      if (render && state.objectId == id && state.sourceDrawIndex + 1 == render->slotMesh(slot)) return state.material;
    return scene::MaterialParameters{};
  };
  AE_EXPECT_EQ(session.sourceMaterialName(meshRenderer(*session.document().find(a))->slotAsset(1)), std::string("Vidro"),
               "o slot mostra o nome do material da fonte");
  const auto rebuilds = renderer.rebuilds;
  std::string diagnostic;

  // 1. Alcance da instância: só o slot 2 de A muda.
  AE_EXPECT_TRUE(session.setSlotMaterialValue(a, 2, EditorSession::MaterialScope::Instance, 0, .9f, diagnostic), diagnostic.c_str());
  AE_EXPECT_TRUE(drawn(session, a, 2).enabled && drawn(session, a, 2).baseColor[0] == .9f, "A · slot 2 desenhado com a cor local");
  AE_EXPECT_TRUE(!drawn(session, b, 2).enabled, "B · slot 2 continua com o material da fonte");
  AE_EXPECT_TRUE(!drawn(session, a, 0).enabled && !drawn(session, a, 1).enabled, "os irmãos de A não mudaram");
  AE_EXPECT_TRUE(session.history().undo(session.document()), "a substituição local se desfaz");
  AE_EXPECT_TRUE(!drawn(session, a, 2).enabled, "desfeita");
  AE_EXPECT_TRUE(session.history().redo(session.document()), "e se refaz");

  // 2. Alcance compartilhado: um material do projeto usado por A e B.
  AE_EXPECT_TRUE(!session.setSlotMaterialValue(b, 1, EditorSession::MaterialScope::Shared, 1, .3f, diagnostic),
                 "material da fonte não é editado como compartilhado");
  const auto shared = session.createMaterialFromSlot(a, 1, diagnostic);
  AE_EXPECT_TRUE(shared.valid(), diagnostic.c_str());
  AE_EXPECT_TRUE(std::filesystem::exists(project.root / "Materiais/Vidro.material"), "arquivo do material no projeto");
  AE_EXPECT_TRUE(session.assignSlotMaterial(b, 1, shared), "B · slot 1 passa a usar o mesmo recurso");
  AE_EXPECT_TRUE(drawn(session, a, 1).baseColor[2] == 1 && drawn(session, a, 1).enabled, "o recurso nasceu com a cor da fonte");
  AE_EXPECT_TRUE(session.setSlotMaterialValue(b, 1, EditorSession::MaterialScope::Shared, 1, .3f, diagnostic), diagnostic.c_str());
  AE_EXPECT_EQ(drawn(session, a, 1).baseColor[1], .3f, "A recebeu a edição compartilhada feita a partir de B");
  AE_EXPECT_EQ(drawn(session, b, 1).baseColor[1], .3f, "e B também");
  AE_EXPECT_TRUE(drawn(session, a, 2).baseColor[0] == .9f && !drawn(session, b, 2).enabled, "slots 2 intocados");
  AE_EXPECT_EQ(session.findMaterialAsset(shared)->revision, 2u, "revisão do recurso avançou");
  AE_EXPECT_TRUE(session.takeAppearanceChanged(), "o shell é avisado para republicar");
  AE_EXPECT_EQ(renderer.rebuilds, rebuilds, "nenhuma operação de material republicou geometria");

  // 3. Salvar e reabrir: os dois alcances voltam.
  const auto scenePath = project.root / "cena.aescene";
  AE_EXPECT_TRUE(session.save(scenePath.string().c_str(), 0), "salvo");
  const auto registry = session.serializeAssets();
  EditorSession reopened;
  FakeRenderer second;
  start(reopened, second);
  AE_EXPECT_TRUE(reopened.setProjectDirectory(project.root.string().c_str()), "projeto");
  AE_EXPECT_TRUE(reopened.loadAssets(registry), "registro com o material");
  AE_EXPECT_TRUE(reopened.findMaterialAsset(shared) != nullptr, "material relido do arquivo");
  EditorSession::ModelImportReport rehydrate;
  AE_EXPECT_TRUE(reopened.importModel(projectSource(project), kSource, {}, rehydrate), rehydrate.diagnostic.c_str());
  AE_EXPECT_TRUE(reopened.load(scenePath.string().c_str(), 0), "cena reaberta");
  AE_EXPECT_EQ(drawn(reopened, a, 1).baseColor[1], .3f, "compartilhado depois de reabrir");
  AE_EXPECT_EQ(drawn(reopened, b, 1).baseColor[1], .3f, "nas duas instâncias");
  AE_EXPECT_EQ(drawn(reopened, a, 2).baseColor[0], .9f, "e a substituição local de A");
}

// R1 — reabertura do projeto em lote. O caminho antigo publicava a biblioteca a
// cada fonte (A, A+B, A+B+C); a reabertura agora publica uma vez e chega à mesma
// cena. Uma fonte recusada não impede as outras.
AE_TEST(r1_project_reopen_publishes_all_sources_once_and_matches_the_sequential_path) {
  Project project;
  EditorSession session;
  FakeRenderer renderer;
  start(session, renderer);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto");
  const std::vector<std::pair<std::string, std::vector<u8>>> files{
      {"Fontes/veiculo.glb", vehicleV1()}, {"Fontes/painel.glb", panelThree()}, {"Fontes/placa.glb", panelTwo()}};
  for (const auto &[path, bytes] : files) {
    resources::GltfImport model;
    AE_EXPECT_TRUE(resources::importGlb(bytes, {}, {}, model), model.diagnostic.c_str());
    EditorSession::ModelImportReport report;
    AE_EXPECT_TRUE(session.commitModelImport(bytes, model, path, std::string(), report), report.diagnostic.c_str());
    EditorSession::ModelImportReport instance;
    AE_EXPECT_TRUE(session.instantiateModel(report.source, instance), instance.diagnostic.c_str());
  }
  const auto scenePath = project.root / "cena.aescene";
  AE_EXPECT_TRUE(session.save(scenePath.string().c_str(), 0), "salvo");
  const auto registry = session.serializeAssets();

  EditorSession sequential;
  FakeRenderer one;
  start(sequential, one);
  AE_EXPECT_TRUE(sequential.setProjectDirectory(project.root.string().c_str()), "projeto sequencial");
  AE_EXPECT_TRUE(sequential.loadAssets(registry), "registro sequencial");
  for (const auto &[path, original] : files) {
    std::vector<u8> bytes;
    AE_EXPECT_TRUE(EditorImportTransaction::read(project.root / path, bytes), "fonte gravada");
    EditorSession::ModelImportReport report;
    AE_EXPECT_TRUE(sequential.importModel(bytes, path, {}, report), report.diagnostic.c_str());
  }
  AE_EXPECT_TRUE(sequential.load(scenePath.string().c_str(), 0), "cena sequencial");
  AE_EXPECT_EQ(one.rebuilds, 3u, "o caminho antigo publicava a cada fonte");

  EditorSession batch;
  FakeRenderer other;
  start(batch, other);
  AE_EXPECT_TRUE(batch.setProjectDirectory(project.root.string().c_str()), "projeto em lote");
  AE_EXPECT_TRUE(batch.loadAssets(registry), "registro em lote");
  std::vector<EditorSession::ReopenedSource> sources;
  for (const auto &[path, original] : files) {
    std::vector<u8> bytes;
    AE_EXPECT_TRUE(EditorImportTransaction::read(project.root / path, bytes), "fonte gravada");
    EditorSession::ReopenedSource reopened;
    AE_EXPECT_TRUE(resources::importGlb(bytes, {}, {}, reopened.model), reopened.model.diagnostic.c_str());
    reopened.hash = Sha256::hex(bytes);
    reopened.sourceName = path;
    sources.push_back(std::move(reopened));
  }
  EditorSession::ReopenedSource broken;
  broken.sourceName = "Fontes/quebrado.glb";
  sources.push_back(std::move(broken));
  std::vector<EditorSession::ModelImportReport> reports;
  std::string diagnostic;
  AE_EXPECT_TRUE(batch.reopenSources(sources, reports, diagnostic), diagnostic.c_str());
  AE_EXPECT_EQ(other.rebuilds, 1u, "uma publicação para as três fontes");
  AE_EXPECT_EQ(reports.size(), usize{4}, "um relatório por fonte");
  bool reopenedAll = reports.size() == 4;
  for (usize i = 0; reopenedAll && i < 3; ++i) reopenedAll &= reports[i].diagnostic.empty() && reports[i].reimported;
  AE_EXPECT_TRUE(reopenedAll, "as três fontes registradas reabriram sem criar objetos");
  AE_EXPECT_TRUE(reports.size() == 4 && !reports[3].diagnostic.empty(), "a fonte recusada fica com o motivo e não derruba as outras");
  AE_EXPECT_TRUE(batch.load(scenePath.string().c_str(), 0), "cena em lote");
  AE_EXPECT_EQ(serializeEditorDocument(batch.document(), 0), serializeEditorDocument(sequential.document(), 0),
               "mesma cena que o caminho sequencial");
  std::vector<renderer::MapDrawState> batchStates, sequentialStates;
  AE_EXPECT_TRUE(batch.extractMap(batchStates) && sequential.extractMap(sequentialStates), "as duas extraem");
  AE_EXPECT_EQ(batchStates.size(), sequentialStates.size(), "mesmos desenhos");
}
