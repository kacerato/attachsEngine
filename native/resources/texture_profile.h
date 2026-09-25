#pragma once
#include "core/base.h"
#include "resources/asset_registry.h"
#include "resources/image_decode.h"
#include <array>
#include <atomic>
#include <string>
#include <string_view>

namespace ae::resources {
// Perfil de uma textura do PROJETO (R4: T03, T05, T07, T10, T13): as escolhas
// autorais de como a imagem vira textura publicada. Vale para todos os usos da
// textura; as escolhas por uso (UV, repetição, filtro, transformação) continuam
// no material.
//
// A receita autoral atual vive em `AssetRecord.importerParameters`. O caminho
// `.astra/textures/<guid>.profile` permanece apenas para leitura/migração de
// projetos antigos; valor ausente ou inválido volta ao padrão anterior.
//
// Texturas embutidas nas fontes seguem o perfil de importação da fonte (R3); para
// escolher por textura, extraia a imagem para o projeto.
// 4 (S2): streaming de mipmaps e prioridade, do Texture Importer da Unity.
inline constexpr u32 TextureProfileSchema = 4;
// Faixa da prioridade de streaming da Unity (Texture Importer > Priority).
inline constexpr i32 TextureStreamingPriorityMinimum = -128, TextureStreamingPriorityMaximum = 127;
// Interpretação: pelo uso (cor base e emissão em sRGB, os demais lineares), cor
// (sRGB sempre) ou dado (linear sempre).
inline constexpr u8 TextureInterpretationUse = 0, TextureInterpretationColor = 1,
                    TextureInterpretationData = 2, TextureInterpretationNormal = 3;
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
  // Convenção DirectX opcional. Só é consumida quando `interpretation` é
  // Normal; fica persistida ao alternar temporariamente o tipo no Inspector.
  bool invertNormalGreen = false;
  // Mantém a fração de texels que passa no alpha test ao gerar mips de cor.
  // Desligado preserva exatamente o comportamento anterior.
  bool preserveAlphaCoverage = false;
  float alphaCoverageCutoff = 0.5f;
  // "Stream Mipmap Levels" e "Priority" (S2). Não mudam os bytes preparados:
  // valem na próxima publicação, sem repreparar a imagem.
  bool streamingMipmaps = true;
  i32 streamingPriority = 0;
};
// Igualdade só do que muda os bytes preparados (o streaming fica de fora).
bool sameTexturePreparation(const TextureProfile &a, const TextureProfile &b) noexcept;
bool sameTextureProfile(const TextureProfile &a, const TextureProfile &b) noexcept;
bool validTextureProfile(const TextureProfile &profile) noexcept;
std::string serializeTextureProfile(const TextureProfile &profile);
// Falha fechada: schema diferente, campo fora dos passos ou JSON inválido.
bool parseTextureProfile(std::string_view text, TextureProfile &out);
std::string textureProfilePath(const AssetGuid &texture);

// Espalha a cor dos texels visíveis (alfa >= `threshold`) para os vizinhos
// transparentes, um anel por passo, sem tocar no alfa. Devolve quantos texels
// receberam cor.
u32 dilateTransparentEdges(DecodedImage &image, u32 passes = 8, u8 threshold = 1,
                           const std::atomic<bool> *cancel = nullptr);
} // namespace ae::resources
