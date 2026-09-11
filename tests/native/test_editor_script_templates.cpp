#include "harness.h"
#include "editor/editor_code_workspace.h"
#include "editor/editor_filesystem.h"
#include "editor/editor_script_templates.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace ae;
using namespace ae::editor;
namespace fs = std::filesystem;

namespace {
fs::path scratch(const char *prefix) {
  return fs::temp_directory_path() /
         (std::string("aether-") + prefix + "-" +
          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}
std::string readAll(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
} // namespace

AE_TEST(script_templates_carry_a_class_name_description_and_compilable_source) {
  AE_EXPECT_TRUE(!editorScriptTemplates.empty(), "há modelos");
  AE_EXPECT_TRUE(!kEditorScriptContracts.empty(), "arquivo de contratos embutido");
  bool anyContract = false;
  for (const auto &model : editorScriptTemplates) {
    AE_EXPECT_TRUE(!model.className.empty() && !model.name.empty() && !model.description.empty(),
                   "modelo identificado e descrito");
    // O nome da classe precisa aparecer no código: é o que a criação substitui.
    AE_EXPECT_TRUE(model.source.find(model.className) != std::string_view::npos,
                   "o modelo declara a própria classe");
    AE_EXPECT_TRUE(model.source.find("Behavior") != std::string_view::npos, "deriva de Behavior");
    anyContract = anyContract || model.contracts;
  }
  AE_EXPECT_TRUE(anyContract, "ao menos um modelo interage por capacidade");
}

AE_TEST(creating_from_a_template_renames_the_class_and_brings_its_contracts) {
  const auto root = scratch("templates");
  fs::create_directories(root);
  EditorFileSystem files;
  AE_EXPECT_TRUE(files.setRoot(root.string().c_str()), "projeto aberto");

  // Encontra o primeiro modelo que depende dos contratos: é o caso que também
  // precisa trazer o arquivo auxiliar.
  u32 index = 0;
  while (index < editorScriptTemplates.size() && !editorScriptTemplates[index].contracts) ++index;
  AE_EXPECT_TRUE(index < editorScriptTemplates.size(), "modelo com contratos");
  const auto &model = editorScriptTemplates[index];

  EditorCodeWorkspace workspace;
  AE_EXPECT_TRUE(workspace.createScript(files, "PortaDoPatio", index), workspace.error().c_str());
  const auto created = readAll(root / "Scripts" / "PortaDoPatio.cs");
  AE_EXPECT_TRUE(!created.empty(), "arquivo criado");
  AE_EXPECT_TRUE(created.find("class PortaDoPatio") != std::string::npos, "classe renomeada");
  AE_EXPECT_TRUE(created.find("project.PortaDoPatio") != std::string::npos, "ComponentId acompanha o nome");
  AE_EXPECT_TRUE(created.find(std::string(model.className)) == std::string::npos,
                 "nenhum vestígio do nome do modelo");
  AE_EXPECT_TRUE(fs::exists(root / "Scripts" / "Contratos.cs"), "contratos acompanham o modelo");

  // Criar um segundo script do mesmo modelo não pode reescrever os contratos:
  // o usuário pode ter acrescentado uma capacidade própria lá.
  const auto contractsPath = root / "Scripts" / "Contratos.cs";
  {
    std::ofstream out(contractsPath, std::ios::binary | std::ios::app);
    out << "\n// acrescentado pelo usuario\n";
  }
  const auto edited = readAll(contractsPath);
  AE_EXPECT_TRUE(workspace.createScript(files, "PortaDaSala", index), workspace.error().c_str());
  AE_EXPECT_EQ(readAll(contractsPath), edited, "contratos preservados");

  // Sem modelo, o arquivo vazio de sempre.
  AE_EXPECT_TRUE(workspace.createScript(files, "Solto"), workspace.error().c_str());
  const auto empty = readAll(root / "Scripts" / "Solto.cs");
  AE_EXPECT_TRUE(empty.find("class Solto : Behavior") != std::string::npos, "arquivo vazio com a classe pedida");
  AE_EXPECT_TRUE(empty.find("IInteragivel") == std::string::npos, "sem modelo não traz contrato nenhum");

  std::error_code code;
  fs::remove_all(root, code);
}
