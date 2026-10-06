#include "collider_occlusion_fixture.h"
#include "editor/editor_archive.h"
#include "harness.h"
#include "ui_software_raster.h"
#include <fstream>
#include <sstream>
using namespace ae;
namespace {
using namespace editor;
struct U11Fixture {
  std::filesystem::path root;
  std::vector<u8> fontBytes, iconBytes;
  ui::UiFont font;
  ui::UiIconAtlas icons;
  test::ConvexCpuLibrary library;
  EditorSession session;
  EditorEntityId body = 0, part = 0, sibling = 0;
  bool setup() {
    const auto read = [](const char *name) {
      std::ifstream in(std::filesystem::path(AETHER_REPOSITORY_ROOT) /
                           "assets/astra-visual/ui" / name,
                       std::ios::binary);
      return std::vector<u8>(std::istreambuf_iterator<char>(in), {});
    };
    fontBytes = read("astra-ui-font.aeuf");
    iconBytes = read("astra-ui-icons.aeui");
    if (!font.load(fontBytes) || !icons.load(iconBytes))
      return false;
    root = std::filesystem::path(AETHER_REPOSITORY_ROOT) / "build" /
           ("u11-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    if (!session.setProjectDirectory(root.generic_string().c_str()) ||
        !library.connect(session))
      return false;
    session.initialize(&font, &icons);
    session.setSurface({0, 0, 1280, 720}, {});
    auto &g = session.document();
    body = g.createEntity(g.root(), runtime::ObjectKind::Folder, "Owner");
    auto e = *g.find(body);
    static_cast<scene::PhysicsBody *>(
        e.components.add(scene::PhysicsBody::descriptor))
        ->motion = scene::BodyMotion::Static;
    if (!g.applyEntityValues(body, e))
      return false;
    for (u32 n = 0; n < 2; ++n) {
      const auto id =
          g.createEntity(body, runtime::ObjectKind::Mesh,
                         n ? "Shared visual sibling" : "Editable collider");
      e = *g.find(id);
      e.components.add(scene::Collider::descriptor);
      if (!runtime::configurePrimitive(
              e, scene::PrimitiveType::Cube,
              {1, session.mapScene().assetGuid(0),
               session.mapScene().materialForAsset(0)}))
        return false;
      while (e.components.remove(scene::PhysicsBody::descriptor)) {
      }
      for (usize i = 0; i < e.components.size();)
        if (&e.components.at(i)->type() == &scene::Collider::descriptor &&
            e.components.at(i)->instanceId() != 1)
          e.components.removeInstance(e.components.at(i)->instanceId());
        else
          ++i;
      static_cast<scene::Collider *>(e.components.editInstance(1))->owner =
          body;
      e.transform.position[0] = n ? 3 : 0;
      if (!g.applyEntityValues(id, e))
        return false;
      if (n)
        sibling = id;
      else
        part = id;
    }
    const float eye[]{0, 0, -10};
    session.setCameraPose(eye, 0, 0);
    session.setSelection(part);
    session.update();
    return test::tapConvexWidget(session, font,
                                 EditorWidget::ComponentFoldBase);
  }
  bool tap(EditorWidget w) { return test::tapConvexWidget(session, font, w); }
  void pointer(ui::UiPoint p, u32 id = 90) {
    session.handlePointer({id, ui::UiPointerPhase::Down, p, 0});
    session.handlePointer({id, ui::UiPointerPhase::Up, p, .02});
    session.update();
  }
};
bool physicsRay(runtime::ScenePhysics &p, float x, runtime::QueryHit &hit) {
  const float origin[]{x, 3, 0}, direction[]{0, -6, 0};
  return p.rayCast(origin, direction, {}, hit);
}
u32 positiveFace(const ColliderTopology &d, u32 axis) {
  for (u32 f = 0; f < d.faces.size(); ++f) {
    bool found = true;
    for (auto v : d.faces[f])
      found = found && d.vertices[v][axis] > .49f;
    if (found)
      return f;
  }
  return ~0u;
}
} // namespace
AE_TEST(u11_topology_publish_isolated_uid_solver_history_and_reopen) {
  U11Fixture f;
  AE_EXPECT_TRUE(f.setup(), "real resource/Inspector fixture");
  auto &s = f.session;
  auto &g = s.document();
  std::string error;
  const auto before = serializeEditorDocument(g, 0);
  const auto visual = meshRenderer(*g.find(f.part))->asset;
  const auto sibling = *g.find(f.sibling);
  AE_EXPECT_TRUE(f.tap(EditorWidget::ColliderGeometryOpen),
                 "actual contextual creation path");
  AE_EXPECT_TRUE(s.colliderTopology().active() &&
                     s.colliderTopology().faces.size() == 6 &&
                     s.colliderTopology().vertices.size() == 8,
                 "physical cube welds into six real faces");
  const float point[]{0, 0, -.5f};
  f.pointer(projectWorldToScreen(s.view(), point).screen);
  AE_EXPECT_TRUE(s.colliderTopology().selected.size() == 1 &&
                     !s.colliderTopology().vertexMode,
                 "viewport picks the actual front face");
  AE_EXPECT_TRUE(
      s.selectColliderTopology(positiveFace(s.colliderTopology(), 0)),
      "choose side face");
  AE_EXPECT_TRUE(f.tap(EditorWidget::ColliderGeometryCoordinateX),
                 "real coordinate keyboard path");
  AE_EXPECT_TRUE(s.completeTextEdit(s.pendingTextEdit(), "1.5", true),
                 "numeric authoring edits selected face vertices");
  s.update();
  AE_EXPECT_TRUE(serializeEditorDocument(g, 0) == before &&
                     s.history().undoDepth() == 0,
                 "preview has no authored mutation");
  AE_EXPECT_TRUE(s.applyColliderTopology(error), error.c_str());
  s.update();
  const auto *c = inspectedCollider(g, f.part, 1);
  AE_EXPECT_TRUE(c && c->shape == scene::ColliderShape::Mesh && c->convex &&
                     c->collisionMesh.valid() && c->owner == f.body,
                 "immutable physics resource preserves UID/owner");
  AE_EXPECT_TRUE(meshRenderer(*g.find(f.part))->asset == visual &&
                     meshRenderer(*g.find(f.sibling))->asset == visual &&
                     g.find(f.sibling)->components.size() ==
                         sibling.components.size(),
                 "shared visual and sibling stay untouched");
  const auto published = serializeEditorDocument(g, 0);
  runtime::GameWorld world;
  runtime::ScenePhysics physics;
  test::ConvexMapGeometry geometry(s.mapScene());
  runtime::QueryHit hit;
  AE_EXPECT_TRUE(
      world.load(g) && physics.start(world, &geometry) &&
          physicsRay(physics, 1.3f, hit) && hit.object == f.body &&
          hit.colliderObject == f.part && hit.colliderInstance == 1,
      "actual edited Jolt shape reaches outside the untouched visual");
  physics.stop();
  AE_EXPECT_TRUE(
      s.history().undo(g) && serializeEditorDocument(g, 0) == before &&
          s.history().redo(g) && serializeEditorDocument(g, 0) == published,
      "one authored Undo/Redo restores exact archive");
  EditorDocument reloaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(published, 0, reloaded),
                 "scene archive roundtrip");
  EditorSession reopened;
  test::ConvexCpuLibrary library;
  AE_EXPECT_TRUE(
      library.connect(reopened) &&
          reopened.setProjectDirectory(f.root.generic_string().c_str()) &&
          reopened.loadAssets(s.serializeAssets()),
      "fresh library and persistent registry");
  std::vector<EditorSession::ReopenedSource> sources;
  for (const auto &record : s.assets().records())
    if (record.type == resources::AssetType::Mesh) {
      std::vector<u8> bytes;
      resources::GltfImport model;
      AE_EXPECT_TRUE(
          EditorImportTransaction::read(
              f.root / EditorImportTransaction::fromUtf8(record.path), bytes) &&
              resources::importGlb(bytes, {}, {}, model),
          "saved immutable GLB is parsable");
      sources.push_back({std::move(model), record.contentHash, record.path});
    }
  std::vector<EditorSession::ModelImportReport> reports;
  AE_EXPECT_TRUE(reopened.reopenSources(sources, reports, error),
                 error.c_str());
  test::ConvexMapGeometry freshGeometry(reopened.mapScene());
  AE_EXPECT_TRUE(world.load(reloaded) && physics.start(world, &freshGeometry) &&
                     physicsRay(physics, 1.3f, hit) &&
                     hit.colliderInstance == 1,
                 "reopened resource still drives physical queries");
  physics.stop();
  const auto shared = inspectedCollider(g, f.part, 1)->collisionMesh;
  auto other = *g.find(f.sibling);
  auto *otherShape = static_cast<scene::Collider *>(other.components.editInstance(1));
  otherShape->shape = scene::ColliderShape::Mesh;
  otherShape->convex = true;
  otherShape->meshLocalPose = true;
  otherShape->collisionMesh = shared;
  AE_EXPECT_TRUE(g.applyEntityValues(f.sibling, other), "shared physical resource");
  const auto beforeVertex = serializeEditorDocument(g, 0);
  AE_EXPECT_TRUE(s.beginColliderTopology(f.part, 1, error), error.c_str());
  AE_EXPECT_TRUE(f.tap(EditorWidget::ColliderGeometryVertex), "vertex mode");
  u32 corner = ~0u;
  for (u32 n = 0; n < s.colliderTopology().vertices.size(); ++n) {
    const auto &p = s.colliderTopology().vertices[n];
    if (p[0] > 1.49f && p[1] > .49f && p[2] < -.49f) corner = n;
  }
  AE_EXPECT_TRUE(corner != ~0u && s.selectColliderTopology(corner) &&
                     s.editColliderTopologyCoordinate(0, 2.5f, error) &&
                     s.applyColliderTopology(error), error.c_str());
  AE_EXPECT_TRUE(inspectedCollider(g, f.part, 1)->collisionMesh != shared &&
                     inspectedCollider(g, f.sibling, 1)->collisionMesh == shared &&
                     meshRenderer(*g.find(f.part))->asset == visual,
                 "vertex publication forks shared collision, preserving sibling and visual");
  test::ConvexMapGeometry editedGeometry(s.mapScene());
  const float vertexOrigin[]{2.2f, 3, -.4f}, down[]{0, -6, 0};
  AE_EXPECT_TRUE(world.load(g) && physics.start(world, &editedGeometry) &&
                     physics.rayCast(vertexOrigin, down, {}, hit) && hit.colliderObject == f.part &&
                     physicsRay(physics, 4.3f, hit) && hit.colliderObject == f.sibling,
                 "new vertex hull and unchanged shared sibling both reach the solver");
  physics.stop();
  AE_EXPECT_TRUE(s.history().undo(g) && serializeEditorDocument(g, 0) == beforeVertex,
                 "vertex publication is one reversible scene change");
}
AE_TEST(u11_topology_vertex_cancel_stale_occlusion_gesture_and_compact) {
  U11Fixture f;
  AE_EXPECT_TRUE(f.setup(), "real UI fixture");
  auto &s = f.session;
  auto &g = s.document();
  std::string error;
  const auto original = serializeEditorDocument(g, 0);
  AE_EXPECT_TRUE(s.beginColliderTopology(f.part, 1, error) &&
                     f.tap(EditorWidget::ColliderGeometryVertex),
                 "vertex mode");
  const auto vertex = s.colliderTopology().vertices[0];
  float pose[16];
  AE_EXPECT_TRUE(colliderTopologyPose(g, f.part, 1, pose),
                 "physical local frame");
  auto p = topologyWorldPoint(pose, vertex);
  f.pointer(projectWorldToScreen(s.view(), p.data()).screen);
  AE_EXPECT_TRUE(s.colliderTopology().selected.size() == 1,
                 "actual visible vertex pointer");
  const auto base = s.colliderTopology().vertices;
  AE_EXPECT_TRUE(s.editColliderTopologyCoordinate(0, -1, error) &&
                     s.colliderTopology().vertices != base,
                 "selected vertex affects candidate geometry");
  AE_EXPECT_TRUE(f.tap(EditorWidget::ColliderGeometryUndo) &&
                     s.colliderTopology().vertices == base &&
                     f.tap(EditorWidget::ColliderGeometryRedo),
                 "draft Undo/Redo without scene changes");
  s.cancelColliderTopology();
  AE_EXPECT_TRUE(serializeEditorDocument(g, 0) == original &&
                     s.history().undoDepth() == 0,
                 "cancel leaves source and history intact");
  AE_EXPECT_TRUE(
      s.beginColliderTopology(f.part, 1, error) &&
          s.selectColliderTopology(positiveFace(s.colliderTopology(), 0)),
      "face gesture target");
  s.setSurface({0, 0, 800, 400}, {});
  s.update();
  ui::UiInputRouter router;
  ui::UiDrawList list;
  list.begin(s.screen().surface, f.font.metrics(ui::UiFontWeight::Regular));
  buildEditorScreen(s.screen(), ui::defaultTheme(), list, router);
  ui::UiPoint grip{};
  bool found = false;
  for (float y = 2; y < 400 && !found; y += 4)
    for (float x = 2; x < 800; x += 4)
      if (router.hitTest({x, y}).widgetId ==
          widgetId(EditorWidget::ColliderGeometryAxisX)) {
        grip = {x, y};
        found = true;
        break;
      }
  AE_EXPECT_TRUE(found, "compact X grip reachable");
  const auto initial = s.colliderTopology().vertices;
  s.handlePointer({92, ui::UiPointerPhase::Down, grip, 0});
  s.handlePointer({92, ui::UiPointerPhase::Move, {grip.x + 35, grip.y}, .1});
  s.handlePointer({93, ui::UiPointerPhase::Down, {8, 8}, .11});
  s.handlePointer({93, ui::UiPointerPhase::Up, {8, 8}, .12});
  s.cancelPointers();
  s.update();
  AE_EXPECT_TRUE(
      s.colliderTopology().vertices == initial &&
          serializeEditorDocument(g, 0) == original,
      "exclusive gesture/focus cancel restores candidate and preserves scene");
  test::UiSoftwareTarget image;
  image.resize(800, 400, .06f, .07f, .09f);
  const auto &im = s.immediateGui();
  test::rasterizeUi(s.instances(), f.font, f.icons, image, im.atlas(),
                    im.atlasWidth(), im.atlasHeight());
  std::ofstream capture(std::filesystem::path(AETHER_REPOSITORY_ROOT) /
                            "build/u11-topology-compact.ppm",
                        std::ios::binary);
  capture << "P6\n800 400\n255\n";
  for (usize i = 0; i < image.pixels.size(); i += 4)
    for (u32 c = 0; c < 3; ++c)
      capture.put(static_cast<char>(
          std::clamp(image.pixels[i + c], 0.f, 1.f) * 255 + .5f));
  AE_EXPECT_TRUE(capture.good(), "real compact raster capture");
  s.cancelColliderTopology();
  const auto wall = test::addOcclusionWall(s, f.part);
  AE_EXPECT_TRUE(wall && s.beginColliderTopology(f.part, 1, error),
                 "foreground occluder");
  s.setSurface({0, 0, 1280, 720}, {});
  s.update();
  const float center[]{0, 0, -.5f};
  f.pointer(projectWorldToScreen(s.view(), center).screen);
  AE_EXPECT_TRUE(
      s.colliderTopology().selected.empty() &&
          f.tap(EditorWidget::ColliderGeometryHidden),
      "foreign geometry blocks face unless explicit hidden selection");
  f.pointer(projectWorldToScreen(s.view(), center).screen);
  AE_EXPECT_TRUE(s.colliderTopology().selected.size() == 1,
                 "explicit hidden mode permits picking");
  auto changed = *g.find(f.part);
  changed.transform.position[1] = 1;
  g.applyEntityValues(f.part, changed);
  AE_EXPECT_TRUE(!s.applyColliderTopology(error),
                 "stale preview cannot publish");
  s.update();
  AE_EXPECT_TRUE(!s.colliderTopology().active(), "changed scene closes draft");
}
AE_TEST(u11_physics_diagnostic_com_support_authority_cache_and_lifecycle) {
  U11Fixture f;
  AE_EXPECT_TRUE(f.setup(), "fixture");
  auto &s = f.session;
  auto &g = s.document();
  const auto floor =
      g.createEntity(g.root(), runtime::ObjectKind::Folder, "Ground");
  auto ground = *g.find(floor);
  ground.transform.position[1] = -.6f;
  static_cast<scene::PhysicsBody *>(ground.components.add(scene::PhysicsBody::descriptor))->motion = scene::BodyMotion::Static;
  auto *shape = static_cast<scene::Collider *>(
      ground.components.add(scene::Collider::descriptor));
  shape->halfX = shape->halfZ = 20;
  shape->halfY = .1f;
  g.applyEntityValues(floor, ground);
  runtime::GameWorld world;
  runtime::ScenePhysics physics;
  AE_EXPECT_TRUE(world.load(g) && physics.start(world), physics.error().c_str());
  runtime::ScenePhysics::Diagnostic d;
  AE_EXPECT_TRUE(physics.diagnostic(world, f.body, .5f, d) && d.hasBody &&
                     d.colliderCount == 2 && d.body.centerOfMass.x > 1 &&
                     d.hasSupport && d.support == floor && !d.motorSupport &&
                     d.authority == runtime::TransformAuthority::PhysicsBody,
                 "real asymmetric COM, geometry probes and query support, "
                 "static authority follows the actual world ownership contract");
  auto owner = *g.find(f.body);
  auto *body = static_cast<scene::PhysicsBody *>(
      owner.components.edit(scene::PhysicsBody::descriptor));
  body->motion = scene::BodyMotion::Dynamic;
  body->freezeRotation[0] = body->freezeRotation[1] = body->freezeRotation[2] =
      true;
  owner.components.add(scene::DynamicBodyMotor::descriptor);
  g.applyEntityValues(f.body, owner);
  physics.stop();
  AE_EXPECT_TRUE(world.load(g) && physics.start(world) &&
                     physics.diagnostic(world, f.body, .5f, d) &&
                     d.authority == runtime::TransformAuthority::PhysicsBody &&
                     !d.motorSupport,
                 "dynamic authority and pre-step query distinction");
  for (u32 n = 0; n < 12; ++n)
    AE_EXPECT_TRUE(physics.advance(1. / 60, world), "real motor support steps");
  runtime::ScenePhysics::DynamicMotorState motor;
  AE_EXPECT_TRUE(physics.dynamicMotorState(f.body, motor) &&
                     motor.hasMeasuredStep && motor.grounded &&
                     physics.diagnostic(world, f.body, .5f, d) &&
                     d.motorSupport && d.hasSupport &&
                     std::abs(d.supportPoint.y + .5f) < .03f,
                 "observed motor point comes from its accepted hit");
  physics.stop();
  s.update();
  AE_EXPECT_TRUE(f.tap(EditorWidget::PhysicsDiagnosticOpen),
                 "real Inspector diagnostic route");
  const auto builds = s.screen().physicsDiagnosticBuilds;
  AE_EXPECT_TRUE(s.screen().physicsDiagnosticHasCom &&
                     !s.screen().physicsDiagnosticLive && builds == 1,
                 "authoring preflight without running scripts");
  for (u32 n = 0; n < 20; ++n)
    s.update();
  AE_EXPECT_TRUE(s.screen().physicsDiagnosticBuilds == builds,
                 "idle inspection reuses solver; no per-frame cooking");
  AE_EXPECT_TRUE(f.tap(EditorWidget::PhysicsDiagnosticClose), "exit route");
  for (u32 n = 0; n < 20; ++n)
    s.update();
  AE_EXPECT_TRUE(s.screen().physicsDiagnosticBuilds == builds,
                 "closed tool never builds a world");
  const auto player = g.createEntity(g.root(), runtime::ObjectKind::Folder,
                                     "Independent character");
  auto e = *g.find(player);
  e.transform.position[0] = 10;
  e.transform.position[1] = 2;
  e.components.add(scene::Character::descriptor);
  g.applyEntityValues(player, e);
  AE_EXPECT_TRUE(
      world.load(g) && physics.start(world) &&
          physics.diagnostic(world, player, .5f, d) && d.hasCharacter &&
          d.authority == runtime::TransformAuthority::Character && !d.hasBody &&
          d.character.hasCapsule && std::abs(d.character.capsuleRadius-.45f)<.001f &&
          std::abs(d.character.capsuleTop.y-d.character.capsuleBottom.y-1.1f)<.001f,
      "Character authority is independent of shape/name and never receives a "
      "fake COM");
  for (u32 n=0;n<120;++n)
    AE_EXPECT_TRUE(physics.advance(1./60, world), "real Character grounding");
  AE_EXPECT_TRUE(physics.diagnostic(world, player, .5f, d) && d.hasSupport &&
                     d.support == floor && d.motorSupport && d.character.hasMeasuredStep,
                 "Character support position and owner come from actual Jolt contacts");
  physics.stop();
  const auto cameraId=g.createEntity(g.root(),runtime::ObjectKind::Camera,"Actual Play camera");
  e=*g.find(cameraId);e.transform.position[1]=1;e.transform.position[2]=-8;e.transform.rotationDegrees[2]=31;
  auto *camera=static_cast<scene::Camera *>(e.components.add(scene::Camera::descriptor));
  camera->projection=scene::CameraProjection::Orthographic;camera->orthographicHalfHeight=5;
  AE_EXPECT_TRUE(g.applyEntityValues(cameraId,e), "authored runtime camera");
  s.setSelection(f.part);s.update();
  if(s.screen().expandedNative!=1)AE_EXPECT_TRUE(f.tap(EditorWidget::ComponentFoldBase),"open Collider");
  AE_EXPECT_TRUE(f.tap(EditorWidget::PhysicsDiagnosticOpen) &&
                     f.tap(EditorWidget::PlayFromTopBar), "actual diagnostic-to-Play workflow");
  std::vector<renderer::MapDrawState> draws;
  AE_EXPECT_TRUE(s.extractPlayMap(draws) && f.tap(EditorWidget::PlayInspect) &&
                     f.tap(EditorWidget::PhysicsDiagnosticRefresh), "live read-only inspection");
  s.update();
  AE_EXPECT_TRUE(s.screen().physicsDiagnosticLive && s.screen().physicsDiagnosticHasCom &&
                     renderer::isOrthographic(s.view().frustum) &&
                     std::abs(s.view().frustum.cameraPosition[2]+8)<.001f,
                 "diagnostics use the renderer's actual runtime camera/projection");
  ui::UiDrawList list;ui::UiInputRouter router;
  list.begin(s.screen().surface,f.font.metrics(ui::UiFontWeight::Regular));router.beginFrame();
  buildEditorScreen(s.screen(),ui::defaultTheme(),list,router);
  const auto projected=projectWorldToScreen(s.view(),s.screen().physicsDiagnosticCom);
  bool comMarker=false;
  for(const auto &command:list.commands())
    if(command.kind==ui::UiPrimitive::Image && command.image==static_cast<ui::UiImageId>(ui::UiIcon::PhysicsDiagnostic) &&
       command.color==ui::defaultTheme().color.warning && projected.valid &&
       std::abs(command.bounds.x+12-projected.screen.x)<.01f &&
       std::abs(command.bounds.y+12-projected.screen.y)<.01f)comMarker=true;
  AE_EXPECT_TRUE(comMarker, "Play actually draws solver COM at the runtime projection, not only Inspector fields");
}
AE_TEST(u11_scale_full_geometry_budget_and_256_part_solver) {
  using Clock = std::chrono::steady_clock;
  std::vector<EditorPickMesh::Triangle> triangles;
  triangles.reserve(100000);
  for (u32 y = 0; y < 200; ++y)
    for (u32 x = 0; x < 250; ++x) {
      const float a = float(x), b = float(y);
      triangles.push_back({a, 0, b, a + 1, 0, b, a, 0, b + 1});
      triangles.push_back({a + 1, 0, b, a + 1, 0, b + 1, a, 0, b + 1});
    }
  ColliderTopology draft;
  std::string error;
  AE_EXPECT_TRUE(draft.build(triangles, error) && draft.faces.size() == 1 &&
                     draft.indices.size() == 300000 && draft.validate(.001f),
                 "100k triangles remain whole, welded, and selectable despite "
                 "drawing budget");
  const float origin[]{125.2f, 2, 100.3f}, direction[]{0, -1, 0},
      identity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  float distance;
  u32 ordinal=~0u;
  const auto start = Clock::now();
  AE_EXPECT_TRUE(
      draft.surface.intersect(origin, direction, identity, distance,0,10,&ordinal) &&
          std::abs(distance - 2) < .001f && (ordinal==50250 || ordinal==50251) &&
          draft.faceOfTriangle[ordinal]==0 && draft.surface.triangles()[ordinal]==triangles[ordinal],
      "full geometry BVH preserves source triangle/face identity, not sampled contour");
  std::printf("U11_SCALE mesh triangles=%zu vertices=%zu faces=%zu "
              "build_ms=%.3f validate_ms=%.3f bvh_pick_ms=%.3f\n",
              triangles.size(), draft.vertices.size(), draft.faces.size(),
              draft.buildMs, draft.validateMs, ColliderTopology::ms(start));
  triangles.push_back(triangles.back());
  ColliderTopology refused;
  AE_EXPECT_TRUE(!refused.build(triangles, error) && !error.empty(),
                 "over-budget source is explicitly refused without truncation");
  for (u32 count : {1u, 16u, 64u, 256u}) {
    EditorDocument g;
    const auto id = g.createEntity(g.root(), runtime::ObjectKind::Folder,
                                   "Scaled compound");
    auto owner = *g.find(id);
    owner.components.add(scene::PhysicsBody::descriptor);
    g.applyEntityValues(id, owner);
    for (u32 n = 0; n < count; ++n) {
      const auto child =
          g.createEntity(id, runtime::ObjectKind::Folder, "Part");
      auto e = *g.find(child);
      auto *c = static_cast<scene::Collider *>(
          e.components.add(scene::Collider::descriptor));
      c->owner = id;
      c->shape = scene::ColliderShape::Cylinder;
      c->centerX = float(n % 16) * 2;
      c->centerZ = float(n / 16) * 2;
      g.applyEntityValues(child, e);
    }
    runtime::GameWorld world;
    runtime::ScenePhysics physics;
    const auto begin = Clock::now();
    AE_EXPECT_TRUE(world.load(g) && physics.start(world),
                   "real compound build at scale");
    const double build = ColliderTopology::ms(begin);
    runtime::ScenePhysics::Diagnostic d;
    const auto query = Clock::now();
    AE_EXPECT_TRUE(
        physics.diagnostic(world, id, .5f, d) && d.colliderCount == count &&
            d.probeCount <= AetherBodyGroundProbeCapacityV1,
        "all physical parts represented in solver, bounded support query");
    const double queryMs = ColliderTopology::ms(query);
    const auto viewBegin = Clock::now();
    const auto visuals = collectComponentVisuals(g, id, 2, nullptr, 0, 0, 0, 1);
    usize segments = 0;
    for (const auto &v : visuals)
      if (v.icon == ui::UiIcon::ComponentCollider)
        segments += v.segments.size();
    const auto viewMs = ColliderTopology::ms(viewBegin);
    AE_EXPECT_TRUE(segments <= MaximumVisibleColliderSegments,
                   "drawing alone respects aggregate budget");
    std::printf("U11_SCALE parts=%u solver_build_ms=%.3f diagnostic_ms=%.3f "
                "contours_ms=%.3f segments=%zu probes=%u\n",
                count, build, queryMs, viewMs, segments, d.probeCount);
  }
}
AE_TEST(u11_concave_topology_cavity_and_disabled_invalid_publication) {
  U11Fixture f;
  AE_EXPECT_TRUE(f.setup(), "resource fixture");
  auto &s=f.session; auto &g=s.document(); std::string error;
  const auto id=test::convexFixtureObject(s,error);
  AE_EXPECT_TRUE(id, error.c_str());
  auto e=*g.find(id);
  const auto visual=meshRenderer(e)->asset;
  e.transform.position[0]=10;
  static_cast<scene::PhysicsBody *>(e.components.add(scene::PhysicsBody::descriptor))->motion=scene::BodyMotion::Static;
  auto *c=static_cast<scene::Collider *>(e.components.add(scene::Collider::descriptor));
  const auto uid=c->instanceId();
  c->shape=scene::ColliderShape::Mesh; c->convex=false; c->meshLocalPose=true; c->collisionMesh=visual;
  AE_EXPECT_TRUE(g.applyEntityValues(id,e) && s.beginColliderTopology(id,uid,error) &&
                     f.tap(EditorWidget::ColliderGeometryVertex), error.c_str());
  bool selected=false;
  for(u32 v=0;v<s.colliderTopology().vertices.size();++v) {
    const auto &p=s.colliderTopology().vertices[v];
    if(p[0]>.9f && p[2]>1.99f) {
      AE_EXPECT_TRUE(s.selectColliderTopology(v,selected), "select entire concave arm end");
      selected=true;
    }
  }
  AE_EXPECT_TRUE(selected && s.editColliderTopologyCoordinate(2,3,error) &&
                     s.applyColliderTopology(error), error.c_str());
  AE_EXPECT_TRUE(meshRenderer(*g.find(id))->asset==visual &&
                     inspectedCollider(g,id,uid)->collisionMesh!=visual &&
                     !inspectedCollider(g,id,uid)->convex,
                 "concave topology publication preserves visual and concavity");
  test::ConvexMapGeometry geometry(s.mapScene()); runtime::GameWorld world;
  runtime::ScenePhysics physics; runtime::QueryHit hit;
  const float arm[]{11.5f,3,2.6f}, cavity[]{10,3,0}, down[]{0,-6,0};
  AE_EXPECT_TRUE(world.load(g) && physics.start(world,&geometry) &&
                     physics.rayCast(arm,down,{},hit) && hit.colliderObject==id &&
                     !physics.rayCast(cavity,down,{},hit),
                 "full triangle solver gains edited arm while preserving cavity");
  physics.stop();
  e=*g.find(id); e.active=false;
  static_cast<scene::PhysicsBody *>(e.components.edit(scene::PhysicsBody::descriptor))->motion=scene::BodyMotion::Dynamic;
  static_cast<scene::Collider *>(e.components.editInstance(uid))->enabled=false;
  AE_EXPECT_TRUE(g.applyEntityValues(id,e) && s.beginColliderTopology(id,uid,error), error.c_str());
  const auto sceneBefore=serializeEditorDocument(g,0), assetsBefore=s.serializeAssets();
  AE_EXPECT_TRUE(!s.applyColliderTopology(error) && !error.empty() &&
                     serializeEditorDocument(g,0)==sceneBefore && s.serializeAssets()==assetsBefore,
                 "disabled/inactive concave dynamic shapes cannot evade solver validation or publish partial state");
}
