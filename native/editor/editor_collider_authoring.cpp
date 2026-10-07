#include "core/sha256.h"
#include "editor/editor_session.h"
#include <iomanip>
#include <sstream>
namespace ae::editor {
namespace {
std::string topologyHash(std::span<const EditorPickMesh::Triangle> triangles) {
  return Sha256::hex({reinterpret_cast<const u8 *>(triangles.data()),
                      triangles.size() * sizeof(EditorPickMesh::Triangle)});
}
} // namespace
bool EditorSession::collectColliderTopology(
    EditorEntityId object, u64 instance,
    std::vector<EditorPickMesh::Triangle> &out, std::string &error) const {
  out.clear();
  const auto *c = inspectedCollider(document_, object, instance);
  const auto *e = document_.find(object);
  if (!c || !c->valid()) {
    error = "Colisor inválido ou removido";
    return false;
  }
  if (c->shape == scene::ColliderShape::Mesh) {
    const auto slots = visual_detail::meshColliderSlots(*c, *e, mapScene_);
    if (slots.empty()) {
      error = "Recurso físico indisponível";
      return false;
    }
    if (c->convex) {
      EditorMapScene::CollisionHullPreview p;
      if (!mapScene_.collisionHullPreview(slots, c->hullTolerance, p)) {
        error = std::string(p.diagnostic);
        return false;
      }
      out.assign(p.triangles.begin(), p.triangles.end());
    } else
      for (auto slot : slots) {
        std::span<const EditorPickMesh::Triangle> triangles;
        float relative[16];
        if (!mapScene_.localGeometry(slot, triangles, relative)) {
          error = "Geometria física indisponível";
          return false;
        }
        if (out.size() + triangles.size() >
            ColliderTopology::MaximumTriangles) {
          error = "Limite de edição: 100.000 triângulos";
          return false;
        }
        for (const auto &t : triangles) {
          EditorPickMesh::Triangle transformed;
          for (u32 v = 0; v < 3; ++v) {
            ColliderTopology::Point p;
            std::copy_n(t.data() + v * 3, 3, p.data());
            auto q = topologyWorldPoint(relative, p);
            std::copy(q.begin(), q.end(), transformed.begin() + v * 3);
          }
          out.push_back(transformed);
        }
      }
  } else {
    // Explicit conversion, never a substitute for the still-authored primitive.
    // Finite angular resolution keeps the actual convex solver's vertex budget.
    std::vector<AetherVec3> points;
    if (c->shape == scene::ColliderShape::Box)
      for (u32 i = 0; i < 8; ++i)
        points.push_back({i & 1 ? c->halfX : -c->halfX,
                          i & 2 ? c->halfY : -c->halfY,
                          i & 4 ? c->halfZ : -c->halfZ});
    else if (c->shape == scene::ColliderShape::Cylinder)
      for (u32 side = 0; side < 2; ++side)
        for (u32 n = 0; n < 24; ++n) {
          const float a = n * 6.28318530718f / 24;
          points.push_back({c->radius * std::cos(a),
                            side ? c->halfHeight : -c->halfHeight,
                            c->radius * std::sin(a)});
        }
    else
      for (u32 row = 0; row <= 8; ++row)
        for (u32 n = 0; n < (row == 0 || row == 8 ? 1u : 16u); ++n) {
          const float a = row * 3.14159265359f / 8, b = n * 6.28318530718f / 16;
          const float y = c->radius * std::cos(a) +
                          (c->shape == scene::ColliderShape::Capsule
                               ? (row < 4 ? c->halfHeight : -c->halfHeight)
                               : 0);
          points.push_back({c->radius * std::sin(a) * std::cos(b), y,
                            c->radius * std::sin(a) * std::sin(b)});
          if (c->shape == scene::ColliderShape::Capsule && row == 4)
            points.push_back({c->radius * std::cos(b), c->halfHeight,
                              c->radius * std::sin(b)});
        }
    physics::CookedConvexHull h;
    auto settings = AetherMeshCookingDefaultsV1;
    settings.hullTolerance = c->hullTolerance;
    if (!physics::cookConvexHull(points, settings, h, error))
      return false;
    for (usize i = 0; i < h.indices.size(); i += 3) {
      EditorPickMesh::Triangle t;
      for (u32 v = 0; v < 3; ++v) {
        const auto &p = h.vertices[h.indices[i + v]];
        t[v * 3] = p.x;
        t[v * 3 + 1] = p.y;
        t[v * 3 + 2] = p.z;
      }
      out.push_back(t);
    }
  }
  if (out.empty() || out.size() > ColliderTopology::MaximumTriangles) {
    error = "Malha vazia ou fora do orçamento de edição";
    return false;
  }
  error.clear();
  return true;
}
bool EditorSession::beginColliderTopology(EditorEntityId object, u64 instance,
                                          std::string &error) {
  if (isPlaying() || history_.isOpen() || state_.multiSelect ||
      state_.selectionSet.size() > 1) {
    error = "Edite um Colisor em Edit Mode, após finalizar o gesto atual";
    return false;
  }
  const auto *c = inspectedCollider(document_, object, instance);
  if (!c) {
    error = "Selecione um Colisor";
    return false;
  }
  const auto extractionStarted = std::chrono::steady_clock::now();
  std::vector<EditorPickMesh::Triangle> triangles;
  if (!collectColliderTopology(object, instance, triangles, error))
    return false;
  ColliderTopology draft;
  draft.object = object;
  draft.instance = instance;
  draft.convex = c->shape != scene::ColliderShape::Mesh || c->convex;
  draft.converted = c->shape != scene::ColliderShape::Mesh;
  if (!draft.build(triangles, error) || !draft.validate(c->hullTolerance)) {
    if (error.empty())
      error = draft.error;
    return false;
  }
  draft.sourceHash = topologyHash(triangles);
  draft.buildMs += ColliderTopology::ms(extractionStarted) - draft.buildMs -
                   draft.validateMs;
  cancelColliderTopology();
  setSelection(object);
  colliderTopology_ = std::move(draft);
  colliderTopologyVersion_ = sceneVersion();
  colliderTopologyRoot_ = files_.rootPath();
  state_.colliderTopology = &colliderTopology_;
  state_.expandedNative = instance;
  state_.componentSelection = object;
  state_.tool = EditorGizmoMode::Select;
  state_.physicsDiagnosticOpen = false;
  diagnosticScene_.reset();
  state_.status =
      "Colisão em prévia · toque numa face ou vértice · visual preservado";
  error.clear();
  return true;
}
void EditorSession::cancelColliderTopology() {
  topologyDragOpen_ = false;
  topologyDragBase_.clear();
  colliderTopology_.clear();
  state_.colliderTopology = nullptr;
}
bool EditorSession::selectColliderTopology(u32 element, bool additive) {
  auto &d = colliderTopology_;
  if (!d.active() ||
      element >= (d.vertexMode ? d.vertices.size() : d.faces.size()) ||
      topologyDragOpen_)
    return false;
  if (!additive)
    d.selected.clear();
  auto found = std::find(d.selected.begin(), d.selected.end(), element);
  if (found == d.selected.end())
    d.selected.push_back(element);
  else if (additive)
    d.selected.erase(found);
  return true;
}
bool EditorSession::editColliderTopologyCoordinate(u32 axis, float value,
                                                   std::string &error) {
  auto &d = colliderTopology_;
  ColliderTopology::Point center;
  if (!d.active() || axis > 2 || !d.centroid(center) || !std::isfinite(value)) {
    error = "Selecione elementos e informe uma coordenada finita";
    return false;
  }
  d.checkpoint();
  d.translate(d.vertices, axis, value - center[axis]);
  const auto *c = inspectedCollider(document_, d.object, d.instance);
  if (!c || !d.validate(c->hullTolerance)) {
    error = d.error;
    return false;
  }
  error.clear();
  return true;
}
bool EditorSession::applyColliderTopology(std::string &error) {
  const auto publicationStart = std::chrono::steady_clock::now();
  auto &d = colliderTopology_;
  const auto version = sceneVersion();
  const auto fail = [&](std::string e) {
    error = std::move(e);
    return false;
  };
  if (!d.active() || isPlaying() || history_.isOpen() || topologyDragOpen_ ||
      version.epoch != colliderTopologyVersion_.epoch ||
      version.revision != colliderTopologyVersion_.revision ||
      files_.rootPath() != colliderTopologyRoot_ ||
      state_.selection != d.object)
    return fail("Prévia desatualizada; reabra a edição");
  if (files_.rootPath().empty() || !publishGeometry_)
    return fail("Abra um projeto com publicação de recursos antes de aplicar");
  std::vector<EditorPickMesh::Triangle> original;
  if (!collectColliderTopology(d.object, d.instance, original, error))
    return false;
  if (topologyHash(original) != d.sourceHash)
    return fail("Recurso de origem mudou; reabra a prévia");
  const auto *collider = inspectedCollider(document_, d.object, d.instance);
  if (!collider || !d.validate(collider->hullTolerance))
    return fail(d.error);
  resources::ConvexBakePart part;
  part.vertices = d.vertices;
  part.indices = d.indices;
  part.sourceObject = d.object;
  std::vector<u8> bytes;
  if (!resources::writeCollisionTopologyGlb(part, d.sourceHash, bytes, error))
    return false;
  resources::GltfImport model;
  if (!resources::importGlb(bytes, importLimits_, {}, model))
    return fail(model.diagnostic);
  const auto hash = Sha256::hex(bytes);
  const auto path = "Collision/edit-" + hash + ".glb";
  std::string expected;
  if (const auto *asset = assets_.findByPath(path)) {
    if (asset->contentHash != hash)
      return fail("Recurso existente foi alterado; não será sobrescrito");
    expected = hash;
  }
  auto sources = importedSources_;
  auto assets = assets_;
  ModelImportReport report;
  StagedSource staged;
  if (!stageSource(model, hash, path, resources::ImportAmbiguityPolicy::Refuse,
                   sources, assets, report, staged, {}))
    return fail(report.diagnostic);
  const auto source =
      std::find_if(sources.begin(), sources.end(),
                   [&](const auto &s) { return s.guid == report.source; });
  if (source == sources.end() || source->identities.size() != 1)
    return fail("A edição deve produzir um recurso físico único");
  const auto guid = source->identities[0];
  auto candidate = *document_.find(d.object);
  auto edited = *collider;
  edited.shape = scene::ColliderShape::Mesh;
  edited.convex = d.convex;
  edited.collisionMesh = guid;
  if (d.converted)
    edited.meshLocalPose = true;
  if (!candidate.components.replaceInstance(d.instance, edited))
    return fail("Colisor recusou a geometria candidata");
  class Geometry final : public runtime::CollisionGeometrySource {
    const EditorMapScene &map;
    const ColliderTopology &draft;
    resources::AssetGuid guid;

  public:
    Geometry(const EditorMapScene &m, const ColliderTopology &d,
             resources::AssetGuid g)
        : map(m), draft(d), guid(g) {}
    bool meshTriangles(u32 slot, std::vector<float> &out) const override {
      std::span<const EditorPickMesh::Triangle> ts;
      float matrix[16];
      if (!map.localGeometry(slot, ts, matrix))
        return false;
      for (const auto &t : ts)
        for (u32 v = 0; v < 3; ++v) {
          ColliderTopology::Point p;
          std::copy_n(t.data() + v * 3, 3, p.data());
          auto w = topologyWorldPoint(matrix, p);
          out.insert(out.end(), w.begin(), w.end());
        }
      return true;
    }
    bool meshTriangles(const resources::AssetGuid &g,
                       std::vector<float> &out) const override {
      if (g != guid)
        return meshTriangles(map.assetSlot(g), out);
      for (auto i : draft.indices)
        out.insert(out.end(), draft.vertices[i].begin(),
                   draft.vertices[i].end());
      return true;
    }
  } geometry(mapScene_, d, guid);
  auto graph = document_;
  if (!graph.applyEntityValues(d.object, candidate))
    return fail("Cena candidata recusada");
  auto validationTarget = candidate;
  auto validationCollider = edited;
  validationCollider.enabled = true;
  if (!validationTarget.components.replaceInstance(d.instance,
                                                   validationCollider) ||
      !graph.applyEntityValues(d.object, validationTarget))
    return fail("Colisor candidato não pode ser validado");
  const auto owner = colliderInspectionBody(graph, d.object, d.instance);
  // Validate even inactive authored shapes; publication must not hide an error
  // until a later activation. Only the temporary graph is made active.
  for (const auto target : {d.object, owner})
    for (auto e = graph.find(target); e; e = graph.find(e->parent))
      if (!e->active) {
        auto value = *e;
        value.active = true;
        graph.applyEntityValues(value.id, value);
      }
  runtime::GameWorld world;
  runtime::ScenePhysics physics;
  if (!world.load(graph) || !physics.start(world, &geometry))
    return fail(physics.error().empty() ? "Solver recusou a cena candidata"
                                        : physics.error());
  std::vector<resources::AssetGuid> dependencies;
  if (collider->collisionMesh.valid())
    for (const auto &s : importedSources_)
      if (std::find(s.identities.begin(), s.identities.end(),
                    collider->collisionMesh) != s.identities.end())
        dependencies.push_back(s.guid);
  if (!commitModelImport(bytes, model, path, expected, report,
                         resources::ImportAmbiguityPolicy::Refuse, {}, {}, {},
                         dependencies))
    return fail(report.diagnostic);
  const auto object = d.object;
  const auto instance = d.instance;
  if (!history_.begin("Editar faces/vértices de colisão"))
    return fail("Histórico ocupado; objeto não alterado");
  if (!history_.applyValues(document_, object, candidate)) {
    history_.cancel(document_);
    return fail("Objeto recusou a publicação");
  }
  history_.end();
  cancelColliderTopology();
  state_.expandedNative = instance;
  std::ostringstream publication;
  publication << "Malha física aplicada · visual e UID preservados · 1 Undo · "
              << std::fixed << std::setprecision(2) << ColliderTopology::ms(publicationStart) << " ms";
  state_.status = publication.str();
  error.clear();
  return true;
}
bool EditorSession::pickColliderTopology(ui::UiPoint pixel) {
  auto &d = colliderTopology_;
  if (!d.active())
    return false;
  const auto start = std::chrono::steady_clock::now();
  float pose[16];
  if (!colliderTopologyPose(document_, d.object, d.instance, pose))
    return true;
  const auto ray = screenPointToRay(view_, pixel);
  float nearest = ray.maximumDistance;
  u32 triangle = ~0u;
  d.surface.intersect(ray.origin, ray.direction, pose, nearest,
                      ray.minimumDistance, ray.maximumDistance, &triangle);
  buildPickCandidates(nullptr, true);
  const auto foreign = pickNearest(candidates_, ray, d.object, true);
  u32 selected = ~0u;
  if (!d.vertexMode) {
    if (triangle != ~0u &&
        (d.hidden || !foreign.hit || foreign.distance >= nearest - .005f))
      selected = d.faceOfTriangle[triangle];
  } else {
    float closest = 22.f * 22.f;
    for (u32 i = 0; i < d.vertices.size(); ++i) {
      const auto p = topologyWorldPoint(pose, d.vertices[i]);
      const auto projected = projectWorldToScreen(view_, p.data());
      if (!projected.valid)
        continue;
      const float dx = projected.screen.x - pixel.x,
                  dy = projected.screen.y - pixel.y,
                  squared = dx * dx + dy * dy;
      if (squared >= closest)
        continue;
      const auto vertexRay = screenPointToRay(view_, projected.screen);
      float depth = 0;
      for (u32 k = 0; k < 3; ++k)
        depth += (p[k] - vertexRay.origin[k]) * vertexRay.direction[k];
      if (!d.hidden) {
        float h;
        const bool blocked = d.surface.intersect(
            vertexRay.origin, vertexRay.direction, pose, h,
            vertexRay.minimumDistance,
            std::max(vertexRay.minimumDistance, depth - .005f));
        if (blocked || colliderPointOccluded(candidates_, view_,
                                             projected.screen, d.object, depth))
          continue;
      }
      closest = squared;
      selected = i;
    }
  }
  if (selected != ~0u)
    selectColliderTopology(selected, d.additive);
  else if (!d.additive)
    d.selected.clear();
  ++d.picks;
  d.pickMs = ColliderTopology::ms(start);
  state_.status = d.selected.empty()
                      ? "Nenhum elemento visível selecionado"
                      : "Seleção física · " +
                            std::to_string(d.selected.size()) +
                            (d.vertexMode ? " vértice(s)" : " face(s)");
  return true;
}
void EditorSession::openPhysicsDiagnostic(bool open) {
  state_.physicsDiagnosticOpen = open;
  diagnosticLastTime_ = -1;
  if (!open) {
    diagnosticScene_.reset();
    state_.physicsDiagnosticProbes.clear();
  }
}
bool EditorSession::handleColliderAuthoringInput(
    const ui::UiPointerEvent &event, const ui::UiPointerRouting &routing) {
  auto &d = colliderTopology_;
  const auto key = routing.widgetId;
  if (routing.tapped &&
      key == widgetId(EditorWidget::ColliderAuthoringDetails)) {
    state_.colliderAuthoringDetails = !state_.colliderAuthoringDetails;
    return true;
  }
  const auto axisBase = widgetId(EditorWidget::ColliderGeometryAxisX);
  if (topologyDragOpen_ ||
      (d.active() && key >= axisBase && key < axisBase + 3)) {
    if (!topologyDragOpen_ && event.phase == ui::UiPointerPhase::Down &&
        viewportPointers_.empty() && !history_.isOpen()) {
      ColliderTopology::Point center;
      float pose[16];
      const u32 axis = key - axisBase;
      if (d.centroid(center) &&
          colliderTopologyPose(document_, d.object, d.instance, pose)) {
        auto point = topologyWorldPoint(pose, center);
        topologyDragScale_ = std::sqrt(pose[axis * 4] * pose[axis * 4] +
                                       pose[axis * 4 + 1] * pose[axis * 4 + 1] +
                                       pose[axis * 4 + 2] * pose[axis * 4 + 2]);
        if (topologyDragScale_ > 1e-6f) {
          for (u32 k = 0; k < 3; ++k) {
            topologyDragHandle_.point[k] = point[k];
            topologyDragHandle_.axis[k] =
                pose[axis * 4 + k] / topologyDragScale_;
          }
          if (cameraHandleRayParameter(view_, topologyDragHandle_,
                                       event.position, topologyDragStart_)) {
            topologyDragOpen_ = true;
            topologyDragPointer_ = event.pointerId;
            topologyDragAxis_ = axis;
            topologyDragBase_ = d.vertices;
            topologyDragView_ = view_;
          }
        }
      }
    }
    if (topologyDragOpen_ && event.pointerId == topologyDragPointer_) {
      if (event.phase == ui::UiPointerPhase::Cancel) {
        d.vertices = topologyDragBase_;
        const auto *c = inspectedCollider(document_, d.object, d.instance);
        if (c)
          d.validate(c->hullTolerance);
        topologyDragOpen_ = false;
      } else {
        if (routing.dragging) {
          float parameter;
          if (cameraHandleRayParameter(topologyDragView_, topologyDragHandle_,
                                       event.position, parameter))
            d.translate(topologyDragBase_, topologyDragAxis_,
                        (parameter - topologyDragStart_) / topologyDragScale_);
        }
        if (routing.released) {
          if (d.vertices != topologyDragBase_) {
            d.undo.push_back(topologyDragBase_);
            if (d.undo.size() > 32)
              d.undo.erase(d.undo.begin());
            d.redo.clear();
          }
          const auto *c = inspectedCollider(document_, d.object, d.instance);
          if (c)
            d.validate(c->hullTolerance);
          topologyDragOpen_ = false;
        }
      }
    }
    return true;
  }
  if (!routing.tapped)
    return false;
  std::string error;
  if (key == widgetId(EditorWidget::ColliderGeometryOpen)) {
    if (!beginColliderTopology(state_.selection, state_.expandedNative, error))
      state_.status = error;
    return true;
  }
  if (key == widgetId(EditorWidget::PhysicsDiagnosticOpen)) {
    cancelColliderTopology();
    openPhysicsDiagnostic(true);
    return true;
  }
  if (key == widgetId(EditorWidget::PhysicsDiagnosticClose)) {
    openPhysicsDiagnostic(false);
    return true;
  }
  if (key == widgetId(EditorWidget::PhysicsDiagnosticRefresh)) {
    diagnosticScene_.reset();
    diagnosticLastTime_ = -1;
    return true;
  }
  if (key == widgetId(EditorWidget::PhysicsDiagnosticVisual)) {
    state_.physicsDiagnosticVisual = !state_.physicsDiagnosticVisual;
    return true;
  }
  if (key == widgetId(EditorWidget::PhysicsDiagnosticCollision)) {
    state_.physicsDiagnosticCollision = !state_.physicsDiagnosticCollision;
    return true;
  }
  if (key == widgetId(EditorWidget::PhysicsDiagnosticSupport)) {
    state_.physicsDiagnosticSupport = !state_.physicsDiagnosticSupport;
    return true;
  }
  if (!d.active() || state_.numericField)
    return false;
  if (key == widgetId(EditorWidget::ColliderGeometryClose)) {
    cancelColliderTopology();
    state_.status = "Prévia descartada; cena preservada";
    return true;
  }
  if (key == widgetId(EditorWidget::ColliderGeometryApply)) {
    if (!applyColliderTopology(error))
      state_.status = error;
    return true;
  }
  if (key == widgetId(EditorWidget::ColliderGeometryVertex) ||
      key == widgetId(EditorWidget::ColliderGeometryFace)) {
    d.vertexMode = key == widgetId(EditorWidget::ColliderGeometryVertex);
    d.selected.clear();
    return true;
  }
  if (key == widgetId(EditorWidget::ColliderGeometryHidden)) {
    d.hidden = !d.hidden;
    return true;
  }
  if (key == widgetId(EditorWidget::ColliderGeometryAdditive)) {
    d.additive = !d.additive;
    return true;
  }
  if (key == widgetId(EditorWidget::ColliderGeometryUndo) ||
      key == widgetId(EditorWidget::ColliderGeometryRedo) ||
      key == widgetId(EditorWidget::Undo) ||
      key == widgetId(EditorWidget::Redo)) {
    const auto *c = inspectedCollider(document_, d.object, d.instance);
    if (c)
      d.travel(key == widgetId(EditorWidget::ColliderGeometryRedo) ||
                   key == widgetId(EditorWidget::Redo),
               c->hullTolerance);
    return true;
  }
  const auto coordinateBase =
      widgetId(EditorWidget::ColliderGeometryCoordinateX);
  if (key >= coordinateBase && key < coordinateBase + 3) {
    ColliderTopology::Point center;
    if (d.centroid(center)) {
      state_.numericField = key;
      state_.numericEntity = d.object;
      state_.numericInstance = d.instance;
      state_.numericProperty = "collision-topology";
      std::snprintf(state_.numericText, sizeof(state_.numericText), "%.9g",
                    double(center[key - coordinateBase]));
      state_.numericReplace = true;
      state_.numericError = false;
    }
    return true;
  }
  return false;
}
void EditorSession::refreshColliderAuthoring() {
  auto &d = colliderTopology_;
  const auto version = sceneVersion();
  if (d.active() &&
      (isPlaying() || state_.workspace != EditorWorkspace::Scene ||
       state_.selection != d.object || state_.expandedNative != d.instance ||
       version.epoch != colliderTopologyVersion_.epoch ||
       version.revision != colliderTopologyVersion_.revision ||
       files_.rootPath() != colliderTopologyRoot_)) {
    cancelColliderTopology();
    state_.status = "Prévia de colisão cancelada: alvo, cena ou modo mudou";
  }
  state_.colliderTopology = d.active() ? &d : nullptr;
  if (state_.workspace != EditorWorkspace::Scene &&
      state_.workspace != EditorWorkspace::Play) {
    openPhysicsDiagnostic(false);
    return;
  }
  if (!state_.physicsDiagnosticOpen) {
    diagnosticScene_.reset();
    return;
  }
  if (d.active()) {
    openPhysicsDiagnostic(false);
    return;
  }
  const auto &graph = playInspecting() ? playScene_.document() : document_;
  auto target =
      colliderInspectionBody(graph, state_.selection, state_.expandedNative);
  if (!target)
    target = state_.selection;
  const auto previousTarget = state_.physicsDiagnosticTarget;
  state_.physicsDiagnosticTarget = target;
  state_.physicsDiagnosticLive = isPlaying() && playScene_.active();
  const bool stale = !diagnosticScene_ ||
                     version.epoch != diagnosticVersion_.epoch ||
                     version.revision != diagnosticVersion_.revision ||
                     files_.rootPath() != diagnosticRoot_;
  if (!state_.physicsDiagnosticLive && !stale && diagnosticLastTime_ >= 0 &&
      previousTarget == target)
    return;
  if (state_.physicsDiagnosticLive && diagnosticLastTime_ >= 0 &&
      previousTarget == target && state_.uiTime - diagnosticLastTime_ < .1)
    return;
  const auto start = std::chrono::steady_clock::now();
  state_.physicsDiagnosticError.clear();
  state_.physicsDiagnosticHasCom = state_.physicsDiagnosticHasSupport = false;
  state_.physicsDiagnosticHasCapsule=false;
  const runtime::GameWorld *world;
  const runtime::ScenePhysics *physics;
  if (state_.physicsDiagnosticLive) {
    diagnosticScene_.reset();
    world = &playScene_.world();
    physics = &playScene_.physics();
  } else {
    if (stale) {
      diagnosticScene_ = std::make_unique<PhysicsPreview>();
      ++state_.physicsDiagnosticBuilds;
      if (!EditorPlayScene::previewPhysics(
              document_, mapScene_, diagnosticScene_->world,
              diagnosticScene_->physics, state_.physicsDiagnosticError)) {
        diagnosticVersion_ = version;
        diagnosticRoot_ = files_.rootPath();
        diagnosticLastTime_ = state_.uiTime;
        return;
      }
      diagnosticVersion_ = version;
      diagnosticRoot_ = files_.rootPath();
    }
    world = &diagnosticScene_->world;
    physics = &diagnosticScene_->physics;
  }
  runtime::ScenePhysics::Diagnostic value;
  if (!physics->diagnostic(*world, target, .5f, value)) {
    state_.physicsDiagnosticError = "Alvo sem estado físico disponível";
    return;
  }
  const char *authorities[]{"Livre (Transform)", "Corpo físico 3D",
                            "Personagem", "Corpo físico 2D"};
  std::ostringstream text;
  text.imbue(std::locale::classic());
  text << std::fixed << std::setprecision(3);
  text << (state_.physicsDiagnosticLive ? "Jolt · Play ao vivo"
                                        : "Jolt · prévia autoral, sem simular")
       << '\n';
  text << "Autoridade: " << authorities[static_cast<u32>(value.authority)]
       << '\n';
  if(value.hasControl) {
    const auto &control=value.control;
    text << "Posse: " << (state_.physicsDiagnosticLive&&control.measured
        ? (control.source==scene::MotorControlSource::None?"Sem intenção":scene::motorControlName(control.source)) : "Aguardando passo em Play") << '\n';
    if(state_.physicsDiagnosticLive&&control.measured) {
      text << (control.focused?"Com foco":"Entrada suspensa") << " · prioridade " << control.priority << " · candidatos: ";
      bool any=false;for(u32 n=1;n<=5;++n)if(control.candidates&(1u<<n)){if(any)text << ", ";text << scene::motorControlName(scene::MotorControlSource(n));any=true;}
      if(!any)text << "nenhum";
      text << "\nIntenção " << control.right << " / " << control.forward << (control.jump?" · salto solicitado":"") << '\n';
    }
  }
  text << "Dono: " << target << " · " << value.colliderCount << " Colisores · "
       << physics->bodyCount() << " corpos na cena\n";
  if(!value.hasCharacter) {
  text << "Caixas " << value.shapeCounts[0] << " · esferas "
       << value.shapeCounts[1] << " · cápsulas " << value.shapeCounts[2]
       << '\n';
  text << "Malhas " << value.shapeCounts[3] << " · cilindros "
       << value.shapeCounts[4] << " · desenho até 9.600 segmentos\n";
  }
  if(value.hasCharacter && value.character.hasCapsule) {
    state_.physicsDiagnosticHasCapsule=true;
    const auto a=value.character.capsuleBottom,b=value.character.capsuleTop;
    std::copy_n(&a.x,3,state_.physicsDiagnosticCapsuleBottom);
    std::copy_n(&b.x,3,state_.physicsDiagnosticCapsuleTop);
    state_.physicsDiagnosticCapsuleRadius=value.character.capsuleRadius;
    text << "Cápsula atual do solver · raio " << value.character.capsuleRadius << '\n';
  }
  if (value.hasBody) {
    auto com = value.body.centerOfMass;
    state_.physicsDiagnosticHasCom = true;
    state_.physicsDiagnosticCom[0] = com.x;
    state_.physicsDiagnosticCom[1] = com.y;
    state_.physicsDiagnosticCom[2] = com.z;
    text << "COM mundial: " << com.x << " / " << com.y << " / " << com.z
         << '\n';
    text << "Velocidade: " << value.body.linear.x << " / "
         << value.body.linear.y << " / " << value.body.linear.z << " u/s\n";
    text << ((value.body.flags & 1)   ? "Ativo"
             : (value.body.flags & 8) ? "Dormindo"
                                      : "Estático")
         << " · " << value.probeCount << " pontos de apoio geométricos\n";
  } else
    text << (value.hasCharacter
                 ? "Personagem virtual · COM de Body não aplicável"
                 : "Sem Body/Character no solver")
         << '\n';
  const bool actual = value.motorSupport && state_.physicsDiagnosticLive;
  text << (value.hasCharacter && !value.character.hasMeasuredStep
               ? "Personagem ainda sem passo físico: "
           : actual ? "Apoio do motor: "
                    : "Consulta sob a geometria (0,5 u): ")
       << (value.hasSupport ? "encontrado" : "ausente") << '\n';
  if (value.hasSupport) {
    state_.physicsDiagnosticHasSupport = true;
    const auto p = value.supportPoint, n = value.supportNormal;
    state_.physicsDiagnosticPoint[0] = p.x;
    state_.physicsDiagnosticPoint[1] = p.y;
    state_.physicsDiagnosticPoint[2] = p.z;
    state_.physicsDiagnosticNormal[0] = n.x;
    state_.physicsDiagnosticNormal[1] = n.y;
    state_.physicsDiagnosticNormal[2] = n.z;
    text << "Ponto: " << p.x << " / " << p.y << " / " << p.z << " · alvo "
         << value.support << '\n';
  }
  state_.physicsDiagnosticProbes.clear();
  for (u32 i = 0; i < value.probeCount; ++i)
    state_.physicsDiagnosticProbes.push_back(
        {value.probes[i].x, value.probes[i].y, value.probes[i].z});
  state_.physicsDiagnosticText = text.str();
  state_.physicsDiagnosticMs = ColliderTopology::ms(start);
  diagnosticLastTime_ = state_.uiTime;
}
} // namespace ae::editor
