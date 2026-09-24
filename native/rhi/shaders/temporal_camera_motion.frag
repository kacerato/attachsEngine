#version 450
#extension GL_GOOGLE_include_directive : require
// Vetor de movimento de câmera para todo pixel (G6-B), o equivalente ao passe
// de "camera motion vectors" do URP: reconstrói a posição de mundo pela
// profundidade e reprojeta com a pose anterior. Objetos que se moveram são
// sobrescritos em seguida, no mesmo render pass, pelo passe por objeto com
// teste de profundidade EQUAL. Superfícies sem profundidade própria (água,
// transparentes) herdam o movimento do que está atrás e são marcadas pelas
// máscaras de reatividade/composição.
precision highp float;
precision highp int;
#define AETHER_TEMPORAL_SET 0
#define AETHER_TEMPORAL_BINDING 1
#include "temporal_projection.glsl"
layout(set=0,binding=0) uniform sampler2D sceneDepth;
layout(push_constant) uniform CameraMotionPush {
  vec4 extent; // xy = extensão renderizada / alocação da profundidade
} push;
layout(location=0) in highp vec2 vUv;
layout(location=0) out highp vec2 outVelocity;

void main() {
  highp vec2 viewport=temporalFrame.postViewport.zw;
  highp vec2 local=(vUv-temporalFrame.postViewport.xy)/max(viewport,vec2(1.0e-6));
  if(any(lessThan(local,vec2(0.0)))||any(greaterThan(local,vec2(1.0)))) {
    outVelocity=vec2(0.0);
    return;
  }
  highp float depth=texture(sceneDepth,vUv*push.extent.xy).r;
  // Superfície vista neste pixel com jitter; sem ele, a posição da mesma
  // superfície é o NDC do pixel menos o deslocamento do quadro.
  highp vec2 physicalNdc=local*2.0-1.0-temporalFrame.temporalJitter.xy;
  highp vec4 s=temporalFrame.temporalSurface;
  // A pré-transformação é uma rotação de 0/90/180/270 graus: a inversa é a transposta.
  highp vec2 cameraNdc=vec2(s.x*physicalNdc.x+s.z*physicalNdc.y,s.y*physicalNdc.x+s.w*physicalNdc.y);
  highp float nearPlane=temporalFrame.temporalProjection.z,farPlane=temporalFrame.temporalProjection.w;
  highp float aspect=temporalFrame.temporalProjection.y;
  highp float halfHeight=temporalFrame.temporalParameters.x;
  highp vec3 view;
  if(halfHeight>0.0) {
    view=vec3(cameraNdc.x*halfHeight*aspect,-cameraNdc.y*halfHeight,mix(nearPlane,farPlane,depth));
  } else {
    highp float focal=temporalFrame.temporalProjection.x;
    highp float viewZ=nearPlane*farPlane/max(farPlane-depth*(farPlane-nearPlane),1.0e-6);
    view=vec3(cameraNdc.x*aspect*viewZ/focal,-cameraNdc.y*viewZ/focal,viewZ);
  }
  highp vec3 right=temporalFrame.temporalCurrentView[0].xyz;
  highp vec3 up=temporalFrame.temporalCurrentView[1].xyz;
  highp vec3 forward=temporalFrame.temporalCurrentView[2].xyz;
  highp vec3 previousView;
  if(depth>=0.999999 && halfHeight<=0.0) {
    // Céu infinito: responde só à rotação, nunca à translação da câmera.
    highp vec3 direction=right*view.x+up*view.y+forward*view.z;
    previousView=vec3(dot(temporalFrame.temporalPreviousView[0].xyz,direction),
                      dot(temporalFrame.temporalPreviousView[1].xyz,direction),
                      dot(temporalFrame.temporalPreviousView[2].xyz,direction));
  } else {
    highp vec3 eye=vec3(temporalFrame.temporalCurrentView[0].w,temporalFrame.temporalCurrentView[1].w,
                        temporalFrame.temporalCurrentView[2].w);
    previousView=temporalViewPosition(eye+right*view.x+up*view.y+forward*view.z,true);
  }
  if(halfHeight<=0.0 && previousView.z<=1.0e-4) {
    // Atrás da câmera anterior: não existe amostra reprojetável. Um vetor para
    // fora da imagem faz TAA e bibliotecas tratarem o pixel como desoclusão.
    outVelocity=vec2(4.0);
    return;
  }
  outVelocity=(temporalProjectView(previousView)-(physicalNdc))*0.5*viewport;
}
