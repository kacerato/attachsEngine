#pragma once
#include <filesystem>
#include <string>
#include <string_view>
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
  bool createDirectory(const std::string &relative);
  bool createTextFile(const std::string &relative,std::string_view text);
  // Renomear e mover são a MESMA operação aqui, como no registro: o que muda é
  // o caminho. Falha fechada — destino já ocupado, caminho fora do projeto, ou
  // uma pasta movida para dentro dela mesma não acontecem pela metade.
  bool movePath(const std::string &relative,const std::string &destination);
  // Apaga arquivo ou pasta inteira. Recusa a raiz e recusa `.astra`, que é o
  // estado do projeto e não um recurso do usuário.
  bool removePath(const std::string &relative);
  // Relê a árvore visível do disco, preservando o que estava expandido.
  //
  // `refresh` só recarrega a pasta corrente (`entries_`); a árvore que o painel
  // desenha é outra estrutura, montada por `toggle`. Sem isto, um arquivo
  // renomeado continuava aparecendo com o nome antigo até o projeto ser
  // reaberto — e o nome antigo já não existia no disco.
  bool rebuildTree();
  // `.astra` guarda histórico, registro e cache — estado do projeto, não
  // recurso do usuário. Ele some da navegação normal, e some da árvore de
  // verdade: mostrá-lo convida a apagar, e apagá-lo custa o projeto inteiro.
  //
  // O caminho continua resolvível para quem souber pedir por nome: é assim que
  // um diagnóstico ainda pode apontar para dentro dele.
  void showProjectState(bool value) { showProjectState_=value; }
  bool showingProjectState() const noexcept { return showProjectState_; }
  // Verdadeiro quando o caminho já existe dentro da raiz do projeto.
  bool exists(const std::string &relative) const;
  std::string rootPath() const;
  std::string resolveFile(const std::string &relative) const;
private:
  bool resolve(const std::string &relative,std::filesystem::path &out) const;
  std::filesystem::path root_;
  bool showProjectState_=false;
  std::string current_,error_;
  std::vector<EditorFileEntry> entries_,tree_;
};
}
