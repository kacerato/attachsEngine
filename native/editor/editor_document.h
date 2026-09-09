// O documento de cena do editor: o que a hierarquia mostra, o que o gizmo move
// e o que o Inspector edita.
//
// **Toda mutação passa por aqui, e o documento nunca é editado direto pela UI.**
// É o que torna undo/redo possível sem que cada painel se lembre de implementar
// o seu (ver editor_history.h). Um campo que a UI pudesse escrever sem passar
// por um comando seria um campo que o Ctrl+Z não desfaz — e o usuário não tem
// como saber quais são.
//
// A entidade é um POD de tamanho fixo, com nome em `char[]` e sem ponteiros.
// Isso não é economia de alocação: é o que permite que o histórico guarde
// "antes" e "depois" copiando a struct inteira, sem clonagem profunda e sem uma
// segunda representação que possa divergir do original.
//
// Este é o modelo do EDITOR, não o do runtime. Ele descreve o que o usuário
// autorou; a extração para lotes de renderização e para a ECS em C# é uma
// projeção dele, e mora em outro lugar.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once
#include "renderer/water_route.h"

#include "core/base.h"
#include "renderer/material_override.h"

#include <span>
#include <string_view>
#include <vector>

namespace ae::editor {

// Zero nunca é uma entidade válida: é o pai da raiz e o valor de "nada
// selecionado". Um id é estável pela vida do documento e nunca é reciclado,
// então um comando antigo no histórico nunca acerta um objeto diferente.
using EditorEntityId = u32;
inline constexpr EditorEntityId kInvalidEntity = 0;

enum class EditorEntityKind : u8 {
  // Transform-only grouping object; moving it transforms its descendants.
  Folder,
  Mesh,
  Light,
  Camera,
  Water,
  Effect,
};

inline constexpr u32 kEditorNameCapacity = 64;

// Euler em graus, na ordem que o Inspector mostra (X, Y, Z). Graus e não
// radianos porque é o que o campo edita: converter na borda do documento
// deixaria o valor exibido diferente do valor guardado depois de ida e volta.
struct EditorTransform final {
  float position[3]{0.0f, 0.0f, 0.0f};
  float rotationDegrees[3]{0.0f, 0.0f, 0.0f};
  float scale[3]{1.0f, 1.0f, 1.0f};
};

bool isTransformValid(const EditorTransform &transform) noexcept;

struct EditorEntity final {
  EditorEntityId id = kInvalidEntity;
  EditorEntityId parent = kInvalidEntity;
  EditorEntityKind kind = EditorEntityKind::Folder;
  // Terminado em zero, sempre. `name[capacity-1]` é reservado ao terminador,
  // então o nome útil tem 63 bytes.
  char name[kEditorNameCapacity]{};
  EditorTransform transform{};
  // Authoring flags consumed by extraction and Inspector.
  bool active = true;
  bool visible = true;
  bool castShadow = true;
  bool receiveShadow = true;
  bool isStatic = false;
  u32 layer = 0;
  // Recurso que dá corpo à entidade: malha, perfil de luz, material de água.
  // Zero é "nenhum" — uma pasta e uma entidade recém-criada não têm.
  u32 assetId = 0;
  renderer::MaterialOverride material{};
  renderer::WaterRoute route{};
  bool waterPhysicsEnabled=true;
  bool waterInfinite=false;
  bool rigidBodyEnabled=false;
  // depth, uniform current X/Z, wave gain, foam gain, ripple gain, optical gain.
  float waterBody[7]{3,0,0,1,1,1,1};
  // mass, drag, collider half-extents X/Y/Z.
  float rigidBody[5]{50,1,.5f,.5f,.5f};
  // Root scene environment: sun, ambient, exposure multipliers; sky rotation offset in degrees.
  float environment[4]{1,1,1,0};
  bool waterEnabled=false;
  bool waterSpectrumEnabled=false;
  bool waterLayoutEnabled=false;
  // count, log2 FFT resolution, wavelength interval, seed, domain scale, MiB budget.
  float waterLayout[9]{3,7,2,2048,1,1,8,1,1};
  float water[34]{3,1,1,1.6f,.72f,1,1.05f,.14f,.1f,1.333f,0,0,1400,
    10,100000,20,.8f,.2f,.1f, 0,65,100000,1,.1f,.35f,
    1,1,1, 1,1,1, .8f,4,.5f};
};

class EditorDocument final {
public:
  static constexpr u32 kMaximumEntities = 65536;

  EditorDocument();

  // Recomeça com apenas a raiz. Os ids voltam a contar do início: um documento
  // novo é um documento novo, e o histórico é zerado junto por quem o possui.
  void reset();

  // A raiz existe sempre e não pode ser destruída, renomeada por comando nem
  // reparentada. Ela é o ancestral de tudo e o alvo do "adicionar à cena".
  EditorEntityId root() const noexcept { return rootId_; }

  EditorEntityId createEntity(EditorEntityId parent, EditorEntityKind kind,
                              std::string_view name);
  // Recria uma entidade com um id específico. Existe para o `undo` de uma
  // remoção: se o id mudasse ao voltar, toda seleção e todo comando posterior
  // no histórico passariam a apontar para outra coisa.
  bool restoreEntity(const EditorEntity &entity, u32 childIndex);

  // Remove a entidade e toda a subárvore dela. Falso para a raiz ou id inválido.
  bool destroyEntity(EditorEntityId id);

  bool setName(EditorEntityId id, std::string_view name);
  bool setTransform(EditorEntityId id, const EditorTransform &transform);
  // Substitui os campos editáveis de uma vez. `id`, `parent` e `kind` do
  // argumento são ignorados: mudar pai é reparent e mudar tipo não existe.
  bool applyEntityValues(EditorEntityId id, const EditorEntity &values);
  bool reparent(EditorEntityId id, EditorEntityId newParent, u32 childIndex);

  bool exists(EditorEntityId id) const noexcept;
  const EditorEntity *find(EditorEntityId id) const noexcept;
  // Índice da entidade na lista de filhos do próprio pai. É o que o `undo` de
  // uma remoção precisa para devolver o objeto à mesma linha da hierarquia.
  bool childIndexOf(EditorEntityId id, u32 &outIndex) const noexcept;
  std::span<const EditorEntityId> childrenOf(EditorEntityId id) const noexcept;
  // A subárvore em pré-ordem, começando pela própria entidade. Vazio para id
  // inválido. Usado pela remoção e por qualquer operação recursiva.
  void collectSubtree(EditorEntityId id, std::vector<EditorEntityId> &out) const;
  bool isDescendantOf(EditorEntityId candidate, EditorEntityId ancestor) const noexcept;

  // Entidades vivas, incluindo a raiz. Nao e o tamanho do vetor interno: ids
  // nunca sao reciclados, entao o vetor guarda tambem os buracos dos removidos.
  u32 entityCount() const noexcept { return aliveCount_; }
  // Sobe a cada mutação aceita. É o que diz ao renderer e à UI que a extração
  // do frame anterior não vale mais, sem que ninguém precise comparar estado.
  u64 revision() const noexcept { return revision_; }

private:
  struct Record final {
    EditorEntity entity{};
    std::vector<EditorEntityId> children;
    bool alive = false;
  };

  Record *record(EditorEntityId id) noexcept;
  const Record *record(EditorEntityId id) const noexcept;
  bool detachFromParent(EditorEntityId id);
  bool attachToParent(EditorEntityId id, EditorEntityId parent, u32 childIndex);

  std::vector<Record> records_;  // indexado por id; a posição 0 nunca é usada
  EditorEntityId rootId_ = kInvalidEntity;
  EditorEntityId nextId_ = 1;
  u32 aliveCount_ = 0;
  u64 revision_ = 0;
};

// Copia com truncamento e terminador garantido. Exposta porque o histórico e os
// testes precisam montar entidades sem passar pelo documento.
void assignEntityName(EditorEntity &entity, std::string_view name) noexcept;

} // namespace ae::editor
