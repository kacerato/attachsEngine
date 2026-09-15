#pragma once
#include "core/base.h"
#include "resources/asset_registry.h"
#include "resources/image_decode.h"
#include <array>
#include <string>
#include <string_view>

namespace ae::resources {
// Perfil de uma textura do PROJETO (R4: T03, T05, T07, T10, T13): as escolhas
// autorais de como a imagem vira textura publicada. Vale para todos os usos da
// textura; as escolhas por uso (UV, repetição, filtro, transformação) continuam
// no material.
//
// Onde vive: `.astra/textures/<guid>.profile`. Arquivo ausente ou inválido volta
// ao padrão, que é o comportamento anterior ao perfil.
//
// Texturas embutidas nas fontes seguem o perfil de importação da fonte (R3); para
// escolher por textura, extraia a imagem para o projeto.
inline constexpr u32 TextureProfileSchema = 1;
// Interpretação: pelo uso (cor base e emissão em sRGB, os demais lineares), cor
// (sRGB sempre) ou dado (linear sempre).
inline constexpr u8 TextureInterpretationUse = 0, TextureInterpretationColor = 1, TextureInterpretationData = 2;
// Maior lado residente. Zero segue o teto do projeto e do aparelho; nunca sobe acima dele.
inline constexpr std::array<u32, 6> TextureDimensionSteps{0, 256, 512, 1024, 2048, 4096};

struct TextureProfile {
  u8 interpretation = TextureInterpretationUse;
  u32 maximumDimension = 0;
  bool mipmaps = true;
  // Cor dos texels transparentes copiada dos vizinhos visíveis antes dos mips:
  // tira o halo escuro das bordas recortadas sem mudar o alfa.
  bool dilateEdges = false;
  // Anisotropia da qualidade escolhida no aparelho; desligar fica por textura.
  bool anisotropy = true;
};
bool sameTextureProfile(const TextureProfile &a, const TextureProfile &b) noexcept;
bool validTextureProfile(const TextureProfile &profile) noexcept;
std::string serializeTextureProfile(const TextureProfile &profile);
// Falha fechada: schema diferente, campo fora dos passos ou JSON inválido.
bool parseTextureProfile(std::string_view text, TextureProfile &out);
std::string textureProfilePath(const AssetGuid &texture);

// Espalha a cor dos texels visíveis (alfa >= `threshold`) para os vizinhos
// transparentes, um anel por passo, sem tocar no alfa. Devolve quantos texels
// receberam cor.
u32 dilateTransparentEdges(DecodedImage &image, u32 passes = 8, u8 threshold = 1);
} // namespace ae::resources
