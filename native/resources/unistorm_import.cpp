#include "resources/unistorm_import.h"

#include <algorithm>
#include <cmath>

namespace ae::resources {
namespace {
using Node = UnityYamlNode;
float num(const Node &node, std::string_view key, float fallback) {
  double value = 0;
  const auto *found = node.get(key);
  return found && found->number(value) && std::isfinite(value) ? static_cast<float>(value) : fallback;
}
i64 integer(const Node &node, std::string_view key, i64 fallback) {
  i64 value = 0;
  const auto *found = node.get(key);
  return found && found->integer(value) ? value : fallback;
}
// Yes_No do UniStorm: Yes = 0, No = 1.
bool yes(const Node &node, std::string_view key, bool fallback) {
  const auto *found = node.get(key);
  i64 value = 0;
  return found && found->integer(value) ? value == 0 : fallback;
}
const UnityYamlObject *behaviour(const UnityYamlDocument &document, std::string_view script) {
  for (const auto &object : document.objects) {
    if (object.classId != 114) continue;
    UnityObjectReference reference;
    const auto *node = object.value.get("m_Script");
    if (node && unityObjectReference(*node, reference) && reference.guid.text() == script) return &object;
  }
  return nullptr;
}
std::string text(const Node &node, std::string_view key) {
  const auto *found = node.get(key);
  return found && found->kind == Node::Kind::Scalar ? found->scalar : std::string();
}
} // namespace

bool uniStormScript(const UnityYamlDocument &document, std::string &guid) {
  for (const auto &object : document.objects) {
    if (object.classId != 114) continue;
    UnityObjectReference reference;
    const auto *node = object.value.get("m_Script");
    if (node && unityObjectReference(*node, reference) && reference.external()) { guid = reference.guid.text(); return true; }
  }
  return false;
}

bool readUniStormWeather(const UnityYamlDocument &document, UniStormWeather &out, std::string &diagnostic) {
  const auto *object = behaviour(document, UniStormWeatherTypeScript);
  if (!object) { diagnostic = "Não é um Weather Type do UniStorm."; return false; }
  const auto &n = object->value;
  UniStormWeather result;
  result.name = text(n, "WeatherTypeName");
  if (result.name.empty()) result.name = text(n, "m_Name");
  if (result.name.empty() || result.name.size() > 80) { diagnostic = "Weather Type do UniStorm sem nome."; return false; }
  auto &w = result.weather;
  // Tipo: precipitação com relâmpago é tempestade; abaixo de zero é neve;
  // sem precipitação, o nível de nuvens decide entre limpo e nublado.
  const bool precipitation = yes(n, "PrecipitationWeatherType", false);
  const bool lightning = yes(n, "UseLightning", false);
  const i64 temperature = integer(n, "TemperatureType", 1);  // 0 abaixo de zero, 1 acima, 2 ambos
  const i64 cloudLevel = integer(n, "CloudLevel", 0);        // 0 limpo .. 4 nublado, 5 não muda
  const float amount = std::max(0.0f, num(n, "ParticleEffectAmount", 200));
  if (precipitation) {
    w.kind = lightning ? scene::WeatherKind::Storm : temperature == 0 ? scene::WeatherKind::Snow : scene::WeatherKind::Rain;
    // Quantidade de partículas do efeito: 1000 é a chuva forte do pacote.
    w.intensity = std::clamp(amount / 1000.0f, 0.15f, 1.0f);
  } else {
    constexpr float levels[]{.1f, .3f, .5f, .7f, .9f};
    w.kind = cloudLevel >= 3 && cloudLevel <= 4 ? scene::WeatherKind::Cloudy : scene::WeatherKind::Clear;
    w.intensity = cloudLevel >= 0 && cloudLevel <= 4 ? levels[cloudLevel] : .5f;
    if (cloudLevel == 5) result.approximations.push_back("Nível de nuvens \"não muda\" vira nublado médio.");
    if (lightning) result.approximations.push_back("Relâmpago sem chuva (Dry Thunder) só existe na Tempestade da Astra.");
  }
  w.lightning = lightning;
  // Escala da neblina: a densidade de chuva do pacote (0,005) é a de chuva da Astra (0,045).
  w.fogDensity = std::clamp(num(n, "FogDensity", .005f) * 9.0f, 0.0f, 0.2f);
  w.windInfluence = std::clamp(num(n, "WindIntensity", .25f), 0.0f, 2.0f);
  const float rate = std::clamp(amount * 7.0f, 0.0f, 20000.0f);
  if (w.kind == scene::WeatherKind::Snow) w.snowRate = rate;
  else if (precipitation) w.rainRate = rate;
  const i64 shader = integer(n, "ShaderControl", 2);  // 0 chuva, 1 neve, 2 nenhum
  w.surfaceWetness = shader == 0 ? 1.0f : 0.0f;
  w.surfacePuddles = shader == 0 ? .6f : 0.0f;
  w.surfaceSnow = shader == 1 ? 1.0f : 0.0f;
  if (!yes(n, "UseWeatherEffect", false) && precipitation)
    result.approximations.push_back("O tipo não liga partícula de efeito; o Clima dosa a Chuva/Neve ligadas a ele.");
  if (yes(n, "UseAuroras", false)) result.approximations.push_back("Auroras sem consumidor no céu da Astra.");
  if (yes(n, "OverrideFogColor", false) || yes(n, "OverrideCloudColor", false))
    result.approximations.push_back("Gradientes de cor da neblina e das nuvens pela hora ficam com as cores do Clima.");
  if (const auto *node = n.get("WeatherEffect")) unityObjectReference(*node, result.effect);
  if (const auto *node = n.get("AdditionalWeatherEffect")) unityObjectReference(*node, result.additionalEffect);
  if (const auto *node = n.get("WeatherSound")) unityObjectReference(*node, result.sound);
  if (const auto *node = n.get("CloudProfileComponent")) unityObjectReference(*node, result.cloudProfile);
  result.useSound = yes(n, "UseWeatherSound", false) && result.sound.external();
  result.soundVolume = std::clamp(num(n, "WeatherVolume", 1), 0.0f, 1.0f);
  if (!w.valid()) { diagnostic = "Valores do clima fora do contrato do Clima."; return false; }
  out = std::move(result);
  diagnostic.clear();
  return true;
}

bool readUniStormCloud(const UnityYamlDocument &document, UniStormCloud &out, std::string &diagnostic) {
  const auto *object = behaviour(document, UniStormCloudProfileScript);
  if (!object) { diagnostic = "Não é um Cloud Profile do UniStorm."; return false; }
  const auto &n = object->value;
  UniStormCloud result;
  result.name = text(n, "m_Name");
  if (result.name.empty()) result.name = text(n, "ProfileName");
  if (result.name.empty() || result.name.size() > 80) { diagnostic = "Cloud Profile do UniStorm sem nome."; return false; }
  auto &c = result.clouds;
  c.enabled = true;
  // O perfil volumétrico do pacote vira a camada 2D da Astra: densidade igual,
  // o viés de cobertura desloca a cobertura média e o detalhe vira os cirros.
  c.density = std::clamp(num(n, "Density", .9f), 0.0f, 1.0f);
  c.coverage = std::clamp(.55f + num(n, "CoverageBias", .0175f) * 4.0f, 0.0f, 1.0f);
  c.cirrus = std::clamp(num(n, "DetailStrength", .114f) * 3.0f, 0.0f, 1.0f);
  const float softness = std::clamp(num(n, "EdgeSoftness", .067f) + num(n, "BaseSoftness", .13f), 0.0f, 1.0f);
  c.opacity = std::clamp(1.0f - softness * .25f, .5f, 1.0f);
  result.approximations.push_back("Nuvens volumétricas do UniStorm viram a camada de nuvens da Astra (sem raymarch).");
  if (!c.valid()) { diagnostic = "Nuvens fora do contrato do Cloud Layer."; return false; }
  out = std::move(result);
  diagnostic.clear();
  return true;
}

} // namespace ae::resources
