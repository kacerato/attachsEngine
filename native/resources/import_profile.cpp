#include "resources/import_profile.h"
#include "resources/json_reader.h"
#include "resources/texture_compression.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ae::resources {
namespace {
bool isScaleStep(float scale) noexcept {
  return std::any_of(ImportScaleSteps.begin(), ImportScaleSteps.end(),
                     [scale](float step) { return std::fabs(step - scale) <= step * 1e-5f; });
}
bool isTextureStep(u32 dimension) noexcept {
  return std::find(ImportTextureDimensionSteps.begin(), ImportTextureDimensionSteps.end(), dimension) !=
         ImportTextureDimensionSteps.end();
}
} // namespace

namespace {
// A ordem em que o autor excluiu não é escolha: dois perfis que excluem os
// mesmos nós são o mesmo perfil.
bool sameExcludedNodes(const ImportProfile &a, const ImportProfile &b) noexcept {
  if (a.excludedNodes.size() != b.excludedNodes.size()) return false;
  for (const auto &node : a.excludedNodes) if (!b.excludes(node)) return false;
  return true;
}
bool sameCollisionMeshes(const ImportProfile &a, const ImportProfile &b) noexcept {
  if (a.collisionMeshes.size() != b.collisionMeshes.size()) return false;
  for (const auto &recipe : a.collisionMeshes) {
    const auto found=std::find_if(b.collisionMeshes.begin(), b.collisionMeshes.end(), [&](const auto &candidate) {
      return candidate.source==recipe.source && candidate.trianglePercent==recipe.trianglePercent &&
             candidate.maximumError==recipe.maximumError && collisionMeshGuid(candidate)==collisionMeshGuid(recipe);
    });
    if(found==b.collisionMeshes.end()) return false;
  }
  return true;
}
} // namespace

bool sameImportProfile(const ImportProfile &a, const ImportProfile &b) noexcept {
  return std::fabs(a.scale - b.scale) <= std::max(a.scale, b.scale) * 1e-5f &&
         a.maximumTextureDimension == b.maximumTextureDimension && a.normals == b.normals &&
         a.normalWeighting == b.normalWeighting && a.smoothingAngle == b.smoothingAngle && a.tangents == b.tangents &&
         a.importCameras == b.importCameras && a.importLights == b.importLights &&
         a.textureCompression == b.textureCompression && a.textureStreaming == b.textureStreaming &&
         a.textureStreamingPriority == b.textureStreamingPriority &&
         sameExcludedNodes(a, b) && sameCollisionMeshes(a, b);
}

bool sameImportPreparation(const ImportProfile &a, const ImportProfile &b) noexcept {
  auto left = a, right = b;
  left.excludedNodes.clear();
  right.excludedNodes.clear();
  left.collisionMeshes.clear();
  right.collisionMeshes.clear();
  right.textureStreaming = left.textureStreaming;
  right.textureStreamingPriority = left.textureStreamingPriority;
  return sameImportProfile(left, right);
}

bool validImportProfile(const ImportProfile &profile) noexcept {
  bool collisionRecipesValid=true;
  for(usize i=0;i<profile.collisionMeshes.size() && collisionRecipesValid;++i) {
    const auto &recipe=profile.collisionMeshes[i];
    collisionRecipesValid=validCollisionMeshRecipe(recipe);
    for(usize earlier=0;earlier<i && collisionRecipesValid;++earlier)
      collisionRecipesValid=profile.collisionMeshes[earlier].source!=recipe.source &&
          collisionMeshGuid(profile.collisionMeshes[earlier])!=collisionMeshGuid(recipe);
  }
  return std::isfinite(profile.scale) && isScaleStep(profile.scale) && isTextureStep(profile.maximumTextureDimension) &&
         profile.normals <= GltfNormalsCalculate && profile.normalWeighting <= GltfNormalWeightAngle &&
         profile.smoothingAngle <= 180 &&
         profile.tangents <= GltfTangentsCalculate && validTextureCompression(profile.textureCompression) &&
         profile.textureStreamingPriority >= -128 && profile.textureStreamingPriority <= 127 &&
         profile.excludedNodes.size() <= 65536 &&
         profile.collisionMeshes.size() <= 65536 && collisionRecipesValid;
}

std::string serializeImportProfile(const ImportProfile &profile) {
  char scale[32];
  std::snprintf(scale, sizeof scale, "%.9g", static_cast<double>(profile.scale));
  return "{\"schema\":" + std::to_string(ImportProfileSchema) + ",\"scale\":" + scale +
         ",\"maximumTextureDimension\":" + std::to_string(profile.maximumTextureDimension) +
         ",\"normals\":" + std::to_string(profile.normals) +
         ",\"normalWeighting\":" + std::to_string(profile.normalWeighting) +
         ",\"smoothingAngle\":" + std::to_string(profile.smoothingAngle) +
         ",\"tangents\":" + std::to_string(profile.tangents) +
         ",\"importCameras\":" + (profile.importCameras ? "true" : "false") +
         ",\"importLights\":" + (profile.importLights ? "true" : "false") +
         ",\"textureCompression\":" + std::to_string(profile.textureCompression) +
         ",\"textureStreaming\":" + (profile.textureStreaming ? "true" : "false") +
         ",\"textureStreamingPriority\":" + std::to_string(profile.textureStreamingPriority) + ",\"excludedNodes\":[" + [&] {
           std::string list;
           for (const auto &node : profile.excludedNodes) {
             if (!list.empty()) list += ',';
             list += '"' + node.text() + '"';
           }
           return list;
         }() + "],\"collisionMeshes\":[" + [&] {
           std::string list;
           for (const auto &recipe : profile.collisionMeshes) {
             if (!list.empty()) list += ',';
             char error[32]; std::snprintf(error, sizeof error, "%.9g", static_cast<double>(recipe.maximumError));
             list += "{\"source\":\"" + recipe.source.text() + "\",\"identity\":\"" +
                     collisionMeshGuid(recipe).text() + "\",\"trianglePercent\":" +
                     std::to_string(recipe.trianglePercent) + ",\"maximumError\":" + error + "}";
           }
           return list;
         }() + "]}\n";
}

bool parseImportProfile(std::string_view text, ImportProfile &out) {
  out = {};
  JsonDocument document;
  if (!JsonDocument::parse(text, document) || !document.root() || document.root()->kind != JsonDocument::Kind::Object)
    return false;
  const auto &root = *document.root();
  // Schemas ANTERIORES são lidos: um perfil salvo antes de um campo existir
  // continua valendo, com o campo novo no padrão que reproduz o comportamento
  // de então. Recusá-lo faria o arquivo cair no próximo nível em silêncio — a
  // escala ×100 que o autor escolheu voltaria a ×1 na reimportação seguinte.
  // Só schema NOVO demais é recusado: ele pode carregar escolha que este
  // leitor não sabe honrar.
  const auto schema = document.index(root, "schema");
  if (schema < 1 || schema > static_cast<i64>(ImportProfileSchema)) return false;
  const auto *scale = document.member(root, "scale");
  const auto dimension = document.index(root, "maximumTextureDimension");
  if (!scale || scale->kind != JsonDocument::Kind::Number || dimension < 0) return false;
  ImportProfile parsed;
  parsed.scale = static_cast<float>(scale->number);
  parsed.maximumTextureDimension = static_cast<u32>(std::min<i64>(dimension, 1 << 20));
  // Dentro do schema DECLARADO, campo ausente é recusa, não padrão silencioso:
  // o schema diz quais campos o arquivo tem, e a falta de um deles significa
  // arquivo truncado ou escrito por outra coisa. O que o schema declarado não
  // conhecia fica no padrão da struct, que é o comportamento daquela época.
  if (schema >= 2) {
    const auto normals = document.index(root, "normals");
    const auto weighting = document.index(root, "normalWeighting");
    const auto tangents = document.index(root, "tangents");
    if (normals < 0 || weighting < 0 || tangents < 0) return false;
    parsed.normals = static_cast<u8>(std::min<i64>(normals, 255));
    parsed.normalWeighting = static_cast<u8>(std::min<i64>(weighting, 255));
    parsed.tangents = static_cast<u8>(std::min<i64>(tangents, 255));
  }
  parsed.smoothingAngle = 180;
  if (schema >= 5) {
    const auto angle = document.index(root, "smoothingAngle");
    if (angle < 0) return false;
    parsed.smoothingAngle = static_cast<u32>(std::min<i64>(angle, 1000));
  }
  if (schema >= 3) {
    const auto *cameras = document.member(root, "importCameras");
    if (!cameras || cameras->kind != JsonDocument::Kind::Boolean) return false;
    parsed.importCameras = cameras->boolean;
  }
  if (schema >= 8) {
    const auto *lights = document.member(root, "importLights");
    if (!lights || lights->kind != JsonDocument::Kind::Boolean) return false;
    parsed.importLights = lights->boolean;
  }
  if (schema >= 9) {
    const auto compression = document.index(root, "textureCompression");
    if (compression < 0) return false;
    parsed.textureCompression = static_cast<u8>(std::min<i64>(compression, 255));
  }
  if (schema >= 10) {
    const auto *streaming = document.member(root, "textureStreaming");
    const double priority = document.number(root, "textureStreamingPriority", 1e9);
    if (!streaming || streaming->kind != JsonDocument::Kind::Boolean || std::floor(priority) != priority ||
        priority < -128 || priority > 127) return false;
    parsed.textureStreaming = streaming->boolean;
    parsed.textureStreamingPriority = static_cast<i32>(priority);
  }
  if (schema >= 4) {
    const auto *excluded = document.member(root, "excludedNodes");
    if (!excluded || excluded->kind != JsonDocument::Kind::Array || excluded->childCount > 65536) return false;
    for (u32 i = 0; i < excluded->childCount; ++i) {
      const auto *item = document.child(*excluded, i);
      AssetGuid node;
      // Identidade ilegível não vira "nenhum nó": o arquivo inteiro é recusado,
      // porque aceitar o resto traria de volta, em silêncio, o nó excluído.
      if (!item || item->kind != JsonDocument::Kind::String || !AssetGuid::parse(document.textOf(*item), node) ||
          !node.valid())
        return false;
      if (!parsed.excludes(node)) parsed.excludedNodes.push_back(node);
    }
  }
  if (schema >= 6) {
    const auto *recipes = document.member(root, "collisionMeshes");
    if (!recipes || recipes->kind != JsonDocument::Kind::Array || recipes->childCount > 65536) return false;
    for (u32 i = 0; i < recipes->childCount; ++i) {
      const auto *item = document.child(*recipes, i);
      if (!item || item->kind != JsonDocument::Kind::Object) return false;
      const auto *source = document.member(*item, "source");
      const auto *identity = document.member(*item, "identity");
      const auto percent = document.index(*item, "trianglePercent");
      const auto *error = document.member(*item, "maximumError");
      CollisionMeshRecipe recipe;
      if (!source || source->kind != JsonDocument::Kind::String ||
          !AssetGuid::parse(document.textOf(*source), recipe.source) || percent < 0 || percent > 255 ||
          !error || error->kind != JsonDocument::Kind::Number) return false;
      recipe.trianglePercent = static_cast<u8>(percent);
      recipe.maximumError = static_cast<float>(error->number);
      if(schema>=7) {
        if(!identity || identity->kind!=JsonDocument::Kind::String ||
           !AssetGuid::parse(document.textOf(*identity),recipe.identity) || !recipe.identity.valid()) return false;
      } else {
        const auto rounded=static_cast<u32>(std::lround(recipe.maximumError*1000000.0f));
        recipe.identity=assetGuidFromSeed("collision-mesh:v1:"+recipe.source.text()+":"+
                                         std::to_string(recipe.trianglePercent)+":"+std::to_string(rounded));
      }
      if (!validCollisionMeshRecipe(recipe) ||
          std::any_of(parsed.collisionMeshes.begin(), parsed.collisionMeshes.end(),
                      [&](const auto &known) { return known.source == recipe.source; })) return false;
      parsed.collisionMeshes.push_back(recipe);
    }
  }
  if (!validImportProfile(parsed)) return false;
  out = parsed;
  return true;
}

GltfImportLimits applyImportProfile(GltfImportLimits limits, const ImportProfile &profile) {
  if (!validImportProfile(profile)) return limits;
  limits.rootScale = profile.scale;
  limits.normals = profile.normals;
  limits.normalWeighting = profile.normalWeighting;
  limits.smoothingAngle = static_cast<float>(profile.smoothingAngle);
  limits.tangents = profile.tangents;
  limits.importCameras = profile.importCameras;
  limits.importLights = profile.importLights;
  // O perfil pede; o aparelho decide se amostra ASTC (mesma checagem do KTX2).
  limits.textureCompression = limits.astc4x4 ? profile.textureCompression : 0;
  limits.maximumTextureDimension = std::min(limits.maximumTextureDimension, profile.maximumTextureDimension);
  limits.minimumTextureDimension = std::min(limits.minimumTextureDimension, limits.maximumTextureDimension);
  return limits;
}

std::string importProfilePath(const AssetGuid &source) { return ".astra/imports/" + source.text() + ".profile"; }

u32 nearestImportScaleStep(float scale) noexcept {
  u32 best = 4;
  float distance = INFINITY;
  for (u32 i = 0; i < ImportScaleSteps.size(); ++i) {
    // Distância em escala logarítmica: 0,5 está mais perto de 1 que de 0,1.
    const float d = std::isfinite(scale) && scale > 0 ? std::fabs(std::log(scale / ImportScaleSteps[i])) : INFINITY;
    if (d < distance) {
      distance = d;
      best = i;
    }
  }
  return best;
}
} // namespace ae::resources
