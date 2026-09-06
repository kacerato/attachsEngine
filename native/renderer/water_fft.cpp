#include "renderer/water_fft.h"
#include <algorithm>
#include <cmath>

namespace ae::renderer {
bool WaterFftPlan::initialize(u32 size) {
  if(size < 2 || size > 2048 || (size & (size-1)) != 0) return false;
  // Build locally so invalid authoring or allocation failure never publishes
  // a partially configured plan.
  std::vector<u32> permutation(size);
  std::vector<std::complex<float>> roots(size/2);
  u32 bits=0;
  for(u32 remaining=size; remaining>1; remaining>>=1) ++bits;
  for(u32 i=0;i<size;++i) {
    u32 reversed=0, value=i;
    for(u32 bit=0;bit<bits;++bit) { reversed=(reversed<<1)|(value&1); value>>=1; }
    permutation[i]=reversed;
  }
  for(u32 i=0;i<size/2;++i) {
    const double angle=-6.2831853071795864769*static_cast<double>(i)/size;
    roots[i]={static_cast<float>(std::cos(angle)),static_cast<float>(std::sin(angle))};
  }
  permutation_=std::move(permutation); roots_=std::move(roots); size_=size;
  return true;
}

void WaterFftPlan::transformStrided(std::complex<float>* data, usize stride, bool inverse) const noexcept {
  for(u32 i=0;i<size_;++i)
    if(i<permutation_[i]) std::swap(data[i*stride],data[permutation_[i]*stride]);
  for(u32 width=2;width<=size_;width<<=1) {
    const u32 half=width/2, step=size_/width;
    for(u32 start=0;start<size_;start+=width) for(u32 i=0;i<half;++i) {
      const auto root=inverse?std::conj(roots_[i*step]):roots_[i*step];
      const auto even=data[(start+i)*stride];
      const auto odd=data[(start+i+half)*stride]*root;
      data[(start+i)*stride]=even+odd;
      data[(start+i+half)*stride]=even-odd;
    }
  }
  if(inverse) for(u32 i=0;i<size_;++i) data[i*stride]/=static_cast<float>(size_);
}

bool WaterFftPlan::transform(std::span<std::complex<float>> data, bool inverse) const noexcept {
  if(size_==0 || data.size()!=size_) return false;
  transformStrided(data.data(),1,inverse); return true;
}

bool WaterFftPlan::transform2D(std::span<std::complex<float>> data, bool inverse) const noexcept {
  if(size_==0 || data.size()!=static_cast<usize>(size_)*size_) return false;
  for(u32 row=0;row<size_;++row) transformStrided(data.data()+row*size_,1,inverse);
  for(u32 column=0;column<size_;++column) transformStrided(data.data()+column,size_,inverse);
  return true;
}
bool WaterSpectralField::initialize(u32 size, float patchLength, float depth,
    std::span<const std::complex<float>> initialSpectrum) {
  if(!std::isfinite(patchLength) || patchLength<=0 || !std::isfinite(depth) || depth<=0 ||
      initialSpectrum.size()!=static_cast<usize>(size)*size) return false;
  for(auto value:initialSpectrum)
    if(!std::isfinite(value.real()) || !std::isfinite(value.imag())) return false;
  WaterFftPlan plan;
  if(!plan.initialize(size)) return false;
  std::vector<std::complex<float>> initial(initialSpectrum.begin(),initialSpectrum.end());
  std::vector<std::complex<float>> spatial(initial.size());
  std::vector<double> frequencies(initial.size());
  decltype(channels_) channels;
  for(auto &channel:channels) channel.resize(initial.size());
  for(u32 y=0;y<size;++y) for(u32 x=0;x<size;++x) {
    const int kx=x<=size/2?static_cast<int>(x):static_cast<int>(x)-static_cast<int>(size);
    const int ky=y<=size/2?static_cast<int>(y):static_cast<int>(y)-static_cast<int>(size);
    const double k=6.2831853071795864769*std::hypot(kx,ky)/patchLength;
    frequencies[y*size+x]=std::sqrt(9.81*k*std::tanh(k*depth));
  }
  plan_=std::move(plan); initial_=std::move(initial); spatial_=std::move(spatial);
  angularFrequency_=std::move(frequencies);
  channels_=std::move(channels); patchLength_=patchLength;
  return true;
}

bool WaterSpectralField::update(double timeSeconds) noexcept {
  const u32 size=plan_.size();
  if(size==0 || !std::isfinite(timeSeconds)) return false;
  for(u32 y=0;y<size;++y) for(u32 x=0;x<size;++x) {
    const usize index=y*size+x;
    const usize opposite=((size-y)%size)*size+(size-x)%size;
    const double phase=std::remainder(angularFrequency_[index]*timeSeconds,6.2831853071795864769);
    if(!std::isfinite(phase)) return false;
    const std::complex<float> rotation{static_cast<float>(std::cos(phase)),static_cast<float>(std::sin(phase))};
    spatial_[index]=initial_[index]*rotation+std::conj(initial_[opposite])*std::conj(rotation);
    const float dk=6.2831853071795864769f/patchLength_;
    // Odd derivative symbols must vanish at Nyquist to retain Hermitian symmetry.
    const float kx=x==size/2?0.0f:dk*(x<size/2?static_cast<int>(x):static_cast<int>(x)-static_cast<int>(size));
    const float kz=y==size/2?0.0f:dk*(y<size/2?static_cast<int>(y):static_cast<int>(y)-static_cast<int>(size));
    const float k=std::hypot(kx,kz), inverseK=k>0?1/k:0;
    const auto h=spatial_[index];
    const auto ih=std::complex<float>(-h.imag(),h.real());
    // Positive choppiness pulls samples toward crests (compression at maxima).
    channels_[0][index]=ih*(kx*inverseK);
    channels_[1][index]=ih*(kz*inverseK);
    channels_[2][index]=ih*kx;
    channels_[3][index]=ih*kz;
    channels_[4][index]=-h*(kx*kx*inverseK);
    channels_[5][index]=-h*(kx*kz*inverseK);
    channels_[6][index]=-h*(kz*kz*inverseK);
  }
  if(!plan_.transform2D(spatial_,true)) return false;
  for(auto &channel:channels_) if(!plan_.transform2D(channel,true)) return false;
  return true;
}

std::span<const std::complex<float>> WaterSpectralField::channel(Channel channel) const noexcept {
  const auto index=static_cast<usize>(channel);
  return index<channels_.size()?std::span<const std::complex<float>>(channels_[index]):std::span<const std::complex<float>>{};
}

float WaterSpectralField::jacobian(usize sample, float choppiness) const noexcept {
  if(sample>=spatial_.size() || !std::isfinite(choppiness)) return 1;
  const float xx=channels_[4][sample].real()*choppiness;
  const float xz=channels_[5][sample].real()*choppiness;
  const float zz=channels_[6][sample].real()*choppiness;
  return (1+xx)*(1+zz)-xz*xz;
}
} // namespace ae::renderer
