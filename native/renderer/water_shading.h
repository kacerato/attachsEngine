#pragma once

#include <algorithm>
#include <cmath>

namespace ae::renderer {

// Independent runtime axes; do not change the serialized WaterProfile layout.
struct WaterShadingSettings final {
  float specularAntialiasing = .5f; // zero preserves authored roughness
  float contactFoamWidth = 1.35f;   // metres along the view ray; zero disables
  // Espessura visual acima da superfície líquida. A espuma não altera o
  // volume usado pela física: ela é uma camada aerada sobre a crista.
  float foamElevation = .14f;
  // Multiplicador de cobertura anterior à quebra textural. Mantê-lo separado
  // de WaterProfile::foamDecay permite aumentar área sem estourar radiância.
  float foamCoverage = 1.35f;
  // Ondas menores que as cascatas principais podem participar da silhueta no
  // campo próximo. O shader filtra por tamanho de célula antes de deslocar.
  float microDisplacement = .10f;
  float microWavelength = .85f;
};

inline bool validateWaterShading(const WaterShadingSettings &settings) noexcept {
  return std::isfinite(settings.specularAntialiasing) && settings.specularAntialiasing >= 0 &&
      settings.specularAntialiasing <= 1 && std::isfinite(settings.contactFoamWidth) &&
      settings.contactFoamWidth >= 0 && settings.contactFoamWidth <= 10 &&
      std::isfinite(settings.foamElevation) && settings.foamElevation >= 0 &&
      settings.foamElevation <= 1 && std::isfinite(settings.foamCoverage) &&
      settings.foamCoverage >= 0 && settings.foamCoverage <= 4 &&
      std::isfinite(settings.microDisplacement) && settings.microDisplacement >= 0 &&
      settings.microDisplacement <= 1 && std::isfinite(settings.microWavelength) &&
      settings.microWavelength >= .2f && settings.microWavelength <= 8;
}

inline float maximumWaterDetailDisplacement(const WaterShadingSettings &settings) noexcept {
  return validateWaterShading(settings)
      ? settings.foamElevation + settings.microDisplacement * 4.0f : 0.0f;
}

// CPU oracle for the shader's normal-variance filter. The 0.25 cap bounds
// broadening across silhouette discontinuities; authored roughness is a floor.
inline float filteredWaterRoughness(float roughness, float normalGradientSquared,
                                    float strength) noexcept {
  if (!std::isfinite(roughness) || !std::isfinite(normalGradientSquared) ||
      !std::isfinite(strength)) return 1;
  const float r = std::clamp(roughness, .025f, 1.0f);
  const float variance = std::min(.25f, std::max(0.0f, normalGradientSquared) *
                                       std::clamp(strength, 0.0f, 1.0f));
  return std::sqrt(std::sqrt(std::min(1.0f, r * r * r * r + variance)));
}

} // namespace ae::renderer
