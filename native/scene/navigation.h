// Navegação (bloco J, F067–F070): Superfície, Agente, Obstáculo, Link e
// Modificador. Os valores aqui são autoria; o bake (editor/editor_navigation)
// e a execução (runtime/scene_navigation) são os consumidores.
//
// Referências: Unity AI Navigation 2.0
// https://docs.unity3d.com/Packages/com.unity.ai.navigation@2.0/manual/NavMeshSurface.html
// https://docs.unity3d.com/6000.0/Documentation/Manual/class-NavMeshAgent.html
// https://docs.unity3d.com/6000.0/Documentation/Manual/class-NavMeshObstacle.html
// https://docs.unity3d.com/Packages/com.unity.ai.navigation@2.0/manual/NavMeshLink.html
// https://docs.unity3d.com/Packages/com.unity.ai.navigation@2.0/manual/NavMeshModifier.html
// Godot 4.5 NavigationRegion3D / NavigationAgent3D / NavigationObstacle3D / NavigationLink3D
// https://docs.godotengine.org/en/4.5/classes/class_navigationagent3d.html
// Adaptações: o Agente escolhe a superfície (ou a mais próxima) em vez de
// "tipo de agente"; o Obstáculo sempre recorta (TileCache) — o modo só de
// desvio da Unity não existe aqui; o Agente pode perseguir um objeto alvo.
#pragma once
#include "scene/components.h"
#include "scene/physics_field.h"

#include <array>
#include <cmath>
#include <limits>

namespace ae::scene {

enum class NavCollect : u32 {All=0,Children=1,Volume=2};
// Áreas embutidas, na ordem do arquivo: nunca renumerar.
enum class NavArea : u32 {Walkable=0,NotWalkable=1,Jump=2,Difficult=3};
enum class NavAvoidance : u32 {None=0,Low=1,Medium=2,Good=3,High=4};
enum class NavObstacleShape : u32 {Box=0,Cylinder=1};
enum class NavModifierMode : u32 {Area=0,Ignore=1};

inline constexpr std::array<ComponentEnumOption,3> navCollectOptions{{{0,"Toda a cena"},{1,"Este objeto e filhos"},{2,"Volume"}}};
inline constexpr std::array<ComponentEnumOption,4> navAreaOptions{{{0,"Caminhável"},{1,"Não caminhável"},{2,"Salto"},{3,"Difícil"}}};
inline constexpr std::array<ComponentEnumOption,3> navLinkAreaOptions{{{0,"Caminhável"},{2,"Salto"},{3,"Difícil"}}};
inline constexpr std::array<ComponentEnumOption,5> navAvoidanceOptions{{{0,"Nenhum"},{1,"Baixo"},{2,"Médio"},{3,"Bom"},{4,"Alto"}}};
inline constexpr std::array<ComponentEnumOption,2> navObstacleShapeOptions{{{0,"Caixa"},{1,"Cilindro"}}};
inline constexpr std::array<ComponentEnumOption,2> navModifierModeOptions{{{0,"Alterar área"},{1,"Ignorar no bake"}}};

template<class T> bool navNumbersValid(const T &value) {
  for(const auto &p:T::descriptor.numbers) {const float v=p.read(value);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
  return true;
}
template<class T> void navWriteNumbers(const T &value,std::ostream &out) {for(const auto &p:T::descriptor.numbers) out<<' '<<p.read(value);}
template<class T> bool navReadNumbers(T &value,std::istream &in) {for(const auto &p:T::descriptor.numbers) if(!(in>>*p.write(value))) return false;return true;}

// --- Superfície -----------------------------------------------------------------
class NavSurface final : public ComponentValue {
public:
  bool enabled=true;
  NavCollect collect=NavCollect::All;
  u32 layer=0;
  float volumeCenter[3]{0,0,0},volumeSize[3]{20,6,20};
  float agentRadius=.5f,agentHeight=2,agentMaxClimb=.4f,agentMaxSlope=45;
  float cellSize=.2f,cellHeight=.1f,minRegionArea=2;
  float edgeMaxLength=12,edgeMaxError=1.3f,detailSampleDistance=6,detailSampleMaxError=1,tileSize=64;
  resources::AssetGuid data{};
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<NavSurface>(*this);}
  bool valid() const override {
    return navNumbersValid(*this)&&static_cast<u32>(collect)<=2&&layer<=32&&tileSize==std::floor(tileSize);
  }
  void write(std::ostream &out) const override {
    out<<enabled<<' '<<static_cast<u32>(collect)<<' '<<layer<<' '<<data.high<<' '<<data.low;navWriteNumbers(*this,out);
  }
  bool read(std::istream &in,u32 version) override {
    u32 c=0;if(version!=1||!(in>>enabled>>c>>layer>>data.high>>data.low)) return false;
    collect=static_cast<NavCollect>(c);return navReadNumbers(*this,in)&&valid();
  }
};
inline const NavSurface &navSurface(const ComponentValue &v) {return static_cast<const NavSurface &>(v);}
inline NavSurface &navSurface(ComponentValue &v) {return static_cast<NavSurface &>(v);}

// --- Agente ---------------------------------------------------------------------
class NavAgent final : public ComponentValue {
public:
  bool enabled=true,autoBraking=true,updateRotation=true,useJump=true,useDifficult=true;
  u64 surface=0,target=0;
  float speed=3.5f,angularSpeed=120,acceleration=8,stoppingDistance=0;
  float radius=.5f,height=2,baseOffset=0;
  float jumpCost=1,difficultCost=4,repathDistance=.5f;
  NavAvoidance avoidance=NavAvoidance::High;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<NavAgent>(*this);}
  bool valid() const override {
    return navNumbersValid(*this)&&static_cast<u32>(avoidance)<=4&&surface<=std::numeric_limits<u32>::max()&&
           target<=std::numeric_limits<u32>::max();
  }
  void write(std::ostream &out) const override {
    out<<enabled<<' '<<autoBraking<<' '<<updateRotation<<' '<<useJump<<' '<<useDifficult<<' '<<surface<<' '<<target<<' '
       <<static_cast<u32>(avoidance);navWriteNumbers(*this,out);
  }
  bool read(std::istream &in,u32 version) override {
    u32 a=0;if(version!=1||!(in>>enabled>>autoBraking>>updateRotation>>useJump>>useDifficult>>surface>>target>>a)) return false;
    avoidance=static_cast<NavAvoidance>(a);return navReadNumbers(*this,in)&&valid();
  }
};
inline const NavAgent &navAgent(const ComponentValue &v) {return static_cast<const NavAgent &>(v);}
inline NavAgent &navAgent(ComponentValue &v) {return static_cast<NavAgent &>(v);}

// --- Obstáculo ------------------------------------------------------------------
class NavObstacle final : public ComponentValue {
public:
  bool enabled=true,carveOnlyStationary=true;
  NavObstacleShape shape=NavObstacleShape::Box;
  float center[3]{0,0,0},size[3]{1,1,1};
  float radius=.5f,height=2,moveThreshold=.1f,stationaryTime=.5f;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<NavObstacle>(*this);}
  bool valid() const override {return navNumbersValid(*this)&&static_cast<u32>(shape)<=1;}
  void write(std::ostream &out) const override {out<<enabled<<' '<<carveOnlyStationary<<' '<<static_cast<u32>(shape);navWriteNumbers(*this,out);}
  bool read(std::istream &in,u32 version) override {
    u32 s=0;if(version!=1||!(in>>enabled>>carveOnlyStationary>>s)) return false;
    shape=static_cast<NavObstacleShape>(s);return navReadNumbers(*this,in)&&valid();
  }
};
inline const NavObstacle &navObstacle(const ComponentValue &v) {return static_cast<const NavObstacle &>(v);}
inline NavObstacle &navObstacle(ComponentValue &v) {return static_cast<NavObstacle &>(v);}

// --- Link -----------------------------------------------------------------------
class NavLink final : public ComponentValue {
public:
  bool enabled=true,bidirectional=true;
  NavArea area=NavArea::Walkable;
  float start[3]{0,0,-1},end[3]{0,0,1};
  float radius=.5f;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<NavLink>(*this);}
  bool valid() const override {return navNumbersValid(*this)&&area!=NavArea::NotWalkable&&static_cast<u32>(area)<=3;}
  void write(std::ostream &out) const override {out<<enabled<<' '<<bidirectional<<' '<<static_cast<u32>(area);navWriteNumbers(*this,out);}
  bool read(std::istream &in,u32 version) override {
    u32 a=0;if(version!=1||!(in>>enabled>>bidirectional>>a)) return false;
    area=static_cast<NavArea>(a);return navReadNumbers(*this,in)&&valid();
  }
};
inline const NavLink &navLink(const ComponentValue &v) {return static_cast<const NavLink &>(v);}
inline NavLink &navLink(ComponentValue &v) {return static_cast<NavLink &>(v);}

// --- Modificador ----------------------------------------------------------------
class NavModifier final : public ComponentValue {
public:
  bool applyToChildren=true;
  NavModifierMode mode=NavModifierMode::Area;
  NavArea area=NavArea::NotWalkable;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<NavModifier>(*this);}
  bool valid() const override {return static_cast<u32>(mode)<=1&&static_cast<u32>(area)<=3;}
  void write(std::ostream &out) const override {out<<applyToChildren<<' '<<static_cast<u32>(mode)<<' '<<static_cast<u32>(area);}
  bool read(std::istream &in,u32 version) override {
    u32 m=0,a=0;if(version!=1||!(in>>applyToChildren>>m>>a)) return false;
    mode=static_cast<NavModifierMode>(m);area=static_cast<NavArea>(a);return valid();
  }
};
inline const NavModifier &navModifier(const ComponentValue &v) {return static_cast<const NavModifier &>(v);}
inline NavModifier &navModifier(ComponentValue &v) {return static_cast<NavModifier &>(v);}

namespace nav_detail {
inline bool volume(const ComponentValue &v) {return navSurface(v).collect==NavCollect::Volume;}
inline bool usesDifficult(const ComponentValue &v) {return navAgent(v).useDifficult;}
inline bool usesJump(const ComponentValue &v) {return navAgent(v).useJump;}
inline bool rotates(const ComponentValue &v) {return navAgent(v).updateRotation;}
inline bool chases(const ComponentValue &v) {return navAgent(v).target!=0;}
inline bool box(const ComponentValue &v) {return navObstacle(v).shape==NavObstacleShape::Box;}
inline bool cylinder(const ComponentValue &v) {return navObstacle(v).shape==NavObstacleShape::Cylinder;}
inline bool stationary(const ComponentValue &v) {return navObstacle(v).carveOnlyStationary;}
inline bool changesArea(const ComponentValue &v) {return navModifier(v).mode==NavModifierMode::Area;}
} // namespace nav_detail

#define AE_NAV_NUMBER(TYPE,ACCESS,LABEL,MIN,MAX,STEP,MEMBER,ID,GROUP,UNIT,HELP,VISIBLE) \
  ComponentNumber{LABEL,MIN,MAX,STEP,[](const ComponentValue &v)->const float &{return ACCESS(v).MEMBER;}, \
   [](ComponentValue &v)->float *{return &ACCESS(v).MEMBER;},ID,{GROUP,UNIT,HELP,VISIBLE},false}

inline constexpr std::array<ComponentNumber,18> navSurfaceNumbers{{
  AE_NAV_NUMBER(NavSurface,navSurface,"Raio do agente",0,50,.05f,agentRadius,"agent_radius","Agente","m","Distância mantida das paredes; a malha recua por este raio",nullptr),
  AE_NAV_NUMBER(NavSurface,navSurface,"Altura do agente",.05f,100,.1f,agentHeight,"agent_height","Agente","m","Espaço livre mínimo acima do chão",nullptr),
  AE_NAV_NUMBER(NavSurface,navSurface,"Altura do degrau",0,50,.05f,agentMaxClimb,"agent_max_climb","Agente","m","Desnível que o agente sobe sem link",nullptr),
  AE_NAV_NUMBER(NavSurface,navSurface,"Inclinação máxima",0,89,1,agentMaxSlope,"agent_max_slope","Agente","°","Rampas mais íngremes não entram na malha",nullptr),
  AE_NAV_NUMBER(NavSurface,navSurface,"Centro X",-100000,100000,.1f,volumeCenter[0],"volume_center_x","Coleta","m",nullptr,nav_detail::volume),
  AE_NAV_NUMBER(NavSurface,navSurface,"Centro Y",-100000,100000,.1f,volumeCenter[1],"volume_center_y","Coleta","m",nullptr,nav_detail::volume),
  AE_NAV_NUMBER(NavSurface,navSurface,"Centro Z",-100000,100000,.1f,volumeCenter[2],"volume_center_z","Coleta","m",nullptr,nav_detail::volume),
  AE_NAV_NUMBER(NavSurface,navSurface,"Tamanho X",.1f,100000,.5f,volumeSize[0],"volume_size_x","Coleta","m",nullptr,nav_detail::volume),
  AE_NAV_NUMBER(NavSurface,navSurface,"Tamanho Y",.1f,100000,.5f,volumeSize[1],"volume_size_y","Coleta","m",nullptr,nav_detail::volume),
  AE_NAV_NUMBER(NavSurface,navSurface,"Tamanho Z",.1f,100000,.5f,volumeSize[2],"volume_size_z","Coleta","m",nullptr,nav_detail::volume),
  AE_NAV_NUMBER(NavSurface,navSurface,"Tamanho da célula",.01f,10,.01f,cellSize,"cell_size","Precisão","m","Menor detalhe horizontal; menor é mais preciso e mais lento",nullptr),
  AE_NAV_NUMBER(NavSurface,navSurface,"Altura da célula",.01f,10,.01f,cellHeight,"cell_height","Precisão","m","Menor detalhe vertical",nullptr),
  AE_NAV_NUMBER(NavSurface,navSurface,"Área mínima de região",0,10000,.5f,minRegionArea,"min_region_area","Precisão","m²","Ilhas menores são descartadas",nullptr),
  AE_NAV_NUMBER(NavSurface,navSurface,"Tile",16,128,16,tileSize,"tile_size","Precisão","células","Lado do tile; obstáculos reconstroem só os tiles que tocam",nullptr),
  AE_NAV_NUMBER(NavSurface,navSurface,"Aresta máxima",0,1000,1,edgeMaxLength,"edge_max_length","Avançado","m","Zero não limita o comprimento das bordas",nullptr),
  AE_NAV_NUMBER(NavSurface,navSurface,"Erro de borda",.1f,10,.1f,edgeMaxError,"edge_max_error","Avançado","células","Quanto o contorno pode se afastar das paredes",nullptr),
  AE_NAV_NUMBER(NavSurface,navSurface,"Amostra de detalhe",0,100,.5f,detailSampleDistance,"detail_sample_distance","Avançado","células","Menor que 1 desliga a malha de detalhe de altura",nullptr),
  AE_NAV_NUMBER(NavSurface,navSurface,"Erro de detalhe",0,100,.5f,detailSampleMaxError,"detail_sample_max_error","Avançado","células",nullptr,nullptr),
}};
inline constexpr std::array<ComponentBoolean,1> navSurfaceBooleans{{
  {"enabled","Ativa",[](const ComponentValue &v){return navSurface(v).enabled;},[](ComponentValue &v,bool b){navSurface(v).enabled=b;},
   {"Geral","","Desligada não carrega a malha no Play; o bake é preservado"}},
}};
inline constexpr std::array<ComponentEnum,2> navSurfaceEnums{{
  {"collect","Coletar",navCollectOptions,[](const ComponentValue &v){return static_cast<u32>(navSurface(v).collect);},
   [](ComponentValue &v,u32 x){navSurface(v).collect=static_cast<NavCollect>(x);},
   {"Coleta","","Colisores estáticos que entram no bake"}},
  {"layer","Camada dos colisores",fieldLayers,[](const ComponentValue &v){return navSurface(v).layer;},
   [](ComponentValue &v,u32 x){navSurface(v).layer=x;},{"Coleta","","Todas ou só uma camada física do projeto"}},
}};
inline constexpr std::array<ComponentTriple,2> navSurfaceTriples{{
  {"volume_center","Centro do volume",{"volume_center_x","volume_center_y","volume_center_z"}},
  {"volume_size","Tamanho do volume",{"volume_size_x","volume_size_y","volume_size_z"}},
}};
inline const std::array<ComponentResourceBinding,1> navSurfaceResources{{
  {"data","Malha assada",resources::AssetType::NavMesh,
   [](const ComponentValue &){return 1u;},
   [](const ComponentValue &v,u32){return navSurface(v).data;},
   [](ComponentValue &v,u32 slot,resources::AssetGuid g){if(slot) return false;navSurface(v).data=g;return true;},
   {"Geral","","Recurso .navmesh produzido pelo bake; o Play carrega este arquivo"}}
}};
inline constexpr std::array<ComponentMethod,2> navSurfaceMethods{{
  {"is_ready","Pronta","Verdadeiro quando a malha desta superfície está carregada no Play",{},ComponentValueKind::Boolean},
  {"polygon_count","Polígonos","Polígonos da malha carregada, com recortes de obstáculos",{},ComponentValueKind::Integer},
}};
inline const ComponentType NavSurface::descriptor{
  "astra.navigation.surface",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<NavSurface>();},
  navSurfaceNumbers,navSurfaceBooleans,navSurfaceEnums,nullptr,false,{},navSurfaceTriples,navSurfaceResources,{},{},{},navSurfaceMethods,{}};

inline constexpr std::array<ComponentNumber,10> navAgentNumbers{{
  AE_NAV_NUMBER(NavAgent,navAgent,"Velocidade",0,1000,.1f,speed,"speed","Movimento","m/s",nullptr,nullptr),
  AE_NAV_NUMBER(NavAgent,navAgent,"Aceleração",0,1000,.5f,acceleration,"acceleration","Movimento","m/s²",nullptr,nullptr),
  AE_NAV_NUMBER(NavAgent,navAgent,"Giro",0,3600,10,angularSpeed,"angular_speed","Movimento","°/s","Velocidade de giro para a direção do movimento",nav_detail::rotates),
  AE_NAV_NUMBER(NavAgent,navAgent,"Distância de parada",0,1000,.1f,stoppingDistance,"stopping_distance","Movimento","m","Chega quando o caminho restante fica abaixo disto",nullptr),
  AE_NAV_NUMBER(NavAgent,navAgent,"Raio",.05f,50,.05f,radius,"radius","Corpo","m","Usado no desvio entre agentes",nullptr),
  AE_NAV_NUMBER(NavAgent,navAgent,"Altura",.05f,100,.1f,height,"height","Corpo","m",nullptr,nullptr),
  AE_NAV_NUMBER(NavAgent,navAgent,"Deslocamento da base",-100,100,.05f,baseOffset,"base_offset","Corpo","m","Altura do pivô acima da malha quando o agente move o próprio objeto",nullptr),
  AE_NAV_NUMBER(NavAgent,navAgent,"Custo do salto",1,1000,.5f,jumpCost,"jump_cost","Áreas","×",nullptr,nav_detail::usesJump),
  AE_NAV_NUMBER(NavAgent,navAgent,"Custo da área difícil",1,1000,.5f,difficultCost,"difficult_cost","Áreas","×","Multiplica o comprimento percorrido nessa área",nav_detail::usesDifficult),
  AE_NAV_NUMBER(NavAgent,navAgent,"Refazer caminho a",.05f,100,.1f,repathDistance,"repath_distance","Movimento","m","O alvo precisa se mover isto para o caminho ser refeito",nav_detail::chases),
}};
inline constexpr std::array<ComponentBoolean,5> navAgentBooleans{{
  {"enabled","Ativo",[](const ComponentValue &v){return navAgent(v).enabled;},[](ComponentValue &v,bool b){navAgent(v).enabled=b;},
   {"Geral","","Desligado sai da multidão e para de mover o objeto"}},
  {"auto_braking","Frear ao chegar",[](const ComponentValue &v){return navAgent(v).autoBraking;},[](ComponentValue &v,bool b){navAgent(v).autoBraking=b;},
   {"Movimento","","Desacelera perto do destino; desligado mantém a velocidade até a distância de parada"}},
  {"update_rotation","Girar para o movimento",[](const ComponentValue &v){return navAgent(v).updateRotation;},[](ComponentValue &v,bool b){navAgent(v).updateRotation=b;},
   {"Movimento","","Gira o objeto em Y para a direção da velocidade"}},
  {"use_jump","Usar links e áreas de salto",[](const ComponentValue &v){return navAgent(v).useJump;},[](ComponentValue &v,bool b){navAgent(v).useJump=b;},
   {"Áreas",""}},
  {"use_difficult","Atravessar área difícil",[](const ComponentValue &v){return navAgent(v).useDifficult;},[](ComponentValue &v,bool b){navAgent(v).useDifficult=b;},
   {"Áreas",""}},
}};
inline constexpr std::array<ComponentEnum,1> navAgentEnums{{
  {"avoidance","Desvio de agentes",navAvoidanceOptions,[](const ComponentValue &v){return static_cast<u32>(navAgent(v).avoidance);},
   [](ComponentValue &v,u32 x){navAgent(v).avoidance=static_cast<NavAvoidance>(x);},
   {"Corpo","","Qualidade da amostragem de velocidades contra outros agentes"}},
}};
inline constexpr std::array<ComponentObjectReference,2> navAgentReferences{{
  {"surface","Superfície","astra.navigation.surface",ObjectReferenceScope::Any,"Mais próxima",
   [](const ComponentValue &v){return navAgent(v).surface;},[](ComponentValue &v,u64 x){navAgent(v).surface=x;},
   {"Geral","","Malha usada pelo agente; vazio escolhe a superfície que contém o objeto"}},
  {"target","Seguir objeto","",ObjectReferenceScope::Other,"Nenhum",
   [](const ComponentValue &v){return navAgent(v).target;},[](ComponentValue &v,u64 x){navAgent(v).target=x;},
   {"Geral","","Persegue o objeto sem script; SetDestination substitui o alvo"}},
}};
inline constexpr std::array<ComponentParameter,1> navPointParameter{{{"point","Ponto",ComponentValueKind::Vector3,"m"}}};
inline constexpr std::array<ComponentMethod,9> navAgentMethods{{
  {"set_destination","Ir para","Calcula o caminho e começa a andar; falso quando o ponto está longe da malha",navPointParameter,ComponentValueKind::Boolean},
  {"stop","Parar","Para no lugar e guarda o destino para Retomar"},
  {"resume","Retomar","Volta ao último destino depois de Parar"},
  {"warp","Teleportar","Move o objeto e o agente para o ponto mais próximo da malha",navPointParameter,ComponentValueKind::Boolean},
  {"remaining_distance","Distância restante","Comprimento do caminho até o destino; infinito sem caminho",{},ComponentValueKind::Number},
  {"path_status","Estado do caminho","0 sem caminho, 1 completo, 2 parcial, 3 inválido",{},ComponentValueKind::Integer},
  {"has_path","Tem caminho","Verdadeiro enquanto há um destino válido",{},ComponentValueKind::Boolean},
  {"is_on_link","Em link","Verdadeiro durante a travessia de um Link",{},ComponentValueKind::Boolean},
  {"velocity","Velocidade","Velocidade pedida pela multidão neste quadro",{},ComponentValueKind::Vector3},
}};
inline constexpr std::array<ComponentEvent,3> navAgentEvents{{
  {"destination_reached","Chegou","Emitido uma vez quando o caminho restante fica abaixo da distância de parada"},
  {"path_failed","Caminho falhou","Emitido quando o destino não tem caminho"},
  {"link_entered","Entrou no link","Emitido quando o agente começa a atravessar um Link"},
}};
inline const ComponentType NavAgent::descriptor{
  "astra.navigation.agent",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<NavAgent>();},
  navAgentNumbers,navAgentBooleans,navAgentEnums,nullptr,false,navAgentReferences,{},{},{},{},{},navAgentMethods,navAgentEvents};

inline constexpr std::array<ComponentNumber,10> navObstacleNumbers{{
  AE_NAV_NUMBER(NavObstacle,navObstacle,"Centro X",-1000,1000,.05f,center[0],"center_x","Forma","m",nullptr,nullptr),
  AE_NAV_NUMBER(NavObstacle,navObstacle,"Centro Y",-1000,1000,.05f,center[1],"center_y","Forma","m",nullptr,nullptr),
  AE_NAV_NUMBER(NavObstacle,navObstacle,"Centro Z",-1000,1000,.05f,center[2],"center_z","Forma","m",nullptr,nullptr),
  AE_NAV_NUMBER(NavObstacle,navObstacle,"Tamanho X",.01f,1000,.1f,size[0],"size_x","Forma","m",nullptr,nav_detail::box),
  AE_NAV_NUMBER(NavObstacle,navObstacle,"Tamanho Y",.01f,1000,.1f,size[1],"size_y","Forma","m",nullptr,nav_detail::box),
  AE_NAV_NUMBER(NavObstacle,navObstacle,"Tamanho Z",.01f,1000,.1f,size[2],"size_z","Forma","m",nullptr,nav_detail::box),
  AE_NAV_NUMBER(NavObstacle,navObstacle,"Raio",.01f,500,.05f,radius,"radius","Forma","m",nullptr,nav_detail::cylinder),
  AE_NAV_NUMBER(NavObstacle,navObstacle,"Altura",.01f,1000,.1f,height,"height","Forma","m",nullptr,nav_detail::cylinder),
  AE_NAV_NUMBER(NavObstacle,navObstacle,"Limiar de movimento",0,100,.05f,moveThreshold,"move_threshold","Recorte","m","Deslocamento que conta como movimento",nullptr),
  AE_NAV_NUMBER(NavObstacle,navObstacle,"Tempo até parado",0,60,.1f,stationaryTime,"stationary_time","Recorte","s","Parado por este tempo volta a recortar",nav_detail::stationary),
}};
inline constexpr std::array<ComponentBoolean,2> navObstacleBooleans{{
  {"enabled","Ativo",[](const ComponentValue &v){return navObstacle(v).enabled;},[](ComponentValue &v,bool b){navObstacle(v).enabled=b;},
   {"Geral","","Desligado devolve a área recortada à malha"}},
  {"carve_only_stationary","Recortar só parado",[](const ComponentValue &v){return navObstacle(v).carveOnlyStationary;},[](ComponentValue &v,bool b){navObstacle(v).carveOnlyStationary=b;},
   {"Recorte","","Em movimento o recorte sai; ao parar ele volta (menos reconstruções)"}},
}};
inline constexpr std::array<ComponentEnum,1> navObstacleEnums{{
  {"shape","Forma",navObstacleShapeOptions,[](const ComponentValue &v){return static_cast<u32>(navObstacle(v).shape);},
   [](ComponentValue &v,u32 x){navObstacle(v).shape=static_cast<NavObstacleShape>(x);},{"Forma","","Caixa gira com o objeto em Y; cilindro fica de pé"}},
}};
inline constexpr std::array<ComponentTriple,2> navObstacleTriples{{
  {"center","Centro",{"center_x","center_y","center_z"}},
  {"size","Tamanho",{"size_x","size_y","size_z"}},
}};
inline constexpr std::array<ComponentMethod,1> navObstacleMethods{{
  {"is_carving","Recortando","Verdadeiro quando o recorte deste obstáculo está aplicado na malha",{},ComponentValueKind::Boolean},
}};
inline const ComponentType NavObstacle::descriptor{
  "astra.navigation.obstacle",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<NavObstacle>();},
  navObstacleNumbers,navObstacleBooleans,navObstacleEnums,nullptr,false,{},navObstacleTriples,{},{},{},{},navObstacleMethods,{}};

inline constexpr std::array<ComponentNumber,7> navLinkNumbers{{
  AE_NAV_NUMBER(NavLink,navLink,"Início X",-10000,10000,.1f,start[0],"start_x","Pontos","m",nullptr,nullptr),
  AE_NAV_NUMBER(NavLink,navLink,"Início Y",-10000,10000,.1f,start[1],"start_y","Pontos","m",nullptr,nullptr),
  AE_NAV_NUMBER(NavLink,navLink,"Início Z",-10000,10000,.1f,start[2],"start_z","Pontos","m",nullptr,nullptr),
  AE_NAV_NUMBER(NavLink,navLink,"Fim X",-10000,10000,.1f,end[0],"end_x","Pontos","m",nullptr,nullptr),
  AE_NAV_NUMBER(NavLink,navLink,"Fim Y",-10000,10000,.1f,end[1],"end_y","Pontos","m",nullptr,nullptr),
  AE_NAV_NUMBER(NavLink,navLink,"Fim Z",-10000,10000,.1f,end[2],"end_z","Pontos","m",nullptr,nullptr),
  AE_NAV_NUMBER(NavLink,navLink,"Raio de conexão",.05f,50,.05f,radius,"radius","Pontos","m","Distância da malha em que cada ponta se conecta",nullptr),
}};
inline constexpr std::array<ComponentBoolean,2> navLinkBooleans{{
  {"enabled","Ativo",[](const ComponentValue &v){return navLink(v).enabled;},[](ComponentValue &v,bool b){navLink(v).enabled=b;},
   {"Geral","","Desligado remove a conexão da malha"}},
  {"bidirectional","Nos dois sentidos",[](const ComponentValue &v){return navLink(v).bidirectional;},[](ComponentValue &v,bool b){navLink(v).bidirectional=b;},
   {"Geral","","Desligado só vai do início para o fim"}},
}};
inline constexpr std::array<ComponentEnum,1> navLinkEnums{{
  {"area","Área",navLinkAreaOptions,[](const ComponentValue &v){return static_cast<u32>(navLink(v).area);},
   [](ComponentValue &v,u32 x){navLink(v).area=static_cast<NavArea>(x);},{"Geral","","Salto exige que o agente use links e áreas de salto"}},
}};
inline constexpr std::array<ComponentTriple,2> navLinkTriples{{
  {"start","Início",{"start_x","start_y","start_z"}},
  {"end","Fim",{"end_x","end_y","end_z"}},
}};
inline const ComponentType NavLink::descriptor{
  "astra.navigation.link",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<NavLink>();},
  navLinkNumbers,navLinkBooleans,navLinkEnums,nullptr,true,{},navLinkTriples,{},{},{},{},{},{}};

inline constexpr std::array<ComponentBoolean,1> navModifierBooleans{{
  {"apply_to_children","Aplicar aos filhos",[](const ComponentValue &v){return navModifier(v).applyToChildren;},[](ComponentValue &v,bool b){navModifier(v).applyToChildren=b;},
   {"Geral","","Filhos sem modificador próprio herdam este"}},
}};
inline constexpr std::array<ComponentEnum,2> navModifierEnums{{
  {"mode","Modo",navModifierModeOptions,[](const ComponentValue &v){return static_cast<u32>(navModifier(v).mode);},
   [](ComponentValue &v,u32 x){navModifier(v).mode=static_cast<NavModifierMode>(x);},{"Geral","","Ignorar tira os colisores do bake"}},
  {"area","Área",navAreaOptions,[](const ComponentValue &v){return static_cast<u32>(navModifier(v).area);},
   [](ComponentValue &v,u32 x){navModifier(v).area=static_cast<NavArea>(x);},{"Geral","","Área do chão destes colisores na malha",nav_detail::changesArea}},
}};
inline const ComponentType NavModifier::descriptor{
  "astra.navigation.modifier",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<NavModifier>();},
  {},navModifierBooleans,navModifierEnums,nullptr,false,{},{},{},{},{},{},{},{}};
#undef AE_NAV_NUMBER

} // namespace ae::scene
