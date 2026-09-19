// LOD Group autoral (G3): a métrica e a escolha de nível são as da Unity.
//
// Protegido aqui: a altura relativa na tela decide o nível; o nível escolhido
// fica visível com a subárvore dele e os outros somem; abaixo do último nível
// o grupo inteiro some; um objeto comum a dois níveis não some por estar no
// nível que não foi escolhido; e referência fora da hierarquia do grupo não
// esconde nada.
#include "harness.h"
#include "editor/editor_document.h"
#include "runtime/lod_groups.h"

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace ae;
using namespace ae::editor;

namespace {
bool contains(const std::vector<runtime::ObjectId> &ids, runtime::ObjectId id) {
  return std::binary_search(ids.begin(), ids.end(), id);
}

struct Rig {
  EditorDocument doc;
  EditorEntityId group = 0, level[3]{}, mesh[3]{};
};

// Grupo na origem com três níveis como filhos, cada um com uma malha dentro.
// Tamanho 2, FOV de 60°: altura relativa = 1,732 / distância. Com as
// transições padrão (60, 30, 10) o LOD 0 vale até 2,89 m, o LOD 1 até 5,77 m e
// o LOD 2 até 17,3 m; além disso, nada.
void build(Rig &rig) {
  rig.group = rig.doc.createEntity(rig.doc.root(), EditorEntityKind::Folder, "Árvore");
  const char *names[]{"LOD0", "LOD1", "LOD2"};
  for (u32 i = 0; i < 3; ++i) {
    rig.level[i] = rig.doc.createEntity(rig.group, EditorEntityKind::Folder, names[i]);
    rig.mesh[i] = rig.doc.createEntity(rig.level[i], EditorEntityKind::Mesh, "Malha");
  }
  auto values = *rig.doc.find(rig.group);
  auto *lod = static_cast<scene::LodGroup *>(values.components.add(scene::LodGroup::descriptor));
  lod->size = 2;
  for (u32 i = 0; i < 3; ++i) lod->levels[i] = rig.level[i];
  rig.doc.applyEntityValues(rig.group, values);
}

runtime::LodView at(float distance) {
  runtime::LodView view;
  view.position[2] = -distance;
  view.verticalFov = 60 * .017453292519943295f;
  return view;
}
} // namespace

AE_TEST(lod_relative_height_is_the_unity_screen_fraction) {
  const float fov = 60 * .017453292519943295f;
  AE_EXPECT_TRUE(std::abs(scene::lodRelativeHeight(2, 1, fov, 0) - 1.7320508f) < 1e-4f, "tamanho sobre a altura vista");
  AE_EXPECT_TRUE(std::abs(scene::lodRelativeHeight(2, 2, fov, 0) - .8660254f) < 1e-4f, "cai com a distância");
  AE_EXPECT_TRUE(std::abs(scene::lodRelativeHeight(2, 50, fov, 5) - .2f) < 1e-6f, "ortográfica não depende da distância");
  AE_EXPECT_TRUE(std::isinf(scene::lodRelativeHeight(2, 0, fov, 0)), "dentro do grupo: nível mais detalhado");

  scene::LodGroup group;
  AE_EXPECT_EQ(scene::selectLodGroupLevel(group, .7f), 0u, "acima de 60%: LOD 0");
  AE_EXPECT_EQ(scene::selectLodGroupLevel(group, .6f), 0u, "exatamente na transição ainda é o nível");
  AE_EXPECT_EQ(scene::selectLodGroupLevel(group, .2f), 2u, "entre 30% e 10%: LOD 2");
  AE_EXPECT_EQ(scene::selectLodGroupLevel(group, .05f), 3u, "abaixo do último: Culled");
}

AE_TEST(lod_group_hides_every_level_but_the_chosen_one) {
  Rig rig;
  build(rig);
  auto hidden = runtime::lodHiddenObjects(rig.doc, at(2));
  AE_EXPECT_TRUE(!contains(hidden, rig.mesh[0]) && contains(hidden, rig.mesh[1]) && contains(hidden, rig.mesh[2]),
                 "perto: só o LOD 0 fica");
  AE_EXPECT_TRUE(contains(hidden, rig.level[1]), "o objeto do nível some junto com a subárvore");
  AE_EXPECT_TRUE(!contains(hidden, rig.group), "o próprio grupo nunca some");

  hidden = runtime::lodHiddenObjects(rig.doc, at(4));
  AE_EXPECT_TRUE(contains(hidden, rig.mesh[0]) && !contains(hidden, rig.mesh[1]) && contains(hidden, rig.mesh[2]), "médio: LOD 1");
  hidden = runtime::lodHiddenObjects(rig.doc, at(10));
  AE_EXPECT_TRUE(contains(hidden, rig.mesh[0]) && contains(hidden, rig.mesh[1]) && !contains(hidden, rig.mesh[2]), "longe: LOD 2");
  hidden = runtime::lodHiddenObjects(rig.doc, at(30));
  AE_EXPECT_TRUE(contains(hidden, rig.mesh[0]) && contains(hidden, rig.mesh[1]) && contains(hidden, rig.mesh[2]),
                 "abaixo do último nível: tudo some (Culled)");

  // A escala do grupo entra no tamanho: dobrar a escala dobra a distância.
  auto values = *rig.doc.find(rig.group);
  for (float &s : values.transform.scale) s = 2;
  AE_EXPECT_TRUE(rig.doc.applyEntityValues(rig.group, values), "escala aplicada");
  hidden = runtime::lodHiddenObjects(rig.doc, at(4));
  AE_EXPECT_TRUE(!contains(hidden, rig.mesh[0]), "com escala 2, a 4 m ainda é LOD 0");
}

AE_TEST(lod_group_keeps_an_object_shared_by_the_chosen_level) {
  // A mesma base em LOD 0 e LOD 1 (a Unity deixa um renderer em vários níveis):
  // escolhido o LOD 0, ela não pode sumir por também estar no LOD 1.
  Rig rig;
  build(rig);
  auto values = *rig.doc.find(rig.group);
  auto *lod = static_cast<scene::LodGroup *>(values.components.edit(scene::LodGroup::descriptor));
  lod->levels[1] = rig.mesh[0];
  AE_EXPECT_TRUE(rig.doc.applyEntityValues(rig.group, values), "nível 1 aponta para a malha do nível 0");
  const auto hidden = runtime::lodHiddenObjects(rig.doc, at(2));
  AE_EXPECT_TRUE(!contains(hidden, rig.mesh[0]), "o objeto do nível escolhido fica, mesmo listado em outro nível");
}

AE_TEST(lod_group_ignores_references_outside_its_hierarchy) {
  Rig rig;
  build(rig);
  const auto stranger = rig.doc.createEntity(rig.doc.root(), EditorEntityKind::Mesh, "Fora");
  auto values = *rig.doc.find(rig.group);
  auto *lod = static_cast<scene::LodGroup *>(values.components.edit(scene::LodGroup::descriptor));
  lod->levels[1] = stranger;
  rig.doc.applyEntityValues(rig.group, values);
  const auto hidden = runtime::lodHiddenObjects(rig.doc, at(2));
  AE_EXPECT_TRUE(!contains(hidden, stranger), "um objeto de fora do grupo nunca é escondido por ele");
}

AE_TEST(lod_group_file_round_trips_and_refuses_bad_transitions) {
  scene::LodGroup group;
  group.levelCount = 2;
  group.transitions = {50, 12, 5, 1};
  group.levels = {7, 9, 0, 0};
  group.size = 3.5f;
  std::stringstream out;
  group.write(out);
  scene::LodGroup read;
  AE_EXPECT_TRUE(read.read(out, 1), "relê");
  AE_EXPECT_TRUE(read.valid() && read.levelCount == 2 && read.levels[1] == 9 && read.size == 3.5f, "igual ao gravado");

  scene::LodGroup bad;
  bad.transitions = {30, 30, 10, 5};
  AE_EXPECT_TRUE(!bad.valid(), "dois níveis na mesma altura nunca seriam escolhidos");
  bad.transitions = {60, 30, 10, 5};
  bad.size = 0;
  AE_EXPECT_TRUE(!bad.valid(), "tamanho zero não projeta nada");
}
