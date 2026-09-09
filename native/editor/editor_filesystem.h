#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace ae::editor {
struct EditorFileEntry {
  std::string name, relativePath;
  bool directory=false;
  unsigned depth=0;
  bool expanded=false;
};
// Navegação restrita ao projeto. Não interpreta um arquivo como asset importado.
// Executar apenas por ação explícita; nenhuma leitura de disco por frame.
class EditorFileSystem final {
public:
  static constexpr unsigned MaximumEntries=4096;
  bool setRoot(const char *path);
  bool open(const std::string &relative);
  bool refresh() { return open(current_); }
  bool up();
  bool toggle(unsigned index);
  const std::vector<EditorFileEntry> &tree() const { return tree_; }
  const std::vector<EditorFileEntry> &entries() const { return entries_; }
  const std::string &current() const { return current_; }
  const std::string &error() const { return error_; }
  bool ready() const { return !root_.empty(); }
  std::string resolveFile(const std::string &relative) const;
private:
  bool resolve(const std::string &relative,std::filesystem::path &out) const;
  std::filesystem::path root_;
  std::string current_,error_;
  std::vector<EditorFileEntry> entries_,tree_;
};
}
