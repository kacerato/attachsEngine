#include "resources/environment_profile.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace ae::resources {
namespace {
void writeColor(std::ostream &out,const float value[3]) {
  out<<' '<<value[0]<<' '<<value[1]<<' '<<value[2];
}
bool readColor(std::istream &in,float value[3]) {
  return static_cast<bool>(in>>value[0]>>value[1]>>value[2]);
}
// v9 (G2): gradação de cor no fim da linha. Perfis antigos ficam neutros.
void writeGrading(std::ostream &out,const renderer::ColorGrading &g) {
  out<<' '<<g.temperature<<' '<<g.tint;
  writeColor(out,g.liftColor);out<<' '<<g.lift;
  writeColor(out,g.gammaColor);out<<' '<<g.gamma;
  writeColor(out,g.gainColor);out<<' '<<g.gain;
  writeColor(out,g.shadowsColor);out<<' '<<g.shadows;
  writeColor(out,g.midtonesColor);out<<' '<<g.midtones;
  writeColor(out,g.highlightsColor);out<<' '<<g.highlights;
  out<<' '<<g.shadowsStart<<' '<<g.shadowsEnd<<' '<<g.highlightsStart<<' '<<g.highlightsEnd
     <<' '<<g.vibrance;
}
bool readGrading(std::istream &in,renderer::ColorGrading &g) {
  return static_cast<bool>(in>>g.temperature>>g.tint) &&
         readColor(in,g.liftColor)&&static_cast<bool>(in>>g.lift) &&
         readColor(in,g.gammaColor)&&static_cast<bool>(in>>g.gamma) &&
         readColor(in,g.gainColor)&&static_cast<bool>(in>>g.gain) &&
         readColor(in,g.shadowsColor)&&static_cast<bool>(in>>g.shadows) &&
         readColor(in,g.midtonesColor)&&static_cast<bool>(in>>g.midtones) &&
         readColor(in,g.highlightsColor)&&static_cast<bool>(in>>g.highlights) &&
         static_cast<bool>(in>>g.shadowsStart>>g.shadowsEnd>>g.highlightsStart>>g.highlightsEnd>>g.vibrance);
}
}

bool EnvironmentProfile::valid() const {
  if(!guid.valid()||!revision||name.empty()||name.size()>128||!values.valid()) return false;
  for(unsigned char c:name) if(c<32||c==127) return false;
  return true;
}

std::string EnvironmentProfile::serialize() const {
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<std::setprecision(std::numeric_limits<float>::max_digits10);
  out<<(skybox?"ASTRA_SKYBOX_MATERIAL ":"ASTRA_ENVIRONMENT_PROFILE ")<<FormatVersion<<' '<<guid.text()<<' '<<revision<<' '<<std::quoted(name)
     <<' '<<values.fog<<' '<<values.post<<' '<<values.bloom<<' '<<values.vignette
     <<' '<<values.filmGrain
     <<' '<<values.ambientOcclusion<<' '<<static_cast<u32>(values.sky)<<' '
     <<static_cast<u32>(values.toneMapper)<<' '<<values.physicalAtmosphereHighQuality;
  writeColor(out,values.skyZenith);writeColor(out,values.skyHorizon);writeColor(out,values.ground);
  out<<' '<<values.atmosphere<<' '<<values.sunDiskDegrees<<' '<<values.sunDiskIntensity;
  writeColor(out,values.fogColor);
  out<<' '<<values.fogDensity<<' '<<values.fogStart<<' '<<values.fogBaseHeight
     <<' '<<values.fogHeightFalloff<<' '<<values.exposureCompensation
     <<' '<<values.bloomThreshold<<' '<<values.bloomIntensity<<' '<<values.contrast
     <<' '<<values.saturation<<' '<<values.vignetteIntensity<<' '<<values.filmGrainIntensity
     <<' '<<values.ambientOcclusionRadius<<' '<<values.ambientOcclusionIntensity
     <<' '<<values.ambientOcclusionPower<<' '<<values.ambientOcclusionBias
     <<' '<<values.physicalSkyIntensity<<' '<<values.airDensity<<' '<<values.aerosolDensity
     <<' '<<values.aerosolAnisotropy<<' '<<values.planetRadiusKm<<' '<<values.observerHeightKm
     <<' '<<values.rayleighScaleHeightKm<<' '<<values.aerosolScaleHeightKm
     <<' '<<values.atmosphereHeightKm<<' '<<values.groundAlbedo
     <<' '<<(values.environmentMap.valid()?values.environmentMap.text():"-")
     <<' '<<values.hdriRotationDegrees<<' '<<values.hdriExposureEv
     <<' '<<static_cast<u32>(values.exposureMode)<<' '<<static_cast<u32>(values.exposureMetering)
     <<' '<<static_cast<u32>(values.exposureAdaptation)<<' '<<values.fixedExposure
     <<' '<<values.exposureLimitMin<<' '<<values.exposureLimitMax
     <<' '<<values.exposureHistogramLow<<' '<<values.exposureHistogramHigh
     <<' '<<values.exposureTargetMidGrey<<' '<<values.exposureSpeedDarkToLight
     <<' '<<values.exposureSpeedLightToDark
     <<' '<<values.fogLightEnergy;
  writeGrading(out,values.grading);
  out<<' '<<values.sunGlare<<' '<<values.moonPhase<<' '<<values.moonSizeDegrees
     <<' '<<values.moonIntensity<<' '<<values.starIntensity;
  const auto &clouds=values.clouds;
  out<<' '<<clouds.enabled<<' '<<clouds.castShadows<<' '<<clouds.opacity<<' '<<clouds.coverage
     <<' '<<clouds.cirrus<<' '<<clouds.altitudeMeters<<' '<<clouds.sizeMeters;
  writeColor(out,clouds.tint);
  out<<' '<<clouds.exposureEv<<' '<<clouds.windOrientationDegrees<<' '<<clouds.windSpeedKmh
     <<' '<<clouds.density<<' '<<clouds.ambientDimmer<<' '<<clouds.shadowMultiplier;
  // v12: Color Adjustments e Bloom completos, no fim da linha.
  out<<' '<<values.postExposure;writeColor(out,values.colorFilter);out<<' '<<values.hueShift
     <<' '<<values.bloomScatter;writeColor(out,values.bloomTint);out<<' '<<values.bloomClamp;
  // v13 (PK-1/PK-2): janela Lighting e Skybox/Procedural.
  out<<' '<<values.skyExposure<<' '<<static_cast<u32>(values.sunDisk)<<' '<<values.sunSize
     <<' '<<values.sunSizeConvergence<<' '<<static_cast<u32>(values.ambientSource);
  writeColor(out,values.ambientSkyColor);writeColor(out,values.ambientEquatorColor);
  writeColor(out,values.ambientGroundColor);writeColor(out,values.ambientColor);
  out<<' '<<static_cast<u32>(values.fogMode)<<' '<<static_cast<u32>(values.fogColorMode)<<' '<<values.fogEnd
     <<' '<<(values.skybox.valid()?values.skybox.text():"-");
  // v14 (G4c/G4d): espessura das nuvens e Volumetric Clouds.
  out<<' '<<clouds.thicknessMeters<<' '<<clouds.type<<' '<<clouds.shapeFactor<<' '<<clouds.erosion<<' '<<clouds.steps;
  out<<'\n';
  return out.str();
}

bool EnvironmentProfile::deserialize(std::string_view text,EnvironmentProfile &out) {
  if(text.size()>64u*1024u) return false;
  std::istringstream in{std::string(text)};in.imbue(std::locale::classic());
  EnvironmentProfile candidate;std::string magic,guid;u32 version=0,sky=0,tone=0;
  if(!(in>>magic>>version>>guid>>candidate.revision>>std::quoted(candidate.name)) ||
     (magic!="ASTRA_ENVIRONMENT_PROFILE"&&!(magic=="ASTRA_SKYBOX_MATERIAL"&&version>=13))||
     (version<1||version>FormatVersion)||
     !AssetGuid::parse(guid,candidate.guid) ||
     !(in>>candidate.values.fog>>candidate.values.post>>candidate.values.bloom>>
       candidate.values.vignette))
    return false;
  if(version>=2 && !(in>>candidate.values.filmGrain)) return false;
  if(!(in>>candidate.values.ambientOcclusion>>sky>>tone) || sky>(version>=3?2u:1u)||
     tone>(version>=12?4u:version>=5?2u:1u)) return false;
  if(version<12 && tone==static_cast<u32>(renderer::ToneMapper::Reinhard)) tone=static_cast<u32>(renderer::ToneMapper::Neutral);
  if(version>=3 && !(in>>candidate.values.physicalAtmosphereHighQuality)) return false;
  candidate.values.active=true;candidate.values.priority=0;
  candidate.values.sky=static_cast<renderer::SkyModel>(sky);
  candidate.values.toneMapper=static_cast<renderer::ToneMapper>(tone);
  if(!readColor(in,candidate.values.skyZenith)||!readColor(in,candidate.values.skyHorizon)||
     !readColor(in,candidate.values.ground) ||
     !(in>>candidate.values.atmosphere>>candidate.values.sunDiskDegrees>>candidate.values.sunDiskIntensity) ||
     !readColor(in,candidate.values.fogColor) ||
     !(in>>candidate.values.fogDensity>>candidate.values.fogStart)) return false;
  if(version>=6 && !(in>>candidate.values.fogBaseHeight>>candidate.values.fogHeightFalloff)) return false;
  if(!(in>>candidate.values.exposureCompensation>>
       candidate.values.bloomThreshold>>candidate.values.bloomIntensity>>candidate.values.contrast>>
       candidate.values.saturation>>candidate.values.vignetteIntensity)) return false;
  if(version>=2 && !(in>>candidate.values.filmGrainIntensity)) return false;
  if(!(in>>
       candidate.values.ambientOcclusionRadius>>candidate.values.ambientOcclusionIntensity>>
       candidate.values.ambientOcclusionPower>>candidate.values.ambientOcclusionBias)) return false;
  if(version>=3 && !(in>>candidate.values.physicalSkyIntensity>>candidate.values.airDensity>>
       candidate.values.aerosolDensity>>candidate.values.aerosolAnisotropy>>
       candidate.values.planetRadiusKm>>candidate.values.observerHeightKm>>
       candidate.values.rayleighScaleHeightKm>>candidate.values.aerosolScaleHeightKm>>
       candidate.values.atmosphereHeightKm>>candidate.values.groundAlbedo)) return false;
  if(version>=4) {
    std::string map;
    if(!(in>>map>>candidate.values.hdriRotationDegrees>>candidate.values.hdriExposureEv) ||
       (map!="-"&&!AssetGuid::parse(map,candidate.values.environmentMap))) return false;
  }
  // v12: Exposure do HDRP (modo, metering, adaptação, EV100). Da v7 à v11 o
  // arquivo trazia a exposição automática antiga, convertida no fim.
  bool legacyAuto=false,legacyCenter=false;
  auto &v=candidate.values;
  if(version>=12) {
    u32 mode=0,metering=0,adaptation=0;
    if(!(in>>mode>>metering>>adaptation>>v.fixedExposure>>v.exposureLimitMin>>v.exposureLimitMax>>
         v.exposureHistogramLow>>v.exposureHistogramHigh>>v.exposureTargetMidGrey>>
         v.exposureSpeedDarkToLight>>v.exposureSpeedLightToDark) || mode>2 || metering>2 || adaptation>1)
      return false;
    v.exposureMode=static_cast<renderer::ExposureMode>(mode);
    v.exposureMetering=static_cast<renderer::ExposureMetering>(metering);
    v.exposureAdaptation=static_cast<renderer::ExposureAdaptation>(adaptation);
  } else if(version>=7 && !(in>>legacyAuto>>v.exposureLimitMin>>v.exposureLimitMax>>
      v.exposureHistogramLow>>v.exposureHistogramHigh>>v.exposureTargetMidGrey>>
      v.exposureSpeedDarkToLight>>v.exposureSpeedLightToDark>>legacyCenter)) return false;
  if(version>=8 && !(in>>candidate.values.fogLightEnergy)) return false;
  if(version>=9 && !readGrading(in,candidate.values.grading)) return false;
  // v10 (G4): corpos celestes. Perfil antigo fica com os padrões novos.
  if(version>=10 && !(in>>candidate.values.sunGlare>>candidate.values.moonPhase>>
                      candidate.values.moonSizeDegrees>>candidate.values.moonIntensity>>
                      candidate.values.starIntensity)) return false;
  // v11 (G4b): Cloud Layer. Perfil antigo fica sem nuvens.
  if(version>=11) {
    auto &clouds=candidate.values.clouds;
    if(!(in>>clouds.enabled>>clouds.castShadows>>clouds.opacity>>clouds.coverage>>clouds.cirrus>>
         clouds.altitudeMeters>>clouds.sizeMeters) || !readColor(in,clouds.tint) ||
       !(in>>clouds.exposureEv>>clouds.windOrientationDegrees>>clouds.windSpeedKmh>>clouds.density>>
         clouds.ambientDimmer>>clouds.shadowMultiplier)) return false;
  }
  if(version>=12) {
    auto &x=candidate.values;
    if(!(in>>x.postExposure)||!readColor(in,x.colorFilter)||!(in>>x.hueShift>>x.bloomScatter)||
       !readColor(in,x.bloomTint)||!(in>>x.bloomClamp)) return false;
  } else candidate.values.bloomScatter=0; // perfil antigo: o raio de bloom de antes
  if(version<12) renderer::migrateLegacyExposure(v,version>=7,legacyAuto,legacyCenter);
  candidate.skybox=magic=="ASTRA_SKYBOX_MATERIAL";
  if(version>=13) {
    auto &w=candidate.values;u32 sun=0,ambient=0,fogMode=0,fogColor=0;std::string skybox;
    if(!(in>>w.skyExposure>>sun>>w.sunSize>>w.sunSizeConvergence>>ambient) ||
       !readColor(in,w.ambientSkyColor)||!readColor(in,w.ambientEquatorColor)||
       !readColor(in,w.ambientGroundColor)||!readColor(in,w.ambientColor)||
       !(in>>fogMode>>fogColor>>w.fogEnd>>skybox)||sun>2||ambient>2||fogMode>2||fogColor>1||
       (skybox!="-"&&!AssetGuid::parse(skybox,w.skybox))) return false;
    w.sunDisk=static_cast<renderer::SunDiskMode>(sun);w.ambientSource=static_cast<renderer::AmbientSource>(ambient);
    w.fogMode=static_cast<renderer::FogMode>(fogMode);w.fogColorMode=static_cast<renderer::FogColorMode>(fogColor);
  } else {
    // Perfil antigo: o céu e a neblina de antes (como a migração do Ambiente v18).
    candidate.values.atmosphere=1.0f;
    candidate.values.sunDisk=candidate.values.sunDiskIntensity>0.0f?renderer::SunDiskMode::HighQuality:renderer::SunDiskMode::None;
    candidate.values.fogMode=renderer::FogMode::Exponential;
    candidate.values.fogColorMode=renderer::FogColorMode::SkyLight;
  }
  if(version>=14) {
    auto &c=candidate.values.clouds;
    if(!(in>>c.thicknessMeters>>c.type>>c.shapeFactor>>c.erosion>>c.steps)) return false;
  }
  in>>std::ws;if(!in.eof()||!candidate.valid()) return false;
  out=std::move(candidate);return true;
}

renderer::SceneEnvironment applyEnvironmentProfile(const renderer::SceneEnvironment &instance,
                                                    const EnvironmentProfile &profile) {
  auto result=profile.values;
  result.active=instance.active;result.priority=instance.priority;
  // Luz indireta é do LUGAR (a sala escura, o pátio aberto), não da aparência
  // compartilhada: fica com a instância, como a prioridade.
  result.indirectDiffuse=instance.indirectDiffuse;result.indirectSpecular=instance.indirectSpecular;
  return result;
}

bool sameEnvironmentAppearance(const renderer::SceneEnvironment &instance,
                               const EnvironmentProfile &profile) {
  auto normalized=instance;normalized.active=true;normalized.priority=0;
  auto expected=profile.values;expected.active=true;expected.priority=0;
  return normalized.skyExposure==expected.skyExposure&&normalized.sunDisk==expected.sunDisk&&
      normalized.sunSize==expected.sunSize&&normalized.sunSizeConvergence==expected.sunSizeConvergence&&
      normalized.ambientSource==expected.ambientSource&&
      std::equal(std::begin(normalized.ambientSkyColor),std::end(normalized.ambientSkyColor),expected.ambientSkyColor)&&
      std::equal(std::begin(normalized.ambientEquatorColor),std::end(normalized.ambientEquatorColor),expected.ambientEquatorColor)&&
      std::equal(std::begin(normalized.ambientGroundColor),std::end(normalized.ambientGroundColor),expected.ambientGroundColor)&&
      std::equal(std::begin(normalized.ambientColor),std::end(normalized.ambientColor),expected.ambientColor)&&
      normalized.fogMode==expected.fogMode&&normalized.fogColorMode==expected.fogColorMode&&
      normalized.fogEnd==expected.fogEnd&&normalized.skybox==expected.skybox&&
      normalized.environmentMap==expected.environmentMap&&
      normalized.hdriRotationDegrees==expected.hdriRotationDegrees&&normalized.hdriExposureEv==expected.hdriExposureEv&&
      normalized.fog==expected.fog&&normalized.post==expected.post&&
      normalized.bloom==expected.bloom&&normalized.vignette==expected.vignette&&
      normalized.filmGrain==expected.filmGrain&&
      normalized.ambientOcclusion==expected.ambientOcclusion&&normalized.sky==expected.sky&&
      normalized.toneMapper==expected.toneMapper&&
      std::equal(std::begin(normalized.skyZenith),std::end(normalized.skyZenith),expected.skyZenith)&&
      std::equal(std::begin(normalized.skyHorizon),std::end(normalized.skyHorizon),expected.skyHorizon)&&
      std::equal(std::begin(normalized.ground),std::end(normalized.ground),expected.ground)&&
      std::equal(std::begin(normalized.fogColor),std::end(normalized.fogColor),expected.fogColor)&&
      normalized.atmosphere==expected.atmosphere&&normalized.sunDiskDegrees==expected.sunDiskDegrees&&
      normalized.sunDiskIntensity==expected.sunDiskIntensity&&
      normalized.physicalAtmosphereHighQuality==expected.physicalAtmosphereHighQuality&&
      normalized.physicalSkyIntensity==expected.physicalSkyIntensity&&
      normalized.airDensity==expected.airDensity&&normalized.aerosolDensity==expected.aerosolDensity&&
      normalized.aerosolAnisotropy==expected.aerosolAnisotropy&&
      normalized.planetRadiusKm==expected.planetRadiusKm&&
      normalized.observerHeightKm==expected.observerHeightKm&&
      normalized.rayleighScaleHeightKm==expected.rayleighScaleHeightKm&&
      normalized.aerosolScaleHeightKm==expected.aerosolScaleHeightKm&&
      normalized.atmosphereHeightKm==expected.atmosphereHeightKm&&
      normalized.groundAlbedo==expected.groundAlbedo&&normalized.fogDensity==expected.fogDensity&&
      normalized.fogStart==expected.fogStart&&normalized.fogBaseHeight==expected.fogBaseHeight&&
      normalized.fogHeightFalloff==expected.fogHeightFalloff&&
      normalized.fogLightEnergy==expected.fogLightEnergy&&
      normalized.exposureMode==expected.exposureMode&&normalized.exposureMetering==expected.exposureMetering&&
      normalized.exposureAdaptation==expected.exposureAdaptation&&
      normalized.fixedExposure==expected.fixedExposure&&
      normalized.exposureCompensation==expected.exposureCompensation&&
      normalized.exposureLimitMin==expected.exposureLimitMin&&
      normalized.exposureLimitMax==expected.exposureLimitMax&&
      normalized.exposureHistogramLow==expected.exposureHistogramLow&&
      normalized.exposureHistogramHigh==expected.exposureHistogramHigh&&
      normalized.exposureTargetMidGrey==expected.exposureTargetMidGrey&&
      normalized.exposureSpeedDarkToLight==expected.exposureSpeedDarkToLight&&
      normalized.exposureSpeedLightToDark==expected.exposureSpeedLightToDark&&
      normalized.bloomThreshold==expected.bloomThreshold&&normalized.bloomIntensity==expected.bloomIntensity&&
      normalized.contrast==expected.contrast&&normalized.saturation==expected.saturation&&
      normalized.vignetteIntensity==expected.vignetteIntensity&&
      normalized.filmGrainIntensity==expected.filmGrainIntensity&&
      normalized.ambientOcclusionRadius==expected.ambientOcclusionRadius&&
      normalized.ambientOcclusionIntensity==expected.ambientOcclusionIntensity&&
      normalized.ambientOcclusionPower==expected.ambientOcclusionPower&&
      normalized.ambientOcclusionBias==expected.ambientOcclusionBias&&
      normalized.grading==expected.grading&&
      normalized.sunGlare==expected.sunGlare&&normalized.moonPhase==expected.moonPhase&&
      normalized.moonSizeDegrees==expected.moonSizeDegrees&&
      normalized.moonIntensity==expected.moonIntensity&&normalized.starIntensity==expected.starIntensity&&
      normalized.clouds==expected.clouds&&
      normalized.postExposure==expected.postExposure&&normalized.hueShift==expected.hueShift&&
      std::equal(std::begin(normalized.colorFilter),std::end(normalized.colorFilter),expected.colorFilter)&&
      normalized.bloomScatter==expected.bloomScatter&&normalized.bloomClamp==expected.bloomClamp&&
      std::equal(std::begin(normalized.bloomTint),std::end(normalized.bloomTint),expected.bloomTint);
}
renderer::SceneEnvironment applySkyboxMaterial(const renderer::SceneEnvironment &instance,
                                               const EnvironmentProfile &material) {
  auto result=instance;const auto &sky=material.values;
  result.sky=sky.sky;result.environmentMap=sky.environmentMap;
  result.hdriRotationDegrees=sky.hdriRotationDegrees;result.hdriExposureEv=sky.hdriExposureEv;
  std::copy_n(sky.skyZenith,3,result.skyZenith);std::copy_n(sky.ground,3,result.ground);
  result.atmosphere=sky.atmosphere;result.skyExposure=sky.skyExposure;
  result.sunDisk=sky.sunDisk;result.sunSize=sky.sunSize;result.sunSizeConvergence=sky.sunSizeConvergence;
  result.sunDiskDegrees=sky.sunDiskDegrees;result.sunDiskIntensity=sky.sunDiskIntensity;result.sunGlare=sky.sunGlare;
  result.moonPhase=sky.moonPhase;result.moonSizeDegrees=sky.moonSizeDegrees;
  result.moonIntensity=sky.moonIntensity;result.starIntensity=sky.starIntensity;
  result.physicalAtmosphereHighQuality=sky.physicalAtmosphereHighQuality;
  result.physicalSkyIntensity=sky.physicalSkyIntensity;result.airDensity=sky.airDensity;
  result.aerosolDensity=sky.aerosolDensity;result.aerosolAnisotropy=sky.aerosolAnisotropy;
  result.planetRadiusKm=sky.planetRadiusKm;result.observerHeightKm=sky.observerHeightKm;
  result.rayleighScaleHeightKm=sky.rayleighScaleHeightKm;result.aerosolScaleHeightKm=sky.aerosolScaleHeightKm;
  result.atmosphereHeightKm=sky.atmosphereHeightKm;result.groundAlbedo=sky.groundAlbedo;
  result.skybox=material.guid;
  return result;
}

renderer::SceneEnvironment skyboxMaterialValues(const renderer::SceneEnvironment &values) {
  auto result=applySkyboxMaterial(renderer::SceneEnvironment{},EnvironmentProfile{{},1,{},true,values});
  result.active=true;result.priority=0;result.skybox={};
  return result;
}
} // namespace ae::resources
