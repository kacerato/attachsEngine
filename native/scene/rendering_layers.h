#pragma once
#include "scene/components.h"
#include <array>

namespace ae::scene {
// Máscaras de iluminação, como na Unity 6 (URP):
//
//  - Culling Mask da Luz: quais CAMADAS DE OBJETO (as camadas de gameplay do
//    projeto, o `layer` de cada objeto) a luz alcança. Padrão: tudo.
//  - Rendering Layers: uma segunda máscara, só de renderização, que existe na
//    Luz e no Mesh Renderer. A luz ilumina o renderer quando as duas máscaras
//    têm um bit em comum. Padrão dos dois: "Padrão" (bit 0), então nada muda
//    até o autor separar camadas.
//
// As duas são decididas na CPU, por desenho, na máscara de luzes que o
// fragmento já lê (renderer/punctual_lights.h): nenhuma luz excluída custa
// busca no shader. O sol principal usa um bit próprio da mesma palavra.
//
// Referências: https://docs.unity3d.com/6000.0/Documentation/Manual/urp/light-component.html
//              https://docs.unity3d.com/6000.0/Documentation/Manual/features/rendering-layers.html
inline constexpr u32 kRenderingLayerCount = 8;
inline constexpr u32 kDefaultRenderingLayers = 1u;
inline constexpr u32 kAllRenderingLayers = (1u << kRenderingLayerCount) - 1u;
inline constexpr u32 kAllObjectLayers = ~0u;

inline constexpr std::array<ComponentEnumOption, kRenderingLayerCount> renderingLayerOptions{{
  {1u << 0, "Padrão"}, {1u << 1, "Camada 1"}, {1u << 2, "Camada 2"}, {1u << 3, "Camada 3"},
  {1u << 4, "Camada 4"}, {1u << 5, "Camada 5"}, {1u << 6, "Camada 6"}, {1u << 7, "Camada 7"}
}};

// As 32 camadas de objeto. O nome exibido vem do projeto (Configurações ›
// Camadas); estes rótulos são o que a API C# e a matriz de propriedades veem.
inline constexpr std::array<ComponentEnumOption, 32> objectLayerOptions{{
  {1u << 0, "Padrão"}, {1u << 1, "Camada 1"}, {1u << 2, "Camada 2"}, {1u << 3, "Camada 3"},
  {1u << 4, "Camada 4"}, {1u << 5, "Camada 5"}, {1u << 6, "Camada 6"}, {1u << 7, "Camada 7"},
  {1u << 8, "Camada 8"}, {1u << 9, "Camada 9"}, {1u << 10, "Camada 10"}, {1u << 11, "Camada 11"},
  {1u << 12, "Camada 12"}, {1u << 13, "Camada 13"}, {1u << 14, "Camada 14"}, {1u << 15, "Camada 15"},
  {1u << 16, "Camada 16"}, {1u << 17, "Camada 17"}, {1u << 18, "Camada 18"}, {1u << 19, "Camada 19"},
  {1u << 20, "Camada 20"}, {1u << 21, "Camada 21"}, {1u << 22, "Camada 22"}, {1u << 23, "Camada 23"},
  {1u << 24, "Camada 24"}, {1u << 25, "Camada 25"}, {1u << 26, "Camada 26"}, {1u << 27, "Camada 27"},
  {1u << 28, "Camada 28"}, {1u << 29, "Camada 29"}, {1u << 30, "Camada 30"}, {1u << 31, "Camada 31"}
}};

// A regra única que extrator, testes e documentação usam.
inline constexpr bool lightReachesRenderer(u32 lightCullingMask, u32 lightRenderingLayers,
                                           u32 objectLayer, u32 rendererRenderingLayers) noexcept {
  const u32 layerBit = objectLayer < 32 ? 1u << objectLayer : 0u;
  return (lightCullingMask & layerBit) != 0 && (lightRenderingLayers & rendererRenderingLayers) != 0;
}
} // namespace ae::scene
