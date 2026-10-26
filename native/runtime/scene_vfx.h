#pragma once
// Comportamentos de efeito em Play (G6a): Projétil, Laser, Instanciar ao
// colidir, Girar e Som repetido (scene/vfx_behaviors.h). Tudo passa pelo mundo do Play
// como um script passaria: pose pelo caminho de transform, prefabs pelo
// instanciar do mundo, destruição adiada pela fila, áudio pelos comandos da
// Fonte. Nada fica escondido: os pontos da Linha que o Laser escreve aparecem
// no Inspector do Play.
#include "runtime/game_world.h"
#include "runtime/prefab.h"
#include "runtime/scene_audio.h"
#include "runtime/scene_particles.h"
#include "runtime/scene_physics.h"

#include <functional>
#include <map>
#include <string>
#include <utility>

namespace ae::runtime {

class SceneVfx final {
public:
  using PrefabLoader = std::function<bool(resources::AssetGuid, Prefab &, std::string &)>;
  struct Report {
    u32 projectiles = 0, lasers = 0, sounds = 0, rotators = 0, spawned = 0, refused = 0;
    std::string lastError;
  };
  void reset();
  void setPrefabLoader(PrefabLoader loader) { loader_ = std::move(loader); }
  // Depois da física e antes das partículas: projéteis andam e acertam, lasers
  // miram, sons tocam no tempo deles.
  bool advance(GameWorld &world, const ScenePhysics &physics, SceneParticles &particles, SceneAudio &audio, float elapsed);
  // Depois das partículas: os toques deste quadro viram efeitos.
  bool spawnCollisions(GameWorld &world, const SceneParticles &particles);
  const Report &report() const noexcept { return report_; }

private:
  using Key = std::pair<ObjectId, u64>;
  struct ProjectileState { bool started = false; };
  struct SoundState { double time = 0, next = 0; bool started = false; };
  // Cria o prefab sob `parent`, na pose de mundo dada, e o destrói depois de
  // `lifetime` segundos (negativo: a duração das partículas da raiz ou do
  // primeiro filho, como os scripts dos pacotes). Devolve o handle criado.
  ObjectHandle spawn(GameWorld &world, resources::AssetGuid prefab, const ObjectHandle &parent, const float (&position)[3],
                     const float (&rotation)[4], double lifetime);

  PrefabLoader loader_;
  std::map<Key, ProjectileState> projectiles_;
  std::map<Key, SoundState> sounds_;
  std::map<Key, bool> destroying_;
  std::map<resources::AssetGuid, Prefab> prefabs_;
  u32 random_ = 0x9e3779b9u;
  Report report_;
};

} // namespace ae::runtime
