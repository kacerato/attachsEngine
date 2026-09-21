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
}

bool EnvironmentProfile::valid() const {
  if(!guid.valid()||!revision||name.empty()||name.size()>128||!values.valid()) return false;
  for(unsigned char c:name) if(c<32||c==127) return false;
  return true;
}

std::string EnvironmentProfile::serialize() const {
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<std::setprecision(std::numeric_limits<float>::max_digits10);
  out<<"ASTRA_ENVIRONMENT_PROFILE "<<FormatVersion<<' '<<guid.text()<<' '<<revision<<' '<<std::quoted(name)
     <<' '<<values.fog<<' '<<values.post<<' '<<values.bloom<<' '<<values.vignette
     <<' '<<values.filmGrain
     <<' '<<values.ambientOcclusion<<' '<<static_cast<u32>(values.sky)<<' '
     <<static_cast<u32>(values.toneMapper);
  writeColor(out,values.skyZenith);writeColor(out,values.skyHorizon);writeColor(out,values.ground);
  out<<' '<<values.atmosphere<<' '<<values.sunDiskDegrees<<' '<<values.sunDiskIntensity;
  writeColor(out,values.fogColor);
  out<<' '<<values.fogDensity<<' '<<values.fogStart<<' '<<values.exposureEv
     <<' '<<values.bloomThreshold<<' '<<values.bloomIntensity<<' '<<values.contrast
     <<' '<<values.saturation<<' '<<values.vignetteIntensity<<' '<<values.filmGrainIntensity
     <<' '<<values.ambientOcclusionRadius<<' '<<values.ambientOcclusionIntensity
     <<' '<<values.ambientOcclusionPower<<' '<<values.ambientOcclusionBias<<'\n';
  return out.str();
}

bool EnvironmentProfile::deserialize(std::string_view text,EnvironmentProfile &out) {
  if(text.size()>64u*1024u) return false;
  std::istringstream in{std::string(text)};in.imbue(std::locale::classic());
  EnvironmentProfile candidate;std::string magic,guid;u32 version=0,sky=0,tone=0;
  if(!(in>>magic>>version>>guid>>candidate.revision>>std::quoted(candidate.name)) ||
     magic!="ASTRA_ENVIRONMENT_PROFILE"||(version<1||version>FormatVersion)||
     !AssetGuid::parse(guid,candidate.guid) ||
     !(in>>candidate.values.fog>>candidate.values.post>>candidate.values.bloom>>
       candidate.values.vignette))
    return false;
  if(version>=2 && !(in>>candidate.values.filmGrain)) return false;
  if(!(in>>candidate.values.ambientOcclusion>>sky>>tone) || sky>1||tone>1) return false;
  candidate.values.active=true;candidate.values.priority=0;
  candidate.values.sky=static_cast<renderer::SkyModel>(sky);
  candidate.values.toneMapper=static_cast<renderer::ToneMapper>(tone);
  if(!readColor(in,candidate.values.skyZenith)||!readColor(in,candidate.values.skyHorizon)||
     !readColor(in,candidate.values.ground) ||
     !(in>>candidate.values.atmosphere>>candidate.values.sunDiskDegrees>>candidate.values.sunDiskIntensity) ||
     !readColor(in,candidate.values.fogColor) ||
     !(in>>candidate.values.fogDensity>>candidate.values.fogStart>>candidate.values.exposureEv>>
       candidate.values.bloomThreshold>>candidate.values.bloomIntensity>>candidate.values.contrast>>
       candidate.values.saturation>>candidate.values.vignetteIntensity)) return false;
  if(version>=2 && !(in>>candidate.values.filmGrainIntensity)) return false;
  if(!(in>>
       candidate.values.ambientOcclusionRadius>>candidate.values.ambientOcclusionIntensity>>
       candidate.values.ambientOcclusionPower>>candidate.values.ambientOcclusionBias)) return false;
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
  return normalized.fog==expected.fog&&normalized.post==expected.post&&
      normalized.bloom==expected.bloom&&normalized.vignette==expected.vignette&&
      normalized.filmGrain==expected.filmGrain&&
      normalized.ambientOcclusion==expected.ambientOcclusion&&normalized.sky==expected.sky&&
      normalized.toneMapper==expected.toneMapper&&
      std::equal(std::begin(normalized.skyZenith),std::end(normalized.skyZenith),expected.skyZenith)&&
      std::equal(std::begin(normalized.skyHorizon),std::end(normalized.skyHorizon),expected.skyHorizon)&&
      std::equal(std::begin(normalized.ground),std::end(normalized.ground),expected.ground)&&
      std::equal(std::begin(normalized.fogColor),std::end(normalized.fogColor),expected.fogColor)&&
      normalized.atmosphere==expected.atmosphere&&normalized.sunDiskDegrees==expected.sunDiskDegrees&&
      normalized.sunDiskIntensity==expected.sunDiskIntensity&&normalized.fogDensity==expected.fogDensity&&
      normalized.fogStart==expected.fogStart&&normalized.exposureEv==expected.exposureEv&&
      normalized.bloomThreshold==expected.bloomThreshold&&normalized.bloomIntensity==expected.bloomIntensity&&
      normalized.contrast==expected.contrast&&normalized.saturation==expected.saturation&&
      normalized.vignetteIntensity==expected.vignetteIntensity&&
      normalized.filmGrainIntensity==expected.filmGrainIntensity&&
      normalized.ambientOcclusionRadius==expected.ambientOcclusionRadius&&
      normalized.ambientOcclusionIntensity==expected.ambientOcclusionIntensity&&
      normalized.ambientOcclusionPower==expected.ambientOcclusionPower&&
      normalized.ambientOcclusionBias==expected.ambientOcclusionBias;
}
} // namespace ae::resources
