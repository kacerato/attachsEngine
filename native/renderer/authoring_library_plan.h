#pragma once
#include "renderer/authoring_texture.h"
#include <span>
#include <vector>

namespace ae::renderer {
// R2/R4: publicação incremental da biblioteca de autoria.
//
// A sessão entrega a lista COMPLETA de texturas a cada publicação, mas cada uma
// é um objeto compartilhado e imutável: a mesma decodificação (mesma fonte, mesmo
// perfil, mesmo sampler) chega como o MESMO ponteiro. Um ponteiro que já estava
// na publicação anterior tem exatamente os mesmos pixels e sampler na GPU, e a
// imagem pode ser reaproveitada em vez de subir de novo. Só as novas sobem.
//
// `reuse[i]` é o índice na lista anterior ou NoReuse. A imagem tem um dono só:
// um ponteiro repetido na lista nova reaproveita só na primeira ocorrência.
inline constexpr u32 AuthoringTextureNoReuse = 0xFFFFFFFFu;

struct AuthoringTextureReusePlan {
  std::vector<u32> reuse;
  u32 reused = 0, uploaded = 0;
};

inline AuthoringTextureReusePlan planAuthoringTextureReuse(std::span<const SharedAuthoringTexture> previous,
                                                           std::span<const SharedAuthoringTexture> next) {
  AuthoringTextureReusePlan plan;
  plan.reuse.assign(next.size(), AuthoringTextureNoReuse);
  std::vector<u8> taken(previous.size());
  for (usize i = 0; i < next.size(); ++i) {
    if (next[i])
      for (usize j = 0; j < previous.size(); ++j)
        if (!taken[j] && previous[j] == next[i]) {
          plan.reuse[i] = static_cast<u32>(j);
          taken[j] = 1;
          break;
        }
    if (plan.reuse[i] == AuthoringTextureNoReuse) ++plan.uploaded;
    else ++plan.reused;
  }
  return plan;
}
} // namespace ae::renderer
