#include "dirt_road_frame.glsl"
layout(set=1,binding=0,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters;
  vec4 skyZenithCloudCoverage;
  vec4 skyHorizonCloudDensity;
  vec4 groundColorSaturation;
  vec4 cloudLightWindSpeed;
  vec4 sceneSky;
  vec4 sceneFogColorDensity;
  vec4 sceneFog;
  vec4 scenePost;
  vec4 sceneAo;
  vec4 sceneAoDetail;
  vec4 worldToViewRow0;
  vec4 worldToViewRow1;
  vec4 worldToViewRow2;
  vec4 quality;
  mat4 shadowViewProjection[4];
  vec4 shadowSplitDepths;
  vec4 shadowParameters;
  vec4 shadowWorldUnitsPerTexel;
  vec4 shadowFilterParameters; // xy=PCF radii, zw=temporal jitter in physical NDC
  vec4 shadowTransitionParameters;
  vec4 materialDistanceParameters;
  vec4 waterParameters;
  vec4 waterOptics;
  vec4 waterDeepColorFoam;
  vec4 waterShallowColorDistance;
  vec4 waterAbsorption;
  vec4 waterWaveShape[8];
  vec4 waterWaveMotion[8];
  vec4 waterInteractionParameters;
  vec4 waterInteractionShape[8];
  vec4 waterInteractionMotion[8];
  // centro x, centro z, lado da area em metros, resolucao da grade. Precisa
  // casar com a copia em environment_lighting.glsl e com DirtRoadFrameUniform:
  // o bloco e declarado uma vez por estagio e as tres descrevem a mesma memoria.
  vec4 waterRippleArea;
  // espuma elevada, cobertura, deslocamento micro, comprimento micro
  vec4 waterSurfaceDetail;
} environment;
#ifdef AETHER_WATER_RIPPLES
#include "water_ripple_sampling.glsl"
#endif
#ifdef AETHER_SPECTRAL_WATER
#include "water_spectral_sampling.glsl"
#endif
layout(location=0) in vec3 inPosition;
layout(location=1) in vec4 inNormal;
layout(location=2) in vec4 inTangent;
layout(location=3) in vec2 inUv0;
layout(location=4) in vec2 inUv1;
layout(location=5) in vec4 inColor;
layout(location=6) in mat4 inModel;
layout(location=10) in vec4 inTint;
layout(location=11) in vec4 inNormalColumn0;
layout(location=12) in vec4 inNormalColumn1;
layout(location=13) in vec4 inNormalColumn2;
// Precisao dos varyings (item 2.2.5). Cada varying e gravado na memoria de tile
// e reinterpolado por fragmento: em fp16 esse trafego cai pela metade, e sao 12
// floats por pixel entre normal, tangente, cor e dither.
//
// vPosition e vUv permanecem highp e nao por simetria: vPosition alimenta o
// vetor de visao em coordenadas de mundo, num mapa de centenas de unidades, e as
// UV enderecam texturas de ate 4096 px, onde a mantissa do fp16 (~2048 passos em
// [0,1]) ja nao resolve um texel.
layout(location=0) out highp vec3 vPosition;
layout(location=1) out mediump vec3 vNormal;
layout(location=2) out mediump vec4 vTangent;
layout(location=3) out highp vec2 vUv0;
layout(location=4) out highp vec2 vUv1;
layout(location=5) out mediump vec4 vColor;
// LOD dither cross-fade factor (see renderer::selectLodLevel /
// PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md): [-1,1], 0 = fully opaque, positive =
// outgoing mask, negative = incoming complementary mask. Piggybacks
// on GpuMeshInstance.normalColumns[7] (column 1's .w), which
// buildNormalMatrix leaves at 0 unconditionally -- column 0's .w is the only
// one carrying real data (tangent handedness sign), so this slot never grew
// the 128-byte instance struct or its vertex binding stride.
layout(location=6) out mediump float vDither;
#ifdef AETHER_WATER_RIPPLES
layout(location=9) out highp vec2 vRippleGeometrySlope;
#endif
#ifdef AETHER_SPECTRAL_WATER
layout(location=7) out mediump float vSpectralFoam;
layout(location=8) out highp vec3 vSpectralCoordinates; // undisplaced XZ and mesh spacing
#endif
const uint MATERIAL_IMPOSTOR=256u; // renderer::MapMaterialImpostor
const uint MATERIAL_WATER=512u; // renderer::MapMaterialWater
void main() {
#ifdef AETHER_WATER_RIPPLES
  vRippleGeometrySlope=vec2(0);
#endif
#ifdef AETHER_SPECTRAL_WATER
  vSpectralFoam=0;
  vSpectralCoordinates=vec3(0);
#endif
  // Posicao, view e clip permanecem highp em toda a cadeia: fp16 em coordenada
  // de mundo ou de clip produz tremor de vertice e z-fighting visiveis, e a
  // subtracao relativa a camera e exatamente o caso de cancelamento catastrofico.
  // Impostor de folhagem: o quad chega em espaco local, centrado na origem, e
  // gira em torno de Y para encarar a camera ANTES da matriz de mundo. O bake
  // v2 guarda varias vistas azimutais; os 16 bits superiores de materialFlags.y
  // descrevem o macro-tile e UV1 guarda sua origem no atlas.
  //
  // Normal e tangente giram com a posicao. Sem isso o quad encararia a camera
  // mas continuaria iluminado como se ainda encarasse a direcao em que foi
  // assado, e a folhagem distante alternaria entre clara e escura conforme o
  // giro -- defeito mais visivel que a troca de LOD que o impostor resolve.
  highp vec3 modelPosition=inPosition;
  highp vec3 modelNormal=inNormal.xyz;
  highp vec3 modelTangent=inTangent.xyz;
  highp vec2 resolvedUv0=inUv0;
  highp vec2 resolvedUv1=inUv1;
  mediump float impostorViewBlend=0.0;
  bool impostor=(frame.materialFlags.x&MATERIAL_IMPOSTOR)!=0u;
  if(impostor) {
    highp vec3 worldCenter=inModel[3].xyz;
    highp vec2 planarToObject=worldCenter.xz-frame.cameraPositionNear.xz;
    if(environment.worldToViewRow0.w>0.0) planarToObject=environment.worldToViewRow2.xz;
    highp float planarLength=length(planarToObject);
    highp vec2 forward=planarLength>1.0e-5?planarToObject/planarLength:
        vec2(sin(frame.cameraFrame.y),cos(frame.cameraFrame.y));
    highp vec3 worldForward=vec3(forward.x,0.0,forward.y);
    highp vec3 worldRight=vec3(forward.y,0.0,-forward.x);
    highp mat3 faceCamera=mat3(worldRight,vec3(0.0,1.0,0.0),worldForward);
    modelPosition=faceCamera*inPosition;
    modelNormal=faceCamera*inNormal.xyz;
    modelTangent=faceCamera*inTangent.xyz;

    uint metadata=frame.materialFlags.y>>16u;
    uint viewColumns=(metadata>>8u)&15u;
    uint viewRows=(metadata>>12u)&15u;
    if(viewColumns>0u && viewRows>0u) {
      highp vec2 macroScale=exp2(-vec2(float(metadata&15u),float((metadata>>4u)&15u)));
      uint viewCount=viewColumns*viewRows;
      highp float azimuth=atan(worldForward.x,worldForward.z);
      highp float viewPosition=fract(azimuth/6.28318530718+1.0)*float(viewCount);
      uint firstView=uint(floor(viewPosition))%viewCount;
      uint secondView=(firstView+1u)%viewCount;
      highp vec2 frameScale=macroScale/vec2(float(viewColumns),float(viewRows));
      highp vec2 firstCell=vec2(float(firstView%viewColumns),float(firstView/viewColumns));
      highp vec2 secondCell=vec2(float(secondView%viewColumns),float(secondView/viewColumns));
      // Keep exact-edge fragments in this view's cell when the fragment shader
      // clamps the footprint at the chosen resident mip level.
      highp vec2 localUv=clamp(inUv0,vec2(0.00001),vec2(0.99999));
      resolvedUv0=inUv1+(firstCell+localUv)*frameScale;
      resolvedUv1=inUv1+(secondCell+localUv)*frameScale;
      impostorViewBlend=fract(viewPosition);
    }
  }
  highp vec3 worldPosition=(inModel*vec4(modelPosition,1)).xyz;
  highp mat3 linear=mat3(inModel);
  highp mat3 normalMatrix=mat3(inNormalColumn0.xyz,inNormalColumn1.xyz,inNormalColumn2.xyz);
  if((frame.materialFlags.x&MATERIAL_WATER)!=0u) {
    bool authoredSurface=(frame.materialFlags.x&2048u)!=0u; // WaterAuthoringResource
    bool routeSurface=(frame.materialFlags.x&4096u)!=0u;
    highp vec4 waterLayers=authoredSurface?frame.emissiveFactorStrength:vec4(1);
    highp float waterCellSize=0.0;
    if(authoredSurface) waterCellSize=inUv1.x*max(length(linear[0]),length(linear[2]));
    if(routeSurface) waterCellSize=inNormal.w*512.0*max(length(linear[0]),length(linear[2]));
    if(routeSurface) {
      highp vec2 direction=linear[0].xz*inUv1.x+linear[2].xz*inUv1.y;
      resolvedUv1=length(direction)>.0001?normalize(direction)*length(inUv1):vec2(0);
      resolvedUv0.y*=length(linear[1]);
    }
    if((frame.materialFlags.x&1024u)!=0u) { // MapMaterialWaterCameraGrid
      highp float gridScale=environment.waterShallowColorDistance.w/max(inUv1.y,1.0);
      worldPosition.xz=frame.cameraPositionNear.xz+inPosition.xz*gridScale;
      waterCellSize=inUv1.x*gridScale;
    }
    // The mesh is immutable. Only this shader evaluates the spectrum for every
    // visible vertex; the CPU counterpart is reserved for sparse gameplay and
    // buoyancy queries. Keeping world XZ unchanged also makes adjacent clipmap
    // patches mathematically watertight.
    highp float height=environment.waterParameters.y;
    highp vec2 slope=vec2(0.0);
    if(authoredSurface) {
      height+=worldPosition.y;
      highp vec3 planeNormal=normalize(normalMatrix*(routeSurface?inNormal.xyz:vec3(0,1,0)));
      if(abs(planeNormal.y)>.001) slope=-planeNormal.xz/planeNormal.y;
    }
    highp vec2 baseSlope=slope;
#ifdef AETHER_SPECTRAL_WATER
    highp vec2 horizontal=vec2(0.0);
    highp vec3 horizontalDerivative=vec3(0.0);
    vSpectralFoam=0.0;
    vSpectralCoordinates=vec3(worldPosition.xz,waterCellSize);
    // Modo 7 de WaterCostIsolation: separa o custo de vértice (leituras de
    // storage buffer por cascata) do custo de rasterização da mesma malha.
    if(int(environment.waterInteractionParameters.y+0.5)!=7)
      addWaterSpectrum(worldPosition.xz,waterCellSize,height,slope,horizontal,horizontalDerivative,vSpectralFoam);
#else
    int waveCount=clamp(int(environment.waterParameters.x+0.5),0,8);
    for(int waveIndex=0;waveIndex<8;++waveIndex) {
      if(waveIndex>=waveCount) break;
      highp vec4 shape=environment.waterWaveShape[waveIndex];
      highp vec4 motion=environment.waterWaveMotion[waveIndex];
      // Filter geometry below the grid's representable wavelength. This is
      // continuous, avoiding rings of popping waves as the camera moves.
      highp float wavelength=6.28318530718/max(shape.w,1.0e-6);
      shape.z*=smoothstep(2.0,5.0,wavelength/max(waterCellSize,0.001));
      highp float angle=shape.w*dot(shape.xy,worldPosition.xz)-
                         motion.x*environment.waterParameters.z+motion.z;
      highp float sine=sin(angle),cosine=cos(angle);
      highp float crest=0.25*motion.y;
      highp float normalization=1.0+crest;
      height+=shape.z*(sine+crest*(2.0*sine*sine-1.0))/normalization;
      slope+=shape.xy*(shape.z*shape.w*cosine*(1.0+4.0*crest*sine)/normalization);
    }
#endif
    highp float stillHeight=environment.waterParameters.y+(authoredSurface?worldPosition.y:0);
    height=mix(stillHeight,height,waterLayers.x);
    slope=baseSlope+(slope-baseSlope)*waterLayers.x;
#ifdef AETHER_SPECTRAL_WATER
    horizontal*=waterLayers.x;horizontalDerivative*=waterLayers.x;
#endif
    // Micro-ondas que ainda cabem na malha participam da silhueta. Abaixo do
    // Nyquist geométrico elas desaparecem continuamente daqui e permanecem no
    // fragmento como normal filtrada; isso evita tanto aliasing quanto um anel
    // abruptamente liso ao redor da câmera.
    highp float microAmplitude=waterLayers.x*environment.waterSurfaceDetail.z*
        environment.waterParameters.w;
    highp float microBase=max(environment.waterSurfaceDetail.w,0.2);
    const highp vec2 microDirections[3]=vec2[3](
        normalize(vec2(.91,.41)),normalize(vec2(-.32,.95)),normalize(vec2(.68,-.73)));
    const highp float microRatios[3]=float[3](1.0,.63,.39);
    const highp float microWeights[3]=float[3](.48,.32,.20);
    for(int microIndex=0;microIndex<3;++microIndex) {
      highp float wavelength=microBase*microRatios[microIndex];
      highp float geometryWeight=smoothstep(2.0,5.0,wavelength/max(waterCellSize,.001));
      highp float waveNumber=6.28318530718/wavelength;
      highp float phase=waveNumber*dot(microDirections[microIndex],worldPosition.xz)-
          environment.waterParameters.z*(1.7+float(microIndex)*.43);
      highp float amplitude=microAmplitude*microWeights[microIndex]*geometryWeight;
      height+=sin(phase)*amplitude;
      slope+=microDirections[microIndex]*(cos(phase)*amplitude*waveNumber);
    }
    int interactionCount=clamp(int(environment.waterInteractionParameters.x+0.5),0,8);
    for(int interactionIndex=0;interactionIndex<interactionCount;++interactionIndex) {
      highp vec4 shape=environment.waterInteractionShape[interactionIndex];
      highp vec4 motion=environment.waterInteractionMotion[interactionIndex];
      highp float age=environment.waterParameters.z-shape.z;
      if(shape.w==0.0||age<0.0||age>motion.w) continue;
      highp vec2 delta=worldPosition.xz-shape.xy;
      highp float distance=length(delta);
      highp float waveNumber=6.28318530718/max(motion.x,0.1);
      highp float radial=distance-motion.y*age;
      highp float width=max(0.35,motion.x*0.55);
      highp float gaussian=exp(-(radial*radial)/(width*width));
      highp float envelope=waterLayers.z*shape.w*gaussian*exp(-motion.z*age);
      highp float angle=waveNumber*radial;
      height+=envelope*sin(angle);
      if(distance>1.0e-5) {
        highp float derivative=envelope*(waveNumber*cos(angle)-
            2.0*radial/(width*width)*sin(angle));
        slope+=delta/distance*derivative;
      }
    }
#ifdef AETHER_WATER_RIPPLES
    // A ondulacao dinamica soma na mesma altura do espectro, e sua inclinacao
    // na mesma normal, independentemente de o provedor global ser analitico ou
    // espectral. Ela e deslocamento vertical puro.
    highp float rippleHeight=sampleWaterRipple(worldPosition.xz);
    if(rippleHeight!=0.0) {
      height+=rippleHeight*waterLayers.z;
      // No espacamento da propria malha: abaixo dele a geometria nao carrega o
      // detalhe de qualquer forma, e o fragmento e quem o mostra.
      vRippleGeometrySlope=sampleWaterRippleSlope(worldPosition.xz,max(waterCellSize,0.25))*waterLayers.z;
      slope+=vRippleGeometrySlope;
    }
#endif
    // A película de espuma é aerada e fica acima do plano líquido. Elevar a
    // própria geometria, em vez de apenas clarear o fragmento, dá espessura à
    // crista e corrige a silhueta sem deslocar toda a massa d'água.
#ifdef AETHER_SPECTRAL_WATER
    highp float elevationFoam=vSpectralFoam;
#else
    highp float elevationSlope=length(slope)/max(1.0+length(slope),.001);
    highp float elevationFoam=smoothstep(environment.waterOptics.w,
        min(environment.waterOptics.w+.22,1.0),elevationSlope);
#endif
    elevationFoam=clamp(elevationFoam*environment.waterSurfaceDetail.y,0.0,1.0);
    height+=environment.waterSurfaceDetail.x*elevationFoam*elevationFoam;
    worldPosition.y=height;
#ifdef AETHER_SPECTRAL_WATER
    worldPosition.xz+=horizontal;
    highp vec3 tangentX=vec3(1.0+horizontalDerivative.x,slope.x,horizontalDerivative.y);
    highp vec3 tangentZ=vec3(horizontalDerivative.y,slope.y,1.0+horizontalDerivative.z);
    highp vec3 surfaceNormal=cross(tangentZ,tangentX);
    vNormal=dot(surfaceNormal,surfaceNormal)>1.0e-12?normalize(surfaceNormal):vec3(0,1,0);
    vTangent=vec4(dot(tangentX,tangentX)>1.0e-12?normalize(tangentX):vec3(1,0,0),1.0);
#else
    vNormal=normalize(vec3(-slope.x,1.0,-slope.y));
    vTangent=vec4(normalize(vec3(1.0,slope.x,0.0)),1.0);
#endif
  } else {
    vNormal=normalize(normalMatrix*modelNormal);
    vTangent=vec4(normalize(linear*modelTangent),inTangent.w*inNormalColumn0.w);
  }
  vPosition=worldPosition;
  vUv0=resolvedUv0; vUv1=resolvedUv1; vColor=inColor*inTint;
  if(impostor && (frame.materialFlags.y>>16u)!=0u) vColor.a=impostorViewBlend;
  vDither=inNormalColumn1.w;
  highp vec3 relative=worldPosition-frame.cameraPositionNear.xyz;
  highp vec3 view=vec3(dot(environment.worldToViewRow0.xyz,relative),
                 dot(environment.worldToViewRow1.xyz,relative),
                 dot(environment.worldToViewRow2.xyz,relative));
  highp float farPlane=uintBitsToFloat(frame.materialFlags.w);
  highp float nearPlane=frame.cameraPositionNear.w;
  highp float focal=environment.shadowTransitionParameters.z>0.0?environment.shadowTransitionParameters.z:1.732050808;
  highp float halfHeight=environment.worldToViewRow0.w;
  bool orthographic=halfHeight>0.0;
  if(orthographic) focal=1.0/halfHeight;
  highp vec2 xy=vec2(view.x*focal/frame.cameraFrame.x,-view.y*focal);
  highp vec2 projected=vec2(dot(frame.surfaceTransform.xy,xy),
                            dot(frame.surfaceTransform.zw,xy));
  highp float clipW=orthographic?1.0:view.z;
  projected += environment.shadowFilterParameters.zw * clipW;
  gl_Position=vec4(projected,
      orthographic?(view.z-nearPlane)/(farPlane-nearPlane):
      (farPlane*view.z-nearPlane*farPlane)/(farPlane-nearPlane),clipW);
}
