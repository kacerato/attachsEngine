#pragma once
// Bibliotecas de valores do Inspector: amostras de cor, presets de gradiente e
// presets de curva (Unity 6000.0 Manual/InspectorColorPicker e
// InspectorCurves — "Presets", "Libraries", "Add Factory Presets To Current
// Library"). Cada tipo tem várias bibliotecas nomeadas e uma ativa; cada
// entrada tem nome e o valor no MESMO texto do campo autoral ("r g b a" para
// cor, o formato de gradiente e o de curva de scene/script_behavior.h), então
// aplicar uma entrada é gravar esse texto pelo caminho de sempre.
//
// Moram no projeto (.astra/libraries/), não na cena: são ferramenta de
// autoria compartilhada entre cenas, sem identidade de objeto nenhuma.
#include "editor/editor_import_transaction.h"
#include "scene/script_behavior.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace ae::editor {

enum class EditorLibraryKind : u8 { Color, Gradient, Curve };

struct EditorLibraryEntry {
  std::string name, value;
};
struct EditorValueLibrary {
  std::string name;
  std::vector<EditorLibraryEntry> entries;
};

class EditorValueLibraries {
public:
  static constexpr const char *Magic = "ASTRA_VALUE_LIBRARIES_1";
  static constexpr u32 kMaximumLibraries = 64, kMaximumEntries = 256;

  EditorLibraryKind kind = EditorLibraryKind::Color;
  std::vector<EditorValueLibrary> libraries;
  u32 active = 0;

  static const char *path(EditorLibraryKind kind) {
    switch (kind) {
      case EditorLibraryKind::Color: return ".astra/libraries/colors.astra";
      case EditorLibraryKind::Gradient: return ".astra/libraries/gradients.astra";
      case EditorLibraryKind::Curve: return ".astra/libraries/curves.astra";
    }
    return "";
  }
  // O tipo de campo que valida uma entrada da biblioteca.
  static std::string_view valueType(EditorLibraryKind kind) {
    return kind == EditorLibraryKind::Color ? "color:hdr" : kind == EditorLibraryKind::Gradient ? "gradient" : "curve";
  }
  static bool validName(std::string_view name) {
    if (name.empty() || name.size() > 64 || name.find_first_not_of(' ') == std::string_view::npos) return false;
    for (unsigned char c : name) if (c < 32 || c == 127) return false;
    return true;
  }

  EditorValueLibrary &current() {
    if (libraries.empty()) libraries.push_back({"Padrão", {}});
    if (active >= libraries.size()) active = 0;
    return libraries[active];
  }
  const EditorValueLibrary *currentOrNull() const {
    return libraries.empty() ? nullptr : &libraries[std::min<usize>(active, libraries.size() - 1)];
  }

  // Sem arquivo é uma biblioteca "Padrão" vazia, não um erro.
  bool load(const std::string &root, EditorLibraryKind which, std::string &error) {
    kind = which;
    libraries.clear();
    active = 0;
    root_ = root;
    std::filesystem::path file;
    if (root.empty() || !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root), path(kind), file)) {
      error = "Projeto indisponível";
      return false;
    }
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) return true;
    std::vector<u8> bytes;
    if (!EditorImportTransaction::read(file, bytes, 8u * 1024u * 1024u)) { error = "Não foi possível ler a biblioteca"; return false; }
    std::istringstream in(std::string(bytes.begin(), bytes.end()));
    in.imbue(std::locale::classic());
    std::string magic;
    u32 count = 0, selected = 0;
    if (!(in >> magic >> selected >> count) || magic != Magic || count > kMaximumLibraries) {
      error = "Biblioteca inválida";
      return false;
    }
    std::vector<EditorValueLibrary> candidate;
    for (u32 i = 0; i < count; ++i) {
      EditorValueLibrary library;
      u32 entries = 0;
      if (!(in >> std::quoted(library.name) >> entries) || !validName(library.name) || entries > kMaximumEntries) {
        error = "Biblioteca inválida";
        return false;
      }
      for (u32 k = 0; k < entries; ++k) {
        EditorLibraryEntry entry;
        if (!(in >> std::quoted(entry.name) >> std::quoted(entry.value)) || !validName(entry.name) ||
            !scene::validScriptPropertyValue(valueType(kind), entry.value)) {
          error = "Entrada inválida na biblioteca";
          return false;
        }
        library.entries.push_back(std::move(entry));
      }
      candidate.push_back(std::move(library));
    }
    in >> std::ws;
    if (!in.eof()) { error = "Dados extras na biblioteca"; return false; }
    libraries = std::move(candidate);
    active = selected < libraries.size() ? selected : 0;
    return true;
  }
  bool save(std::string &error) const {
    std::filesystem::path file;
    if (root_.empty() || !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root_), path(kind), file)) {
      error = "Abra um projeto para guardar a biblioteca";
      return false;
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << Magic << ' ' << active << ' ' << libraries.size() << '\n';
    for (const auto &library : libraries) {
      out << std::quoted(library.name) << ' ' << library.entries.size() << '\n';
      for (const auto &entry : library.entries) out << std::quoted(entry.name) << ' ' << std::quoted(entry.value) << '\n';
    }
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    if (ec || !EditorImportTransaction::writeText(file, out.str())) { error = "Não foi possível gravar a biblioteca"; return false; }
    return true;
  }

  // Operações da Unity sobre a biblioteca ativa. Falso quando o valor, o nome
  // ou o índice não servem; nada muda nesse caso.
  bool add(std::string_view name, std::string_view value) {
    auto &library = current();
    if (library.entries.size() >= kMaximumEntries || !validName(name) || !scene::validScriptPropertyValue(valueType(kind), value)) return false;
    library.entries.push_back({std::string(name), std::string(value)});
    return true;
  }
  bool replace(u32 index, std::string_view value) {
    auto &library = current();
    if (index >= library.entries.size() || !scene::validScriptPropertyValue(valueType(kind), value)) return false;
    library.entries[index].value = value;
    return true;
  }
  bool remove(u32 index) {
    auto &library = current();
    if (index >= library.entries.size()) return false;
    library.entries.erase(library.entries.begin() + index);
    return true;
  }
  bool move(u32 index, u32 target) {
    auto &library = current();
    if (index >= library.entries.size() || target >= library.entries.size()) return false;
    auto entry = std::move(library.entries[index]);
    library.entries.erase(library.entries.begin() + index);
    library.entries.insert(library.entries.begin() + target, std::move(entry));
    return true;
  }
  bool rename(u32 index, std::string_view name) {
    auto &library = current();
    if (index >= library.entries.size() || !validName(name)) return false;
    library.entries[index].name = name;
    return true;
  }
  bool createLibrary(std::string_view name) {
    if (libraries.size() >= kMaximumLibraries || !validName(name)) return false;
    for (const auto &library : libraries) if (library.name == name) return false;
    current();
    libraries.push_back({std::string(name), {}});
    active = static_cast<u32>(libraries.size() - 1);
    return true;
  }
  bool select(u32 index) {
    if (index >= libraries.size()) return false;
    active = index;
    return true;
  }
  // Próximo nome livre "Base N" para salvar sem pedir texto (a Unity também
  // salva direto e deixa renomear depois).
  std::string nextName(std::string_view base) {
    auto &library = current();
    for (u32 n = static_cast<u32>(library.entries.size()) + 1;; ++n) {
      std::string candidate = std::string(base) + " " + std::to_string(n);
      bool used = false;
      for (const auto &entry : library.entries) used = used || entry.name == candidate;
      if (!used) return candidate;
    }
  }

private:
  std::string root_;
};

} // namespace ae::editor
