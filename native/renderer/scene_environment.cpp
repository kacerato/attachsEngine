#include "renderer/scene_environment.h"

#include "renderer/volume_parameters.h"

#include <array>

namespace ae::renderer {
namespace {
float length3(const float value[3]) {
  return std::sqrt(value[0]*value[0]+value[1]*value[1]+value[2]*value[2]);
}

void transformPoint(const float matrix[16], const float point[3], float out[3]) {
  for (u32 row=0;row<3;++row)
    out[row]=matrix[row]*point[0]+matrix[4+row]*point[1]+matrix[8+row]*point[2]+matrix[12+row];
}

void transformVector(const float matrix[16], const float vector[3], float out[3]) {
  for (u32 row=0;row<3;++row)
    out[row]=matrix[row]*vector[0]+matrix[4+row]*vector[1]+matrix[8+row]*vector[2];
}

float distanceOutside(const SceneEnvironmentVolume &volume,const float camera[3]) {
  if(volume.shape==EnvironmentVolumeShape::Global) return 0;
  float local[3];transformPoint(volume.worldToLocal,camera,local);
  float deltaLocal[3]{};
  if(volume.shape==EnvironmentVolumeShape::Box) {
    for(u32 axis=0;axis<3;++axis) {
      const float half=volume.boxSize[axis]*.5f;
      deltaLocal[axis]=local[axis]-std::clamp(local[axis],-half,half);
    }
  } else {
    const float radius=length3(local);
    if(radius<=volume.sphereRadius) return 0;
    const float scale=1.0f-volume.sphereRadius/radius;
    for(u32 axis=0;axis<3;++axis) deltaLocal[axis]=local[axis]*scale;
  }
  float deltaWorld[3];transformVector(volume.localToWorld,deltaLocal,deltaWorld);
  return length3(deltaWorld);
}

float mix(float from,float to,float amount) {return amount>=1.0f?to:from+(to-from)*amount;}

float volumeInfluence(const SceneEnvironmentVolume &volume,const float camera[3]) {
  if(volume.weight<=0) return 0;
  const float distance=distanceOutside(volume,camera);
  if(distance<=0) return volume.weight;
  if(volume.blendDistance<=0 || distance>=volume.blendDistance) return 0;
  return volume.weight*(1.0f-distance/volume.blendDistance);
}

// Curvas: Hermite com as tangentes da chave, como o AnimationCurve.Evaluate
// (o mesmo cálculo do editor de curvas, scene/script_curve.h).
double curveTangent(const ColorCurve &curve,u32 i,bool incoming) {
  const auto &k=curve.keys[i];const u32 mode=incoming?k.left:k.right;
  const auto *previous=i?&curve.keys[i-1]:nullptr,*next=i+1<curve.count?&curve.keys[i+1]:nullptr;
  const double before=previous?(double(k.value)-previous->value)/(double(k.time)-previous->time):0;
  const double after=next?(double(next->value)-k.value)/(double(next->time)-k.time):0;
  const double smooth=previous&&next?(double(next->value)-previous->value)/(double(next->time)-previous->time):0;
  if(mode==2) return incoming?before:after;          // Linear
  if(mode==1) return smooth;                          // Auto
  if(mode==4)                                         // ClampedAuto
    return before*after>0?std::copysign(std::min(std::abs(smooth),3*std::min(std::abs(before),std::abs(after))),smooth):0;
  if(!incoming && !k.broken && k.left==0 && k.right==0) return k.in;
  return incoming?k.in:k.out;
}
float hermite(const ColorCurveKey &a,const ColorCurveKey &b,double outgoing,double incoming,double time) {
  if(a.right==3 || b.left==3) return time>=b.time?b.value:a.value; // Constant
  const double dt=double(b.time)-a.time;
  if(dt<=0) return b.value;
  const double s=(time-a.time)/dt,s2=s*s,s3=s2*s;
  return static_cast<float>((2*s3-3*s2+1)*a.value+(s3-2*s2+s)*dt*outgoing+(-2*s3+3*s2)*b.value+(s3-s2)*dt*incoming);
}
} // namespace

bool colorCurveNeutral(u32 channel,const ColorCurve &curve) noexcept {
  if(curve.count==0) return true;
  // Neutro quando a curva amostrada coincide com a sem chaves: a diagonal
  // nas de canal, 0,5 nas de matiz e saturação.
  for(u32 i=0;i<=32;++i) {
    const float t=static_cast<float>(i)/32.0f;
    const float expected=colorCurveIsDiagonal(channel)?t:0.5f;
    if(std::abs(evaluateColorCurve(channel,curve,t)-expected)>1.0e-4f) return false;
  }
  return true;
}

float evaluateColorCurve(u32 channel,const ColorCurve &curve,float time) noexcept {
  if(!std::isfinite(time)) time=0;
  if(curve.count==0) return colorCurveIsDiagonal(channel)?std::clamp(time,0.0f,1.0f):0.5f;
  if(curve.count==1) return curve.keys[0].value;
  const auto &first=curve.keys[0],&last=curve.keys[curve.count-1];
  if(colorCurveLoops(channel)) {
    // TextureCurve com loop: o domínio repete de 0 a 1; entre a última chave e
    // a primeira (+1) a curva continua pela volta.
    time-=std::floor(time);
    if(time<first.time || time>last.time) {
      ColorCurveKey a=last,b=first;
      double t=time;
      if(time<first.time) {a.time-=1.0f;} else {b.time+=1.0f;}
      return hermite(a,b,curveTangent(curve,curve.count-1,false),curveTangent(curve,0,true),t);
    }
  } else time=std::clamp(time,first.time,last.time);
  u32 i=1;
  while(i<curve.count-1 && curve.keys[i].time<time) ++i;
  return hermite(curve.keys[i-1],curve.keys[i],curveTangent(curve,i-1,false),curveTangent(curve,i,true),time);
}

void updateColorCurveTangents(ColorCurve &curve) noexcept {
  for(u32 i=0;i<curve.count;++i) {
    const double incoming=curveTangent(curve,i,true),outgoing=curveTangent(curve,i,false);
    curve.keys[i].in=static_cast<float>(std::clamp(incoming,-1.0e6,1.0e6));
    curve.keys[i].out=static_cast<float>(std::clamp(outgoing,-1.0e6,1.0e6));
  }
}

namespace {
} // namespace

SceneEnvironment defaultSceneEnvironment() {
  SceneEnvironment result{};
  result.active=true;
  result.syncEffectSwitches();
  return result;
}

float sceneEnvironmentVolumeInfluence(const SceneEnvironmentVolume &volume,const float cameraPosition[3]) {
  return volume.valid()?std::clamp(volumeInfluence(volume,cameraPosition),0.0f,1.0f):0.0f;
}

SceneEnvironment resolveSceneEnvironment(std::span<const SceneEnvironmentVolume> volumes,
                                         const float cameraPosition[3],u32 layerMask,
                                         SceneEnvironmentBlendReport *report) {
  if(report) *report={};
  std::vector<const SceneEnvironmentVolume*> ordered;
  ordered.reserve(volumes.size());
  for(const auto &volume:volumes) {
    if(!volume.valid() || !volume.environment.active || volume.layer>31 || !(layerMask&(1u<<volume.layer))) continue;
    ordered.push_back(&volume);if(report) ++report->considered;
  }
  std::stable_sort(ordered.begin(),ordered.end(),[](const auto *a,const auto *b) {
    if(a->environment.priority!=b->environment.priority)
      return a->environment.priority<b->environment.priority;
    // Menor identidade era o desempate do ambiente global v1; aplicá-la por
    // último preserva esse contrato ao migrar para mistura de volumes.
    return a->stableId>b->stableId;
  });
  // Sem volume, os padrões do mundo (o Default Volume Profile): parâmetros
  // desmarcados nunca vazam do primeiro volume da lista.
  SceneEnvironment result=defaultSceneEnvironment();
  bool initialized=false;
  // Neblina e nuvens entram e saem pela "presença", como antes pela densidade
  // e pela opacidade: um volume que as desliga as apaga aos poucos.
  float fogPresence=result.fog?1.0f:0.0f,cloudPresence=result.clouds.enabled?1.0f:0.0f;
  const auto parameters=volumeParameters();
  const u32 fogIndex=volumeParameterIndex("fog"),cloudIndex=volumeParameterIndex("clouds");
  const u32 mapIndex=volumeParameterIndex("environment_map"),mapRotationIndex=volumeParameterIndex("hdri_rotation_degrees"),
            mapExposureIndex=volumeParameterIndex("hdri_exposure_ev");
  for(const auto *volume:ordered) {
    const float amount=sceneEnvironmentVolumeInfluence(*volume,cameraPosition);
    if(amount<=0) continue;
    initialized=true;
    if(report) {
      ++report->contributing;
      if(amount>report->dominantInfluence ||
         (amount==report->dominantInfluence && (!report->dominant || volume->stableId<report->dominant))) {
        report->dominant=volume->stableId;report->dominantInfluence=amount;
      }
    }
    // Nuvens: quem as desliga só apaga a opacidade (a forma fica); quem as
    // traz para um céu sem nuvens traz a forma inteira e só a opacidade cresce.
    const bool cloudsArrive=cloudPresence<=0.0f&&volume->environment.clouds.enabled;
    // O panorama troca com t > 0 e leva junto a orientação e a exposição dele:
    // misturar o giro de um HDRI com o de outro não tem sentido.
    const bool mapSwaps=volume->overrides.contributes(mapIndex)&&
                        volume->environment.environmentMap!=result.environmentMap;
    for(u32 index=0;index<parameters.size();++index) {
      if(!volume->overrides.contributes(index)) continue;
      if(mapSwaps&&(index==mapRotationIndex||index==mapExposureIndex)) {
        blendVolumeParameter(parameters[index],result,volume->environment,1.0f);continue;
      }
      if(parameters[index].group==VolumeGroup::Clouds&&index!=cloudIndex) {
        if(!volume->environment.clouds.enabled) continue;
        blendVolumeParameter(parameters[index],result,volume->environment,cloudsArrive?1.0f:amount);
        continue;
      }
      if(index==fogIndex) {fogPresence=mix(fogPresence,volume->environment.fog?1.0f:0.0f,amount);continue;}
      if(index==cloudIndex) {cloudPresence=mix(cloudPresence,volume->environment.clouds.enabled?1.0f:0.0f,amount);continue;}
      blendVolumeParameter(parameters[index],result,volume->environment,amount);
    }
  }
  result.fog=fogPresence>0.0f;
  result.fogDensity*=fogPresence;
  result.clouds.opacity*=cloudPresence;
  result.clouds.enabled=cloudPresence>0.0f&&result.clouds.opacity>0.0f;
  result.syncEffectSwitches();
  result.active=initialized;
  return result;
}
} // namespace ae::renderer
