#pragma once
// G6b: dados do UniStorm 5.3 (Black Horizon Studios) como dados da Astra.
// Só os valores: os scripts e shaders do pacote nunca são importados como
// código. Formato estudado: os ScriptableObject WeatherType e CloudProfile
// serializados em texto (YAML da Unity, MonoBehaviour com m_Script).
//
//  - WeatherType → valores do componente Clima (scene/weather.h): tipo,
//    intensidade, neblina, vento, relâmpago, taxa de chuva/neve e superfícies.
//    A Astra não tem um recurso "perfil de clima": o equivalente prático é o
//    preset do Clima, que o editor grava na biblioteca de presets do projeto.
//  - CloudProfile → o Cloud Layer de um perfil de Ambiente (só nuvens).
// As referências do tipo (partícula de chuva, som, ícone) voltam como GUID da
// Unity para quem chama ligar aos recursos convertidos.
#include "renderer/scene_environment.h"
#include "resources/unity_yaml.h"
#include "scene/weather.h"

#include <string>
#include <vector>

namespace ae::resources {

inline constexpr std::string_view UniStormWeatherTypeScript = "e4ec900128fbff24683f8f2aac8c2192";
inline constexpr std::string_view UniStormCloudProfileScript = "e61fb300c8082e942917c8b932209ad4";

struct UniStormWeather {
  std::string name;
  scene::Weather weather;
  UnityObjectReference effect, additionalEffect, sound, cloudProfile;
  // ParticleEffectVector e AdditionalParticleEffectVector: onde o UniStorm põe
  // cada efeito em relação ao jogador (eixos da Unity, Z ainda não espelhado).
  float effectOffset[3]{0, 28, 0}, additionalOffset[3]{0, 28, 0};
  // ParticleEffectAmount e AdditionalParticleEffectAmount: a taxa de emissão
  // que o sistema escreve no efeito (o prefab vem com 0).
  float effectAmount = 0, additionalAmount = 0;
  bool useSound = false;
  float soundVolume = 1;
  std::vector<std::string> approximations;
};
struct UniStormCloud {
  std::string name;
  renderer::CloudLayer clouds;
  std::vector<std::string> approximations;
};

// Falso quando o YAML não é um WeatherType/CloudProfile do UniStorm.
bool readUniStormWeather(const UnityYamlDocument &document, UniStormWeather &out, std::string &diagnostic);
bool readUniStormCloud(const UnityYamlDocument &document, UniStormCloud &out, std::string &diagnostic);
// O script do primeiro MonoBehaviour do documento (para a Biblioteca escolher).
bool uniStormScript(const UnityYamlDocument &document, std::string &guid);

} // namespace ae::resources
