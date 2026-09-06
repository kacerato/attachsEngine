#include "renderer/water_ripples.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {
namespace {

// Courant em duas dimensões. Acima disso a equação explícita não degrada
// suavemente: satura em poucos quadros e a água vira serra.
constexpr float CourantLimit = 0.70710678f;  // 1/raiz(2)
// Margem para não operar exatamente no limite, onde erro de ponto flutuante já
// basta para atravessá-lo.
constexpr float CourantSafety = 0.9f;

bool finite(float value) noexcept { return std::isfinite(value); }

} // namespace

bool validateWaterRipples(const WaterRippleSettings &settings) noexcept {
  if (!finite(settings.areaSize) || !finite(settings.propagationSpeed) ||
      !finite(settings.damping)) return false;
  return settings.areaSize > 0.0f && settings.areaSize <= 1024.0f &&
         settings.resolution >= 8u && settings.resolution <= 1024u &&
         settings.propagationSpeed > 0.0f && settings.propagationSpeed <= 100.0f &&
         settings.damping >= 0.0f && settings.damping <= 100.0f;
}

float waterRippleMaximumStep(const WaterRippleSettings &settings) noexcept {
  if (!validateWaterRipples(settings)) return 0.0f;
  const float cell = settings.areaSize / static_cast<float>(settings.resolution);
  return CourantSafety * CourantLimit * cell / settings.propagationSpeed;
}

bool WaterRippleField::initialize(const WaterRippleSettings &settings) {
  if (!validateWaterRipples(settings)) return false;
  settings_ = settings;
  resolution_ = settings.resolution;
  cellSize_ = settings.areaSize / static_cast<float>(resolution_);
  const usize cells = static_cast<usize>(resolution_) * resolution_;
  current_.assign(cells, 0.0f);
  previous_.assign(cells, 0.0f);
  scratch_.assign(cells, 0.0f);
  return true;
}

void WaterRippleField::clear() noexcept {
  std::fill(current_.begin(), current_.end(), 0.0f);
  std::fill(previous_.begin(), previous_.end(), 0.0f);
  std::fill(scratch_.begin(), scratch_.end(), 0.0f);
}

void WaterRippleField::recenter(float centreX, float centreZ) noexcept {
  if (!finite(centreX) || !finite(centreZ)) return;
  if (centreX == centreX_ && centreZ == centreZ_) return;
  centreX_ = centreX;
  centreZ_ = centreZ;
  // O conteúdo não acompanha o centro. Arrastá-lo prenderia a fase da onda à
  // câmera, e o padrão inteiro deslizaria com o observador em vez de ficar na
  // água — o defeito é discreto parado e gritante em movimento.
  clear();
}

void WaterRippleField::addImpulse(float x, float z, float radius, float amplitude) noexcept {
  if (resolution_ == 0 || !finite(x) || !finite(z) || !finite(radius) || !finite(amplitude)) return;
  if (!(radius > 0.0f) || amplitude == 0.0f) return;

  const float half = settings_.areaSize * 0.5f;
  const float localX = x - centreX_ + half;
  const float localZ = z - centreZ_ + half;
  const float cellRadius = radius / cellSize_;
  const int minimumColumn = static_cast<int>(std::floor((localX - radius) / cellSize_));
  const int maximumColumn = static_cast<int>(std::ceil((localX + radius) / cellSize_));
  const int minimumRow = static_cast<int>(std::floor((localZ - radius) / cellSize_));
  const int maximumRow = static_cast<int>(std::ceil((localZ + radius) / cellSize_));
  const float centreColumn = localX / cellSize_;
  const float centreRow = localZ / cellSize_;

  for (int row = minimumRow; row <= maximumRow; ++row) {
    if (row < 0 || row >= static_cast<int>(resolution_)) continue;
    for (int column = minimumColumn; column <= maximumColumn; ++column) {
      if (column < 0 || column >= static_cast<int>(resolution_)) continue;
      const float dx = static_cast<float>(column) + 0.5f - centreColumn;
      const float dz = static_cast<float>(row) + 0.5f - centreRow;
      const float distance = std::sqrt(dx * dx + dz * dz);
      if (distance > cellRadius) continue;
      // Cosseno elevado: bordas com derivada nula. Um degrau ou um cone
      // injetariam alta frequência que a grade não resolve, e ela volta como
      // xadrez em vez de onda.
      const float falloff = 0.5f * (1.0f + std::cos(3.14159265358979f * distance / cellRadius));
      const usize cell = index(static_cast<u32>(column), static_cast<u32>(row));
      const float delta = amplitude * falloff;
      // O deslocamento entra nos dois quadros de histórico: a superfície é
      // empurrada e solta do repouso. Somar só no atual diz ao integrador que
      // ela chegou ali em um passo, o que é uma velocidade enorme, e a
      // amplitude cresce sozinha nos primeiros quadros.
      current_[cell] += delta;
      previous_[cell] += delta;
    }
  }
}

void WaterRippleField::stepOnce(float seconds) noexcept {
  const float courant = settings_.propagationSpeed * seconds / cellSize_;
  const float squared = courant * courant;
  const u32 last = resolution_ - 1u;

  for (u32 row = 0; row < resolution_; ++row) {
    for (u32 column = 0; column < resolution_; ++column) {
      const usize here = index(column, row);
      // A moldura é resolvida depois, pela condição de radiação.
      if (row == 0 || column == 0 || row == last || column == last) continue;
      const float neighbours = current_[index(column - 1u, row)] + current_[index(column + 1u, row)] +
                               current_[index(column, row - 1u)] + current_[index(column, row + 1u)];
      const float laplacian = neighbours - 4.0f * current_[here];
      // Verlet na equação de onda: h(t+dt) = 2h - h(t-dt) + C²·laplaciano.
      float next = 2.0f * current_[here] - previous_[here] + squared * laplacian;
      // Amortecimento exponencial exato para o passo, em vez de subtrair uma
      // fração: assim o decaimento não muda quando o passo é subdividido.
      next *= std::exp(-settings_.damping * seconds);
      scratch_[here] = finite(next) ? next : 0.0f;
    }
  }

  // Condição de radiação de Mur, primeira ordem. A borda copia o vizinho
  // interno com o atraso de um passo, o que deixa a onda que chega continuar
  // para fora em vez de voltar.
  //
  // Prender a moldura em zero é uma parede rígida: ela reflete com inversão de
  // fase e a área vira caixa acústica, cujo eco aparece como padrão quadrado na
  // superfície. Uma faixa de amortecimento antes da moldura não resolve — foi
  // tentada e medida, e piorava: a própria faixa é um degrau de impedância que
  // reflete, e alargá-la ou reforçá-la aumentava o eco em vez de reduzi-lo.
  // Num modelo do solver, com impulso no centro e sem amortecimento, o eco que
  // volta ao centro caiu de 0,119 com parede para 0,0069 com Mur.
  const float mur = (courant - 1.0f) / (courant + 1.0f);
  const auto radiate = [&](usize border, usize inner) {
    const float value = current_[inner] + mur * (scratch_[inner] - current_[border]);
    scratch_[border] = finite(value) ? value : 0.0f;
  };
  for (u32 row = 1; row < last; ++row) {
    radiate(index(0u, row), index(1u, row));
    radiate(index(last, row), index(last - 1u, row));
  }
  for (u32 column = 0; column <= last; ++column) {
    radiate(index(column, 0u), index(column, 1u));
    radiate(index(column, last), index(column, last - 1u));
  }

  previous_.swap(current_);
  current_.swap(scratch_);
}

bool WaterRippleField::advance(float seconds) noexcept {
  if (resolution_ == 0 || !finite(seconds) || seconds < 0.0f) return false;
  if (seconds == 0.0f) return true;
  const float maximum = waterRippleMaximumStep(settings_);
  if (!(maximum > 0.0f)) return false;

  // Subdividir em vez de confiar em quem chama: um quadro longo não pode fazer
  // a simulação explodir, e um quadro longo é justamente o que acontece quando
  // o aparelho está sob pressão.
  const u32 steps = static_cast<u32>(std::ceil(seconds / maximum));
  // Teto para que uma pausa longa não vire um congelamento maior ainda ao
  // recuperar. Perder ondulação depois de uma pausa é invisível; travar não é.
  constexpr u32 MaximumSteps = 32u;
  const u32 bounded = std::min(steps, MaximumSteps);
  // Ao bater no teto, o tempo excedente é descartado em vez de esticar o passo.
  // Esticar atravessaria a condição de Courant justamente no quadro longo, que
  // é quando o aparelho já está em apuros — e aí a água vira serra.
  const float step = std::min(seconds / static_cast<float>(bounded), maximum);
  for (u32 iteration = 0; iteration < bounded; ++iteration) stepOnce(step);
  return true;
}

float WaterRippleField::height(float x, float z) const noexcept {
  if (resolution_ == 0 || !finite(x) || !finite(z)) return 0.0f;
  const float half = settings_.areaSize * 0.5f;
  const float column = (x - centreX_ + half) / cellSize_ - 0.5f;
  const float row = (z - centreZ_ + half) / cellSize_ - 0.5f;
  const float flooredColumn = std::floor(column), flooredRow = std::floor(row);
  const int baseColumn = static_cast<int>(flooredColumn);
  const int baseRow = static_cast<int>(flooredRow);
  if (baseColumn < 0 || baseRow < 0 ||
      baseColumn + 1 >= static_cast<int>(resolution_) ||
      baseRow + 1 >= static_cast<int>(resolution_)) return 0.0f;

  const float fractionColumn = column - flooredColumn;
  const float fractionRow = row - flooredRow;
  const float topLeft = current_[index(static_cast<u32>(baseColumn), static_cast<u32>(baseRow))];
  const float topRight = current_[index(static_cast<u32>(baseColumn + 1), static_cast<u32>(baseRow))];
  const float bottomLeft = current_[index(static_cast<u32>(baseColumn), static_cast<u32>(baseRow + 1))];
  const float bottomRight = current_[index(static_cast<u32>(baseColumn + 1), static_cast<u32>(baseRow + 1))];
  const float top = topLeft + (topRight - topLeft) * fractionColumn;
  const float bottom = bottomLeft + (bottomRight - bottomLeft) * fractionColumn;
  return top + (bottom - top) * fractionRow;
}

void WaterRippleField::slope(float x, float z, float &slopeX, float &slopeZ) const noexcept {
  slopeX = 0.0f;
  slopeZ = 0.0f;
  if (resolution_ == 0) return;
  // Diferença central sobre a própria interpolação: mais barato que derivar a
  // bilinear analiticamente e contínuo entre células, que é o que evita facetas.
  const float step = cellSize_;
  slopeX = (height(x + step, z) - height(x - step, z)) / (2.0f * step);
  slopeZ = (height(x, z + step) - height(x, z - step)) / (2.0f * step);
}

double WaterRippleField::totalEnergy() const noexcept {
  double total = 0.0;
  for (float value : current_) {
    const double amplitude = static_cast<double>(value);
    total += amplitude * amplitude;
  }
  return total;
}

} // namespace ae::renderer
