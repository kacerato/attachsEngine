#pragma once
#include "scene/components.h"
#include "scene/rendering_layers.h"
#include <array>
#include <cmath>
namespace ae::scene {
// Câmera no modelo do Inspector da Camera da Unity 6/URP
// (https://docs.unity3d.com/6000.0/Documentation/Manual/urp/camera-component-reference.html):
//  - Lente/Projeção: perspectiva ou ortográfica, campo vertical, planos e a
//    Physical Camera (distância focal, sensor e Gate Fit). Com a câmera física
//    ligada, o campo de visão sai da focal e do sensor, como na Unity.
//  - Renderização: pós-processamento, antisserrilhado, sombras, prioridade
//    (Depth/Priority) e Culling Mask pelas camadas de objeto do projeto.
//  - Ambiente: Background Type (Skybox, Cor sólida, Não inicializado), cor de
//    fundo e a máscara de volumes de ambiente.
//  - Saída: HDR e Viewport Rect (origem no canto inferior esquerdo, frações da
//    tela).
// Várias câmeras na tela desenham da menor para a maior prioridade; a de cima
// cobre a de baixo dentro do próprio retângulo. Overlay (Camera Stack), Target
// Texture e MSAA ainda não existem: não há campo para eles.
enum class CameraProjection : u32 { Perspective, Orthographic };
enum class CameraBackground : u32 { Skybox, SolidColor, Uninitialized };
enum class CameraAntialiasing : u32 { Project, None, Fxaa };
enum class CameraGateFit : u32 { Vertical, Horizontal, Fill, Overscan, None };
class Camera final : public ComponentValue {
public:

#include "scene/generated/camera_Camera_fields0.inc"

#include "scene/generated/camera_Camera_fields1.inc"

  CameraProjection projection=CameraProjection::Perspective;

#include "scene/generated/camera_Camera_fields2.inc"

  u32 environmentMask=~0u;

#include "scene/generated/camera_Camera_fields3.inc"

#include "scene/generated/camera_Camera_fields4.inc"

  CameraBackground background=CameraBackground::Skybox;
  // Cor de fundo da Unity (49,77,121 sRGB) em RGB linear.
  float backgroundColor[3]{.0307f,.0742f,.1912f};
  u32 cullingMask=kAllObjectLayers;
  CameraAntialiasing antialiasing=CameraAntialiasing::Project;
  CameraGateFit gateFit=CameraGateFit::Horizontal;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Camera>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {const auto v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    const bool maskValid=environmentMask==~0u || (environmentMask && !(environmentMask&(environmentMask-1)) && environmentMask<=128u);
    return farPlane>nearPlane && priority==std::floor(priority) && static_cast<u32>(projection)<=1 && maskValid &&
           static_cast<u32>(background)<=2 && static_cast<u32>(antialiasing)<=2 && static_cast<u32>(gateFit)<=4;
  }
  // v4 acrescenta ao fim de v3: fundo, cor, Culling Mask, Viewport Rect,
  // pós/sombras/HDR/AA e a câmera física. Os padrões reproduzem o quadro de
  // uma cena v3 (céu, todas as camadas, tela inteira, pós e sombras ligados).
  void write(std::ostream &out) const override {
    out<<enabled<<' '<<verticalFov<<' '<<nearPlane<<' '<<farPlane<<' '<<priority<<' '<<orthographicHalfHeight<<' '
       <<static_cast<u32>(projection)<<' '<<environmentMask<<' '
       <<static_cast<u32>(background)<<' '<<backgroundColor[0]<<' '<<backgroundColor[1]<<' '<<backgroundColor[2]<<' '
       <<cullingMask<<' '<<viewportX<<' '<<viewportY<<' '<<viewportWidth<<' '<<viewportHeight<<' '
       <<postProcessing<<' '<<renderShadows<<' '<<hdr<<' '<<static_cast<u32>(antialiasing)<<' '
       <<physicalCamera<<' '<<focalLength<<' '<<sensorWidth<<' '<<sensorHeight<<' '<<static_cast<u32>(gateFit)<<' ';
  }
  bool read(std::istream &in,u32 version) override {
    if((version<1||version>4)||!(in>>enabled>>verticalFov>>nearPlane>>farPlane>>priority)) return false;
    projection=CameraProjection::Perspective;orthographicHalfHeight=5;environmentMask=~0u;
    resetV4();
    if(version==2) {u32 mode=0;if(!(in>>orthographicHalfHeight>>mode)||mode>1) return false;projection=static_cast<CameraProjection>(mode);}
    if(version>=3) {u32 mode=0;if(!(in>>orthographicHalfHeight>>mode>>environmentMask)||mode>1) return false;projection=static_cast<CameraProjection>(mode);}
    if(version==4) {
      u32 back=0,aa=0,fit=0;
      if(!(in>>back>>backgroundColor[0]>>backgroundColor[1]>>backgroundColor[2]>>cullingMask
             >>viewportX>>viewportY>>viewportWidth>>viewportHeight>>postProcessing>>renderShadows>>hdr>>aa
             >>physicalCamera>>focalLength>>sensorWidth>>sensorHeight>>fit) || back>2 || aa>2 || fit>4) return false;
      background=static_cast<CameraBackground>(back);antialiasing=static_cast<CameraAntialiasing>(aa);
      gateFit=static_cast<CameraGateFit>(fit);
    }
    return true;
  }
private:
  void resetV4() {
    const Camera defaults;
    background=defaults.background;std::copy(defaults.backgroundColor,defaults.backgroundColor+3,backgroundColor);
    cullingMask=defaults.cullingMask;viewportX=defaults.viewportX;viewportY=defaults.viewportY;
    viewportWidth=defaults.viewportWidth;viewportHeight=defaults.viewportHeight;
    postProcessing=defaults.postProcessing;renderShadows=defaults.renderShadows;hdr=defaults.hdr;
    antialiasing=defaults.antialiasing;physicalCamera=defaults.physicalCamera;focalLength=defaults.focalLength;
    sensorWidth=defaults.sensorWidth;sensorHeight=defaults.sensorHeight;gateFit=defaults.gateFit;
  }
};

// Lente efetiva para uma tela de proporção `screenAspect` (largura/altura).
// Sem câmera física, o campo vertical autoral vale. Com ela, a focal e o
// sensor dão os campos vertical e horizontal, e o Gate Fit decide qual eixo
// do sensor casa com a tela (Unity Camera.GateFitMode): Vertical, Horizontal,
// Fill (o menor campo, a tela fica dentro do sensor), Overscan (o maior, o
// sensor inteiro aparece) e None (estica o sensor na tela: a proporção da
// projeção passa a ser a do sensor).
struct CameraLens {float verticalFovRadians=1.0471976f;float aspect=1;};
inline CameraLens cameraLens(bool physical,float verticalFovDegrees,float focalLength,float sensorWidth,float sensorHeight,
                             CameraGateFit fit,float screenAspect) {
  CameraLens lens;lens.aspect=screenAspect>0&&std::isfinite(screenAspect)?screenAspect:1.f;
  if(!physical || !(focalLength>0) || !(sensorWidth>0) || !(sensorHeight>0)) {
    lens.verticalFovRadians=verticalFovDegrees*.017453292519943295f;return lens;
  }
  const float vertical=2*std::atan(sensorHeight/(2*focalLength));
  const float horizontal=2*std::atan(sensorWidth/(2*focalLength*lens.aspect));
  switch(fit) {
  case CameraGateFit::Vertical: lens.verticalFovRadians=vertical;break;
  case CameraGateFit::Horizontal: lens.verticalFovRadians=horizontal;break;
  case CameraGateFit::Fill: lens.verticalFovRadians=std::min(vertical,horizontal);break;
  case CameraGateFit::Overscan: lens.verticalFovRadians=std::max(vertical,horizontal);break;
  case CameraGateFit::None: lens.verticalFovRadians=vertical;lens.aspect=sensorWidth/sensorHeight;break;
  }
  lens.verticalFovRadians=std::clamp(lens.verticalFovRadians,.0017453f,3.124139f);
  return lens;
}
inline CameraLens cameraLens(const Camera &camera,float screenAspect) {
  return cameraLens(camera.physicalCamera,camera.verticalFov,camera.focalLength,camera.sensorWidth,camera.sensorHeight,
                    camera.gateFit,screenAspect);
}
// Focal que reproduz um campo vertical no sensor (Camera.FieldOfViewToFocalLength).
inline float cameraFocalLengthForFov(float verticalFovDegrees,float sensorHeight) {
  const float half=std::tan(std::clamp(verticalFovDegrees,1.f,170.f)*.008726646259971648f);
  return std::clamp(sensorHeight/(2*half),.1f,1000.f);
}

#include "scene/generated/camera_cameraNumbers.inc"
#include "scene/generated/camera_cameraBooleans.inc"
inline constexpr std::array<ComponentEnumOption,2> cameraProjectionOptions{{{0,"Perspectiva"},{1,"Ortográfica"}}};
inline constexpr std::array<ComponentEnumOption,9> cameraEnvironmentMaskOptions{{
  {~0u,"Todos os ambientes"},{1u,"Ambiente 0"},{2u,"Ambiente 1"},{4u,"Ambiente 2"},{8u,"Ambiente 3"},
  {16u,"Ambiente 4"},{32u,"Ambiente 5"},{64u,"Ambiente 6"},{128u,"Ambiente 7"}
}};
inline constexpr std::array<ComponentEnumOption,3> cameraBackgroundOptions{{
  {0,"Céu (Skybox)"},{1,"Cor sólida"},{2,"Não inicializado"}
}};
inline constexpr std::array<ComponentEnumOption,3> cameraAntialiasingOptions{{
  {0,"Do projeto"},{1,"Nenhum"},{2,"FXAA"}
}};
inline constexpr std::array<ComponentEnumOption,5> cameraGateFitOptions{{
  {0,"Vertical"},{1,"Horizontal"},{2,"Preencher"},{3,"Sobrevarredura"},{4,"Nenhum (esticar)"}
}};
// Culling Mask: um bit por camada de objeto do projeto, a mesma lista da
// Culling Mask da Luz (scene/rendering_layers.h); o Inspector mostra os nomes.
#include "scene/generated/camera_cameraEnums.inc"
inline constexpr std::array<ComponentTriple,1> cameraTriples{{
  {"background_color","Cor de fundo",{"background_color.r","background_color.g","background_color.b"},ComponentTripleKind::LinearColor}
}};
inline const ComponentType Camera::descriptor{
  "astra.camera",4,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Camera>();},cameraNumbers,cameraBooleans,cameraEnums,
  nullptr,false,{},cameraTriples
};
}
