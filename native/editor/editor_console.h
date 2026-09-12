// O console editorial: o que o projeto tem a dizer, num lugar só.
//
// Hoje o compilador fala numa lista apertada dentro do editor de código e os
// scripts falam no `logcat`, que não existe para quem usa o aparelho. Um erro
// de execução não tem onde aparecer. Este armazém junta as três fontes e
// preserva de cada linha o que é preciso para IR até ela: arquivo e linha, ou
// o objeto da cena.
//
// Sem Vulkan, sem Android, sem I/O: recebe linhas prontas e é testável no host.
#pragma once

#include "core/base.h"
#include "editor/editor_code_workspace.h"

#include <span>
#include <string>
#include <vector>

namespace ae::editor {

enum class EditorConsoleSeverity : u8 { Info, Warning, Error };

// De onde a linha veio. Separar as origens é o que permite limpar o bloco do
// compilador sem apagar o histórico de execução, e vice-versa.
enum class EditorConsoleOrigin : u8 { Editor, Compiler, Script };

struct EditorConsoleEntry final {
  EditorConsoleSeverity severity = EditorConsoleSeverity::Info;
  EditorConsoleOrigin origin = EditorConsoleOrigin::Editor;
  std::string message;
  // Onde o problema mora. Vazio quando não há para onde ir.
  std::string file;
  u32 line = 0;
  u32 column = 0;
  // Objeto da cena que originou a linha. Zero quando não há.
  u64 object = 0;
  // Quantas vezes esta MESMA linha se repetiu em seguida.
  u32 repeats = 1;
};

class EditorConsole final {
public:
  // Um `Update` que registra a cada quadro produz 60 linhas por segundo. Sem
  // teto, o console cresceria até o editor engasgar; com teto, a linha mais
  // antiga sai e a mais nova entra, que é o que se espera de um console.
  static constexpr usize Capacity = 512;

  void add(EditorConsoleEntry entry) {
    if (entry.message.empty()) return;
    // Repetição CONSECUTIVA vira contagem, não linha nova. É o que torna
    // legível um script que fala todo quadro: uma linha com "×137" diz mais do
    // que 137 linhas iguais, e custa menos.
    if (!entries_.empty()) {
      EditorConsoleEntry &last = entries_.back();
      if (last.severity == entry.severity && last.origin == entry.origin &&
          last.message == entry.message && last.file == entry.file &&
          last.line == entry.line && last.object == entry.object) {
        if (last.repeats < 0xFFFFFFFFu) ++last.repeats;
        return;
      }
    }
    if (entries_.size() >= Capacity) entries_.erase(entries_.begin());
    entries_.push_back(std::move(entry));
  }

  // O bloco do compilador troca INTEIRO a cada compilação.
  //
  // Um diagnóstico antigo de um arquivo que agora compila é mentira, e mentira
  // que o usuário persegue: ele abre a linha apontada e encontra código certo.
  // Por isso não é acrescentar — é substituir.
  void replaceCompiler(std::span<const EditorCodeDiagnostic> diagnostics) {
    std::vector<EditorConsoleEntry> kept;
    kept.reserve(entries_.size());
    for (auto &entry : entries_) {
      if (entry.origin == EditorConsoleOrigin::Compiler) continue;
      // Tirar o bloco do compilador pode encostar duas linhas iguais que antes
      // tinham um diagnóstico entre elas. A contagem acontece na inserção, e
      // sem esta passagem o console acumularia pares repetidos a cada
      // compilação — exatamente no painel que existe para não repetir.
      if (!kept.empty() && kept.back().severity == entry.severity &&
          kept.back().origin == entry.origin && kept.back().message == entry.message &&
          kept.back().file == entry.file && kept.back().line == entry.line &&
          kept.back().object == entry.object) {
        kept.back().repeats += entry.repeats;
        continue;
      }
      kept.push_back(std::move(entry));
    }
    entries_ = std::move(kept);
    for (const auto &diagnostic : diagnostics) {
      EditorConsoleEntry entry;
      entry.severity = diagnostic.error ? EditorConsoleSeverity::Error
                                        : EditorConsoleSeverity::Warning;
      entry.origin = EditorConsoleOrigin::Compiler;
      entry.message = diagnostic.code.empty() ? diagnostic.message
                                              : diagnostic.code + ": " + diagnostic.message;
      entry.file = diagnostic.file;
      entry.line = diagnostic.line;
      entry.column = diagnostic.column;
      add(std::move(entry));
    }
  }

  void clear() noexcept { entries_.clear(); }

  std::span<const EditorConsoleEntry> entries() const noexcept { return entries_; }

  u32 count(EditorConsoleSeverity severity) const noexcept {
    u32 total = 0;
    for (const auto &entry : entries_) if (entry.severity == severity) total += entry.repeats;
    return total;
  }

  bool visible(EditorConsoleSeverity severity) const noexcept {
    return visible_[static_cast<usize>(severity)];
  }
  void setVisible(EditorConsoleSeverity severity, bool on) noexcept {
    visible_[static_cast<usize>(severity)] = on;
  }
  void toggle(EditorConsoleSeverity severity) noexcept {
    setVisible(severity, !visible(severity));
  }

  // Índices das linhas que passam pelo filtro, da mais antiga para a mais nova.
  // Índices e não cópias: a lista é desenhada virtualizada, e copiar o texto de
  // quinhentas linhas por quadro para mostrar vinte seria trabalho jogado fora.
  std::vector<u32> filtered() const {
    std::vector<u32> result;
    result.reserve(entries_.size());
    for (u32 index = 0; index < entries_.size(); ++index)
      if (visible(entries_[index].severity)) result.push_back(index);
    return result;
  }

  const EditorConsoleEntry *at(u32 index) const noexcept {
    return index < entries_.size() ? &entries_[index] : nullptr;
  }

private:
  std::vector<EditorConsoleEntry> entries_;
  bool visible_[3]{true, true, true};
};

} // namespace ae::editor
