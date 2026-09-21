// O que a engine REALMENTE faz, em uma lista só, legível por qualquer camada.
//
// O plano universal de cenários realistas exige que a autoria distinga quatro
// situações que, na tela, um autor confunde com "bug": o valor está ausente na
// fonte, o valor não foi importado, o valor existe mas **o backend não tem
// consumidor para ele**, e o valor existe mas **a política global o desligou**.
// As duas últimas dependem de uma pergunta que precisa ter UMA resposta no
// programa inteiro: "isto tem consumidor implementado hoje?".
//
// Sem este arquivo, essa resposta estava espalhada — `punctual_lights.h` sabia
// que sombra pontual não existe, `rendering_policy.h` sabia quais eixos sabe
// resolver, e o inspetor não sabia de nada, então ou oferecia um interruptor
// sem shader atrás ou escondia a propriedade sem dizer por quê. As duas saídas
// são piores que declarar o estado.
//
// Regras deste registro:
//
// 1. **Estado é sobre código, não sobre intenção.** `Implemented` significa que
//    existe passe/shader/serviço lendo o valor HOJE. `Planned` é uma capacidade
//    prevista pelos planos e sem consumidor: nenhuma propriedade autorável pode
//    depender dela (o teste `component_reflection` recusa).
// 2. **`DeviceLimited` é diferente de `Planned`.** O consumidor existe; o
//    aparelho pode recusar. O valor autoral persiste e a degradação é relatada
//    pela política resolvida — nunca apagada do arquivo do projeto.
// 3. **Uma verdade só.** Tabelas específicas de subsistema (por exemplo
//    `renderer::lightCapabilities`) fixam `static_assert` contra este registro,
//    de modo que divergir não compila.
// 4. **Camada base.** `core` é visível por `scene`, `renderer`, `runtime` e
//    `editor`; o registro não pode depender de nenhum deles.
#pragma once

#include "core/base.h"

#include <array>
#include <string_view>

namespace ae::core {

enum class CapabilityState : u32 {
  // Há consumidor no código: passe, shader, serviço ou bridge lendo o valor.
  Implemented = 0,
  // Existe consumidor, mas o backend/aparelho pode recusar. A política resolvida
  // reporta a degradação; o dado autoral continua válido e salvo.
  DeviceLimited = 1,
  // Previsto por plano, sem consumidor. Nenhuma propriedade pode depender dela.
  Planned = 2,
};

struct EngineCapability {
  // Identidade persistente, usada por propriedades, presets e relatórios.
  // Hierárquica por ponto: `dominio.familia.detalhe`.
  std::string_view id;
  const char *name;
  CapabilityState state;
  // Arquivo/módulo que implementa (ou implementaria) o consumidor. É o endereço
  // que um relatório de capacidade entrega a quem for conferir a afirmação.
  std::string_view owner;
  // Por que ainda não está disponível, quando não está. Vazio quando está.
  const char *limitation;
};

// A lista. Acrescentar uma linha aqui é barato; MUDAR um estado é uma
// afirmação sobre o código e deve vir junto do consumidor que a sustenta.
inline constexpr std::array<EngineCapability, 36> engineCapabilities{{
  // --- Luz -----------------------------------------------------------------
  {"render.light.directional", "Sol direcional", CapabilityState::Implemented,
   "renderer/punctual_lights.h + rhi/shaders/material_shading.glsl", ""},
  {"render.light.punctual", "Luzes pontuais e spot", CapabilityState::Implemented,
   "renderer/punctual_lights.h (8 por quadro)", ""},
  {"render.light.temperature", "Temperatura de cor em kelvin", CapabilityState::Implemented,
   "scene/light_units.h + runtime/scene_lights.cpp", ""},
  {"render.light.photometric", "Unidades fotométricas (lux, lumen, candela)", CapabilityState::Implemented,
   "scene/light_units.h + runtime/scene_lights.cpp", ""},
  {"render.light.cookie", "Máscara projetada (cookie)", CapabilityState::Planned,
   "renderer/punctual_lights.h", "Sem amostragem de textura por luz no shader de fragmento"},
  {"render.light.area", "Luz de área", CapabilityState::Planned, "renderer/punctual_lights.h",
   "Sem integração de fonte com extensão"},

  // --- Sombra --------------------------------------------------------------
  {"render.shadow.directional", "Sombra do sol em cascatas", CapabilityState::Implemented,
   "renderer/shadow_cascades.cpp + rhi/shaders/shadow_depth.vert", ""},
  {"render.shadow.punctual", "Sombra de luz pontual ou spot", CapabilityState::Implemented,
   "renderer/shadow_atlas.h", "Atlas em quadtree: spot ocupa um mapa, pontual seis faces"},

  // --- Ambiente, GI e sondas ----------------------------------------------
  {"render.ambient.hemispheric", "Ambiente hemisférico céu/chão", CapabilityState::Implemented,
   "renderer/environment_lighting.h", ""},
  {"render.ambient.specular", "Reflexo especular do ambiente", CapabilityState::Implemented,
   "renderer/environment_map.cpp", ""},
  {"render.gi.lightmap", "Iluminação indireta assada em lightmap", CapabilityState::Planned,
   "renderer/ (serviço de bake)", "Sem serviço de bake, UV de lightmap nem atlas"},
  {"render.probe.irradiance", "Sonda de irradiância por objeto", CapabilityState::Planned,
   "renderer/ (volume de sondas)", "Sem recurso de sonda nem amostragem por instância"},
  {"render.probe.reflection", "Sonda de reflexão local", CapabilityState::Planned,
   "renderer/environment_map.cpp", "Sem captura local, atlas nem blend por volume"},

  // --- Material ------------------------------------------------------------
  {"render.material.pbr", "Superfície PBR metálico/rugosidade", CapabilityState::Implemented,
   "rhi/shaders/material_shading.glsl", ""},
  {"render.material.alpha_mask", "Recorte por alfa, inclusive na sombra", CapabilityState::Implemented,
   "rhi/shaders/shadow_depth_masked.frag", ""},
  {"render.material.alpha_blend", "Transparência com mistura", CapabilityState::Implemented,
   "renderer/map_draw_update.h", ""},
  {"render.material.double_sided", "Desenho de face dupla", CapabilityState::Implemented,
   "renderer/map_draw_update.h", ""},
  {"render.material.uv_transform", "Conjunto de UV e transformação por binding", CapabilityState::Implemented,
   "rhi/shaders/world_uv.glsl", ""},
  {"render.material.variants", "Especialização de pipeline por material", CapabilityState::DeviceLimited,
   "renderer/rendering_policy.cpp", "Drivers móveis podem regredir com muitos pipelines pequenos"},
  {"render.material.clearcoat", "Camada de verniz e transmissão", CapabilityState::Planned,
   "rhi/shaders/material_shading.glsl", "Sem variante de BRDF com camada adicional"},

  // --- Textura -------------------------------------------------------------
  {"render.texture.anisotropy", "Filtragem anisotrópica", CapabilityState::DeviceLimited,
   "rhi/ (feature samplerAnisotropy)", "Depende da GPU expor samplerAnisotropy"},
  {"render.texture.bindless", "Indexação sem limite de descritor", CapabilityState::DeviceLimited,
   "rhi/bindless_registry", "Depende de descriptor indexing no backend"},

  // --- Geometria e visibilidade -------------------------------------------
  {"render.lod.package", "Níveis de detalhe do pacote de mapa", CapabilityState::Implemented,
   "renderer/lod_selection.cpp", ""},
  {"render.lod.group", "Grupo de LOD autoral por objeto", CapabilityState::Implemented,
   "runtime/lod_groups.h + renderer/lod_dither.glsl",
   "Níveis são objetos do autor ou da convenção _LOD<n>; simplificação automática de malha ainda não existe"},
  {"render.visibility.hzb", "Oclusão por pirâmide de profundidade", CapabilityState::Implemented,
   "renderer/hzb_visibility.cpp", ""},
  {"render.instancing.gpu", "Culling e compactação de desenho em GPU", CapabilityState::Implemented,
   "renderer/gpu_draw_culling.cpp", ""},
  {"render.motion_vectors", "Vetores de movimento por objeto", CapabilityState::Planned,
   "renderer/frame_graph.cpp", "Sem pose anterior por instância nem alvo de velocidade"},

  // --- Céu e atmosfera ----------------------------------------------------
  {"render.environment.atmosphere", "Céu atmosférico", CapabilityState::Implemented,
   "runtime/scene_environment.cpp + rhi/shaders/dirt_road_sky.frag", ""},
  {"render.environment.fog", "Neblina por profundidade", CapabilityState::Implemented,
   "rhi/shaders/post_process_common.glsl", ""},
  {"render.environment.volumes", "Volumes de ambiente por câmera", CapabilityState::Implemented,
   "runtime/scene_environment.cpp + renderer/scene_environment.cpp", ""},

  // --- Pós-processamento ---------------------------------------------------
  {"render.post.tonemap", "Exposição e mapeamento de tom", CapabilityState::Implemented,
   "rhi/shaders/post_process_common.glsl", ""},
  {"render.post.bloom", "Brilho estourado", CapabilityState::Implemented,
   "rhi/shaders/post_process_common.glsl", ""},
  {"render.post.film_grain", "Grão de filme", CapabilityState::Implemented,
   "rhi/shaders/post_process_common.glsl", ""},
  {"render.post.ambient_occlusion", "Oclusão ambiente em tela", CapabilityState::Implemented,
   "rhi/shaders/post_process_common.glsl", ""},
  {"render.aa.fxaa", "Antisserrilhado espacial", CapabilityState::Implemented,
   "rhi/shaders/post_process_common.glsl", ""},
  {"render.aa.temporal", "Antisserrilhado temporal", CapabilityState::DeviceLimited,
   "rhi/shaders/post_process_temporal.frag", "Exige histórico e profundidade alocáveis no backend"},
}};

inline constexpr const EngineCapability *findEngineCapability(std::string_view id) {
  for (const auto &entry : engineCapabilities)
    if (entry.id == id) return &entry;
  return nullptr;
}

// Uma capacidade vazia é "sempre disponível": a maioria das propriedades não
// depende de nada além do consumidor já declarado pelo componente.
inline constexpr bool engineCapabilityDeclared(std::string_view id) {
  return id.empty() || findEngineCapability(id) != nullptr;
}

// Verdadeiro quando existe consumidor no código. `DeviceLimited` conta como
// disponível na AUTORIA — quem resolve o aparelho é a política, não o inspetor.
inline constexpr bool engineCapabilityAuthorable(std::string_view id) {
  if (id.empty()) return true;
  const auto *entry = findEngineCapability(id);
  return entry && entry->state != CapabilityState::Planned;
}

inline constexpr const char *engineCapabilityLimitation(std::string_view id) {
  const auto *entry = findEngineCapability(id);
  return entry ? entry->limitation : "";
}

inline constexpr const char *capabilityStateName(CapabilityState state) {
  switch (state) {
  case CapabilityState::Implemented: return "implementada";
  case CapabilityState::DeviceLimited: return "limitada pelo aparelho";
  case CapabilityState::Planned: return "planejada";
  }
  return "desconhecida";
}

// Identidades duplicadas fariam `findEngineCapability` responder por ordem de
// declaração — o mesmo defeito que `Components::read` recusa nos tipos.
inline constexpr bool engineCapabilitiesUnique() {
  for (usize i = 0; i < engineCapabilities.size(); ++i)
    for (usize j = 0; j < i; ++j)
      if (engineCapabilities[i].id == engineCapabilities[j].id) return false;
  return true;
}
static_assert(engineCapabilitiesUnique(), "capacidade declarada duas vezes");

} // namespace ae::core
