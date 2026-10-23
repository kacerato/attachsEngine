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
inline constexpr std::array<EngineCapability, 68> engineCapabilities{{
  // --- Luz -----------------------------------------------------------------
  {"render.light.directional", "Sol direcional", CapabilityState::Implemented,
   "renderer/punctual_lights.h + rhi/shaders/material_shading.glsl", ""},
  {"render.light.punctual", "Luzes pontuais e spot", CapabilityState::Implemented,
   "renderer/punctual_lights.h (até 16 por quadro, 8 por objeto)", ""},
  {"render.light.additional_directional", "Direcionais adicionais (lua, preenchimento)", CapabilityState::Implemented,
   "renderer/punctual_lights.h + rhi/shaders/dirt_road_shading.glsl", "Sem sombra: só o sol principal tem cascatas, como a luz principal da URP"},
  {"render.light.layers", "Máscara de culling e camadas de renderização da luz", CapabilityState::Implemented,
   "renderer/punctual_lights.h (máscara por desenho) + rhi/shaders/dirt_road_vertex.glsl", ""},
  {"render.light.sun_source", "Fonte do sol explícita (Sun Source)", CapabilityState::Implemented,
   "scene/lighting_settings.h + renderer/punctual_lights.h", ""},
  {"render.light.baked", "Luz Mista ou Assada (bake de iluminação)", CapabilityState::Planned,
   "renderer/lightmap_baker (PK-6)", "Sem bake: toda luz é de tempo real; o modo fica gravado"},
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
  {"render.shadow.renderer", "Projetar e receber sombra por Mesh Renderer", CapabilityState::Implemented,
   "platform/android/instanced_renderer.cpp (passes de sombra) + rhi/shaders/dirt_road_shading.glsl", ""},

  // --- Ambiente, GI e sondas ----------------------------------------------
  {"render.ambient.hemispheric", "Luz ambiente (Skybox, Gradiente, Cor)", CapabilityState::Implemented,
   "renderer/environment_ambient.cpp + rhi/shaders/environment_lighting.glsl",
   "Environment Lighting da janela Iluminação: sonda SH L2 do céu procedural, do HDRI, de um gradiente ou de uma cor, com Intensity Multiplier; o nível Constante usa o hemisfério céu/chão da mesma sonda"},
  {"render.ambient.specular", "Reflexo especular do ambiente", CapabilityState::Implemented,
   "renderer/environment_map.cpp", ""},
  {"render.gi.lightmap", "Lightmap indireto externo por submalha", CapabilityState::DeviceLimited,
   "scene/lightmap_binding.h + rhi/shaders/dirt_road_shading.glsl", "RGB linear de irradiância; UV1 autoral; exige bindless. Sem bake, unwrap ou probes"},
  {"render.probe.irradiance", "Sondas de luz (Light Probe Group, SH L2 por objeto)", CapabilityState::Implemented,
   "renderer/light_probes.cpp + platform/android/instanced_probes.inl + rhi/shaders/probe_lighting.glsl",
   "Bake por captura no aparelho: sol, céu e um rebate; luzes de tempo real não entram no SH, como na Unity. Sem Anchor Override nem LPPV"},
  {"render.probe.reflection", "Sonda de reflexo local (Baked/Realtime, box projection, 2 por objeto)", CapabilityState::Implemented,
   "renderer/reflection_probes.cpp + platform/android/instanced_probes.inl + rhi/shaders/probe_lighting.glsl",
   "8 sondas por quadro, 256 px por face; sem tipo Custom, Shadow Distance nem oclusão própria da captura"},

  // --- Material ------------------------------------------------------------
  {"render.material.pbr", "Shader do Material: Lit (metálico/especular), Simple Lit, Unlit, Particles", CapabilityState::Implemented,
   "rhi/shaders/dirt_road_shading.glsl + editor/editor_material_fields.h",
   "PK-9. Altura e detalhe exigem bindless; sem ele o material desenha sem esses mapas"},
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
  {"render.material.clearcoat", "Camada de verniz (Clear Coat do Complex Lit)", CapabilityState::Implemented,
   "rhi/shaders/dirt_road_shading.glsl", "PK-9: sol e reflexo do ambiente no verniz; luzes adicionais só na camada de baixo. Sem transmissão"},

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
  {"render.motion_vectors", "Vetores de movimento por pixel", CapabilityState::Implemented,
   "platform/android/instanced_motion.inl + rhi/shaders/temporal_projection.glsl",
   "Transparência e água usam as máscaras de reatividade/composição, não vetor próprio"},
  {"render.skinning", "Malha deformada por esqueleto", CapabilityState::Implemented,
   "editor/editor_map_scene.cpp (paleta) + platform/android/instanced_skinning.inl (compute)",
   "Até 4 influências por vértice e 256 juntas por skin; morph targets não são importados"},

  // --- Céu e atmosfera ----------------------------------------------------
  {"render.environment.time_of_day", "Hora do dia", CapabilityState::Implemented,
   "renderer/time_of_day.cpp + runtime/scene_lights.cpp",
   "Sol por hora, data e latitude, cor por elevação, céu procedural de dia, crepúsculo e noite e fase da lua pela data; sem luz direcional da lua nem eventos de nascer e pôr do sol"},
  {"render.environment.clouds", "Nuvens (Cloud Layer)", CapabilityState::Implemented,
   "renderer/cloud_layer.cpp + rhi/shaders/cloud_layer.glsl",
   "Uma camada 2D com mapa procedural, vento, autossombra em 3 passos e sombra no chão pela luz direcional principal; sem mapa de nuvens importado, sem segunda camada nem nuvens volumétricas"},
  {"render.environment.weather", "Clima (chuva, neve, relâmpago)", CapabilityState::Implemented,
   "runtime/scene_weather.cpp + runtime/scene_particles.cpp",
   "Componente Clima escreve as nuvens e a neblina do próprio Ambiente e a emissão de partículas de Chuva e Neve sobre a câmera; relâmpago é clarão nas nuvens e na neblina, sem raio desenhado, trovão nem respingo no chão (colisão de partícula é o P4)"},
  {"render.environment.celestial", "Sol, lua e estrelas no céu", CapabilityState::Implemented,
   "rhi/shaders/dirt_road_sky.frag",
   "Disco solar com limbo e auréola, lua com fase iluminada pelo sol e estrelas noturnas; lua e estrelas ainda sem órbita astronômica nem cintilação por turbulência real"},
  {"render.environment.atmosphere", "Céu atmosférico", CapabilityState::Implemented,
   "runtime/scene_environment.cpp + rhi/shaders/dirt_road_sky.frag", ""},
  {"render.environment.physical_atmosphere", "Céu físico Rayleigh/Mie", CapabilityState::Implemented,
   "renderer/scene_environment.cpp + rhi/shaders/dirt_road_sky.frag", ""},
  {"render.environment.fog", "Neblina por profundidade", CapabilityState::Implemented,
   "rhi/shaders/post_process_common.glsl",
   "Fog da janela Iluminação: Linear (Início/Fim), Exponencial e Exponencial ao quadrado, cor própria ou acompanhando a luz do céu, decaimento por altura nos modos exponenciais"},
  {"render.environment.skybox", "Material do céu (Skybox Material)", CapabilityState::Implemented,
   "resources/environment_profile.cpp + runtime/scene_environment.cpp",
   "Recurso .skybox com shader Procedural (Sun, Sun Size, Convergence, Atmosphere Thickness, Exposure), Panorâmico (HDRI) ou Físico, escolhido na janela Iluminação; sem Cubemap de 6 faces nem override por câmera"},
  {"render.environment.volumes", "Volumes de ambiente por câmera", CapabilityState::Implemented,
   "runtime/scene_environment.cpp + renderer/scene_environment.cpp", ""},

  // --- Câmera (PK-7) -------------------------------------------------------
  {"render.camera.background", "Fundo da câmera: céu, cor sólida ou sem limpeza", CapabilityState::Implemented,
   "platform/android/instanced_renderer.cpp", ""},
  {"render.camera.culling_mask", "Culling Mask por camada de objeto", CapabilityState::Implemented,
   "platform/android/instanced_renderer.cpp",
   "Esconde o desenho e a sombra do objeto nesta câmera; luzes ainda não filtram por camada"},
  {"render.camera.viewport", "Viewport Rect e várias câmeras na tela por prioridade", CapabilityState::Implemented,
   "editor/editor_scene_camera.h + platform/android/android_main.cpp",
   "Até duas câmeras visíveis ao mesmo tempo; a de cima sai sem sombras e sem TAA"},
  {"render.camera.post", "Pós, sombras, HDR e antisserrilhado por câmera", CapabilityState::Implemented,
   "platform/android/instanced_renderer.cpp", "TAA por câmera só quando o projeto já aloca o histórico"},
  {"render.camera.physical", "Câmera física: focal, sensor e Gate Fit", CapabilityState::Implemented,
   "scene/camera.h", "Sem deslocamento de lente, abertura, ISO ou obturador"},
  {"render.camera.stack", "Camera Stack (Overlay) e Target Texture", CapabilityState::Planned,
   "platform/android/instanced_renderer.cpp",
   "Exige composição com alfa sobre a câmera Base e um recurso Render Texture amostrável por material"},
  {"render.camera.msaa", "MSAA por câmera", CapabilityState::Planned,
   "platform/android/instanced_renderer.cpp", "O renderer não tem passe multiamostrado; AA é FXAA ou TAA"},

  // --- Pós-processamento ---------------------------------------------------
  {"render.post.tonemap", "Exposição e mapeamento de tom", CapabilityState::Implemented,
   "rhi/shaders/post_process_common.glsl", ""},
  {"render.post.auto_exposure", "Exposição automática por histograma", CapabilityState::Implemented,
   "platform/android/instanced_auto_exposure.inl", "Histograma e adaptação por vista; o EV fica restrito à faixa autoral"},
  {"render.post.bloom", "Brilho estourado", CapabilityState::Implemented,
   "rhi/shaders/post_process_common.glsl", ""},
  {"render.post.film_grain", "Grão de filme", CapabilityState::Implemented,
   "rhi/shaders/post_process_common.glsl", ""},
  {"render.post.color_grading", "Gradação de cor por LUT 32³ e perfis prontos", CapabilityState::Implemented,
   "renderer/color_grading.cpp + rhi/shaders/post_process_common.glsl",
   "Balanço de branco, lift/gama/ganho, sombras/médios/realces e vibração; sem mixer de canais, curvas nem .cube"},
  {"render.post.ambient_occlusion", "Oclusão ambiente em tela", CapabilityState::Implemented,
   "rhi/shaders/post_process_common.glsl", ""},
  {"render.aa.fxaa", "Antisserrilhado espacial", CapabilityState::Implemented,
   "rhi/shaders/post_process_common.glsl", ""},
  {"render.aa.temporal", "Antisserrilhado temporal", CapabilityState::DeviceLimited,
   "rhi/shaders/post_process_temporal.frag", "Exige histórico e profundidade alocáveis no backend"},
  {"render.upscale.temporal", "Ampliação temporal Arm ASR / AMD FSR 2", CapabilityState::DeviceLimited,
   "rhi/temporal_upscaler.cpp + platform/android/instanced_temporal_upscaler.inl",
   "Exige float16/int16, formatos de storage e subgrupos no aparelho; recusa com motivo quando falta"},

  // --- Partículas (G5) -----------------------------------------------------
  {"render.particles.simulation", "Simulação de partículas em CPU", CapabilityState::Implemented,
   "renderer/particles/particle_simulation.cpp + runtime/scene_particles.cpp",
   "Main, Emission (taxa e rajadas), Shape (6 formas), Velocity/Limit Velocity/Force over Lifetime, Noise (ruído de valor) e Cor/Tamanho/Rotação ao longo da vida; jobs por sistema, SoA, semente determinística"},
  {"render.particles.billboard", "Partículas billboard e esticadas", CapabilityState::Implemented,
   "platform/android/instanced_particles.inl + rhi/shaders/particle_billboard.vert", "Ordenação por sistema e dentro do sistema; desenhadas depois dos transparentes da malha"},
  {"render.particles.material", "Material de partícula mistura/aditivo", CapabilityState::Implemented,
   "rhi/shaders/particle_material.frag + renderer/particles/particle_textures.cpp",
   "Texturas próprias da engine (8 formas, 3 ruídos, 3 máscaras); textura do projeto ainda não"},
  {"render.particles.soft", "Partícula suave por profundidade", CapabilityState::DeviceLimited,
   "platform/android/instanced_particles.inl", "Exige profundidade da cena amostrável; sem ela a partícula fica com borda dura"},
  {"render.particles.mesh", "Partícula de malha", CapabilityState::Planned, "renderer/particles/", "Pacote P2"},
  {"render.particles.trails", "Rastros, TrailRenderer e LineRenderer", CapabilityState::Planned, "renderer/particles/", "Pacote P3"},
  {"render.particles.collision", "Colisão de partículas com planos", CapabilityState::Implemented,
   "renderer/particles/particle_simulation.cpp",
   "Modo Planes da Unity: até dois objetos-plano com quique, amortecimento, perda de vida e velocidade mínima; sem colisão com a física da cena (modo World)"},
  {"render.particles.sub_emitters", "Sub-emissores", CapabilityState::Implemented, "runtime/scene_particles.cpp",
   "Ao nascer, ao colidir e ao morrer, com quantidade por evento; o sub-sistema usa a própria forma e visual. Sem herdar cor, tamanho ou velocidade"},
  {"render.particles.compute", "Simulação de partículas em compute (níveis A/S)", CapabilityState::Planned, "renderer/particles/", "Pacote P5"},

  // --- Animação ------------------------------------------------------------
  {"animation.clip", "Clipe de animação por nós", CapabilityState::Implemented,
   "runtime/scene_animation.cpp + resources/skeletal_animation.cpp",
   "Translação, rotação e escala; pesos de morph não; sem mistura entre clipes"},
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
