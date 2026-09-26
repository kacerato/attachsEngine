// Inspector e Hierarquia com o Play rodando.
//
// Unity 6000.0 (Manual/GameObjects e ScriptReference/EditorApplication-isPlaying):
// com o jogo em execução o Inspector continua editável, a mudança vale na hora
// para o jogo e é descartada ao sair do Play. Na Astra o mundo de execução é o
// `runtime::GameWorld`, que já recusa o que não pode mudar em Play; o editor
// não ganha um segundo caminho de escrita.
//
// O Inspector escreve num ESPELHO do grafo do mundo (um `EditorDocument` com
// histórico descartável) usando exatamente os mesmos controles da edição
// autoral. Esta função compara duas fotos do espelho e aplica ao mundo só o que
// mudou, pela API pública dele: nome, estado, camada, flags de desenho, pose,
// pai, criação e remoção de objetos, componentes adicionados e removidos, e
// cada propriedade que mudou (número, booleano, enumeração, referência, valor
// por slot e recurso). Campos que o mundo recusa voltam em `refused` com o
// motivo; o que o mundo mudou por conta própria desde a foto não é revertido,
// porque só o que o usuário tocou é escrito.
#pragma once
#include "editor/editor_play_scene.h"
#include "runtime/game_world.h"
#include "runtime/scene_graph.h"

#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ae::editor {

struct PlayEditContext {
  EditorPlayScene &play;
  const resources::AssetRegistry &assets;
  std::span<const resources::EnvironmentProfile> environmentProfiles{};
  runtime::ComponentResourceResolver resolveResource{};
};

struct PlayEditResult {
  u32 applied = 0;
  // Primeira recusa, pronta para a barra de estado; vazia quando tudo entrou.
  // `refusedField` é só o nome do campo, para caber na faixa do Inspector.
  std::string refused;
  std::string refusedField;
  // Objetos criados no espelho e o id que receberam no mundo.
  std::vector<std::pair<runtime::ObjectId, runtime::ObjectId>> created;
};

PlayEditResult applyPlayEdits(const runtime::SceneGraph &before, const runtime::SceneGraph &after,
                              const PlayEditContext &context);

} // namespace ae::editor
