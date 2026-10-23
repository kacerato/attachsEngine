#pragma once

#include "renderer/scene_environment.h"
#include "scene/components.h"
#include <array>

namespace ae::scene {

// Números que só existem a partir da v17 (Color Adjustments e Bloom completos).
inline bool environmentNumberSinceV17(std::string_view id) {
  return id=="post_exposure"||id=="hue_shift"||id.starts_with("color_filter.")||
         id=="bloom_scatter"||id=="bloom_clamp"||id.starts_with("bloom_tint.");
}

// Números da janela Lighting (PK-1/PK-2), a partir da v18.
inline bool environmentNumberSinceV18(std::string_view id) {
  return id=="sky_exposure"||id.starts_with("sun_size")||id.starts_with("ambient_sky_color")||
         id.starts_with("ambient_equator_color")||id.starts_with("ambient_ground_color")||
         id.starts_with("ambient_color")||id=="fog_end";
}

class Environment final : public ComponentValue {
public:
  Environment() { values.active = true; }
  static constexpr u32 CurrentVersion = 18;
  renderer::SceneEnvironment values{};
  renderer::EnvironmentVolumeShape shape=renderer::EnvironmentVolumeShape::Global;

#include "scene/generated/environment_Environment_fields0.inc"

  float weight=1.0f,blendDistance=0.0f,boxSize[3]{10,10,10},sphereRadius=5.0f;
  u32 layer=0;
  resources::AssetGuid profile{};

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override {
    return std::make_unique<Environment>(*this);
  }
  bool valid() const override {
    if(!values.valid()||static_cast<u32>(shape)>2||layer>7||!std::isfinite(weight)||weight<0||weight>1||
       !std::isfinite(blendDistance)||blendDistance<0||!std::isfinite(sphereRadius)||sphereRadius<=0) return false;
    for(float value:boxSize) if(!std::isfinite(value)||value<=0) return false;
    return true;
  }
  void write(std::ostream &out) const override {
    out << values.active << ' ' << values.fog << ' ' << values.post << ' '
        << values.bloom << ' ' << values.vignette << ' ' << values.filmGrain << ' '
        << static_cast<u32>(values.sky) << ' ' << static_cast<u32>(values.toneMapper) << ' '
        << values.ambientOcclusion << ' '
        << static_cast<u32>(shape) << ' ' << overrideSky << ' ' << overrideFog << ' '
        << overridePost << ' ' << layer << ' ' << (profile.valid()?profile.text():"-") << ' ' << overrideIndirect
        << ' ' << values.physicalAtmosphereHighQuality
        << ' ' << (values.environmentMap.valid()?values.environmentMap.text():"-")
        << ' ' << static_cast<u32>(values.exposureMode) << ' ' << static_cast<u32>(values.exposureMetering)
        << ' ' << static_cast<u32>(values.exposureAdaptation)
        << ' ' << overrideExposure << ' ' << overrideBloom << ' ' << overrideVignette
        << ' ' << overrideColorAdjustments << ' ' << overrideColorGrading << ' ' << overrideFilmGrain
        << ' ' << overrideAmbientOcclusion
        << ' ' << values.clouds.enabled << ' ' << values.clouds.castShadows << ' ' << overrideClouds
        << ' ' << static_cast<u32>(values.sunDisk) << ' ' << static_cast<u32>(values.ambientSource)
        << ' ' << static_cast<u32>(values.fogMode) << ' ' << static_cast<u32>(values.fogColorMode)
        << ' ' << (values.skybox.valid()?values.skybox.text():"-")
        << ' ' << values.clouds.type;
    for (const auto &property : descriptor.numbers) out << ' ' << property.read(*this);
  }
  bool read(std::istream &in, u32 version) override {
    u32 sky = 0, tone = 0;
    loadedVersion = version;
    if ((version < 1 || version > 19) || !(in >> values.active >> values.fog >> values.post >> values.bloom >>
                          values.vignette))
      return false;
    if(version<7) {
      const renderer::SceneEnvironment defaults{};
      values.physicalAtmosphereHighQuality=defaults.physicalAtmosphereHighQuality;
      values.physicalSkyIntensity=defaults.physicalSkyIntensity;
      values.airDensity=defaults.airDensity;values.aerosolDensity=defaults.aerosolDensity;
      values.aerosolAnisotropy=defaults.aerosolAnisotropy;
      values.planetRadiusKm=defaults.planetRadiusKm;values.observerHeightKm=defaults.observerHeightKm;
      values.rayleighScaleHeightKm=defaults.rayleighScaleHeightKm;
      values.aerosolScaleHeightKm=defaults.aerosolScaleHeightKm;
      values.atmosphereHeightKm=defaults.atmosphereHeightKm;values.groundAlbedo=defaults.groundAlbedo;
    }
    if(version>=5 && !(in>>values.filmGrain)) return false;
    if(!(in>>sky>>tone) || sky>(version>=7?2u:1u) || tone>(version>=17?4u:version>=9?2u:1u)) return false;
    // v17: Reinhard (só legado) vira o Neutral da Unity, a curva mais próxima.
    if(version<17 && tone==static_cast<u32>(renderer::ToneMapper::Reinhard)) tone=static_cast<u32>(renderer::ToneMapper::Neutral);
    if(version>=3 && !(in>>values.ambientOcclusion)) return false;
    values.sky = static_cast<renderer::SkyModel>(sky);
    values.toneMapper = static_cast<renderer::ToneMapper>(tone);
    if(version>=2) {
      u32 mode=0;
      if(!(in>>mode>>overrideSky>>overrideFog>>overridePost>>layer)||mode>2||layer>31) return false;
      shape=static_cast<renderer::EnvironmentVolumeShape>(mode);
    }
    if(version>=4) {
      std::string token;
      if(!(in>>token) || (token!="-"&&!resources::AssetGuid::parse(token,profile))) return false;
    }
    // Antes da v6 não havia controle de luz indireta: o volume participa com o
    // valor neutro (1), que é exatamente o que ele fazia.
    overrideIndirect=true;values.indirectDiffuse=1;values.indirectSpecular=1;
    if(version>=6 && !(in>>overrideIndirect)) return false;
    if(version>=7 && !(in>>values.physicalAtmosphereHighQuality)) return false;
    values.environmentMap={};values.hdriRotationDegrees=0;values.hdriExposureEv=0;
    if(version>=8) {
      std::string token;
      if(!(in>>token)||(token!="-"&&!resources::AssetGuid::parse(token,values.environmentMap))) return false;
    }
    // v17: Exposure do HDRP (modo, metering, adaptação) e um override por
    // efeito de pós. Antes, uma chave só (overridePost) valia para todos.
    bool legacyAuto=false,legacyCenter=false;
    {
      const renderer::SceneEnvironment defaults{};
      values.exposureMode=defaults.exposureMode;values.exposureMetering=defaults.exposureMetering;
      values.exposureAdaptation=defaults.exposureAdaptation;values.fixedExposure=defaults.fixedExposure;
      values.exposureLimitMin=defaults.exposureLimitMin;values.exposureLimitMax=defaults.exposureLimitMax;
      values.exposureHistogramLow=defaults.exposureHistogramLow;
      values.exposureHistogramHigh=defaults.exposureHistogramHigh;
      values.exposureTargetMidGrey=defaults.exposureTargetMidGrey;
      values.exposureSpeedDarkToLight=defaults.exposureSpeedDarkToLight;
      values.exposureSpeedLightToDark=defaults.exposureSpeedLightToDark;
    }
    if(version>=17) {
      u32 mode=0,metering=0,adaptation=0;
      if(!(in>>mode>>metering>>adaptation>>overrideExposure>>overrideBloom>>overrideVignette>>
           overrideColorAdjustments>>overrideColorGrading>>overrideFilmGrain>>overrideAmbientOcclusion) ||
         mode>2 || metering>2 || adaptation>1) return false;
      values.exposureMode=static_cast<renderer::ExposureMode>(mode);
      values.exposureMetering=static_cast<renderer::ExposureMetering>(metering);
      values.exposureAdaptation=static_cast<renderer::ExposureAdaptation>(adaptation);
    } else {
      if(version>=11 && !(in>>legacyAuto>>legacyCenter)) return false;
      overrideExposure=overrideBloom=overrideVignette=overrideColorAdjustments=
          overrideColorGrading=overrideFilmGrain=overrideAmbientOcclusion=overridePost;
    }
    // v16 (G4b): Cloud Layer. Cena antiga fica sem nuvens: o mesmo céu de antes.
    if(version>=16) {
      if(!(in>>values.clouds.enabled>>values.clouds.castShadows>>overrideClouds)) return false;
    } else {values.clouds={};overrideClouds=true;}
    // v18 (PK-1/PK-2): janela Lighting da Unity. Cena antiga: céu como antes
    // (espessura 1, Exposure equivalente, disco High Quality se havia disco),
    // ambiente do Skybox, neblina exponencial que acompanha o céu -- a mesma
    // imagem. A escala antiga pelo sol (skyScale) é migrada pelo documento,
    // que conhece as luzes (runtime/environment_migration.h).
    if(version>=18) {
      u32 sun=0,ambient=0,fogMode=0,fogColor=0;std::string token;
      if(!(in>>sun>>ambient>>fogMode>>fogColor>>token)||sun>2||ambient>2||fogMode>2||fogColor>1||
         (token!="-"&&!resources::AssetGuid::parse(token,values.skybox))) return false;
      values.sunDisk=static_cast<renderer::SunDiskMode>(sun);
      values.ambientSource=static_cast<renderer::AmbientSource>(ambient);
      values.fogMode=static_cast<renderer::FogMode>(fogMode);
      values.fogColorMode=static_cast<renderer::FogColorMode>(fogColor);
    } else {
      const renderer::SceneEnvironment defaults{};
      values.skybox={};values.ambientSource=renderer::AmbientSource::Skybox;
      values.fogMode=renderer::FogMode::Exponential;values.fogColorMode=renderer::FogColorMode::SkyLight;
      values.skyExposure=defaults.skyExposure;values.sunSize=defaults.sunSize;
      values.sunSizeConvergence=defaults.sunSizeConvergence;values.fogEnd=defaults.fogEnd;
      std::copy_n(defaults.ambientSkyColor,3,values.ambientSkyColor);
      std::copy_n(defaults.ambientEquatorColor,3,values.ambientEquatorColor);
      std::copy_n(defaults.ambientGroundColor,3,values.ambientGroundColor);
      std::copy_n(defaults.ambientColor,3,values.ambientColor);
    }
    // v19 (G4d): tipo Volumetric Clouds. Cena antiga continua na camada 2D.
    if(version>=19) {
      if(!(in>>values.clouds.type)||values.clouds.type>1) return false;
    } else values.clouds.type=0;
    if(version<12) values.fogLightEnergy=1.0f;
    // v13 (G2): gradação de cor. Cena antiga fica neutra: sem LUT, mesma imagem.
    if(version<13) values.grading={};
    // v14 (G4): sol, lua e estrelas. Cena antiga recebe os padrões novos: o céu
    // dela ganha auréola, lua e estrelas -- é a correção pedida, não um estilo.
    // v15: o zênite vira o Sky Tint da Unity (0,5 neutro). Uma cor de céu antiga
    // como tom deixaria o céu saturado; a cena antiga passa ao tom neutro.
    if(version<15) for(float &channel:values.skyZenith) channel=0.5f;
    if(version<14) {
      const renderer::SceneEnvironment defaults{};
      values.sunGlare=defaults.sunGlare;values.moonPhase=defaults.moonPhase;
      values.moonSizeDegrees=defaults.moonSizeDegrees;values.moonIntensity=defaults.moonIntensity;
      values.starIntensity=defaults.starIntensity;
    }
    for (const auto &property : descriptor.numbers) {
      if(version<18 && environmentNumberSinceV18(property.id)) continue;
      // v19 (G4c/G4d): espessura da camada e Volumetric Clouds; cena antiga fica
      // com os padrões.
      if(version<19 && (property.id=="cloud_thickness" || property.id=="cloud_shape_factor" ||
                        property.id=="cloud_erosion" || property.id=="cloud_steps")) continue;
      if(version<16 && property.id.starts_with("cloud_")) continue;
      if(version<17 && environmentNumberSinceV17(property.id)) continue;
      if(version<14 && (property.id.starts_with("sun_glare") || property.id.starts_with("moon_") ||
                        property.id.starts_with("star_"))) continue;
      if(version<13 && property.id.starts_with("grading_")) continue;
      if(version<12 && property.id=="fog_light_energy") continue;
      if(version<17 && property.id=="exposure_fixed") continue;
      if(version<11 && property.id.starts_with("exposure_") && property.id!="exposure_compensation") continue;
      if(version<8 && property.id.starts_with("hdri_")) continue;
      if(version<10 && (property.id=="fog_base_height" || property.id=="fog_height_falloff")) continue;
      if(version<5 && property.id=="film_grain_intensity") continue;
      if(version<6 && property.id.starts_with("indirect_")) continue;
      if(version<7 && (property.id.starts_with("physical_") || property.id.starts_with("air_") ||
                       property.id.starts_with("aerosol_") || property.id.starts_with("planet_") ||
                       property.id.starts_with("observer_") || property.id.starts_with("rayleigh_") ||
                       property.id=="atmosphere_height_km" || property.id=="ground_albedo")) continue;
      // Campos espaciais não existiam no arquivo v1.
      if (version>=3 || (version==2 && !property.id.starts_with("ambient_occlusion_")) ||
          property.id=="priority" || property.id.starts_with("sky_") ||
          property.id.starts_with("ground.") || property.id=="atmosphere" ||
          property.id.starts_with("sun_disk_") || property.id.starts_with("fog_") ||
          property.id=="exposure_compensation" || property.id.starts_with("bloom_") ||
          property.id=="contrast" || property.id=="saturation" || property.id=="vignette_intensity" ||
          property.id.starts_with("indirect_"))
        if (!(in >> *property.write(*this))) return false;
    }
    if(version<17) renderer::migrateLegacyExposure(values,version>=11,legacyAuto,legacyCenter);
    if(version<17) {
      // Cena antiga: sem filtro nem giro de matiz, e o bloom com o raio de antes.
      const renderer::SceneEnvironment defaults{};
      values.postExposure=0;values.hueShift=0;values.bloomScatter=0;values.bloomClamp=defaults.bloomClamp;
      for(u32 channel=0;channel<3;++channel) values.colorFilter[channel]=values.bloomTint[channel]=1;
    }
    if(version<18) {
      // "Força atmosférica" (mistura 0..1 com o panorama) vira Atmosphere
      // Thickness; a espessura do céu de antes era sempre 1.
      values.atmosphere=1.0f;
      values.sunDisk=values.sunDiskIntensity>0.0f?renderer::SunDiskMode::HighQuality:renderer::SunDiskMode::None;
    }
    return valid();
  }
  // Versão em que esta instância foi lida (não é gravada): a migração de
  // documento (skyScale, multiplicadores da raiz) só toca o que veio de antes.
  u32 loadedVersion = CurrentVersion;

};

inline bool environmentIsLocal(const ComponentValue &value) {
  return static_cast<const Environment&>(value).shape!=renderer::EnvironmentVolumeShape::Global;
}
inline bool environmentIsBox(const ComponentValue &value) {
  return static_cast<const Environment&>(value).shape==renderer::EnvironmentVolumeShape::Box;
}
inline bool environmentIsSphere(const ComponentValue &value) {
  return static_cast<const Environment&>(value).shape==renderer::EnvironmentVolumeShape::Sphere;
}

inline bool environmentUsesAtmosphere(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.sky == renderer::SkyModel::Atmosphere;
}
// O horizonte autoral era do gradiente antigo; o céu procedural (v15, como o
// Skybox/Procedural da Unity) calcula o horizonte pelo sol. Fica no arquivo.
inline bool environmentNeverVisible(const ComponentValue &) {return false;}
inline bool environmentUsesHdri(const ComponentValue &value) {
  return static_cast<const Environment&>(value).values.sky==renderer::SkyModel::Hdri;
}
inline bool environmentUsesProceduralSky(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.sky != renderer::SkyModel::Hdri;
}
inline bool environmentUsesPhysicalAtmosphere(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.sky == renderer::SkyModel::PhysicalAtmosphere;
}
inline bool environmentUsesClouds(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.clouds.enabled;
}
inline bool environmentUsesVolumetricClouds(const ComponentValue &value) {
  const auto &clouds=static_cast<const Environment &>(value).values.clouds;
  return clouds.enabled&&clouds.type==1;
}
inline bool environmentUsesCloudLayer(const ComponentValue &value) {
  const auto &clouds=static_cast<const Environment &>(value).values.clouds;
  return clouds.enabled&&clouds.type==0;
}
inline bool environmentUsesCloudShadows(const ComponentValue &value) {
  const auto &clouds=static_cast<const Environment &>(value).values.clouds;
  return clouds.enabled&&clouds.castShadows;
}
// PK-2: com Material do céu, o céu é o do recurso (editado nele, compartilhado);
// os campos de céu da instância ficam só de leitura, nunca sem efeito calado.
inline bool environmentSkyIsLocal(const ComponentValue &value) {
  return !static_cast<const Environment &>(value).values.skybox.valid();
}
inline bool environmentUsesProceduralSun(const ComponentValue &value) {
  const auto &environment = static_cast<const Environment &>(value).values;
  return environment.sky == renderer::SkyModel::Atmosphere && environment.sunDisk != renderer::SunDiskMode::None;
}
inline bool environmentUsesHighQualitySun(const ComponentValue &value) {
  const auto &environment = static_cast<const Environment &>(value).values;
  return environment.sky == renderer::SkyModel::Atmosphere && environment.sunDisk == renderer::SunDiskMode::HighQuality;
}
inline bool environmentUsesAmbientGradient(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.ambientSource == renderer::AmbientSource::Gradient;
}
inline bool environmentUsesAmbientColor(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.ambientSource == renderer::AmbientSource::Color;
}
inline bool environmentUsesLinearFog(const ComponentValue &value) {
  const auto &environment = static_cast<const Environment &>(value).values;
  return environment.fog && environment.fogMode == renderer::FogMode::Linear;
}
inline bool environmentUsesExponentialFog(const ComponentValue &value) {
  const auto &environment = static_cast<const Environment &>(value).values;
  return environment.fog && environment.fogMode != renderer::FogMode::Linear;
}
inline bool environmentUsesFog(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.fog;
}
inline bool environmentUsesPost(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.post;
}
inline bool environmentUsesAutoExposure(const ComponentValue &value) {
  const auto &environment=static_cast<const Environment &>(value).values;
  return environment.post&&environment.automaticExposure();
}
inline bool environmentUsesFixedExposure(const ComponentValue &value) {
  const auto &environment=static_cast<const Environment &>(value).values;
  return environment.post&&!environment.automaticExposure();
}
inline bool environmentUsesExposureHistogram(const ComponentValue &value) {
  const auto &environment=static_cast<const Environment &>(value).values;
  return environment.post&&environment.exposureMode==renderer::ExposureMode::AutomaticHistogram;
}
inline bool environmentUsesProgressiveExposure(const ComponentValue &value) {
  const auto &environment=static_cast<const Environment &>(value).values;
  return environmentUsesAutoExposure(value)&&environment.exposureAdaptation==renderer::ExposureAdaptation::Progressive;
}
inline bool environmentUsesBloom(const ComponentValue &value) {
  const auto &environment = static_cast<const Environment &>(value).values;
  return environment.post && environment.bloom;
}
inline bool environmentUsesVignette(const ComponentValue &value) {
  const auto &environment = static_cast<const Environment &>(value).values;
  return environment.post && environment.vignette;
}
inline bool environmentUsesFilmGrain(const ComponentValue &value) {
  const auto &environment = static_cast<const Environment &>(value).values;
  return environment.post && environment.filmGrain;
}
inline bool environmentUsesAmbientOcclusion(const ComponentValue &value) {
  const auto &environment = static_cast<const Environment &>(value).values;
  return environment.post && environment.ambientOcclusion;
}

#include "scene/generated/environment_environmentNumbers.inc"

#include "scene/generated/environment_environmentTailNumbers.inc"

#include "scene/generated/environment_environmentAmbientOcclusionNumbers.inc"

#include "scene/generated/environment_environmentIndirectNumbers.inc"

#include "scene/generated/environment_environmentPhysicalAtmosphereNumbers.inc"

#include "scene/generated/environment_environmentVolumeNumbers.inc"

#include "scene/generated/environment_environmentHdriNumbers.inc"
#include "scene/generated/environment_environmentAutoExposureNumbers.inc"
#include "scene/generated/environment_environmentGradingNumbers.inc"
#include "scene/generated/environment_environmentCelestialNumbers.inc"
#include "scene/generated/environment_environmentCloudNumbers.inc"
#include "scene/generated/environment_environmentPostExtraNumbers.inc"
#include "scene/generated/environment_environmentLightingNumbers.inc"

// ComponentType recebe um span contínuo. Juntar as duas partes em um único
// array constexpr evita uma tabela paralela apenas para a paginação do editor.
inline constexpr auto environmentAllNumbers = [] {
  std::array<ComponentNumber, environmentNumbers.size() + environmentTailNumbers.size()+
      environmentAmbientOcclusionNumbers.size()+environmentVolumeNumbers.size()+environmentIndirectNumbers.size()+
      environmentPhysicalAtmosphereNumbers.size()+environmentAutoExposureNumbers.size()+environmentHdriNumbers.size()+
      environmentGradingNumbers.size()+environmentCelestialNumbers.size()+environmentCloudNumbers.size()+
      environmentPostExtraNumbers.size()+environmentLightingNumbers.size()> out{};
  for (usize i = 0; i < environmentNumbers.size(); ++i) out[i] = environmentNumbers[i];
  for (usize i = 0; i < environmentTailNumbers.size(); ++i)
    out[environmentNumbers.size() + i] = environmentTailNumbers[i];
  for(usize i=0;i<environmentAmbientOcclusionNumbers.size();++i)
    out[environmentNumbers.size()+environmentTailNumbers.size()+i]=environmentAmbientOcclusionNumbers[i];
  for(usize i=0;i<environmentVolumeNumbers.size();++i)
    out[environmentNumbers.size()+environmentTailNumbers.size()+environmentAmbientOcclusionNumbers.size()+i]=environmentVolumeNumbers[i];
  for(usize i=0;i<environmentIndirectNumbers.size();++i)
    out[environmentNumbers.size()+environmentTailNumbers.size()+environmentAmbientOcclusionNumbers.size()+
        environmentVolumeNumbers.size()+i]=environmentIndirectNumbers[i];
  for(usize i=0;i<environmentPhysicalAtmosphereNumbers.size();++i)
    out[environmentNumbers.size()+environmentTailNumbers.size()+environmentAmbientOcclusionNumbers.size()+
        environmentVolumeNumbers.size()+environmentIndirectNumbers.size()+i]=environmentPhysicalAtmosphereNumbers[i];
  // Exposição, HDRI e gradação ficam no fim, nessa ordem: o arquivo grava os
  // números na ordem da tabela, e cada versão só acrescentou no fim.
  // Nuvens (v16) depois dos corpos celestes (v14).
  // Color Adjustments e Bloom completos (v17) depois das nuvens; janela
  // Lighting (v18) por último.
  const usize lighting=environmentLightingNumbers.size();
  const usize extra=environmentPostExtraNumbers.size()+lighting;
  const usize clouds=environmentCloudNumbers.size()+extra;
  const usize tail=environmentHdriNumbers.size()+environmentGradingNumbers.size()+environmentCelestialNumbers.size()+clouds;
  for(usize i=0;i<environmentAutoExposureNumbers.size();++i)
    out[out.size()-tail-environmentAutoExposureNumbers.size()+i]=environmentAutoExposureNumbers[i];
  for(usize i=0;i<environmentHdriNumbers.size();++i)
    out[out.size()-tail+i]=environmentHdriNumbers[i];
  for(usize i=0;i<environmentGradingNumbers.size();++i)
    out[out.size()-clouds-environmentCelestialNumbers.size()-environmentGradingNumbers.size()+i]=environmentGradingNumbers[i];
  for(usize i=0;i<environmentCelestialNumbers.size();++i)
    out[out.size()-clouds-environmentCelestialNumbers.size()+i]=environmentCelestialNumbers[i];
  for(usize i=0;i<environmentCloudNumbers.size();++i) out[out.size()-clouds+i]=environmentCloudNumbers[i];
  for(usize i=0;i<environmentPostExtraNumbers.size();++i) out[out.size()-extra+i]=environmentPostExtraNumbers[i];
  for(usize i=0;i<lighting;++i) out[out.size()-lighting+i]=environmentLightingNumbers[i];
  return out;
}();

#include "scene/generated/environment_environmentBooleans.inc"

// PK-2 (A-06): o "shader" do Material do céu, como Skybox/Procedural,
// Skybox/Panoramic e o Physically Based Sky. Valores do arquivo mantidos.
inline constexpr std::array<ComponentEnumOption, 3> skyOptions{{{0,"Panorâmico (HDRI)"},{1,"Procedural"},{2,"Físico"}}};
// Ordem da Unity (None, Neutral, ACES); AgX é extra. Reinhard sai do menu: só legado.
inline constexpr std::array<ComponentEnumOption, 4> toneOptions{{{3,"Nenhum"},{4,"Neutro"},{1,"ACES"},{2,"AgX"}}};
inline constexpr std::array<ComponentEnumOption,3> environmentShapeOptions{{{0,"Global"},{1,"Caixa"},{2,"Esfera"}}};
inline constexpr std::array<ComponentEnumOption,8> environmentLayerOptions{{
  {0,"Ambiente 0"},{1,"Ambiente 1"},{2,"Ambiente 2"},{3,"Ambiente 3"},
  {4,"Ambiente 4"},{5,"Ambiente 5"},{6,"Ambiente 6"},{7,"Ambiente 7"}
}};
inline constexpr std::array<ComponentEnumOption,3> sunDiskOptions{{{0,"Nenhum"},{1,"Simples"},{2,"Alta qualidade"}}};
inline constexpr std::array<ComponentEnumOption,3> ambientSourceOptions{{{0,"Céu (Skybox)"},{1,"Gradiente"},{2,"Cor"}}};
inline constexpr std::array<ComponentEnumOption,3> fogModeOptions{{{0,"Linear"},{1,"Exponencial"},{2,"Exponencial ao quadrado"}}};
inline constexpr std::array<ComponentEnumOption,2> fogColorModeOptions{{{0,"Cor"},{1,"Acompanhar o céu"}}};
inline constexpr std::array<ComponentEnumOption,3> exposureModeOptions{{{0,"Fixa"},{1,"Automática"},{2,"Automática por histograma"}}};
inline constexpr std::array<ComponentEnumOption,3> exposureMeteringOptions{{{0,"Média"},{1,"Pontual"},{2,"Peso no centro"}}};
inline constexpr std::array<ComponentEnumOption,2> exposureAdaptationOptions{{{0,"Imediata"},{1,"Progressiva"}}};
inline constexpr std::array<ComponentEnumOption,3> exposureMidGreyOptions{{{0,"Cinza 12,5%"},{1,"Cinza 14%"},{2,"Cinza 18%"}}};
inline constexpr std::array<ComponentEnumOption,2> cloudTypeOptions{{{0,"Camada 2D"},{1,"Volumétricas"}}};
inline constexpr std::array<ComponentEnum, 13> environmentEnums{{
  {"sky", "Shader do céu", skyOptions, [](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).values.sky);}, [](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.sky=static_cast<renderer::SkyModel>(n);}, {"Céu","","Procedural, Panorâmico (HDRI) ou Físico, como o shader do Material do céu",nullptr,environmentSkyIsLocal,"render.environment.atmosphere","rhi/shaders/dirt_road_sky.frag",Invalidate::Draw}},
  {"sun_disk","Sol",sunDiskOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).values.sunDisk);},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.sunDisk=static_cast<renderer::SunDiskMode>(n);},{"Céu","","Disco do sol no céu procedural: nenhum, simples ou alta qualidade",environmentUsesAtmosphere,environmentSkyIsLocal,"render.environment.atmosphere","rhi/shaders/sky_procedural.glsl",Invalidate::Draw}},
  {"ambient_source","Fonte",ambientSourceOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).values.ambientSource);},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.ambientSource=static_cast<renderer::AmbientSource>(n);},{"Luz ambiente","","De onde vem a luz ambiente: do céu, de um gradiente ou de uma cor",nullptr,nullptr,"render.ambient.hemispheric","renderer/environment_ambient.cpp",Invalidate::Draw}},
  {"fog_mode","Modo",fogModeOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).values.fogMode);},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.fogMode=static_cast<renderer::FogMode>(n);},{"Neblina","","Linear entre Início e Fim, ou exponencial pela densidade",environmentUsesFog,nullptr,"render.environment.fog","rhi/shaders/post_process_common.glsl",Invalidate::Draw}},
  {"fog_color_mode","Modo de cor",fogColorModeOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).values.fogColorMode);},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.fogColorMode=static_cast<renderer::FogColorMode>(n);},{"Neblina","","A cor escolhida (o céu fica limpo), ou a mesma cor cobrindo o céu e apagando à noite",environmentUsesFog,nullptr,"render.environment.fog","platform/android/instanced_renderer.cpp",Invalidate::Draw}},
  {"tone_mapper", "Tonemapping", toneOptions, [](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).values.toneMapper);}, [](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.toneMapper=static_cast<renderer::ToneMapper>(n);}, {"Tonemapping","","Curva aplicada ao HDR antes da saída",environmentUsesPost,nullptr,"render.post.tonemap","rhi/shaders/post_process_common.glsl",Invalidate::Policy}},
  {"cloud_type","Tipo",cloudTypeOptions,[](const ComponentValue &v){return static_cast<const Environment&>(v).values.clouds.type;},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.clouds.type=n;},{"Nuvens","","Camada 2D (Cloud Layer, barata) ou Volumétricas (Volumetric Clouds: cúmulos com volume)",environmentUsesClouds,nullptr,"render.environment.clouds","rhi/shaders/cloud_volume.frag",Invalidate::Draw}},
  {"volume_shape","Modo",environmentShapeOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).shape);},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).shape=static_cast<renderer::EnvironmentVolumeShape>(n);},{"Volume","","Global afeta toda vista; formas usam o Transform do objeto",nullptr,nullptr,"render.environment.volumes","runtime/scene_environment.cpp",Invalidate::Draw}},
  {"exposure_mode","Modo",exposureModeOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).values.exposureMode);},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.exposureMode=static_cast<renderer::ExposureMode>(n);},{"Exposição","","Fixa usa o EV100 informado; as automáticas medem a luminância da câmera a cada quadro",environmentUsesPost,nullptr,"render.post.auto_exposure","platform/android/instanced_auto_exposure.inl",Invalidate::Policy}},
  {"exposure_metering","Medição",exposureMeteringOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).values.exposureMetering);},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.exposureMetering=static_cast<renderer::ExposureMetering>(n);},{"Exposição","","Quais pixels pesam na média: a imagem toda, um círculo no centro ou peso que cai até a borda",environmentUsesAutoExposure,nullptr,"render.post.auto_exposure","rhi/shaders/exposure_histogram.comp",Invalidate::Policy}},
  {"exposure_adaptation","Adaptação",exposureAdaptationOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).values.exposureAdaptation);},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.exposureAdaptation=static_cast<renderer::ExposureAdaptation>(n);},{"Exposição","","Imediata salta para a medição; progressiva acompanha como o olho",environmentUsesAutoExposure,nullptr,"render.post.auto_exposure","rhi/shaders/exposure_reduce.comp",Invalidate::Policy}},
  {"exposure_target_mid_grey_choice","Cinza médio alvo",exposureMidGreyOptions,[](const ComponentValue &v){const float g=static_cast<const Environment&>(v).values.exposureTargetMidGrey;return g<.1325f?0u:g<.16f?1u:2u;},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.exposureTargetMidGrey=n==0?.125f:n==1?.14f:.18f;},{"Exposição","","Tom para o qual a média medida é levada",environmentUsesAutoExposure,nullptr,"render.post.auto_exposure","rhi/shaders/exposure_reduce.comp",Invalidate::Policy}},
  {"volume_layer","Camada",environmentLayerOptions,[](const ComponentValue &v){return static_cast<const Environment&>(v).layer;},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).layer=n;},{"Volume","","Filtro consultado pela câmera",nullptr,nullptr,"render.environment.volumes","renderer/scene_environment.cpp",Invalidate::Draw}},
}};

inline constexpr std::array<ComponentTriple, 18> environmentTriples{{
  {"color_filter","Filtro de cor",{"color_filter.r","color_filter.g","color_filter.b"},ComponentTripleKind::LinearColor},
  {"bloom_tint","Tom do bloom",{"bloom_tint.r","bloom_tint.g","bloom_tint.b"},ComponentTripleKind::LinearColor},
  {"sky_zenith","Tom do céu",{"sky_zenith.r","sky_zenith.g","sky_zenith.b"},ComponentTripleKind::LinearColor},
  {"sky_horizon","Cor do horizonte",{"sky_horizon.r","sky_horizon.g","sky_horizon.b"},ComponentTripleKind::LinearColor},
  {"ground","Cor do chão",{"ground.r","ground.g","ground.b"},ComponentTripleKind::LinearColor},
  {"fog_color","Cor da neblina",{"fog_color.r","fog_color.g","fog_color.b"},ComponentTripleKind::LinearColor},
  {"box_size","Tamanho da caixa",{"box_size.x","box_size.y","box_size.z"},ComponentTripleKind::Vector},
  {"grading_lift_color","Lift",{"grading_lift_color.r","grading_lift_color.g","grading_lift_color.b"},ComponentTripleKind::LinearColor},
  {"grading_gamma_color","Gama",{"grading_gamma_color.r","grading_gamma_color.g","grading_gamma_color.b"},ComponentTripleKind::LinearColor},
  {"grading_gain_color","Ganho",{"grading_gain_color.r","grading_gain_color.g","grading_gain_color.b"},ComponentTripleKind::LinearColor},
  {"grading_shadows_color","Sombras",{"grading_shadows_color.r","grading_shadows_color.g","grading_shadows_color.b"},ComponentTripleKind::LinearColor},
  {"grading_midtones_color","Médios",{"grading_midtones_color.r","grading_midtones_color.g","grading_midtones_color.b"},ComponentTripleKind::LinearColor},
  {"grading_highlights_color","Realces",{"grading_highlights_color.r","grading_highlights_color.g","grading_highlights_color.b"},ComponentTripleKind::LinearColor},
  {"cloud_tint","Tom",{"cloud_tint.r","cloud_tint.g","cloud_tint.b"},ComponentTripleKind::LinearColor},
  // Environment Lighting: cores no seletor sRGB, como na Unity.
  {"ambient_sky_color","Céu",{"ambient_sky_color.r","ambient_sky_color.g","ambient_sky_color.b"},ComponentTripleKind::SrgbColor},
  {"ambient_equator_color","Equador",{"ambient_equator_color.r","ambient_equator_color.g","ambient_equator_color.b"},ComponentTripleKind::SrgbColor},
  {"ambient_ground_color","Chão",{"ambient_ground_color.r","ambient_ground_color.g","ambient_ground_color.b"},ComponentTripleKind::SrgbColor},
  {"ambient_color","Cor ambiente",{"ambient_color.r","ambient_color.g","ambient_color.b"},ComponentTripleKind::SrgbColor},
}};

inline constexpr std::array<ComponentResourceBinding,3> environmentResources{{
  {"profile","Perfil",resources::AssetType::EnvironmentProfile,
   [](const ComponentValue &){return 1u;},
   [](const ComponentValue &v,u32){return static_cast<const Environment&>(v).profile;},
   [](ComponentValue &v,u32,resources::AssetGuid value){static_cast<Environment&>(v).profile=value;return true;},
   {"Geral","","Aparência compartilhada; forma, peso, prioridade e camada continuam nesta instância",
    nullptr,nullptr,"render.environment.volumes","resources/environment_profile.cpp",Invalidate::Draw|Invalidate::Policy},
   true,{}},
  {"environment_map","Mapa HDRI",resources::AssetType::EnvironmentMap,
   [](const ComponentValue &){return 1u;},
   [](const ComponentValue &v,u32){return static_cast<const Environment&>(v).values.environmentMap;},
   [](ComponentValue &v,u32,resources::AssetGuid value){static_cast<Environment&>(v).values.environmentMap=value;return true;},
   {"Céu","","Panorama HDR linear: céu, irradiância global e reflexão compartilham a mesma fonte",
    environmentUsesHdri,environmentSkyIsLocal,"render.environment.atmosphere","resources/environment_map_asset.cpp",Invalidate::Draw|Invalidate::TextureResidency},
   true,{}},
  // PK-2 (A-06): Skybox Material da janela Lighting. Com material, o céu desta
  // instância é o do recurso (compartilhado); vazio, vale o céu embutido.
  {"skybox","Material do céu",resources::AssetType::SkyboxMaterial,
   [](const ComponentValue &){return 1u;},
   [](const ComponentValue &v,u32){return static_cast<const Environment&>(v).values.skybox;},
   [](ComponentValue &v,u32,resources::AssetGuid value){static_cast<Environment&>(v).values.skybox=value;return true;},
   {"Céu","","Material de céu do projeto (Procedural, Panorâmico ou Físico); vazio usa o céu deste Ambiente",
    nullptr,nullptr,"render.environment.skybox","resources/environment_profile.cpp",Invalidate::Draw|Invalidate::TextureResidency},
   true,{}}
}};

inline const ComponentType Environment::descriptor{
  "astra.render.environment", Environment::CurrentVersion,
  []()->std::unique_ptr<ComponentValue>{return std::make_unique<Environment>();},
  environmentAllNumbers, environmentBooleans, environmentEnums, nullptr, false, {}, environmentTriples,
  environmentResources
};

} // namespace ae::scene
