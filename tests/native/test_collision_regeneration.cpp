#include "convex_bake_fixture.h"
#include "harness.h"
#include "scene/collision_recipe.h"
#include "scene/prefab_link.h"
#include "ui_software_raster.h"
#include <fstream>
using namespace ae;
namespace {
struct RecipeFixture {
  std::filesystem::path root =
      std::filesystem::path(AETHER_REPOSITORY_ROOT) / "build" /
      ("collision-recipe-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  test::ConvexCpuLibrary library;
  editor::EditorSession session;
  ui::UiFont font;
  ui::UiIconAtlas icons;
  // Both loaders retain views of the binary pixel data. Keep the backing files
  // alive for routing AND software rendering, as the Android asset owner does.
  std::vector<u8> fontBytes, iconBytes;
  editor::EditorEntityId owner = 0, left = 0, right = 0;
  bool setup() {
    const auto read = [](const char *file) {
      std::ifstream in(std::filesystem::path(AETHER_REPOSITORY_ROOT) /
                           "assets/astra-visual/ui" / file,
                       std::ios::binary);
      return std::vector<u8>(std::istreambuf_iterator<char>(in), {});
    };
    fontBytes = read("astra-ui-font.aeuf");
    iconBytes = read("astra-ui-icons.aeui");
    if (!font.load(fontBytes) || !icons.load(iconBytes))
      return false;
    std::filesystem::create_directories(root);
    if (!session.setProjectDirectory(root.generic_string().c_str()) ||
        !library.connect(session))
      return false;
    session.initialize(&font, &icons);
    session.setSurface({0, 0, 1280, 720}, {});
    auto &g = session.document();
    owner = g.createEntity(g.root(), runtime::ObjectKind::Folder, "RecipeBody");
    for (u32 i = 0; i < 2; ++i) {
      const auto id = g.createEntity(owner, runtime::ObjectKind::Mesh,
                                     i ? "RightSource" : "LeftSource");
      auto e = *g.find(id);
      if (!runtime::configurePrimitive(
              e, scene::PrimitiveType::Cube,
              {1, session.mapScene().assetGuid(0),
               session.mapScene().materialForAsset(0)}))
        return false;
      e.components.remove(scene::PhysicsBody::descriptor);
      e.components.remove(scene::Collider::descriptor);
      e.transform.position[0] = i ? 2 : -2;
      if (!g.applyEntityValues(id, e))
        return false;
      (i ? right : left) = id;
    }
    const editor::EditorSession::MotorBakeSource selected[]{{left, 0},
                                                            {right, 0}};
    std::string error;
    if (!session.selectMotorDecompositionSources(owner, selected, error))
      return false;
    resources::ConvexBakeSettings settings;
    settings.maximumParts = 4;
    settings.voxelResolution = 10000;
    settings.timeBudgetSeconds = 30;
    if (!session.beginMotorDecomposition(owner, settings, error) ||
        !test::waitConvexBake(session) ||
        !session.applyMotorDecomposition(error)) {
      std::fprintf(stderr, "recipe setup: %s %s\n", error.c_str(),
                   session.motorDecompositionProgress().error.c_str());
      return false;
    }
    return true;
  }
  bool regenerate(std::string &error) {
    return session.beginMotorRegeneration(owner, error) &&
           test::waitConvexBake(session);
  }
};
bool cast(runtime::ScenePhysics &physics, float x, runtime::QueryHit &hit) {
  const float p[]{x, 3, 0}, d[]{0, -6, 0};
  return physics.rayCast(p, d, {}, hit);
}
void capture(editor::EditorSession &s, const ui::UiFont &font,
             const ui::UiIconAtlas &icons, const std::filesystem::path &path) {
  s.update();
  test::UiSoftwareTarget target;
  const auto size = s.screen().surface;
  target.resize(static_cast<u32>(size.width), static_cast<u32>(size.height),
                .1f, .1f, .1f);
  const auto &im = s.immediateGui();
  const auto &images = s.guiImages();
  test::rasterizeUi(s.instances(), font, icons, target, im.atlas(),
                    im.atlasWidth(), im.atlasHeight(), images.pixels(),
                    images.size());
  std::ofstream out(path, std::ios::binary);
  out << "P6\n"
      << static_cast<u32>(size.width) << ' ' << static_cast<u32>(size.height)
      << "\n255\n";
  for (usize i = 0; i < target.pixels.size(); i += 4)
    for (u32 c = 0; c < 3; ++c)
      out.put(static_cast<char>(
          std::clamp(target.pixels[i + c], 0.f, 1.f) * 255 + .5f));
}
} // namespace
AE_TEST(collision_recipe_regeneration_history_reopen_and_real_solver) {
  RecipeFixture f;
  AE_EXPECT_TRUE(f.setup(),
                 "real builtin source meshes, worker, publication and recipe");
  auto &s = f.session;
  auto &g = s.document();
  std::string error;
  const auto recipe = *scene::collisionRecipe(g.find(f.owner)->components);
  AE_EXPECT_TRUE(recipe.sources.size() == 2 && recipe.parts.size() == 2 &&
                     recipe.settings.maximumParts == 4,
                 "persistent exact settings and source instances");
  const auto a = recipe.parts[0].collider, b = recipe.parts[1].collider;
  auto authored = *g.find(f.owner);
  auto *part =
      static_cast<scene::Collider *>(authored.components.editInstance(a));
  part->centerX = .3f;
  part->rotationY = 12;
  part->hullTolerance = .005f;
  AE_EXPECT_TRUE(authored.components.removeInstance(b) &&
                     g.applyEntityValues(f.owner, authored),
                 "local pose/cooking edits and local component removal");
  auto source = *g.find(f.left);
  source.transform.position[0] -= .2f;
  AE_EXPECT_TRUE(g.applyEntityValues(f.left, source), "source change");
  const auto before = editor::serializeEditorDocument(g, 0);
  const auto depth = s.history().undoDepth();
  AE_EXPECT_TRUE(f.regenerate(error), error.c_str());
  AE_EXPECT_TRUE(editor::serializeEditorDocument(g, 0) == before &&
                     s.history().undoDepth() == depth,
                 "worker and correspondence do not mutate authoring");
  AE_EXPECT_TRUE(!s.screen().motorBakeMappingConfirmed &&
                     !s.applyMotorDecomposition(error),
                 "spatial proposal needs explicit confirmation");
  AE_EXPECT_TRUE(s.confirmMotorDecompositionMapping(error) &&
                     s.applyMotorDecomposition(error),
                 error.c_str());
  const auto *object = g.find(f.owner);
  const auto *after =
      static_cast<const scene::Collider *>(object->components.findInstance(a));
  AE_EXPECT_TRUE(after && after->centerX == .3f && after->rotationY == 12 &&
                     after->hullTolerance == .005f &&
                     !object->components.findInstance(b),
                 "stable UID, effective fields and tombstone preserved");
  const auto *newRecipe = scene::collisionRecipe(object->components);
  AE_EXPECT_TRUE(newRecipe->parts.size() == 2 &&
                     newRecipe->parts[0].baseline.centerX == 0,
                 "new base is distinct from the local override");
  std::ostringstream oldBody, newBody;
  runtime::physicsBody(authored)->write(oldBody);
  runtime::physicsBody(*object)->write(newBody);
  AE_EXPECT_TRUE(oldBody.str() == newBody.str(),
                 "Body unchanged by geometry authoring");
  test::ConvexMapGeometry geometry(s.mapScene());
  runtime::GameWorld world;
  runtime::ScenePhysics physics;
  runtime::QueryHit hit;
  AE_EXPECT_TRUE(world.load(g) && physics.start(world, &geometry) &&
                     cast(physics, -1.9f, hit) && hit.colliderInstance == a &&
                     !cast(physics, 2, hit),
                 "actual regenerated Jolt shape and preserved deletion");
  physics.stop();
  const auto saved = editor::serializeEditorDocument(g, 0);
  AE_EXPECT_TRUE(s.history().undoDepth() == depth + 1 && s.history().undo(g) &&
                     editor::serializeEditorDocument(g, 0) == before &&
                     s.history().redo(g) &&
                     editor::serializeEditorDocument(g, 0) == saved,
                 "single byte-exact Undo/Redo");
  editor::EditorDocument reopened;
  AE_EXPECT_TRUE(editor::deserializeEditorDocument(saved, 0, reopened),
                 "recipe with typed source identities round-trips");
  editor::EditorSession fresh;
  test::ConvexCpuLibrary library;
  AE_EXPECT_TRUE(
      library.connect(fresh) &&
          fresh.setProjectDirectory(f.root.generic_string().c_str()) &&
          fresh.loadAssets(s.serializeAssets()),
      "fresh asset registry and source library");
  fresh.document() = reopened;
  std::vector<editor::EditorSession::ReopenedSource> savedSources;
  for (const auto &record : s.assets().records())
    if (record.type == resources::AssetType::Mesh) {
      std::vector<u8> bytes;
      resources::GltfImport model;
      AE_EXPECT_TRUE(
          editor::EditorImportTransaction::read(
              f.root / editor::EditorImportTransaction::fromUtf8(record.path),
              bytes) &&
              resources::importGlb(bytes, {}, {}, model),
          "read actual published geometry");
      savedSources.push_back(
          {std::move(model), record.contentHash, record.path});
    }
  std::vector<editor::EditorSession::ModelImportReport> reports;
  AE_EXPECT_TRUE(fresh.reopenSources(savedSources, reports, error),
                 error.c_str());
  fresh.setSelection(f.owner);
  AE_EXPECT_TRUE(fresh.beginMotorRegeneration(f.owner, error) &&
                     test::waitConvexBake(fresh) &&
                     fresh.confirmMotorDecompositionMapping(error) &&
                     fresh.applyMotorDecomposition(error),
                 error.c_str());
  const auto *twice = static_cast<const scene::Collider *>(
      fresh.document().find(f.owner)->components.findInstance(a));
  AE_EXPECT_TRUE(
      twice && twice->centerX == .3f &&
          !fresh.document().find(f.owner)->components.findInstance(b),
      "second regeneration after reopen retains override and tombstone");
}
AE_TEST(collision_recipe_prefab_instance_references_and_independence) {
  RecipeFixture f;
  AE_EXPECT_TRUE(f.setup(), "physical recipe fixture");
  auto &s = f.session;
  auto &g = s.document();
  std::string error;
  const auto asset = s.createPrefab(f.owner, error);
  AE_EXPECT_TRUE(asset.valid(), error.c_str());
  const auto instance = s.instantiatePrefab(asset, g.root(), error);
  AE_EXPECT_TRUE(instance, error.c_str());
  const auto *recipe = scene::collisionRecipe(g.find(instance)->components);
  AE_EXPECT_TRUE(recipe && recipe->sources[0].object != f.left &&
                     recipe->sources[1].object != f.right &&
                     g.isDescendantOf(static_cast<editor::EditorEntityId>(
                                          recipe->sources[0].object),
                                      instance),
                 "source references remapped into the cloned prefab");
  const auto uid = recipe->parts.front().collider;
  auto child = *g.find(
      static_cast<editor::EditorEntityId>(recipe->sources.front().object));
  child.transform.position[0] -= .15f;
  AE_EXPECT_TRUE(g.applyEntityValues(child.id, child), "instance source only");
  auto object = *g.find(instance);
  static_cast<scene::Collider *>(object.components.editInstance(uid))->centerY =
      .2f;
  AE_EXPECT_TRUE(g.applyEntityValues(instance, object),
                 "local prefab collider override");
  const auto untouched = runtime::serializePrefabObject(*g.find(f.owner));
  AE_EXPECT_TRUE(s.beginMotorRegeneration(instance, error) &&
                     test::waitConvexBake(s) &&
                     s.confirmMotorDecompositionMapping(error) &&
                     s.applyMotorDecomposition(error),
                 error.c_str());
  AE_EXPECT_TRUE(runtime::serializePrefabObject(*g.find(f.owner)) ==
                         untouched &&
                     scene::prefabLink(g.find(instance)->components) &&
                     static_cast<const scene::Collider *>(
                         g.find(instance)->components.findInstance(uid))
                             ->centerY == .2f,
                 "regeneration preserves prefab link and isolates shared "
                 "source/other instance");
  editor::PrefabOverrideView view;
  AE_EXPECT_TRUE(s.inspectPrefabOverrides(instance, view, error),
                 error.c_str());
  bool recipeOverride = false;
  for (const auto &row : view.rows)
    recipeOverride |=
        row.component ==
        scene::collisionRecipe(g.find(instance)->components)->instanceId();
  AE_EXPECT_TRUE(recipeOverride,
                 "recipe baseline change is an actual atomic prefab override");
  usize recipeRow = view.rows.size();
  for (usize i = 0; i < view.rows.size(); ++i)
    if (view.rows[i].component ==
        scene::collisionRecipe(g.find(instance)->components)->instanceId())
      recipeRow = i;
  AE_EXPECT_TRUE(
      recipeRow < view.rows.size() && view.rows[recipeRow].applyable &&
          view.rows[recipeRow].applyReason.find("Revisão física") !=
              std::string::npos,
      "editor explicitly discloses recipe and physical parts as one revision");
  AE_EXPECT_TRUE(s.applyPrefabOverride(view, recipeRow, false, error),
                 error.c_str());
  const auto third = s.instantiatePrefab(asset, g.root(), error);
  AE_EXPECT_TRUE(third, error.c_str());
  for (const auto id : {f.owner, third}) {
    const auto *published = scene::collisionRecipe(g.find(id)->components);
    AE_EXPECT_TRUE(
        published && published->geometryHash ==
                         scene::collisionRecipe(g.find(instance)->components)
                             ->geometryHash,
        "new and inherited instances receive the same published recipe");
    for (const auto &p : published->parts) {
      const auto *c = static_cast<const scene::Collider *>(
          g.find(id)->components.findInstance(p.collider));
      AE_EXPECT_TRUE(c && c->collisionMesh == p.baseline.collisionMesh,
                     "recipe publication also transfers concrete physical "
                     "meshes instead of corrupting baseline comparison");
    }
    AE_EXPECT_TRUE(static_cast<const scene::Collider *>(
                       g.find(id)->components.findInstance(uid))
                           ->centerY == .2f,
                   "explicit physical revision carries authored part fields");
  }
}
AE_TEST(collision_recipe_mapping_staleness_missing_source_and_publish_refusal) {
  RecipeFixture f;
  AE_EXPECT_TRUE(f.setup(), "physical recipe fixture");
  auto &s = f.session;
  auto &g = s.document();
  std::string error;
  AE_EXPECT_TRUE(f.regenerate(error), error.c_str());
  const auto a = s.screen().motorBakePartMapping[0],
             b = s.screen().motorBakePartMapping[1];
  AE_EXPECT_TRUE(a && b && a != b &&
                     !s.setMotorDecompositionPartMapping(0, b, error),
                 "bijection rejects duplicate previous component");
  AE_EXPECT_TRUE(s.setMotorDecompositionPartMapping(0, 0, error) &&
                     s.setMotorDecompositionPartMapping(0, a, error) &&
                     s.confirmMotorDecompositionMapping(error),
                 error.c_str());
  const auto before = editor::serializeEditorDocument(g, 0);
  const auto registry = s.serializeAssets();
  f.library.refuse = true;
  // Force a distinct revision so the actual publisher must run.
  auto child = *g.find(f.left);
  child.transform.position[0] -= .1f;
  AE_EXPECT_TRUE(g.applyEntityValues(f.left, child) &&
                     !s.applyMotorDecomposition(error),
                 "stale Apply fails before publishing");
  s.cancelMotorDecomposition();
  const auto changed = editor::serializeEditorDocument(g, 0);
  AE_EXPECT_TRUE(f.regenerate(error) &&
                     s.confirmMotorDecompositionMapping(error) &&
                     !s.applyMotorDecomposition(error),
                 "real publisher refusal rolls back");
  AE_EXPECT_TRUE(editor::serializeEditorDocument(g, 0) == changed &&
                     s.serializeAssets() == registry,
                 "scene, recipe and asset journal remain unchanged on refusal");
  f.library.refuse = false;
  auto missing = *g.find(f.owner);
  auto *r = static_cast<scene::CollisionRecipe *>(
      missing.components.edit(scene::CollisionRecipe::descriptor));
  r->sources[0].object = 0;
  AE_EXPECT_TRUE(g.applyEntityValues(f.owner, missing),
                 "missing authoring source remains a diagnosable draft");
  s.cancelMotorDecomposition();
  AE_EXPECT_TRUE(!s.beginMotorRegeneration(f.owner, error) && !error.empty(),
                 "missing source does not cook visual fallback");
  (void)before;
}
AE_TEST(collision_recipe_real_glb_reimport_and_physical_mesh_override) {
  RecipeFixture f;
  AE_EXPECT_TRUE(f.setup(), "published physical recipe");
  auto &s = f.session;
  auto &g = s.document();
  std::string error;
  const auto uid =
      scene::collisionRecipe(g.find(f.owner)->components)->parts[0].collider;
  const auto cube = s.mapScene().assetGuid(0);
  const auto publishCube = [&](float width, const std::string &expected,
                               editor::EditorSession::ModelImportReport
                                   &report) {
    std::span<const editor::EditorPickMesh::Triangle> triangles;
    float transform[16];
    if (!s.mapScene().localGeometry(s.mapScene().assetSlot(cube), triangles,
                                    transform))
      return false;
    resources::ConvexBakePart part;
    for (const auto &t : triangles)
      for (u32 v = 0; v < 3; ++v) {
        part.indices.push_back(static_cast<u32>(part.vertices.size()));
        part.vertices.push_back({t[v * 3] * width, t[v * 3 + 1], t[v * 3 + 2]});
      }
    std::vector<u8> bytes;
    resources::GltfImport model;
    return resources::writeCollisionTopologyGlb(part, std::string(64, '0'),
                                                bytes, error) &&
           resources::importGlb(bytes, {}, {}, model) &&
           s.commitModelImport(bytes, model, "Sources/reimported-cube.glb",
                               expected, report);
  };
  editor::EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(publishCube(1, {}, report) &&
                     s.instantiateModel(report.source, report, false),
                 report.diagnostic.c_str());
  const auto imported = s.selection();
  const auto asset = runtime::meshRenderer(*g.find(imported))->slotAsset(0);
  auto child = *g.find(f.left);
  auto *render = runtime::editMeshRenderer(child);
  *render->editSlotAsset(0) = asset;
  *render->editSlotMesh(0) = s.mapScene().assetSlot(asset);
  AE_EXPECT_TRUE(
      g.applyEntityValues(f.left, child) && g.destroyEntity(imported),
      "source object keeps identity and uses actual imported GLB mesh");
  AE_EXPECT_TRUE(f.regenerate(error) &&
                     s.confirmMotorDecompositionMapping(error) &&
                     s.applyMotorDecomposition(error),
                 error.c_str());
  auto edited = *g.find(f.owner);
  static_cast<scene::Collider *>(edited.components.editInstance(uid))->centerZ =
      .15f;
  AE_EXPECT_TRUE(g.applyEntityValues(f.owner, edited),
                 "local pose before source reimport");
  const auto before = runtime::serializePrefabObject(*g.find(f.owner));
  const auto oldSource = report.source;
  const auto oldHash = s.assets().find(oldSource)->contentHash;
  AE_EXPECT_TRUE(publishCube(1.4f, oldHash, report) && report.reimported &&
                     report.source == oldSource &&
                     s.mapScene().assetSlot(asset),
                 report.diagnostic.c_str());
  AE_EXPECT_TRUE(runtime::serializePrefabObject(*g.find(f.owner)) == before,
                 "source reimport does not silently replace physics or recipe");
  AE_EXPECT_TRUE(f.regenerate(error) &&
                     s.confirmMotorDecompositionMapping(error) &&
                     s.applyMotorDecomposition(error),
                 error.c_str());
  const auto *collider = static_cast<const scene::Collider *>(
      g.find(f.owner)->components.findInstance(uid));
  const auto &base =
      scene::collisionRecipe(g.find(f.owner)->components)->parts[0];
  AE_EXPECT_TRUE(collider && collider->centerZ == .15f &&
                     base.high[0] - base.low[0] > 1.3f,
                 "new source bounds consumed while local pose and UID survive");
  test::ConvexMapGeometry geometry(s.mapScene());
  runtime::GameWorld world;
  runtime::ScenePhysics physics;
  runtime::QueryHit hit;
  AE_EXPECT_TRUE(world.load(g) && physics.start(world, &geometry) &&
                     cast(physics, -1.35f, hit) && hit.colliderInstance == uid,
                 "actual solver sees enlarged regenerated GLB");
  physics.stop();
  edited = *g.find(f.owner);
  auto *local =
      static_cast<scene::Collider *>(edited.components.editInstance(uid));
  local->collisionMesh = cube;
  local->centerX = -2;
  local->convex = false;
  local->enabled = false;
  AE_EXPECT_TRUE(g.applyEntityValues(f.owner, edited) && f.regenerate(error) &&
                     s.confirmMotorDecompositionMapping(error),
                 error.c_str());
  const auto registry = s.serializeAssets(),
             invalid = editor::serializeEditorDocument(g, 0);
  AE_EXPECT_TRUE(!s.applyMotorDecomposition(error) && !error.empty() &&
                     s.serializeAssets() == registry &&
                     editor::serializeEditorDocument(g, 0) == invalid,
                 "disabled invalid dynamic mesh override cannot conceal an "
                 "unsafe publication");
  s.cancelMotorDecomposition();
  edited = *g.find(f.owner);
  local = static_cast<scene::Collider *>(edited.components.editInstance(uid));
  local->convex = true;
  local->enabled = true;
  AE_EXPECT_TRUE(g.applyEntityValues(f.owner, edited) && f.regenerate(error) &&
                     s.confirmMotorDecompositionMapping(error) &&
                     s.applyMotorDecomposition(error),
                 error.c_str());
  collider = static_cast<const scene::Collider *>(
      g.find(f.owner)->components.findInstance(uid));
  AE_EXPECT_TRUE(
      collider && collider->collisionMesh == cube && collider->centerX == -2 &&
          collider->centerZ == .15f,
      "separate physical mesh resource and its pose remain authored overrides");
  runtime::GameWorld manual;
  AE_EXPECT_TRUE(manual.load(g) && physics.start(manual, &geometry) &&
                     cast(physics, -2, hit) && hit.colliderInstance == uid &&
                     !cast(physics, -1.35f, hit),
                 "solver consumes manual physical mesh after regeneration");
}
AE_TEST(collision_recipe_compact_review_pointer_and_visual_capture) {
  RecipeFixture f;
  AE_EXPECT_TRUE(f.setup(), "executable native UI fixture");
  auto &s = f.session;
  auto &g = s.document();
  std::string error;
  auto child = *g.find(f.left);
  child.transform.position[0] -= .2f;
  AE_EXPECT_TRUE(g.applyEntityValues(f.left, child), "changed source");
  s.setSurface({0, 0, 800, 400}, {});
  s.setSelection(f.owner);
  s.frameAll();
  std::filesystem::create_directories(f.root / "scenes");
  const std::string descriptor =
      R"({"format":"ASTRA-PROJECT-1","resourceSource":"independent","project":{"name":"Collision Recipe U05U09","template":"empty","scenes":1,"assets":2},"mainScene":"scenes/editor.aescene","editorScene":"scenes/editor.aescene"})";
  AE_EXPECT_TRUE(editor::EditorImportTransaction::writeText(
                     f.root / "project.json", descriptor) &&
                     editor::EditorImportTransaction::writeText(
                         f.root / "scenes/editor.aescene",
                         editor::serializeEditorDocument(g, 0)),
                 "isolated editable device acceptance project");
  AE_EXPECT_TRUE(
      test::tapConvexWidget(s, f.font, editor::EditorWidget::InspectorMenu) &&
          test::tapConvexWidget(s, f.font,
                                editor::EditorWidget::MotorSetupOpen),
      "real contextual object route");
  capture(s, f.font, f.icons, f.root / "start-compact.ppm");
  AE_EXPECT_TRUE(test::tapConvexWidget(
                     s, f.font, editor::EditorWidget::MotorBakeRegenerate) &&
                     test::waitConvexBake(s),
                 "real route enters regeneration with persisted custom budget");
  AE_EXPECT_TRUE(s.screen().motorBakeBudget == 3,
                 "custom persisted settings do not falsely select a preset");
  capture(s, f.font, f.icons, f.root / "review-compact.ppm");
  AE_EXPECT_TRUE(
      test::tapConvexWidget(s, f.font,
                            editor::EditorWidget::MotorBakeMappingBase) &&
          test::tapConvexWidget(s, f.font,
                                editor::EditorWidget::MotorBakeConfirmMapping),
      "compact mapping and confirmation are reachable");
  capture(s, f.font, f.icons, f.root / "confirmed-compact.ppm");
  AE_EXPECT_TRUE(
      test::tapConvexWidget(s, f.font, editor::EditorWidget::MotorBakeNext),
      "all candidate parts accessible");
  AE_EXPECT_TRUE(s.screen().motorBakePage == 1,
                 "actual pointer event selects the second candidate page");
  capture(s, f.font, f.icons, f.root / "next-compact.ppm");
  s.setSurface({0, 0, 1500, 700}, {});
  AE_EXPECT_TRUE(test::tapConvexWidget(s, f.font,
                                     editor::EditorWidget::MotorBakePrevious) &&
                     test::tapConvexWidget(s, f.font,
                                          editor::EditorWidget::MotorBakeNext),
                 "full Inspector chrome still exposes every candidate page");
  AE_EXPECT_TRUE(s.screen().motorBakePage == 1,
                 "noncompact review does not hide the second candidate");
  capture(s, f.font, f.icons, f.root / "next-full.ppm");
  std::printf("Collision recipe UI captures: %s\n",
              f.root.generic_string().c_str());
}
