#include "harness.h"
#include "platform/first_person_controller.h"

#include <cmath>

using namespace ae::platform;

AE_TEST(Virtual_joystick_dead_zone_clamps_and_releases_pointer) {
  VirtualJoystick joystick;
  AE_EXPECT_TRUE(joystick.begin(7, 100, 200, 1000, 500), "begin");
  AE_EXPECT_TRUE(joystick.update(7, 102, 200), "small update");
  AE_EXPECT_EQ(joystick.state().axisX, 0.0f, "dead zone");
  AE_EXPECT_TRUE(joystick.update(7, 500, 200), "large update");
  AE_EXPECT_TRUE(std::abs(joystick.state().axisX - 1.0f) < .0001f, "circular clamp");
  AE_EXPECT_TRUE(!joystick.end(8), "foreign pointer ignored");
  AE_EXPECT_TRUE(joystick.end(7), "owner release");
  AE_EXPECT_TRUE(!joystick.state().active, "inactive");
}

AE_TEST(First_person_touch_routes_move_and_look_without_role_jump) {
  FirstPersonTouchControls controls;
  AE_EXPECT_TRUE(controls.pointerDown(1, 100, 400, 1000, 500), "move down");
  AE_EXPECT_TRUE(controls.pointerDown(2, 800, 200, 1000, 500), "look down");
  controls.pointerMove(1, 100, 300, 1000, 500);
  controls.pointerMove(2, 900, 250, 1000, 500);
  const FirstPersonInput input = controls.consumeInput();
  AE_EXPECT_TRUE(input.moveForward > 0.5f, "forward action");
  AE_EXPECT_TRUE(std::abs(input.lookScreenX - .1f) < .0001f, "look x");
  AE_EXPECT_TRUE(std::abs(input.lookScreenY - .1f) < .0001f, "look y");
  AE_EXPECT_TRUE(controls.pointerUp(1), "move release");
  controls.pointerMove(2, 950, 250, 1000, 500);
  AE_EXPECT_TRUE(controls.consumeInput().lookScreenX > 0.0f, "look remains owner");
}

AE_TEST(First_person_movement_is_yaw_relative_normalized_and_ground_planar) {
  FirstPersonController controller;
  FreeCameraState state{};
  state.position[1] = 160.0f;
  state.yaw = 1.57079632679f;
  FirstPersonInput input{};
  input.moveRight = 1.0f;
  input.moveForward = 1.0f;
  AE_EXPECT_TRUE(controller.update(state, input, 1.0f), "update");
  AE_EXPECT_TRUE(state.position[0] > 0.0f, "yaw forward contributes x");
  AE_EXPECT_TRUE(state.position[2] < 0.0f, "yaw right contributes negative z");
  AE_EXPECT_EQ(state.position[1], 160.0f, "ground plane preserved");
  AE_EXPECT_TRUE(std::hypot(state.position[0], state.position[2]) <= 1.21f,
                 "delta time is spike-clamped and diagonal normalized");
}
