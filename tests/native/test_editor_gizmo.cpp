#include "editor/editor_gizmo.h"
#include "harness.h"

#include <cmath>

using namespace ae;
using namespace ae::editor;
using namespace ae::renderer;

namespace {

EditorViewport viewportLookingForward() {
  EditorViewport viewport{};
  const float position[3] = {0.0f, 0.0f, 0.0f};
  viewport.frustum = buildPerspectiveFrustum(position, 0.0f, 0.0f, 1600.0f / 900.0f, {});
  viewport.rect = {0.0f, 0.0f, 1600.0f, 900.0f};
  return viewport;
}

bool nearlyEqual(float first, float second, float tolerance = 0.05f) {
  return std::fabs(first - second) <= tolerance;
}

float screenLengthOf(const EditorGizmoFrame &frame, u32 axis) {
  const float dx = frame.axisEndScreen[axis].x - frame.originScreen.x;
  const float dy = frame.axisEndScreen[axis].y - frame.originScreen.y;
  return std::sqrt(dx * dx + dy * dy);
}

} // namespace

AE_TEST(gizmo_keeps_the_same_screen_size_at_any_distance) {
  // É a razão de existir do quadro: um gizmo que encolhe com a distância fica
  // inutilizável em um objeto a duzentos metros.
  const EditorViewport viewport = viewportLookingForward();
  EditorGizmoSettings settings{};
  settings.screenLengthPixels = 120.0f;

  const float near[3] = {0.0f, 0.0f, 5.0f};
  const float far[3] = {0.0f, 0.0f, 200.0f};
  const EditorGizmoFrame nearFrame = buildGizmoFrame(viewport, near, settings);
  const EditorGizmoFrame farFrame = buildGizmoFrame(viewport, far, settings);
  AE_EXPECT_TRUE(nearFrame.valid && farFrame.valid, "");
  AE_EXPECT_TRUE(nearlyEqual(screenLengthOf(nearFrame, 1), 120.0f, 1.0f),
                 "o eixo vertical mede o comprimento pedido de perto");
  AE_EXPECT_TRUE(nearlyEqual(screenLengthOf(farFrame, 1), 120.0f, 1.0f),
                 "e o mesmo comprimento de longe");
  AE_EXPECT_TRUE(farFrame.axisWorldLength > nearFrame.axisWorldLength,
                 "o que muda e quantos metros aquele traco representa");
}

AE_TEST(gizmo_axes_point_the_way_the_mockup_draws_them) {
  const EditorViewport viewport = viewportLookingForward();
  const float origin[3] = {0.0f, 0.0f, 20.0f};
  const EditorGizmoFrame frame = buildGizmoFrame(viewport, origin, EditorGizmoSettings{});
  AE_EXPECT_TRUE(frame.valid, "");
  AE_EXPECT_TRUE(frame.axisEndScreen[0].x > frame.originScreen.x, "X vermelho vai para a direita");
  AE_EXPECT_TRUE(frame.axisEndScreen[1].y < frame.originScreen.y, "Y verde sobe na tela");
  AE_EXPECT_TRUE(frame.axisUsable[0] && frame.axisUsable[1], "");
}

AE_TEST(gizmo_axis_pointing_at_the_camera_is_not_grabbable) {
  // Olhando de frente para o objeto, Z aponta para longe da camera e a projecao
  // dele degenera num ponto: qualquer arraste viraria um salto enorme.
  const EditorViewport viewport = viewportLookingForward();
  const float origin[3] = {0.0f, 0.0f, 20.0f};
  EditorGizmoSettings settings{};
  settings.minimumAxisPixels = 24.0f;
  const EditorGizmoFrame frame = buildGizmoFrame(viewport, origin, settings);
  AE_EXPECT_TRUE(frame.valid, "");
  AE_EXPECT_TRUE(!frame.axisUsable[2], "Z esta apontado para dentro da tela");
  AE_EXPECT_TRUE(pickGizmoHandle(frame, frame.originScreen, settings) !=
                     EditorGizmoHandle::AxisZ,
                 "e nao pode ser pego nem no proprio centro");
}

AE_TEST(gizmo_picks_the_axis_under_the_finger) {
  const EditorViewport viewport = viewportLookingForward();
  const float origin[3] = {0.0f, 0.0f, 20.0f};
  EditorGizmoSettings settings{};
  const EditorGizmoFrame frame = buildGizmoFrame(viewport, origin, settings);

  const ui::UiPoint onX{(frame.originScreen.x + frame.axisEndScreen[0].x) * 0.5f,
                        frame.originScreen.y};
  AE_EXPECT_TRUE(pickGizmoHandle(frame, onX, settings) == EditorGizmoHandle::AxisX, "");

  const ui::UiPoint onY{frame.originScreen.x,
                        (frame.originScreen.y + frame.axisEndScreen[1].y) * 0.5f};
  AE_EXPECT_TRUE(pickGizmoHandle(frame, onY, settings) == EditorGizmoHandle::AxisY, "");
}

AE_TEST(gizmo_returns_none_far_from_every_axis) {
  // Longe do gizmo o toque pertence a selecao ou a camera, e e o chamador que
  // decide qual dos dois.
  const EditorViewport viewport = viewportLookingForward();
  const float origin[3] = {0.0f, 0.0f, 20.0f};
  EditorGizmoSettings settings{};
  const EditorGizmoFrame frame = buildGizmoFrame(viewport, origin, settings);
  const ui::UiPoint far{frame.originScreen.x + 400.0f, frame.originScreen.y + 300.0f};
  AE_EXPECT_TRUE(pickGizmoHandle(frame, far, settings) == EditorGizmoHandle::None, "");
}

AE_TEST(gizmo_pick_does_not_extend_past_the_drawn_arrow) {
  // Prolongar a reta faria a seta ser pegavel do outro lado do objeto, onde nao
  // ha nada desenhado.
  const EditorViewport viewport = viewportLookingForward();
  const float origin[3] = {0.0f, 0.0f, 20.0f};
  EditorGizmoSettings settings{};
  const EditorGizmoFrame frame = buildGizmoFrame(viewport, origin, settings);
  const ui::UiPoint beyond{frame.axisEndScreen[0].x + 200.0f, frame.originScreen.y};
  AE_EXPECT_TRUE(pickGizmoHandle(frame, beyond, settings) == EditorGizmoHandle::None, "");
}

AE_TEST(gizmo_drag_along_x_moves_only_x) {
  const EditorViewport viewport = viewportLookingForward();
  const float origin[3] = {0.0f, 0.0f, 20.0f};
  EditorGizmoSettings settings{};
  const EditorGizmoFrame frame = buildGizmoFrame(viewport, origin, settings);

  EditorTransform initial{};
  initial.position[2] = 20.0f;
  EditorGizmoDrag drag{};
  AE_EXPECT_TRUE(beginGizmoDrag(frame, EditorGizmoHandle::AxisX, initial, settings, drag), "");

  // Arrastar exatamente o comprimento do eixo na tela move exatamente o
  // comprimento de mundo que ele representa.
  const float pixels = screenLengthOf(frame, 0);
  EditorTransform moved{};
  AE_EXPECT_TRUE(resolveGizmoTranslation(drag, {pixels, 0.0f}, moved), "");
  AE_EXPECT_TRUE(nearlyEqual(moved.position[0], frame.axisWorldLength, 0.01f), "");
  AE_EXPECT_TRUE(moved.position[1] == 0.0f && moved.position[2] == 20.0f,
                 "os outros eixos nao se movem");
}

AE_TEST(gizmo_drag_perpendicular_to_the_axis_does_nothing) {
  const EditorViewport viewport = viewportLookingForward();
  const float origin[3] = {0.0f, 0.0f, 20.0f};
  EditorGizmoSettings settings{};
  const EditorGizmoFrame frame = buildGizmoFrame(viewport, origin, settings);
  EditorGizmoDrag drag{};
  beginGizmoDrag(frame, EditorGizmoHandle::AxisX, EditorTransform{}, settings, drag);
  EditorTransform moved{};
  AE_EXPECT_TRUE(resolveGizmoTranslation(drag, {0.0f, 300.0f}, moved), "");
  AE_EXPECT_TRUE(nearlyEqual(moved.position[0], 0.0f, 0.001f),
                 "so a componente ao longo do eixo conta");
}

AE_TEST(gizmo_drag_is_reproducible_and_never_accumulates) {
  // A propriedade que impede o objeto de perseguir o dedo com ganho crescente:
  // o mesmo deslocamento total sempre da o mesmo resultado.
  const EditorViewport viewport = viewportLookingForward();
  const float origin[3] = {0.0f, 0.0f, 20.0f};
  EditorGizmoSettings settings{};
  const EditorGizmoFrame frame = buildGizmoFrame(viewport, origin, settings);
  EditorTransform initial{};
  initial.position[2] = 20.0f;
  EditorGizmoDrag drag{};
  beginGizmoDrag(frame, EditorGizmoHandle::AxisX, initial, settings, drag);

  EditorTransform first{};
  EditorTransform second{};
  resolveGizmoTranslation(drag, {60.0f, 0.0f}, first);
  // Cem chamadas intermediarias, como um arraste real produz.
  for (u32 step = 0; step < 100; ++step) {
    EditorTransform scratch{};
    resolveGizmoTranslation(drag, {static_cast<float>(step), 0.0f}, scratch);
  }
  resolveGizmoTranslation(drag, {60.0f, 0.0f}, second);
  AE_EXPECT_TRUE(first.position[0] == second.position[0],
                 "o resultado depende so do deslocamento total, nunca do caminho");
}

AE_TEST(gizmo_drag_back_to_the_start_restores_the_initial_position) {
  const EditorViewport viewport = viewportLookingForward();
  const float origin[3] = {1.5f, 0.0f, 20.0f};
  EditorGizmoSettings settings{};
  const EditorGizmoFrame frame = buildGizmoFrame(viewport, origin, settings);
  EditorTransform initial{};
  initial.position[0] = 1.5f;
  initial.position[2] = 20.0f;
  EditorGizmoDrag drag{};
  beginGizmoDrag(frame, EditorGizmoHandle::AxisX, initial, settings, drag);
  EditorTransform moved{};
  resolveGizmoTranslation(drag, {200.0f, 0.0f}, moved);
  resolveGizmoTranslation(drag, {0.0f, 0.0f}, moved);
  AE_EXPECT_TRUE(nearlyEqual(moved.position[0], 1.5f, 0.001f), "");
}

AE_TEST(gizmo_snap_is_applied_to_the_final_position) {
  // Encaixar o deslocamento manteria o desalinhamento original para sempre; o
  // passo de grade existe justamente para alinhar.
  const EditorViewport viewport = viewportLookingForward();
  const float origin[3] = {0.3f, 0.0f, 20.0f};
  EditorGizmoSettings settings{};
  settings.snapStep = 1.0f;
  const EditorGizmoFrame frame = buildGizmoFrame(viewport, origin, settings);
  EditorTransform initial{};
  initial.position[0] = 0.3f;
  initial.position[2] = 20.0f;
  EditorGizmoDrag drag{};
  beginGizmoDrag(frame, EditorGizmoHandle::AxisX, initial, settings, drag);
  EditorTransform moved{};
  const float pixels = screenLengthOf(frame, 0);
  resolveGizmoTranslation(drag, {pixels, 0.0f}, moved);
  AE_EXPECT_TRUE(nearlyEqual(moved.position[0], std::round(moved.position[0]), 0.0001f),
                 "a posicao final cai na grade, e nao o deslocamento");
}

AE_TEST(gizmo_drag_cannot_begin_on_an_unusable_axis_or_without_a_handle) {
  const EditorViewport viewport = viewportLookingForward();
  const float origin[3] = {0.0f, 0.0f, 20.0f};
  EditorGizmoSettings settings{};
  const EditorGizmoFrame frame = buildGizmoFrame(viewport, origin, settings);
  EditorGizmoDrag drag{};
  AE_EXPECT_TRUE(!beginGizmoDrag(frame, EditorGizmoHandle::AxisZ, EditorTransform{}, settings, drag),
                 "Z esta degenerado nesta pose");
  AE_EXPECT_TRUE(!beginGizmoDrag(frame, EditorGizmoHandle::None, EditorTransform{}, settings, drag),
                 "");
  AE_EXPECT_TRUE(!drag.active, "");
  EditorTransform moved{};
  AE_EXPECT_TRUE(!resolveGizmoTranslation(drag, {10.0f, 0.0f}, moved),
                 "um arraste inativo nunca produz transform");
}

AE_TEST(gizmo_refuses_an_origin_behind_the_camera) {
  const EditorViewport viewport = viewportLookingForward();
  const float behind[3] = {0.0f, 0.0f, -10.0f};
  AE_EXPECT_TRUE(!buildGizmoFrame(viewport, behind, EditorGizmoSettings{}).valid, "");
}

AE_TEST(gizmo_refuses_malformed_settings) {
  const EditorViewport viewport = viewportLookingForward();
  const float origin[3] = {0.0f, 0.0f, 20.0f};
  EditorGizmoSettings zeroLength{};
  zeroLength.screenLengthPixels = 0.0f;
  AE_EXPECT_TRUE(!buildGizmoFrame(viewport, origin, zeroLength).valid, "");
  EditorGizmoSettings negativeSnap{};
  negativeSnap.snapStep = -1.0f;
  AE_EXPECT_TRUE(!isGizmoSettingsValid(negativeSnap), "");
}

AE_TEST(gizmo_rotate_and_scale_do_not_translate_the_entity) {
  EditorGizmoDrag drag{};drag.active=true;drag.handle=EditorGizmoHandle::AxisX;
  drag.frame.valid=true;drag.frame.axisUsable[0]=true;drag.frame.axisEndScreen[0]={100,0};
  drag.frame.axisWorldLength=10;
  EditorTransform rotated{},scaled{};
  AE_EXPECT_TRUE(resolveGizmoTransform(drag,EditorGizmoMode::Rotate,{90,0},rotated),"rotate");
  AE_EXPECT_EQ(rotated.rotationDegrees[0],90.0f,"rotation mode changes angle");
  AE_EXPECT_EQ(rotated.position[0],0.0f,"position unchanged");
  AE_EXPECT_TRUE(resolveGizmoTransform(drag,EditorGizmoMode::Scale,{100,0},scaled),"scale");
  AE_EXPECT_TRUE(scaled.scale[0]>2.7f && scaled.scale[0]<2.8f,"scale multiplier");
  AE_EXPECT_EQ(scaled.position[0],0.0f,"position unchanged");
}

AE_TEST(gizmo_ring_angle_matches_projected_world_points) {
  const auto view=viewportLookingForward();const float origin[3]{0,0,5};
  for(float expected : {0.0f,.5f,1.5707963f,-2.0f}) {
    float point[3];gizmoRingPoint(origin,2,1,expected,point);
    const auto projected=projectWorldToScreen(view,point);float angle=0;
    AE_EXPECT_TRUE(projected.valid && gizmoRingAngle(view,origin,2,projected.screen,angle),"ray hits ring plane");
    AE_EXPECT_TRUE(nearlyEqual(angle,expected,.0001f),"angle is geometric, not pixel displacement");
  }
  float angle=0;
  AE_EXPECT_TRUE(!gizmoRingAngle(view,origin,0,{800,450},angle),"parallel ray rejected");
  AE_EXPECT_TRUE(!gizmoRingAngle(view,origin,2,{800,450},angle),"undefined centre rejected");
}

AE_TEST(gizmo_plane_intersection_roundtrips_and_rejects_parallel) {
  const auto view=viewportLookingForward();const float origin[3]{0,0,10};
  const float point[3]{2,-3,10};float hit[3]{};
  const auto screen=projectWorldToScreen(view,point).screen;
  AE_EXPECT_TRUE(gizmoPlanePoint(view,origin,2,screen,hit),"XY hit");
  for(u32 i=0;i<3;++i) AE_EXPECT_TRUE(std::abs(hit[i]-point[i])<.0001f,"exact projected point");
  AE_EXPECT_TRUE(!gizmoPlanePoint(view,origin,0,{800,450},hit),"parallel ray");
  const float behind[3]{0,0,-10};
  AE_EXPECT_TRUE(!gizmoPlanePoint(view,behind,2,screen,hit),"behind camera");
}
