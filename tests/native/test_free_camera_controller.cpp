#include "harness.h"
#include "platform/free_camera_controller.h"

#include <cmath>

using namespace ae::platform;

AE_TEST(Free_camera_one_finger_looks_and_clamps_pitch) {
  FreeCameraController camera;
  FreeCameraTouch start{1, 100.0f, 100.0f};
  AE_EXPECT_TRUE(camera.updateTouches(&start, 1, 1000, 500), "baseline");
  FreeCameraTouch moved{1, 350.0f, 1000.0f};
  AE_EXPECT_TRUE(camera.updateTouches(&moved, 1, 1000, 500), "move");
  AE_EXPECT_TRUE(std::abs(camera.state().yaw - 1.5707963f) < .0001f, "quarter turn");
  AE_EXPECT_TRUE(camera.state().pitch <= camera.settings().maximumPitchRadians, "pitch clamp");
}

AE_TEST(Free_camera_two_fingers_pan_and_dolly_all_axes) {
  FreeCameraController camera;
  FreeCameraTouch first[2]{{4, 200, 200}, {9, 400, 200}};
  AE_EXPECT_TRUE(camera.updateTouches(first, 2, 1000, 500), "baseline");
  FreeCameraTouch moved[2]{{4, 250, 150}, {9, 550, 150}};
  AE_EXPECT_TRUE(camera.updateTouches(moved, 2, 1000, 500), "gesture");
  AE_EXPECT_TRUE(camera.state().position[0] > 0, "right");
  AE_EXPECT_TRUE(camera.state().position[1] > 0, "up");
  AE_EXPECT_TRUE(camera.state().position[2] > 0, "forward");
}

AE_TEST(Free_camera_pointer_transition_rebases_without_jump) {
  FreeCameraController camera;
  FreeCameraTouch one{1, 100, 100};
  camera.updateTouches(&one, 1, 1000, 500);
  FreeCameraTouch two[2]{{1, 100, 100}, {2, 800, 400}};
  camera.updateTouches(two, 2, 1000, 500);
  AE_EXPECT_EQ(camera.state().position[0], 0.0f, "no transition pan");
  AE_EXPECT_EQ(camera.state().position[2], 0.0f, "no transition dolly");
}
