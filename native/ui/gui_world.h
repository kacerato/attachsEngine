#pragma once
#include "ui/gui_document.h"
#include "renderer/camera_ray.h"
namespace ae::ui {
class GuiWorldFrame final {
public:
  bool configure(const GuiCanvas &canvas,const renderer::PerspectiveFrustum &camera,const UiRect &viewport,const UiRect &surface,const float *hostMatrix=nullptr);
  // Plane ray parameter is converted to world distance; used for scene picking.
  bool map(UiPoint screen,UiPoint &local,float &distance) const;
  bool project(UiPoint local,UiPoint &screen,float &depth) const;
  const float *projection() const {return projection_;}
  bool valid() const {return valid_;}
private:
  GuiCanvas canvas_{};renderer::PerspectiveFrustum camera_{};
  UiRect viewport_{},surface_{};
  float origin_[3]{},right_[3]{},down_[3]{},projection_[12]{};
  bool valid_=false;
};
} // namespace ae::ui
