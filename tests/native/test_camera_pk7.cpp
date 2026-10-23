// PK-7 da auditoria full Unity: Câmera com Background Type, Culling Mask,
// Viewport Rect, pós/sombras/HDR/AA por câmera, Physical Camera e a ordem de
// desenho por prioridade. Referência: Unity 6000.0 URP Camera component
// (https://docs.unity3d.com/6000.0/Documentation/Manual/urp/camera-component-reference.html).
#include "harness.h"
#include "editor/editor_scene_camera.h"
#include "runtime/game_view.h"
#include "runtime/object_tags.h"
#include "scene/camera.h"
#include "scene/component_api_csharp.h"
#include "scene/component_properties.h"
#include "scene/component_reflection.h"
#include "scene/component_schema.h"

#include <array>
#include <cmath>
#include <functional>
#include <sstream>
#include <string>

using namespace ae;

namespace {
bool close(float a,float b,float e=1e-4f) {return std::abs(a-b)<=e;}
struct CameraScene {
  runtime::SceneGraph g;
  runtime::ObjectId camera(const char *name,float priority,const std::function<void(scene::Camera&)> &setup={}) {
    const auto id=g.createEntity(g.root(),runtime::ObjectKind::Folder,name);auto v=*g.find(id);
    auto *c=static_cast<scene::Camera*>(v.components.add(scene::Camera::descriptor));
    c->priority=priority;if(setup) setup(*c);
    g.applyEntityValues(id,v);return id;
  }
};
} // namespace

AE_TEST(camera_v3_scene_opens_with_unity_defaults) {
  // Linha de uma cena v3: enabled fov near far priority ortho projeção máscara.
  std::istringstream v3("1 45 0.3 500 2 7 1 4 ");
  scene::Camera camera;
  AE_EXPECT_TRUE(camera.read(v3,3),"v3 continua legível");
  AE_EXPECT_TRUE(camera.projection==scene::CameraProjection::Orthographic && camera.environmentMask==4u,"campos v3 preservados");
  AE_EXPECT_TRUE(camera.background==scene::CameraBackground::Skybox && camera.cullingMask==~0u,"céu e todas as camadas");
  AE_EXPECT_TRUE(camera.viewportX==0 && camera.viewportY==0 && camera.viewportWidth==1 && camera.viewportHeight==1,"tela inteira");
  AE_EXPECT_TRUE(camera.postProcessing && camera.renderShadows && camera.hdr && !camera.physicalCamera,"pós, sombras e HDR ligados");
  AE_EXPECT_TRUE(camera.valid(),"valor migrado é válido");
  AE_EXPECT_EQ(scene::Camera::descriptor.version,4u,"formato da câmera sobe para v4");
}

AE_TEST(camera_v4_round_trip_keeps_every_field) {
  scene::Camera source;
  source.background=scene::CameraBackground::SolidColor;source.backgroundColor[0]=.25f;source.backgroundColor[1]=.5f;source.backgroundColor[2]=.75f;
  source.cullingMask=0x5u;source.viewportX=.5f;source.viewportWidth=.5f;source.viewportHeight=.75f;
  source.postProcessing=false;source.renderShadows=false;source.hdr=false;source.antialiasing=scene::CameraAntialiasing::Fxaa;
  source.physicalCamera=true;source.focalLength=35;source.sensorWidth=24.89f;source.sensorHeight=18.66f;source.gateFit=scene::CameraGateFit::Overscan;
  std::stringstream text;source.write(text);
  scene::Camera back;
  AE_EXPECT_TRUE(back.read(text,4),"v4 lê o que v4 escreve");
  AE_EXPECT_TRUE(back.background==source.background && close(back.backgroundColor[2],.75f),"fundo");
  AE_EXPECT_TRUE(back.cullingMask==0x5u,"culling mask");
  AE_EXPECT_TRUE(close(back.viewportX,.5f) && close(back.viewportWidth,.5f) && close(back.viewportHeight,.75f),"viewport");
  AE_EXPECT_TRUE(!back.postProcessing && !back.renderShadows && !back.hdr && back.antialiasing==scene::CameraAntialiasing::Fxaa,"renderização");
  AE_EXPECT_TRUE(back.physicalCamera && close(back.focalLength,35) && close(back.sensorHeight,18.66f) && back.gateFit==scene::CameraGateFit::Overscan,"câmera física");
  std::istringstream bad("1 60 0.1 2000 0 5 0 4294967295 7 0 0 0 4294967295 0 0 1 1 1 1 1 0 0 20 36 24 1 ");
  scene::Camera rejected;
  AE_EXPECT_TRUE(!rejected.read(bad,4),"tipo de fundo inexistente é recusado");
}

AE_TEST(camera_physical_lens_follows_unity_gate_fit) {
  const float aspect=16.f/9.f;
  auto lens=scene::cameraLens(false,60,50,36,24,scene::CameraGateFit::Horizontal,aspect);
  AE_EXPECT_TRUE(close(lens.verticalFovRadians,1.0471976f) && close(lens.aspect,aspect),"sem câmera física vale o campo autoral");
  const float vertical=2*std::atan(24.f/(2*50)),horizontal=2*std::atan(36.f/(2*50*aspect));
  lens=scene::cameraLens(true,60,50,36,24,scene::CameraGateFit::Vertical,aspect);
  AE_EXPECT_TRUE(close(lens.verticalFovRadians,vertical),"Vertical casa a altura do sensor");
  lens=scene::cameraLens(true,60,50,36,24,scene::CameraGateFit::Horizontal,aspect);
  AE_EXPECT_TRUE(close(lens.verticalFovRadians,horizontal),"Horizontal casa a largura do sensor");
  lens=scene::cameraLens(true,60,50,36,24,scene::CameraGateFit::Fill,aspect);
  AE_EXPECT_TRUE(close(lens.verticalFovRadians,std::min(vertical,horizontal)),"Fill recorta dentro do sensor");
  lens=scene::cameraLens(true,60,50,36,24,scene::CameraGateFit::Overscan,aspect);
  AE_EXPECT_TRUE(close(lens.verticalFovRadians,std::max(vertical,horizontal)),"Overscan mostra o sensor inteiro");
  lens=scene::cameraLens(true,60,50,36,24,scene::CameraGateFit::None,aspect);
  AE_EXPECT_TRUE(close(lens.verticalFovRadians,vertical) && close(lens.aspect,1.5f),"None estica: proporção do sensor");
  AE_EXPECT_TRUE(close(scene::cameraFocalLengthForFov(60,24),20.7846097f,1e-3f),"60° em 24 mm = 20,78 mm");
  scene::Camera camera;
  AE_EXPECT_TRUE(close(camera.focalLength,scene::cameraFocalLengthForFov(camera.verticalFov,camera.sensorHeight),1e-3f),
                 "a focal padrão reproduz o campo padrão no sensor vertical");
  lens=scene::cameraLens(true,60,85,36,24,scene::CameraGateFit::Vertical,aspect);
  AE_EXPECT_TRUE(lens.verticalFovRadians<scene::cameraLens(true,60,24,36,24,scene::CameraGateFit::Vertical,aspect).verticalFovRadians,
                 "teleobjetiva fecha o campo");
}

AE_TEST(camera_culling_mask_is_a_layer_bitmask) {
  const auto *property=[]()->const scene::ComponentEnum*{
    for(const auto &p:scene::Camera::descriptor.enums) {if(p.id=="culling_mask") return &p;}
    return nullptr;}();
  AE_EXPECT_TRUE(property && property->flags && property->options.size()==32,"32 camadas, como a Unity");
  if(!property) return;
  AE_EXPECT_TRUE(scene::componentEnumLabel(*property,0)=="Nada" && scene::componentEnumLabel(*property,~0u)=="Tudo","Nada e Tudo");
  AE_EXPECT_TRUE(scene::componentEnumLabel(*property,1u<<3)=="Camada 3","uma camada mostra o nome");
  AE_EXPECT_TRUE(scene::componentEnumLabel(*property,0x6u)=="Camada 1, Camada 2","várias camadas pelos nomes");
  AE_EXPECT_TRUE(property->options.data()==scene::objectLayerOptions.data(),"mesma lista de camadas da Luz");
  scene::Components components;components.add(scene::Camera::descriptor);
  AE_EXPECT_TRUE(scene::setComponentProperty(components,"astra.camera","culling_mask",0x5u)==scene::ComponentPropertyStatus::Applied,
                 "qualquer combinação de camadas é aceita");
  AE_EXPECT_TRUE(static_cast<const scene::Camera*>(components.find(scene::Camera::descriptor))->cullingMask==0x5u,"valor escrito");
  AE_EXPECT_TRUE(scene::setComponentProperty(components,"astra.camera","background_type",7u)==scene::ComponentPropertyStatus::InvalidValue,
                 "enum comum continua recusando valor fora das opções");
  const auto api=scene::componentCSharpApi();
  AE_EXPECT_TRUE(api.find("public CullingMaskOption CullingMask")!=std::string::npos,"C# expõe a máscara como [Flags] enum, como a Luz");
  AE_EXPECT_TRUE(api.find("public BackgroundTypeOption BackgroundType")!=std::string::npos,"C# expõe o tipo de fundo");
  AE_EXPECT_TRUE(api.find("public float FocalLength")!=std::string::npos && api.find("public bool PhysicalCamera")!=std::string::npos,"C# câmera física");
}

AE_TEST(camera_stack_orders_by_priority_and_hides_covered_cameras) {
  CameraScene scene;
  const auto low=scene.camera("Baixa",0);
  const auto high=scene.camera("Alta",5);
  auto stack=editor::resolveSceneCameraStack(scene.g);
  AE_EXPECT_EQ(stack.size(),usize{1},"a de maior prioridade cobre a outra inteira");
  AE_EXPECT_TRUE(!stack.empty() && stack.front().entity==high,"e é a mesma que resolveSceneCamera elege");
  AE_EXPECT_TRUE(editor::resolveSceneCamera(scene.g).entity==high,"eleição única preservada");
  // Tela dividida: duas câmeras de meia tela.
  auto v=*scene.g.find(low);
  auto *left=static_cast<scene::Camera*>(v.components.edit(scene::Camera::descriptor));left->viewportWidth=.5f;
  scene.g.applyEntityValues(low,v);
  v=*scene.g.find(high);
  auto *right=static_cast<scene::Camera*>(v.components.edit(scene::Camera::descriptor));right->viewportX=.5f;right->viewportWidth=.5f;
  right->cullingMask=1u<<2;right->background=scene::CameraBackground::SolidColor;
  scene.g.applyEntityValues(high,v);
  stack=editor::resolveSceneCameraStack(scene.g);
  AE_EXPECT_EQ(stack.size(),usize{2},"duas metades: as duas aparecem");
  AE_EXPECT_TRUE(stack.size()==2 && stack[0].entity==low && stack[1].entity==high,"de baixo para cima por prioridade");
  if(stack.size()==2) {
    AE_EXPECT_TRUE(stack[1].cullingMask==(1u<<2) && stack[1].background==scene::CameraBackground::SolidColor,"saída copiada para quem desenha");
    const auto rect=stack[1].screenRect({0,0,200,100});
    AE_EXPECT_TRUE(close(rect.x,100) && close(rect.y,0) && close(rect.width,100) && close(rect.height,100),"metade direita");
    AE_EXPECT_TRUE(!stack[0].coversView(),"meia tela não cobre a vista");
  }
  // Uma câmera de tela inteira "Não inicializado" não esconde as de baixo.
  scene.camera("Topo",9,[](scene::Camera &c){c.background=scene::CameraBackground::Uninitialized;});
  stack=editor::resolveSceneCameraStack(scene.g);
  AE_EXPECT_EQ(stack.size(),usize{3},"sem limpeza, as de baixo continuam visíveis");
  // Retângulo de área zero não desenha.
  scene.camera("Nada",20,[](scene::Camera &c){c.viewportWidth=0;});
  AE_EXPECT_EQ(editor::resolveSceneCameraStack(scene.g).size(),usize{3},"viewport vazio fica fora da pilha");
}

AE_TEST(camera_main_camera_tag_picks_the_game_camera_like_camera_main) {
  CameraScene scene;
  const auto player=scene.camera("Jogador",0,[](scene::Camera &c){c.viewportWidth=.5f;});
  const auto overview=scene.camera("Vista",1,[](scene::Camera &c){c.viewportX=.5f;c.viewportWidth=.5f;});
  AE_EXPECT_TRUE(editor::resolveSceneCamera(scene.g).entity==overview,"sem marca, a maior prioridade é a câmera do jogo");
  auto v=*scene.g.find(player);v.tag="MainCamera";scene.g.applyEntityValues(player,v);
  AE_EXPECT_TRUE(editor::resolveSceneCamera(scene.g).entity==player,"MainCamera vence, como Camera.main");
  const auto stack=editor::resolveSceneCameraStack(scene.g);
  AE_EXPECT_TRUE(stack.size()==2 && stack.back().entity==overview,"a ordem de desenho continua sendo a prioridade");
}

AE_TEST(camera_main_camera_is_a_builtin_tag_like_unity) {
  runtime::ObjectTags tags;
  AE_EXPECT_TRUE(tags.contains("MainCamera") && tags.contains("Untagged"),"MainCamera existe em todo projeto");
  AE_EXPECT_TRUE(!tags.remove("MainCamera"),"tag embutida não se apaga");
  AE_EXPECT_TRUE(tags.add("Inimigo"),"tag do projeto");
  std::stringstream text;tags.write(text);
  AE_EXPECT_TRUE(text.str()=="ASTRA_TAGS 1 1\n\"Inimigo\"\n","embutidas ficam fora do arquivo");
  runtime::ObjectTags back;AE_EXPECT_TRUE(back.read(text) && back==tags,"ida e volta");
  std::istringstream legacy("ASTRA_TAGS 1 2\n\"MainCamera\"\n\"Inimigo\"\n");
  runtime::ObjectTags old;AE_EXPECT_TRUE(old.read(legacy) && old==tags,"catálogo antigo com MainCamera declarada abre igual");
}

AE_TEST(camera_viewport_rect_measures_from_the_bottom_left) {
  editor::SceneCameraPose pose;pose.viewport[0]=0;pose.viewport[1]=.5f;pose.viewport[2]=1;pose.viewport[3]=.5f;
  const auto top=pose.screenRect({10,20,200,100});
  AE_EXPECT_TRUE(close(top.x,10) && close(top.y,20) && close(top.width,200) && close(top.height,50),"y = 0,5 é a metade de CIMA");
  pose.viewport[0]=.75f;pose.viewport[1]=0;pose.viewport[2]=.5f;pose.viewport[3]=1;
  float clipped[4];
  AE_EXPECT_TRUE(pose.clippedViewport(clipped) && close(clipped[2],.25f),"retângulo para fora da tela é recortado");
  // pixelRect: o raio pelo centro do retângulo da câmera é o eixo óptico.
  runtime::GameView view;view.valid=true;view.width=200;view.height=100;
  renderer::PerspectiveVisibilitySettings settings;
  view.frustum=renderer::buildPerspectiveFrustum(std::array<float,3>{0,0,0}.data(),0,0,1.f,settings);
  view.pixelX=100;view.pixelY=0;view.pixelWidth=100;view.pixelHeight=100;
  float origin[3],direction[3];
  AE_EXPECT_TRUE(runtime::gameViewScreenRay(view,150,50,origin,direction),"raio válido");
  AE_EXPECT_TRUE(close(direction[0],0,1e-3f) && close(direction[1],0,1e-3f) && direction[2]>.99f,"centro do pixelRect olha para frente");
  const float ahead[3]{0,0,10};float screen[3];
  AE_EXPECT_TRUE(runtime::gameViewWorldToScreen(view,ahead,screen) && close(screen[0],150,1e-2f) && close(screen[1],50,1e-2f),
                 "WorldToScreenPoint volta ao centro do retângulo");
}

AE_TEST(camera_reflection_declares_real_consumers) {
  const auto rows=scene::allComponentContracts();
  bool mask=false,background=false,focal=false;
  for(const auto &row:rows) {
    if(row.typeId!="astra.camera") continue;
    mask|=row.propertyId=="culling_mask" && row.domain.find(" + ")!=std::string::npos;
    background|=row.propertyId=="background_type";
    focal|=row.propertyId=="focal_length";
  }
  for(const auto &issue:scene::auditComponentContracts())
    AE_EXPECT_TRUE(issue.typeId!="astra.camera",(std::string("contrato da câmera: ")+std::string(issue.propertyId)+" — "+issue.message).c_str());
  AE_EXPECT_TRUE(mask && background && focal,"propriedades novas aparecem no contrato com domínio próprio");
  for(const auto id:{"render.camera.background","render.camera.culling_mask","render.camera.viewport","render.camera.post","render.camera.physical"})
    AE_EXPECT_TRUE(core::engineCapabilityAuthorable(id),"capacidade com consumidor");
  AE_EXPECT_TRUE(!core::engineCapabilityAuthorable("render.camera.stack") && !core::engineCapabilityAuthorable("render.camera.msaa"),
                 "Overlay, Target Texture e MSAA declarados como pendentes");
}
