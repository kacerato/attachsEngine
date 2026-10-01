#include "resources/asset_registry.h"
#include <utility>
#include <vector>
#include "core/sha256.h"

#include <algorithm>
#include <iomanip>
#include <locale>
#include <sstream>

namespace ae::resources {
namespace {
const char *kTypeNames[]{"", "mesh", "material", "texture", "script", "input_actions", "scene", "environment_profile", "environment_map", "animation_clip", "prefab", "audio_clip"};

bool hexDigit(char c, u32 &out) {
  if (c >= '0' && c <= '9') { out = static_cast<u32>(c - '0'); return true; }
  if (c >= 'a' && c <= 'f') { out = static_cast<u32>(c - 'a') + 10; return true; }
  return false;
}

// Caminho de projeto aceitável: relativo, sem `..`, sem raiz, sem barra
// invertida. O registro não é um lugar de onde se escapa para fora do projeto.
bool acceptablePath(std::string_view path) {
  if (path.empty() || path.size() > 1024) return false;
  if (path.front() == '/' || path.front() == '.') return false;
  if (path.find('\\') != std::string_view::npos) return false;
  if (path.find(':') != std::string_view::npos) return false;
  if (path.find("//") != std::string_view::npos) return false;
  if (path.back() == '/') return false;
  usize start = 0;
  while (start <= path.size()) {
    const auto end = path.find('/', start);
    const auto segment = path.substr(start, (end == std::string_view::npos ? path.size() : end) - start);
    if (segment.empty() || segment == "." || segment == "..") return false;
    for (char c : segment) if (static_cast<unsigned char>(c) < 0x20) return false;
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return true;
}

bool acceptableHash(std::string_view hash) {
  if (hash.empty()) return true; // recurso sem fonte
  if (hash.size() != 64) return false;
  u32 digit = 0;
  for (char c : hash) if (!hexDigit(c, digit)) return false;
  return true;
}
} // namespace

std::string AssetGuid::text() const {
  static const char *digits = "0123456789abcdef";
  std::string out;
  out.reserve(32);
  for (u32 i = 0; i < 16; ++i) out.push_back(digits[(high >> (60 - i * 4)) & 15]);
  for (u32 i = 0; i < 16; ++i) out.push_back(digits[(low >> (60 - i * 4)) & 15]);
  return out;
}

bool AssetGuid::parse(std::string_view text, AssetGuid &out) {
  if (text.size() != 32) return false;
  u64 high = 0, low = 0, digit = 0;
  u32 value = 0;
  for (u32 i = 0; i < 32; ++i) {
    if (!hexDigit(text[i], value)) return false;
    digit = static_cast<u64>(value);
    if (i < 16) high = (high << 4) | digit;
    else low = (low << 4) | digit;
  }
  if (high == 0 && low == 0) return false;
  out.high = high;
  out.low = low;
  return true;
}

AssetGuid assetGuidFromSeed(std::span<const u8> seed) {
  Sha256 hash;
  hash.update(seed);
  const auto digest = hash.digest();
  AssetGuid guid{};
  for (u32 i = 0; i < 8; ++i) guid.high = (guid.high << 8) | digest[i];
  for (u32 i = 0; i < 8; ++i) guid.low = (guid.low << 8) | digest[8 + i];
  // Zero é "sem recurso" em toda a API. A chance de a SHA-256 produzir 128 bits
  // zerados é desprezível, mas "desprezível" não é "impossível" e a ambiguidade
  // seria silenciosa.
  if (!guid.valid()) guid.low = 1;
  return guid;
}

AssetGuid assetGuidFromSeed(std::string_view seed) {
  return assetGuidFromSeed(std::span<const u8>(reinterpret_cast<const u8 *>(seed.data()), seed.size()));
}

const char *assetTypeName(AssetType type) {
  const auto index = static_cast<u32>(type);
  return index >= 1 && index < std::size(kTypeNames) ? kTypeNames[index] : "";
}

bool parseAssetType(std::string_view text, AssetType &out) {
  for (u32 i = 1; i < std::size(kTypeNames); ++i)
    if (text == kTypeNames[i]) { out = static_cast<AssetType>(i); return true; }
  return false;
}

bool AssetRecord::valid() const {
  if (!guid.valid() || !assetTypeName(type)[0]) return false;
  if (!acceptablePath(path)) return false;
  if (!source.empty() && !acceptablePath(source)) return false;
  if (!acceptableHash(contentHash)) return false;
  if (importerParameters.size() > 64 * 1024) return false;
  if (dependencies.size() > 4096 || derived.size() > 4096) return false;
  for (const auto &dependency : dependencies) if (!dependency.valid() || dependency == guid) return false;
  for (const auto &file : derived) if (!acceptablePath(file)) return false;
  return true;
}

bool AssetRegistry::add(AssetRecord record) {
  if (records_.size() >= MaximumRecords || !record.valid()) return false;
  if (find(record.guid) || findByPath(record.path)) return false;
  // Uma dependência para um GUID que não está no registro é uma referência
  // pendurada com aparência de válida. Recusa na entrada.
  for (const auto &dependency : record.dependencies) if (!find(dependency)) return false;
  records_.push_back(std::move(record));
  return true;
}

const AssetRecord *AssetRegistry::find(const AssetGuid &guid) const {
  for (const auto &record : records_) if (record.guid == guid) return &record;
  return nullptr;
}

AssetRecord *AssetRegistry::edit(const AssetGuid &guid) {
  for (auto &record : records_) if (record.guid == guid) return &record;
  return nullptr;
}

const AssetRecord *AssetRegistry::findByPath(std::string_view path) const {
  for (const auto &record : records_) if (record.path == path) return &record;
  return nullptr;
}

bool AssetRegistry::setPath(const AssetGuid &guid, std::string_view path) {
  auto *record = edit(guid);
  if (!record || !acceptablePath(path)) return false;
  if (record->path == path) return true;
  if (findByPath(path)) return false;
  record->path = std::string(path);
  return true;
}

int AssetRegistry::retargetPrefix(std::string_view oldPrefix, std::string_view newPrefix) {
  if(oldPrefix.empty() || newPrefix.empty()) return -1;
  // O caminho casa quando e o proprio prefixo ou quando comeca com ele seguido
  // de barra. Sem a barra, mover "Fontes" levaria "FontesAntigas" junto.
  const auto matches=[&](std::string_view path) {
    if(path.size()<oldPrefix.size() || path.compare(0,oldPrefix.size(),oldPrefix)!=0) return false;
    return path.size()==oldPrefix.size() || path[oldPrefix.size()]=='/';
  };
  std::vector<std::pair<usize,std::string>> planned;
  for(usize index=0;index<records_.size();++index) {
    if(!matches(records_[index].path)) continue;
    planned.emplace_back(index,std::string(newPrefix)+records_[index].path.substr(oldPrefix.size()));
  }
  if(planned.empty()) return 0;
  // Confere TUDO antes de escrever qualquer coisa.
  for(const auto &[index,path]:planned) {
    for(usize other=0;other<records_.size();++other) {
      if(records_[other].path!=path) continue;
      bool moving=false;
      for(const auto &[movingIndex,ignored]:planned) if(movingIndex==other) {moving=true;break;}
      if(!moving) return -1;
    }
    for(const auto &[otherIndex,otherPath]:planned)
      if(otherIndex!=index && otherPath==path) return -1;
  }
  for(auto &[index,path]:planned) records_[index].path=std::move(path);
  // `source` e `derived` apontam para o mesmo lugar no disco. Um arquivo que
  // muda de pasta muda para todos: deixar a fonte para tras faria o projeto
  // reabrir procurando o arquivo onde ele nao esta mais.
  const auto retarget=[&](std::string &value) {
    if(!matches(value)) return;
    value=std::string(newPrefix)+value.substr(oldPrefix.size());
  };
  for(auto &record:records_) {
    retarget(record.source);
    for(auto &file:record.derived) retarget(file);
  }
  return static_cast<int>(planned.size());
}

bool AssetRegistry::publishImport(const AssetGuid &guid, std::string_view contentHash, u32 importerVersion,
                                  std::string_view importerParameters, std::vector<std::string> derived,
                                  std::vector<AssetGuid> dependencies) {
  auto *record = edit(guid);
  if (!record) return false;
  // Candidato completo antes de tocar no registro: uma reimportação que falha no
  // meio não pode deixar metade dos derivados novos com metade dos antigos.
  AssetRecord candidate = *record;
  candidate.contentHash = std::string(contentHash);
  candidate.importerVersion = importerVersion;
  candidate.importerParameters = std::string(importerParameters);
  candidate.derived = std::move(derived);
  candidate.dependencies = std::move(dependencies);
  if (!candidate.valid()) return false;
  for (const auto &dependency : candidate.dependencies) if (!find(dependency)) return false;
  *record = std::move(candidate);
  return true;
}

std::vector<AssetGuid> AssetRegistry::dependents(const AssetGuid &guid) const {
  std::vector<AssetGuid> result;
  for (const auto &record : records_)
    for (const auto &dependency : record.dependencies)
      if (dependency == guid) { result.push_back(record.guid); break; }
  return result;
}

bool AssetRegistry::remove(const AssetGuid &guid) {
  if (!dependents(guid).empty()) return false;
  for (auto i = records_.begin(); i != records_.end(); ++i)
    if (i->guid == guid) { records_.erase(i); return true; }
  return false;
}

std::string AssetRegistry::serialize() const {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << "AETHER_ASSETS " << FormatVersion << ' ' << records_.size() << '\n';
  for (const auto &record : records_) {
    out << record.guid.text() << ' ' << assetTypeName(record.type) << ' '
        << std::quoted(record.path) << ' ' << std::quoted(record.source) << ' '
        << std::quoted(record.contentHash) << ' ' << record.importerVersion << ' '
        << std::quoted(record.importerParameters) << ' ' << record.dependencies.size();
    for (const auto &dependency : record.dependencies) out << ' ' << dependency.text();
    out << ' ' << record.derived.size();
    for (const auto &file : record.derived) out << ' ' << std::quoted(file);
    out << '\n';
  }
  return out.str();
}

bool AssetRegistry::deserialize(std::string_view text, AssetRegistry &out) {
  std::istringstream in{std::string(text)};
  in.imbue(std::locale::classic());
  std::string magic;
  u32 version = 0;
  usize count = 0;
  if (!(in >> magic >> version >> count) || magic != "AETHER_ASSETS") return false;
  if (version != FormatVersion || count > MaximumRecords) return false;
  AssetRegistry candidate;
  for (usize i = 0; i < count; ++i) {
    AssetRecord record;
    std::string guid, type;
    usize dependencyCount = 0, derivedCount = 0;
    if (!(in >> guid >> type)) return false;
    if (!AssetGuid::parse(guid, record.guid) || !parseAssetType(type, record.type)) return false;
    if (!(in >> std::quoted(record.path) >> std::quoted(record.source) >> std::quoted(record.contentHash) >>
          record.importerVersion >> std::quoted(record.importerParameters) >> dependencyCount))
      return false;
    if (dependencyCount > 4096) return false;
    for (usize d = 0; d < dependencyCount; ++d) {
      std::string dependency;
      AssetGuid parsed;
      if (!(in >> dependency) || !AssetGuid::parse(dependency, parsed)) return false;
      record.dependencies.push_back(parsed);
    }
    if (!(in >> derivedCount) || derivedCount > 4096) return false;
    for (usize d = 0; d < derivedCount; ++d) {
      std::string file;
      if (!(in >> std::quoted(file))) return false;
      record.derived.push_back(std::move(file));
    }
    // As dependências podem apontar para recursos declarados DEPOIS deste no
    // arquivo; por isso a verificação de existência fica para o fim.
    auto pending = std::move(record.dependencies);
    record.dependencies.clear();
    if (!candidate.add(std::move(record))) return false;
    candidate.records_.back().dependencies = std::move(pending);
  }
  for (const auto &record : candidate.records_) {
    if (!record.valid()) return false;
    for (const auto &dependency : record.dependencies) if (!candidate.find(dependency)) return false;
  }
  in >> std::ws;
  if (!in.eof()) return false;
  out = std::move(candidate);
  return true;
}

} // namespace ae::resources
