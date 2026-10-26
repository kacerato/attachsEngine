#pragma once

#include <algorithm>
#include <cmath>

namespace ae::physics {

// Direção do corpo do Personagem (Starter Assets ThirdPersonController da Unity:
// o alvo é atan2 do comando somado ao yaw da câmera, e o giro chega lá por
// Mathf.SmoothDampAngle com RotationSmoothTime). Só gira enquanto há comando;
// parado, o corpo fica na última direção.
struct CharacterFacing {
  float yaw = 0;       // radianos, frente = +Z girado por yaw (mesma convenção do motor)
  float velocity = 0;  // rad/s do amortecimento
  bool known = false;
};

// Mathf.SmoothDampAngle: mola criticamente amortecida pelo caminho mais curto,
// com velocidade máxima. smoothTime zero gira o máximo permitido no passo.
inline float smoothDampAngle(float current, float target, float &velocity, float smoothTime,
                             float maximumSpeed, float deltaSeconds) {
  constexpr float Tau = 6.28318530718f;
  target = current + std::remainder(target - current, Tau);
  if (smoothTime <= 1e-4f) {
    const float step = maximumSpeed * deltaSeconds, gap = target - current;
    velocity = 0;
    return std::abs(gap) <= step ? target : current + std::copysign(step, gap);
  }
  const float omega = 2.0f / smoothTime, x = omega * deltaSeconds;
  const float decay = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
  const float maximumChange = maximumSpeed * smoothTime;
  const float change = std::clamp(current - target, -maximumChange, maximumChange);
  const float clampedTarget = current - change;
  const float temp = (velocity + omega * change) * deltaSeconds;
  velocity = (velocity - omega * temp) * decay;
  float output = clampedTarget + (change + temp) * decay;
  if ((target - current > 0.0f) == (output > target)) {
    output = target;
    velocity = 0;
  }
  return output;
}

// Um passo fixo. `right`/`forward` são o comando (-1..1) e `referenceYaw` o
// yaw da câmera (ou zero para o mundo). Devolve verdadeiro quando o corpo girou.
inline bool stepCharacterFacing(CharacterFacing &facing, float right, float forward, float referenceYaw,
                                float smoothTime, float maximumDegreesPerSecond, float deltaSeconds) {
  if (!facing.known || !std::isfinite(right) || !std::isfinite(forward) || !std::isfinite(referenceYaw) ||
      !std::isfinite(deltaSeconds) || deltaSeconds <= 0) return false;
  if (right * right + forward * forward < 0.0025f) {
    facing.velocity = 0;
    return false;
  }
  const float sine = std::sin(referenceYaw), cosine = std::cos(referenceYaw);
  const float x = cosine * right + sine * forward, z = -sine * right + cosine * forward;
  const float target = std::atan2(x, z);
  const float before = facing.yaw;
  facing.yaw = smoothDampAngle(facing.yaw, target, facing.velocity, std::max(0.0f, smoothTime),
                               std::max(1.0f, maximumDegreesPerSecond) * 0.0174532925f, deltaSeconds);
  facing.yaw = std::remainder(facing.yaw, 6.28318530718f);
  return facing.yaw != before;
}

} // namespace ae::physics
