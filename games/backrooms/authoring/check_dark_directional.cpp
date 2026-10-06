#include "renderer/punctual_lights.h"
#include <array>
#include <cassert>
#include <iostream>
int main() {
  using namespace ae::renderer;
  std::array<SceneLight,2> lights{};
  lights[0].modality=LightModality::Directional;
  lights[0].objectId=1;
  lights[0].intensity=0;
  lights[1].modality=LightModality::Point;
  lights[1].intensity=0;
  assert(selectDirectionalLight(lights)==&lights[0]);
  assert(!lightContributes(lights[0])&&!lightContributes(lights[1]));
  lights[1].modality=LightModality::Directional;
  lights[1].intensity=2;
  assert(selectDirectionalLight(lights)==&lights[1]);
  assert(selectDirectionalLight({})==nullptr);
  std::cout<<"PASS: authored zero suppresses fallback; positive directional wins; zero lights consume no local slots\n";
}
