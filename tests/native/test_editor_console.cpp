#include "harness.h"
#include "editor/editor_console.h"

#include <string>

using namespace ae;
using namespace ae::editor;

namespace {
EditorConsoleEntry script(const char *message, u64 object = 7) {
  EditorConsoleEntry entry;
  entry.severity = EditorConsoleSeverity::Info;
  entry.origin = EditorConsoleOrigin::Script;
  entry.message = message;
  entry.object = object;
  return entry;
}
EditorCodeDiagnostic diagnostic(const char *file, u32 line, const char *code,
                                const char *message, bool error = true) {
  EditorCodeDiagnostic out;
  out.file = file;
  out.line = line;
  out.column = 1;
  out.code = code;
  out.message = message;
  out.error = error;
  return out;
}
} // namespace

AE_TEST(a_line_repeated_every_frame_becomes_one_line_with_a_count) {
  // O caso que faz um console ser util ou inutil. Um `Update` que registra a
  // cada quadro produz 60 linhas por segundo; sem colapsar, a primeira coisa
  // interessante sai da tela em dois segundos.
  EditorConsole console;
  for (int i = 0; i < 137; ++i) console.add(script("velocidade 3.5"));
  AE_EXPECT_EQ(console.entries().size(), usize{1}, "uma linha so");
  AE_EXPECT_EQ(console.entries()[0].repeats, 137u, "com a contagem");
  AE_EXPECT_EQ(console.count(EditorConsoleSeverity::Info), 137u, "a contagem entra no total");

  // Uma linha diferente no meio quebra a sequencia: colapsar por conteudo em
  // qualquer posicao esconderia a ordem em que as coisas aconteceram.
  console.add(script("pulou"));
  console.add(script("velocidade 3.5"));
  AE_EXPECT_EQ(console.entries().size(), usize{3}, "a ordem dos acontecimentos sobrevive");
}

AE_TEST(the_console_never_grows_without_bound) {
  EditorConsole console;
  for (int i = 0; i < static_cast<int>(EditorConsole::Capacity) + 200; ++i)
    console.add(script(("linha " + std::to_string(i)).c_str()));
  AE_EXPECT_EQ(console.entries().size(), EditorConsole::Capacity, "teto respeitado");
  // A mais NOVA sobrevive: um console que descarta o fim nao serve para nada.
  AE_EXPECT_TRUE(console.entries().back().message.find("711") != std::string::npos,
                 "a linha mais recente esta la");
}

AE_TEST(a_new_build_replaces_the_compiler_block_and_keeps_the_rest) {
  // Um diagnostico antigo de um arquivo que agora compila e mentira -- e
  // mentira que o usuario persegue: ele abre a linha apontada e encontra
  // codigo certo.
  EditorConsole console;
  console.add(script("comecou"));
  const EditorCodeDiagnostic first[]{diagnostic("Scripts/A.cs", 12, "CS1002", "; esperado")};
  console.replaceCompiler(first);
  AE_EXPECT_EQ(console.entries().size(), usize{2}, "log e diagnostico convivem");
  AE_EXPECT_EQ(console.count(EditorConsoleSeverity::Error), 1u, "um erro");

  const EditorCodeDiagnostic second[]{
      diagnostic("Scripts/B.cs", 3, "CS0168", "variavel nao usada", false)};
  console.replaceCompiler(second);
  AE_EXPECT_EQ(console.count(EditorConsoleSeverity::Error), 0u, "o erro antigo saiu");
  AE_EXPECT_EQ(console.count(EditorConsoleSeverity::Warning), 1u, "o aviso novo entrou");
  bool logKept = false;
  for (const auto &entry : console.entries())
    if (entry.origin == EditorConsoleOrigin::Script) logKept = true;
  AE_EXPECT_TRUE(logKept, "o historico de execucao nao e apagado por uma compilacao");

  console.replaceCompiler({});
  AE_EXPECT_EQ(console.count(EditorConsoleSeverity::Warning), 0u, "compilacao limpa limpa o bloco");
  AE_EXPECT_EQ(console.entries().size(), usize{1}, "e so o bloco");
}

AE_TEST(the_filter_hides_lines_without_losing_them) {
  EditorConsole console;
  console.add(script("um"));
  const EditorCodeDiagnostic problems[]{
      diagnostic("Scripts/A.cs", 1, "CS1", "erro"),
      diagnostic("Scripts/A.cs", 2, "CS2", "aviso", false)};
  console.replaceCompiler(problems);
  AE_EXPECT_EQ(console.filtered().size(), usize{3}, "tudo visivel no comeco");

  console.toggle(EditorConsoleSeverity::Info);
  AE_EXPECT_EQ(console.filtered().size(), usize{2}, "sem os informativos");
  AE_EXPECT_EQ(console.entries().size(), usize{3}, "filtrar nao apaga");
  console.toggle(EditorConsoleSeverity::Warning);
  const auto onlyErrors = console.filtered();
  AE_EXPECT_EQ(onlyErrors.size(), usize{1}, "so erros");
  AE_EXPECT_TRUE(console.at(onlyErrors[0])->severity == EditorConsoleSeverity::Error, "e o erro");

  console.toggle(EditorConsoleSeverity::Info);
  console.toggle(EditorConsoleSeverity::Warning);
  AE_EXPECT_EQ(console.filtered().size(), usize{3}, "o filtro volta sem perda");
}

AE_TEST(a_line_carries_where_to_go) {
  // Sem isto o console e so texto. O que o torna util e poder tocar numa linha
  // e chegar no lugar: arquivo e linha para o compilador, objeto para o script.
  EditorConsole console;
  const EditorCodeDiagnostic problems[]{diagnostic("Scripts/Porta.cs", 42, "CS0103", "nome nao existe")};
  console.replaceCompiler(problems);
  const auto *entry = console.at(0);
  AE_EXPECT_TRUE(entry != nullptr, "linha existe");
  AE_EXPECT_TRUE(entry->file == "Scripts/Porta.cs" && entry->line == 42, "leva ate a fonte");
  AE_EXPECT_TRUE(entry->message.find("CS0103") != std::string::npos, "o codigo do erro aparece");

  console.add(script("colidiu", 91));
  AE_EXPECT_EQ(console.at(1)->object, 91u, "e o log leva ate o objeto");

  console.clear();
  AE_EXPECT_TRUE(console.entries().empty(), "limpar limpa");
  AE_EXPECT_TRUE(console.at(0) == nullptr, "e nao devolve lixo");
}

#include "editor/editor_screen.h"
#include "editor/editor_theme.h"
#include "ui/ui_draw_list.h"
#include "ui/ui_input.h"

AE_TEST(every_console_row_is_reachable_by_touch) {
  // O que este teste fixa: o console e desenhado por CIMA do editor de codigo,
  // e a cadeia de toque tem varios blocos que consomem o evento por workspace.
  // Registrar a linha nao basta -- ela precisa continuar alcancavel depois de
  // todo mundo que desenha em volta.
  //
  // Verificado ao contrario durante a construcao: com o console decidido no
  // fim da cadeia, os filtros respondiam e as LINHAS nao, porque um bloco
  // anterior ficava com o toque.
  EditorConsole console;
  const EditorCodeDiagnostic problems[]{
      diagnostic("Scripts/Porta.cs", 12, "CS1002", "; esperado"),
      diagnostic("Scripts/Porta.cs", 19, "CS0103", "nome nao existe")};
  console.replaceCompiler(problems);
  console.add(script("rodou", 5));

  EditorDocument document;
  EditorScreenState state;
  state.document = &document;
  state.console = &console;
  state.workspace = EditorWorkspace::Code;
  state.surface = {0, 0, 1400, 900};

  ui::UiDrawList list;
  ui::UiInputRouter router;
  const auto layout = buildEditorScreen(state, editorTheme(), list, router);
  AE_EXPECT_TRUE(!layout.consolePanel.isEmpty(), "o painel do console existe");
  AE_EXPECT_TRUE(layout.consoleRowCount == 3u, "as tres linhas contam");

  // Cada linha visivel tem de responder, e responder COM O SEU proprio indice:
  // um deslocamento de um faz o usuario saltar para o lugar errado, que e pior
  // do que nao saltar.
  const auto rows = console.filtered();
  const auto visible = layout.consoleVisibleRows < rows.size()
                           ? static_cast<usize>(layout.consoleVisibleRows) : rows.size();
  u32 reachable = 0;
  for (u32 index = 0; index < console.entries().size(); ++index) {
    bool found = false;
    for (float y = layout.consolePanel.y; y < layout.consolePanel.bottom(); y += 4.0f)
      for (float x = layout.consolePanel.x + 20.0f; x < layout.consolePanel.right(); x += 40.0f) {
        const auto hit = router.hitTest({x, y});
        if (hit.widgetId == widgetId(EditorWidget::ConsoleRowBase) + index) found = true;
      }
    if (found) ++reachable;
  }
  AE_EXPECT_EQ(static_cast<usize>(reachable), visible, "toda linha visivel responde ao toque");

  // Os comandos do cabecalho tambem, senao filtrar vira decoracao.
  bool clear = false, errors = false;
  for (float x = layout.consolePanel.x; x < layout.consolePanel.right(); x += 6.0f)
    for (float y = layout.consolePanel.y; y < layout.consolePanel.y + 34.0f; y += 4.0f) {
      const auto hit = router.hitTest({x, y});
      if (hit.widgetId == widgetId(EditorWidget::ConsoleClear)) clear = true;
      if (hit.widgetId == widgetId(EditorWidget::ConsoleError)) errors = true;
    }
  AE_EXPECT_TRUE(clear && errors, "limpar e o filtro de erros respondem");
}
