#include "editor/editor_filesystem.h"
#include <algorithm>

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
}
