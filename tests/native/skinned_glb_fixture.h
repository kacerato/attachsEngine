#pragma once
// GLB sintético com skin e animação (G6-B), para testar importação, cache,
// editor e runtime sem depender de arquivos externos.
//
// Nós: Rig (raiz) -> Hip (junta) -> Arm (junta, +1 m em Y); Rig -> Body (malha
// com skin 0). A malha é um quadrado de 1 x 2 m: os dois vértices de baixo
// seguem só o Hip; os dois de cima seguem o Arm (0,6) e o Hip (0,2), pesos que
// não somam um no arquivo e chegam renormalizados (0,75/0,25).
// Clipe "Wave", 1 s: Arm gira 90° em Z (LINEAR), Hip salta 2 m em Z em 0,5 s
// (STEP), Hip escala 1 -> 2 (CUBICSPLINE, tangentes zero) e um canal de pesos
// de morph que a importação conta como não suportado.
#include "core/base.h"

#include <cstring>
#include <string>
#include <vector>

namespace ae::test {

// `looseJoint`: a segunda junta aponta para um nó fora da cena (recusa esperada).
inline std::vector<u8> skinnedAnimatedGlb(bool looseJoint = false) {
  std::vector<u8> binary;
  const auto floats = [&](std::initializer_list<float> values) {
    for (float value : values) {
      u8 bytes[4];
      std::memcpy(bytes, &value, 4);
      binary.insert(binary.end(), bytes, bytes + 4);
    }
  };
  // 0: posições (48 B)
  floats({0, 0, 0, 1, 0, 0, 0, 2, 0, 1, 2, 0});
  // 48: JOINTS_0 u8x4 (16 B)
  for (u8 value : {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}) binary.push_back(value);
  // 64: WEIGHTS_0 float4 (64 B)
  floats({1, 0, 0, 0, 1, 0, 0, 0, .6f, .2f, 0, 0, .6f, .2f, 0, 0});
  // 128: índices u16 (12 B) + 4 de alinhamento
  for (u16 index : {0, 1, 2, 2, 1, 3}) {
    binary.push_back(static_cast<u8>(index & 0xff));
    binary.push_back(static_cast<u8>(index >> 8));
  }
  binary.insert(binary.end(), 4, 0);
  // 144: bind inversa (Hip identidade; Arm translação -1 em Y), 128 B
  floats({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
  floats({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1});
  // 272: tempos 0 e 1
  floats({0, 1});
  // 280: rotação: identidade e 90° em Z
  floats({0, 0, 0, 1, 0, 0, 0.70710678f, 0.70710678f});
  // 312: tempos do STEP
  floats({0, .5f});
  // 320: translação STEP
  floats({0, 0, 0, 0, 0, 2});
  // 344: escala CUBICSPLINE: (entrada, valor, saída) por chave
  floats({0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0});
  // 416: pesos de morph (canal não suportado)
  floats({0, 1});
  const std::string json = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
"nodes":[{"name":"Rig","children":[1,3]},{"name":"Hip","children":[2]},{"name":"Arm","translation":[0,1,0]},
{"name":"Body","mesh":0,"skin":0}],
"meshes":[{"name":"Body","primitives":[{"attributes":{"POSITION":0,"JOINTS_0":1,"WEIGHTS_0":2},"indices":3}]}],
"skins":[{"name":"Rig","joints":[1,2],"inverseBindMatrices":4,"skeleton":1}],
"animations":[{"name":"Wave","samplers":[
{"input":5,"output":6,"interpolation":"LINEAR"},{"input":7,"output":8,"interpolation":"STEP"},
{"input":5,"output":9,"interpolation":"CUBICSPLINE"},{"input":5,"output":10}],
"channels":[{"sampler":0,"target":{"node":2,"path":"rotation"}},{"sampler":1,"target":{"node":1,"path":"translation"}},
{"sampler":2,"target":{"node":1,"path":"scale"}},{"sampler":3,"target":{"node":3,"path":"weights"}}]}],
"buffers":[{"byteLength":424}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":16},
{"buffer":0,"byteOffset":64,"byteLength":64},{"buffer":0,"byteOffset":128,"byteLength":12},
{"buffer":0,"byteOffset":144,"byteLength":128},{"buffer":0,"byteOffset":272,"byteLength":8},
{"buffer":0,"byteOffset":280,"byteLength":32},{"buffer":0,"byteOffset":312,"byteLength":8},
{"buffer":0,"byteOffset":320,"byteLength":24},{"buffer":0,"byteOffset":344,"byteLength":72},
{"buffer":0,"byteOffset":416,"byteLength":8}],
"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[0,0,0],"max":[1,2,0]},
{"bufferView":1,"componentType":5121,"count":4,"type":"VEC4"},
{"bufferView":2,"componentType":5126,"count":4,"type":"VEC4"},
{"bufferView":3,"componentType":5123,"count":6,"type":"SCALAR"},
{"bufferView":4,"componentType":5126,"count":2,"type":"MAT4"},
{"bufferView":5,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},
{"bufferView":6,"componentType":5126,"count":2,"type":"VEC4"},
{"bufferView":7,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[0.5]},
{"bufferView":8,"componentType":5126,"count":2,"type":"VEC3"},
{"bufferView":9,"componentType":5126,"count":6,"type":"VEC3"},
{"bufferView":10,"componentType":5126,"count":2,"type":"SCALAR"}]})";
  std::string text = json;
  if (looseJoint) {
    const std::string body = R"({"name":"Body","mesh":0,"skin":0}])", joints = R"("joints":[1,2])";
    text.replace(text.find(body), body.size(), R"({"name":"Body","mesh":0,"skin":0},{"name":"Loose"}])");
    text.replace(text.find(joints), joints.size(), R"("joints":[1,4])");
  }
  while (text.size() % 4) text.push_back(' ');
  while (binary.size() % 4) binary.push_back(0);
  std::vector<u8> glb;
  const auto put = [&](u32 value) {
    for (u32 i = 0; i < 4; ++i) glb.push_back(static_cast<u8>(value >> (i * 8)));
  };
  put(0x46546C67);
  put(2);
  put(static_cast<u32>(12 + 8 + text.size() + 8 + binary.size()));
  put(static_cast<u32>(text.size()));
  put(0x4E4F534A);
  glb.insert(glb.end(), text.begin(), text.end());
  put(static_cast<u32>(binary.size()));
  put(0x004E4942);
  glb.insert(glb.end(), binary.begin(), binary.end());
  return glb;
}

} // namespace ae::test
