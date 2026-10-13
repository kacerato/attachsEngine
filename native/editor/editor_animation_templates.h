#pragma once
// Modelos de cena para os tutoriais de animacao e locomocao (N03-N06). Feitos
// de cubos autorais, entao funcionam em projeto vazio; cada um ja nasce com os
// componentes reais ligados entre si (IK, apoio, interacao, Personagem,
// colisores e camera), montados pelo mesmo caminho da autoria manual e
// desfeitos com um unico Desfazer.
//
// Convencao dos indices: cada lista de nos e local ao modelo; componentes
// apontam para nos pelo indice. Juntas sao pastas (pose do osso); a malha
// fica num filho "visual" para a escala do cubo nao passar para os filhos.
#include "editor/editor_scene_template_types.h"

namespace ae::editor::detail {

#define AE_JOINT(name,parent,x,y,z) {name,parent,{x,y,z},{0,0,0},{1,1,1},false,SceneTemplateLight::None,0,0,{1,1,1},false}
#define AE_CUBE(name,parent,x,y,z,sx,sy,sz,r,g,b) {name,parent,{x,y,z},{0,0,0},{sx,sy,sz},true,SceneTemplateLight::None,0,0,{r,g,b},false}
#define AE_CUBE_R(name,parent,x,y,z,rx,ry,rz,sx,sy,sz,r,g,b) {name,parent,{x,y,z},{rx,ry,rz},{sx,sy,sz},true,SceneTemplateLight::None,0,0,{r,g,b},false}
#define AE_SUN(parent) {"Sol",parent,{0,8,0},{50,-35,0},{1,1,1},false,SceneTemplateLight::Directional,100000,0,{1,.96f,.90f},false}

// Corpo humano de 1,8 m: pelve a 1 m, pernas de 0,93 m, bracos de 0,55 m.
#define AE_MANNEQUIN_NODES \
  AE_JOINT("Manequim",-1,0,0,0),                                   /* 0 */ \
  AE_JOINT("Pelve",0,0,1,0),                                       /* 1 */ \
  AE_CUBE("Pelve · visual",1,0,0,0,.35f,.18f,.2f,.30f,.45f,.70f),  /* 2 */ \
  AE_JOINT("Coluna",1,0,.12f,0),                                   /* 3 */ \
  AE_CUBE("Coluna · visual",3,0,.15f,0,.32f,.3f,.18f,.32f,.48f,.74f), /* 4 */ \
  AE_JOINT("Peito",3,0,.3f,0),                                     /* 5 */ \
  AE_CUBE("Peito · visual",5,0,.12f,0,.42f,.26f,.22f,.34f,.52f,.78f), /* 6 */ \
  AE_JOINT("Cabeça",5,0,.33f,0),                                   /* 7 */ \
  AE_CUBE("Cabeça · visual",7,0,.12f,0,.22f,.25f,.24f,.86f,.72f,.62f), /* 8 */ \
  AE_JOINT("Braço dir.",5,-.27f,.2f,0),                            /* 9 */ \
  AE_CUBE("Braço dir. · visual",9,0,-.14f,0,.1f,.28f,.1f,.34f,.52f,.78f), /* 10 */ \
  AE_JOINT("Antebraço dir.",9,0,-.28f,.03f),                       /* 11 */ \
  AE_CUBE("Antebraço dir. · visual",11,0,-.13f,0,.09f,.26f,.09f,.86f,.72f,.62f), /* 12 */ \
  AE_JOINT("Mão dir.",11,0,-.27f,0),                               /* 13 */ \
  AE_CUBE("Mão dir. · visual",13,0,-.05f,0,.08f,.1f,.05f,.86f,.72f,.62f), /* 14 */ \
  AE_JOINT("Braço esq.",5,.27f,.2f,0),                             /* 15 */ \
  AE_CUBE("Braço esq. · visual",15,0,-.14f,0,.1f,.28f,.1f,.34f,.52f,.78f), /* 16 */ \
  AE_JOINT("Antebraço esq.",15,0,-.28f,.03f),                      /* 17 */ \
  AE_CUBE("Antebraço esq. · visual",17,0,-.13f,0,.09f,.26f,.09f,.86f,.72f,.62f), /* 18 */ \
  AE_JOINT("Mão esq.",17,0,-.27f,0),                               /* 19 */ \
  AE_CUBE("Mão esq. · visual",19,0,-.05f,0,.08f,.1f,.05f,.86f,.72f,.62f), /* 20 */ \
  AE_JOINT("Coxa dir.",1,-.12f,-.08f,0),                           /* 21 */ \
  AE_CUBE("Coxa dir. · visual",21,0,-.21f,0,.13f,.42f,.13f,.22f,.24f,.30f), /* 22 */ \
  AE_JOINT("Canela dir.",21,0,-.43f,.03f),                         /* 23 */ \
  AE_CUBE("Canela dir. · visual",23,0,-.2f,0,.11f,.4f,.11f,.22f,.24f,.30f), /* 24 */ \
  AE_JOINT("Pé dir.",23,0,-.42f,0),                                /* 25 */ \
  AE_CUBE("Pé dir. · visual",25,0,-.03f,.06f,.11f,.06f,.22f,.15f,.15f,.18f), /* 26 */ \
  AE_JOINT("Coxa esq.",1,.12f,-.08f,0),                            /* 27 */ \
  AE_CUBE("Coxa esq. · visual",27,0,-.21f,0,.13f,.42f,.13f,.22f,.24f,.30f), /* 28 */ \
  AE_JOINT("Canela esq.",27,0,-.43f,.03f),                         /* 29 */ \
  AE_CUBE("Canela esq. · visual",29,0,-.2f,0,.11f,.4f,.11f,.22f,.24f,.30f), /* 30 */ \
  AE_JOINT("Pé esq.",29,0,-.42f,0),                                /* 31 */ \
  AE_CUBE("Pé esq. · visual",31,0,-.03f,.06f,.11f,.06f,.22f,.15f,.15f,.18f), /* 32 */ \
  AE_CUBE("Chão",-1,0,-.1f,0,14,.2f,14,.55f,.56f,.52f),          /* 33 */ \
  AE_SUN(-1)                                                       /* 34 */

// Mesma exposicao do cenario de referencia: o sol de 100.000 lux pede -6,8 EV.
inline constexpr SceneTemplateVolume kAnimationDayExposure[]{{"Exposição do dia",-1,{0,0,0},false,{1,1,1},-6.8f,0,0,1,1}};

// 1. Manequim com IK de mao, olhar e apoio dos pes (um pe sobre um bloco).
inline constexpr SceneTemplateNode kMannequinNodes[]{
  AE_MANNEQUIN_NODES,
  AE_CUBE("Alvo da mão",-1,-.45f,1.3f,.3f,.08f,.08f,.08f,.95f,.65f,.15f), /* 35 */
  AE_CUBE("Olhar",-1,.6f,1.7f,1.6f,.12f,.12f,.12f,.95f,.65f,.15f),        /* 36 */
  AE_CUBE("Bloco sob o pé",-1,-.12f,.075f,.05f,.4f,.15f,.45f,.48f,.46f,.42f), /* 37 */
};
inline constexpr SceneTemplateComponent kMannequinComponents[]{
  {33,SceneTemplateComponentKind::StaticCollider},{37,SceneTemplateComponentKind::StaticCollider},
  {13,SceneTemplateComponentKind::TwoBoneIk,35},
  {7,SceneTemplateComponentKind::LookAt,36,-1,1},
  {25,SceneTemplateComponentKind::FootGrounding},{31,SceneTemplateComponentKind::FootGrounding},
  {0,SceneTemplateComponentKind::Grounder,1},
};
inline constexpr SceneTemplateView kMannequinViews[]{{"Manequim",{0,1,0},4,.6f,.15f},{"Pés no bloco",{0,.2f,0},2,.9f,.25f}};

// 2. Quadrupede: tronco, pescoco e cabeca, quatro patas de dois ossos e cauda
// com inercia; uma rampa sob as patas da frente.
inline constexpr SceneTemplateNode kQuadrupedNodes[]{
  AE_JOINT("Quadrúpede",-1,0,0,0),                                                  /* 0 */
  AE_JOINT("Tronco",0,0,.62f,0),                                                    /* 1 */
  AE_CUBE("Tronco · visual",1,0,0,0,.34f,.26f,.9f,.80f,.48f,.20f),                 /* 2 */
  AE_JOINT("Pescoço",1,0,.1f,.42f),                                                 /* 3 */
  AE_CUBE("Pescoço · visual",3,0,.1f,.06f,.14f,.24f,.14f,.80f,.48f,.20f),          /* 4 */
  AE_JOINT("Cabeça",3,0,.24f,.1f),                                                  /* 5 */
  AE_CUBE("Cabeça · visual",5,0,0,.08f,.2f,.18f,.3f,.86f,.56f,.26f),               /* 6 */
#define AE_LEG(prefix,base,x,z) \
  AE_JOINT(prefix " · ombro",1,x,-.08f,z),                                           \
  AE_CUBE(prefix " · ombro visual",base,0,-.13f,0,.08f,.26f,.08f,.70f,.40f,.18f),  \
  AE_JOINT(prefix " · joelho",base,0,-.26f,-.03f),                                  \
  AE_CUBE(prefix " · joelho visual",base+2,0,-.12f,0,.07f,.24f,.07f,.30f,.22f,.18f), \
  AE_JOINT(prefix " · pata",base+2,0,-.24f,.03f),                                   \
  AE_CUBE(prefix " · pata visual",base+4,0,-.02f,.03f,.08f,.04f,.12f,.20f,.15f,.12f)
  AE_LEG("Frente dir.",7,-.12f,.32f),     /* 7..12, pata 11 */
  AE_LEG("Frente esq.",13,.12f,.32f),     /* 13..18, pata 17 */
  AE_LEG("Trás dir.",19,-.12f,-.32f),     /* 19..24, pata 23 */
  AE_LEG("Trás esq.",25,.12f,-.32f),      /* 25..30, pata 29 */
#undef AE_LEG
  AE_JOINT("Cauda",1,0,.06f,-.45f),                                                 /* 31 */
  AE_CUBE("Cauda · visual",31,0,0,-.12f,.07f,.07f,.24f,.86f,.56f,.26f),            /* 32 */
  AE_JOINT("Cauda · meio",31,0,0,-.24f),                                            /* 33 */
  AE_CUBE("Cauda · meio visual",33,0,0,-.12f,.06f,.06f,.24f,.95f,.92f,.88f),       /* 34 */
  AE_JOINT("Cauda · ponta",33,0,0,-.24f),                                           /* 35 */
  AE_CUBE("Chão",-1,0,-.1f,0,14,.2f,14,.55f,.56f,.52f),                           /* 36 */
  AE_CUBE_R("Rampa",-1,0,.02f,.75f,-12,0,0,1.4f,.2f,1,.48f,.46f,.42f),            /* 37 */
  AE_SUN(-1),                                                                       /* 38 */
};
inline constexpr SceneTemplateComponent kQuadrupedComponents[]{
  {36,SceneTemplateComponentKind::StaticCollider},{37,SceneTemplateComponentKind::StaticCollider},
  {11,SceneTemplateComponentKind::FootGrounding},{17,SceneTemplateComponentKind::FootGrounding},
  {23,SceneTemplateComponentKind::FootGrounding},{29,SceneTemplateComponentKind::FootGrounding},
  {0,SceneTemplateComponentKind::Grounder,1},
  {31,SceneTemplateComponentKind::Inertia},{33,SceneTemplateComponentKind::Inertia},
};
inline constexpr SceneTemplateView kQuadrupedViews[]{{"Quadrúpede",{0,.5f,0},3.2f,1.1f,.2f}};

// 3. Braco mecanico: base, quatro segmentos e garra; IK de cadeia (FABRIK)
// ate o alvo, com limite de rotacao no primeiro segmento.
inline constexpr SceneTemplateNode kArmNodes[]{
  AE_JOINT("Braço mecânico",-1,0,0,0),                                          /* 0 */
  AE_CUBE("Base",0,0,.1f,0,.6f,.2f,.6f,.25f,.27f,.30f),                        /* 1 */
  AE_JOINT("Segmento 1",0,0,.2f,0),                                             /* 2 */
  AE_CUBE("Segmento 1 · visual",2,0,.2f,0,.14f,.4f,.14f,.95f,.70f,.10f),       /* 3 */
  AE_JOINT("Segmento 2",2,0,.4f,0),                                             /* 4 */
  AE_CUBE("Segmento 2 · visual",4,0,.2f,0,.12f,.4f,.12f,.95f,.70f,.10f),       /* 5 */
  AE_JOINT("Segmento 3",4,0,.4f,0),                                             /* 6 */
  AE_CUBE("Segmento 3 · visual",6,0,.18f,0,.1f,.36f,.1f,.95f,.70f,.10f),       /* 7 */
  AE_JOINT("Segmento 4",6,0,.36f,0),                                            /* 8 */
  AE_CUBE("Segmento 4 · visual",8,0,.15f,0,.09f,.3f,.09f,.95f,.70f,.10f),      /* 9 */
  AE_JOINT("Garra",8,0,.3f,0),                                                  /* 10 */
  AE_CUBE("Garra · visual",10,0,.04f,0,.18f,.08f,.08f,.25f,.27f,.30f),         /* 11 */
  AE_CUBE("Alvo da garra",-1,.7f,.9f,.5f,.1f,.1f,.1f,.30f,.80f,.95f),          /* 12 */
  AE_CUBE("Chão",-1,0,-.1f,0,8,.2f,8,.55f,.56f,.52f),                          /* 13 */
  AE_SUN(-1),                                                                   /* 14 */
};
inline constexpr SceneTemplateComponent kArmComponents[]{
  {13,SceneTemplateComponentKind::StaticCollider},
  {10,SceneTemplateComponentKind::ChainIk,12,-1,4},
  {2,SceneTemplateComponentKind::RotationCone,-1,-1,60},
};
inline constexpr SceneTemplateView kArmViews[]{{"Braço",{.2f,.8f,.2f},3,.8f,.2f}};

// 4. Interacao sem script: o manequim alcanca e pega a caixa ao dar Play.
inline constexpr SceneTemplateNode kInteractionNodes[]{
  AE_MANNEQUIN_NODES,
  AE_CUBE("Caixa",-1,-.4f,1.25f,.3f,.14f,.14f,.14f,.80f,.30f,.25f),            /* 35 */
};
inline constexpr SceneTemplateComponent kInteractionComponents[]{
  {33,SceneTemplateComponentKind::StaticCollider},
  {35,SceneTemplateComponentKind::InteractionObject},
  {0,SceneTemplateComponentKind::InteractionSystem,13,35},
  {25,SceneTemplateComponentKind::FootGrounding},{31,SceneTemplateComponentKind::FootGrounding},
  {0,SceneTemplateComponentKind::Grounder,1},
};
inline constexpr SceneTemplateView kInteractionViews[]{{"Pegar a caixa",{-.2f,1,.2f},2.6f,.4f,.12f}};

// 4b. Carregar com as duas maos (Final IK 2.2, Interaction Pick Up 2-Handed):
// a mesa, a caixa, as pegadas e o ponto de carregar vem da mesma montagem que o
// autor aplica a um humanoide importado (Acoes do objeto > Carregar caixa).
inline constexpr SceneTemplateNode kCarryNodes[]{AE_MANNEQUIN_NODES};
inline constexpr SceneTemplateComponent kCarryComponents[]{{33,SceneTemplateComponentKind::StaticCollider}};
inline constexpr SceneTemplateView kCarryViews[]{{"Carregar a caixa",{0,.9f,.3f},3.2f,1.1f,.15f}};

// 5. Personagem jogavel: corpo visivel, camera seguidora e um percurso com
// rampa, degraus e passagem baixa (agachar). Locomocao com aceleracao e
// frenagem; o modo (Solo, Voo, Nado, Dirigir) se troca no inspector.
inline constexpr SceneTemplateNode kPlayableNodes[]{
  AE_JOINT("Personagem",-1,0,.05f,0),                                         /* 0 */
  AE_CUBE("Corpo · visual",0,0,.75f,0,.5f,1.1f,.3f,.30f,.45f,.70f),          /* 1 */
  AE_CUBE("Cabeça · visual",0,0,1.5f,0,.3f,.3f,.3f,.86f,.72f,.62f),          /* 2 */
  AE_CUBE("Frente · visual",0,0,1.5f,.18f,.1f,.08f,.08f,.15f,.15f,.18f),     /* 3 */
  AE_CUBE("Chão",-1,0,-.1f,6,24,.2f,32,.55f,.56f,.52f),                      /* 4 */
  AE_CUBE_R("Rampa",-1,-4,.6f,6,-15,0,0,3,.2f,5,.48f,.46f,.42f),            /* 5 */
  AE_CUBE("Degrau 1",-1,3,.1f,4,2,.2f,1,.62f,.60f,.56f),                     /* 6 */
  AE_CUBE("Degrau 2",-1,3,.2f,5,2,.4f,1,.62f,.60f,.56f),                     /* 7 */
  AE_CUBE("Degrau 3",-1,3,.3f,6,2,.6f,1,.62f,.60f,.56f),                     /* 8 */
  AE_CUBE("Pilar esq.",-1,-.9f,.7f,10,.3f,1.4f,.3f,.35f,.37f,.40f),          /* 9 */
  AE_CUBE("Pilar dir.",-1,.9f,.7f,10,.3f,1.4f,.3f,.35f,.37f,.40f),           /* 10 */
  AE_CUBE("Passagem baixa",-1,0,1.5f,10,2.1f,.2f,1.2f,.35f,.37f,.40f),       /* 11 */
  {"Câmera",-1,{0,2.4f,-4.5f},{18,0,0},{1,1,1},false,SceneTemplateLight::None,0,0,{1,1,1},false}, /* 12 */
  AE_SUN(-1),                                                                 /* 13 */
};
inline constexpr SceneTemplateComponent kPlayableComponents[]{
  {4,SceneTemplateComponentKind::StaticCollider},{5,SceneTemplateComponentKind::StaticCollider},
  {6,SceneTemplateComponentKind::StaticCollider},{7,SceneTemplateComponentKind::StaticCollider},
  {8,SceneTemplateComponentKind::StaticCollider},{9,SceneTemplateComponentKind::StaticCollider},
  {10,SceneTemplateComponentKind::StaticCollider},{11,SceneTemplateComponentKind::StaticCollider},
  {0,SceneTemplateComponentKind::Character},
  {12,SceneTemplateComponentKind::FollowCamera,0},
};
inline constexpr SceneTemplateView kPlayableViews[]{{"Percurso",{0,.8f,5},12,.9f,.35f}};

#undef AE_JOINT
#undef AE_CUBE
#undef AE_CUBE_R
#undef AE_SUN
#undef AE_MANNEQUIN_NODES
} // namespace ae::editor::detail
