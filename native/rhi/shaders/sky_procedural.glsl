// Céu procedural como o Skybox/Procedural da Unity: espalhamento atmosférico de
// O'Neil (GPU Gems 2, cap. 16). Mesma conta de renderer/procedural_sky.cpp --
// os dois precisam mudar juntos. A cor sai do ângulo do sol; tom, chão e
// espessura são os parâmetros que a Unity expõe.
//
// Quem inclui define PI. `toSun` aponta do chão para o sol, unitário.

const float AstraSkyOuterRadius=1.025;
const float AstraSkyInnerRadius=1.0;
const float AstraSkyCameraHeight=0.0001;
const float AstraSkyScale=1.0/(AstraSkyOuterRadius-AstraSkyInnerRadius);
const float AstraSkyScaleDepth=0.25;
const float AstraSkyScaleOverScaleDepth=AstraSkyScale/AstraSkyScaleDepth;
const float AstraSkyMie=0.0010;
const float AstraSkySunBrightness=20.0;

float astraSkyScaleFunction(float cosine) {
  float x=1.0-cosine;
  return AstraSkyScaleDepth*exp(-0.00287+x*(0.459+x*(3.83+x*(-6.80+x*5.25))));
}

// `mie` devolve o cOut da Unity (espalhamento Mie no raio): é a cor do sol que
// atravessa a atmosfera, usada pelo disco do Skybox/Procedural.
vec3 astraProceduralSky(vec3 view,vec3 toSun,vec3 tint,vec3 ground,float thickness,float exposure,out vec3 mie) {
  mie=vec3(0.0);
  vec3 wavelength=vec3(0.65,0.57,0.475)+0.15*((1.0-clamp(tint,0.0,1.0))*2.0-1.0);
  vec3 invWavelength=1.0/pow(wavelength,vec3(4.0));
  float rayleigh=0.0025*pow(clamp(thickness,0.0,5.0),2.5);
  float krESun=rayleigh*AstraSkySunBrightness,kmESun=AstraSkyMie*AstraSkySunBrightness;
  float kr4Pi=rayleigh*4.0*PI,km4Pi=AstraSkyMie*4.0*PI;
  vec3 camera=vec3(0.0,AstraSkyInnerRadius+AstraSkyCameraHeight,0.0);
  vec3 front=vec3(0.0);
  if(view.y>=0.0) {
    float far=sqrt(AstraSkyOuterRadius*AstraSkyOuterRadius+AstraSkyInnerRadius*AstraSkyInnerRadius*view.y*view.y-
                   AstraSkyInnerRadius*AstraSkyInnerRadius)-AstraSkyInnerRadius*view.y;
    float height=AstraSkyInnerRadius+AstraSkyCameraHeight;
    float startOffset=exp(AstraSkyScaleOverScaleDepth*(-AstraSkyCameraHeight))*
                      astraSkyScaleFunction(dot(view,camera)/height);
    float sampleLength=far*0.5,scaledLength=sampleLength*AstraSkyScale;
    vec3 point=camera+view*sampleLength*0.5;
    for(int i=0;i<2;++i) {
      float h=length(point);
      float d=exp(AstraSkyScaleOverScaleDepth*(AstraSkyInnerRadius-h));
      float scatter=startOffset+d*(astraSkyScaleFunction(dot(toSun,point)/h)-astraSkyScaleFunction(dot(view,point)/h));
      vec3 attenuate=exp(-clamp(scatter,0.0,50.0)*(invWavelength*kr4Pi+km4Pi));
      front+=attenuate*d*scaledLength;
      point+=view*sampleLength;
    }
    float cosine=dot(toSun,view);
    mie=front*kmESun;
    return exposure*front*invWavelength*krESun*(0.75*(1.0+cosine*cosine));
  }
  float far=-AstraSkyCameraHeight/min(-0.001,view.y);
  float depth=exp(-AstraSkyCameraHeight/AstraSkyScaleDepth);
  float cameraScale=astraSkyScaleFunction(-view.y),lightScale=astraSkyScaleFunction(toSun.y);
  float cameraOffset=depth*cameraScale,temp=lightScale+cameraScale;
  float sampleLength=far*0.5,scaledLength=sampleLength*AstraSkyScale;
  vec3 point=camera+view*sampleLength*0.5;
  vec3 attenuate=vec3(0.0);
  for(int i=0;i<2;++i) {
    float h=length(point);
    float d=exp(AstraSkyScaleOverScaleDepth*(AstraSkyInnerRadius-h));
    float scatter=d*temp-cameraOffset;
    attenuate=exp(-clamp(scatter,0.0,50.0)*(invWavelength*kr4Pi+km4Pi));
    front+=attenuate*d*scaledLength;
    point+=view*sampleLength;
  }
  vec3 cIn=front*(invWavelength*krESun+kmESun);
  return exposure*(cIn+ground*ground*clamp(attenuate,0.0,1.0));
}

vec3 astraProceduralSky(vec3 view,vec3 toSun,vec3 tint,vec3 ground,float thickness,float exposure) {
  vec3 mie;
  return astraProceduralSky(view,toSun,tint,ground,thickness,exposure,mie);
}

// PK-2 (A-08): o disco do Skybox/Procedural da Unity (calcSunAttenuation).
// mode 1 = Simple (mancha de raio Sun Size), 2 = High Quality (fase de Mie
// focada por Sun Size Convergence; Sun Size abre a auréola).
float astraProceduralSunAttenuation(vec3 view,vec3 toSun,float mode,float size,float convergence) {
  if(mode<0.5) return 0.0;
  if(mode<1.5) {
    float spot=1.0-smoothstep(0.0,max(size,1.0e-4),length(toSun-view));
    return spot*spot;
  }
  const float g=-0.990,g2=0.9801;
  float focused=pow(clamp(dot(toSun,view),0.0,1.0),max(convergence,1.0));
  float temp=1.0+g2-2.0*g*(-focused);
  temp=max(pow(temp,pow(max(size,1.0e-4),0.65)*10.0),1.0e-4);
  return 1.5*((1.0-g2)/(2.0+g2))*(1.0+focused*focused)/temp;
}
