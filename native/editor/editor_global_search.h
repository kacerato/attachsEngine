#pragma once
// Busca global do editor (Unity 6000.0 Manual/search-overview): uma consulta,
// vários provedores. Aqui: objetos da cena (h:), arquivos do projeto (p:) e
// receitas de criação (m:, o equivalente do provedor de menus para "Create").
// A consulta aceita "t:<tipo>" para objetos com um componente cujo nome, id ou
// script contém o texto, como no seletor avançado. Função pura: a sessão monta
// o índice de arquivos ao abrir a busca e aplica o resultado escolhido.
#include "editor/editor_creation_catalog.h"
#include "editor/editor_filesystem.h"
#include "editor/editor_reference_picker.h"

#include <algorithm>
#include <span>
#include <string>
#include <vector>

namespace ae::editor {

// EditorSearchProvider, EditorSearchResult e EditorSearchCounts ficam em
// editor_screen.h: a tela guarda os resultados.

inline ui::UiIcon editorFileIcon(const EditorFileEntry &file) {
  const auto &n = file.name;
  if (file.directory) return ui::UiIcon::AssetsFolder;
  if (n.ends_with(".cs")) return ui::UiIcon::IdeCode;
  if (n.ends_with(".glb") || n.ends_with(".gltf") || n.ends_with(".obj") || n.ends_with(".fbx")) return ui::UiIcon::AssetsFileMesh;
  if (n.ends_with(".png") || n.ends_with(".jpg") || n.ends_with(".jpeg") || n.ends_with(".ktx2") || n.ends_with(".hdr") ||
      n.ends_with(".exr")) return ui::UiIcon::AssetsTexture;
  if (n.ends_with(".aemat")) return ui::UiIcon::AssetsMaterial;
  if (n.ends_with(".aescene")) return ui::UiIcon::SceneObject;
  return ui::UiIcon::AssetsFile;
}

// Separa provedor, filtros de tipo e palavras. Um prefixo de provedor ("h:",
// "p:", "m:") vale sobre o chip escolhido, como na Unity.
struct EditorSearchQuery {
  EditorSearchProvider provider = EditorSearchProvider::All;
  std::vector<std::string> types, words;
};
inline EditorSearchQuery parseEditorSearch(std::string_view query, EditorSearchProvider chosen) {
  EditorSearchQuery parsed;
  parsed.provider = chosen;
  std::string token;
  const auto flush = [&] {
    if (token.size() >= 2 && token[1] == ':') {
      const char p = static_cast<char>(token[0] | 0x20);
      const auto rest = token.substr(2);
      if (p == 't') { if (!rest.empty()) parsed.types.push_back(editorSearchKey(rest)); token.clear(); return; }
      if (p == 'h' || p == 'p' || p == 'm') {
        parsed.provider = p == 'h' ? EditorSearchProvider::Scene : p == 'p' ? EditorSearchProvider::Project : EditorSearchProvider::Create;
        token = rest;
      }
    }
    if (!token.empty()) parsed.words.push_back(editorSearchKey(token));
    token.clear();
  };
  for (const char c : query) { if (c == ' ') flush(); else token += c; }
  flush();
  return parsed;
}

inline bool editorSearchWords(const std::string &key, const std::vector<std::string> &words, u8 &rank) {
  rank = 0;
  for (usize i = 0; i < words.size(); ++i) {
    const auto at = key.find(words[i]);
    if (at == std::string::npos) return false;
    if (i == 0) rank = at == 0 ? 0 : (key[at - 1] == ' ' || key[at - 1] == '_' || key[at - 1] == '/') ? 1 : 2;
  }
  return true;
}

// `creationAllowed(i)` diz se a receita i pode ser criada agora (a mesma trava
// do menu Criar). Resultados em ordem de relevância e depois de provedor; no
// máximo `limit`. `counts` recebe quantos casaram em cada provedor, antes do
// corte, para os chips.
template <class Allowed>
std::vector<EditorSearchResult> editorGlobalSearch(const runtime::SceneGraph &document, std::span<const EditorFileEntry> files,
                                                   std::string_view text, EditorSearchProvider chosen, Allowed creationAllowed,
                                                   EditorSearchCounts &counts, usize limit = 300) {
  counts = {};
  std::vector<EditorSearchResult> results;
  const auto query = parseEditorSearch(text, chosen);
  const bool empty = query.words.empty() && query.types.empty();
  // Sem texto e sem provedor escolhido não há o que listar (a Unity mostra só
  // as buscas recentes); com um provedor escolhido, lista o provedor inteiro.
  if (empty && query.provider == EditorSearchProvider::All) return results;
  const auto wants = [&](EditorSearchProvider p) { return query.provider == EditorSearchProvider::All || query.provider == p; };
  if (wants(EditorSearchProvider::Scene)) {
    std::vector<EditorEntityId> ids;
    document.collectSubtree(document.root(), ids);
    for (const auto id : ids) {
      const auto *entity = document.find(id);
      if (!entity || id == document.root()) continue;
      bool match = true;
      for (const auto &type : query.types) match = match && editorEntityHasComponent(*entity, type);
      u8 rank = 0;
      if (!match || !editorSearchWords(editorSearchKey(entity->name), query.words, rank)) continue;
      std::string path;
      for (const auto *up = document.find(entity->parent); up && up->id != document.root(); up = document.find(up->parent))
        path = path.empty() ? std::string(up->name) : std::string(up->name) + " / " + path;
      const usize components = entity->components.size();
      ++counts.scene;
      results.push_back({EditorSearchProvider::Scene, id, entity->name,
                         (path.empty() ? std::string("Cena") : "Cena / " + path) + " · " + std::to_string(components) +
                             (components == 1 ? " componente" : " componentes"),
                         ui::UiIcon::None, rank});
    }
  }
  // Tipos só filtram objetos: com "t:" os outros provedores não casam.
  if (query.types.empty() && wants(EditorSearchProvider::Project)) {
    for (usize i = 0; i < files.size(); ++i) {
      u8 rank = 0;
      if (!editorSearchWords(editorSearchKey(files[i].name), query.words, rank)) continue;
      const auto slash = files[i].relativePath.rfind('/');
      ++counts.project;
      results.push_back({EditorSearchProvider::Project, i, files[i].name,
                         "Projeto / " + (slash == std::string::npos ? std::string() : files[i].relativePath.substr(0, slash)),
                         editorFileIcon(files[i]), rank});
    }
  }
  if (query.types.empty() && wants(EditorSearchProvider::Create)) {
    for (u32 i = 0; i < editorCreationCatalog.size(); ++i) {
      const auto &entry = editorCreationCatalog[i];
      if (!creationAllowed(i)) continue;
      u8 rank = 0;
      if (!editorSearchWords(editorSearchKey(entry.name), query.words, rank)) {
        // Termos de outras engines ("Cube", "Rigidbody") contam, com relevância menor.
        if (!editorSearchWords(editorSearchKey(entry.searchTerms), query.words, rank)) continue;
        rank = 2;
      }
      ++counts.create;
      results.push_back({EditorSearchProvider::Create, i, entry.name, std::string("Criar · ") + entry.description, entry.icon, rank});
    }
  }
  std::stable_sort(results.begin(), results.end(), [](const auto &a, const auto &b) { return a.rank < b.rank; });
  if (results.size() > limit) results.resize(limit);
  return results;
}

} // namespace ae::editor
