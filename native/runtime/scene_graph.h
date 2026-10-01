// O grafo de cena do runtime: a hierarquia de objetos que o jogo executa.
//
// Este arquivo não conhece editor, UI, histórico nem seleção. Ele é a estrutura
// de dados que o mundo de execução possui e que o editor também usa como
// armazenamento do documento autoral (ver editor/editor_document.h), de modo
// que física, scripts e extração de desenho leiam um único tipo de objeto.
//
// Ids nunca são reciclados dentro de um grafo: um comando antigo no histórico —
// ou um handle guardado por um script — nunca acerta outro objeto por acaso.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once
#include "core/base.h"
#include "runtime/gameplay_layers.h"
#include "runtime/object_tags.h"
#include "runtime/object_groups.h"
#include "runtime/scene_views.h"
#include "runtime/input_actions.h"
#include "scene/components.h"

#include <span>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <functional>

namespace ae::runtime {

using ObjectId = u32;
inline constexpr ObjectId kInvalidObject = 0;

enum class ObjectKind : u8 {
  // Generic object: groups descendants and can carry nonvisual components.
  Folder,
  Mesh,
  Light,
  Camera,
  Water,
  Effect,
};

inline constexpr u32 kNameCapacity = 64;

// Euler em graus, na ordem que o Inspector mostra (X, Y, Z). Graus e não
// radianos porque é o que o campo edita: converter na borda do documento
// deixaria o valor exibido diferente do valor guardado depois de ida e volta.
struct Transform final {
  float position[3]{0.0f, 0.0f, 0.0f};
  float rotationDegrees[3]{0.0f, 0.0f, 0.0f};
  float scale[3]{1.0f, 1.0f, 1.0f};
};

bool isTransformValid(const Transform &transform) noexcept;

struct SceneObject final {
  ObjectId id = kInvalidObject;
  ObjectId parent = kInvalidObject;
  ObjectKind kind = ObjectKind::Folder;
  // Terminado em zero, sempre. `name[capacity-1]` é reservado ao terminador,
  // então o nome útil tem 63 bytes.
  char name[kNameCapacity]{};
  Transform transform{};
  // Object-wide authoring flags. Camera/mesh capability lives in components.
  bool active = true;
  bool visible = true;
  bool castShadow = true;
  bool receiveShadow = true;
  bool isStatic = false;
  u32 layer = 0;
  std::string tag{ObjectTags::Untagged};
  ObjectGroups groups{};
  scene::Components components{};
  bool rigidBodyEnabled = false;
  // mass, drag, collider half-extents X/Y/Z.
  float rigidBody[5]{50, 1, .5f, .5f, .5f};
  // Root scene environment: sun, ambient, exposure multipliers; sky rotation offset in degrees.
  float environment[4]{1, 1, 1, 0};
};

// Copia com truncamento e terminador garantido. Exposta porque o histórico e os
// testes precisam montar objetos sem passar pelo grafo.
void assignObjectName(SceneObject &object, std::string_view name) noexcept;
using ObjectCloneMap = std::unordered_map<ObjectId,ObjectId>;
// Identidade de componente é local ao objeto: conserva instanceId e troca owner.
bool remapObjectReferences(SceneObject &object,const ObjectCloneMap &mapping);
bool remapSubtreeReferences(std::span<SceneObject> objects,const ObjectCloneMap &mapping);

class SceneGraph {
public:
  static constexpr u32 kMaximumObjects = 65536;

  SceneGraph();
  SceneGraph(const SceneGraph &) = default;
  SceneGraph(SceneGraph &&) noexcept = default;
  SceneGraph &operator=(const SceneGraph &) = default;
  SceneGraph &operator=(SceneGraph &&) noexcept = default;
  virtual ~SceneGraph() = default;

  // Recomeça com apenas a raiz. Os ids voltam a contar do início: um grafo novo
  // é um grafo novo, e o histórico é zerado junto por quem o possui.
  void reset();

  // A raiz existe sempre e não pode ser destruída, renomeada por comando nem
  // reparentada. Ela é o ancestral de tudo e o alvo do "adicionar à cena".
  ObjectId root() const noexcept { return rootId_; }

  ObjectId createEntity(ObjectId parent, ObjectKind kind, std::string_view name);
  ObjectId cloneSubtree(ObjectId source,ObjectId parent,ObjectCloneMap &mapping,const std::function<bool(ObjectId)> &include={});
  ObjectId cloneSubtree(const SceneGraph &sourceGraph,ObjectId source,ObjectId parent,ObjectCloneMap &mapping,const std::function<bool(ObjectId)> &include={});
  // Recria um objeto com um id específico. Existe para o `undo` de uma remoção:
  // se o id mudasse ao voltar, toda seleção e todo comando posterior no
  // histórico passariam a apontar para outra coisa.
  bool restoreEntity(const SceneObject &object, u32 childIndex);

  // Remove o objeto e toda a subárvore dele. Falso para a raiz ou id inválido.
  bool destroyEntity(ObjectId id);

  bool setName(ObjectId id, std::string_view name);
  bool setTransform(ObjectId id, const Transform &transform);
  // Substitui os campos editáveis de uma vez. `id`, `parent` e `kind` do
  // argumento são ignorados: mudar pai é reparent e mudar tipo não existe.
  bool applyEntityValues(ObjectId id, const SceneObject &values);
  bool reparent(ObjectId id, ObjectId newParent, u32 childIndex);
  bool setActive(ObjectId id, bool active);
  // Acesso mutável à coleção de componentes, marcando a revisão. **Não é o
  // caminho do editor**: quem mantém histórico passa por `applyEntityValues`,
  // senão a alteração não entra no desfazer. Existe para o mundo de execução,
  // que não tem histórico e precisa escrever componente sem copiar o objeto.
  scene::Components *editComponents(ObjectId id);

  bool exists(ObjectId id) const noexcept;
  const SceneObject *find(ObjectId id) const noexcept;
  // Índice do objeto na lista de filhos do próprio pai. É o que o `undo` de uma
  // remoção precisa para devolvê-lo à mesma linha da hierarquia.
  bool childIndexOf(ObjectId id, u32 &outIndex) const noexcept;
  std::span<const ObjectId> childrenOf(ObjectId id) const noexcept;
  // A subárvore em pré-ordem, começando pelo próprio objeto. Vazio para id
  // inválido. Usado pela remoção e por qualquer operação recursiva.
  void collectSubtree(ObjectId id, std::vector<ObjectId> &out) const;
  bool isDescendantOf(ObjectId candidate, ObjectId ancestor) const noexcept;
  // Verdadeiro quando o objeto e todos os seus ancestrais estão ativos.
  bool activeInHierarchy(ObjectId id) const noexcept;

  // Objetos vivos, incluindo a raiz. Nao e o tamanho do vetor interno: ids nunca
  // sao reciclados, entao o vetor guarda tambem os buracos dos removidos.
  u32 entityCount() const noexcept { return aliveCount_; }
  // Persist the allocation frontier when editing a reusable source: removed
  // identities must not be reassigned to unrelated objects after reopening.
  ObjectId nextObjectId() const noexcept { return nextId_; }
  bool reserveObjectIdsUntil(ObjectId next);
  // Sobe a cada mutação aceita. É o que diz ao renderer e à UI que a extração do
  // frame anterior não vale mais, sem que ninguém precise comparar estado.
  u64 revision() const noexcept { return revision_; }

  // Estado de CENA, não de objeto: as camadas de gameplay do projeto viajam com
  // o documento, são copiadas para o mundo de execução junto com a hierarquia e
  // voltam ao arquivo na gravação.
  const GameplayLayers &layers() const noexcept { return layers_; }
  // Snapshot do catálogo do projeto; o arquivo da cena guarda só a atribuição.
  const ObjectTags &tags() const noexcept {return tags_;}
  void setTags(const ObjectTags &tags) {if(tags_!=tags) {tags_=tags;++revision_;}}
  void setLayers(const GameplayLayers &value) {
    if (layers_ == value) return;
    layers_ = value;
    ++revision_;
  }
  // Enquadramentos salvos da cena: comparar duas versões de um cenário exige
  // repetir a MESMA vista, então ela é dado do documento, não do aparelho.
  const SceneViews &views() const noexcept { return views_; }
  void setViews(const SceneViews &value) {
    if (views_ == value) return;
    views_ = value;
    ++revision_;
  }
  const InputActionMap &inputActions() const noexcept { return input_; }
  bool setInputActions(const InputActionMap &value) {
    if (!value.valid()) return false;
    if (input_ == value) return true;
    input_ = value;
    ++revision_;
    return true;
  }

protected:
  // Ponto de extensão para consumidores com invariantes próprias de aparência.
  // O grafo já recusa transformação inválida e coleção de componentes inválida;
  // o editor acrescenta os limites das propriedades numéricas autorais.
  virtual bool acceptObject(const SceneObject &object) const;

private:
  struct Record final {
    SceneObject object{};
    std::vector<ObjectId> children;
    bool alive = false;
  };

  Record *record(ObjectId id) noexcept;
  const Record *record(ObjectId id) const noexcept;
  bool detachFromParent(ObjectId id);
  bool attachToParent(ObjectId id, ObjectId parent, u32 childIndex);

  std::vector<Record> records_;  // indexado por id; a posição 0 nunca é usada
  GameplayLayers layers_{};
  ObjectTags tags_{};
  SceneViews views_{};
  InputActionMap input_{};
  ObjectId rootId_ = kInvalidObject;
  ObjectId nextId_ = 1;
  u32 aliveCount_ = 0;
  u64 revision_ = 0;
};

} // namespace ae::runtime
