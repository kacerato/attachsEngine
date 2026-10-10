#pragma once
#include "core/base.h"
#include <optional>
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
  enum class MetadataKind : u8 {LibraryIndex=1,SourceProfile=2,NodeMap=3};
  struct Metadata {MetadataKind kind;std::string path,expectedHash;};
  static constexpr usize MaximumMetadata=8;
private:
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
  static bool write(const std::filesystem::path &path,std::span<const u8> bytes,std::string *diagnostic=nullptr) {
    struct Input {std::span<const u8> bytes;usize offset=0;} input{bytes};
    const auto utf8=path.u8string();
    platform::AssetFileDiagnostic result;
    const bool ok=platform::replaceAssetFile(reinterpret_cast<const char*>(utf8.c_str()),bytes.size(),[](void *context,void *data,size_t capacity) {
      auto &source=*static_cast<Input *>(context);
      const auto count=std::min(capacity,source.bytes.size()-source.offset);
      if(count) std::memcpy(data,source.bytes.data()+source.offset,count);
      source.offset+=count;return static_cast<int>(count);
    },&input,&result);
    if(!ok&&diagnostic) {
      const char *stages[]{"nenhuma","argumento","caminho","abrir","ler","gravar","flush","sincronizar","fechar","publicar"};
      *diagnostic="Falha ao "+std::string(stages[static_cast<unsigned>(result.stage)])+" "+std::string(reinterpret_cast<const char*>(utf8.c_str()))+" · código "+std::to_string(result.code);
    }
    return ok;
  }
  static bool writeText(const std::filesystem::path &path,const std::string &text,std::string *diagnostic=nullptr) {
    return write(path,{reinterpret_cast<const u8 *>(text.data()),text.size()},diagnostic);
  }
  struct TextEdit {std::string path,expectedHash,text;};
  // Publicação de recursos autorais pequenos (materiais/perfis). O journal
  // existente continua sendo o único dono do rollback e da recuperação.
  static bool publishTextBatch(const std::string &project,const std::vector<TextEdit> &edits,
                               const std::string &registry,std::string &diagnostic,
                               const std::string &extra={},const std::string &extraText={}) {
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
        if(!writeText(root/fromUtf8(staged),edits[i].text,&diagnostic)) {
          diagnostic+="; nada publicado.";std::filesystem::remove_all(staging,error);return false;
        }
        companions.push_back({staged,edits[i].path});
      }
    }
    EditorImportTransaction transaction(project);
    if(!transaction.begin(edits.front().path,edits.front().expectedHash,diagnostic,extra,companions)) {
      if(!staging.empty() && !std::filesystem::exists(root/".astra/import-transaction/journal",error) && !error)
        std::filesystem::remove_all(staging,error);
      return false;
    }
    const auto &text=edits.front().text;
    if(!transaction.commit({reinterpret_cast<const u8*>(text.data()),text.size()},registry,extraText,&diagnostic)) {
      const auto reason=diagnostic;
      const bool restored=transaction.rollback();
      diagnostic=reason+(restored?"; recursos e registro restaurados.":"; recuperação pendente, backups preservados no journal.");
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
    if(parsed && (magic=="ASTRA_IMPORT_2" || magic=="ASTRA_IMPORT_3" || magic=="ASTRA_IMPORT_4")) {
      std::string extra;
      parsed=static_cast<bool>(stream>>std::quoted(extra)>>transaction.hadExtra_);
      if(parsed && extra!="-") {
        if(!extra.starts_with(".astra/imports/") || !safePath(transaction.root_,extra,transaction.extra_)) parsed=false;
        transaction.extraRelative_=extra;
      }
      usize count=0;
      if(parsed && (magic=="ASTRA_IMPORT_3"||magic=="ASTRA_IMPORT_4")) parsed=static_cast<bool>(stream>>count) && count<=MaximumCompanions;
      for(usize i=0;parsed && i<count;++i) {
        Companion companion;bool had=false;
        parsed=static_cast<bool>(stream>>std::quoted(companion.staged)>>std::quoted(companion.relative)>>had) &&
               transaction.validCompanion(companion);
        transaction.companions_.push_back(std::move(companion));transaction.hadCompanion_.push_back(had);
      }
      if(parsed&&magic=="ASTRA_IMPORT_4") {
        usize entries=0;parsed=static_cast<bool>(stream>>entries)&&entries>0&&entries<=MaximumMetadata;
        for(usize i=0;parsed&&i<entries;++i) {
          u32 kind=0;Metadata metadata{};bool had=false;
          parsed=static_cast<bool>(stream>>kind>>std::quoted(metadata.path)>>had)&&kind>=1&&kind<=3;
          metadata.kind=static_cast<MetadataKind>(kind);
          std::filesystem::path target;
          parsed=parsed&&transaction.validMetadata(metadata)&&safePath(transaction.root_,metadata.path,target)&&metadata.path!=transaction.extraRelative_;
          for(const auto &previous:transaction.metadata_)if(previous.path==metadata.path)parsed=false;
          transaction.metadata_.push_back(std::move(metadata));transaction.hadMetadata_.push_back(had);
        }
      }
    } else if(magic!="ASTRA_IMPORT_1") parsed=false;
    stream>>std::ws;if(!stream.eof())parsed=false;
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
             std::span<const Companion> companions={},std::span<const Metadata> metadata={}) {
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
    sourceExpectedHash_=expectedHash;registryExpectedHash_.clear();extraExpectedHash_.clear();
    hadRegistry_=std::filesystem::exists(registry_,error);if(error) return false;
    std::filesystem::create_directories(directory_,error);if(error) return false;
    std::filesystem::create_directories(source_.parent_path(),error);if(error) return false;
    if(hadSource_ && !write(directory_/"source.backup",current,&diagnostic)) return false;
    if(hadRegistry_) {
      if(!read(registry_,current,32u*1024u*1024u)) {diagnostic="Registro anterior indisponível.";return false;}
      registryExpectedHash_=Sha256::hex(current);
      if(!write(directory_/"registry.backup",current,&diagnostic)) return false;
    }
    extraRelative_=extra;hadExtra_=false;
    if(!extra.empty()) {
      hadExtra_=std::filesystem::exists(extra_,error);if(error) return false;
      std::filesystem::create_directories(extra_.parent_path(),error);if(error) return false;
      if(hadExtra_) {
        if(!read(extra_,current,64u*1024u*1024u)) {diagnostic="Mapa anterior indisponível.";return false;}
        extraExpectedHash_=Sha256::hex(current);
        if(!write(directory_/"extra.backup",current,&diagnostic))return false;
      }
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
    metadata_.clear();hadMetadata_.clear();
    if(metadata.size()>MaximumMetadata) {diagnostic="Metadados demais na transacao.";return false;}
    if(!metadata.empty()) {std::filesystem::create_directories(directory_/"metadata",error);if(error)return false;}
    for(const auto &entry:metadata) {
      std::filesystem::path target;
      if(!validMetadata(entry)||entry.path==extraRelative_||!safePath(root_,entry.path,target)) {diagnostic="Destino tipado de metadados invalido.";return false;}
      for(const auto &previous:metadata_)if(previous.path==entry.path) {diagnostic="Metadado duplicado.";return false;}
      const bool existed=std::filesystem::exists(target,error);if(error)return false;
      current.clear();if(existed&&!read(target,current,metadataLimit(entry.kind))) {diagnostic="Metadado anterior indisponivel.";return false;}
      if((existed?Sha256::hex(current):std::string())!=entry.expectedHash) {diagnostic="Metadado mudou desde o preparo: "+entry.path;return false;}
      std::filesystem::create_directories(target.parent_path(),error);if(error)return false;
      if(existed&&!write(metadataBackup(metadata_.size()),current,&diagnostic))return false;
      metadata_.push_back(entry);hadMetadata_.push_back(existed);
    }
    relative_=relative;
    if(!mark("prepared",&diagnostic)) return false;
    active_=true;return true;
  }
  bool commit(std::span<const u8> bytes,const std::string &registry,const std::string &extra={},std::string *diagnostic=nullptr,
              std::span<const std::optional<std::string>> metadataText={}) {
    if(!active_)return false;
    // No commit write has happened yet. A conflicting external edit must survive
    // both this refusal and a caller's customary rollback().
    if(!unchanged(source_,sourceExpectedHash_,1ull<<30)||!unchanged(registry_,registryExpectedHash_,32u<<20)||
       (!extraRelative_.empty()&&!unchanged(extra_,extraExpectedHash_,64u<<20))) {
      if(diagnostic)*diagnostic="Fonte ou registro mudou durante o preparo; publicação recusada.";
      active_=false;cleanup();return false;
    }
    for(const auto &metadata:metadata_) {
      std::filesystem::path target;std::vector<u8> current;std::error_code error;
      bool valid=safePath(root_,metadata.path,target);
      const bool exists=valid&&std::filesystem::exists(target,error);
      valid=valid&&!error&&(!exists||read(target,current,metadataLimit(metadata.kind)))&&
          (exists?Sha256::hex(current):std::string())==metadata.expectedHash;
      if(!valid) {
        if(diagnostic)*diagnostic="Metadado mudou durante o preparo: "+metadata.path;
        active_=false;cleanup();return false;
      }
    }
    if(metadataText.size()!=metadata_.size()) {active_=false;cleanup();return false;}
    for(usize i=0;i<metadataText.size();++i)if(metadataText[i]&&metadataText[i]->size()>metadataLimit(metadata_[i].kind)) {active_=false;cleanup();return false;}
    // Companheiros primeiro: o principal só aponta para arquivos que já estão lá.
    for(usize i=0;i<companions_.size();++i) {
      std::filesystem::path staged,target;std::error_code error;
      if(!safePath(root_,companions_[i].staged,staged) || !safePath(root_,companions_[i].relative,target)) return false;
      std::filesystem::create_directories(target.parent_path(),error);if(error) return false;
      if(hadCompanion_[i]) {std::filesystem::rename(target,companionBackup(i),error);if(error) return false;}
      std::filesystem::rename(staged,target,error);if(error) return false;
    }
    if(!write(source_,bytes,diagnostic) || !writeText(registry_,registry,diagnostic)) return false;
    if(!extraRelative_.empty() && !writeText(extra_,extra,diagnostic)) return false;
    for(usize i=0;i<metadata_.size();++i) {
      std::filesystem::path target;if(!safePath(root_,metadata_[i].path,target))return false;
      if(metadataText[i]) {if(!writeText(target,*metadataText[i],diagnostic))return false;}
      else {std::error_code error;std::filesystem::remove(target,error);if(error)return false;}
    }
    if(!mark("committed",diagnostic)) return false;
    active_=false;cleanup();return true;
  }
  // The same prepared journal owns resource removal. A crash before the
  // committed marker restores both the original file and its registry.
  bool commitRemoval(const std::string &registry,const std::string &extra={}) {
    if(!active_||!hadSource_||!companions_.empty()||!metadata_.empty())return false;
    if(!unchanged(source_,sourceExpectedHash_,1ull<<30)||!unchanged(registry_,registryExpectedHash_,32u<<20)||
       (!extraRelative_.empty()&&!unchanged(extra_,extraExpectedHash_,64u<<20))) {active_=false;cleanup();return false;}
    std::error_code error;if(!std::filesystem::remove(source_,error)||error)return false;
    if(!writeText(registry_,registry)||(!extraRelative_.empty()&&!writeText(extra_,extra))||!mark("committed"))return false;
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
  bool mark(const char *state,std::string *diagnostic=nullptr) {
    std::ostringstream out;
    out<<(!metadata_.empty()?"ASTRA_IMPORT_4 ":companions_.empty()?"ASTRA_IMPORT_2 ":"ASTRA_IMPORT_3 ")<<state<<' '<<std::quoted(relative_)<<' '<<hadSource_<<' '<<hadRegistry_<<' '
       <<std::quoted(extraRelative_.empty()?std::string("-"):extraRelative_)<<' '<<hadExtra_;
    if(!companions_.empty()||!metadata_.empty()) {
      out<<' '<<companions_.size();
      for(usize i=0;i<companions_.size();++i)
        out<<' '<<std::quoted(companions_[i].staged)<<' '<<std::quoted(companions_[i].relative)<<' '<<hadCompanion_[i];
    }
    if(!metadata_.empty()) {
      out<<' '<<metadata_.size();
      for(usize i=0;i<metadata_.size();++i)out<<' '<<static_cast<u32>(metadata_[i].kind)<<' '<<std::quoted(metadata_[i].path)<<' '<<hadMetadata_[i];
    }
    out<<'\n';
    return writeText(journal_,out.str(),diagnostic);
  }
  static usize metadataLimit(MetadataKind kind) {return kind==MetadataKind::LibraryIndex?16u<<20:kind==MetadataKind::NodeMap?64u<<20:64u<<10;}
  bool validMetadata(const Metadata &entry) const {
    if(entry.kind==MetadataKind::LibraryIndex)return entry.path==".astra/imports/library-index.astra";
    constexpr std::string_view prefix=".astra/imports/";
    const std::string_view suffix=entry.kind==MetadataKind::SourceProfile?".profile":".nodes";
    if((entry.kind!=MetadataKind::SourceProfile&&entry.kind!=MetadataKind::NodeMap)||!entry.path.starts_with(prefix)||!entry.path.ends_with(suffix)||entry.path.size()!=prefix.size()+32+suffix.size())return false;
    const auto id=std::string_view(entry.path).substr(prefix.size(),32);bool nonzero=false;
    for(const char c:id) {if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;nonzero=nonzero||c!='0';}
    return nonzero;
  }
  std::filesystem::path metadataBackup(usize index) const {return directory_/"metadata"/std::to_string(index);}
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
    bool metadata=true;
    for(usize i=0;i<metadata_.size();++i) {
      std::filesystem::path target;std::vector<u8> bytes;std::error_code error;
      bool restored=safePath(root_,metadata_[i].path,target);
      if(restored&&hadMetadata_[i])restored=read(metadataBackup(i),bytes,metadataLimit(metadata_[i].kind))&&write(target,bytes);
      else if(restored) {std::filesystem::remove(target,error);restored=!error;}
      metadata=restored&&metadata;
    }
    return source && registry && extra && companions && metadata;
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
    for(usize i=0;i<metadata_.size();++i)std::filesystem::remove(metadataBackup(i),error);
    std::filesystem::remove(directory_/"metadata",error);

  }
  static bool unchanged(const std::filesystem::path &path,const std::string &hash,usize maximum) {
    std::error_code error;std::vector<u8> bytes;const bool exists=std::filesystem::exists(path,error);
    return !error&&(!exists||read(path,bytes,maximum))&&(exists?Sha256::hex(bytes):std::string())==hash;
  }
  std::string sourceExpectedHash_,registryExpectedHash_,extraExpectedHash_;
  std::filesystem::path root_,directory_,journal_,source_,registry_,extra_;
  std::string relative_,extraRelative_;
  std::vector<Metadata> metadata_;
  std::vector<bool> hadMetadata_;
  std::vector<Companion> companions_;
  std::vector<bool> hadCompanion_;
  bool hadSource_=false,hadRegistry_=false,hadExtra_=false,active_=false;
};
}
