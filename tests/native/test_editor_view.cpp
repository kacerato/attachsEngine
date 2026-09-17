#include "editor/editor_view.h"
#include "editor/editor_camera.h"
#include "editor/editor_grid.h"
#include "editor/editor_gizmo.h"
#include "harness.h"

#include <cmath>
#include <vector>

using namespace ae;
using namespace ae::editor;
using namespace ae::renderer;

namespace {

// Câmera na origem olhando para +Z, que é a direção "para frente" do espaço de
// vista desta engine (ver a nota de convenção em editor_view.h).
EditorViewport viewportLookingForward(float width = 1600.0f, float height = 900.0f) {
  EditorViewport viewport{};
  const float position[3] = {0.0f, 0.0f, 0.0f};
  PerspectiveVisibilitySettings settings{};
  settings.nearPlane = 0.1f;
  settings.farPlane = 1000.0f;
  viewport.frustum = buildPerspectiveFrustum(position, 0.0f, 0.0f, width / height, settings);
  viewport.rect = {0.0f, 0.0f, width, height};
  return viewport;
}

bool nearlyEqual(float first, float second, float tolerance = 0.05f) {
  return std::fabs(first - second) <= tolerance;
}

} // namespace

AE_TEST(camera_clip_selection_skips_hidden_objects_and_keeps_visible_mesh_surface) {
  for(auto mode:{CameraProjection::Perspective,CameraProjection::Orthographic}) {
    auto view=viewportLookingForward(800,400);
    PerspectiveVisibilitySettings settings;settings.projection=mode;settings.nearPlane=2;settings.farPlane=10;
    const float origin[3]{};view.frustum=buildPerspectiveFrustum(origin,0,0,2,settings);
    auto ray=screenPointToRay(view,{400,200});
    std::vector<EditorPickCandidate> candidates(3);
    for(u32 i=0;i<3;++i) {candidates[i].id=i+1;candidates[i].radius=.2f;candidates[i].center[2]=i==0?1.f:i==1?5.f:12.f;}
    AE_EXPECT_EQ(pickNearest(candidates,ray).id,2u,"clipped front object does not intercept touch");
    candidates[1].selectable=false;
    AE_EXPECT_TRUE(!pickNearest(candidates,ray).hit,"nothing selectable inside interval");
    auto mesh=std::make_shared<EditorPickMesh>();
    AE_EXPECT_TRUE(mesh->build({{-1,-1,1,1,-1,1,0,1,1},{-1,-1,6,1,-1,6,0,1,6}}),"two surfaces in one mesh");
    candidates.resize(1);candidates[0].mesh=mesh;candidates[0].center[2]=3.5f;candidates[0].radius=4;
    const auto hit=pickNearest(candidates,ray);
    AE_EXPECT_TRUE(hit.hit && nearlyEqual(hit.distance,6,.001f),"visible second surface survives clipped first surface");
    ray.maximumDistance=5;
    AE_EXPECT_TRUE(!pickNearest(candidates,ray).hit,"triangle beyond far is rejected");
  }
}

AE_TEST(camera_clip_selection_uses_view_depth_for_oblique_rays) {
  auto view=viewportLookingForward(800,400);
  const auto ray=screenPointToRay(view,{790,200});
  AE_EXPECT_TRUE(ray.valid && ray.maximumDistance>view.frustum.farPlane,"oblique ray travel exceeds axial depth");
  AE_EXPECT_TRUE(nearlyEqual(ray.maximumDistance*ray.direction[2],view.frustum.farPlane,.001f),"far boundary is a plane, not sphere");
  AE_EXPECT_TRUE(nearlyEqual(ray.minimumDistance*ray.direction[2],view.frustum.nearPlane,.001f),"near boundary matches renderer");
}

AE_TEST(orthographic_projection_keeps_size_and_parallel_pick_rays) {
  auto view=viewportLookingForward(800,400);
  PerspectiveVisibilitySettings settings;
  settings.projection=CameraProjection::Orthographic;settings.orthographicHalfHeight=2;
  settings.nearPlane=.1f;settings.farPlane=100;settings.boundsScale=1;settings.boundsMargin=0;
  const float origin[3]{};
  view.frustum=buildPerspectiveFrustum(origin,0,0,2,settings);
  const float nearPoint[3]{2,1,5},farPoint[3]{2,1,50};
  const auto a=projectWorldToScreen(view,nearPoint),b=projectWorldToScreen(view,farPoint);
  AE_EXPECT_TRUE(a.valid && b.valid,"both distances project");
  AE_EXPECT_TRUE(nearlyEqual(a.screen.x,600)&&nearlyEqual(a.screen.y,100),"extent in scene units");
  AE_EXPECT_TRUE(nearlyEqual(a.screen.x,b.screen.x)&&nearlyEqual(a.screen.y,b.screen.y),"distance does not change size");
  const auto ray=screenPointToRay(view,a.screen),centre=screenPointToRay(view,{400,200});
  AE_EXPECT_TRUE(ray.valid && centre.valid,"valid parallel rays");
  AE_EXPECT_TRUE(nearlyEqual(ray.origin[0],2)&&nearlyEqual(ray.origin[1],1),"origin follows screen point");
  AE_EXPECT_TRUE(nearlyEqual(ray.direction[0],centre.direction[0])&&nearlyEqual(ray.direction[2],1),"parallel direction");
  std::vector<EditorPickCandidate> candidates(2);
  for(u32 i=0;i<2;++i) {candidates[i].id=i+1;candidates[i].selectable=true;candidates[i].radius=.25f;
    candidates[i].center[0]=2;candidates[i].center[1]=1;candidates[i].center[2]=i?50:5;}
  const auto pick=pickNearest(candidates,ray);
  AE_EXPECT_TRUE(pick.hit && pick.id==1,"parallel ray selects nearest actual object");
}

AE_TEST(orthographic_rotated_view_roundtrips_and_interpolates_ray_origins) {
  auto view=viewportLookingForward(800,400);
  PerspectiveVisibilitySettings settings;settings.projection=CameraProjection::Orthographic;
  settings.orthographicHalfHeight=3;settings.roll=.6f;
  const float position[3]{4,8,-3};view.frustum=buildPerspectiveFrustum(position,.7f,-.3f,2,settings);
  view.surfaceTransform={0,-1,1,0};
  const ui::UiPoint pixel{570,140};const auto ray=screenPointToRay(view,pixel);
  float point[3];for(u32 k=0;k<3;++k) point[k]=ray.origin[k]+ray.direction[k]*20;
  const auto projected=projectWorldToScreen(view,point);
  AE_EXPECT_TRUE(projected.valid && nearlyEqual(projected.screen.x,pixel.x)&&nearlyEqual(projected.screen.y,pixel.y),"roll and display pre-rotation share inverse");
  const float triangle[3][2]{{-1,-1},{3,-1},{-1,3}};
  const auto interpolated=cameraRayIsAffineInNdc(view.frustum,view.surfaceTransform,triangle,.5f,.25f,.25f);
  const auto direct=cameraRayFromNdc(view.frustum,view.surfaceTransform,0,0);
  for(u32 k=0;k<3;++k) {
    AE_EXPECT_TRUE(nearlyEqual(interpolated.originOffset[k],direct.originOffset[k]),"origins interpolate affinely");
    AE_EXPECT_TRUE(nearlyEqual(interpolated.direction[k],direct.direction[k]),"directions remain parallel");
  }
}

AE_TEST(orthographic_clipping_culling_and_hzb_use_box_and_linear_depth) {
  auto view=viewportLookingForward(800,400);
  PerspectiveVisibilitySettings settings;settings.projection=CameraProjection::Orthographic;
  settings.orthographicHalfHeight=2;settings.nearPlane=1;settings.farPlane=101;
  settings.boundsScale=1;settings.boundsMargin=0;const float position[3]{};
  view.frustum=buildPerspectiveFrustum(position,0,0,2,settings);
  const float inside[3]{0,0,51},outside[3]{5,0,90},behind[3]{0,0,110};
  AE_EXPECT_TRUE(isSphereVisible(view.frustum,inside,.2f),"inside box");
  AE_EXPECT_TRUE(!isSphereVisible(view.frustum,outside,.2f),"far depth does not widen box");
  AE_EXPECT_TRUE(!isSphereVisible(view.frustum,behind,.2f),"far clipping");
  ui::UiPoint a,b;const float left[3]{-20,0,50},right[3]{20,0,50};
  AE_EXPECT_TRUE(projectSegmentToScreen(view,left,right,a,b),"clip crossing segment");
  AE_EXPECT_TRUE(nearlyEqual(a.x,0)&&nearlyEqual(b.x,800),"side clipping at constant width");
  const float otherBehind[3]{1,0,110};
  AE_EXPECT_TRUE(!projectSegmentToScreen(view,behind,otherBehind,a,b),"reject segments beyond far");
  const auto rect=projectBoundsToHzbScreenRect(view.frustum,inside,1,{});
  AE_EXPECT_TRUE(rect.valid && nearlyEqual(rect.maxU-rect.minU,.25f,.001f),"orthographic screen bounds");
  AE_EXPECT_TRUE(nearlyEqual(rect.nearDepth,.49f,.001f),"linear nearest depth");
  AE_EXPECT_TRUE(nearlyEqual(cameraNormalizedDepth(view.frustum,51),.5f,.001f),"middle depth is half");
  settings.orthographicHalfHeight=0;
  AE_EXPECT_TRUE(!buildPerspectiveFrustum(position,0,0,2,settings).valid,"zero extent rejected");
}

AE_TEST(orthographic_gizmo_and_grid_scale_follow_extent_not_distance) {
  auto view=viewportLookingForward(800,400);
  PerspectiveVisibilitySettings settings;settings.projection=CameraProjection::Orthographic;
  settings.orthographicHalfHeight=4;const float position[3]{0,10,0};
  view.frustum=buildPerspectiveFrustum(position,0,.5f,2,settings);
  const auto ray=screenPointToRay(view,{400,200});
  float nearPoint[3],farPoint[3];
  for(u32 k=0;k<3;++k) {nearPoint[k]=position[k]+ray.direction[k]*5;farPoint[k]=position[k]+ray.direction[k]*50;}
  const auto nearGizmo=buildGizmoFrame(view,nearPoint,{}),farGizmo=buildGizmoFrame(view,farPoint,{});
  AE_EXPECT_TRUE(nearGizmo.valid && farGizmo.valid,"gizmos at both depths");
  AE_EXPECT_TRUE(nearlyEqual(nearGizmo.axisWorldLength,farGizmo.axisWorldLength,.0001f),"constant projected size");
  const auto closeGrid=buildEditorGridPlan(view);
  view.frustum.cameraPosition[1]=100;
  const auto farGrid=buildEditorGridPlan(view);
  AE_EXPECT_TRUE(closeGrid.enabled && farGrid.enabled,"grid policies valid");
  AE_EXPECT_TRUE(nearlyEqual(closeGrid.minorSpacing,farGrid.minorSpacing,.0001f)&&
                 nearlyEqual(closeGrid.minorOpacity,farGrid.minorOpacity,.0001f),"cell scale independent of camera altitude");
}

AE_TEST(view_projects_the_point_straight_ahead_to_the_centre) {
  const EditorViewport viewport = viewportLookingForward();
  const float ahead[3] = {0.0f, 0.0f, 10.0f};
  const EditorProjectedPoint projected = projectWorldToScreen(viewport, ahead);
  AE_EXPECT_TRUE(projected.valid, "");
  AE_EXPECT_TRUE(nearlyEqual(projected.screen.x, 800.0f), "");
  AE_EXPECT_TRUE(nearlyEqual(projected.screen.y, 450.0f), "");
  AE_EXPECT_TRUE(nearlyEqual(projected.viewDepth, 10.0f), "profundidade em unidades de mundo");
}

AE_TEST(view_up_in_the_world_is_up_on_the_screen) {
  // O eixo Y do mundo aponta para cima e o Y da tela cresce para baixo. Errar
  // este sinal faz a seta verde do gizmo apontar para o chao.
  const EditorViewport viewport = viewportLookingForward();
  const float above[3] = {0.0f, 2.0f, 10.0f};
  const EditorProjectedPoint projected = projectWorldToScreen(viewport, above);
  AE_EXPECT_TRUE(projected.valid, "");
  AE_EXPECT_TRUE(projected.screen.y < 450.0f, "acima no mundo e acima na tela");
  AE_EXPECT_TRUE(nearlyEqual(projected.screen.x, 800.0f), "e nao desloca lateralmente");
}

AE_TEST(view_right_in_the_world_is_right_on_the_screen) {
  const EditorViewport viewport = viewportLookingForward();
  const float right[3] = {2.0f, 0.0f, 10.0f};
  const EditorProjectedPoint projected = projectWorldToScreen(viewport, right);
  AE_EXPECT_TRUE(projected.valid && projected.screen.x > 800.0f, "");
}

AE_TEST(view_refuses_to_project_what_is_behind_the_near_plane) {
  // Um gizmo desenhado atras da camera apareceria espelhado e responderia ao
  // contrario do arraste.
  const EditorViewport viewport = viewportLookingForward();
  const float behind[3] = {0.0f, 0.0f, -5.0f};
  AE_EXPECT_TRUE(!projectWorldToScreen(viewport, behind).valid, "");
  const float atCamera[3] = {0.0f, 0.0f, 0.0f};
  AE_EXPECT_TRUE(!projectWorldToScreen(viewport, atCamera).valid, "");
}

AE_TEST(view_ray_and_projection_are_inverses) {
  // A propriedade que sustenta a selecao: o raio que sai do pixel de um objeto
  // tem de acertar aquele objeto.
  const EditorViewport viewport = viewportLookingForward();
  const float points[][3] = {{0, 0, 10}, {3, -2, 25}, {-7, 4, 12}, {1.5f, 0.5f, 40}};
  for (const auto &point : points) {
    const EditorProjectedPoint projected = projectWorldToScreen(viewport, point);
    AE_EXPECT_TRUE(projected.valid, "");
    const EditorRay ray = screenPointToRay(viewport, projected.screen);
    AE_EXPECT_TRUE(ray.valid, "");
    const float distance = distanceToCamera(viewport, point);
    for (u32 axis = 0; axis < 3; ++axis) {
      const float reconstructed = ray.origin[axis] + ray.direction[axis] * distance;
      AE_EXPECT_TRUE(nearlyEqual(reconstructed, point[axis], 0.01f),
                     "o raio do pixel reconstroi o ponto original");
    }
  }
}

AE_TEST(view_ray_from_the_centre_points_forward) {
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  AE_EXPECT_TRUE(ray.valid, "");
  AE_EXPECT_TRUE(nearlyEqual(ray.direction[0], 0.0f, 0.001f), "");
  AE_EXPECT_TRUE(nearlyEqual(ray.direction[1], 0.0f, 0.001f), "");
  AE_EXPECT_TRUE(nearlyEqual(ray.direction[2], 1.0f, 0.001f), "");
}

AE_TEST(view_rotating_the_camera_rotates_the_ray) {
  // Com yaw de 90 graus a camera olha para +X: o raio central tem de segui-la.
  EditorViewport viewport = viewportLookingForward();
  const float position[3] = {0.0f, 0.0f, 0.0f};
  viewport.frustum = buildPerspectiveFrustum(position, 1.57079633f, 0.0f, 16.0f / 9.0f, {});
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  AE_EXPECT_TRUE(ray.valid, "");
  AE_EXPECT_TRUE(nearlyEqual(ray.direction[0], 1.0f, 0.01f), "");
  AE_EXPECT_TRUE(nearlyEqual(ray.direction[2], 0.0f, 0.01f), "");
}

AE_TEST(view_uses_the_viewport_rectangle_and_not_the_whole_screen) {
  // No editor a vista 3D fica entre a barra superior e a dock. Projetar contra
  // a tela inteira deslocaria toda selecao pela altura da barra.
  EditorViewport viewport = viewportLookingForward();
  viewport.rect = {320.0f, 56.0f, 880.0f, 772.0f};
  const float ahead[3] = {0.0f, 0.0f, 10.0f};
  const EditorProjectedPoint projected = projectWorldToScreen(viewport, ahead);
  AE_EXPECT_TRUE(projected.valid, "");
  AE_EXPECT_TRUE(nearlyEqual(projected.screen.x, 760.0f), "centro do retangulo da vista");
  AE_EXPECT_TRUE(nearlyEqual(projected.screen.y, 442.0f), "");

  const EditorRay ray = screenPointToRay(viewport, projected.screen);
  AE_EXPECT_TRUE(nearlyEqual(ray.direction[2], 1.0f, 0.01f), "e o raio inverso concorda");
}

AE_TEST(view_survives_a_ninety_degree_surface_pre_rotation) {
  // A pre-rotacao do display entra aqui do mesmo jeito que entra na oclusao;
  // ida e volta tem de continuar exata com a tela girada.
  EditorViewport viewport = viewportLookingForward();
  viewport.surfaceTransform = {0.0f, -1.0f, 1.0f, 0.0f};
  const float point[3] = {2.0f, 1.0f, 15.0f};
  const EditorProjectedPoint projected = projectWorldToScreen(viewport, point);
  AE_EXPECT_TRUE(projected.valid, "");
  const EditorRay ray = screenPointToRay(viewport, projected.screen);
  AE_EXPECT_TRUE(ray.valid, "");
  const float distance = distanceToCamera(viewport, point);
  for (u32 axis = 0; axis < 3; ++axis)
    AE_EXPECT_TRUE(
        nearlyEqual(ray.origin[axis] + ray.direction[axis] * distance, point[axis], 0.01f), "");
}

AE_TEST(view_invalid_viewports_are_refused_instead_of_producing_pixels) {
  EditorViewport empty{};
  AE_EXPECT_TRUE(!isViewportValid(empty), "frustum invalido");
  const float point[3] = {0, 0, 10};
  AE_EXPECT_TRUE(!projectWorldToScreen(empty, point).valid, "");
  AE_EXPECT_TRUE(!screenPointToRay(empty, {0, 0}).valid, "");

  EditorViewport degenerate = viewportLookingForward();
  degenerate.rect = {0, 0, 0, 900};
  AE_EXPECT_TRUE(!isViewportValid(degenerate), "vista de largura zero");

  EditorViewport collapsed = viewportLookingForward();
  collapsed.surfaceTransform = {0.0f, 0.0f, 0.0f, 0.0f};
  AE_EXPECT_TRUE(!isViewportValid(collapsed),
                 "determinante nulo colapsaria a tela numa linha");
}

AE_TEST(picking_selects_the_nearest_candidate_along_the_ray) {
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({1, {0.0f, 0.0f, 50.0f}, 2.0f, true});
  candidates.push_back({2, {0.0f, 0.0f, 10.0f}, 2.0f, true});
  candidates.push_back({3, {0.0f, 0.0f, 30.0f}, 2.0f, true});
  const EditorPickResult result = pickNearest(candidates, ray);
  AE_EXPECT_TRUE(result.hit, "");
  AE_EXPECT_EQ(result.id, 2u, "o mais proximo vence, nao o primeiro da lista");
  AE_EXPECT_TRUE(nearlyEqual(result.distance, 8.0f, 0.01f), "distancia ate a superficie");
}

AE_TEST(picking_misses_what_the_ray_does_not_cross) {
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({1, {100.0f, 0.0f, 10.0f}, 2.0f, true});
  AE_EXPECT_TRUE(!pickNearest(candidates, ray).hit, "");
}

AE_TEST(picking_ignores_what_is_behind_the_camera) {
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({1, {0.0f, 0.0f, -20.0f}, 2.0f, true});
  AE_EXPECT_TRUE(!pickNearest(candidates, ray).hit, "");
}

AE_TEST(picking_respects_the_selectable_flag) {
  // O olho e o cadeado da hierarquia precisam significar algo no viewport.
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({1, {0.0f, 0.0f, 10.0f}, 2.0f, false});
  candidates.push_back({2, {0.0f, 0.0f, 30.0f}, 2.0f, true});
  const EditorPickResult result = pickNearest(candidates, ray);
  AE_EXPECT_EQ(result.id, 2u, "o objeto travado e atravessado, nao selecionado");
}

AE_TEST(picking_ties_go_to_the_first_registered_candidate) {
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({7, {0.0f, 0.0f, 10.0f}, 2.0f, true});
  candidates.push_back({8, {0.0f, 0.0f, 10.0f}, 2.0f, true});
  AE_EXPECT_EQ(pickNearest(candidates, ray).id, 7u,
               "empate resolvido por ordem torna a selecao a mesma entre frames");
}

AE_TEST(picking_with_the_camera_inside_the_sphere_hits_at_zero) {
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({1, {0.0f, 0.0f, 0.0f}, 5.0f, true});
  const EditorPickResult result = pickNearest(candidates, ray);
  AE_EXPECT_TRUE(result.hit && result.distance == 0.0f,
                 "estar dentro do objeto e um acerto, nao um erro");
}

AE_TEST(picking_ignores_malformed_candidates) {
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({0, {0.0f, 0.0f, 10.0f}, 2.0f, true});
  candidates.push_back({1, {0.0f, 0.0f, 10.0f}, 0.0f, true});
  candidates.push_back({2, {std::nanf(""), 0.0f, 10.0f}, 2.0f, true});
  AE_EXPECT_TRUE(!pickNearest(candidates, ray).hit, "");
}

AE_TEST(picking_with_an_invalid_ray_never_hits) {
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({1, {0.0f, 0.0f, 10.0f}, 2.0f, true});
  AE_EXPECT_TRUE(!pickNearest(candidates, EditorRay{}).hit, "");
}

AE_TEST(editor_grid_plan_scales_with_the_observed_distance) {
  EditorCamera camera;
  const ui::UiRect rect{0,0,800,400};
  const auto close = buildEditorGridPlan(buildEditorViewport(camera,rect,{}));
  camera.distance = 100000;
  camera.target[0] = 700000;
  camera.target[2] = -400000;
  const auto far = buildEditorGridPlan(buildEditorViewport(camera,rect,{}));
  AE_EXPECT_TRUE(close.valid() && far.valid(),"os dois planos valem");
  AE_EXPECT_TRUE(far.minorSpacing>close.minorSpacing*100,"a célula acompanha a escala da câmera");
  // A grossa é sempre a década seguinte: é o par que coexiste na tela.
  AE_EXPECT_EQ(close.majorSpacing,close.minorSpacing*10,"a grossa é a década seguinte");
  AE_EXPECT_TRUE(far.fadeDistance>close.fadeDistance,"o sumiço acompanha a distância observada");
  // Nada de contagem de segmentos: o custo do desenho não cresce com a escala
  // porque não há lista nenhuma para crescer.
  AE_EXPECT_TRUE(std::isfinite(far.minorSpacing) && std::isfinite(far.fadeDistance),"plano finito");
}

AE_TEST(editor_grid_plan_blends_scales_instead_of_switching_decades) {
  // O piscar do zoom vinha de trocar TODAS as linhas de uma vez ao mudar de
  // década. Aqui a célula fina tem que perder peso continuamente, e chegar a
  // zero antes de a próxima década assumir.
  EditorCamera camera;
  const ui::UiRect rect{0,0,800,400};
  float previousSpacing=0,previousWeight=1;
  bool sawDecline=false;
  for(float distance=2;distance<200;distance*=1.08f) {
    camera.distance=distance;
    const auto plan=buildEditorGridPlan(buildEditorViewport(camera,rect,{}));
    AE_EXPECT_TRUE(plan.valid(),"plano válido em toda a varredura");
    AE_EXPECT_TRUE(plan.minorOpacity>=0 && plan.minorOpacity<=1,"peso normalizado");
    if(plan.minorSpacing==previousSpacing && plan.minorOpacity<previousWeight-1e-4f) sawDecline=true;
    // Ao trocar de década, a célula fina precisa já ter chegado perto de zero:
    // é isso que impede o degrau visível.
    if(previousSpacing>0 && plan.minorSpacing>previousSpacing)
      AE_EXPECT_TRUE(previousWeight<.2f,"a célula fina já tinha sumido quando a década trocou");
    previousSpacing=plan.minorSpacing;previousWeight=plan.minorOpacity;
  }
  AE_EXPECT_TRUE(sawDecline,"o peso cai dentro da mesma década");
}

AE_TEST(editor_grid_plan_handles_ground_level_and_rejects_invalid_settings) {
  const auto view=viewportLookingForward();
  const auto plan=buildEditorGridPlan(view);
  AE_EXPECT_TRUE(plan.valid(),"câmera horizontal tem plano estável");
  EditorGridSettings settings; settings.minimumSpacing=0;
  AE_EXPECT_TRUE(!buildEditorGridPlan(view,settings).valid(),"espaçamento inválido recusado");
  EditorGridSettings fade; fade.fadeDistanceFactor=0;
  AE_EXPECT_TRUE(!buildEditorGridPlan(view,fade).valid(),"distância de sumiço inválida recusada");
  AE_EXPECT_TRUE(!buildEditorGridPlan(EditorViewport{}).valid(),"viewport inválido recusado");
}

AE_TEST(editor_camera_clip_range_contains_centimetre_and_kilometre_selections) {
  EditorCamera camera;camera.distance=.01f;
  auto view=buildEditorViewport(camera,{0,0,800,400},{});
  AE_EXPECT_TRUE(projectWorldToScreen(view,camera.target).valid,"centimetre pivot ahead of near plane");
  AE_EXPECT_TRUE(view.frustum.nearPlane<camera.distance*.1f,"near adapts to fine editing");
  camera.distance=100000;
  view=buildEditorViewport(camera,{0,0,800,400},{});
  AE_EXPECT_TRUE(view.frustum.farPlane>camera.distance*2,"large selections within far plane");
}

AE_TEST(view_segment_clips_extreme_horizon_coordinates_before_projection) {
  auto view=viewportLookingForward();
  const float a[]{-1e8f,-1,-1e5f},b[]{1e8f,-1,1e5f};
  ae::ui::UiPoint first,last;
  if(projectSegmentToScreen(view,a,b,first,last)) {
    for(const auto point:{first,last}) {
      AE_EXPECT_TRUE(std::isfinite(point.x)&&std::isfinite(point.y),"coordenadas finitas");
      AE_EXPECT_TRUE(point.x>=-.01f && point.x<=1600.01f && point.y>=-.01f && point.y<=900.01f,"recorte dentro da tela");
    }
  }
}

AE_TEST(pick_mesh_rejects_empty_triangle_region_and_orders_real_surfaces) {
  auto mesh=std::make_shared<EditorPickMesh>();
  AE_EXPECT_TRUE(mesh->build({{{-2,-2,5, 2,-2,5, -2,2,5}}}),"triangle");
  EditorPickCandidate candidate;candidate.id=1;candidate.center[2]=5;candidate.radius=4;candidate.mesh=mesh;
  EditorRay ray;ray.valid=true;ray.direction[2]=1;ray.origin[0]=ray.origin[1]=1.5f;
  AE_EXPECT_TRUE(!pickNearest({&candidate,1},ray).hit,"sphere hit outside actual triangle is rejected");
  ray.origin[0]=ray.origin[1]=-.5f;
  AE_EXPECT_EQ(pickNearest({&candidate,1},ray).distance,5.0f,"surface distance");
  EditorPickCandidate instances[]{candidate,candidate};instances[0].model[10]=2;instances[0].center[2]=10;instances[0].radius=8;
  instances[1].id=2;
  AE_EXPECT_EQ(pickNearest(instances,ray).id,2u,"nearest real surface wins even when first sphere encloses camera");
}
AE_TEST(pick_mesh_affine_transform_keeps_world_distance_and_handles_bvh) {
  EditorPickMesh mesh;std::vector<EditorPickMesh::Triangle> triangles;
  for(int i=0;i<100;++i) {const float x=float(i*4);triangles.push_back({x-1,-1,0,x+1,-1,0,x,1,0});}
  AE_EXPECT_TRUE(mesh.build(std::move(triangles)),"build multiple BVH levels");
  float model[]{2,0,0,0, .5f,3,0,0, 0,0,-4,0, 7,2,12,1};
  const float origin[]{7,2,0},direction[]{0,0,1};float distance=0;
  AE_EXPECT_TRUE(mesh.intersect(origin,direction,model,distance),"shear and reflection");
  AE_EXPECT_EQ(distance,12.0f,"world-space distance under nonuniform scale");
  model[0]=0;
  AE_EXPECT_TRUE(!mesh.intersect(origin,direction,model,distance),"singular instance rejected");
}
