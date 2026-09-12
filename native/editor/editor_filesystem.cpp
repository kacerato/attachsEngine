#include "editor/editor_filesystem.h"
#include <algorithm>
#include <cstdio>

namespace ae::editor {
namespace fs=std::filesystem;
namespace {
fs::path fromUtf8(std::string_view bytes) { return fs::path(std::u8string(bytes.begin(),bytes.end())); }
std::string utf8(const fs::path &path) {
  const auto bytes=path.generic_u8string();
  return {reinterpret_cast<const char *>(bytes.data()),bytes.size()};
}
}
bool EditorFileSystem::setRoot(const char *path) {
  root_.clear();current_.clear();entries_.clear();tree_.clear();error_.clear();
  if(!path || !*path) {error_="Projeto sem pasta";return false;}
  std::error_code error;
  auto candidate=fs::canonical(fromUtf8(path),error);
  if(error || !fs::is_directory(candidate,error) || error) {error_="Pasta do projeto indisponível";return false;}
  root_=std::move(candidate);
  if(!open("")) return false;
  tree_={{"Projeto", "", true, 0, true}};
  for(auto entry:entries_) {entry.depth=1;tree_.push_back(std::move(entry));}
  return true;
}
bool EditorFileSystem::resolve(const std::string &relative,fs::path &out) const {
  if(root_.empty() || relative.find('\0')!=std::string::npos) return false;
  const auto input=fromUtf8(relative);
  if(input.is_absolute() || input.has_root_name()) return false;
  std::error_code error;
  out=fs::canonical(root_/input,error);
  if(error) return false;
  auto a=root_.begin(),b=out.begin();
  for(;a!=root_.end();++a,++b) if(b==out.end() || *a!=*b) return false;
  return true;
}
// Resolve um caminho que ainda NAO existe: `resolve` usa `canonical`, que
// exige o arquivo no disco. Para um destino o que importa e que o pai exista e
// que o resultado continue dentro do projeto.
static bool resolveTarget(const fs::path &root,const std::string &relative,fs::path &out) {
  if(root.empty() || relative.empty() || relative.find(char(0))!=std::string::npos) return false;
  const auto input=fromUtf8(relative);
  if(input.is_absolute() || input.has_root_name()) return false;
  for(const auto &part:input) if(part=="..") return false;
  out=(root/input).lexically_normal();
  auto a=root.begin(),b=out.begin();
  for(;a!=root.end();++a,++b) if(b==out.end() || *a!=*b) return false;
  return true;
}

bool EditorFileSystem::movePath(const std::string &relative,const std::string &destination) {
  fs::path from,to;
  if(!resolve(relative,from)) {error_="Origem invalida ou fora do projeto";return false;}
  if(!resolveTarget(root_,destination,to)) {error_="Destino invalido ou fora do projeto";return false;}
  if(from==root_) {error_="A raiz do projeto nao pode ser movida";return false;}
  std::error_code error;
  if(fs::exists(to,error)) {error_="Ja existe um item com esse nome no destino";return false;}
  // Uma pasta movida para dentro dela mesma some junto com o que carrega. O
  // `std::filesystem` nem sempre recusa; recusar aqui e barato.
  auto a=from.begin(),b=to.begin();
  bool inside=true;
  for(;a!=from.end();++a,++b) if(b==to.end() || *a!=*b) {inside=false;break;}
  if(inside && fs::is_directory(from,error)) {error_="Uma pasta nao pode ser movida para dentro dela mesma";return false;}
  if(!fs::exists(to.parent_path(),error) || error) {error_="A pasta de destino nao existe";return false;}
  fs::rename(from,to,error);
  if(error) {error_="Nao foi possivel mover";return false;}
  error_.clear();
  return rebuildTree();
}

bool EditorFileSystem::removePath(const std::string &relative) {
  fs::path path;
  if(!resolve(relative,path)) {error_="Caminho invalido ou fora do projeto";return false;}
  if(path==root_) {error_="A raiz do projeto nao pode ser apagada";return false;}
  // `.astra` e o estado do projeto -- historico, registro, cache. Apagar por
  // engano custaria o projeto inteiro, e ele nao aparece como recurso.
  const auto relativeToRoot=path.lexically_relative(root_);
  if(!relativeToRoot.empty() && relativeToRoot.begin()->string()==".astra") {
    error_="A pasta .astra pertence ao projeto e nao pode ser apagada aqui";return false;
  }
  std::error_code error;
  fs::remove_all(path,error);
  if(error) {error_="Nao foi possivel apagar";return false;}
  error_.clear();
  return rebuildTree();
}

bool EditorFileSystem::open(const std::string &relative) {
  fs::path path;
  std::error_code error;
  if(!resolve(relative,path) || !fs::is_directory(path,error) || error) {
    error_="Pasta inválida ou fora do projeto";return false;
  }
  std::vector<EditorFileEntry> prepared;
  fs::directory_iterator iterator(path,error),end;
  if(error) {error_="Não foi possível ler a pasta";return false;}
  while(iterator!=end) {
    const auto entry=*iterator;
    const auto status=entry.symlink_status(error);
    if(error) {error_="Não foi possível ler um arquivo";return false;}
    if(!fs::is_symlink(status) && (fs::is_directory(status) || fs::is_regular_file(status))) {
      if(prepared.size()>=MaximumEntries) {error_="Pasta excede o limite de 4096 entradas";return false;}
      prepared.push_back({utf8(entry.path().filename()),utf8(entry.path().lexically_relative(root_)),fs::is_directory(status)});
    }
    iterator.increment(error);
    if(error) {error_="Leitura da pasta interrompida";return false;}
  }
  std::sort(prepared.begin(),prepared.end(),[](const auto &a,const auto &b) {
    return a.directory!=b.directory?a.directory>b.directory:a.name<b.name;
  });
  auto next=utf8(path.lexically_relative(root_));
  if(next==".") next.clear();
  current_=std::move(next);entries_=std::move(prepared);error_.clear();return true;
}
bool EditorFileSystem::rebuildTree() {
  if(root_.empty()) return false;
  std::vector<std::string> expanded;
  for(const auto &entry:tree_)
    if(entry.directory && entry.expanded && !entry.relativePath.empty())
      expanded.push_back(entry.relativePath);
  if(!open("")) return false;
  std::vector<EditorFileEntry> rebuilt{{"Projeto", "", true, 0, true}};
  for(auto entry:entries_) {entry.depth=1;rebuilt.push_back(std::move(entry));}
  tree_=std::move(rebuilt);
  // Reabre o que estava aberto. A varredura recomeca do zero a cada expansao
  // porque expandir insere linhas no meio: guardar indices daria o indice de
  // antes da insercao. Uma pasta que sumiu simplesmente nao e reencontrada.
  for(const auto &path:expanded) {
    for(unsigned index=0;index<tree_.size();++index) {
      if(tree_[index].relativePath!=path || !tree_[index].directory || tree_[index].expanded) continue;
      toggle(index);
      break;
    }
  }
  error_.clear();
  return true;
}

bool EditorFileSystem::toggle(unsigned index) {
  if(index>=tree_.size() || !tree_[index].directory) return false;
  const auto entry=tree_[index];
  if(entry.expanded) {
    auto end=index+1;
    while(end<tree_.size() && tree_[end].depth>entry.depth) ++end;
    tree_.erase(tree_.begin()+index+1,tree_.begin()+end);
    tree_[index].expanded=false;
    return true;
  }
  // Preparar antes de publicar: uma falha de IO preserva a árvore visível.
  EditorFileSystem reader;reader.root_=root_;
  if(!reader.open(entry.relativePath)) {error_=reader.error_;return false;}
  if(tree_.size()+reader.entries_.size()>MaximumEntries) {
    error_="Árvore excede o limite de 4096 entradas";return false;
  }
  for(auto &child:reader.entries_) child.depth=entry.depth+1;
  tree_.insert(tree_.begin()+index+1,reader.entries_.begin(),reader.entries_.end());
  tree_[index].expanded=true;error_.clear();return true;
}
bool EditorFileSystem::up() {return open(utf8(fromUtf8(current_).parent_path()));}
std::string EditorFileSystem::resolveFile(const std::string &relative) const {
  fs::path path;std::error_code error;
  if(!resolve(relative,path) || !fs::is_regular_file(path,error) || error) return {};
  return utf8(path);
}
std::string EditorFileSystem::rootPath() const {return utf8(root_);}
bool EditorFileSystem::createDirectory(const std::string &relative) {
  const auto input=fromUtf8(relative);
  if(input.empty() || input.is_absolute() || input.has_root_name() || relative.find('\0')!=std::string::npos) {
    error_="Nome de pasta inválido";return false;
  }
  fs::path current=root_;
  if(current.empty()) {error_="Projeto sem pasta";return false;}
  for(const auto &part:input) {
    if(part==".." || part==".") {error_="Caminho relativo inválido";return false;}
    current/=part;std::error_code error;
    auto status=fs::symlink_status(current,error);
    if(error && error!=std::errc::no_such_file_or_directory) {error_="Pasta indisponível";return false;}
    if(fs::exists(status)) {
      if(fs::is_symlink(status) || !fs::is_directory(status)) {error_="O caminho já existe e não é uma pasta local";return false;}
    } else {
      error.clear();if(!fs::create_directory(current,error) || error) {error_="Não foi possível criar a pasta";return false;}
    }
  }
  error_.clear();return true;
}
bool EditorFileSystem::exists(const std::string &relative) const {
  std::filesystem::path path;
  std::error_code code;
  return resolve(relative,path) && std::filesystem::exists(path,code);
}
bool EditorFileSystem::createTextFile(const std::string &relative,std::string_view text) {
  const auto input=fromUtf8(relative);fs::path parent;
  if(input.empty() || input.filename()=="." || input.filename()==".." ||
     !resolve(utf8(input.parent_path()),parent) || text.find('\0')!=std::string_view::npos) {
    error_="Caminho do arquivo inválido";return false;
  }
  if(input.is_absolute() || input.has_root_name() || relative.find('\0')!=std::string::npos) {
    error_="Arquivo fora do projeto";return false;
  }
  const auto destination=parent/input.filename();
#ifdef _WIN32
  FILE *file=_wfopen(destination.c_str(),L"wbx");
#else
  FILE *file=std::fopen(destination.c_str(),"wbx");
#endif
  if(!file) {error_="Arquivo já existe ou não pode ser criado";return false;}
  const bool written=std::fwrite(text.data(),1,text.size(),file)==text.size();
  const bool closed=std::fclose(file)==0;
  if(!written || !closed) {
    std::error_code ignored;fs::remove(destination,ignored);
    error_="Gravação do novo arquivo falhou";return false;
  }
  error_.clear();return true;
}

}
