#pragma once
#include "editor/editor_screen.h"
#include "scene/component_properties.h"
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
  bool composed() const {return action==EditorWidget::None;}
};

namespace recipe {
using scene::ComponentPropertyValue;
inline constexpr std::string_view body="astra.physics.body",collider="astra.physics.collider",
  light="astra.render.light",camera="astra.camera",follow="astra.camera.follow",
  character="astra.physics.character",timer="astra.time.timer";
inline constexpr u32 Static=0,Kinematic=1,Dynamic=2;       // scene::BodyMotion
inline constexpr u32 Box=0,Sphere=1,Capsule=2;             // scene::ColliderShape
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
#undef AE_BODY_RECIPE

inline const CreationComponent characterRecipe[]{{character}};
inline const CreationComponent timerRecipe[]{{timer}};
inline const CreationComponent cameraRecipe[]{{camera}};
inline const CreationComponent followCamera[]{{camera},{follow}};
} // namespace recipe

inline constexpr const char *creationCategories[]{"Básicos","Geometria","Água","Física","Luzes","Gameplay"};
inline const std::array<EditorCreationEntry,27> editorCreationCatalog{{
  {"basic.empty",EditorWidget::CreateGroup,0,"Objeto vazio","Organiza filhos e transforma o conjunto.",ui::UiIcon::EditorAuthorObject},
  {"basic.camera",EditorWidget::None,0,"Câmera","Captura a vista atual para executar a cena.",ui::UiIcon::EditorAuthorCamera,
    runtime::ObjectKind::Camera,recipe::cameraRecipe,CreationPose::EditorCamera,0,{},{},"Camera Camera3D"},
  {"basic.follow_camera",EditorWidget::None,0,"Câmera seguidora","Cria câmera que acompanha o objeto selecionado no Play.",
    ui::UiIcon::EditorAuthorCamera,runtime::ObjectKind::Camera,recipe::followCamera,CreationPose::BehindSelection,0,
    recipe::follow,"target","Follow Cinemachine"},
  {"geometry.cube",EditorWidget::CreateCube,1,"Cubo","Malha com transformação e material editáveis.",ui::UiIcon::EditorAuthorObject},
  {"geometry.ground",EditorWidget::CreateGround,1,"Chão","Superfície geométrica para compor o cenário.",ui::UiIcon::EditorAuthorGrid},
  {"water.surface",EditorWidget::CreateFiniteWater,2,"Superfície de água","Volume finito com profundidade e corrente.",ui::UiIcon::WaterAuthorSurface},
  {"water.ocean",EditorWidget::CreateOceanWater,2,"Oceano","Superfície extensa com ondas espectrais.",ui::UiIcon::WaterAuthorSurface},
  {"water.river",EditorWidget::CreateRiverWater,2,"Rio por pontos","Traçado com largura, profundidade e fluxo.",ui::UiIcon::WaterAuthorRoute},
  {"water.buoyant_box",EditorWidget::CreateBuoyantBox,3,"Caixa flutuante","Corpo rígido com massa e arrasto na água.",ui::UiIcon::WaterAuthorPhysics},
  {"import.model",EditorWidget::ImportModel,1,"Importar modelo","Abre um .glb do aparelho e traz suas malhas.",ui::UiIcon::EditorAuthorZoom},
  // Modelos de cena (Scene Templates da Unity, aplicados à cena aberta).
  {"basic.scene_template",EditorWidget::CreateSceneTemplate,0,"Modelo de cena","Cenário pronto para comparar mudanças, com vistas salvas.",ui::UiIcon::SceneLayers},
  {"light.directional",EditorWidget::None,4,"Luz direcional","Ilumina a cena na direção do objeto.",ui::UiIcon::LightingSun,
    runtime::ObjectKind::Light,recipe::directional,CreationPose::ViewTargetTilted,0,{},{},"Directional Light Sol"},
  {"light.point",EditorWidget::None,4,"Luz pontual","Emite em todas as direções a partir da posição.",ui::UiIcon::LightingSun,
    runtime::ObjectKind::Light,recipe::point,CreationPose::ViewTarget,2,{},{},"Point Light Omni"},
  {"light.spot",EditorWidget::None,4,"Luz spot","Emite dentro de um cone orientável.",ui::UiIcon::LightingSun,
    runtime::ObjectKind::Light,recipe::spot,CreationPose::ViewTargetFacing,2,{},{},"Spot Light Cone"},
  {"physics.static_box",EditorWidget::None,3,"Caixa de colisão","Corpo estático com colisor de caixa, sem malha visual.",ui::UiIcon::ComponentCollider,
    runtime::ObjectKind::Folder,recipe::staticBox,CreationPose::ViewTarget,0,{},{},"Box Collider StaticBody3D"},
  {"physics.static_sphere",EditorWidget::None,3,"Esfera de colisão","Corpo estático com colisor esférico, sem malha visual.",ui::UiIcon::ComponentCollider,
    runtime::ObjectKind::Folder,recipe::staticSphere,CreationPose::ViewTarget,0,{},{},"Sphere Collider StaticBody3D"},
  {"physics.static_capsule",EditorWidget::None,3,"Cápsula de colisão","Corpo estático com colisor de cápsula, sem malha visual.",ui::UiIcon::ComponentCollider,
    runtime::ObjectKind::Folder,recipe::staticCapsule,CreationPose::ViewTarget,0,{},{},"Capsule Collider StaticBody3D"},
  {"physics.dynamic_box",EditorWidget::None,3,"Caixa dinâmica","Corpo dinâmico com colisor de caixa.",ui::UiIcon::ComponentPhysics,
    runtime::ObjectKind::Folder,recipe::dynamicBox,CreationPose::ViewTarget,2,{},{},"Rigidbody RigidBody3D"},
  {"physics.dynamic_sphere",EditorWidget::None,3,"Esfera dinâmica","Corpo dinâmico com colisor esférico, sem malha visual.",ui::UiIcon::PhysicsDynamicSphere,
    runtime::ObjectKind::Folder,recipe::dynamicSphere,CreationPose::ViewTarget,2,{},{},"Rigidbody RigidBody3D"},
  {"physics.dynamic_capsule",EditorWidget::None,3,"Cápsula dinâmica","Corpo dinâmico com colisor de cápsula, sem malha visual.",ui::UiIcon::PhysicsDynamicCapsule,
    runtime::ObjectKind::Folder,recipe::dynamicCapsule,CreationPose::ViewTarget,2,{},{},"Rigidbody RigidBody3D"},
  {"physics.trigger_box",EditorWidget::None,3,"Sensor de caixa","Corpo sensor com colisor de caixa e eventos de contato.",ui::UiIcon::ComponentPhysics,
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
  {"gameplay.timer",EditorWidget::None,5,"Timer","Dispara eventos para comportamentos em intervalos configuráveis.",ui::UiIcon::ScriptingCode,
    runtime::ObjectKind::Folder,recipe::timerRecipe,CreationPose::ViewTarget,0,{},{},"Timer"}
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
