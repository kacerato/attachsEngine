// R4 — perfil de textura do projeto: formato e processamento de bordas.
#include "harness.h"
#include "resources/texture_profile.h"

using namespace ae;
using namespace ae::resources;

AE_TEST(r4_texture_profile_round_trips_fails_closed_and_dilates_edges) {
  TextureProfile profile;
  profile.interpretation = TextureInterpretationData;
  profile.maximumDimension = 512;
  profile.mipmaps = false;
  profile.dilateEdges = true;
  profile.anisotropy = false;
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
