#include "renderer/physical_atmosphere.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ae::renderer {
namespace {
using Vector3 = std::array<double, 3>;
using Density = std::array<double, 2>;
constexpr double Pi = 3.14159265358979323846;

double dot(const Vector3 &a, const Vector3 &b) {
  return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
Vector3 add(const Vector3 &a,const Vector3 &b) {
  return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};
}
Vector3 multiply(const Vector3 &value,double scale) {
  return {value[0]*scale,value[1]*scale,value[2]*scale};
}
Vector3 normalize(const Vector3 &value) {
  const double length=std::sqrt(dot(value,value));
  return length>0?multiply(value,1.0/length):Vector3{};
}
bool raySphere(const Vector3 &origin,const Vector3 &direction,double radius,
               double &nearDistance,double &farDistance) {
  const Vector3 normalizedOrigin=multiply(origin,1.0/radius);
  const double b=dot(normalizedOrigin,direction);
  const double radialDistance=std::sqrt(dot(normalizedOrigin,normalizedOrigin));
  const double c=(radialDistance-1.0)*(radialDistance+1.0);
  const double discriminant=b*b-c;
  const double tolerance=4.0e-14*std::max({1.0,std::abs(b*b),std::abs(c)});
  if(discriminant < -tolerance) return false;
  const double root=std::sqrt(std::max(discriminant,0.0));
  nearDistance=(-b-root)*radius;farDistance=(-b+root)*radius;
  return true;
}

Density densityAt(const Vector3 &position,double planetRadius,const Density &scaleHeights) {
  const double height=std::max(std::sqrt(dot(position,position))-planetRadius,0.0);
  return {std::exp(-height/scaleHeights[0]),std::exp(-height/scaleHeights[1])};
}
double cubic(double value) {return value*value*value;}

Density opticalDepth(const Vector3 &origin,const Vector3 &direction,double planetRadius,
                     double atmosphereRadius,const Density &scaleHeights,int steps) {
  double nearDistance=0,farDistance=0;
  if(!raySphere(origin,direction,atmosphereRadius,nearDistance,farDistance)||farDistance<=0)
    return {};
  Density result{};
  for(int index=0;index<steps;++index) {
    const double segmentStart=cubic(double(index)/steps)*farDistance;
    const double segmentEnd=cubic(double(index+1)/steps)*farDistance;
    const auto density=densityAt(add(origin,multiply(direction,(segmentStart+segmentEnd)*.5)),
                                 planetRadius,scaleHeights);
    for(u32 medium=0;medium<2;++medium)
      result[medium]+=density[medium]*(segmentEnd-segmentStart);
  }
  return result;
}

Vector3 integrate(const PhysicalAtmosphereGroundIrradianceInput &input,
                  const Vector3 &origin,const Vector3 &direction,double distance,
                  const Vector3 &sunDirection) {
  const Density scaleHeights{input.rayleighScaleHeightKm,input.aerosolScaleHeightKm};
  const Vector3 betaRayleigh{.005802*input.airDensity,.013558*input.airDensity,
                             .033100*input.airDensity};
  const Vector3 betaAerosol{.021*input.aerosolDensity,.021*input.aerosolDensity,
                            .021*input.aerosolDensity};
  const Vector3 betaAerosolExtinction=multiply(betaAerosol,1.10);
  const double atmosphereRadius=input.planetRadiusKm+input.atmosphereHeightKm;
  const double cosine=std::clamp(dot(direction,sunDirection),-1.0,1.0);
  const double rayleighPhase=3.0*(1.0+cosine*cosine)/(16.0*Pi);
  const double g=input.aerosolAnisotropy,g2=g*g;
  const double denominator=std::max(1.0+g2-2.0*g*cosine,.001);
  const double aerosolPhase=3.0*(1.0-g2)*(1.0+cosine*cosine)/
      (8.0*Pi*(2.0+g2)*std::pow(denominator,1.5));
  Density viewDepth{};Vector3 scattered{};
  constexpr int ViewSteps=16,LightSteps=8;
  for(int index=0;index<ViewSteps;++index) {
    const double segmentStart=cubic(double(index)/ViewSteps)*distance;
    const double segmentEnd=cubic(double(index+1)/ViewSteps)*distance;
    const double segmentLength=segmentEnd-segmentStart;
    const auto samplePosition=add(origin,multiply(direction,(segmentStart+segmentEnd)*.5));
    const auto density=densityAt(samplePosition,input.planetRadiusKm,scaleHeights);
    Density segmentDepth{density[0]*segmentLength,density[1]*segmentLength};
    for(u32 medium=0;medium<2;++medium) viewDepth[medium]+=segmentDepth[medium]*.5;

    const auto sampleUp=normalize(samplePosition);
    double groundNear=0,groundFar=0;
    const auto shadowOrigin=add(samplePosition,multiply(sampleUp,.001));
    const bool groundShadow=raySphere(shadowOrigin,sunDirection,input.planetRadiusKm,
                                      groundNear,groundFar)&&groundNear>0;
    if(!groundShadow) {
      const auto sunDepth=opticalDepth(samplePosition,sunDirection,input.planetRadiusKm,
                                       atmosphereRadius,scaleHeights,LightSteps);
      for(u32 channel=0;channel<3;++channel) {
        const double extinction=betaRayleigh[channel]*(viewDepth[0]+sunDepth[0])+
            betaAerosolExtinction[channel]*(viewDepth[1]+sunDepth[1]);
        scattered[channel]+=std::exp(-extinction)*
            (betaRayleigh[channel]*density[0]*rayleighPhase+
             betaAerosol[channel]*density[1]*aerosolPhase)*segmentLength;
      }
    }
    for(u32 medium=0;medium<2;++medium) viewDepth[medium]+=segmentDepth[medium]*.5;
  }
  return scattered;
}
} // namespace

bool PhysicalAtmosphereGroundIrradianceInput::valid() const noexcept {
  const float scalars[]{airDensity,aerosolDensity,aerosolAnisotropy,planetRadiusKm,
      rayleighScaleHeightKm,aerosolScaleHeightKm,atmosphereHeightKm,sunIntensity,
      sunDirection[0],sunDirection[1],sunDirection[2],sunColor[0],sunColor[1],sunColor[2]};
  for(float value:scalars) if(!std::isfinite(value)) return false;
  if(airDensity<0||airDensity>8||aerosolDensity<0||aerosolDensity>8||
     aerosolAnisotropy<0||aerosolAnisotropy>.95f||planetRadiusKm<1||planetRadiusKm>100000||
     rayleighScaleHeightKm<.1f||rayleighScaleHeightKm>100||
     aerosolScaleHeightKm<.05f||aerosolScaleHeightKm>50||
     atmosphereHeightKm<1||atmosphereHeightKm>1000||sunIntensity<0) return false;
  for(float value:sunColor) if(value<0) return false;
  if(sunIntensity>0) {
    const double lengthSquared=double(sunDirection[0])*sunDirection[0]+
        double(sunDirection[1])*sunDirection[1]+double(sunDirection[2])*sunDirection[2];
    if(lengthSquared<1.0e-12) return false;
  }
  return true;
}

bool computePhysicalAtmosphereGroundIrradiance(
    const PhysicalAtmosphereGroundIrradianceInput &input,float out[3]) noexcept {
  if(!out||!input.valid()) return false;
  float candidate[3]{};
  if(input.sunIntensity>0&&(input.airDensity>0||input.aerosolDensity>0)) {
    const Vector3 sunDirection=normalize({input.sunDirection[0],input.sunDirection[1],
                                         input.sunDirection[2]});
    const Vector3 normal{0,1,0};
    Vector3 tangent=add(sunDirection,multiply(normal,-dot(normal,sunDirection)));
    if(std::sqrt(dot(tangent,tangent))<.0001) tangent={1,0,0};
    else tangent=normalize(tangent);
    const std::array<Vector3,3> directions{{normal,normalize(add(normal,tangent)),
                                           normalize(add(normal,multiply(tangent,-1)))}};
    const double weights[]{.5,.25,.25};
    const double atmosphereRadius=input.planetRadiusKm+input.atmosphereHeightKm;
    const Vector3 origin{0,input.planetRadiusKm+.001,0};
    Vector3 averageRadiance{};
    for(u32 sample=0;sample<directions.size();++sample) {
      double nearDistance=0,farDistance=0;
      if(!raySphere(origin,directions[sample],atmosphereRadius,nearDistance,farDistance)) return false;
      const auto radiance=integrate(input,origin,directions[sample],std::max(farDistance,0.0),sunDirection);
      for(u32 channel=0;channel<3;++channel)
        averageRadiance[channel]+=radiance[channel]*weights[sample];
    }
    for(u32 channel=0;channel<3;++channel) {
      const double irradiance=Pi*averageRadiance[channel]*input.sunColor[channel]*input.sunIntensity;
      if(!std::isfinite(irradiance)||irradiance<0||irradiance>std::numeric_limits<float>::max()) return false;
      candidate[channel]=static_cast<float>(irradiance);
    }
  }
  std::copy(std::begin(candidate),std::end(candidate),out);
  return true;
}

bool PhysicalAtmosphereGroundIrradianceCache::resolve(
    const PhysicalAtmosphereGroundIrradianceInput &input,float out[3]) noexcept {
  if(!out||!input.valid()) return false;
  if(!initialized_||!(input==input_)) {
    float candidate[3];
    if(!computePhysicalAtmosphereGroundIrradiance(input,candidate)) return false;
    input_=input;std::copy(std::begin(candidate),std::end(candidate),irradiance_);
    initialized_=true;++revision_;
  }
  std::copy(std::begin(irradiance_),std::end(irradiance_),out);
  return true;
}

} // namespace ae::renderer
