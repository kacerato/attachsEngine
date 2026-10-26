#pragma once
// Conversão do Particle System da Unity (classID 198) e do Particle System
// Renderer (199) para o Sistema de partículas da Astra (G6a).
//
// Formato estudado: a serialização de texto da Unity (Manual/FormatDescription
// e Manual/ParticleSystemModules), módulo a módulo. Cada campo com consumidor
// na Astra é convertido para a convenção daqui: reflexão LH→RH em Z (vetores
// com z negado, giros X/Y negados), radianos→graus, cores sRGB→linear e a
// frente da forma virada para −Z local, como o objeto refletido pede.
//
// O que não tem consumidor não some em silêncio: `losses` lista o que muda a
// aparência (módulo ignorado, malha de forma, distorção) e `approximations` o
// que fica parecido (modo de curva, eixo único, ruído sem as três curvas).
#include "resources/unity_yaml.h"
#include "scene/particle_system.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace ae::resources {

struct UnityParticleImport {
  scene::ParticleSystemSettings settings;
  bool rendererEnabled = true;
  // Material do renderer (slot 0) e o da trilha (slot 1, a Astra desenha a
  // trilha com o do sistema). Malha do Render Mode Mesh quando não é embutida.
  UnityObjectReference material, trailMaterial, mesh;
  // Referências locais do prefab, por fileID, resolvidas pelo conversor de
  // prefab: Transform dos planos de colisão, ParticleSystem dos sub-emissores,
  // Light da luz modelo.
  i64 collisionPlanes[2]{};
  i64 subEmitterBirth = 0, subEmitterCollision = 0, subEmitterDeath = 0;
  i64 lightTemplate = 0;
  // Mais sub-emissores (fileID do ParticleSystem, acontecimento 0/1/2).
  std::array<std::pair<i64, u32>, scene::ParticleSystemSettings::kExtraSubEmitters> extraSubEmitters{};
  std::vector<std::string> losses, approximations;
};

// Unity: m_RenderMode 4 (Mesh) aponta para uma malha; as primitivas embutidas
// (GUID 0000000000000000e000000000000000) viram as malhas embutidas da Astra.
bool unityBuiltinParticleMesh(const UnityObjectReference &mesh, scene::ParticleMeshShape &shape, bool &approximate);

// `renderer` pode faltar (sistema sem renderer não desenha). Falso só para
// dados malformados ou fora do contrato; módulos sem consumidor viram perdas.
bool convertUnityParticleSystem(const UnityYamlNode &system, const UnityYamlNode *renderer, UnityParticleImport &out,
                                std::string &diagnostic);

// sRGB (o seletor de cor da Unity) para linear, como a Unity faz num projeto
// linear antes de desenhar.
float unityLinearFromSrgb(float value);

} // namespace ae::resources
