#pragma once
// Exportacao glTF 2.0 binaria (N06): hierarquia de nos (esqueleto) e clipes
// com canais de translacao, rotacao e escala amostrados (LINEAR). Malhas,
// skins e materiais nao fazem parte desta saida: quem exporta declara o que
// ficou de fora no relatorio. Unidades em metros, Y para cima, rotacoes em
// quaternion XYZW, como o glTF exige.
#include "resources/skeletal_animation.h"
#include <string>
#include <vector>

namespace ae::resources {
struct GltfExportNode {
  std::string name;
  i32 parent=-1;
  float translation[3]{},rotation[4]{0,0,0,1},scale[3]{1,1,1};
};
struct GltfExportChannel {
  u32 node=0;AnimationPath path=AnimationPath::Translation;
  std::vector<float> times,values; // 3 (T/S) ou 4 (R) valores por tempo
};
struct GltfExportAnimation {std::string name;std::vector<GltfExportChannel> channels;};
// Falha fechada: `out` so recebe bytes validos.
bool writeGlb(const std::vector<GltfExportNode> &nodes,const std::vector<GltfExportAnimation> &animations,
              std::vector<u8> &out,std::string &diagnostic);
}
