#pragma once
// G4c: clima da cena, do jeito prático da Unity -- um script de clima que muda o
// Cloud Layer de um Volume e dosa Particle Systems de chuva e neve presos à
// câmera. O componente scene::Weather vive no objeto de um Ambiente; aqui ele
// ESCREVE propriedades visíveis: nuvens e neblina desse Ambiente, a taxa de
// emissão dos sistemas de Chuva e Neve e a posição deles sobre a câmera.
//
// Fora do Play o clima é instantâneo (como um [ExecuteAlways]); em Play passa
// pela Transição e a Tempestade acende relâmpagos. Só escreve quando o valor
// muda: um clima parado não altera a revisão do grafo.
#include "runtime/scene_graph.h"
#include "scene/weather.h"

#include <functional>
#include <map>
#include <utility>

namespace ae::runtime {

// O que um tipo de clima pede às nuvens, à neblina e à precipitação.
struct WeatherLook {
  float coverage = 0, cirrus = 0, density = 0, tint = 1;
  float rain = 0, snow = 0;  // fração da emissão máxima
  float fog = 0;             // fração da Neblina máxima
  float storm = 0;           // 1 na Tempestade (relâmpagos)
};
// Alvos de cada tipo na intensidade dada. Os pontos fixos seguem a ideia dos
// presets Sparse/Cloudy/Overcast/Stormy das Volumetric Clouds do HDRP 17.
WeatherLook weatherTarget(scene::WeatherKind kind, float intensity);
// Clarão do relâmpago (0..1) no instante dado: um raio por janela de
// `interval` segundos, em posição sorteada pela semente, com dois pulsos.
float lightningFlash(u32 seed, double seconds, float interval);

// Câmera da cena que a precipitação acompanha: posição de mundo e guinada
// (0 olha para +Z), como o filho da Main Camera na Unity.
struct WeatherCamera {
  float position[3]{};
  float yaw = 0;
};

class SceneWeather final {
public:
  struct Report {
    u32 weathers = 0, writes = 0;
    float flash = 0;
  };
  void reset() { states_.clear(); }
  // `live`: Play (transição e relâmpagos). `camera`: posição de mundo da câmera
  // da cena, ou nulo sem câmera. `writeTransform` grava a pose dos emissores:
  // documento no editor, GameWorld no Play (respeita a autoridade do transform).
  Report advance(SceneGraph &graph, double seconds, float deltaSeconds, bool live, const WeatherCamera *camera,
                 const std::function<bool(ObjectId, const Transform &)> &writeTransform);
  // Estado atual (já misturado) de um clima: para o Inspector e os testes.
  bool current(ObjectId object, u64 instance, WeatherLook &out) const;

private:
  struct State {
    WeatherLook look;
    bool initialized = false;
  };
  std::map<std::pair<ObjectId, u64>, State> states_;
};

} // namespace ae::runtime
