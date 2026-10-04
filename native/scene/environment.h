#pragma once

#include "renderer/scene_environment.h"
#include "scene/components.h"
#include <array>

namespace ae::scene {

class Environment final : public ComponentValue {
public:
  Environment() { values.active = true; }
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
        << ' ' << values.autoExposure << ' ' << values.autoExposureCenterWeighted;
    for (const auto &property : descriptor.numbers) out << ' ' << property.read(*this);
  }
  bool read(std::istream &in, u32 version) override {
    u32 sky = 0, tone = 0;
    if ((version < 1 || version > 12) || !(in >> values.active >> values.fog >> values.post >> values.bloom >>
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
    if(!(in>>sky>>tone) || sky>(version>=7?2u:1u) || tone>(version>=9?2u:1u)) return false;
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
    if(version>=11) {
      if(!(in>>values.autoExposure>>values.autoExposureCenterWeighted)) return false;
    } else {
      values.autoExposure=false;values.autoExposureCenterWeighted=false;
      const renderer::SceneEnvironment defaults{};
      values.autoExposureMinEv=defaults.autoExposureMinEv;
      values.autoExposureMaxEv=defaults.autoExposureMaxEv;
      values.autoExposureLowPercent=defaults.autoExposureLowPercent;
      values.autoExposureHighPercent=defaults.autoExposureHighPercent;
      values.autoExposureTargetGrey=defaults.autoExposureTargetGrey;
      values.autoExposureSpeedUp=defaults.autoExposureSpeedUp;
      values.autoExposureSpeedDown=defaults.autoExposureSpeedDown;
    }
    if(version<12) values.fogLightEnergy=1.0f;
    for (const auto &property : descriptor.numbers) {
      if(version<12 && property.id=="fog_light_energy") continue;
      if(version<11 && property.id.starts_with("auto_exposure_")) continue;
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
          property.id=="exposure_ev" || property.id.starts_with("bloom_") ||
          property.id=="contrast" || property.id=="saturation" || property.id=="vignette_intensity" ||
          property.id.starts_with("indirect_"))
        if (!(in >> *property.write(*this))) return false;
    }
    return valid();
  }
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
inline bool environmentUsesHdri(const ComponentValue &value) {
  return static_cast<const Environment&>(value).values.sky==renderer::SkyModel::Hdri;
}
inline bool environmentUsesProceduralSky(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.sky != renderer::SkyModel::Hdri;
}
inline bool environmentUsesPhysicalAtmosphere(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.sky == renderer::SkyModel::PhysicalAtmosphere;
}
inline bool environmentUsesFog(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.fog;
}
inline bool environmentUsesPost(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.post;
}
inline bool environmentUsesAutoExposure(const ComponentValue &value) {
  const auto &environment=static_cast<const Environment &>(value).values;
  return environment.post&&environment.autoExposure;
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

// ComponentType recebe um span contínuo. Juntar as duas partes em um único
// array constexpr evita uma tabela paralela apenas para a paginação do editor.
inline constexpr auto environmentAllNumbers = [] {
  std::array<ComponentNumber, environmentNumbers.size() + environmentTailNumbers.size()+
      environmentAmbientOcclusionNumbers.size()+environmentVolumeNumbers.size()+environmentIndirectNumbers.size()+
      environmentPhysicalAtmosphereNumbers.size()+environmentAutoExposureNumbers.size()+environmentHdriNumbers.size()> out{};
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
  for(usize i=0;i<environmentAutoExposureNumbers.size();++i)
    out[out.size()-environmentHdriNumbers.size()-environmentAutoExposureNumbers.size()+i]=environmentAutoExposureNumbers[i];
  for(usize i=0;i<environmentHdriNumbers.size();++i)
    out[out.size()-environmentHdriNumbers.size()+i]=environmentHdriNumbers[i];
  return out;
}();

#include "scene/generated/environment_environmentBooleans.inc"

inline constexpr std::array<ComponentEnumOption, 3> skyOptions{{{0,"HDRI"},{1,"Atmosfera"},{2,"Atmosfera física"}}};
inline constexpr std::array<ComponentEnumOption, 3> toneOptions{{{0,"Reinhard"},{1,"ACES"},{2,"AgX"}}};
inline constexpr std::array<ComponentEnumOption,3> environmentShapeOptions{{{0,"Global"},{1,"Caixa"},{2,"Esfera"}}};
inline constexpr std::array<ComponentEnumOption,8> environmentLayerOptions{{
  {0,"Ambiente 0"},{1,"Ambiente 1"},{2,"Ambiente 2"},{3,"Ambiente 3"},
  {4,"Ambiente 4"},{5,"Ambiente 5"},{6,"Ambiente 6"},{7,"Ambiente 7"}
}};
inline constexpr std::array<ComponentEnum, 4> environmentEnums{{
  {"sky", "Céu", skyOptions, [](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).values.sky);}, [](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.sky=static_cast<renderer::SkyModel>(n);}, {"Atmosfera","","Fonte visual do céu",nullptr,nullptr,"render.environment.atmosphere","rhi/shaders/dirt_road_sky.frag",Invalidate::Draw}},
  {"tone_mapper", "Tonemapping", toneOptions, [](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).values.toneMapper);}, [](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.toneMapper=static_cast<renderer::ToneMapper>(n);}, {"Pós","","Curva aplicada ao HDR antes da saída",environmentUsesPost,nullptr,"render.post.tonemap","rhi/shaders/post_process_common.glsl",Invalidate::Policy}},
  {"volume_shape","Modo",environmentShapeOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).shape);},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).shape=static_cast<renderer::EnvironmentVolumeShape>(n);},{"Volume","","Global afeta toda vista; formas usam o Transform do objeto",nullptr,nullptr,"render.environment.volumes","runtime/scene_environment.cpp",Invalidate::Draw}},
  {"volume_layer","Camada",environmentLayerOptions,[](const ComponentValue &v){return static_cast<const Environment&>(v).layer;},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).layer=n;},{"Volume","","Filtro consultado pela câmera",nullptr,nullptr,"render.environment.volumes","renderer/scene_environment.cpp",Invalidate::Draw}},
}};

inline constexpr std::array<ComponentTriple, 5> environmentTriples{{
  {"sky_zenith","Cor do zênite",{"sky_zenith.r","sky_zenith.g","sky_zenith.b"},ComponentTripleKind::LinearColor},
  {"sky_horizon","Cor do horizonte",{"sky_horizon.r","sky_horizon.g","sky_horizon.b"},ComponentTripleKind::LinearColor},
  {"ground","Cor do chão",{"ground.r","ground.g","ground.b"},ComponentTripleKind::LinearColor},
  {"fog_color","Cor da neblina",{"fog_color.r","fog_color.g","fog_color.b"},ComponentTripleKind::LinearColor},
  {"box_size","Tamanho da caixa",{"box_size.x","box_size.y","box_size.z"},ComponentTripleKind::Vector},
}};

inline constexpr std::array<ComponentResourceBinding,2> environmentResources{{
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
   {"HDRI","","Panorama HDR linear: céu, irradiância global e reflexão compartilham a mesma fonte",
    environmentUsesHdri,nullptr,"render.environment.atmosphere","resources/environment_map_asset.cpp",Invalidate::Draw|Invalidate::TextureResidency},
   true,{}}
}};

inline const ComponentType Environment::descriptor{
  "astra.render.environment", 12,
  []()->std::unique_ptr<ComponentValue>{return std::make_unique<Environment>();},
  environmentAllNumbers, environmentBooleans, environmentEnums, nullptr, false, {}, environmentTriples,
  environmentResources
};

} // namespace ae::scene
