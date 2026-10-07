// DSP do mixer da Astra (bloco H, F061): processamento estéreo intercalado a
// 48 kHz, sem alocação no caminho de áudio. Usado pelo nó de bus de
// runtime/scene_audio.cpp e testado isoladamente.
//
// Referências estudadas:
// - Filtros: "Cookbook formulae for audio EQ biquad filter coefficients" (R. Bristow-Johnson),
//   os mesmos tipos do AudioEffectFilter da Godot 4.5
//   https://docs.godotengine.org/en/4.5/classes/class_audioeffectfilter.html
// - Eco: Unity 6000.0 Audio Echo Effect https://docs.unity3d.com/6000.0/Documentation/Manual/class-AudioEchoEffect.html
// - Reverb: Freeverb (Jezar at Dreampoint, domínio público), base do AudioEffectReverb da Godot 4.5
//   https://docs.godotengine.org/en/4.5/classes/class_audioeffectreverb.html
// - Compressor: AudioEffectCompressor da Godot 4.5 (limiar, razão, ataque, liberação, ganho, sidechain)
//   https://docs.godotengine.org/en/4.5/classes/class_audioeffectcompressor.html
// - Espacialização: Unity AudioSource (Spatial Blend, curvas de atenuação, cone, Doppler)
//   https://docs.unity3d.com/6000.0/Documentation/Manual/class-AudioSource.html
#pragma once
#include "core/base.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ae::runtime::audio_dsp {

inline constexpr float SampleRate=48000.f;
inline constexpr float Pi=3.14159265358979f;
inline float dbToGain(float db) {return std::pow(10.f,db/20.f);}
inline float gainToDb(float gain) {return gain>1e-6f?20.f*std::log10(gain):-120.f;}

enum class FilterMode : u32 {LowPass=0,HighPass=1,BandPass=2,Notch=3,Peak=4,LowShelf=5,HighShelf=6};

// Biquad estéreo (forma direta II transposta). Os coeficientes só são
// recalculados quando um parâmetro muda.
struct Biquad {
  float b0=1,b1=0,b2=0,a1=0,a2=0;
  float z1[2]{},z2[2]{};
  FilterMode mode=FilterMode::LowPass;float cutoff=-1,q=-1,gainDb=1e9f;
  void configure(FilterMode m,float hz,float resonance,float db) {
    hz=std::clamp(hz,10.f,SampleRate*.49f);resonance=std::clamp(resonance,.05f,40.f);
    if(m==mode&&hz==cutoff&&resonance==q&&db==gainDb) return;
    mode=m;cutoff=hz;q=resonance;gainDb=db;
    const float w=2*Pi*hz/SampleRate,c=std::cos(w),s=std::sin(w),alpha=s/(2*resonance),A=std::pow(10.f,db/40.f);
    float B0=1,B1=0,B2=0,A0=1,A1=0,A2=0;
    switch(m) {
      case FilterMode::LowPass: B0=(1-c)/2;B1=1-c;B2=(1-c)/2;A0=1+alpha;A1=-2*c;A2=1-alpha;break;
      case FilterMode::HighPass: B0=(1+c)/2;B1=-(1+c);B2=(1+c)/2;A0=1+alpha;A1=-2*c;A2=1-alpha;break;
      case FilterMode::BandPass: B0=alpha;B1=0;B2=-alpha;A0=1+alpha;A1=-2*c;A2=1-alpha;break;
      case FilterMode::Notch: B0=1;B1=-2*c;B2=1;A0=1+alpha;A1=-2*c;A2=1-alpha;break;
      case FilterMode::Peak: B0=1+alpha*A;B1=-2*c;B2=1-alpha*A;A0=1+alpha/A;A1=-2*c;A2=1-alpha/A;break;
      case FilterMode::LowShelf: {const float r=2*std::sqrt(A)*alpha;
        B0=A*((A+1)-(A-1)*c+r);B1=2*A*((A-1)-(A+1)*c);B2=A*((A+1)-(A-1)*c-r);A0=(A+1)+(A-1)*c+r;A1=-2*((A-1)+(A+1)*c);A2=(A+1)+(A-1)*c-r;break;}
      case FilterMode::HighShelf: {const float r=2*std::sqrt(A)*alpha;
        B0=A*((A+1)+(A-1)*c+r);B1=-2*A*((A-1)+(A+1)*c);B2=A*((A+1)+(A-1)*c-r);A0=(A+1)-(A-1)*c+r;A1=2*((A-1)-(A+1)*c);A2=(A+1)-(A-1)*c-r;break;}
    }
    b0=B0/A0;b1=B1/A0;b2=B2/A0;a1=A1/A0;a2=A2/A0;
  }
  void process(float *stereo,u32 frames) {
    for(u32 f=0;f<frames;++f) for(u32 ch=0;ch<2;++ch) {
      float &x=stereo[f*2+ch];const float y=b0*x+z1[ch];
      z1[ch]=b1*x-a1*y+z2[ch];z2[ch]=b2*x-a2*y;x=y;
    }
  }
  void reset() {z1[0]=z1[1]=z2[0]=z2[1]=0;}
};

// Eco: linha de atraso estéreo com realimentação.
struct Echo {
  static constexpr u32 MaximumFrames=static_cast<u32>(SampleRate*5);
  std::vector<float> line=std::vector<float>(MaximumFrames*2,0.f);u32 cursor=0;
  void process(float *stereo,u32 frames,float delayMs,float feedback,float wet,float dry) {
    const u32 delay=std::clamp<u32>(static_cast<u32>(delayMs*SampleRate/1000.f),1,MaximumFrames-1);
    feedback=std::clamp(feedback,0.f,.98f);
    for(u32 f=0;f<frames;++f) {
      const u32 read=(cursor+MaximumFrames-delay)%MaximumFrames;
      for(u32 ch=0;ch<2;++ch) {
        const float x=stereo[f*2+ch],d=line[read*2+ch];
        line[cursor*2+ch]=x+d*feedback;stereo[f*2+ch]=x*dry+d*wet;
      }
      cursor=(cursor+1)%MaximumFrames;
    }
  }
  void reset() {std::fill(line.begin(),line.end(),0.f);cursor=0;}
};

// Freeverb: 8 filtros pente com amortecimento e 4 passa-tudo por canal,
// afinações originais (44,1 kHz) escaladas para 48 kHz; espalhamento estéreo 23.
struct Reverb {
  struct Comb {std::vector<float> buffer;u32 index=0;float store=0;
    float run(float input,float feedback,float damp) {
      const float out=buffer[index];store=out*(1-damp)+store*damp;buffer[index]=input+store*feedback;
      if(++index>=buffer.size()) {index=0;}
      return out;}};
  struct Allpass {std::vector<float> buffer;u32 index=0;
    float run(float input) {const float b=buffer[index];buffer[index]=input+b*.5f;if(++index>=buffer.size()) {index=0;}
      return b-input;}};
  std::array<Comb,8> combs[2];std::array<Allpass,4> allpasses[2];
  std::vector<float> predelay=std::vector<float>(static_cast<usize>(SampleRate/2)*2,0.f);u32 predelayCursor=0;
  Reverb() {
    static constexpr u32 combTuning[8]{1116,1188,1277,1356,1422,1491,1557,1617},allpassTuning[4]{556,441,341,225};
    const auto scale=[](u32 n){return std::max<u32>(1,static_cast<u32>(std::lround(n*SampleRate/44100.f)));};
    for(u32 ch=0;ch<2;++ch) {
      for(u32 i=0;i<8;++i) combs[ch][i].buffer.assign(scale(combTuning[i]+ch*23),0.f);
      for(u32 i=0;i<4;++i) allpasses[ch][i].buffer.assign(scale(allpassTuning[i]+ch*23),0.f);
    }
  }
  void process(float *stereo,u32 frames,float roomSize,float damping,float width,float predelayMs,float wet,float dry) {
    const float feedback=std::clamp(roomSize,0.f,1.f)*.28f+.7f,damp=std::clamp(damping,0.f,1.f)*.4f;
    width=std::clamp(width,0.f,1.f);const float wet1=wet*3*(width/2+.5f),wet2=wet*3*((1-width)/2);
    const u32 size=static_cast<u32>(predelay.size()/2);
    const u32 delay=std::min(size-1,static_cast<u32>(std::max(0.f,predelayMs)*SampleRate/1000.f));
    for(u32 f=0;f<frames;++f) {
      const float inL=stereo[f*2],inR=stereo[f*2+1];
      predelay[predelayCursor*2]=inL;predelay[predelayCursor*2+1]=inR;
      const u32 read=(predelayCursor+size-delay)%size;predelayCursor=(predelayCursor+1)%size;
      const float input=(predelay[read*2]+predelay[read*2+1])*.015f;
      float out[2]{};
      for(u32 ch=0;ch<2;++ch) {
        for(auto &c:combs[ch]) out[ch]+=c.run(input,feedback,damp);
        for(auto &a:allpasses[ch]) out[ch]=a.run(out[ch]);
      }
      stereo[f*2]=out[0]*wet1+out[1]*wet2+inL*dry;
      stereo[f*2+1]=out[1]*wet1+out[0]*wet2+inR*dry;
    }
  }
};

// Compressor de realimentação direta com detector de pico. O detector lê o
// próprio sinal ou, com sidechain, o nível medido de outro bus (ducking).
struct Compressor {
  float envelope=0,reductionDb=0;
  void process(float *stereo,u32 frames,float thresholdDb,float ratio,float attackMs,float releaseMs,float makeupDb,float mix,
               const float *sidechainLevel) {
    const float attack=std::exp(-1.f/(std::max(.01f,attackMs)*.001f*SampleRate));
    const float release=std::exp(-1.f/(std::max(.1f,releaseMs)*.001f*SampleRate));
    const float slope=1-1/std::max(1.f,ratio),makeup=dbToGain(makeupDb);mix=std::clamp(mix,0.f,1.f);
    for(u32 f=0;f<frames;++f) {
      const float detector=sidechainLevel?*sidechainLevel:std::max(std::abs(stereo[f*2]),std::abs(stereo[f*2+1]));
      envelope=detector>envelope?attack*envelope+(1-attack)*detector:release*envelope+(1-release)*detector;
      const float over=gainToDb(envelope)-thresholdDb;
      reductionDb=over>0?over*slope:0;
      const float gain=dbToGain(-reductionDb)*makeup;
      for(u32 ch=0;ch<2;++ch) {float &x=stereo[f*2+ch];x=x*(1-mix)+x*gain*mix;}
    }
  }
};

// Espacialização calculada pela Astra (não pelo spatializer do miniaudio):
// a mistura espacial interpola entre o resultado 2D e o 3D, como o Spatial
// Blend da Unity. Coordenadas: +X direita, +Y cima, +Z frente.
enum class Rolloff : u32 {Linear=0,Inverse=1,Exponential=2};
struct SpatialInput {
  float listener[3]{},listenerForward[3]{0,0,1},listenerUp[3]{0,1,0},listenerVelocity[3]{};
  float source[3]{},sourceForward[3]{0,0,1},sourceVelocity[3]{};
  Rolloff rolloff=Rolloff::Inverse;float minDistance=1,maxDistance=100,rolloffFactor=1;
  float coneInner=360,coneOuter=360,coneGain=0,doppler=1,pan=0,blend=1;
};
struct SpatialOutput {float gain=1,pan=0,pitch=1,distance=0;};
inline float distanceGain(Rolloff model,float d,float minimum,float maximum,float factor) {
  d=std::clamp(d,minimum,maximum);
  switch(model) {
    case Rolloff::Linear: return maximum>minimum?std::clamp(1-factor*(d-minimum)/(maximum-minimum),0.f,1.f):1;
    case Rolloff::Exponential: return std::pow(d/minimum,-factor);
    default: return minimum/(minimum+factor*(d-minimum));
  }
}
inline SpatialOutput spatialize(const SpatialInput &in) {
  const auto dot=[](const float *a,const float *b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
  const auto unit=[](float *v){const float l=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(l>1e-6f) for(u32 i=0;i<3;++i) v[i]/=l;return l;};
  float rel[3]{in.source[0]-in.listener[0],in.source[1]-in.listener[1],in.source[2]-in.listener[2]};
  SpatialOutput out;out.distance=unit(rel);
  const float blend=std::clamp(in.blend,0.f,1.f);
  float gain=distanceGain(in.rolloff,out.distance,in.minDistance,in.maxDistance,in.rolloffFactor);
  // Cone: ângulo entre a frente da fonte e a direção fonte → ouvinte.
  if(in.coneInner<360||in.coneOuter<360) {
    float forward[3]{in.sourceForward[0],in.sourceForward[1],in.sourceForward[2]};unit(forward);
    const float toListener[3]{-rel[0],-rel[1],-rel[2]};
    const float angle=out.distance>1e-6f?std::acos(std::clamp(dot(forward,toListener),-1.f,1.f))*360.f/Pi:0;  // ângulo total do cone
    const float cone=angle<=in.coneInner?1:angle>=in.coneOuter?in.coneGain:
      1+(in.coneGain-1)*(angle-in.coneInner)/std::max(1e-3f,in.coneOuter-in.coneInner);
    gain*=cone;
  }
  // Direita do ouvinte = cima × frente (+X quando frente é +Z e cima é +Y).
  const float *f=in.listenerForward,*u=in.listenerUp;
  float right[3]{u[1]*f[2]-u[2]*f[1],u[2]*f[0]-u[0]*f[2],u[0]*f[1]-u[1]*f[0]};unit(right);
  const float spatialPan=out.distance>1e-6f?std::clamp(dot(rel,right),-1.f,1.f):0;
  float pitch=1;
  if(in.doppler>0&&out.distance>1e-6f) {
    // OpenAL 1.1: velocidades projetadas no vetor fonte → ouvinte; fonte se
    // aproximando ou ouvinte se aproximando sobem o tom.
    constexpr float speed=343.3f;const float toListener[3]{-rel[0],-rel[1],-rel[2]};
    const float vl=std::min(dot(in.listenerVelocity,toListener),speed/in.doppler*.99f);
    const float vs=std::min(dot(in.sourceVelocity,toListener),speed/in.doppler*.99f);
    pitch=std::clamp((speed-in.doppler*vl)/(speed-in.doppler*vs),.25f,4.f);
  }
  out.gain=1+(gain-1)*blend;out.pan=in.pan+(spatialPan-in.pan)*blend;out.pitch=1+(pitch-1)*blend;
  return out;
}

} // namespace ae::runtime::audio_dsp
