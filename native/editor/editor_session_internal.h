#pragma once
// Shared implementation details for the session and its input action dispatcher.
#include "editor/editor_document.h"
#include "resources/asset_registry.h"
#include "resources/material_asset.h"
#include "resources/texture_profile.h"
#include "ui/ui_icon_id.h"
#include <filesystem>

namespace ae::editor::session_detail {
inline constexpr u32 TextureProfileFields=11;
struct FileKind {const char *label,*plural;ui::UiIcon icon;};
inline constexpr FileKind fileKinds[]{
  {"textura","Texturas",ui::UiIcon::AssetsTexture},{"material","Materiais",ui::UiIcon::AssetsMaterial},
  {"modelo","Modelos",ui::UiIcon::AssetsFileMesh},{"mapa HDRI","Mapas HDRI",ui::UiIcon::LightingSky},
  {"perfil","Perfis de ambiente",ui::UiIcon::LightingSceneEffects},{"script","Scripts",ui::UiIcon::ScriptingCode},
  {"cena","Cenas",ui::UiIcon::SceneObject},{"arquivo","Outros arquivos",ui::UiIcon::AssetsFile}};

std::string baseName(const std::string &path);
// Registro em disco atrás da memória (pendente até salvar) é aceito; divergente, nunca.
bool persistedRegistry(const std::filesystem::path &root,const resources::AssetRegistry &expected,bool pending=false);
bool textureProfileToggle(u32 field);
std::string textureProfileFieldText(const resources::TextureProfile &profile,u32 field);
void copyTextureProfileField(resources::TextureProfile &to,const resources::TextureProfile &from,u32 field);
void setTextureProfileToggle(resources::TextureProfile &profile,u32 field,bool on);
void materialDifferences(const resources::MaterialAsset &a,const resources::MaterialAsset &b,std::vector<std::string> &out);
bool copyMaterialChange(const resources::MaterialAsset &before,const resources::MaterialAsset &after,
                        resources::MaterialAsset &target,std::string_view field);
std::string materialWidgetField(u32 widget,u32 binding);
std::string materialFieldText(const resources::MaterialAsset &material,std::string_view key);
std::string componentKind(const scene::ComponentValue &value);
u32 componentOccurrence(const scene::Components &components,const scene::ComponentValue &value);
const scene::ComponentValue *matchComponent(const scene::Components &components,std::string_view kind,u32 occurrence);
bool multiEditable(const scene::ComponentValue &value);
std::string transformChannel(const EditorTransform &t,u32 row,u32 axis);
std::string propertyText(const scene::ComponentValue &value,std::string_view property);
}
