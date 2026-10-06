#include "ui/gui_world.h"
#include <cmath>
#include <algorithm>
namespace ae::ui {
namespace {float dot(const float *a,const float *b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}}
bool GuiWorldFrame::configure(const GuiCanvas &c,const renderer::PerspectiveFrustum &camera,const UiRect &viewport,const UiRect &surface,const float *hostMatrix) {
  valid_=false;if(c.mode!=GuiCanvasMode::World || !camera.valid || viewport.isEmpty() || surface.isEmpty())return false;
  canvas_=c;camera_=camera;viewport_=viewport;surface_=surface;
  constexpr float radians=.017453292519943295f;
  // Existing camera basis provides an orthonormal yaw/pitch/roll convention.
  const auto rotation=renderer::buildCameraViewBasis(c.rotation[1]*radians,c.rotation[0]*radians,c.rotation[2]*radians);
  for(u32 a=0;a<3;++a) {right_[a]=rotation.row0[a];down_[a]=-rotation.row1[a];origin_[a]=c.position[a]-right_[a]*c.resolution.x*c.unitsPerPixel*.5f-down_[a]*c.resolution.y*c.unitsPerPixel*.5f;}
  if(hostMatrix) {
    for(u32 i=0;i<16;++i)if(!std::isfinite(hostMatrix[i]))return false;
    // Full affine basis: no Euler decomposition, scale loss or hidden shear.
    if(std::abs(hostMatrix[3])+std::abs(hostMatrix[7])+std::abs(hostMatrix[11])+std::abs(hostMatrix[15]-1)>1e-5f)return false;
    float r[3],d[3],o[3];
    for(u32 a=0;a<3;++a){r[a]=hostMatrix[a]*right_[0]+hostMatrix[4+a]*right_[1]+hostMatrix[8+a]*right_[2];d[a]=hostMatrix[a]*down_[0]+hostMatrix[4+a]*down_[1]+hostMatrix[8+a]*down_[2];o[a]=hostMatrix[a]*origin_[0]+hostMatrix[4+a]*origin_[1]+hostMatrix[8+a]*origin_[2]+hostMatrix[12+a];}
    std::copy_n(r,3,right_);std::copy_n(d,3,down_);std::copy_n(o,3,origin_);
  }
  const float rr=dot(right_,right_),dd=dot(down_,down_),rd=dot(right_,down_);
  if(rr<=1e-12f || dd<=1e-12f || (rr*dd-rd*rd)/(rr*dd)<1e-8f)return false;
  const auto view=renderer::buildCameraViewBasis(camera.yaw,camera.pitch,camera.roll);
  const float constant[3]{origin_[0]-camera.cameraPosition[0],origin_[1]-camera.cameraPosition[1],origin_[2]-camera.cameraPosition[2]};
  for(u32 column=0;column<3;++column) {
    const float *world=column==0?right_:column==1?down_:constant;
    float v[3];renderer::cameraWorldToView(view,world,v);
    if(column<2)for(float &a:v)a*=c.unitsPerPixel;
    const float w=renderer::isOrthographic(camera)?(column==2?1.f:0.f):v[2];
    const float x=v[0]/renderer::projectionHalfWidth(camera),y=-v[1]/renderer::projectionHalfHeight(camera);
    projection_[column*4]=x*viewport.width/surface.width+(2*(viewport.x+viewport.width*.5f)/surface.width-1)*w;
    projection_[column*4+1]=y*viewport.height/surface.height+(2*(viewport.y+viewport.height*.5f)/surface.height-1)*w;
    projection_[column*4+2]=renderer::isOrthographic(camera)?(v[2]-(column==2?camera.nearPlane:0))/(camera.farPlane-camera.nearPlane):(camera.farPlane*v[2]-(column==2?camera.nearPlane*camera.farPlane:0))/(camera.farPlane-camera.nearPlane);
    projection_[column*4+3]=w;
  }
  for(float v:projection_)if(!std::isfinite(v))return false;
  valid_=true;return true;
}
bool GuiWorldFrame::project(UiPoint local,UiPoint &screen,float &depth) const {
  if(!valid_)return false;
  float clip[4];for(u32 a=0;a<4;++a)clip[a]=projection_[a]*local.x+projection_[4+a]*local.y+projection_[8+a];
  if(clip[3]<=0 || clip[2]<0 || clip[2]>clip[3])return false;
  screen={(clip[0]/clip[3]+1)*surface_.width*.5f,(clip[1]/clip[3]+1)*surface_.height*.5f};depth=clip[2]/clip[3];return true;
}
bool GuiWorldFrame::map(UiPoint screen,UiPoint &local,float &distance) const {
  if(!valid_)return false;
  const auto ray=renderer::cameraRayFromNdc(camera_,{},(screen.x-viewport_.x)/viewport_.width*2-1,(screen.y-viewport_.y)/viewport_.height*2-1);
  if(!ray.valid)return false;
  float normal[3]{right_[1]*down_[2]-right_[2]*down_[1],right_[2]*down_[0]-right_[0]*down_[2],right_[0]*down_[1]-right_[1]*down_[0]};
  const float denominator=dot(normal,ray.direction);if(std::abs(denominator)<1e-7f)return false;
  float delta[3];for(u32 a=0;a<3;++a)delta[a]=origin_[a]-camera_.cameraPosition[a]-ray.originOffset[a];
  const float t=dot(delta,normal)/denominator;if(t<camera_.nearPlane || t>camera_.farPlane)return false;
  float point[3];for(u32 a=0;a<3;++a)point[a]=-delta[a]+ray.direction[a]*t;
  const float rr=dot(right_,right_),dd=dot(down_,down_),rd=dot(right_,down_),det=rr*dd-rd*rd;
  const float pr=dot(point,right_),pd=dot(point,down_);
  local={(pr*dd-pd*rd)/(det*canvas_.unitsPerPixel),(pd*rr-pr*rd)/(det*canvas_.unitsPerPixel)};
  distance=t*std::sqrt(dot(ray.direction,ray.direction));return std::isfinite(local.x)&&std::isfinite(local.y);
}
} // namespace ae::ui
