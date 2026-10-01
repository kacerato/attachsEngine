// A biblioteca de presets com receitas: vários componentes em um registro.
//
// O que se protege aqui é a compatibilidade e a transação. Uma biblioteca
// gravada pela versão anterior precisa continuar abrindo — um formato novo não
// pode transformar o trabalho já salvo do autor em "arquivo inválido" — e uma
// receita precisa voltar inteira ou não voltar, porque meia receita aplicada é
// um objeto que ninguém montou.
#include "harness.h"

#include "editor/editor_component_presets.h"
#include "scene/collider.h"
#include "scene/light.h"
#include "scene/physics_body.h"

#include <filesystem>
#include <fstream>

using namespace ae;

namespace {
std::string temporaryRoot(const char *name) {
  const auto path = std::filesystem::temp_directory_path() / "astra-presets" / name;
  std::error_code error;
  std::filesystem::remove_all(path, error);
  std::filesystem::create_directories(path, error);
  return path.string();
}
bool writeLibrary(const std::string &root, const std::string &text) {
  const auto path = std::filesystem::path(root) / ".astra";
  std::error_code error;
  std::filesystem::create_directories(path, error);
  std::ofstream out(path / "component-presets.astra", std::ios::binary);
  out << text;
  return out.good();
}
} // namespace

AE_TEST(preset_library_reads_the_previous_single_component_format) {
  const auto root = temporaryRoot("legacy");
  // Um registro da versão 1: id, nome, tipo, versão e payload, sem contagem.
  scene::Light light;
  light.intensity = 17;
  std::ostringstream payload;
  payload.imbue(std::locale::classic());
  light.write(payload);
  std::ostringstream file;
  file.imbue(std::locale::classic());
  file << "ASTRA_COMPONENT_PRESETS_1 1\n1 " << std::quoted(std::string("Luz quente")) << ' '
       << std::quoted(std::string(scene::Light::descriptor.id)) << ' ' << scene::Light::descriptor.version << ' '
       << std::quoted(payload.str()) << '\n';
  AE_EXPECT_TRUE(writeLibrary(root, file.str()), "biblioteca antiga escrita");

  editor::EditorComponentPresets presets;
  std::string error;
  AE_EXPECT_TRUE(presets.load(root, error), error.c_str());
  AE_EXPECT_EQ(presets.entries.size(), 1u, "o registro antigo foi lido");
  AE_EXPECT_TRUE(!presets.entries.front().recipe(), "um componente só não é receita");
  AE_EXPECT_TRUE(presets.entries.front().contains(scene::Light::descriptor.id), "o tipo sobreviveu à migração de formato");
}

AE_TEST(preset_library_stores_and_reloads_a_recipe) {
  const auto root = temporaryRoot("recipe");
  editor::EditorComponentPresets presets;
  std::string error;
  AE_EXPECT_TRUE(presets.load(root, error), error.c_str());

  // Um objeto de porta: corpo físico + colisor. A ordem é a do objeto.
  scene::Components components;
  auto *body = components.add(scene::PhysicsBody::descriptor);
  static_cast<scene::PhysicsBody &>(*body).mass = 12;
  auto *collider = components.add(scene::Collider::descriptor);
  static_cast<scene::Collider &>(*collider).halfY = 2;

  AE_EXPECT_TRUE(presets.captureRecipe("Porta", components, error), error.c_str());
  AE_EXPECT_EQ(presets.entries.size(), 1u, "a receita virou um registro só");
  AE_EXPECT_TRUE(presets.entries.front().recipe(), "duas entradas fazem uma receita");

  editor::EditorComponentPresets reopened;
  AE_EXPECT_TRUE(reopened.load(root, error), error.c_str());
  AE_EXPECT_EQ(reopened.entries.size(), 1u, "a biblioteca reabriu");
  const auto id = reopened.entries.front().id;
  auto values = reopened.instantiateAll(id, error);
  AE_EXPECT_EQ(values.size(), 2u, "a receita volta inteira");
  AE_EXPECT_TRUE(&values[0]->type() == &scene::PhysicsBody::descriptor, "a ordem gravada é preservada");
  AE_EXPECT_TRUE(static_cast<const scene::PhysicsBody &>(*values[0]).mass == 12, "os valores voltam");
  AE_EXPECT_TRUE(static_cast<const scene::Collider &>(*values[1]).halfY == 2, "o segundo componente também");

  // `instantiate` continua sendo a porta de um componente só: pedir um valor
  // único de uma receita precisa ser recusado com motivo, não devolver o
  // primeiro componente como se fosse o preset inteiro.
  AE_EXPECT_TRUE(reopened.instantiate(id, error) == nullptr, "uma receita não é um componente");
}

AE_TEST(recipe_capture_rejects_partial_composition_without_modifying_library) {
  const auto root = temporaryRoot("partial");
  editor::EditorComponentPresets presets;
  std::string error;
  AE_EXPECT_TRUE(presets.load(root, error), error.c_str());

  scene::Components components;
  components.add(scene::Light::descriptor);
  auto *script = components.add(scene::ScriptBehavior::descriptor);
  static_cast<scene::ScriptBehavior &>(*script).scriptType = "Porta";

  AE_EXPECT_TRUE(!presets.captureRecipe("Mista", components, error) && !error.empty(), "não descartar comportamento silenciosamente");
  AE_EXPECT_TRUE(presets.entries.empty(), "nenhuma receita parcial é publicada");
}

AE_TEST(recipe_rejects_a_repeated_singleton_type_on_load) {
  const auto root = temporaryRoot("repeated");
  scene::Light light;
  std::ostringstream payload;
  payload.imbue(std::locale::classic());
  light.write(payload);
  std::ostringstream file;
  file.imbue(std::locale::classic());
  file << "ASTRA_COMPONENT_PRESETS_2 1\n1 " << std::quoted(std::string("Duas luzes")) << " 2";
  for (u32 i = 0; i < 2; ++i)
    file << ' ' << std::quoted(std::string(scene::Light::descriptor.id)) << ' ' << scene::Light::descriptor.version
         << ' ' << std::quoted(payload.str());
  file << '\n';
  AE_EXPECT_TRUE(writeLibrary(root, file.str()), "biblioteca escrita");

  editor::EditorComponentPresets presets;
  std::string error;
  AE_EXPECT_TRUE(!presets.load(root, error), "uma receita impossível de aplicar é recusada na leitura");
}
