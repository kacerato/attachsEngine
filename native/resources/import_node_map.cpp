#include "resources/import_node_map.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace ae::resources {
namespace {
u64 mix(u64 hash, u64 value) {
  for (u32 i = 0; i < 8; ++i) { hash ^= (value >> (i * 8)) & 0xff; hash *= 1099511628211ull; }
  return hash;
}
i64 quantize(float value) { return std::isfinite(value) ? static_cast<i64>(std::llround(value * 1024.0)) : 0; }
bool sameMatrix(const float a[16], const float b[16]) { return std::memcmp(a, b, sizeof(float) * 16) == 0; }
struct GuidHash {
  usize operator()(const AssetGuid &guid) const noexcept { return static_cast<usize>(guid.high ^ (guid.low * 31)); }
};
} // namespace

const ImportNodeRecord *ImportNodeMap::find(const AssetGuid &id) const {
  const auto index = indexOf(id);
  return index >= 0 ? &nodes[static_cast<usize>(index)] : nullptr;
}

i32 ImportNodeMap::indexOf(const AssetGuid &id) const {
  if (!id.valid()) return -1;
  for (usize i = 0; i < nodes.size(); ++i) if (nodes[i].id == id) return static_cast<i32>(i);
  return -1;
}

bool ImportNodeMap::valid() const {
  if (nodes.size() > MaximumNodes || !revision) return false;
  std::unordered_set<AssetGuid, GuidHash> seen, draws;
  for (const auto &node : nodes) {
    if (!node.id.valid() || !seen.insert(node.id).second || node.name.size() > 4096 || node.authoredId.size() > 4096) return false;
    // O pai vem antes do filho: é a ordem em que a reconciliação cria objetos.
    if (node.parent.valid() && (node.parent == node.id || !seen.count(node.parent))) return false;
    if (!node.introduced || node.introduced > revision || node.draws.size() > 65536) return false;
    for (const auto &draw : node.draws) if (!draw.valid() || !draws.insert(draw).second) return false;
    for (float value : node.localMatrix) if (!std::isfinite(value)) return false;
    // Lente inválida no mapa criaria uma câmera que o componente recusaria na
    // hora de aplicar — falha tardia e sem explicação. Recusa aqui.
    if (node.camera && (!std::isfinite(node.cameraNear) || node.cameraNear <= 0 || !std::isfinite(node.cameraFar) ||
                        node.cameraFar < 0 || (node.cameraFar != 0 && node.cameraFar <= node.cameraNear) ||
                        !std::isfinite(node.cameraVerticalFov) || node.cameraVerticalFov <= 0 ||
                        node.cameraVerticalFov >= 180 || !std::isfinite(node.cameraHalfHeight) ||
                        node.cameraHalfHeight <= 0))
      return false;
  }
  return true;
}

std::string ImportNodeMap::serialize() const {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<float>::max_digits10);
  out << "ASTRA_NODEMAP " << FormatVersion << ' ' << revision << ' ' << std::quoted(sourceHash) << ' ' << nodes.size() << '\n';
  for (const auto &node : nodes) {
    out << node.id.text() << ' ' << (node.parent.valid() ? node.parent.text() : std::string("-")) << ' ' << node.introduced
        << ' ' << node.signature << ' ' << std::quoted(node.name) << ' ' << std::quoted(node.authoredId);
    for (float value : node.localMatrix) out << ' ' << value;
    out << ' ' << node.draws.size();
    for (const auto &draw : node.draws) out << ' ' << draw.text();
    out << ' ' << node.camera << ' ' << node.cameraOrthographic << ' ' << node.cameraVerticalFov << ' '
        << node.cameraNear << ' ' << node.cameraFar << ' ' << node.cameraHalfHeight;
    out << '\n';
  }
  return out.str();
}

bool ImportNodeMap::deserialize(std::string_view text, ImportNodeMap &out) {
  if (text.size() > 64u * 1024u * 1024u) return false;
  std::istringstream in{std::string(text)};
  in.imbue(std::locale::classic());
  std::string magic;
  u32 version = 0;
  usize count = 0;
  ImportNodeMap candidate;
  if (!(in >> magic >> version >> candidate.revision >> std::quoted(candidate.sourceHash) >> count) ||
      magic != "ASTRA_NODEMAP" || version != FormatVersion || count > MaximumNodes)
    return false;
  candidate.nodes.reserve(count);
  for (usize i = 0; i < count; ++i) {
    ImportNodeRecord node;
    std::string id, parent;
    usize draws = 0;
    if (!(in >> id >> parent >> node.introduced >> node.signature >> std::quoted(node.name) >> std::quoted(node.authoredId)))
      return false;
    if (!AssetGuid::parse(id, node.id) || (parent != "-" && !AssetGuid::parse(parent, node.parent))) return false;
    for (float &value : node.localMatrix) if (!(in >> value)) return false;
    if (!(in >> draws) || draws > 65536) return false;
    for (usize d = 0; d < draws; ++d) {
      std::string draw;
      AssetGuid parsed;
      if (!(in >> draw) || !AssetGuid::parse(draw, parsed)) return false;
      node.draws.push_back(parsed);
    }
    if (!(in >> node.camera >> node.cameraOrthographic >> node.cameraVerticalFov >> node.cameraNear >>
          node.cameraFar >> node.cameraHalfHeight))
      return false;
    candidate.nodes.push_back(std::move(node));
  }
  in >> std::ws;
  if (!in.eof() || !candidate.valid()) return false;
  out = std::move(candidate);
  return true;
}

std::string importNodeMapPath(const AssetGuid &source) { return ".astra/imports/" + source.text() + ".nodes"; }

std::vector<u64> importNodeSignatures(const GltfImport &model) {
  std::vector<u64> signatures(model.nodes.size(), 0);
  std::vector<u32> primitive(model.nodes.size(), 0);
  for (usize d = 0; d < model.draws.size() && d < model.drawNodes.size(); ++d) {
    const auto node = model.drawNodes[d];
    if (node >= signatures.size()) continue;
    auto hash = signatures[node] ? signatures[node] : 14695981039346656037ull;
    const auto &draw = model.draws[d];
    hash = mix(hash, primitive[node]++);
    hash = mix(hash, draw.indexCount);
    for (u32 axis = 0; axis < 3; ++axis) hash = mix(hash, static_cast<u64>(quantize(draw.boundsCenter[axis])));
    hash = mix(hash, static_cast<u64>(quantize(draw.boundsRadius)));
    signatures[node] = hash ? hash : 1;
  }
  return signatures;
}

bool buildImportNodeMap(const GltfImport &model, const AssetGuid &source, std::string_view contentHash,
                        const ImportNodeMap *previous, ImportAmbiguityPolicy policy, ImportNodeMap &out,
                        ImportMatchReport &report, std::string &diagnostic) {
  report = {};
  diagnostic.clear();
  const usize count = model.nodes.size();
  if (!source.valid() || !count || count > ImportNodeMap::MaximumNodes || model.drawNodes.size() != model.draws.size()) {
    diagnostic = "Modelo sem árvore coerente para o mapa de nós.";
    return false;
  }
  for (usize n = 0; n < count; ++n)
    if (model.nodes[n].parent >= static_cast<i32>(n) || model.nodes[n].parent < -1) {
      diagnostic = "Hierarquia fora de ordem no modelo.";
      return false;
    }
  const auto signatures = importNodeSignatures(model);
  std::vector<std::vector<u32>> drawsOf(count);
  for (usize d = 0; d < model.drawNodes.size(); ++d) {
    if (model.drawNodes[d] >= count) { diagnostic = "Desenho sem nó."; return false; }
    drawsOf[model.drawNodes[d]].push_back(static_cast<u32>(d));
  }

  const bool fresh = !previous || previous->nodes.empty();
  const usize previousCount = fresh ? 0 : previous->nodes.size();
  std::vector<i32> match(count, -1), taken(previousCount, -1);
  std::vector<i32> previousParent(previousCount, -1);
  for (usize j = 0; j < previousCount; ++j) previousParent[j] = previous->indexOf(previous->nodes[j].parent);

  const auto link = [&](usize n, usize j) { match[n] = static_cast<i32>(j); taken[j] = static_cast<i32>(n); };

  // Mesmo conteúdo: a correspondência por posição é PROVADA pelo hash, não
  // suposta. É o caso de toda reabertura do projeto.
  if (!fresh && !contentHash.empty() && previous->sourceHash == contentHash && previousCount == count) {
    bool consistent = true;
    for (usize n = 0; n < count && consistent; ++n)
      consistent = previous->nodes[n].name == model.nodes[n].name && previousParent[n] == model.nodes[n].parent &&
                   previous->nodes[n].draws.size() == drawsOf[n].size();
    if (consistent) {
      for (usize n = 0; n < count; ++n) link(n, n);
      report.sameContent = true;
      report.byStructure = static_cast<u32>(count);
    }
  }

  // Pai do nó novo expresso no arquivo ANTERIOR: -1 raiz, -2 ainda desconhecido.
  const auto parentPrevious = [&](usize n) -> i64 {
    const auto parent = model.nodes[n].parent;
    if (parent < 0) return -1;
    return match[static_cast<usize>(parent)] >= 0 ? match[static_cast<usize>(parent)] : -2;
  };

  if (!fresh && !report.sameContent) {
    // 1. Identificador autoral.
    std::unordered_map<std::string, i32> authoredNew, authoredOld;
    for (usize n = 0; n < count; ++n)
      if (!model.nodes[n].authoredId.empty()) {
        auto [it, inserted] = authoredNew.try_emplace(model.nodes[n].authoredId, static_cast<i32>(n));
        if (!inserted) it->second = -1;
      }
    for (usize j = 0; j < previousCount; ++j)
      if (!previous->nodes[j].authoredId.empty()) {
        auto [it, inserted] = authoredOld.try_emplace(previous->nodes[j].authoredId, static_cast<i32>(j));
        if (!inserted) it->second = -1;
      }
    for (const auto &[key, n] : authoredNew) {
      const auto old = authoredOld.find(key);
      if (n < 0 || old == authoredOld.end() || old->second < 0) continue;
      link(static_cast<usize>(n), static_cast<usize>(old->second));
      ++report.byAuthoredId;
    }

    struct Group { std::vector<u32> incoming, candidates; };
    // Pareia os dois lados de um grupo pelos critérios, do mais estrito ao mais
    // frouxo. Devolve quantas ligações fez.
    const auto pairUnique = [&](Group &group, bool byMatrix) {
      u32 made = 0;
      for (const auto n : group.incoming) {
        if (match[n] >= 0) continue;
        i32 found = -1;
        u32 hits = 0;
        for (const auto j : group.candidates) {
          if (taken[j] >= 0 || previous->nodes[j].signature != signatures[n]) continue;
          if (byMatrix && !sameMatrix(previous->nodes[j].localMatrix, model.nodes[n].localMatrix)) continue;
          found = static_cast<i32>(j);
          ++hits;
        }
        if (hits != 1) continue;
        u32 reverse = 0;
        for (const auto m : group.incoming) {
          if (match[m] >= 0 || signatures[m] != previous->nodes[static_cast<usize>(found)].signature) continue;
          if (byMatrix && !sameMatrix(previous->nodes[static_cast<usize>(found)].localMatrix, model.nodes[m].localMatrix)) continue;
          ++reverse;
        }
        if (reverse != 1) continue;
        link(n, static_cast<usize>(found));
        ++made;
      }
      return made;
    };
    const auto remaining = [&](const Group &group, std::vector<u32> &incoming, std::vector<u32> &candidates) {
      incoming.clear();
      candidates.clear();
      for (const auto n : group.incoming) if (match[n] < 0) incoming.push_back(n);
      for (const auto j : group.candidates) if (taken[j] < 0) candidates.push_back(j);
    };

    std::vector<Group> ambiguous;
    for (u32 pass = 0; pass < count + 2; ++pass) {
      u32 progress = 0;
      ambiguous.clear();
      // 2. Mesmo pai e mesmo nome.
      std::unordered_map<std::string, Group> groups;
      const auto key = [](i64 parent, const std::string &name) { return std::to_string(parent) + '\x1f' + name; };
      for (usize n = 0; n < count; ++n) {
        const auto parent = parentPrevious(n);
        if (match[n] < 0 && parent != -2) groups[key(parent, model.nodes[n].name)].incoming.push_back(static_cast<u32>(n));
      }
      for (usize j = 0; j < previousCount; ++j) {
        if (taken[j] >= 0) continue;
        const auto found = groups.find(key(previousParent[j], previous->nodes[j].name));
        if (found != groups.end()) found->second.candidates.push_back(static_cast<u32>(j));
      }
      // Ordem do arquivo, para o resultado não depender da ordem do hash.
      std::vector<Group *> ordered;
      for (auto &[unused, group] : groups) if (!group.candidates.empty()) ordered.push_back(&group);
      std::sort(ordered.begin(), ordered.end(), [](const Group *a, const Group *b) { return a->incoming.front() < b->incoming.front(); });
      for (auto *group : ordered) {
        u32 made = pairUnique(*group, true);
        made += pairUnique(*group, false);
        std::vector<u32> incoming, candidates;
        remaining(*group, incoming, candidates);
        if (incoming.size() == 1 && candidates.size() == 1) {
          link(incoming.front(), candidates.front());
          ++made;
        } else if (!incoming.empty() && !candidates.empty()) {
          ambiguous.push_back({incoming, candidates});
        }
        report.byStructure += made;
        progress += made;
      }
      if (progress) continue;

      // 3. Renomeação: mesmo pai, geometria e pose idênticas. Um grupo vazio com
      // matriz identidade não prova nada, e fica de fora.
      std::unordered_map<i64, Group> siblings;
      const float identity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
      for (usize n = 0; n < count; ++n) {
        const auto parent = parentPrevious(n);
        if (match[n] < 0 && parent != -2 && (signatures[n] || !sameMatrix(model.nodes[n].localMatrix, identity)))
          siblings[parent].incoming.push_back(static_cast<u32>(n));
      }
      for (usize j = 0; j < previousCount; ++j) {
        const auto found = siblings.find(previousParent[j]);
        if (taken[j] < 0 && found != siblings.end()) found->second.candidates.push_back(static_cast<u32>(j));
      }
      for (auto &[unused, group] : siblings) {
        const auto made = pairUnique(group, true);
        report.renamed += made;
        progress += made;
      }
      if (progress) continue;

      // 4. Mudança de pai: nome e geometria únicos no arquivo inteiro.
      std::unordered_map<std::string, Group> global;
      for (usize n = 0; n < count; ++n)
        if (match[n] < 0 && signatures[n]) global[model.nodes[n].name].incoming.push_back(static_cast<u32>(n));
      for (usize j = 0; j < previousCount; ++j) {
        const auto found = global.find(previous->nodes[j].name);
        if (taken[j] < 0 && found != global.end()) found->second.candidates.push_back(static_cast<u32>(j));
      }
      for (auto &[unused, group] : global) {
        if (group.incoming.size() != 1 || group.candidates.size() != 1) continue;
        const auto made = pairUnique(group, false);
        report.reparented += made;
        progress += made;
      }
      if (progress) continue;

      // Só sobraram grupos ambíguos. Associar pela ordem é decisão do usuário.
      if (policy == ImportAmbiguityPolicy::MatchInOrder && !ambiguous.empty()) {
        for (auto &group : ambiguous) {
          std::vector<u32> incoming, candidates;
          remaining(group, incoming, candidates);
          for (usize i = 0; i < incoming.size() && i < candidates.size(); ++i) {
            link(incoming[i], candidates[i]);
            ++progress;
            ++report.byStructure;
          }
        }
        ambiguous.clear();
        if (progress) continue;
      }
      break;
    }
    for (const auto &group : ambiguous) {
      ImportAmbiguity item;
      const auto parent = model.nodes[group.incoming.front()].parent;
      item.parent = parent >= 0 ? model.nodes[static_cast<usize>(parent)].name : std::string();
      item.name = model.nodes[group.incoming.front()].name;
      item.incoming = static_cast<u32>(group.incoming.size());
      item.previous = static_cast<u32>(group.candidates.size());
      report.ambiguities.push_back(std::move(item));
    }
    if (!report.ambiguities.empty() && policy == ImportAmbiguityPolicy::Refuse) {
      diagnostic = "Correspondência ambígua entre nós de mesmo nome; escolha como associar.";
      return false;
    }
  }

  ImportNodeMap candidate;
  candidate.sourceHash = std::string(contentHash);
  candidate.revision = fresh ? std::max<u32>(1, previous ? previous->revision : 1)
                             : previous->revision + (report.sameContent ? 0 : 1);
  if (!candidate.revision) { diagnostic = "Revisão do mapa esgotada."; return false; }
  std::unordered_set<AssetGuid, GuidHash> usedNodes, usedDraws;
  if (!fresh)
    for (const auto &node : previous->nodes) {
      usedNodes.insert(node.id);
      for (const auto &draw : node.draws) usedDraws.insert(draw);
    }
  // Um caminho de nomes com a ocorrência entre irmãos homônimos: só semente de
  // identidade nova, nunca critério de correspondência.
  std::vector<std::string> paths(count);
  std::vector<std::unordered_map<std::string, u32>> occurrences(count + 1);
  const auto unique = [](std::unordered_set<AssetGuid, GuidHash> &used, const std::string &seed) {
    auto guid = assetGuidFromSeed(seed);
    for (u32 bump = 1; used.count(guid); ++bump) guid = assetGuidFromSeed(seed + "~" + std::to_string(bump));
    used.insert(guid);
    return guid;
  };
  candidate.nodes.resize(count);
  for (usize n = 0; n < count; ++n) {
    const auto &node = model.nodes[n];
    auto &record = candidate.nodes[n];
    const usize scope = node.parent < 0 ? count : static_cast<usize>(node.parent);
    const auto occurrence = occurrences[scope][node.name]++;
    paths[n] = (node.parent < 0 ? std::string() : paths[static_cast<usize>(node.parent)] + "/") + node.name + "#" +
               std::to_string(occurrence);
    record.name = node.name;
    record.authoredId = node.authoredId;
    record.signature = signatures[n];
    std::copy(node.localMatrix, node.localMatrix + 16, record.localMatrix);
    // A câmera do arquivo acompanha o nó dela. O importador já validou a lente;
    // aqui é só transporte, para que a reconciliação saiba criar o componente
    // junto com o objeto.
    for (const auto &camera : model.cameras)
      if (camera.node == n) {
        record.camera = true;
        record.cameraOrthographic = camera.orthographic;
        record.cameraVerticalFov = camera.verticalFovDegrees;
        record.cameraNear = camera.nearPlane;
        record.cameraFar = camera.farPlane;
        record.cameraHalfHeight = camera.orthographicHalfHeight;
        break;
      }
    record.parent = node.parent < 0 ? AssetGuid{} : candidate.nodes[static_cast<usize>(node.parent)].id;
    const ImportNodeRecord *old = match[n] >= 0 ? &previous->nodes[static_cast<usize>(match[n])] : nullptr;
    if (old) {
      record.id = old->id;
      record.introduced = old->introduced;
    } else {
      const std::string seed = fresh ? "no:" + source.text() + ":" + paths[n]
                                     : "no:" + source.text() + ":r" + std::to_string(candidate.revision) + ":" + paths[n];
      record.id = unique(usedNodes, seed);
      record.introduced = candidate.revision;
      if (!fresh) ++report.added;
    }
    for (usize p = 0; p < drawsOf[n].size(); ++p) {
      if (old && p < old->draws.size()) {
        record.draws.push_back(old->draws[p]);
        continue;
      }
      // A chave legada primeiro: é a identidade que cenas anteriores ao mapa já
      // gravaram. Se ela pertencia a outra peça, nasce uma identidade do nó.
      const auto draw = drawsOf[n][p];
      const std::string legacy = "glb:" + source.text() + ":" + (draw < model.keys.size() ? model.keys[draw] : std::to_string(draw));
      auto guid = assetGuidFromSeed(legacy);
      if (usedDraws.count(guid)) guid = unique(usedDraws, "glb:" + source.text() + ":" + record.id.text() + ":" + std::to_string(p));
      else usedDraws.insert(guid);
      record.draws.push_back(guid);
    }
  }
  for (usize j = 0; j < previousCount; ++j)
    if (taken[j] < 0) {
      ++report.removed;
      report.removedNodes.push_back(previous->nodes[j].id);
    }
  if (!candidate.valid()) { diagnostic = "Mapa de nós inconsistente; fonte anterior preservada."; return false; }
  out = std::move(candidate);
  return true;
}

} // namespace ae::resources
