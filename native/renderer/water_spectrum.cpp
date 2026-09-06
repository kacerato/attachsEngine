#include "renderer/water_fft.h"
#include <cmath>
#include <algorithm>

namespace ae::renderer {
namespace {
constexpr double Pi=3.14159265358979323846;
constexpr double Gravity=9.81;

// Explicit integer generator avoids std::normal_distribution implementation
// differences. Half-bin samples exclude both zero and one for Box-Muller.
double uniform(u32 &state) {
  state=1664525u*state+1013904223u;
  return (static_cast<double>(state)+0.5)/4294967296.0;
}
}

bool validateWaterSpectrum(const WaterSpectrumSettings &s) noexcept {
  const float values[]={s.patchLength,s.depth,s.windSpeed,s.windDirection,s.fetch,
    s.swell,s.spread,s.shortWaveDamping,s.minimumWavelength,s.maximumWavelength};
  for(float value:values) if(!std::isfinite(value)) return false;
  return s.resolution>=8 && s.resolution<=2048 && (s.resolution&(s.resolution-1))==0 &&
    s.patchLength>=1 && s.patchLength<=100000 && s.depth>=.1f && s.depth<=10000 &&
    s.windSpeed>=0 && s.windSpeed<=100 && s.fetch>=1 && s.fetch<=10000000 &&
    s.swell>=0 && s.swell<=2 && s.spread>=0 && s.spread<=1 &&
    s.shortWaveDamping>=0 && s.shortWaveDamping<=100 && s.minimumWavelength>=0 &&
    s.maximumWavelength>s.minimumWavelength;
}

bool generateWaterSpectrum(const WaterSpectrumSettings &s,
                          std::vector<std::complex<float>> &amplitudes) {
  if(!validateWaterSpectrum(s)) return false;
  const u32 n=s.resolution;
  std::vector<std::complex<float>> result(static_cast<usize>(n)*n);
  if(s.windSpeed==0) { amplitudes=std::move(result); return true; }
  const double wind=s.windSpeed;
  const double peak=22*std::cbrt(Gravity*Gravity/(wind*s.fetch));
  const double alpha=.076*std::pow(wind*wind/(Gravity*s.fetch),.22);
  const double dk=2*Pi/s.patchLength;
  u32 state=s.seed;
  for(u32 y=0;y<n;++y) for(u32 x=0;x<n;++x) {
    // Keep random sequence independent of spectral band selection.
    const double radius=std::sqrt(-2*std::log(uniform(state)));
    const double randomAngle=2*Pi*uniform(state);
    // Drop Nyquist lines: derivative channels cannot represent their sign
    // unambiguously on an even grid. DC must not change the water level.
    if((x==0 && y==0) || x==n/2 || y==n/2) continue;
    const double kx=(x<n/2?static_cast<int>(x):static_cast<int>(x)-static_cast<int>(n))*dk;
    const double kz=(y<n/2?static_cast<int>(y):static_cast<int>(y)-static_cast<int>(n))*dk;
    const double k=std::hypot(kx,kz), wavelength=2*Pi/k;
    if(wavelength<s.minimumWavelength || wavelength>s.maximumWavelength) continue;
    const double kd=k*s.depth, t=std::tanh(kd);
    const double omega=std::sqrt(Gravity*k*t);
    const double derivative=.5*Gravity*(t+kd*(1-t*t))/omega;
    const double sigma=omega<=peak?.07:.09;
    const double relative=(omega-peak)/(sigma*peak);
    const double enhancement=std::pow(3.3,std::exp(-.5*relative*relative));
    const double ratio=peak/omega;
    const double jonswap=alpha*Gravity*Gravity/std::pow(omega,5)*
      std::exp(-1.25*std::pow(ratio,4))*enhancement;
    const double wh=std::min(omega*std::sqrt(s.depth/Gravity),2.0);
    const double attenuation=wh<=1?.5*wh*wh:1-.5*(2-wh)*(2-wh);
    const double p=omega/peak;
    const double exponent=omega<=peak?6.97*std::pow(p,4.06):
      9.77*std::pow(p,-2.33-1.45*(wind*peak/Gravity-1.17));
    const double shape=std::min(exponent+16*std::tanh(peak/omega)*s.swell*s.swell,10000.0);
    // Exact normalization of cos(theta/2)^(2s), evaluated in log space.
    const double normalization=std::exp(std::lgamma(shape+1)-std::lgamma(shape+.5))/(2*std::sqrt(Pi));
    const double angle=std::atan2(kz,kx)-s.windDirection;
    const double directional=normalization*std::pow(std::abs(std::cos(angle*.5)),2*shape);
    const double spreading=(1-s.spread)*directional+s.spread/(2*Pi);
    const double density=jonswap*attenuation*spreading*derivative/k*
      std::exp(-k*k*s.shortWaveDamping*s.shortWaveDamping);
    // E[|Gaussian complex|²]=2. The conjugate pair in evolution contributes
    // another factor two. N² compensates the normalized inverse transform.
    const double scale=.5*std::sqrt(std::max(0.0,density)*dk*dk)*n*n;
    const double real=radius*std::cos(randomAngle)*scale;
    const double imaginary=radius*std::sin(randomAngle)*scale;
    if(!std::isfinite(real) || !std::isfinite(imaginary)) return false;
    result[y*n+x]={static_cast<float>(real),static_cast<float>(imaginary)};
  }
  amplitudes=std::move(result); return true;
}

bool WaterSpectralField::initialize(const WaterSpectrumSettings &settings) {
  std::vector<std::complex<float>> amplitudes;
  return generateWaterSpectrum(settings,amplitudes) &&
    initialize(settings.resolution,settings.patchLength,settings.depth,amplitudes);
}
} // namespace ae::renderer
