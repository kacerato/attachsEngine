#include "core/sha256.h"
#include "editor/editor_session.h"
#include "scene/collision_recipe.h"
#include "scene/component_preset.h"

namespace ae::editor {
namespace {
using Recipe = scene::CollisionRecipe;
Recipe::Part partBase(const resources::ConvexBakePart &part) {
  Recipe::Part out;
  out.low.fill(INFINITY);
  out.high.fill(-INFINITY);
  std::vector<float> triangles;
  for (auto index : part.indices)
    triangles.insert(triangles.end(), part.vertices[index].begin(),
                     part.vertices[index].end());
  out.geometryHash =
      Sha256::hex({reinterpret_cast<const u8 *>(triangles.data()),
                   triangles.size() * sizeof(float)});
  for (const auto &p : part.vertices)
    for (u32 k = 0; k < 3; ++k) {
      out.low[k] = std::min(out.low[k], p[k]);
      out.high[k] = std::max(out.high[k], p[k]);
    }
  return out;
}
const scene::Collider *partCollider(const EditorEntity &object, u64 id) {
  const auto *c = object.components.findInstance(id);
  return c && &c->type() == &scene::Collider::descriptor
             ? static_cast<const scene::Collider *>(c)
             : nullptr;
}
std::string payload(const scene::Collider &c) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<float>::max_digits10);
  c.write(out);
  return out.str();
}
bool locallyChanged(const EditorEntity &object, const Recipe::Part &p) {
  const auto *c = partCollider(object, p.collider);
  if (!c)
    return true;
  auto normalized = *c;
  if (normalized.owner == object.id)
    normalized.owner = 0;
  return payload(normalized) != payload(p.baseline);
}
float correspondenceCost(const Recipe::Part &a, const Recipe::Part &b) {
  float center = 0, extent = 0, scale = 0;
  for (u32 k = 0; k < 3; ++k) {
    const float al = a.high[k] - a.low[k], bl = b.high[k] - b.low[k];
    const float distance = (a.low[k] + a.high[k] - b.low[k] - b.high[k]) * .5f;
    center += distance * distance;
    extent += (al - bl) * (al - bl);
    scale += std::max(al, bl) * std::max(al, bl);
  }
  return (center + extent * .25f) / std::max(scale, 1e-8f);
}
} // namespace
bool EditorSession::beginMotorRegeneration(EditorEntityId object,
                                           std::string &error) {
  const auto *e = document_.find(object);
  const auto *recipe = e ? scene::collisionRecipe(e->components) : nullptr;
  if (!recipe || recipe->parts.empty() || !recipe->valid()) {
    error = "Este objeto não possui receita válida. Uma revisão antiga não "
            "será reinterpretada como uma receita.";
    return false;
  }
  std::vector<MotorBakeSource> sources;
  for (const auto &s : recipe->sources)
    sources.push_back({static_cast<EditorEntityId>(s.object), s.slot});
  const auto settings = recipe->settings;
  if (!selectMotorDecompositionSources(object, sources, error))
    return false;
  return beginMotorDecomposition(object, settings, error);
}
void EditorSession::initializeMotorBakeMapping() {
  const auto *result = motorBake_ ? motorBake_->result() : nullptr;
  const auto *object = document_.find(motorBakeObject_);
  const auto *recipe =
      object ? scene::collisionRecipe(object->components) : nullptr;
  motorBakePartMapping_.assign(result ? result->parts.size() : 0, 0);
  state_.motorBakePreviousParts.clear();
  state_.motorBakeMappingConfirmed = true;
  if (!motorBakeRegenerating_ || !recipe || !result)
    return;
  for (const auto &p : recipe->parts)
    state_.motorBakePreviousParts.push_back(p.collider);
  std::vector<bool> used(recipe->parts.size());
  // Geometry identity wins over index/order. Every candidate is restricted to
  // its source object; spatial assignment is only an explicit review proposal.
  for (usize i = 0; i < result->parts.size(); ++i) {
    const auto next = partBase(result->parts[i]);
    usize chosen = recipe->parts.size(), count = 0;
    for (usize j = 0; j < recipe->parts.size(); ++j)
      if (!used[j] &&
          recipe->sources[recipe->parts[j].source].object ==
              result->parts[i].sourceObject &&
          recipe->parts[j].geometryHash == next.geometryHash) {
        chosen = j;
        ++count;
      }
    if (count == 1) {
      used[chosen] = true;
      motorBakePartMapping_[i] = recipe->parts[chosen].collider;
    }
  }
  for (usize i = 0; i < result->parts.size(); ++i)
    if (!motorBakePartMapping_[i]) {
      const auto next = partBase(result->parts[i]);
      usize chosen = recipe->parts.size();
      float best = INFINITY, second = INFINITY;
      for (usize j = 0; j < recipe->parts.size(); ++j)
        if (!used[j] && recipe->sources[recipe->parts[j].source].object ==
                            result->parts[i].sourceObject) {
          const auto cost = correspondenceCost(recipe->parts[j], next);
          if (cost < best) {
            second = best;
            best = cost;
            chosen = j;
          } else
            second = std::min(second, cost);
        }
      if (best < .35f && second - best > .025f) {
        // Mutual nearest avoids stealing a part from a better candidate in a
        // split.
        float competitor = INFINITY;
        for (usize k = 0; k < result->parts.size(); ++k)
          if (k != i && !motorBakePartMapping_[k] &&
              result->parts[k].sourceObject == result->parts[i].sourceObject)
            competitor = std::min(
                competitor, correspondenceCost(recipe->parts[chosen],
                                               partBase(result->parts[k])));
        if (competitor - best > .025f) {
          used[chosen] = true;
          motorBakePartMapping_[i] = recipe->parts[chosen].collider;
          state_.motorBakeMappingConfirmed = false;
        }
      }
    }
  for (usize i = 0; i < motorBakePartMapping_.size(); ++i)
    if (const auto id = motorBakePartMapping_[i]) {
      const auto *c = partCollider(*object, id);
      state_.motorBakeEnabled[i] = c && c->enabled;
    }
  state_.motorBakePartMapping = motorBakePartMapping_;
}
bool EditorSession::setMotorDecompositionPartMapping(u32 index, u64 collider,
                                                     std::string &error) {
  const auto version = sceneVersion();
  const auto *e = document_.find(motorBakeObject_);
  const auto *r = e ? scene::collisionRecipe(e->components) : nullptr;
  if (!motorBakeRegenerating_ || !state_.motorBakeReady || !r ||
      index >= motorBakePartMapping_.size() || isPlaying() ||
      version.epoch != motorBakeVersion_.epoch ||
      version.revision != motorBakeVersion_.revision ||
      files_.rootPath() != motorBakeRoot_ ||
      state_.selection != motorBakeObject_) {
    error = "Correspondência desatualizada ou indisponível";
    return false;
  }
  if (collider &&
      std::none_of(r->parts.begin(), r->parts.end(),
                   [&](const auto &p) { return p.collider == collider; })) {
    error = "Colisor não pertence à base desta receita";
    return false;
  }
  for (usize i = 0; i < motorBakePartMapping_.size(); ++i)
    if (i != index && collider && motorBakePartMapping_[i] == collider) {
      error = "Esta parte anterior já está associada; libere sua "
              "correspondência primeiro";
      return false;
    }
  motorBakePartMapping_[index] = collider;
  state_.motorBakePartMapping = motorBakePartMapping_;
  state_.motorBakeMappingConfirmed = false;
  const auto *c = partCollider(*e, collider);
  state_.motorBakeEnabled[index] = !collider || (c && c->enabled);
  refreshMotorBakeCandidate();
  if (&error != &state_.motorSetupError)
    error.clear();
  return true;
}
bool EditorSession::confirmMotorDecompositionMapping(std::string &error) {
  const auto v = sceneVersion();
  if (!motorBakeRegenerating_ || !state_.motorBakeReady || isPlaying() ||
      v.epoch != motorBakeVersion_.epoch ||
      v.revision != motorBakeVersion_.revision ||
      files_.rootPath() != motorBakeRoot_ ||
      state_.selection != motorBakeObject_) {
    error = "Prévia desatualizada; gere novamente";
    return false;
  }
  state_.motorBakeMappingConfirmed = true;
  refreshMotorBakeCandidate();
  error = state_.motorSetupError;
  return error.empty();
}
bool EditorSession::buildMotorBakeCandidate(
    std::span<const resources::AssetGuid> meshes, resources::AssetGuid source,
    EditorEntity &candidate, std::string &error) const {
  const auto fail = [&](std::string s) {
    error = std::move(s);
    return false;
  };
  const auto *result = motorBake_ ? motorBake_->result() : nullptr;
  const auto *original = document_.find(motorBakeObject_);
  if (!result || !original || meshes.size() != result->parts.size() ||
      state_.motorBakeEnabled.size() != meshes.size())
    return fail("Revisão de partes inválida");
  const auto *previous = scene::collisionRecipe(original->components);
  scene::CollisionRecipe recipe;
  recipe.settings = motorBakeSettings_;
  recipe.bakeSource = source;
  recipe.geometryHash = motorBakeSourceHash_;
  for (const auto &s : motorBakeSources_)
    recipe.sources.push_back({s.object, s.slot});
  u64 first = 0;
  scene::Collider seed;
  if (motorBakeRegenerating_) {
    if (!previous || !previous->valid() ||
        motorBakePartMapping_.size() != meshes.size())
      return fail("Base da receita indisponível");
    candidate = *original;
    for (const auto &p : previous->parts) {
      if (std::find(motorBakePartMapping_.begin(), motorBakePartMapping_.end(),
                    p.collider) == motorBakePartMapping_.end()) {
        if (locallyChanged(*original, p))
          return fail("Edição/remoção do Colisor #" +
                      std::to_string(p.collider) +
                      " sem correspondência. Associe a uma parte; nenhuma "
                      "edição será descartada.");
        // Do not invalidate a typed script component reference on regeneration.
        std::vector<EditorEntityId> ids;
        document_.collectSubtree(document_.root(), ids);
        for (auto id : ids)
          for (usize k = 0; k < document_.find(id)->components.size(); ++k)
            if (const auto *script =
                    scene::scriptBehavior(document_.find(id)->components.at(k)))
              for (const auto &property : script->properties) {
                bool referenced = false;
                const auto check = [&](std::string_view type,
                                       std::string_view text) {
                  u64 owner = 0, instance = 0;
                  if (!scene::scriptComponentTypeId(type).empty() &&
                      scene::parseScriptComponentValue(text, owner, instance) &&
                      owner == original->id && instance == p.collider)
                    referenced = true;
                };
                check(property.valueType, property.value);
                const auto element =
                    scene::scriptArrayElementType(property.valueType);
                std::vector<std::string> items;
                if (!element.empty() &&
                    scene::parseScriptArray(property.value, items))
                  for (const auto &item : items)
                    check(element, item);
                if (referenced)
                  return fail("Colisor #" + std::to_string(p.collider) +
                              " é referenciado por script; escolha sua "
                              "correspondência");
              }
        candidate.components.removeInstance(p.collider);
      }
    }
  } else {
    std::string summary;
    if (!prepareDynamicMotor(motorBakeObject_, MotorCollisionPolicy::Decompose,
                             candidate, summary, error))
      return false;
    const auto *c = runtime::colliderComponent(candidate);
    if (!c)
      return fail("Colisor inicial ausente");
    seed = *c;
    first = c->instanceId();
  }
  for (usize i = 0; i < meshes.size(); ++i) {
    const u64 mapped = motorBakeRegenerating_ ? motorBakePartMapping_[i]
                       : i                    ? 0
                                              : first;
    const Recipe::Part *old = nullptr;
    if (motorBakeRegenerating_ && mapped)
      for (const auto &p : previous->parts)
        if (p.collider == mapped)
          old = &p;
    if (motorBakeRegenerating_ && mapped && !old)
      return fail("Correspondência não pertence à receita");
    auto baseline = old                      ? old->baseline
                    : motorBakeRegenerating_ ? scene::Collider{}
                                             : seed;
    baseline.owner = 0;
    baseline.shape = scene::ColliderShape::Mesh;
    baseline.convex = true;
    baseline.collisionMesh = meshes[i];
    baseline.meshLocalPose = true;
    baseline.centerX = baseline.centerY = baseline.centerZ =
        baseline.rotationX = baseline.rotationY = baseline.rotationZ = 0;
    baseline.enabled = motorBakeRegenerating_ && mapped
                           ? old->baseline.enabled
                           : state_.motorBakeEnabled[i];
    auto base = partBase(result->parts[i]);
    base.baseline = baseline;
    const auto origin = std::find_if(
        recipe.sources.begin(), recipe.sources.end(), [&](const auto &s) {
          return s.object == result->parts[i].sourceObject;
        });
    if (origin == recipe.sources.end())
      return fail("Parte sem fonte tipada");
    base.source = static_cast<u32>(origin - recipe.sources.begin());
    const auto *current = motorBakeRegenerating_ && mapped
                              ? partCollider(*original, mapped)
                              : nullptr;
    if (motorBakeRegenerating_ && mapped && !current) {
      base.collider = mapped;
      recipe.parts.push_back(std::move(base));
      continue;
    }
    const auto *slot =
        mapped ? candidate.components.findInstance(mapped)
               : candidate.components.add(scene::Collider::descriptor);
    if (!slot || &slot->type() != &scene::Collider::descriptor)
      return fail("Sem slots para as partes; nenhuma mudança foi publicada");
    const auto uid = slot->instanceId();
    if (!candidate.components.replaceInstance(uid, baseline))
      return fail("Base de colisor inválida");
    if (current) {
      const auto fields =
          scene::changedFields(scene::componentDelta(*current, old->baseline));
      if (!fields.empty()) {
        const auto applied = scene::applyComponentFields(candidate.components,
                                                         *current, fields, uid);
        if (!applied.ok() || applied.rejected)
          return fail("Override incompatível com a nova parte: Colisor #" +
                      std::to_string(uid));
      }
    }
    auto *merged =
        static_cast<scene::Collider *>(candidate.components.editInstance(uid));
    merged->enabled = state_.motorBakeEnabled[i];
    base.collider = uid;
    recipe.parts.push_back(std::move(base));
  }
  auto *target = static_cast<scene::CollisionRecipe *>(
      candidate.components.edit(scene::CollisionRecipe::descriptor));
  if (!target)
    target = static_cast<scene::CollisionRecipe *>(
        candidate.components.add(scene::CollisionRecipe::descriptor));
  if (!target || !recipe.valid() ||
      !candidate.components.replaceInstance(target->instanceId(), recipe))
    return fail("Receita inválida ou sem slot disponível");
  error.clear();
  return true;
}
void EditorSession::refreshMotorBakeCandidate() {
  const auto *result = motorBake_ ? motorBake_->result() : nullptr;
  const auto *e = document_.find(motorBakeObject_);
  if (!result || !e)
    return;
  // Staging computes identities without publishing a resource or GPU package.
  resources::GltfImport model;
  ModelImportReport report;
  StagedSource staged;
  auto sources = importedSources_;
  auto assets = assets_;
  const auto hash = Sha256::hex(result->glb);
  const auto path = "Collision/bake-" + hash + ".glb";
  if (!resources::importGlb(result->glb, importLimits_, {}, model) ||
      !stageSource(model, hash, path, resources::ImportAmbiguityPolicy::Refuse,
                   sources, assets, report, staged, {})) {
    state_.motorSetupError = "Não foi possível preparar a revisão candidata";
    return;
  }
  const auto source =
      std::find_if(sources.begin(), sources.end(),
                   [&](const auto &s) { return s.guid == report.source; });
  if (source == sources.end()) {
    state_.motorSetupError = "Identidade da revisão candidata ausente";
    return;
  }
  EditorEntity candidate;
  std::string error;
  const bool ready = buildMotorBakeCandidate(source->identities, report.source,
                                             candidate, error);
  state_.motorBakePartNotes.assign(result->parts.size(), {});
  const auto *recipe = scene::collisionRecipe(e->components);
  u32 changed = 0, removed = 0;
  for (usize i = 0; i < result->parts.size(); ++i) {
    auto &preview = state_.motorBakePreview[i];
    preview.triangles.clear();
    const auto uid = motorBakeRegenerating_ ? motorBakePartMapping_[i] : 0;
    const auto *c =
        ready ? partCollider(candidate,
                             uid ? uid
                                 : scene::collisionRecipe(candidate.components)
                                       ->parts[i]
                                       .collider)
              : nullptr;
    if (uid && !partCollider(*e, uid)) {
      ++removed;
      state_.motorBakePartNotes[i] = "Remoção local preservada";
      continue;
    }
    bool edited = false;
    if (uid && recipe)
      for (const auto &p : recipe->parts)
        if (p.collider == uid)
          edited = locallyChanged(*e, p);
    if (edited) {
      ++changed;
      state_.motorBakePartNotes[i] = "Edição local preservada";
    }
    std::vector<EditorPickMesh::Triangle> triangles;
    if (c && c->shape == scene::ColliderShape::Mesh &&
        c->collisionMesh != source->identities[i]) {
      // Override of the physical mesh: show its actual existing resource.
      const u32 slot = mapScene_.assetSlot(c->collisionMesh);
      std::span<const EditorPickMesh::Triangle> raw;
      float matrix[16];
      if (c->convex) {
        EditorMapScene::CollisionHullPreview hull;
        if (!slot ||
            !mapScene_.collisionHullPreview({&slot, 1}, c->hullTolerance, hull))
          error = "Casco da edição local indisponível";
        else
          triangles.assign(hull.triangles.begin(), hull.triangles.end());
      } else if (!slot || !mapScene_.localGeometry(slot, raw, matrix)) {
        error = "Malha da edição local indisponível";
      } else
        for (const auto &t : raw) {
          EditorPickMesh::Triangle transformed;
          for (u32 v = 0; v < 3; ++v)
            for (u32 k = 0; k < 3; ++k)
              transformed[v * 3 + k] = matrix[12 + k] + matrix[k] * t[v * 3] +
                                       matrix[4 + k] * t[v * 3 + 1] +
                                       matrix[8 + k] * t[v * 3 + 2];
          triangles.push_back(transformed);
        }
    } else if (!c || c->shape == scene::ColliderShape::Mesh) {
      for (usize n = 0; n < result->parts[i].indices.size(); n += 3) {
        EditorPickMesh::Triangle t;
        for (u32 v = 0; v < 3; ++v)
          std::copy_n(
              result->parts[i].vertices[result->parts[i].indices[n + v]].data(),
              3, t.data() + v * 3);
        triangles.push_back(t);
      }
    }
    if (c && c->shape != scene::ColliderShape::Mesh) {
      state_.motorBakePartNotes[i] = "Forma local preservada · contorno atual";
      // The current component visual remains visible; do not draw a fake hull.
      continue;
    }
    EditorTransform pose;
    if (c && c->meshLocalPose) {
      pose.position[0] = c->centerX;
      pose.position[1] = c->centerY;
      pose.position[2] = c->centerZ;
      pose.rotationDegrees[0] = c->rotationX;
      pose.rotationDegrees[1] = c->rotationY;
      pose.rotationDegrees[2] = c->rotationZ;
    }
    float matrix[16];
    runtime::transformMatrix(pose, matrix);
    for (const auto &t : triangles) {
      EditorPickMesh::Triangle transformed;
      for (u32 v = 0; v < 3; ++v)
        for (u32 k = 0; k < 3; ++k)
          transformed[v * 3 + k] = matrix[12 + k] + matrix[k] * t[v * 3] +
                                   matrix[4 + k] * t[v * 3 + 1] +
                                   matrix[8 + k] * t[v * 3 + 2];
      preview.triangles.push_back(transformed);
    }
    const usize budget = 3200 / std::max<usize>(1, result->parts.size());
    if (preview.triangles.size() > budget) {
      std::vector<EditorPickMesh::Triangle> sampled;
      sampled.reserve(budget);
      for (usize k = 0; k < budget; ++k)
        sampled.push_back(
            preview.triangles[k * preview.triangles.size() / budget]);
      preview.triangles = std::move(sampled);
      state_.motorBakePartNotes[i] += " · contorno amostrado";
    }
  }
  state_.motorBakeMappingSummary = std::to_string(changed) + " edições · " +
                                   std::to_string(removed) +
                                   " remoções preservadas";
  state_.motorSetupError = std::move(error);
  if (motorBakeRegenerating_ && !state_.motorBakeMappingConfirmed &&
      state_.motorSetupError.empty())
    state_.motorSetupError =
        "Revise e confirme a correspondência sugerida entre as partes";
}
} // namespace ae::editor
