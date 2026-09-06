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
      !finite(settings.damping) || !finite(settings.maximumAmplitude)) return false;
  return settings.areaSize > 0.0f && settings.areaSize <= 1024.0f &&
         settings.resolution >= 8u && settings.resolution <= 1024u &&
         settings.propagationSpeed > 0.0f && settings.propagationSpeed <= 100.0f &&
         settings.damping >= 0.0f && settings.damping <= 100.0f &&
         settings.maximumAmplitude > 0.0f && settings.maximumAmplitude <= 20.0f;
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
      current_[cell] = std::clamp(current_[cell] + delta, -settings_.maximumAmplitude,
                                  settings_.maximumAmplitude);
      previous_[cell] = std::clamp(previous_[cell] + delta, -settings_.maximumAmplitude,
                                   settings_.maximumAmplitude);
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
      scratch_[here] = finite(next)
          ? std::clamp(next, -settings_.maximumAmplitude, settings_.maximumAmplitude)
          : 0.0f;
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
    scratch_[border] = finite(value)
        ? std::clamp(value, -settings_.maximumAmplitude, settings_.maximumAmplitude)
        : 0.0f;
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
  const float edgeDistance = std::min(half - std::abs(x - centreX_),
                                      half - std::abs(z - centreZ_));
  if (edgeDistance <= 0.0f) return 0.0f;
  const float column = (x - centreX_ + half) / cellSize_ - 0.5f;
  const float row = (z - centreZ_ + half) / cellSize_ - 0.5f;
  const float flooredColumn = std::floor(column), flooredRow = std::floor(row);
  const int baseColumn = static_cast<int>(flooredColumn);
  const int baseRow = static_cast<int>(flooredRow);

  const float fractionColumn = column - flooredColumn;
  const float fractionRow = row - flooredRow;
  // A moldura virtual fora da grade vale zero. Isso permite interpolar até a
  // borda em vez de saltar do primeiro texel para zero, que desenharia um
  // quadrado na normal da água.
  const float topLeft = rawHeight(baseColumn, baseRow);
  const float topRight = rawHeight(baseColumn + 1, baseRow);
  const float bottomLeft = rawHeight(baseColumn, baseRow + 1);
  const float bottomRight = rawHeight(baseColumn + 1, baseRow + 1);
  const float top = topLeft + (topRight - topLeft) * fractionColumn;
  const float bottom = bottomLeft + (bottomRight - bottomLeft) * fractionColumn;
  const float sampled = top + (bottom - top) * fractionRow;
  // Dois texels de feather casam com a borda absorvente do solver. A função
  // cúbica tem derivada zero nas duas pontas, portanto altura e normal chegam a
  // zero sem costura quando a área local encontra o oceano global.
  const float normalized = std::clamp(edgeDistance / (2.0f * cellSize_), 0.0f, 1.0f);
  const float edgeFade = normalized * normalized * (3.0f - 2.0f * normalized);
  return sampled * edgeFade;
}

float WaterRippleField::rawHeight(int column, int row) const noexcept {
  if (column < 0 || row < 0 || column >= static_cast<int>(resolution_) ||
      row >= static_cast<int>(resolution_)) return 0.0f;
  return current_[index(static_cast<u32>(column), static_cast<u32>(row))];
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

bool WaterRippleField::copyHeightsTo(std::span<float> destination) const noexcept {
  if (resolution_ == 0 || destination.size() < current_.size()) return false;
  std::copy(current_.begin(), current_.end(), destination.begin());
  return true;
}

double WaterRippleField::totalEnergy() const noexcept {
  double total = 0.0;
  for (float value : current_) {
    const double amplitude = static_cast<double>(value);
    total += amplitude * amplitude;
  }
  return total;
}

bool validateWaterWakeSettings(const WaterWakeSettings &settings) noexcept {
  return finite(settings.strength) && settings.strength >= 0.0f && settings.strength <= 8.0f &&
      finite(settings.minimumSpeed) && settings.minimumSpeed >= 0.0f &&
      settings.minimumSpeed <= 50.0f && finite(settings.spacing) && settings.spacing >= .5f &&
      settings.spacing <= 100.0f && finite(settings.widthScale) && settings.widthScale >= .05f &&
      settings.widthScale <= 2.0f && finite(settings.maximumImpulse) &&
      settings.maximumImpulse > 0.0f && settings.maximumImpulse <= 5.0f;
}

bool WaterWakeEmitter::configure(const WaterWakeSettings &settings) noexcept {
  if (!validateWaterWakeSettings(settings)) return false;
  settings_ = settings;
  configured_ = true;
  return true;
}

void WaterWakeEmitter::reset() noexcept {
  distanceRemainder_ = 0.0f;
  emittedSections_ = 0;
}

bool WaterWakeEmitter::update(const WaterWakeInput &input, WaterRippleField &field) noexcept {
  if (!configured_ || !field.isReady() || !finite(input.position.x) ||
      !finite(input.position.y) || !finite(input.forward.x) || !finite(input.forward.y) ||
      !finite(input.velocity.x) || !finite(input.velocity.y) || !finite(input.halfLength) ||
      !finite(input.halfWidth) || !finite(input.submergedFraction) ||
      !finite(input.deltaSeconds) || input.halfLength <= 0.0f || input.halfWidth <= 0.0f ||
      input.submergedFraction < 0.0f || input.submergedFraction > 1.0f ||
      input.deltaSeconds < 0.0f) return false;

  const float forwardLength = std::hypot(input.forward.x, input.forward.y);
  if (!(forwardLength > 1.0e-5f)) return false;
  const WaterVec2 forward{input.forward.x / forwardLength, input.forward.y / forwardLength};
  const WaterVec2 right{forward.y, -forward.x};
  const float signedSpeed = input.velocity.x * forward.x + input.velocity.y * forward.y;
  const float speed = std::abs(signedSpeed);
  if (settings_.strength == 0.0f || input.submergedFraction == 0.0f ||
      speed < settings_.minimumSpeed || input.deltaSeconds == 0.0f) return true;

  distanceRemainder_ += speed * input.deltaSeconds;
  // Um frame longo não pode preencher toda a grade. As seções restantes ficam
  // no acumulador e saem nos próximos frames, preservando densidade espacial.
  constexpr u32 MaximumSectionsPerUpdate = 8;
  // O mesmo metro integrado em 60 ou 120 floats pode terminar poucos ulps
  // abaixo da fronteira. A tolerância é relativa ao espaçamento e não cria
  // uma seção antecipada observável (máximo 0,01%).
  const u32 available = static_cast<u32>((distanceRemainder_ +
      settings_.spacing * 1.0e-4f) / settings_.spacing);
  const u32 sections = std::min(available, MaximumSectionsPerUpdate);
  if (sections == 0) return true;
  distanceRemainder_ = std::max(0.0f,distanceRemainder_ -
      static_cast<float>(sections) * settings_.spacing);
  distanceRemainder_ = std::min(distanceRemainder_, settings_.spacing * 8.0f);

  const float cellSize = field.settings().areaSize /
                         static_cast<float>(field.settings().resolution);
  const float radius = std::max(cellSize * 1.5f, input.halfWidth * settings_.widthScale);
  const float amplitude = std::min(settings_.maximumImpulse,
      speed * .025f * settings_.strength * input.submergedFraction);
  const float travelSign = signedSpeed >= 0.0f ? 1.0f : -1.0f;
  const WaterVec2 travel{forward.x * travelSign, forward.y * travelSign};
  for (u32 section = 0; section < sections; ++section) {
    const float trail = distanceRemainder_ + static_cast<float>(section) * settings_.spacing;
    const WaterVec2 centre{input.position.x - travel.x * trail,
                           input.position.y - travel.y * trail};
    const WaterVec2 bow{centre.x + travel.x * input.halfLength * .82f,
                        centre.y + travel.y * input.halfLength * .82f};
    const WaterVec2 stern{centre.x - travel.x * input.halfLength * .78f,
                          centre.y - travel.y * input.halfLength * .78f};
    field.addImpulse(bow.x, bow.y, radius * 1.25f, amplitude * .55f);
    field.addImpulse(stern.x, stern.y, radius, -amplitude);
    field.addImpulse(stern.x + right.x * input.halfWidth * .72f,
                     stern.y + right.y * input.halfWidth * .72f,
                     radius, amplitude * .48f);
    field.addImpulse(stern.x - right.x * input.halfWidth * .72f,
                     stern.y - right.y * input.halfWidth * .72f,
                     radius, amplitude * .48f);
  }
  emittedSections_ += sections;
  return true;
}

} // namespace ae::renderer
