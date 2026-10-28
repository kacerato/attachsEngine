#include "editor/editor_bundled_packages.h"
#include "editor/editor_import_transaction.h"
#include "core/sha256.h"
#include <fstream>
#include <set>
#include <sstream>

namespace ae::editor {
namespace {
std::vector<std::string_view> fields(std::string_view line) {
  std::vector<std::string_view> out;
  for(usize at=0;;) {
    const auto tab=line.find('\t',at);
    out.push_back(line.substr(at,tab==line.npos?line.npos:tab-at));
    if(tab==line.npos)return out;
    at=tab+1;
  }
}
bool safeRelative(std::string_view relative) {
  if(relative.empty()||relative.size()>1024||relative.front()=='/'||relative.find_first_of("\\:\r\n\t")!=relative.npos)return false;
  for(usize at=0;;) {
    const auto slash=relative.find('/',at);const auto part=relative.substr(at,slash==relative.npos?relative.npos:slash-at);
    if(part.empty()||part=="."||part=="..")return false;
    if(slash==relative.npos)return true;
    at=slash+1;
  }
}
bool hex64(std::string_view text) {
  if(text.size()!=64)return false;
  for(char c:text)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
  return true;
}
}

bool parseBundledPackageIndex(std::string_view text,std::vector<BundledPackage> &packages,std::string &diagnostic) {
  packages.clear();diagnostic.clear();
  const auto fail=[&](std::string message){packages.clear();diagnostic=std::move(message);return false;};
  std::istringstream input{std::string(text)};std::string line;
  if(!std::getline(input,line)||line!="ASTRA_BUNDLED_PACKAGES_1")return fail("Índice de pacotes incluídos sem cabeçalho.");
  std::set<std::string> names,directories,relatives;
  std::vector<std::string> lines;
  while(std::getline(input,line)) {
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line.empty())continue;
    const auto f=fields(line);
    if(f[0]=="package"&&f.size()==3) {
      if(f[1].empty()||f[1].size()>128||!names.insert(std::string(f[1])).second)return fail("Nome de pacote incluído inválido ou repetido.");
      if(!safeRelative(f[2])||f[2].find('/')!=f[2].npos||!directories.insert(std::string(f[2])).second)return fail("Diretório de pacote incluído inválido.");
      packages.push_back({std::string(f[1]),std::string(f[2]),{},0,{}});relatives.clear();lines.emplace_back();
    } else if(f[0]=="file"&&f.size()==5&&!packages.empty()) {
      auto &package=packages.back();
      u64 bytes=0;const std::string size(f[2]);
      if(size.empty()||size.size()>12||size.find_first_not_of("0123456789")!=size.npos)return fail("Tamanho inválido no índice de pacotes incluídos.");
      bytes=std::stoull(size);
      if(!safeRelative(f[1])||!hex64(f[3])||!safeRelative(f[4])||f[4].starts_with(".astra/"))return fail("Arquivo inválido no índice de pacotes incluídos.");
      if(!relatives.insert(std::string(f[4])).second)return fail("Arquivo repetido no pacote incluído: "+std::string(f[4]));
      if(package.files.size()>=EditorImportTransaction::MaximumCompanions)return fail("Pacote incluído com mais de 4096 arquivos.");
      package.files.push_back({std::string(f[4]),package.directory+"/"+std::string(f[1]),std::string(f[3]),bytes});
      package.bytes+=bytes;
    } else return fail("Linha inválida no índice de pacotes incluídos.");
    lines.back()+=line;lines.back()+='\n';
  }
  for(usize i=0;i<packages.size();++i) {
    if(packages[i].files.empty())return fail("Pacote incluído sem arquivos: "+packages[i].name);
    const auto &body=lines[i];
    packages[i].indexHash=Sha256::hex({reinterpret_cast<const u8*>(body.data()),body.size()});
  }
  return true;
}

bool bundledPackageExtracted(const std::filesystem::path &directory,const BundledPackage &package) {
  std::ifstream marker(directory/BundledPackageMarker);std::string stored;
  if(!marker||!std::getline(marker,stored)||stored!=package.indexHash)return false;
  for(const auto &file:package.files) {
    std::filesystem::path path;std::error_code error;
    if(!EditorImportTransaction::safePath(directory,file.relative,path))return false;
    if(std::filesystem::file_size(path,error)!=file.bytes||error)return false;
  }
  return true;
}

bool writeBundledPackageMarker(const std::filesystem::path &directory,const BundledPackage &package) {
  std::ofstream marker(directory/BundledPackageMarker,std::ios::binary|std::ios::trunc);
  marker<<package.indexHash<<'\n';
  return static_cast<bool>(marker);
}

bool copyBundledPackageToStaging(const std::filesystem::path &source,const std::filesystem::path &staging,
    const BundledPackage &package,const std::function<bool()> &cancelled,std::atomic<u64> &done,std::string &diagnostic) {
  std::vector<char> buffer(1u<<20);
  for(const auto &file:package.files) {
    if(cancelled&&cancelled()){diagnostic="Importação do pacote cancelada.";return false;}
    std::filesystem::path from,to;std::error_code error;
    if(!EditorImportTransaction::safePath(source,file.relative,from)||!EditorImportTransaction::safePath(staging,file.relative,to))
      {diagnostic="Caminho fora do pacote: "+file.relative;return false;}
    std::filesystem::create_directories(to.parent_path(),error);
    if(error){diagnostic="Não foi possível criar o preparo do pacote.";return false;}
    std::ifstream in(from,std::ios::binary);std::ofstream out(to,std::ios::binary|std::ios::trunc);
    if(!in||!out){diagnostic="Fonte do pacote incluído ilegível: "+file.relative;return false;}
    Sha256 hash;u64 bytes=0;
    while(in) {
      in.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=in.gcount();
      if(count<=0)break;
      hash.update({reinterpret_cast<const u8*>(buffer.data()),static_cast<usize>(count)});
      out.write(buffer.data(),count);bytes+=static_cast<u64>(count);done+=static_cast<u64>(count);
    }
    out.close();
    if(!out||bytes!=file.bytes||Sha256::hex(hash.digest())!=file.sha256)
      {diagnostic="Arquivo do pacote incluído não confere: "+file.relative;return false;}
  }
  return true;
}
}
