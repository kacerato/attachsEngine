#pragma once
#include "platform/atomic_asset_file.h"
#include "core/sha256.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <span>
#include <vector>
#include <chrono>

namespace ae::editor {
// Single editor writer. A prepared journal restores every file after an
// interrupted publication; a committed journal only needs housekeeping.
// GPU recovery is owned by EditorSession, not by the filesystem transaction.
//
// ASTRA_IMPORT_2 acrescenta UM arquivo derivado ao par fonte+registro: o mapa
// de nós da fonte (M08.2). Cena nova com registro antigo, ou mapa novo com
// fonte antiga, seria exatamente o estado parcial que o journal existe para
// impedir. Journals ASTRA_IMPORT_1 continuam recuperáveis.
//
// ASTRA_IMPORT_3 acrescenta os arquivos COMPANHEIROS de uma fonte em pasta
// (S0: `.bin` e imagens do glTF). Eles chegam já copiados numa pasta de preparo
// do projeto; publicar é MOVER (renomear no mesmo volume), nunca copiar 2 GB de
// novo: o que existia no destino vai para o backup da transação e volta se a
// publicação não terminar.
class EditorImportTransaction {
public:
  struct Companion {
    std::string staged;   // relativo ao projeto, sob `.astra/import-staging/`
    std::string relative; // destino relativo ao projeto, fora de `.astra/`
  };
  static std::filesystem::path fromUtf8(std::string_view text) {return std::filesystem::path(std::u8string(text.begin(),text.end()));}
  static bool safePath(const std::filesystem::path &root,const std::string &relative,std::filesystem::path &out) {
    const auto part=fromUtf8(relative);
    if(root.empty() || !root.is_absolute() || relative.empty() || relative.find('\0')!=std::string::npos || part.is_absolute() || part.has_root_name()) return false;
    out=root;
    std::error_code error;
    for(const auto &item:part) {
      if(item==".." || item==".") return false;
      out/=item;
      const auto status=std::filesystem::symlink_status(out,error);
      if(!error && std::filesystem::is_symlink(status)) return false;
      if(error && error!=std::errc::no_such_file_or_directory) return false;
      error.clear();
    }
    return true;
  }
  static bool read(const std::filesystem::path &path,std::vector<u8> &bytes,usize maximum=256u*1024u*1024u) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input) return false;
    const auto length=input.tellg();
    if(length<0 || static_cast<u64>(length)>maximum) return false;
    bytes.resize(static_cast<usize>(length));input.seekg(0);
    return bytes.empty() || static_cast<bool>(input.read(reinterpret_cast<char *>(bytes.data()),bytes.size()));
  }
  static bool write(const std::filesystem::path &path,std::span<const u8> bytes) {
    struct Input {std::span<const u8> bytes;usize offset=0;} input{bytes};
    return platform::replaceAssetFile(path.string().c_str(),bytes.size(),[](void *context,void *data,size_t capacity) {
      auto &source=*static_cast<Input *>(context);
      const auto count=std::min(capacity,source.bytes.size()-source.offset);
      if(count) std::memcpy(data,source.bytes.data()+source.offset,count);
      source.offset+=count;return static_cast<int>(count);
    },&input);
  }
  static bool writeText(const std::filesystem::path &path,const std::string &text) {
    return write(path,{reinterpret_cast<const u8 *>(text.data()),text.size()});
  }
  struct TextEdit {std::string path,expectedHash,text;};
  // Publicação de recursos autorais pequenos (materiais/perfis). O journal
  // existente continua sendo o único dono do rollback e da recuperação.
  static bool publishTextBatch(const std::string &project,const std::vector<TextEdit> &edits,
                               const std::string &registry,std::string &diagnostic) {
    if(edits.empty()) return true;
    const auto root=fromUtf8(project);
    for(usize i=0;i<edits.size();++i) {
      std::filesystem::path path;std::vector<u8> bytes;
      if(!safePath(root,edits[i].path,path)||!read(path,bytes)||Sha256::hex(bytes)!=edits[i].expectedHash) {
        diagnostic="Recurso mudou no disco: "+edits[i].path;return false;
      }
      for(usize k=0;k<i;++k) if(edits[k].path==edits[i].path) {diagnostic="Recurso duplicado no lote.";return false;}
    }
    std::vector<Companion> companions;std::filesystem::path staging;std::error_code error;
    if(edits.size()>1) {
      const std::string relative=".astra/import-staging/resource-edit-"+
          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
      if(!safePath(root,relative,staging)) {diagnostic="Preparo fora do projeto.";return false;}
      std::filesystem::create_directories(staging.parent_path(),error);
      if(error||!std::filesystem::create_directory(staging,error)) {diagnostic="Não foi possível preparar os recursos.";return false;}
      for(usize i=1;i<edits.size();++i) {
        const std::string staged=relative+"/"+std::to_string(i)+".resource";
        if(!writeText(root/fromUtf8(staged),edits[i].text)) {
          diagnostic="Falha ao preparar os recursos; nada publicado.";std::filesystem::remove_all(staging,error);return false;
        }
        companions.push_back({staged,edits[i].path});
      }
    }
    EditorImportTransaction transaction(project);
    if(!transaction.begin(edits.front().path,edits.front().expectedHash,diagnostic,{},companions)) {
      if(!staging.empty() && !std::filesystem::exists(root/".astra/import-transaction/journal",error) && !error)
        std::filesystem::remove_all(staging,error);
      return false;
    }
    const auto &text=edits.front().text;
    if(!transaction.commit({reinterpret_cast<const u8*>(text.data()),text.size()},registry)) {
      const bool restored=transaction.rollback();
      diagnostic=restored?"Gravação recusada; recursos e registro restaurados.":"Recuperação pendente; backups preservados no journal.";
      if(restored && !staging.empty()) std::filesystem::remove_all(staging,error);
      return false;
    }
    if(!staging.empty()) std::filesystem::remove_all(staging,error);
    return true;
  }
  static bool recover(const std::string &root,std::string &diagnostic) {
    EditorImportTransaction transaction(root);
    if(!transaction.paths()) {diagnostic="Pasta de transação inválida.";return false;}
    std::error_code error;
    if(!std::filesystem::exists(transaction.journal_,error)) return !error;
    std::vector<u8> bytes;
    if(!read(transaction.journal_,bytes,1u<<20)) {diagnostic="Não foi possível ler a transação de importação.";return false;}
    std::istringstream stream(std::string(bytes.begin(),bytes.end()));
    std::string magic,state,relative;
    bool parsed=static_cast<bool>(stream>>magic>>state>>std::quoted(relative)>>transaction.hadSource_>>transaction.hadRegistry_);
    if(parsed && (magic=="ASTRA_IMPORT_2" || magic=="ASTRA_IMPORT_3")) {
      std::string extra;
      parsed=static_cast<bool>(stream>>std::quoted(extra)>>transaction.hadExtra_);
      if(parsed && extra!="-") {
        if(!extra.starts_with(".astra/imports/") || !safePath(transaction.root_,extra,transaction.extra_)) parsed=false;
        transaction.extraRelative_=extra;
      }
      usize count=0;
      if(parsed && magic=="ASTRA_IMPORT_3") parsed=static_cast<bool>(stream>>count) && count<=MaximumCompanions;
      for(usize i=0;parsed && i<count;++i) {
        Companion companion;bool had=false;
        parsed=static_cast<bool>(stream>>std::quoted(companion.staged)>>std::quoted(companion.relative)>>had) &&
               transaction.validCompanion(companion);
        transaction.companions_.push_back(std::move(companion));transaction.hadCompanion_.push_back(had);
      }
    } else if(magic!="ASTRA_IMPORT_1") parsed=false;
    if(!parsed || (state!="prepared" && state!="committed") ||
       !safePath(transaction.root_,relative,transaction.source_) || relative.starts_with(".astra/")) {
      diagnostic="Transação de importação inválida; backups preservados.";return false;
    }
    if(state=="prepared" && !transaction.restore()) {
      diagnostic="Falha ao recuperar importação; backups preservados em .astra/import-transaction.";return false;
    }
    transaction.cleanup();
    diagnostic=state=="prepared"?"Importação interrompida recuperada; fonte e registro anteriores restaurados.":"";
    return true;
  }
  // Preparo de fonte em pasta que sobrou de um processo encerrado no meio da
  // importação. Só depois de `recover`: com journal pendente, o preparo ainda
  // pode ser o que a recuperação precisa e fica onde está.
  static void discardStaging(const std::string &root) {
    std::filesystem::path staging,journal;std::error_code error;
    if(!safePath(fromUtf8(root),".astra/import-staging",staging) ||
       !safePath(fromUtf8(root),".astra/import-transaction/journal",journal)) return;
    if(std::filesystem::exists(journal,error) || error) return;
    std::filesystem::remove_all(staging,error);
  }
  explicit EditorImportTransaction(std::string root):root_(fromUtf8(root)) {}
  // `extra` é o caminho relativo do mapa de nós, sob `.astra/imports/`; vazio
  // quando a publicação não grava mapa.
  static constexpr usize MaximumCompanions=4096;
  bool begin(const std::string &relative,const std::string &expectedHash,std::string &diagnostic,const std::string &extra={},
             std::span<const Companion> companions={}) {
    if(!paths() || relative.starts_with(".astra/") || !safePath(root_,relative,source_)) {
      diagnostic="Destino de importação inválido.";return false;
    }
    if(!extra.empty() && (!extra.starts_with(".astra/imports/") || !safePath(root_,extra,extra_))) {
      diagnostic="Destino do mapa de nós inválido.";return false;
    }
    std::error_code error;
    if(std::filesystem::exists(journal_,error) || error) {diagnostic="Existe uma importação pendente de recuperação.";return false;}
    hadSource_=std::filesystem::exists(source_,error);if(error) return false;
    std::vector<u8> current;
    if(hadSource_ && !read(source_,current)) {diagnostic="Fonte anterior indisponível.";return false;}
    if((hadSource_?Sha256::hex(current):std::string())!=expectedHash) {
      diagnostic="A fonte mudou desde a preparação. Reabra a importação.";return false;
    }
    hadRegistry_=std::filesystem::exists(registry_,error);if(error) return false;
    std::filesystem::create_directories(directory_,error);if(error) return false;
    std::filesystem::create_directories(source_.parent_path(),error);if(error) return false;
    if(hadSource_ && !write(directory_/"source.backup",current)) return false;
    if(hadRegistry_) {
      if(!read(registry_,current,32u*1024u*1024u) || !write(directory_/"registry.backup",current)) return false;
    }
    extraRelative_=extra;hadExtra_=false;
    if(!extra.empty()) {
      hadExtra_=std::filesystem::exists(extra_,error);if(error) return false;
      std::filesystem::create_directories(extra_.parent_path(),error);if(error) return false;
      if(hadExtra_ && (!read(extra_,current,64u*1024u*1024u) || !write(directory_/"extra.backup",current))) return false;
    }
    companions_.clear();hadCompanion_.clear();
    if(companions.size()>MaximumCompanions) {diagnostic="Arquivos demais na pasta do modelo.";return false;}
    for(const auto &companion:companions) {
      std::filesystem::path staged,target;
      if(!validCompanion(companion) || !safePath(root_,companion.staged,staged) || !safePath(root_,companion.relative,target) ||
         !std::filesystem::is_regular_file(staged,error)) {
        diagnostic="Arquivo da pasta do modelo fora do preparo ou do projeto: "+companion.relative;return false;
      }
      companions_.push_back(companion);
      hadCompanion_.push_back(std::filesystem::exists(target,error));if(error) return false;
    }
    if(!companions_.empty()) {
      std::filesystem::create_directories(directory_/"companions",error);if(error) return false;
    }
    relative_=relative;
    if(!mark("prepared")) {diagnostic="Não foi possível preparar o journal de importação.";return false;}
    active_=true;return true;
  }
  bool commit(std::span<const u8> bytes,const std::string &registry,const std::string &extra={}) {
    if(!active_) return false;
    // Companheiros primeiro: o principal só aponta para arquivos que já estão lá.
    for(usize i=0;i<companions_.size();++i) {
      std::filesystem::path staged,target;std::error_code error;
      if(!safePath(root_,companions_[i].staged,staged) || !safePath(root_,companions_[i].relative,target)) return false;
      std::filesystem::create_directories(target.parent_path(),error);if(error) return false;
      if(hadCompanion_[i]) {std::filesystem::rename(target,companionBackup(i),error);if(error) return false;}
      std::filesystem::rename(staged,target,error);if(error) return false;
    }
    if(!write(source_,bytes) || !writeText(registry_,registry)) return false;
    if(!extraRelative_.empty() && !writeText(extra_,extra)) return false;
    if(!mark("committed")) return false;
    active_=false;cleanup();return true;
  }
  bool rollback() {
    if(!active_) return true;
    if(!restore()) return false;
    active_=false;cleanup();return true;
  }
private:
  bool paths() {
    if(!safePath(root_,".astra/import-transaction",directory_) || !safePath(root_,".astra/assets.astra",registry_)) return false;
    std::filesystem::path unused;
    return safePath(root_,".astra/import-transaction/journal",journal_) &&
        safePath(root_,".astra/import-transaction/source.backup",unused) &&
        safePath(root_,".astra/import-transaction/registry.backup",unused) &&
        safePath(root_,".astra/import-transaction/extra.backup",unused);
  }
  bool mark(const char *state) {
    std::ostringstream out;
    out<<(companions_.empty()?"ASTRA_IMPORT_2 ":"ASTRA_IMPORT_3 ")<<state<<' '<<std::quoted(relative_)<<' '<<hadSource_<<' '<<hadRegistry_<<' '
       <<std::quoted(extraRelative_.empty()?std::string("-"):extraRelative_)<<' '<<hadExtra_;
    if(!companions_.empty()) {
      out<<' '<<companions_.size();
      for(usize i=0;i<companions_.size();++i)
        out<<' '<<std::quoted(companions_[i].staged)<<' '<<std::quoted(companions_[i].relative)<<' '<<hadCompanion_[i];
    }
    out<<'\n';
    return writeText(journal_,out.str());
  }
  bool validCompanion(const Companion &companion) const {
    return companion.staged.starts_with(".astra/import-staging/") && !companion.relative.empty() &&
           !companion.relative.starts_with(".astra/");
  }
  std::filesystem::path companionBackup(usize index) const {return directory_/"companions"/std::to_string(index);}
  bool restore() {
    const auto restoreOne=[&](const std::filesystem::path &target,const char *backup,bool existed) {
      if(existed) {std::vector<u8> bytes;return read(directory_/backup,bytes,256u*1024u*1024u) && write(target,bytes);}
      std::error_code error;std::filesystem::remove(target,error);return !error;
    };
    // Companheiro: o que foi publicado sai; o que existia volta do backup. O
    // arquivo novo não volta para o preparo — quem prepara importa de novo.
    bool companions=true;
    for(usize i=0;i<companions_.size();++i) {
      std::filesystem::path target;std::error_code error;
      if(!safePath(root_,companions_[i].relative,target)) {companions=false;continue;}
      const auto backup=companionBackup(i);
      if(std::filesystem::exists(backup,error)) {
        std::filesystem::remove(target,error);error.clear();
        std::filesystem::rename(backup,target,error);companions=companions && !error;
      } else if(!hadCompanion_[i]) {
        std::filesystem::remove(target,error);companions=companions && !error;
      }
    }
    // Evaluate every restoration: failure of one must not skip the others.
    const bool source=restoreOne(source_,"source.backup",hadSource_);
    const bool registry=restoreOne(registry_,"registry.backup",hadRegistry_);
    const bool extra=extraRelative_.empty() || extraRelative_=="-" || restoreOne(extra_,"extra.backup",hadExtra_);
    return source && registry && extra && companions;
  }
  void cleanup() {
    // Fixed internal leaf paths only; no recursive deletion.
    std::error_code error;std::filesystem::remove(journal_,error);
    if(error) return; // retain backups whenever a journal remains
    std::filesystem::remove(directory_/"source.backup",error);
    std::filesystem::remove(directory_/"registry.backup",error);
    std::filesystem::remove(directory_/"extra.backup",error);
    for(usize i=0;i<companions_.size();++i) std::filesystem::remove(companionBackup(i),error);
    std::filesystem::remove(directory_/"companions",error); // só se vazia
  }
  std::filesystem::path root_,directory_,journal_,source_,registry_,extra_;
  std::string relative_,extraRelative_;
  std::vector<Companion> companions_;
  std::vector<bool> hadCompanion_;
  bool hadSource_=false,hadRegistry_=false,hadExtra_=false,active_=false;
};
}
