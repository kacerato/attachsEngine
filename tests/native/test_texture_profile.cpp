// R4 — perfil de textura do projeto: formato e processamento de bordas.
#include "harness.h"
#include "resources/texture_profile.h"
#include "renderer/authoring_library_plan.h"

#include <memory>

using namespace ae;
using namespace ae::resources;

AE_TEST(s2_texture_profile_streaming_round_trips_migrates_and_does_not_reprepare) {
  TextureProfile profile;
  profile.streamingMipmaps = false;
  profile.streamingPriority = -5;
  TextureProfile back;
  AE_EXPECT_TRUE(parseTextureProfile(serializeTextureProfile(profile), back) && !back.streamingMipmaps &&
                 back.streamingPriority == -5, "streaming desligado e prioridade negativa voltam");
  // Um perfil gravado antes do schema 4 entra no streaming com prioridade 0.
  AE_EXPECT_TRUE(parseTextureProfile("{\"schema\":3,\"interpretation\":0,\"maximumDimension\":0,\"mipmaps\":1,"
                                     "\"dilateEdges\":0,\"anisotropy\":1,\"invertNormalGreen\":0,"
                                     "\"preserveAlphaCoverage\":0,\"alphaCoverageCutoff\":500}", back) &&
                 back.streamingMipmaps && back.streamingPriority == 0, "schema 3 migra para o padrão");
  auto text = serializeTextureProfile(profile);
  text.replace(text.find("-5"), 2, "200");
  AE_EXPECT_TRUE(!parseTextureProfile(text, back), "prioridade fora de -128..127 recusada");
  TextureProfile other = profile;
  other.streamingPriority = 9;
  AE_EXPECT_TRUE(sameTexturePreparation(profile, other) && !sameTextureProfile(profile, other),
                 "prioridade muda o perfil, não a preparação");
}

AE_TEST(r4_texture_profile_round_trips_fails_closed_and_dilates_edges) {
  TextureProfile profile;
  profile.interpretation = TextureInterpretationData;
  profile.maximumDimension = 512;
  profile.mipmaps = false;
  profile.dilateEdges = true;
  profile.anisotropy = false;
  profile.invertNormalGreen = true;
  profile.preserveAlphaCoverage = true;
  profile.alphaCoverageCutoff = .65f;
  TextureProfile back;
  AE_EXPECT_TRUE(parseTextureProfile(serializeTextureProfile(profile), back) && sameTextureProfile(profile, back), "perfil volta igual");
  AE_EXPECT_TRUE(!parseTextureProfile(
                     "{\"schema\":1,\"interpretation\":0,\"maximumDimension\":300,\"mipmaps\":1,\"dilateEdges\":0,\"anisotropy\":1}", back),
                 "tamanho fora dos passos recusado");
  AE_EXPECT_TRUE(!parseTextureProfile(
                     "{\"schema\":2,\"interpretation\":0,\"maximumDimension\":0,\"mipmaps\":1,\"dilateEdges\":0,\"anisotropy\":1}", back),
                 "schema diferente recusado");
  AE_EXPECT_TRUE(!parseTextureProfile(
                     "{\"schema\":1,\"interpretation\":3,\"maximumDimension\":0,\"mipmaps\":1,\"dilateEdges\":0,\"anisotropy\":1}", back),
                 "interpretação desconhecida recusada");
  TextureProfile legacy;
  AE_EXPECT_TRUE(parseTextureProfile(
      "{\"schema\":1,\"interpretation\":2,\"maximumDimension\":512,\"mipmaps\":1,\"dilateEdges\":0,\"anisotropy\":1}", legacy) &&
      !legacy.invertNormalGreen && !legacy.preserveAlphaCoverage && legacy.alphaCoverageCutoff == .5f,
      "schema 1 migrates with alpha coverage disabled");
  AE_EXPECT_TRUE(textureProfilePath(assetGuidFromSeed("textura")).starts_with(".astra/textures/"), "perfil mora em .astra/textures");

  // Vermelho opaco, dois transparentes pretos, azul opaco: um passo leva a cor
  // de cada lado para o vizinho transparente, sem tocar no alfa.
  DecodedImage image;
  image.width = 4;
  image.height = 1;
  image.rgba = {255, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255};
  AE_EXPECT_EQ(dilateTransparentEdges(image, 1), 2u, "dois texels receberam cor");
  AE_EXPECT_TRUE(image.rgba[4] == 255 && image.rgba[5] == 0 && image.rgba[7] == 0, "vermelho no vizinho, alfa zero");
  AE_EXPECT_TRUE(image.rgba[10] == 255 && image.rgba[8] == 0 && image.rgba[11] == 0, "azul no outro vizinho, alfa zero");
  DecodedImage empty;
  AE_EXPECT_EQ(dilateTransparentEdges(empty), 0u, "imagem vazia não muda");
}

AE_TEST(r2_r4_authoring_library_reuses_only_the_same_shared_textures) {
  const auto make = [] { return std::make_shared<const renderer::AuthoringTexture>(); };
  const renderer::SharedAuthoringTexture a = make(), b = make(), c = make();
  const std::vector<renderer::SharedAuthoringTexture> previous{a, b};
  // Ordem nova, uma textura nova, uma repetida e uma nula.
  const std::vector<renderer::SharedAuthoringTexture> next{b, c, a, a, nullptr};
  const auto plan = renderer::planAuthoringTextureReuse(previous, next);
  AE_EXPECT_TRUE(plan.reuse.size() == 5 && plan.reuse[0] == 1 && plan.reuse[2] == 0, "mesma textura reaproveita a imagem anterior, em qualquer ordem");
  AE_EXPECT_EQ(plan.reuse[1], renderer::AuthoringTextureNoReuse, "textura nova sobe");
  AE_EXPECT_EQ(plan.reuse[3], renderer::AuthoringTextureNoReuse, "a imagem tem um dono só: a repetição sobe de novo");
  AE_EXPECT_EQ(plan.reuse[4], renderer::AuthoringTextureNoReuse, "entrada nula não reaproveita nada");
  AE_EXPECT_TRUE(plan.reused == 2 && plan.uploaded == 3, "contagem de reaproveitadas e enviadas");
  const auto first = renderer::planAuthoringTextureReuse({}, previous);
  AE_EXPECT_TRUE(first.reused == 0 && first.uploaded == 2, "primeira publicação envia tudo");
  const auto anotherMip = renderer::planAuthoringTextureReuse(previous, previous, 0, 1);
  AE_EXPECT_TRUE(anotherMip.reused == 0 && anotherMip.uploaded == previous.size(),
                 "mesma fonte com outro mip residente volta para a GPU");
}
