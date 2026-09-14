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
#include <algorithm>
#include <chrono>
#include <cctype>
#include <sstream>
#include <limits>

namespace ae::editor {

enum class EditorConsoleSeverity : u8 { Info, Warning, Error };

// De onde a linha veio. Separar as origens é o que permite limpar o bloco do
// compilador sem apagar o histórico de execução, e vice-versa.
enum class EditorConsoleOrigin : u8 { Editor, Compiler, Script, Importer };

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
  u64 eventId=0;
  u64 elapsedMs=0;
  u64 buildGeneration=0,playSession=0,component=0;
  std::string project,sourceExcerpt;
  u32 excerptLine=0;
};

class EditorConsole final {
public:
  // Um `Update` que registra a cada quadro produz 60 linhas por segundo. Sem
  // teto, o console cresceria até o editor engasgar; com teto, a linha mais
  // antiga sai e a mais nova entra, que é o que se espera de um console.
  static constexpr usize Capacity = 512;
  static constexpr usize MaximumBytes=1024*1024;
  u64 dropped() const noexcept {return dropped_;}

  void add(EditorConsoleEntry entry) {
    if (entry.message.empty()) return;
    if(entry.project.size()>1024) entry.project.resize(1024);
    if(entry.sourceExcerpt.size()>4096) entry.sourceExcerpt.clear();
    // Both the event count and its payload are bounded. A single stack trace
    // must not turn a bounded 512-entry console into an unbounded allocation.
    if(entry.message.size()>8192) {
      usize end=8188;while(end && (static_cast<unsigned char>(entry.message[end])&0xc0)==0x80) --end;
      entry.message.resize(end);entry.message+="…";
    }
    if(entry.file.size()>1024) {
      usize end=1024;while(end && (static_cast<unsigned char>(entry.file[end])&0xc0)==0x80) --end;
      entry.file.resize(end);
    }
    // Repetição CONSECUTIVA vira contagem, não linha nova. É o que torna
    // legível um script que fala todo quadro: uma linha com "×137" diz mais do
    // que 137 linhas iguais, e custa menos.
    if (!entries_.empty()) {
      EditorConsoleEntry &last = entries_.back();
      if (last.severity == entry.severity && last.origin == entry.origin &&
          last.message == entry.message && last.file == entry.file &&
          last.line == entry.line && last.column == entry.column && last.object == entry.object &&
          last.buildGeneration==entry.buildGeneration && last.playSession==entry.playSession && last.project==entry.project && last.component==entry.component) {
        if (last.repeats < 0xFFFFFFFFu) ++last.repeats;
        return;
      }
    }
    auto bytes=[&] {usize total=0;for(const auto &value:entries_) total+=value.message.size()+value.file.size()+value.project.size()+value.sourceExcerpt.size();return total;};
    while(entries_.size()>=Capacity || bytes()+entry.message.size()+entry.file.size()+entry.project.size()+entry.sourceExcerpt.size()>MaximumBytes) {
      // Logs cannot evict current compiler problems. Excess diagnostics still
      // remain available in EditorCodeWorkspace::diagnostics().
      const auto victim=std::find_if(entries_.begin(),entries_.end(),[](const auto &value) {return value.origin!=EditorConsoleOrigin::Compiler;});
      if(victim==entries_.end()) {++dropped_;return;}
      entries_.erase(victim);++dropped_;
    }
    entry.eventId=++nextId_;
    entry.elapsedMs=static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now()-started_).count());
    entries_.push_back(std::move(entry));
  }

  // O bloco do compilador troca INTEIRO a cada compilação.
  //
  // Um diagnóstico antigo de um arquivo que agora compila é mentira, e mentira
  // que o usuário persegue: ele abre a linha apontada e encontra código certo.
  // Por isso não é acrescentar — é substituir.
  void replaceCompiler(std::span<const EditorCodeDiagnostic> diagnostics,std::string_view project={}) {
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
          kept.back().file == entry.file && kept.back().line == entry.line && kept.back().column == entry.column &&
          kept.back().object == entry.object && kept.back().buildGeneration==entry.buildGeneration &&
          kept.back().playSession==entry.playSession && kept.back().project==entry.project && kept.back().component==entry.component) {
        kept.back().repeats=static_cast<u32>(std::min<u64>(
            static_cast<u64>(kept.back().repeats)+entry.repeats,std::numeric_limits<u32>::max()));
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
      entry.buildGeneration=diagnostic.generation;entry.project=project;
      entry.sourceExcerpt=diagnostic.sourceExcerpt;entry.excerptLine=diagnostic.excerptLine;
      add(std::move(entry));
    }
  }

  void clear() noexcept { entries_.clear(); }
  void clearLogs() {
    std::erase_if(entries_,[](const auto &entry) {return entry.origin!=EditorConsoleOrigin::Compiler;});
    dropped_=0;
  }

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
  std::vector<u32> filtered(int tab=-1,int origin=-1,std::string_view query={}) const {
    std::vector<u32> result;
    result.reserve(entries_.size());
    auto contains=[&](std::string_view text) {
      return std::search(text.begin(),text.end(),query.begin(),query.end(),[](unsigned char a,unsigned char b) {
        return std::tolower(a)==std::tolower(b);
      })!=text.end();
    };
    for (u32 index = 0; index < entries_.size(); ++index) {
      const auto &entry=entries_[index];
      if(!visible(entry.severity)) continue;
      if(tab==1 && entry.origin!=EditorConsoleOrigin::Compiler) continue;
      if(tab==0 && entry.origin==EditorConsoleOrigin::Compiler) continue;
      if(origin>=0 && static_cast<int>(entry.origin)!=origin) continue;
      if(!query.empty() && !contains(entry.message) && !contains(entry.file)) continue;
      result.push_back(index);
    }
    return result;
  }

  const EditorConsoleEntry *find(u64 id) const noexcept {
    for(const auto &entry:entries_) if(entry.eventId==id) return &entry;
    return nullptr;
  }
  static const char *originName(EditorConsoleOrigin origin) {
    return origin==EditorConsoleOrigin::Importer?"Importador":origin==EditorConsoleOrigin::Compiler?"Compilador":origin==EditorConsoleOrigin::Script?"Script":"Editor";
  }
  static std::string describe(const EditorConsoleEntry &entry) {
    std::ostringstream text;
    text<<"#"<<entry.eventId<<" · +"<<entry.elapsedMs/1000<<"s · "<<originName(entry.origin)<<" · "
        <<(entry.severity==EditorConsoleSeverity::Error?"Erro":entry.severity==EditorConsoleSeverity::Warning?"Aviso":"Informação");
    if(entry.repeats>1) text<<" · x"<<entry.repeats;
    text<<'\n'<<entry.message;
    if(!entry.file.empty()) {
      text<<'\n'<<entry.file;
      if(entry.line) {text<<":"<<entry.line;if(entry.column) text<<":"<<entry.column;}
    }
    if(entry.object) text<<"\nObjeto "<<entry.object;
    if(entry.component) text<<" · Componente "<<entry.component;
    if(entry.buildGeneration) text<<"\nBuild "<<entry.buildGeneration;
    if(entry.playSession) text<<" · Play "<<entry.playSession;
    if(!entry.project.empty()) text<<"\nProjeto "<<entry.project.substr(entry.project.find_last_of("/\\")+1);
    return text.str();
  }

  const EditorConsoleEntry *at(u32 index) const noexcept {
    return index < entries_.size() ? &entries_[index] : nullptr;
  }

private:
  std::vector<EditorConsoleEntry> entries_;
  u64 dropped_=0;
  u64 nextId_=0;
  std::chrono::steady_clock::time_point started_=std::chrono::steady_clock::now();
  bool visible_[3]{true, true, true};
};

} // namespace ae::editor
