#pragma once

#include "core/base.h"
#include "renderer/water_surface.h"

#include <span>
#include <vector>

namespace ae::renderer {

// Ondas dinâmicas: a superfície reagindo ao que acontece nela.
//
// O espectro FFT descreve um mar em regime — vento, pista, profundidade — e não
// tem como saber que um casco passou. Hoje a engine cobre interação com oito
// impulsos analíticos (`WaterInteractionField`), que são anéis somados por cima
// da superfície: não refletem em obstáculo, não interferem entre si e não
// deixam esteira atrás de um corpo em movimento.
//
// Este campo é a equação de onda resolvida numa grade, que é como KWS2 e Crest
// resolvem o mesmo problema. Ele não substitui o espectro: soma a ele, numa
// área pequena em volta da câmera, onde o observador consegue ver detalhe.
//
// Vive em CPU por enquanto, e é deliberado. O solver é o mesmo em qualquer
// backend, e ter a versão testável primeiro é o que permite verificar
// estabilidade, conservação e amortecimento sem um aparelho na mesa. A versão
// em compute vem depois, conferida contra esta.
struct WaterRippleSettings final {
  // Lado da área simulada, em metros. Pequeno de propósito: ondulação de
  // impacto só é legível de perto, e uma área grande gasta resolução onde o
  // espectro já entrega detalhe.
  float areaSize = 32.0f;
  // Células por eixo. Potência de dois não é exigida — a grade não é uma FFT.
  u32 resolution = 128;
  // Velocidade de propagação, em metros por segundo.
  float propagationSpeed = 3.0f;
  // Fração da amplitude perdida por segundo. Sem isso a energia injetada nunca
  // sai e a área vira ruído acumulado depois de alguns minutos.
  float damping = 0.75f;
  // Envelope local em metros. Interações podem chegar de vários corpos no
  // mesmo texel; limitar no solver impede que uma configuração autoral extrema
  // produza picos fora dos bounds de render/física ou uma serra numérica.
  float maximumAmplitude = 2.0f;
};

bool validateWaterRipples(const WaterRippleSettings &settings) noexcept;

// Passo máximo estável para as configurações dadas, em segundos.
//
// A equação de onda explícita diverge quando `c·dt/dx` passa de 1/raiz(2) em
// duas dimensões. Ultrapassar isso não degrada suavemente: a grade satura em
// poucos quadros e a água vira uma serra. O solver subdivide o passo em vez de
// confiar em quem chama.
float waterRippleMaximumStep(const WaterRippleSettings &settings) noexcept;

class WaterRippleField final {
public:
  bool initialize(const WaterRippleSettings &settings);
  void clear() noexcept;
  bool isReady() const noexcept { return resolution_ != 0; }
  const WaterRippleSettings &settings() const noexcept { return settings_; }

  // Centro da área, em coordenadas de mundo. Mover recentra a grade e descarta
  // o que saiu — o histórico não é transportado. Arrastar o conteúdo junto
  // parece certo e não é: a fase da onda ficaria presa à câmera, e o padrão
  // inteiro deslizaria com o observador em vez de ficar na água.
  void recenter(float centreX, float centreZ) noexcept;
  float centreX() const noexcept { return centreX_; }
  float centreZ() const noexcept { return centreZ_; }

  // Perturba a superfície. `radius` em metros, `amplitude` em metros e com
  // sinal: negativo afunda, que é o que um casco faz ao deslocar água.
  // Impulsos fora da área são ignorados em silêncio; é o caso comum, não erro.
  void addImpulse(float x, float z, float radius, float amplitude) noexcept;

  // Avança a simulação. Passos maiores que o limite de estabilidade são
  // subdivididos internamente.
  bool advance(float seconds) noexcept;

  // Altura interpolada em coordenadas de mundo; zero fora da área.
  float height(float x, float z) const noexcept;
  // Inclinação da superfície, para compor com a normal do espectro.
  void slope(float x, float z, float &slopeX, float &slopeZ) const noexcept;

  // Copia o snapshot lógico, em linhas consecutivas, sem expor ownership nem
  // o vetor interno. O renderer usa esta fronteira para publicar a grade à GPU
  // em um único passe, sem fazer quatro leituras bilineares por célula.
  bool copyHeightsTo(std::span<float> destination) const noexcept;

  // Soma dos quadrados — proporcional à energia da superfície. Existe para os
  // testes provarem que a energia injetada de fato sai da grade.
  //
  // Somar os valores absolutos em vez disso mediria a coisa errada: uma onda
  // que se espalha ocupa mais células com amplitude menor, e a soma de |h| pode
  // crescer enquanto a energia cai. Foi assim que o teste da borda absorvente
  // falhou pela primeira vez — a métrica estava errada, não o solver.
  double totalEnergy() const noexcept;

private:
  usize index(u32 column, u32 row) const noexcept {
    return static_cast<usize>(row) * resolution_ + column;
  }
  float rawHeight(int column, int row) const noexcept;
  void stepOnce(float seconds) noexcept;

  WaterRippleSettings settings_{};
  u32 resolution_ = 0;
  float cellSize_ = 1.0f;
  float centreX_ = 0.0f, centreZ_ = 0.0f;
  std::vector<float> current_, previous_, scratch_;
};

// Emissor genérico de esteira para qualquer corpo flutuante. Ele converte
// deslocamento horizontal em uma sequência espacial de fontes de proa, popa e
// bordos; a distância, e não a taxa de quadros, determina quantas são emitidas.
struct WaterWakeSettings final {
  float strength = 1.2f;
  float minimumSpeed = .1f;
  float spacing = 2.0f;
  float widthScale = .22f;
  float maximumImpulse = 1.2f;
};

struct WaterWakeInput final {
  WaterVec2 position{};
  WaterVec2 forward{0.0f, 1.0f};
  WaterVec2 velocity{};
  float halfLength = 1.0f;
  float halfWidth = .5f;
  float submergedFraction = 1.0f;
  float deltaSeconds = 0.0f;
};

bool validateWaterWakeSettings(const WaterWakeSettings &settings) noexcept;

class WaterWakeEmitter final {
public:
  bool configure(const WaterWakeSettings &settings) noexcept;
  void reset() noexcept;
  bool update(const WaterWakeInput &input, WaterRippleField &field) noexcept;
  u64 emittedSections() const noexcept { return emittedSections_; }
  const WaterWakeSettings &settings() const noexcept { return settings_; }

private:
  WaterWakeSettings settings_{};
  float distanceRemainder_ = 0.0f;
  u64 emittedSections_ = 0;
  bool configured_ = false;
};

} // namespace ae::renderer
