#include "harness.h"
#include "editor/editor_code_workspace.h"
#include "editor/editor_filesystem.h"

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

// O relatório do compilador, no formato que `applyBuildReport` lê. Montado aqui
// em vez de invocado do Roslyn: o que está sob teste é a política de publicação,
// não o compilador.
std::string report(bool success, const char *typeId, const char *file) {
  std::string text = "ASTRA_CODE 1 ";
  text += success ? "1 0 " : "0 1 ";
  if (!success) text += "\"Comportamento.cs\" 3 5 1 \"CS1002\" \"; esperado\" ";
  text += success ? "1 " : "0 ";
  if (success) {
    text += std::string("\"") + typeId + "\" \"Comportamento\" \"" + file + "\" 1 ";
    text += "\"velocidade\" \"Velocidade\" \"float\"";
  }
  return text;
}

// Um workspace com um arquivo aberto e um catálogo já publicado, que é o estado
// em que o defeito aparecia.
struct Workspace {
  fs::path root;
  EditorFileSystem files;
  EditorCodeWorkspace code;

  Workspace() {
    root = scratch("catalogo");
    fs::create_directories(root / "Scripts");
    std::ofstream(root / "Scripts" / "Comportamento.cs") << "public sealed class Comportamento {}\n";
  }
  ~Workspace() {
    std::error_code ignored;
    fs::remove_all(root, ignored);
  }
  bool start() {
    if (!files.setRoot(root.string().c_str())) return false;
    if (!code.open(files, "Scripts/Comportamento.cs")) return false;
    return publish("project.Comportamento");
  }
  bool publish(const char *typeId) {
    const auto generation = code.generation();
    if (!code.applyBuildReport(report(true, typeId, "Scripts/Comportamento.cs"), generation)) return false;
    code.publishBuild();
    return true;
  }
  bool edit(const char *text) {
    const auto *buffer = code.active();
    return buffer && code.replace(buffer->id, buffer->revision, text);
  }
};
} // namespace

AE_TEST(editing_code_never_erases_the_published_script_catalogue) {
  // O defeito que este teste fixa: digitar no editor apagava a lista de tipos,
  // o componente sumia de "Adicionar componente" e do inspetor — e continuava
  // anexado e executando no Play. Instância, schema e versão em execução são
  // três coisas distintas.
  Workspace workspace;
  AE_EXPECT_TRUE(workspace.start(), "catálogo publicado");
  AE_EXPECT_EQ(workspace.code.scriptTypes().size(), 1u, "um tipo publicado");
  AE_EXPECT_TRUE(workspace.code.catalogState() == EditorCodeCatalogState::Current,
                 "publicado corresponde ao texto");

  AE_EXPECT_TRUE(workspace.edit("public sealed class Comportamento { int a; }"), "editar");
  AE_EXPECT_EQ(workspace.code.scriptTypes().size(), 1u, "o tipo continua conhecido depois de editar");
  AE_EXPECT_TRUE(workspace.code.catalogState() == EditorCodeCatalogState::Stale,
                 "o estado diz que o texto está à frente do publicado");

  AE_EXPECT_TRUE(workspace.code.undo(), "desfazer");
  AE_EXPECT_EQ(workspace.code.scriptTypes().size(), 1u, "desfazer não apaga o catálogo");
  AE_EXPECT_TRUE(workspace.code.redo(), "refazer");
  AE_EXPECT_EQ(workspace.code.scriptTypes().size(), 1u, "refazer não apaga o catálogo");

  // Criar um arquivo novo também não pode derrubar o que já foi publicado.
  AE_EXPECT_TRUE(workspace.code.createScript(workspace.files, "Outro"), "criar script");
  AE_EXPECT_EQ(workspace.code.scriptTypes().size(), 1u, "criar arquivo não apaga o catálogo");
}

AE_TEST(a_failed_build_keeps_the_catalogue_it_promises_to_keep) {
  Workspace workspace;
  AE_EXPECT_TRUE(workspace.start(), "catálogo publicado");
  AE_EXPECT_TRUE(workspace.edit("public sealed class Comportamento { int a "), "texto quebrado");

  const auto generation = workspace.code.generation();
  AE_EXPECT_TRUE(!workspace.code.applyBuildReport(report(false, "", ""), generation), "compilação falha");
  // A mensagem sempre prometeu isto; agora é verdade.
  AE_EXPECT_TRUE(workspace.code.error().find("preservada") != std::string::npos,
                 "a mensagem promete preservar a versão anterior");
  AE_EXPECT_EQ(workspace.code.scriptTypes().size(), 1u, "e ela é de fato preservada");
  AE_EXPECT_TRUE(workspace.code.catalogState() == EditorCodeCatalogState::Failed, "estado de falha");
  AE_EXPECT_EQ(workspace.code.diagnostics().size(), 1u, "o erro aparece nos diagnósticos");
}

AE_TEST(compiled_types_appear_only_after_the_host_accepts_the_build) {
  // Publicação atômica: anunciar tipos de um assembly que o hospedeiro não
  // carregou faria o inspetor prometer o que o Play não executaria.
  Workspace workspace;
  AE_EXPECT_TRUE(workspace.start(), "catálogo publicado");
  AE_EXPECT_TRUE(workspace.code.scriptTypes()[0].id == "project.Comportamento", "tipo inicial");

  AE_EXPECT_TRUE(workspace.edit("public sealed class Renomeado {}"), "editar");
  const auto generation = workspace.code.generation();
  AE_EXPECT_TRUE(workspace.code.applyBuildReport(report(true, "project.Renomeado", "Scripts/Comportamento.cs"),
                                                 generation),
                 "compilação aceita");
  // Compilou, mas ainda não foi publicada: o catálogo continua o anterior.
  AE_EXPECT_TRUE(workspace.code.scriptTypes()[0].id == "project.Comportamento",
                 "o tipo novo não aparece antes da publicação");

  workspace.code.discardBuild();
  AE_EXPECT_TRUE(workspace.code.scriptTypes()[0].id == "project.Comportamento",
                 "publicação recusada mantém o catálogo anterior");
  AE_EXPECT_TRUE(workspace.code.catalogState() != EditorCodeCatalogState::Failed,
                 "recusar publicação não é falha de compilação");

  AE_EXPECT_TRUE(workspace.code.applyBuildReport(report(true, "project.Renomeado", "Scripts/Comportamento.cs"),
                                                 generation),
                 "compilação de novo");
  workspace.code.publishBuild();
  AE_EXPECT_TRUE(workspace.code.scriptTypes()[0].id == "project.Renomeado", "agora o tipo novo vale");
  AE_EXPECT_TRUE(workspace.code.catalogState() == EditorCodeCatalogState::Current, "em dia com o texto");
}

AE_TEST(a_late_build_report_cannot_overwrite_a_newer_generation) {
  // Build A começa, o usuário edita, build B termina primeiro. O resultado
  // atrasado de A não pode substituir B nem publicar tipos de um texto que não
  // existe mais.
  Workspace workspace;
  AE_EXPECT_TRUE(workspace.start(), "catálogo publicado");
  const auto antiga = workspace.code.generation();
  AE_EXPECT_TRUE(workspace.edit("public sealed class Comportamento { int b; }"), "texto mais novo");

  AE_EXPECT_TRUE(!workspace.code.applyBuildReport(report(true, "project.Antigo", "Scripts/Comportamento.cs"),
                                                  antiga),
                 "relatório de geração antiga é recusado");
  AE_EXPECT_TRUE(workspace.code.scriptTypes()[0].id == "project.Comportamento",
                 "e não publica nada");
}

AE_TEST(closing_the_project_is_the_only_thing_that_clears_the_catalogue) {
  Workspace workspace;
  AE_EXPECT_TRUE(workspace.start(), "catálogo publicado");
  workspace.code.clear();
  AE_EXPECT_TRUE(workspace.code.scriptTypes().empty(), "fechar o projeto limpa o catálogo");
  AE_EXPECT_TRUE(workspace.code.catalogState() == EditorCodeCatalogState::Empty, "estado vazio");
  AE_EXPECT_EQ(workspace.code.publishedGeneration(), 0u, "sem publicação");
}

AE_TEST(typing_is_one_undo_entry_per_session_not_one_per_key) {
  // Escrever direto no editor significa uma chamada por tecla. `replace` guarda
  // uma copia do arquivo inteiro a cada chamada: por tecla, desfazer voltaria
  // caractere a caractere e o historico cresceria com uma copia do arquivo por
  // tecla digitada. `type` agrupa a sessao inteira numa entrada so.
  Workspace workspace;
  AE_EXPECT_TRUE(workspace.start(), "catalogo publicado");
  const auto *buffer = workspace.code.active();
  AE_EXPECT_TRUE(buffer != nullptr, "arquivo aberto");
  const auto id = buffer->id;
  const std::string base = buffer->text;

  AE_EXPECT_TRUE(workspace.code.type(id, base + "a"), "tecla 1");
  AE_EXPECT_TRUE(workspace.code.type(id, base + "ab"), "tecla 2");
  AE_EXPECT_TRUE(workspace.code.type(id, base + "abc"), "tecla 3");
  AE_EXPECT_TRUE(workspace.code.active()->text == base + "abc", "o texto entra ao vivo");
  AE_EXPECT_TRUE(workspace.code.typing(), "a sessao esta aberta");

  AE_EXPECT_TRUE(workspace.code.undo(), "desfazer");
  AE_EXPECT_TRUE(workspace.code.active()->text == base,
                 "desfazer volta a sessao inteira, nao uma tecla");
  AE_EXPECT_TRUE(!workspace.code.typing(), "desfazer fecha a sessao");

  AE_EXPECT_TRUE(workspace.code.redo(), "refazer");
  AE_EXPECT_TRUE(workspace.code.active()->text == base + "abc", "refazer devolve a sessao inteira");

  // Fechar o campo e voltar a digitar comeca uma entrada nova.
  workspace.code.endTypingRun();
  AE_EXPECT_TRUE(workspace.code.type(id, base + "abcd"), "sessao nova");
  AE_EXPECT_TRUE(workspace.code.undo(), "desfazer a sessao nova");
  AE_EXPECT_TRUE(workspace.code.active()->text == base + "abc",
                 "volta ao fim da sessao anterior, nao ao inicio de tudo");

  // Arquivo errado nao aceita tecla: um buffer trocado no meio da digitacao
  // escreveria o texto de um arquivo dentro de outro.
  AE_EXPECT_TRUE(!workspace.code.type(id + 999u, "qualquer"), "id de outro arquivo e recusado");
}
