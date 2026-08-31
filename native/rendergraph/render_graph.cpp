#include "rendergraph/render_graph.h"

#include <algorithm>
#include <deque>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace ae::rendergraph {

ResourceId RenderGraph::addResource(ResourceDesc desc) {
  resources_.push_back(std::move(desc));
  return static_cast<ResourceId>(resources_.size() - 1);
}

PassId RenderGraph::addPass(PassDesc desc) {
  passes_.push_back(std::move(desc));
  return static_cast<PassId>(passes_.size() - 1);
}

namespace {

// Uma aresta de dependência entre passes, anotada com o tipo de acesso que a
// originou. `viaInputAttachment` é o que a fusão de subpasses consulta.
struct Edge {
  PassId from = kInvalidPass;
  PassId to = kInvalidPass;
  bool viaInputAttachment = false;
};

// Constrói as arestas de dependência (RAW/WAW/WAR) a partir da ordem de
// declaração dos passes. Não assumimos nenhuma ordem implícita além da
// ordem de declaração do usuário — dois passes que nunca tocam o mesmo
// recurso não recebem aresta nenhuma entre si (ficam livres para qualquer
// ordem relativa na topológica).
std::vector<Edge> buildEdges(const RenderGraph &graph) {
  std::vector<Edge> edges;

  struct ResourceState {
    std::optional<PassId> lastWriter;
    std::vector<PassId> readersSinceLastWrite;
  };
  std::unordered_map<ResourceId, ResourceState> state;

  for (PassId p = 0; p < static_cast<PassId>(graph.passCount()); ++p) {
    const PassDesc &pass = graph.pass(p);

    // Leituras primeiro: dependem apenas do último escritor (RAW). Duas
    // leituras nunca geram aresta entre si — é exatamente o caso
    // "read->read não insere barreira" que o teste cobra.
    for (const auto &r : pass.reads) {
      auto &st = state[r.resource];
      if (st.lastWriter.has_value()) {
        edges.push_back({*st.lastWriter, p, r.asInputAttachment});
      }
      st.readersSinceLastWrite.push_back(p);
    }

    // Escritas: dependem do escritor anterior (WAW) e de todos os leitores
    // desde então (WAR), preservando a ordem de declaração.
    for (const auto &w : pass.writes) {
      auto &st = state[w.resource];
      if (st.lastWriter.has_value()) {
        edges.push_back({*st.lastWriter, p, false});
      }
      for (PassId reader : st.readersSinceLastWrite) {
        if (reader != p) edges.push_back({reader, p, false});
      }
      st.lastWriter = p;
      st.readersSinceLastWrite.clear();
    }

    // Dependencias explicitas: unico jeito de o grafo fechar um ciclo, pois
    // arestas por recurso seguem sempre a ordem de declaracao.
    for (PassId dep : pass.explicitDependsOn) {
      edges.push_back({dep, p, false});
    }
  }

  return edges;
}

} // namespace

std::optional<CompiledGraph> RenderGraph::compile(CompileError *outError) const {
  const u32 n = static_cast<u32>(passes_.size());
  std::vector<Edge> edges = buildEdges(*this);

  // Adjacência de saída (para topo/BFS) e de entrada (para poda reversa).
  std::vector<std::vector<PassId>> outAdj(n), inAdj(n);
  std::vector<std::vector<bool>> edgeIsAttachment(n); // paralelo a outAdj[from]
  for (const Edge &e : edges) {
    outAdj[e.from].push_back(e.to);
    edgeIsAttachment[e.from].push_back(e.viaInputAttachment);
    inAdj[e.to].push_back(e.from);
  }

  // --- 1. Poda de passes mortos --------------------------------------
  // Um pass é necessário se escreve em algum recurso importado (visível
  // fora do grafo) ou se explicitamente não escreve nada (pass com efeito
  // colateral externo, ex.: readback — tratamos como necessário para não
  // remover silenciosamente algo que o usuário pediu). Todo predecessor de
  // um pass necessário também é necessário (ele produz o que o necessário
  // consome).
  std::vector<bool> necessary(n, false);
  for (PassId p = 0; p < n; ++p) {
    const PassDesc &pass = passes_[p];
    bool writesImported = false;
    for (const auto &w : pass.writes) {
      if (resources_[w.resource].imported) { writesImported = true; break; }
    }
    if (writesImported || pass.writes.empty()) necessary[p] = true;
  }
  {
    std::deque<PassId> queue;
    for (PassId p = 0; p < n; ++p) if (necessary[p]) queue.push_back(p);
    std::vector<bool> visited = necessary;
    while (!queue.empty()) {
      PassId p = queue.front(); queue.pop_front();
      for (PassId pred : inAdj[p]) {
        if (!visited[pred]) {
          visited[pred] = true;
          necessary[pred] = true;
          queue.push_back(pred);
        }
      }
    }
  }

  std::vector<PassId> culled;
  for (PassId p = 0; p < n; ++p) if (!necessary[p]) culled.push_back(p);

  // --- 2. Ordenação topológica (Kahn) sobre o subgrafo vivo, com detecção
  //        de ciclo -----------------------------------------------------
  std::vector<u32> indegree(n, 0);
  for (PassId p = 0; p < n; ++p) {
    if (!necessary[p]) continue;
    for (PassId to : outAdj[p]) {
      if (necessary[to]) indegree[to]++;
    }
  }

  std::deque<PassId> ready;
  for (PassId p = 0; p < n; ++p) {
    if (necessary[p] && indegree[p] == 0) ready.push_back(p);
  }

  std::vector<PassId> order;
  order.reserve(n);
  while (!ready.empty()) {
    // Ordem determinística: sempre o menor id disponível, para que a saída
    // exportada seja estável entre execuções e fácil de testar.
    std::sort(ready.begin(), ready.end());
    PassId p = ready.front();
    ready.pop_front();
    order.push_back(p);
    for (PassId to : outAdj[p]) {
      if (!necessary[to]) continue;
      if (--indegree[to] == 0) ready.push_back(to);
    }
  }

  u32 aliveCount = 0;
  for (PassId p = 0; p < n; ++p) if (necessary[p]) aliveCount++;
  if (order.size() != aliveCount) {
    if (outError) {
      outError->message =
          "ciclo detectado no render graph: existe uma dependência circular "
          "entre passes que impede uma ordem de execução válida";
    }
    return std::nullopt;
  }

  // Posição de cada pass na ordem final, usada para tempos de vida e barreiras.
  std::vector<u32> position(n, 0);
  for (u32 i = 0; i < order.size(); ++i) position[order[i]] = i;

  CompiledGraph result;
  result.executionOrder = order;
  result.culledPasses = culled;

  // --- 3. Tempo de vida dos recursos transitórios ----------------------
  std::unordered_map<ResourceId, ResourceLifetime> lifetimeMap;
  auto touch = [&](ResourceId r, u32 pos) {
    if (resources_[r].imported) return; // vida útil de importado não é gerenciada aqui
    auto it = lifetimeMap.find(r);
    if (it == lifetimeMap.end()) {
      lifetimeMap.emplace(r, ResourceLifetime{r, pos, pos});
    } else {
      it->second.firstUse = std::min(it->second.firstUse, pos);
      it->second.lastUse = std::max(it->second.lastUse, pos);
    }
  };
  for (u32 pos = 0; pos < order.size(); ++pos) {
    const PassDesc &pass = passes_[order[pos]];
    for (const auto &r : pass.reads) touch(r.resource, pos);
    for (const auto &w : pass.writes) touch(w.resource, pos);
  }
  for (auto &kv : lifetimeMap) result.lifetimes.push_back(kv.second);
  std::sort(result.lifetimes.begin(), result.lifetimes.end(),
            [](const ResourceLifetime &a, const ResourceLifetime &b) {
              return a.resource < b.resource;
            });

  // --- 4. Aliasing de memória transitória ------------------------------
  // Bin-packing guloso: percorre recursos transitórios em ordem de
  // primeiro uso; cada um entra no primeiro slot já livre (lastUse < seu
  // firstUse) cujo tamanho comporta; senão abre um slot novo. É um
  // algoritmo guloso simples e determinístico — não é ótimo em geral, mas
  // captura o caso comum do plano (passes em cadeia reaproveitando o
  // mesmo espaço) e a economia é fácil de verificar em teste.
  {
    std::vector<ResourceLifetime> sorted = result.lifetimes;
    std::sort(sorted.begin(), sorted.end(),
              [](const ResourceLifetime &a, const ResourceLifetime &b) {
                return a.firstUse < b.firstUse;
              });

    std::vector<AliasSlot> slots;
    std::vector<u32> slotLastUse;
    u64 totalOriginal = 0;
    for (const auto &lt : sorted) {
      u64 size = resources_[lt.resource].sizeBytes;
      totalOriginal += size;
      i64 chosen = -1;
      for (usize s = 0; s < slots.size(); ++s) {
        if (slotLastUse[s] < lt.firstUse && slots[s].sizeBytes >= size) {
          chosen = static_cast<i64>(s);
          break;
        }
      }
      if (chosen < 0) {
        AliasSlot slot;
        slot.sizeBytes = size;
        slots.push_back(std::move(slot));
        slotLastUse.push_back(lt.lastUse);
        chosen = static_cast<i64>(slots.size() - 1);
      } else {
        slotLastUse[static_cast<usize>(chosen)] = lt.lastUse;
      }
      slots[static_cast<usize>(chosen)].members.push_back(lt.resource);
    }

    u64 totalAliased = 0;
    for (const auto &s : slots) totalAliased += s.sizeBytes;
    result.aliasSlots = std::move(slots);
    result.aliasingSavingsBytes = totalOriginal >= totalAliased ? totalOriginal - totalAliased : 0;
  }

  // --- 5. Barreiras -----------------------------------------------------
  // Para cada recurso, percorremos seus acessos na ordem final de execução
  // e inserimos barreira entre acessos adjacentes exceto read->read.
  {
    struct AccessRec { u32 pos; PassId pass; bool isWrite; };
    std::unordered_map<ResourceId, std::vector<AccessRec>> accessesByResource;
    for (u32 pos = 0; pos < order.size(); ++pos) {
      const PassDesc &pass = passes_[order[pos]];
      for (const auto &r : pass.reads) {
        accessesByResource[r.resource].push_back({pos, order[pos], false});
      }
      for (const auto &w : pass.writes) {
        accessesByResource[w.resource].push_back({pos, order[pos], true});
      }
    }
    for (auto &kv : accessesByResource) {
      ResourceId rid = kv.first;
      auto &accesses = kv.second;
      std::sort(accesses.begin(), accesses.end(),
                [](const AccessRec &a, const AccessRec &b) { return a.pos < b.pos; });
      for (usize i = 1; i < accesses.size(); ++i) {
        const AccessRec &prev = accesses[i - 1];
        const AccessRec &cur = accesses[i];
        if (prev.pos == cur.pos) continue; // mesmo pass lendo e escrevendo: sem barreira entre si
        if (!prev.isWrite && !cur.isWrite) continue; // read->read: sem barreira

        Barrier b;
        b.resource = rid;
        b.beforePass = prev.pass;
        b.afterPass = cur.pass;
        bool depth = resources_[rid].isDepth;
        b.oldLayout = prev.isWrite
                           ? (depth ? ResourceLayout::DepthStencilAttachment : ResourceLayout::ColorAttachment)
                           : ResourceLayout::ShaderReadOnly;
        b.newLayout = cur.isWrite
                           ? (depth ? ResourceLayout::DepthStencilAttachment : ResourceLayout::ColorAttachment)
                           : ResourceLayout::ShaderReadOnly;
        b.srcStage = prev.isWrite ? "ColorAttachmentOutput" : "FragmentShader";
        b.dstStage = cur.isWrite ? "ColorAttachmentOutput" : "FragmentShader";
        b.srcAccess = prev.isWrite ? "ColorAttachmentWrite" : "ShaderRead";
        b.dstAccess = cur.isWrite ? "ColorAttachmentWrite" : "ShaderRead";
        result.barriers.push_back(b);
      }
    }
  }

  // --- 6. LoadOp/StoreOp por análise de uso -----------------------------
  {
    struct FirstLast { PassId firstPass; bool firstIsFullOverwrite; PassId lastPass; bool lastIsWrite; };
    std::unordered_map<ResourceId, FirstLast> fl;
    for (u32 pos = 0; pos < order.size(); ++pos) {
      PassId pid = order[pos];
      const PassDesc &pass = passes_[pid];
      for (const auto &r : pass.reads) {
        auto it = fl.find(r.resource);
        if (it == fl.end()) fl.emplace(r.resource, FirstLast{pid, false, pid, false});
        else { it->second.lastPass = pid; it->second.lastIsWrite = false; }
      }
      for (const auto &w : pass.writes) {
        auto it = fl.find(w.resource);
        if (it == fl.end()) fl.emplace(w.resource, FirstLast{pid, w.fullOverwrite, pid, true});
        else { it->second.lastPass = pid; it->second.lastIsWrite = true; }
      }
    }
    for (auto &kv : fl) {
      ResourceId rid = kv.first;
      const FirstLast &f = kv.second;
      AttachmentUse use;
      use.resource = rid;
      use.pass = f.firstPass;
      // DontCare no load quando o primeiro toque é uma escrita que
      // sobrescreve o recurso inteiro — não há conteúdo anterior a
      // preservar, então carregar da DRAM seria banda desperdiçada.
      use.loadOp = f.firstIsFullOverwrite ? LoadOp::DontCare : LoadOp::Load;
      // DontCare no store quando ninguém lê o valor final: nem um pass
      // subsequente do grafo, nem um consumidor externo (recurso não
      // importado). Recursos importados sempre armazenam — um consumidor
      // fora do grafo pode depender do conteúdo.
      bool consumedAfter = !f.lastIsWrite; // última operação foi leitura => alguém leu o resultado
      bool mustStoreForExternal = resources_[rid].imported;
      use.storeOp = (consumedAfter || mustStoreForExternal) ? StoreOp::Store : StoreOp::DontCare;
      result.attachmentUses.push_back(use);
    }
    std::sort(result.attachmentUses.begin(), result.attachmentUses.end(),
              [](const AttachmentUse &a, const AttachmentUse &b) { return a.resource < b.resource; });
  }

  // --- 8. Fusão de passes em subpasses -----------------------------------
  // (numeração segue a do enunciado; feita antes do item 7 porque a
  // marcação memoryless do item 7 depende dos grupos calculados aqui.)
  //
  // Critério: passes consecutivos na ordem de execução fundem no mesmo
  // grupo quando (a) ambos são passes de rasterização com a MESMA extensão
  // de render target e (b) toda aresta de entrada de um pass vindo de fora
  // do grupo atual é uma leitura de input attachment (mesmo pixel) — nunca
  // fundimos por causa de uma amostragem arbitrária de textura, que pode
  // ler qualquer pixel e exigiria memória completa, não só o registrador
  // do tile.
  {
    std::vector<std::unordered_set<PassId>> incomingNonAttachmentFrom(n);
    // Para cada pass, guarda o conjunto de predecessores cuja aresta NÃO é
    // input-attachment (usado para vetar fusão).
    for (PassId from = 0; from < n; ++from) {
      for (usize i = 0; i < outAdj[from].size(); ++i) {
        PassId to = outAdj[from][i];
        bool viaAttachment = edgeIsAttachment[from][i];
        if (!viaAttachment) incomingNonAttachmentFrom[to].insert(from);
      }
    }

    std::vector<SubpassGroup> groups;
    usize i = 0;
    while (i < order.size()) {
      PassId p = order[i];
      const PassDesc &pd = passes_[p];
      SubpassGroup group;
      group.passes.push_back(p);
      if (pd.extentWidth != 0 && pd.extentHeight != 0) {
        usize j = i + 1;
        std::unordered_set<PassId> inGroup{p};
        while (j < order.size()) {
          PassId q = order[j];
          const PassDesc &qd = passes_[q];
          if (qd.extentWidth != pd.extentWidth || qd.extentHeight != pd.extentHeight) break;
          // q so pode entrar no grupo se NENHUMA dependencia sua sobre um
          // pass ja fundido no grupo for uma dependencia "comum" (nao via
          // input attachment) — essa e exatamente a dependencia que exige
          // amostragem arbitraria de pixel, incompativel com subpass unico.
          // Dependencias sobre passes de FORA do grupo nao importam aqui:
          // eles ja rodaram antes (ordem topologica) e nao impedem fusao.
          bool ok = true;
          for (PassId dep : incomingNonAttachmentFrom[q]) {
            if (inGroup.count(dep)) { ok = false; break; }
          }
          if (!ok) break;
          group.passes.push_back(q);
          inGroup.insert(q);
          ++j;
        }
        i = j;
      } else {
        ++i;
      }
      group.merged = group.passes.size() > 1;
      groups.push_back(std::move(group));
    }
    result.subpassGroups = std::move(groups);
  }

  // --- 7. Anexos memoryless ----------------------------------------------
  // Um recurso transitório nunca precisa ir à DRAM quando todos os passes que
  // o tocam (primeiro ao último) pertencem ao mesmo grupo de render target — o
  // tile fica inteiro na memória on-chip da GPU TBDR do início ao fim da vida
  // do recurso — e nenhum consumidor posterior exige seu conteúdo.
  //
  // A condição NÃO é o grupo estar fundido. Um grupo de um pass só é o caso
  // memoryless mais comum em mobile: o depth de um forward renderer de passe
  // único, escrito e descartado dentro do mesmo render pass. Exigir fusão
  // deixava justamente esse caso de fora, e nenhum anexo real do renderer
  // conseguia LAZILY_ALLOCATED.
  //
  // A contenção no grupo já é suficiente e não se confunde com o storeOp: num
  // grupo fundido o consumidor lê como input attachment, dentro do tile, então
  // o recurso é "consumido" (storeOp Store) e mesmo assim nunca vai à DRAM. Se
  // algum pass fora do grupo lesse o recurso, ele seria o lastUse e cairia em
  // outro grupo, reprovando a checagem abaixo.
  {
    std::unordered_map<PassId, usize> groupOf;
    for (usize g = 0; g < result.subpassGroups.size(); ++g) {
      for (PassId p : result.subpassGroups[g].passes) groupOf[p] = g;
    }
    for (const auto &lt : result.lifetimes) {
      if (resources_[lt.resource].imported) continue;
      PassId firstPass = order[lt.firstUse];
      PassId lastPass = order[lt.lastUse];
      if (groupOf.count(firstPass) == 0 || groupOf.count(lastPass) == 0) continue;
      if (groupOf[firstPass] != groupOf[lastPass]) continue;
      result.memorylessResources.push_back(lt.resource);
    }
    std::sort(result.memorylessResources.begin(), result.memorylessResources.end());
  }

  return result;
}

namespace {
const char *loadOpName(LoadOp op) {
  switch (op) {
    case LoadOp::Load: return "Load";
    case LoadOp::Clear: return "Clear";
    case LoadOp::DontCare: return "DontCare";
  }
  return "?";
}
const char *storeOpName(StoreOp op) {
  switch (op) {
    case StoreOp::Store: return "Store";
    case StoreOp::DontCare: return "DontCare";
  }
  return "?";
}
const char *layoutName(ResourceLayout l) {
  switch (l) {
    case ResourceLayout::Undefined: return "Undefined";
    case ResourceLayout::ColorAttachment: return "ColorAttachment";
    case ResourceLayout::DepthStencilAttachment: return "DepthStencilAttachment";
    case ResourceLayout::ShaderReadOnly: return "ShaderReadOnly";
  }
  return "?";
}
} // namespace

std::string CompiledGraph::exportText(const RenderGraph &graph) const {
  std::ostringstream out;
  out << "=== Render Graph Compilado ===\n";

  out << "-- Ordem de execucao --\n";
  for (usize i = 0; i < executionOrder.size(); ++i) {
    PassId p = executionOrder[i];
    out << "  [" << i << "] " << graph.pass(p).name << " (id=" << p << ")\n";
  }

  if (!culledPasses.empty()) {
    out << "-- Passes podados (mortos) --\n";
    for (PassId p : culledPasses) {
      out << "  " << graph.pass(p).name << " (id=" << p << ")\n";
    }
  }

  out << "-- Grupos de subpass --\n";
  for (usize g = 0; g < subpassGroups.size(); ++g) {
    const auto &group = subpassGroups[g];
    out << "  grupo " << g << (group.merged ? " [FUNDIDO]" : "") << ": ";
    for (usize k = 0; k < group.passes.size(); ++k) {
      if (k) out << " + ";
      out << graph.pass(group.passes[k]).name;
    }
    out << "\n";
  }

  out << "-- Tempos de vida --\n";
  for (const auto &lt : lifetimes) {
    out << "  " << graph.resource(lt.resource).name << ": [" << lt.firstUse
        << ", " << lt.lastUse << "]\n";
  }

  out << "-- Aliasing (economia total: " << aliasingSavingsBytes << " bytes) --\n";
  for (usize s = 0; s < aliasSlots.size(); ++s) {
    out << "  slot " << s << " (" << aliasSlots[s].sizeBytes << " bytes): ";
    for (usize k = 0; k < aliasSlots[s].members.size(); ++k) {
      if (k) out << ", ";
      out << graph.resource(aliasSlots[s].members[k]).name;
    }
    out << "\n";
  }

  out << "-- Anexos memoryless --\n";
  for (ResourceId r : memorylessResources) {
    out << "  " << graph.resource(r).name << "\n";
  }

  out << "-- LoadOp/StoreOp --\n";
  for (const auto &use : attachmentUses) {
    out << "  " << graph.resource(use.resource).name
        << ": load=" << loadOpName(use.loadOp)
        << " store=" << storeOpName(use.storeOp) << "\n";
  }

  out << "-- Barreiras --\n";
  for (const auto &b : barriers) {
    out << "  " << graph.resource(b.resource).name << ": "
        << graph.pass(b.beforePass).name << " -> " << graph.pass(b.afterPass).name
        << " | " << layoutName(b.oldLayout) << " -> " << layoutName(b.newLayout)
        << " | " << b.srcStage << "/" << b.srcAccess << " -> "
        << b.dstStage << "/" << b.dstAccess << "\n";
  }

  return out.str();
}

} // namespace ae::rendergraph
