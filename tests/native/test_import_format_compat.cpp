// Formatos antigos continuam legíveis depois que um campo novo entra.
//
// Estes testes existem porque a falta deles deixou passar um defeito real:
// subir a versão do mapa de nós e do perfil de importação fez os arquivos que
// JÁ ESTÃO nos projetos serem recusados. Recusa, nesses dois arquivos, não é
// erro visível — o mapa ausente faz a reimportação voltar às identidades
// legadas, e o perfil ausente faz a escala escolhida voltar ao padrão. Os dois
// em silêncio. Aqui ficam fixados os textos exatos que as versões anteriores
// escreviam, e o que cada um precisa significar hoje.
#include "harness.h"

#include "resources/import_node_map.h"
#include "resources/import_profile.h"

using namespace ae;
using namespace ae::resources;

AE_TEST(import_profile_schema_1_keeps_the_authors_scale) {
  // Exatamente o que o editor gravava antes das normais e das câmeras.
  ImportProfile parsed;
  AE_EXPECT_TRUE(parseImportProfile(R"({"schema":1,"scale":100,"maximumTextureDimension":1024})", parsed),
                 "o perfil salvo antes dos campos novos continua valendo");
  AE_EXPECT_TRUE(parsed.scale == 100.f, "a escala escolhida pelo autor sobrevive");
  AE_EXPECT_EQ(parsed.maximumTextureDimension, 1024u, "e a dimensão de textura também");
  AE_EXPECT_EQ(parsed.normals, GltfNormalsImport, "normais no comportamento de então");
  AE_EXPECT_EQ(parsed.tangents, GltfTangentsImport, "tangentes no comportamento de então");
  AE_EXPECT_TRUE(!parsed.importCameras, "e câmeras continuam de fora, como eram");
  AE_EXPECT_TRUE(!parsed.importLights, "e luzes também preservam o comportamento anterior");
}

AE_TEST(import_profile_schema_2_reads_without_the_camera_field) {
  ImportProfile parsed;
  AE_EXPECT_TRUE(parseImportProfile(
                     R"({"schema":2,"scale":1,"maximumTextureDimension":512,"normals":1,"normalWeighting":1,"tangents":0})",
                     parsed),
                 "o schema 2 não tinha câmera e continua legível");
  AE_EXPECT_EQ(parsed.normals, GltfNormalsCalculate, "a escolha de normais do schema 2 é preservada");
  AE_EXPECT_TRUE(!parsed.importCameras, "o campo que ele não conhecia fica no padrão");
}

AE_TEST(import_profile_before_schema_5_keeps_every_normal_smooth) {
  // O schema 4 não tinha Smoothing Angle: o que ele publicou foi normal toda
  // suave. Ler com os 60° de um perfil novo mudaria a malha na reimportação.
  ImportProfile parsed;
  AE_EXPECT_TRUE(parseImportProfile(
                     R"({"schema":4,"scale":1,"maximumTextureDimension":512,"normals":1,"normalWeighting":0,"tangents":0,"importCameras":false,"excludedNodes":[]})",
                     parsed),
                 "o schema 4 continua legível");
  AE_EXPECT_EQ(parsed.smoothingAngle, 180u, "sem o campo, tudo suave, como era");
  AE_EXPECT_EQ(ImportProfile{}.smoothingAngle, 60u, "um perfil NOVO nasce com os 60° da Unity");

  ImportProfile sharp;
  sharp.smoothingAngle = 30;
  ImportProfile read;
  AE_EXPECT_TRUE(parseImportProfile(serializeImportProfile(sharp), read) && read.smoothingAngle == 30u, "ida e volta");
  AE_EXPECT_TRUE(!sameImportPreparation(sharp, ImportProfile{}), "outro ângulo pede nova preparação");
  AE_EXPECT_EQ(applyImportProfile({}, sharp).smoothingAngle, 30.0f, "e chega ao importador");
}

AE_TEST(import_profile_schema_10_streaming_round_trips_without_repreparing) {
  ImportProfile profile;
  profile.textureStreaming = false;
  profile.textureStreamingPriority = -3;
  ImportProfile read;
  AE_EXPECT_TRUE(parseImportProfile(serializeImportProfile(profile), read) && !read.textureStreaming &&
                 read.textureStreamingPriority == -3, "streaming da fonte volta");
  AE_EXPECT_TRUE(sameImportPreparation(profile, ImportProfile{}) && !sameImportProfile(profile, ImportProfile{}),
                 "streaming não pede nova preparação, mas é outro perfil");
  AE_EXPECT_TRUE(parseImportProfile(R"({"schema":9,"scale":1,"maximumTextureDimension":512,"normals":0,"normalWeighting":0,"smoothingAngle":60,"tangents":0,"importCameras":false,"importLights":false,"textureCompression":0,"excludedNodes":[],"collisionMeshes":[]})", read) &&
                 read.textureStreaming && read.textureStreamingPriority == 0, "schema 9 entra no streaming");
  auto text = serializeImportProfile(profile);
  text.replace(text.find("\"textureStreamingPriority\":-3"), 29, "\"textureStreamingPriority\":999");
  AE_EXPECT_TRUE(!parseImportProfile(text, read), "prioridade fora da faixa recusa o arquivo");
}

AE_TEST(import_profile_refuses_a_future_schema_and_a_truncated_current_one) {
  ImportProfile parsed;
  AE_EXPECT_TRUE(!parseImportProfile(R"({"schema":99,"scale":1,"maximumTextureDimension":512})", parsed),
                 "schema novo demais pode carregar escolha que este leitor não honra");
  AE_EXPECT_TRUE(!parseImportProfile(R"({"schema":2,"scale":1,"maximumTextureDimension":512})", parsed),
                 "o schema 2 declara normais; sem elas o arquivo está truncado");
  auto current=serializeImportProfile({});
  const auto field=current.find(",\"importLights\":false");
  AE_EXPECT_TRUE(field!=std::string::npos,"o schema atual declara a escolha de luzes");
  current.erase(field,std::string(",\"importLights\":false").size());
  AE_EXPECT_TRUE(!parseImportProfile(current,parsed),"schema 8 sem Import Lights é arquivo truncado");
}

AE_TEST(import_node_map_version_1_is_still_read) {
  // Um mapa v1 com dois nós, pai e filho, do jeito que a primeira versão gravava.
  const std::string v1 =
      "ASTRA_NODEMAP 1 3 \"abc\" 2\n"
      "0000000000000000000000000000000a - 1 0 \"Raiz\" \"\" 1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 0\n"
      "0000000000000000000000000000000b 0000000000000000000000000000000a 2 7 \"Roda\" \"id-roda\" "
      "1 0 0 0 0 1 0 0 0 0 1 0 2 0 0 1 1 0000000000000000000000000000000c\n";
  ImportNodeMap map;
  AE_EXPECT_TRUE(ImportNodeMap::deserialize(v1, map), "o mapa v1 continua legível");
  AE_EXPECT_EQ(map.revision, 3u, "a revisão é preservada");
  AE_EXPECT_EQ(map.nodes.size(), 2u, "os dois nós voltaram");
  AE_EXPECT_EQ(map.nodes[1].authoredId, std::string("id-roda"), "com a identidade do autor");
  AE_EXPECT_EQ(map.nodes[1].introduced, 2u, "e a revisão em que o nó apareceu");
  AE_EXPECT_TRUE(!map.nodes[1].camera, "um mapa v1 não tinha câmera, e o padrão diz isso");

  // Ida e volta: o que foi lido como v1 é gravado na versão atual e relido igual.
  ImportNodeMap again;
  AE_EXPECT_TRUE(ImportNodeMap::deserialize(map.serialize(), again), "o mapa regravado relê");
  AE_EXPECT_EQ(again.nodes[1].authoredId, std::string("id-roda"), "sem perder nada no caminho");
}

AE_TEST(import_node_map_refuses_a_future_version) {
  const std::string future = "ASTRA_NODEMAP 99 1 \"abc\" 0\n";
  ImportNodeMap map;
  AE_EXPECT_TRUE(!ImportNodeMap::deserialize(future, map), "versão nova demais é recusada");
}
