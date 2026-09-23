#pragma once

#include "core/base.h"

namespace ae::renderer {

// Parâmetros que realmente alteram a irradiância hemisférica incidente no
// solo. `physicalSkyIntensity` e groundAlbedo são aplicados no shader depois:
// mudar um ganho ou a reflectância não deve refazer a integração atmosférica.
struct PhysicalAtmosphereGroundIrradianceInput final {
  float airDensity = 1.0f;
  float aerosolDensity = 1.0f;
  float aerosolAnisotropy = 0.76f;
  float planetRadiusKm = 6371.0f;
  float rayleighScaleHeightKm = 8.0f;
  float aerosolScaleHeightKm = 1.2f;
  float atmosphereHeightKm = 100.0f;
  float sunDirection[3]{0.0f, 1.0f, 0.0f};
  float sunColor[3]{1.0f, 1.0f, 1.0f};
  float sunIntensity = 0.0f;

  bool valid() const noexcept;
  friend bool operator==(const PhysicalAtmosphereGroundIrradianceInput &,
                         const PhysicalAtmosphereGroundIrradianceInput &) noexcept = default;
};

// Integra três direções com pesos cosseno no ponto local abaixo da câmera.
// A saída é irradiância RGB; o BRDF Lambertiano divide por pi no sky shader.
bool computePhysicalAtmosphereGroundIrradiance(
    const PhysicalAtmosphereGroundIrradianceInput &input, float out[3]) noexcept;

class PhysicalAtmosphereGroundIrradianceCache final {
public:
  bool resolve(const PhysicalAtmosphereGroundIrradianceInput &input, float out[3]) noexcept;
  u64 revision() const noexcept { return revision_; }

private:
  PhysicalAtmosphereGroundIrradianceInput input_{};
  float irradiance_[3]{};
  u64 revision_ = 0;
  bool initialized_ = false;
};

} // namespace ae::renderer
