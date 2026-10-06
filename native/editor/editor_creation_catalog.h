#pragma once
#include "editor/editor_screen.h"
#include "scene/component_properties.h"
#include "scene/primitive.h"
#include "resources/curve3d.h"
#include "ui/ui_icon_id.h"
#include <array>
#include <span>
#include "ui/ui_text.h"
#include <string>

namespace ae::editor {
inline std::string editorSearchKey(std::string_view text) {
  std::string key;
  for(usize cursor=0;cursor<text.size();) {
    auto c=ui::uiUppercase(ui::nextUiCodepoint(text,cursor));
    if(c>=0xc0&&c<=0xc5)c='A';else if(c==0xc7)c='C';
    else if(c>=0xc8&&c<=0xcb)c='E';else if(c>=0xcc&&c<=0xcf)c='I';
    else if(c>=0xd2&&c<=0xd6)c='O';else if(c>=0xd9&&c<=0xdc)c='U';
    if(c<128)key.push_back(static_cast<char>(c));
  }
  return key;
}

// Receita de criação: um objeto autoral descrito como DADO.
//
// Antes, cada receita era um valor de EditorWidget e um ramo de ternários na
// sessão — doze receitas físicas diferiam só por três enums e ainda assim
// custavam doze entradas de código. Aqui uma receita é uma lista de tipos de
// componente com valores iniciais endereçados por id persistente de propriedade.
// A sessão aplica todas pelo MESMO caminho: fecho de dependências do schema
// (`planComponentAddition`), escrita validada (`setComponentProperty`) e um
// único comando de histórico. Valor recusado pela validação cancela a criação
// inteira, nunca produz objeto pela metade.
struct CreationValue {
  std::string_view component;
  std::string_view property;
  scene::ComponentPropertyValue value;
};
struct CreationComponent {
  std::string_view type;
  std::span<const CreationValue> values{};
};
// Onde o objeto nasce. As poses dependem da vista do editor, que é o que a
// pessoa está olhando quando cria; nenhuma depende de nome de objeto.
enum class CreationPose : u8 {
  ViewTarget,        // no ponto focado da vista, na altura da receita
  ViewTargetFacing,  // idem, orientado como a vista (spot, câmera sobre o alvo)
  ViewTargetTilted,  // idem, inclinado 45° para baixo na direção da vista (sol)
  EditorCamera,      // na pose da câmera do editor (captura o enquadramento)
  BehindSelection    // na posição do objeto selecionado + deslocamento da referência
};
struct EditorCreationEntry {
  // Identidade persistente da receita: não muda com nome, ordem ou idioma.
  std::string_view id;
  // Operações com fluxo próprio (importar, água, modelo de cena, geometria da
  // biblioteca) mantêm a ação existente. Receitas de composição usam None e
  // são executadas pelo caminho genérico.
  EditorWidget action;
  unsigned category;
  const char *name;
  const char *description;
  ui::UiIcon icon;
  runtime::ObjectKind kind=runtime::ObjectKind::Folder;
  std::span<const CreationComponent> components{};
  CreationPose pose=CreationPose::ViewTarget;
  float height=0;
  // Propriedade de referência (componente.propriedade) que recebe o objeto
  // selecionado no momento da criação. Vazio: nenhuma.
  std::string_view selectionComponent{},selectionProperty{};
  const char *searchTerms="";
  scene::PrimitiveType primitive=scene::PrimitiveType::Count;
  bool anchor2DAtCreation=false;
  std::span<const resources::CurvePoint3D> pathPoints{};
  // Componente que recebe a edição inicial da composição, sem mudar a seleção do objeto.
  std::string_view authoringComponent{};
  // A visual child shares resources, never the parent's physical authority.
  scene::PrimitiveType childVisual=scene::PrimitiveType::Count;
  bool composed() const {return action==EditorWidget::None;}
};

namespace recipe {
using scene::ComponentPropertyValue;
inline constexpr std::string_view body="astra.physics.body",collider="astra.physics.collider",
  light="astra.render.light",camera="astra.camera",follow="astra.camera.follow",
  character="astra.physics.character",dynamicMotor="astra.physics.dynamic_motor",timer="astra.time.timer";
inline constexpr u32 Static=0,Kinematic=1,Dynamic=2;       // scene::BodyMotion
inline constexpr u32 Box=0,Sphere=1,Capsule=2,Cylinder=4;             // scene::ColliderShape
inline constexpr u32 Directional=0,Point=1,Spot=2;         // scene::LightKind

inline const CreationValue directionalLight[]{{light,"kind",Directional},{light,"intensity",10000.f}};
inline const CreationValue pointLight[]{{light,"kind",Point},{light,"intensity",1000.f}};
inline const CreationValue spotLight[]{{light,"kind",Spot},{light,"intensity",1000.f}};
inline const CreationComponent directional[]{{light,directionalLight}};
inline const CreationComponent point[]{{light,pointLight}};
inline const CreationComponent spot[]{{light,spotLight}};

#define AE_BODY_RECIPE(name,motion,shape,isSensor) \
  inline const CreationValue name##Body[]{{body,"motion",motion},{body,"sensor",isSensor}}; \
  inline const CreationValue name##Shape[]{{collider,"shape",shape}}; \
  inline const CreationComponent name[]{{body,name##Body},{collider,name##Shape}};
AE_BODY_RECIPE(staticBox,Static,Box,false)
AE_BODY_RECIPE(staticSphere,Static,Sphere,false)
AE_BODY_RECIPE(staticCapsule,Static,Capsule,false)
AE_BODY_RECIPE(dynamicBox,Dynamic,Box,false)
AE_BODY_RECIPE(dynamicSphere,Dynamic,Sphere,false)
AE_BODY_RECIPE(dynamicCapsule,Dynamic,Capsule,false)
AE_BODY_RECIPE(triggerBox,Static,Box,true)
AE_BODY_RECIPE(triggerSphere,Static,Sphere,true)
AE_BODY_RECIPE(triggerCapsule,Static,Capsule,true)
AE_BODY_RECIPE(kinematicBox,Kinematic,Box,false)
AE_BODY_RECIPE(kinematicSphere,Kinematic,Sphere,false)
AE_BODY_RECIPE(staticCylinder,Static,Cylinder,false)
AE_BODY_RECIPE(dynamicCylinder,Dynamic,Cylinder,false)
AE_BODY_RECIPE(triggerCylinder,Static,Cylinder,true)
AE_BODY_RECIPE(kinematicCylinder,Kinematic,Cylinder,false)
#undef AE_BODY_RECIPE
inline const CreationValue motorBody[]{{body,"motion",Dynamic},{body,"freeze_rotation_x",true},{body,"freeze_rotation_y",true},{body,"freeze_rotation_z",true},{body,"mass",70.f},{body,"linear_damping",0.f}};
inline const CreationValue motorCollider[]{{collider,"shape",Cylinder},{collider,"radius",.45f},{collider,"half_height",1.f}};
inline const CreationComponent motorCylinder[]{{body,motorBody},{collider,motorCollider},{dynamicMotor}};
inline const CreationValue mechanismBody[]{{body,"motion",Dynamic}};
#define AE_JOINT_RECIPE(name,kind) \
 inline const CreationValue name##Joint[]{{"astra.physics.joint","kind",u32{kind}}}; \
 inline const CreationComponent name[]{{body,mechanismBody},{collider},{"astra.physics.joint",name##Joint}};


AE_JOINT_RECIPE(fixedJoint,4)
AE_JOINT_RECIPE(coneJoint,5)
AE_JOINT_RECIPE(swingJoint,6)
AE_JOINT_RECIPE(configurableJoint,7)
AE_JOINT_RECIPE(springJoint,8)
#undef AE_JOINT_RECIPE
inline const CreationComponent characterRecipe[]{{character}};
inline const CreationComponent timerRecipe[]{{timer}};
inline constexpr std::string_view physicsConnection="astra.physics.event_connection";
inline const CreationValue connectedSensorBody[]{{body,"motion",Static},{body,"sensor",true}};
inline const CreationValue connectedSensorConnection[]{{physicsConnection,"action",1u}};
inline const CreationComponent connectedSensor[]{{body,connectedSensorBody},{collider},{physicsConnection,connectedSensorConnection}};

inline const CreationComponent cameraRecipe[]{{camera}};
inline const CreationComponent followCamera[]{{camera},{follow}};
inline const CreationValue propulsionBody[]{{body,"motion",Dynamic},{body,"mass",1.f},{body,"gravity_factor",0.f}};
inline const CreationValue propulsionForce[]{{"astra.physics.constant_force","relative_force_z",4.f}};
inline const CreationComponent propulsion[]{{body,propulsionBody},{collider},{"astra.physics.constant_force",propulsionForce}};
inline const CreationComponent fieldGravity[]{{"astra.physics.field.gravity"}};
inline const CreationComponent fieldWind[]{{"astra.physics.field.wind"}};
inline const CreationComponent fieldDrag[]{{"astra.physics.field.drag"}};
inline const CreationComponent fieldRadial[]{{"astra.physics.field.radial"}};
inline const CreationComponent fieldGravity2D[]{{"astra.physics2d.field.gravity"}};
inline const CreationComponent fieldWind2D[]{{"astra.physics2d.field.wind"}};
inline const CreationComponent fieldDrag2D[]{{"astra.physics2d.field.drag"}};
inline const CreationComponent fieldRadial2D[]{{"astra.physics2d.field.radial"}};
inline const CreationComponent springPosition[]{{"astra.spring.position"}};
inline const CreationComponent springRotation[]{{"astra.spring.rotation"}};
inline const CreationComponent springScale[]{{"astra.spring.scale"}};
inline const CreationComponent positionConstraint[]{{"astra.constraint.position"}};
inline const CreationComponent rotationConstraint[]{{"astra.constraint.rotation"}};
inline const CreationComponent scaleConstraint[]{{"astra.constraint.scale"}};
inline const CreationComponent aimConstraint[]{{"astra.constraint.aim"}};
inline const CreationComponent parentConstraint[]{{"astra.constraint.parent"}};
inline const CreationComponent lookAtConstraint[]{{"astra.constraint.look_at"}};
inline const CreationValue tweenDestination[]{{"astra.tween.transform","position_y",2.f},{"astra.tween.transform","relative",true},{"astra.tween.transform","pingpong",true},{"astra.tween.transform","loops",u32{0}}};
inline const CreationComponent tween[]{{"astra.tween.transform",tweenDestination}};
inline const CreationValue connectedTweenValues[]{{"astra.tween.transform","position_y",2.f},{"astra.tween.transform","relative",true},{"astra.tween.transform","finished_action",u32{1}}};
inline const CreationComponent connectedTween[]{{"astra.tween.transform",connectedTweenValues}};
inline const CreationComponent audioSource[]{{"astra.audio.source"}};
// Gatilho sonoro: o sensor emite trigger_enter e a Conexão de evento chama
// AudioSource.play no próprio objeto (receptor vazio). Composição pura de tipos
// existentes; o receptor pode ser trocado na Inspeção.
inline constexpr std::string_view eventConnection="astra.logic.event_connection";
inline const CreationValue soundTriggerBody[]{{body,"motion",Static},{body,"sensor",true}};
inline const CreationValue soundTriggerConnection[]{{eventConnection,"event",u32{3}},{eventConnection,"action",u32{4}},{eventConnection,"method",u32{1}}};
inline const CreationComponent soundTrigger[]{{body,soundTriggerBody},{collider},{"astra.audio.source"},{eventConnection,soundTriggerConnection}};
inline const CreationComponent eventConnectionRecipe[]{{eventConnection}};
inline const CreationComponent audioListener[]{{"astra.audio.listener"}};
inline const CreationComponent audioBus[]{{"astra.audio.bus"}};
inline const CreationValue static2D[]{{"astra.physics2d.body","motion",u32{0}}};
inline const CreationValue circle2D[]{{"astra.physics2d.collider","shape",u32{1}}};
inline const CreationValue capsule2D[]{{"astra.physics2d.collider","shape",u32{2}}};
inline const CreationValue sensor2D[]{{"astra.physics2d.collider","sensor",true}};
inline const CreationValue propulsion2D[]{{"astra.physics2d.constant-force","relative_force_y",12.f}};
inline const CreationValue anchored2D[]{{"astra.physics2d.joint","world_anchor",true},{"astra.physics2d.joint","kind",u32{1}}};
inline const CreationValue connectedSensor2DAction[]{{"astra.physics2d.event_connection","action",1u}};
inline const CreationComponent connectedSensor2D[]{{"astra.physics2d.collider",sensor2D},{"astra.physics2d.body",static2D},{"astra.physics2d.event_connection",connectedSensor2DAction}};
inline const CreationComponent box2D[]{{"astra.physics2d.body"}};
inline const CreationComponent ground2D[]{{"astra.physics2d.body",static2D}};
inline const CreationComponent ball2D[]{{"astra.physics2d.collider",circle2D},{"astra.physics2d.body"}};
inline const CreationComponent capsuleBody2D[]{{"astra.physics2d.collider",capsule2D},{"astra.physics2d.body"}};
inline const CreationComponent trigger2D[]{{"astra.physics2d.collider",sensor2D}};
inline const CreationComponent thruster2D[]{{"astra.physics2d.body"},{"astra.physics2d.constant-force",propulsion2D}};
inline const CreationComponent hinge2D[]{{"astra.physics2d.body"},{"astra.physics2d.joint",anchored2D}};
inline const CreationComponent path[]{{"astra.path"}};
inline const CreationComponent pathFollow[]{{"astra.path.follow"}};
inline const CreationComponent pathCamera[]{{camera},{"astra.path.follow"}};
// Editable initial geometry, stored in the same Path value as subsequent edits.
inline constexpr resources::CurvePoint3D pathPoints[]{
  {0,{-3,0,0},{},{0,0,2}},
  {0,{0,0,3},{-2,0,0},{2,0,0}},
  {0,{3,0,0},{0,0,2},{}}
};
} // namespace recipe

inline constexpr const char *creationCategories[]{"Básicos","Geometria","Água","Física","Luzes","Gameplay","Áudio","Física 2D"};
// Ícones das categorias no trilho da folha de criação (nomes do catálogo do atlas).
inline constexpr std::string_view creationCategoryIcons[]{"scene/object","primitive/cube","nature/water",
  "component/physics","lighting/sun","component/timer","audio/source","physics/body-2d"};
static_assert(std::size(creationCategoryIcons)==std::size(creationCategories));
inline const std::array<EditorCreationEntry,81> editorCreationCatalog{{
  {"field2d.gravity",EditorWidget::None,7,"Campo de gravidade 2D","Área de gravidade local no plano XY.",ui::UiIcon::PhysicsFieldGravity2d,runtime::ObjectKind::Folder,recipe::fieldGravity2D,CreationPose::ViewTarget,0,{},{},"Area2D Gravity"},
  {"field2d.wind",EditorWidget::None,7,"Campo de vento 2D","Vento sobre massa real de corpos Box2D.",ui::UiIcon::PhysicsFieldWind2d,runtime::ObjectKind::Folder,recipe::fieldWind2D,CreationPose::ViewTarget,0,{},{},"Area2D Wind"},
  {"field2d.drag",EditorWidget::None,7,"Campo de arrasto 2D","Amortecimento local linear e angular.",ui::UiIcon::PhysicsFieldDrag2d,runtime::ObjectKind::Folder,recipe::fieldDrag2D,CreationPose::ViewTarget,0,{},{},"Area2D Damp"},
  {"field2d.radial",EditorWidget::None,7,"Campo radial 2D","Atração, repulsão ou vórtice XY.",ui::UiIcon::PhysicsFieldRadial2d,runtime::ObjectKind::Folder,recipe::fieldRadial2D,CreationPose::ViewTarget,0,{},{},"Area2D Point Gravity"},
  {"basic.empty",EditorWidget::CreateGroup,0,"Objeto vazio","Organiza filhos e transforma o conjunto.",ui::UiIcon::EditorAuthorObject},
  {"basic.camera",EditorWidget::None,0,"Câmera","Captura a vista atual para executar a cena.",ui::UiIcon::EditorAuthorCamera,
    runtime::ObjectKind::Camera,recipe::cameraRecipe,CreationPose::EditorCamera,0,{},{},"Camera Camera3D"},
  {"basic.follow_camera",EditorWidget::None,0,"Câmera seguidora","Cria câmera que acompanha o objeto selecionado no Play.",
    ui::UiIcon::ComponentCameraFollow,runtime::ObjectKind::Camera,recipe::followCamera,CreationPose::BehindSelection,0,
    recipe::follow,"target","Follow Cinemachine"},
  {"geometry.cube",EditorWidget::CreateCube,1,"Cubo","Malha com transformação e material editáveis.",ui::UiIcon::PrimitiveCube},
  {"geometry.ground",EditorWidget::CreateGround,1,"Chão","Superfície geométrica para compor o cenário.",ui::UiIcon::EditorAuthorGrid},
  {"water.surface",EditorWidget::CreateFiniteWater,2,"Superfície de água","Volume finito com profundidade e corrente.",ui::UiIcon::WaterAuthorSurface},
  {"water.ocean",EditorWidget::CreateOceanWater,2,"Oceano","Superfície extensa com ondas espectrais.",ui::UiIcon::NatureWater},
  {"water.river",EditorWidget::CreateRiverWater,2,"Rio por pontos","Traçado com largura, profundidade e fluxo.",ui::UiIcon::WaterAuthorRoute},
  {"water.buoyant_box",EditorWidget::CreateBuoyantBox,3,"Caixa flutuante","Corpo rígido com massa e arrasto na água.",ui::UiIcon::WaterAuthorPhysics},
  {"import.model",EditorWidget::ImportModel,1,"Importar modelo","Abre um .glb do aparelho e traz suas malhas.",ui::UiIcon::AssetsImport},
  // Modelos de cena (Scene Templates da Unity, aplicados à cena aberta).
  {"basic.scene_template",EditorWidget::CreateSceneTemplate,0,"Modelo de cena","Cenário pronto para comparar mudanças, com vistas salvas.",ui::UiIcon::SceneLayers},
  {"light.directional",EditorWidget::None,4,"Luz direcional","Ilumina a cena na direção do objeto.",ui::UiIcon::LightDirectional,
    runtime::ObjectKind::Light,recipe::directional,CreationPose::ViewTargetTilted,0,{},{},"Directional Light Sol"},
  {"light.point",EditorWidget::None,4,"Luz pontual","Emite em todas as direções a partir da posição.",ui::UiIcon::LightPoint,
    runtime::ObjectKind::Light,recipe::point,CreationPose::ViewTarget,2,{},{},"Point Light Omni"},
  {"light.spot",EditorWidget::None,4,"Luz spot","Emite dentro de um cone orientável.",ui::UiIcon::LightSpot,
    runtime::ObjectKind::Light,recipe::spot,CreationPose::ViewTargetFacing,2,{},{},"Spot Light Cone"},
  {"physics.joint_fixed",EditorWidget::None,3,"Junta fixa","Corpo dinâmico com junta; selecione o corpo conectado e ajuste as âncoras.",ui::UiIcon::PhysicsJointFixed,runtime::ObjectKind::Folder,recipe::fixedJoint,CreationPose::ViewTarget,2,"astra.physics.joint","connected_body","Joint fixed",scene::PrimitiveType::Count,false,{},"astra.physics.joint"},
  {"physics.joint_cone",EditorWidget::None,3,"Junta de cone","Corpo dinâmico com junta; selecione o corpo conectado e ajuste as âncoras.",ui::UiIcon::PhysicsJointCone,runtime::ObjectKind::Folder,recipe::coneJoint,CreationPose::ViewTarget,2,"astra.physics.joint","connected_body","Joint cone",scene::PrimitiveType::Count,false,{},"astra.physics.joint"},
  {"physics.joint_swing_twist",EditorWidget::None,3,"Junta swing / twist","Corpo dinâmico com junta; selecione o corpo conectado e ajuste as âncoras.",ui::UiIcon::PhysicsJointSwingTwist,runtime::ObjectKind::Folder,recipe::swingJoint,CreationPose::ViewTarget,2,"astra.physics.joint","connected_body","Joint swing_twist",scene::PrimitiveType::Count,false,{},"astra.physics.joint"},
  {"physics.joint_six_dof",EditorWidget::None,3,"Junta configurável","Corpo dinâmico com junta; selecione o corpo conectado e ajuste as âncoras.",ui::UiIcon::PhysicsJointSixDof,runtime::ObjectKind::Folder,recipe::configurableJoint,CreationPose::ViewTarget,2,"astra.physics.joint","connected_body","Joint six_dof",scene::PrimitiveType::Count,false,{},"astra.physics.joint"},
  {"physics.joint_spring",EditorWidget::None,3,"Mola de distância","Corpo dinâmico com junta; selecione o corpo conectado e ajuste as âncoras.",ui::UiIcon::PhysicsJointSpring,runtime::ObjectKind::Folder,recipe::springJoint,CreationPose::ViewTarget,2,"astra.physics.joint","connected_body","Joint spring",scene::PrimitiveType::Count,false,{},"astra.physics.joint"},
  {"physics.static_cylinder",EditorWidget::None,3,"Cilindro de colisão","Cilindro Jolt com tampas planas; raio e meia altura editáveis.",ui::UiIcon::PhysicsStaticCylinder,runtime::ObjectKind::Folder,recipe::staticCylinder,CreationPose::ViewTarget,2,{},{},"CylinderShape3D"},
  {"physics.dynamic_cylinder",EditorWidget::None,3,"Cilindro dinâmico","Cilindro Jolt com tampas planas; raio e meia altura editáveis.",ui::UiIcon::PhysicsDynamicCylinder,runtime::ObjectKind::Folder,recipe::dynamicCylinder,CreationPose::ViewTarget,2,{},{},"CylinderShape3D"},
  {"physics.sensor_cylinder",EditorWidget::None,3,"Sensor cilíndrico","Cilindro Jolt com tampas planas; raio e meia altura editáveis.",ui::UiIcon::PhysicsSensorCylinder,runtime::ObjectKind::Folder,recipe::triggerCylinder,CreationPose::ViewTarget,2,{},{},"CylinderShape3D"},
  {"physics.kinematic_cylinder",EditorWidget::None,3,"Cilindro cinemático","Cilindro Jolt com tampas planas; raio e meia altura editáveis.",ui::UiIcon::PhysicsKinematicCylinder,runtime::ObjectKind::Folder,recipe::kinematicCylinder,CreationPose::ViewTarget,2,{},{},"CylinderShape3D"},
  {"physics.static_box",EditorWidget::None,3,"Caixa de colisão","Corpo estático com colisor de caixa, sem malha visual.",ui::UiIcon::PhysicsStaticBox,
    runtime::ObjectKind::Folder,recipe::staticBox,CreationPose::ViewTarget,0,{},{},"Box Collider StaticBody3D"},
  {"physics.static_sphere",EditorWidget::None,3,"Esfera de colisão","Corpo estático com colisor esférico, sem malha visual.",ui::UiIcon::PhysicsStaticSphere,
    runtime::ObjectKind::Folder,recipe::staticSphere,CreationPose::ViewTarget,0,{},{},"Sphere Collider StaticBody3D"},
  {"physics.static_capsule",EditorWidget::None,3,"Cápsula de colisão","Corpo estático com colisor de cápsula, sem malha visual.",ui::UiIcon::PhysicsStaticCapsule,
    runtime::ObjectKind::Folder,recipe::staticCapsule,CreationPose::ViewTarget,0,{},{},"Capsule Collider StaticBody3D"},
  {"physics.dynamic_box",EditorWidget::None,3,"Caixa dinâmica","Corpo dinâmico com colisor de caixa.",ui::UiIcon::PhysicsDynamicBox,
    runtime::ObjectKind::Folder,recipe::dynamicBox,CreationPose::ViewTarget,2,{},{},"Rigidbody RigidBody3D"},
  {"physics.dynamic_sphere",EditorWidget::None,3,"Esfera dinâmica","Corpo dinâmico com colisor esférico, sem malha visual.",ui::UiIcon::PhysicsDynamicSphere,
    runtime::ObjectKind::Folder,recipe::dynamicSphere,CreationPose::ViewTarget,2,{},{},"Rigidbody RigidBody3D"},
  {"physics.dynamic_capsule",EditorWidget::None,3,"Cápsula dinâmica","Corpo dinâmico com colisor de cápsula, sem malha visual.",ui::UiIcon::PhysicsDynamicCapsule,
    runtime::ObjectKind::Folder,recipe::dynamicCapsule,CreationPose::ViewTarget,2,{},{},"Rigidbody RigidBody3D"},
  {"physics.trigger_box",EditorWidget::None,3,"Sensor de caixa","Corpo sensor com colisor de caixa e eventos de contato.",ui::UiIcon::PhysicsSensorBox,
    runtime::ObjectKind::Folder,recipe::triggerBox,CreationPose::ViewTarget,0,{},{},"Trigger Area3D"},
  {"physics.trigger_sphere",EditorWidget::None,3,"Sensor esférico","Corpo sensor com colisor esférico e eventos de contato.",ui::UiIcon::PhysicsSensorSphere,
    runtime::ObjectKind::Folder,recipe::triggerSphere,CreationPose::ViewTarget,0,{},{},"Trigger Area3D"},
  {"physics.trigger_capsule",EditorWidget::None,3,"Sensor de cápsula","Corpo sensor com colisor de cápsula e eventos de contato.",ui::UiIcon::PhysicsSensorCapsule,
    runtime::ObjectKind::Folder,recipe::triggerCapsule,CreationPose::ViewTarget,0,{},{},"Trigger Area3D"},
  {"physics.kinematic_box",EditorWidget::None,3,"Caixa cinemática","Corpo cinemático com colisor de caixa, movido pela autoria ou script.",ui::UiIcon::PhysicsKinematicBox,
    runtime::ObjectKind::Folder,recipe::kinematicBox,CreationPose::ViewTarget,0,{},{},"Kinematic AnimatableBody3D"},
  {"physics.kinematic_sphere",EditorWidget::None,3,"Esfera cinemática","Corpo cinemático com colisor esférico, movido pela autoria ou script.",ui::UiIcon::PhysicsKinematicSphere,
    runtime::ObjectKind::Folder,recipe::kinematicSphere,CreationPose::ViewTarget,0,{},{},"Kinematic AnimatableBody3D"},
  {"physics.character",EditorWidget::None,3,"Personagem","Controlador físico com cápsula própria.",ui::UiIcon::ComponentCharacter,
    runtime::ObjectKind::Folder,recipe::characterRecipe,CreationPose::ViewTarget,1,{},{},"CharacterController CharacterBody3D"},
  {"physics.character_cylinder",EditorWidget::None,3,"Personagem cilíndrico","Raiz Character com cilindro visual.",ui::UiIcon::ComponentCharacter,
    runtime::ObjectKind::Folder,recipe::characterRecipe,CreationPose::ViewTarget,0,{},{},"Character Cylinder Jogador Cilindro",scene::PrimitiveType::Count,false,{},recipe::character,scene::PrimitiveType::Cylinder},
  {"physics.motor_cylinder",EditorWidget::None,3,"Cilindro com motor","Exemplo físico; configure locomoção em qualquer objeto pelas suas ações.",ui::UiIcon::ComponentDynamicBodyMotor,
    runtime::ObjectKind::Folder,recipe::motorCylinder,CreationPose::ViewTarget,1,{},{},"DynamicCylinder DynamicBodyMotor Rigidbody Jogador Cilindro",scene::PrimitiveType::Count,false,{},recipe::dynamicMotor,scene::PrimitiveType::Cylinder},
  {"physics2d.connected_sensor",EditorWidget::None,3,"Sensor 2D conectado","Ativa o receptor selecionado ao entrar no sensor XY.",ui::UiIcon::EventPhysicsConnection2d,
    runtime::ObjectKind::Folder,recipe::connectedSensor2D,CreationPose::ViewTarget,0,"astra.physics2d.event_connection","receiver","Area2D Sensor Evento",scene::PrimitiveType::Count,false,{},"astra.physics2d.event_connection"},
  {"physics.connected_sensor",EditorWidget::None,3,"Sensor conectado","Ativa o receptor selecionado quando outro corpo entra. Sem receptor, configure a referência na Inspeção.",ui::UiIcon::EventPhysicsConnection,
    runtime::ObjectKind::Folder,recipe::connectedSensor,CreationPose::ViewTarget,0,recipe::physicsConnection,"receiver","Trigger Area3D Evento Conexao",scene::PrimitiveType::Count,false,{},recipe::physicsConnection},
  {"gameplay.event_connection",EditorWidget::None,5,"Conexão de evento","Liga um evento deste objeto a ativar objetos ou chamar métodos, sem script.",ui::UiIcon::ComponentEventConnection,
    runtime::ObjectKind::Folder,recipe::eventConnectionRecipe,CreationPose::ViewTarget,0,recipe::eventConnection,"receiver","UnityEvent Signal Evento Conexao",scene::PrimitiveType::Count,false,{},recipe::eventConnection},
  {"gameplay.sound_trigger",EditorWidget::None,5,"Gatilho sonoro","Sensor que toca o próprio som quando outro corpo entra.",ui::UiIcon::EventSoundTrigger,
    runtime::ObjectKind::Folder,recipe::soundTrigger,CreationPose::ViewTarget,0,{},{},"Trigger Audio Som Evento",scene::PrimitiveType::Count,false,{},recipe::eventConnection},
  {"gameplay.timer",EditorWidget::None,5,"Timer","Dispara eventos para comportamentos em intervalos configuráveis.",ui::UiIcon::ComponentTimer,
    runtime::ObjectKind::Folder,recipe::timerRecipe,CreationPose::ViewTarget,0,{},{},"Timer"},
  {"geometry.sphere",EditorWidget::None,1,"Esfera","Diâmetro 1 m · colisão esférica.",ui::UiIcon::PrimitiveSphere,
    runtime::ObjectKind::Mesh,{},CreationPose::ViewTarget,1,{},{},"Sphere",scene::PrimitiveType::Sphere},
  {"geometry.capsule",EditorWidget::None,1,"Cápsula","Altura 2 m · colisão de cápsula.",ui::UiIcon::PrimitiveCapsule,
    runtime::ObjectKind::Mesh,{},CreationPose::ViewTarget,1,{},{},"Capsule",scene::PrimitiveType::Capsule},
  {"geometry.cylinder",EditorWidget::None,1,"Cilindro","Altura 2 m · colisão por casco convexo de 32 lados.",ui::UiIcon::PrimitiveCylinder,
    runtime::ObjectKind::Mesh,{},CreationPose::ViewTarget,1,{},{},"Cylinder",scene::PrimitiveType::Cylinder},
  {"geometry.plane",EditorWidget::None,1,"Plano","10 × 10 m em XZ · colisão triangular.",ui::UiIcon::PrimitivePlane,
    runtime::ObjectKind::Mesh,{},CreationPose::ViewTarget,0,{},{},"Plane",scene::PrimitiveType::Plane},
  {"geometry.quad",EditorWidget::None,1,"Quad","1 × 1 m em XY · dois triângulos com colisão.",ui::UiIcon::PrimitiveQuad,
    runtime::ObjectKind::Mesh,{},CreationPose::ViewTarget,1,{},{},"Quad",scene::PrimitiveType::Quad},
  {"physics.propulsion",EditorWidget::None,3,"Propulsor físico","Corpo dinâmico e caixa; força local contínua de 4 N, sem gravidade.",ui::UiIcon::ComponentConstantForce,
    runtime::ObjectKind::Folder,recipe::propulsion,CreationPose::ViewTarget,1,{},{},"ConstantForce Propulsão"},
  {"field.gravity",EditorWidget::None,3,"Campo de gravidade","Volume com gravidade local; corpos dinâmicos no centro de massa.",ui::UiIcon::PhysicsFieldGravity,runtime::ObjectKind::Folder,recipe::fieldGravity,CreationPose::ViewTarget,0,{},{},"Area3D Gravity"},
  {"field.wind",EditorWidget::None,3,"Campo de vento","Velocidade do ar e acoplamento; respeita massa e camada.",ui::UiIcon::PhysicsFieldWind,runtime::ObjectKind::Folder,recipe::fieldWind,CreationPose::ViewTarget,0,{},{},"Area3D Wind"},
  {"field.drag",EditorWidget::None,3,"Campo de arrasto","Amortecimento linear e angular local em volume.",ui::UiIcon::PhysicsFieldDrag,runtime::ObjectKind::Folder,recipe::fieldDrag,CreationPose::ViewTarget,0,{},{},"Area3D Damp Drag"},
  {"field.radial",EditorWidget::None,3,"Campo radial","Atração, repulsão e vórtice; centro finito e editável.",ui::UiIcon::PhysicsFieldRadial,runtime::ObjectKind::Folder,recipe::fieldRadial,CreationPose::ViewTarget,0,{},{},"Area3D Point Gravity Vortex"},
  {"spring.position",EditorWidget::None,5,"Mola de posição","Segue a seleção com frequência e amortecimento.",ui::UiIcon::ComponentSpringPosition,runtime::ObjectKind::Folder,recipe::springPosition,CreationPose::ViewTarget,0,"astra.spring.position","target","Spring SmoothDamp Position"},
  {"spring.rotation",EditorWidget::None,5,"Mola de rotação","Segue quaternion por caminhos curtos; velocidade em graus/s.",ui::UiIcon::ComponentSpringRotation,runtime::ObjectKind::Folder,recipe::springRotation,CreationPose::ViewTarget,0,"astra.spring.rotation","target","Spring SmoothDampAngle Rotation"},
  {"spring.scale",EditorWidget::None,5,"Mola de escala","Segue escala positiva; offset é multiplicador.",ui::UiIcon::ComponentSpringScale,runtime::ObjectKind::Folder,recipe::springScale,CreationPose::ViewTarget,0,"astra.spring.scale","target","Spring SmoothDamp Scale"},
  {"constraint.position",EditorWidget::None,5,"Seguir posição","Posição ligada à seleção; fonte editável na Inspeção.",ui::UiIcon::ComponentPositionConstraint,
    runtime::ObjectKind::Folder,recipe::positionConstraint,CreationPose::ViewTarget,0,"astra.constraint.position","target","PositionConstraint"},
  {"constraint.rotation",EditorWidget::None,5,"Seguir rotação","Rotação ligada à seleção, com peso e eixos.",ui::UiIcon::ComponentRotationConstraint,
    runtime::ObjectKind::Folder,recipe::rotationConstraint,CreationPose::ViewTarget,0,"astra.constraint.rotation","target","RotationConstraint"},
  {"constraint.scale",EditorWidget::None,5,"Seguir escala","Escala ligada à seleção, com peso e eixos.",ui::UiIcon::ComponentScaleConstraint,
    runtime::ObjectKind::Folder,recipe::scaleConstraint,CreationPose::ViewTarget,0,"astra.constraint.scale","target","ScaleConstraint"},
  {"constraint.aim",EditorWidget::None,5,"Mirar seleção","Orienta o eixo escolhido para a seleção em Play.",ui::UiIcon::ComponentAimConstraint,
    runtime::ObjectKind::Folder,recipe::aimConstraint,CreationPose::ViewTarget,0,"astra.constraint.aim","target","AimConstraint LookAt"},
  {"constraint.parent",EditorWidget::None,5,"Seguir pose","Posição e rotação relativas à seleção; escala independente.",ui::UiIcon::ComponentParentConstraint,
    runtime::ObjectKind::Folder,recipe::parentConstraint,CreationPose::ViewTarget,0,"astra.constraint.parent","target","ParentConstraint"},
  {"constraint.look_at",EditorWidget::None,5,"Olhar seleção","Orienta +Z para a seleção, com vetor vertical e roll.",ui::UiIcon::ComponentLookAtConstraint,
    runtime::ObjectKind::Folder,recipe::lookAtConstraint,CreationPose::ViewTarget,0,"astra.constraint.look_at","target","LookAtConstraint"},
  {"tween.transform",EditorWidget::None,5,"Tween de transformação","Movimento local de ida e volta; canais e repetição editáveis.",ui::UiIcon::ComponentTweenTransform,
    runtime::ObjectKind::Folder,recipe::tween,CreationPose::ViewTarget,0,{},{},"Tween Transform"},
  {"tween.connected",EditorWidget::None,5,"Tween conectado","Ativa o receptor selecionado depois do destino final; ciclo finito.",ui::UiIcon::EventTweenCompletion,
    runtime::ObjectKind::Folder,recipe::connectedTween,CreationPose::ViewTarget,0,"astra.tween.transform","finished_target","Tween Concluir Evento Conexao",scene::PrimitiveType::Count,false,{},"astra.tween.transform"},
  {"audio.source",EditorWidget::None,6,"Fonte sonora","Emissor real; atribua um WAV importado na Inspeção.",ui::UiIcon::AudioSource,
    runtime::ObjectKind::Folder,recipe::audioSource,CreationPose::ViewTarget,0,{},{},"AudioSource AudioStreamPlayer3D"},
  {"audio.listener",EditorWidget::None,6,"Ouvinte","Pose de escuta espacial; prioridade seleciona um ouvinte.",ui::UiIcon::AudioListener,
    runtime::ObjectKind::Folder,recipe::audioListener,CreationPose::EditorCamera,0,{},{},"AudioListener"},
  {"audio.bus",EditorWidget::None,6,"Bus de áudio","Ganho, mute e solo em cadeia; saída Master por padrão.",ui::UiIcon::AudioBus,
    runtime::ObjectKind::Folder,recipe::audioBus,CreationPose::ViewTarget,0,{},{},"Audio Bus Mixer"},
  {"audio.import",EditorWidget::ImportWave,6,"Importar WAV","Valida PCM e registra um clipe reutilizável no projeto.",ui::UiIcon::AudioClip},
  {"physics2d.box",EditorWidget::None,7,"Caixa dinâmica 2D","Box2D experimental no plano XY; sem malha visual.",ui::UiIcon::PhysicsBody2d,
    runtime::ObjectKind::Folder,recipe::box2D,CreationPose::ViewTarget,2,{},{},"Rigidbody2D Box"},
  {"physics2d.ground",EditorWidget::None,7,"Caixa estática 2D","Colisão XY estática; tamanho editável no colisor.",ui::UiIcon::PhysicsCollider2d,
    runtime::ObjectKind::Folder,recipe::ground2D,CreationPose::ViewTarget,0,{},{},"StaticBody2D"},
  {"physics2d.circle",EditorWidget::None,7,"Círculo dinâmico 2D","Corpo e colisor circular reais no plano XY.",ui::UiIcon::PhysicsBody2d,
    runtime::ObjectKind::Folder,recipe::ball2D,CreationPose::ViewTarget,2,{},{},"CircleCollider2D"},
  {"physics2d.capsule",EditorWidget::None,7,"Cápsula dinâmica 2D","Corpo e cápsula vertical reais no plano XY.",ui::UiIcon::PhysicsBody2d,
    runtime::ObjectKind::Folder,recipe::capsuleBody2D,CreationPose::ViewTarget,2,{},{},"CapsuleCollider2D"},
  {"physics2d.sensor",EditorWidget::None,7,"Sensor 2D","Sensor estático com eventos; não gera resposta de contato.",ui::UiIcon::PhysicsCollider2d,
    runtime::ObjectKind::Folder,recipe::trigger2D,CreationPose::ViewTarget,0,{},{},"Trigger Area2D"},
  {"physics2d.thruster",EditorWidget::None,7,"Propulsor 2D","Corpo dinâmico e força local de 12 N em cada passo.",ui::UiIcon::PhysicsConstantForce2d,
    runtime::ObjectKind::Folder,recipe::thruster2D,CreationPose::ViewTarget,1,{},{},"ConstantForce2D"},
  {"physics2d.hinge",EditorWidget::None,7,"Pivô 2D no mundo","Corpo dinâmico e junta Revolute ancorada; motor e limites editáveis.",ui::UiIcon::PhysicsJoint2d,
    runtime::ObjectKind::Folder,recipe::hinge2D,CreationPose::ViewTarget,1,{},{},"HingeJoint2D Revolute",scene::PrimitiveType::Count,true},
  {"path.curve",EditorWidget::None,5,"Caminho Bézier","Três pontos editáveis; posição e tangentes com histórico.",ui::UiIcon::PathCurve,
    runtime::ObjectKind::Folder,recipe::path,CreationPose::ViewTarget,0,{},{},"Path3D Curve3D Spline Caminho",scene::PrimitiveType::Count,false,recipe::pathPoints},
  {"path.follower",EditorWidget::None,5,"Seguidor de caminho","Escolha um Path; avance por velocidade ou duração.",ui::UiIcon::ComponentPathFollow,
    runtime::ObjectKind::Folder,recipe::pathFollow,CreationPose::ViewTarget,0,"astra.path.follow","target","PathFollow3D Seguidor"},
  {"path.camera",EditorWidget::None,5,"Câmera no caminho","Câmera e seguidor real; selecione o Path antes de criar.",ui::UiIcon::ComponentPathFollow,
    runtime::ObjectKind::Camera,recipe::pathCamera,CreationPose::EditorCamera,0,"astra.path.follow","target","Camera Rail PathFollow3D"}
}};
inline constexpr u32 kCreationRecipeRange=0x0010'0000u;
static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(editorCreationCatalog)>><kCreationRecipeRange);

// O toque que executa a entrada: a ação própria, ou o endereço da receita.
inline u32 creationWidget(u32 index) {
  if(index>=editorCreationCatalog.size()) return 0;
  const auto &entry=editorCreationCatalog[index];
  return entry.composed()?widgetId(EditorWidget::CreationRecipeBase)+index:widgetId(entry.action);
}
inline const EditorCreationEntry *findCreationRecipe(std::string_view id,u32 *index=nullptr) {
  for(u32 i=0;i<editorCreationCatalog.size();++i) if(editorCreationCatalog[i].id==id) {
    if(index) *index=i;
    return &editorCreationCatalog[i];
  }
  return nullptr;
}
// Busca por nome e por termos de outras engines; `query` já normalizada.
inline bool creationMatches(const EditorCreationEntry &entry,std::string_view query) {
  return editorSearchKey(entry.name).find(query)!=std::string::npos ||
         editorSearchKey(entry.searchTerms).find(query)!=std::string::npos;
}
// Visível no menu: busca vazia mostra a categoria escolhida; com busca, todas.
inline bool creationListed(const EditorCreationEntry &entry,std::string_view query,unsigned category) {
  return query.empty()?entry.category==category:creationMatches(entry,query);
}
inline std::vector<u8> creationAlwaysAvailable() {
  std::vector<u8> available(editorCreationCatalog.size(),0);
  for(u32 i=0;i<editorCreationCatalog.size();++i) {
    const auto action=editorCreationCatalog[i].action;
    if(scene::validPrimitive(editorCreationCatalog[i].primitive)||scene::validPrimitive(editorCreationCatalog[i].childVisual)) continue;
    if(action!=EditorWidget::CreateCube && action!=EditorWidget::CreateGround &&
       action!=EditorWidget::CreateFiniteWater && action!=EditorWidget::CreateOceanWater &&
       action!=EditorWidget::CreateRiverWater && action!=EditorWidget::CreateBuoyantBox &&
       action!=EditorWidget::CreateSceneTemplate) available[i]=1;
  }
  return available;
}
inline void enableCreation(EditorScreenState &state,EditorWidget action) {
  if(state.creationAvailable.size()!=editorCreationCatalog.size())
    state.creationAvailable.resize(editorCreationCatalog.size(),0);
  for(u32 i=0;i<editorCreationCatalog.size();++i)
    if(editorCreationCatalog[i].action==action) {state.creationAvailable[i]=1;return;}
}
inline bool creationAvailable(const EditorScreenState &state,u32 index) {
  return index<editorCreationCatalog.size() && index<state.creationAvailable.size() &&
         editorCreationCatalog[index].action!=EditorWidget::ImportModel && state.creationAvailable[index]!=0;
}
// Temporary capability adapter for the existing imported water library.
inline bool waterCreationAvailable(const EditorScreenState &state) {
  for(u32 i=0;i<editorCreationCatalog.size();++i) {
    const auto action=editorCreationCatalog[i].action;
    if((action==EditorWidget::CreateFiniteWater || action==EditorWidget::CreateOceanWater ||
        action==EditorWidget::CreateRiverWater) && creationAvailable(state,i)) return true;
  }
  return false;
}
}
