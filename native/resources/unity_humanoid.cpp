// SPDX-License-Identifier: Apache-2.0
//
// As tabelas e a matemática de músculos deste arquivo são uma tradução para C++
// de humanoid/human_trait.gd e humanoid/transform_util.gd do Unidot Importer:
//   Copyright 2023-present Lyuma and contributors
//   Copyright 2022-2023 lox9973
//   Licensed under the Apache License, Version 2.0 (the "License"); you may not
//   use this file except in compliance with the License. You may obtain a copy
//   of the License at http://www.apache.org/licenses/LICENSE-2.0
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
//   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// As poses de descanso do esqueleto de referência vêm do SkeletonProfileHumanoid
// da Godot 4.3 (MIT, Copyright (c) 2014-present Godot Engine contributors).
// Texto das licenças e créditos em native/third_party/unity_humanoid/.
#include "resources/unity_humanoid.h"
#include "resources/animation_curve.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <string>

namespace ae::resources {
namespace {
struct Q {double x=0,y=0,z=0,w=1;};
struct V {double x=0,y=0,z=0;};
Q mul(const Q &a,const Q &b) {
  return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y+a.y*b.w+a.z*b.x-a.x*b.z,
          a.w*b.z+a.z*b.w+a.x*b.y-a.y*b.x,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
Q conj(const Q &q) {return {-q.x,-q.y,-q.z,q.w};}
Q norm(const Q &q) {const double l=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return l>0?Q{q.x/l,q.y/l,q.z/l,q.w/l}:Q{};}
V operator+(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V operator-(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
V operator*(V a,double s){return {a.x*s,a.y*s,a.z*s};}
double dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
V unit(V a){const double l=std::sqrt(dot(a,a));return l>0?a*(1/l):V{};}
V rotate(const Q &q,V v) {const auto r=mul(mul(q,Q{v.x,v.y,v.z,0}),conj(q));return {r.x,r.y,r.z};}
// Colunas x, y, z de uma rotação.
Q fromBasis(V x,V y,V z) {
  const double m00=x.x,m10=x.y,m20=x.z,m01=y.x,m11=y.y,m21=y.z,m02=z.x,m12=z.y,m22=z.z;
  const double trace=m00+m11+m22;Q q;
  if(trace>0) {const double s=std::sqrt(trace+1)*2;q={(m21-m12)/s,(m02-m20)/s,(m10-m01)/s,.25*s};}
  else if(m00>m11&&m00>m22) {const double s=std::sqrt(1+m00-m11-m22)*2;q={.25*s,(m01+m10)/s,(m02+m20)/s,(m21-m12)/s};}
  else if(m11>m22) {const double s=std::sqrt(1+m11-m00-m22)*2;q={(m01+m10)/s,.25*s,(m12+m21)/s,(m02-m20)/s};}
  else {const double s=std::sqrt(1+m22-m00-m11)*2;q={(m02+m20)/s,(m12+m21)/s,.25*s,(m10-m01)/s};}
  return norm(q);
}

// ---- human_trait.gd ------------------------------------------------------
constexpr const char *BoneName[UnityHumanBoneCount]{
  "Hips","LeftUpperLeg","RightUpperLeg","LeftLowerLeg","RightLowerLeg","LeftFoot","RightFoot","Spine","Chest","Neck","Head",
  "LeftShoulder","RightShoulder","LeftUpperArm","RightUpperArm","LeftLowerArm","RightLowerArm","LeftHand","RightHand",
  "LeftToes","RightToes","LeftEye","RightEye","Jaw",
  "Left Thumb Proximal","Left Thumb Intermediate","Left Thumb Distal",
  "Left Index Proximal","Left Index Intermediate","Left Index Distal",
  "Left Middle Proximal","Left Middle Intermediate","Left Middle Distal",
  "Left Ring Proximal","Left Ring Intermediate","Left Ring Distal",
  "Left Little Proximal","Left Little Intermediate","Left Little Distal",
  "Right Thumb Proximal","Right Thumb Intermediate","Right Thumb Distal",
  "Right Index Proximal","Right Index Intermediate","Right Index Distal",
  "Right Middle Proximal","Right Middle Intermediate","Right Middle Distal",
  "Right Ring Proximal","Right Ring Intermediate","Right Ring Distal",
  "Right Little Proximal","Right Little Intermediate","Right Little Distal","UpperChest"};
constexpr const char *MuscleName[UnityHumanMuscleCount]{
  "Spine Front-Back","Spine Left-Right","Spine Twist Left-Right",
  "Chest Front-Back","Chest Left-Right","Chest Twist Left-Right",
  "UpperChest Front-Back","UpperChest Left-Right","UpperChest Twist Left-Right",
  "Neck Nod Down-Up","Neck Tilt Left-Right","Neck Turn Left-Right",
  "Head Nod Down-Up","Head Tilt Left-Right","Head Turn Left-Right",
  "Left Eye Down-Up","Left Eye In-Out","Right Eye Down-Up","Right Eye In-Out","Jaw Close","Jaw Left-Right",
  "Left Upper Leg Front-Back","Left Upper Leg In-Out","Left Upper Leg Twist In-Out",
  "Left Lower Leg Stretch","Left Lower Leg Twist In-Out","Left Foot Up-Down","Left Foot Twist In-Out","Left Toes Up-Down",
  "Right Upper Leg Front-Back","Right Upper Leg In-Out","Right Upper Leg Twist In-Out",
  "Right Lower Leg Stretch","Right Lower Leg Twist In-Out","Right Foot Up-Down","Right Foot Twist In-Out","Right Toes Up-Down",
  "Left Shoulder Down-Up","Left Shoulder Front-Back","Left Arm Down-Up","Left Arm Front-Back","Left Arm Twist In-Out",
  "Left Forearm Stretch","Left Forearm Twist In-Out","Left Hand Down-Up","Left Hand In-Out",
  "Right Shoulder Down-Up","Right Shoulder Front-Back","Right Arm Down-Up","Right Arm Front-Back","Right Arm Twist In-Out",
  "Right Forearm Stretch","Right Forearm Twist In-Out","Right Hand Down-Up","Right Hand In-Out",
  "LeftHand.Thumb.1 Stretched","LeftHand.Thumb.Spread","LeftHand.Thumb.2 Stretched","LeftHand.Thumb.3 Stretched",
  "LeftHand.Index.1 Stretched","LeftHand.Index.Spread","LeftHand.Index.2 Stretched","LeftHand.Index.3 Stretched",
  "LeftHand.Middle.1 Stretched","LeftHand.Middle.Spread","LeftHand.Middle.2 Stretched","LeftHand.Middle.3 Stretched",
  "LeftHand.Ring.1 Stretched","LeftHand.Ring.Spread","LeftHand.Ring.2 Stretched","LeftHand.Ring.3 Stretched",
  "LeftHand.Little.1 Stretched","LeftHand.Little.Spread","LeftHand.Little.2 Stretched","LeftHand.Little.3 Stretched",
  "RightHand.Thumb.1 Stretched","RightHand.Thumb.Spread","RightHand.Thumb.2 Stretched","RightHand.Thumb.3 Stretched",
  "RightHand.Index.1 Stretched","RightHand.Index.Spread","RightHand.Index.2 Stretched","RightHand.Index.3 Stretched",
  "RightHand.Middle.1 Stretched","RightHand.Middle.Spread","RightHand.Middle.2 Stretched","RightHand.Middle.3 Stretched",
  "RightHand.Ring.1 Stretched","RightHand.Ring.Spread","RightHand.Ring.2 Stretched","RightHand.Ring.3 Stretched",
  "RightHand.Little.1 Stretched","RightHand.Little.Spread","RightHand.Little.2 Stretched","RightHand.Little.3 Stretched"};
// Nomes alternativos dos músculos dos dedos (HumanTrait.MuscleName antigo).
constexpr std::pair<const char*,const char*> TraitMapping[]{
  {"Left Thumb 1 Stretched","LeftHand.Thumb.1 Stretched"},{"Left Thumb Spread","LeftHand.Thumb.Spread"},
  {"Left Thumb 2 Stretched","LeftHand.Thumb.2 Stretched"},{"Left Thumb 3 Stretched","LeftHand.Thumb.3 Stretched"},
  {"Left Index 1 Stretched","LeftHand.Index.1 Stretched"},{"Left Index Spread","LeftHand.Index.Spread"},
  {"Left Index 2 Stretched","LeftHand.Index.2 Stretched"},{"Left Index 3 Stretched","LeftHand.Index.3 Stretched"},
  {"Left Middle 1 Stretched","LeftHand.Middle.1 Stretched"},{"Left Middle Spread","LeftHand.Middle.Spread"},
  {"Left Middle 2 Stretched","LeftHand.Middle.2 Stretched"},{"Left Middle 3 Stretched","LeftHand.Middle.3 Stretched"},
  {"Left Ring 1 Stretched","LeftHand.Ring.1 Stretched"},{"Left Ring Spread","LeftHand.Ring.Spread"},
  {"Left Ring 2 Stretched","LeftHand.Ring.2 Stretched"},{"Left Ring 3 Stretched","LeftHand.Ring.3 Stretched"},
  {"Left Little 1 Stretched","LeftHand.Little.1 Stretched"},{"Left Little Spread","LeftHand.Little.Spread"},
  {"Left Little 2 Stretched","LeftHand.Little.2 Stretched"},{"Left Little 3 Stretched","LeftHand.Little.3 Stretched"},
  {"Right Thumb 1 Stretched","RightHand.Thumb.1 Stretched"},{"Right Thumb Spread","RightHand.Thumb.Spread"},
  {"Right Thumb 2 Stretched","RightHand.Thumb.2 Stretched"},{"Right Thumb 3 Stretched","RightHand.Thumb.3 Stretched"},
  {"Right Index 1 Stretched","RightHand.Index.1 Stretched"},{"Right Index Spread","RightHand.Index.Spread"},
  {"Right Index 2 Stretched","RightHand.Index.2 Stretched"},{"Right Index 3 Stretched","RightHand.Index.3 Stretched"},
  {"Right Middle 1 Stretched","RightHand.Middle.1 Stretched"},{"Right Middle Spread","RightHand.Middle.Spread"},
  {"Right Middle 2 Stretched","RightHand.Middle.2 Stretched"},{"Right Middle 3 Stretched","RightHand.Middle.3 Stretched"},
  {"Right Ring 1 Stretched","RightHand.Ring.1 Stretched"},{"Right Ring Spread","RightHand.Ring.Spread"},
  {"Right Ring 2 Stretched","RightHand.Ring.2 Stretched"},{"Right Ring 3 Stretched","RightHand.Ring.3 Stretched"},
  {"Right Little 1 Stretched","RightHand.Little.1 Stretched"},{"Right Little Spread","RightHand.Little.Spread"},
  {"Right Little 2 Stretched","RightHand.Little.2 Stretched"},{"Right Little 3 Stretched","RightHand.Little.3 Stretched"}};
constexpr i32 MuscleFromBone[UnityHumanBoneCount][3]{
  {-1,-1,-1},{23,22,21},{31,30,29},{25,-1,24},{33,-1,32},{-1,27,26},{-1,35,34},{2,1,0},{5,4,3},{11,10,9},{14,13,12},
  {-1,38,37},{-1,47,46},{41,40,39},{50,49,48},{43,-1,42},{52,-1,51},{-1,45,44},{-1,54,53},{-1,-1,28},{-1,-1,36},
  {-1,16,15},{-1,18,17},{-1,20,19},
  {-1,56,55},{-1,-1,57},{-1,-1,58},{-1,60,59},{-1,-1,61},{-1,-1,62},{-1,64,63},{-1,-1,65},{-1,-1,66},
  {-1,68,67},{-1,-1,69},{-1,-1,70},{-1,72,71},{-1,-1,73},{-1,-1,74},
  {-1,76,75},{-1,-1,77},{-1,-1,78},{-1,80,79},{-1,-1,81},{-1,-1,82},{-1,84,83},{-1,-1,85},{-1,-1,86},
  {-1,88,87},{-1,-1,89},{-1,-1,90},{-1,92,91},{-1,-1,93},{-1,-1,94},{8,7,6}};
constexpr double MuscleDefaultMax[UnityHumanMuscleCount]{
  40,40,40,40,40,40,20,20,20,40,40,40,40,40,40,15,20,15,20,10,10,
  50,60,60,80,90,50,30,50,50,60,60,80,90,50,30,50,
  30,15,100,100,90,80,90,80,40,30,15,100,100,90,80,90,80,40,
  20,25,35,35,50,20,45,45,50,7.5,45,45,50,7.5,45,45,50,20,45,45,
  20,25,35,35,50,20,45,45,50,7.5,45,45,50,7.5,45,45,50,20,45,45};
constexpr double MuscleDefaultMin[UnityHumanMuscleCount]{
  -40,-40,-40,-40,-40,-40,-20,-20,-20,-40,-40,-40,-40,-40,-40,-10,-20,-10,-20,-10,-10,
  -90,-60,-60,-80,-90,-50,-30,-50,-90,-60,-60,-80,-90,-50,-30,-50,
  -15,-15,-60,-100,-90,-80,-90,-80,-40,-15,-15,-60,-100,-90,-80,-90,-80,-40,
  -20,-25,-40,-40,-50,-20,-45,-45,-50,-7.5,-45,-45,-50,-7.5,-45,-45,-50,-20,-45,-45,
  -20,-25,-40,-40,-50,-20,-45,-45,-50,-7.5,-45,-45,-50,-7.5,-45,-45,-50,-20,-45,-45};
// Pré-rotações (x,y,z,w) do esqueleto humanoide de referência, mão direita.
constexpr double PreQ[UnityHumanBoneCount][4]{
  {0,0,0,1},{-0.62644,-0.34855,0.59144,0.36918},{-0.59144,-0.36918,0.62644,0.34855},
  {-0.69691,0.04422,-0.7145,0.04313},{-0.7145,0.04313,-0.69691,0.04422},{0.5,-0.5,-0.5,0.5},{0.5,-0.5,-0.5,0.5},
  {0.46815,-0.52994,0.46815,-0.52994},{0.52661,-0.47189,0.52661,-0.47189},{0.46642,-0.5316,0.46748,-0.5304},{-0.5,0.5,-0.5,0.5},
  {0.03047,-0.00261,-0.9959,-0.08517},{-0.00261,0.03047,0.08518,0.9959},
  {0.505665,-0.400775,0.749395,-0.148645},{-0.400775,0.505665,0.148645,-0.749395},
  {-0.998935,0.046125,0.001085,0.000045},{-0.04613,0.99894,0.00005,0.00108},
  {-0.02914,-0.029083,0.707276,-0.705735},{0.029083,0.02914,-0.705735,0.707276},
  {-0.500002,0.500002,-0.500002,0.500002},{-0.500002,0.500002,-0.500002,0.500002},
  {0.70711,0,-0.70711,0},{0.70711,0,-0.70711,0},{0,0,0,1},
  {-0.957335,0.251575,0.073965,0.121475},{-0.435979,0.550073,-0.413528,0.579935},{-0.435979,0.550073,-0.413528,0.579935},
  {-0.70292,0.26496,0.53029,-0.39305},{-0.67155,0.2898,0.5998,-0.32447},{-0.67155,0.2898,0.5998,-0.32447},
  {-0.66667,0.29261,0.5877,-0.3529},{-0.660575,0.278445,0.633985,-0.290115},{-0.660575,0.278445,0.633985,-0.290115},
  {-0.60736,0.34862,0.64221,-0.31169},{-0.629695,0.327165,0.621835,-0.331305},{-0.629695,0.327165,0.621835,-0.331305},
  {-0.584415,0.369825,0.660045,-0.293315},{-0.608895,0.338425,0.641505,-0.321215},{-0.608895,0.338425,0.641505,-0.321215},
  {0.251455,-0.957385,-0.121415,-0.073715},{0.550156,-0.435959,-0.579765,0.413695},{0.550156,-0.435959,-0.579765,0.413695},
  {0.264955,-0.702925,0.393045,-0.530295},{0.289805,-0.671545,0.324465,-0.599805},{0.289805,-0.671545,0.324465,-0.599805},
  {0.292615,-0.666665,0.352895,-0.587705},{0.278455,-0.660575,0.290125,-0.633985},{0.278455,-0.660575,0.290125,-0.633985},
  {0.34862,-0.60736,0.31169,-0.64221},{0.327165,-0.629695,0.331295,-0.621845},{0.327165,-0.629695,0.331295,-0.621845},
  {0.36982,-0.58442,0.29332,-0.66004},{0.33843,-0.6089,0.32123,-0.6415},{0.33843,-0.6089,0.32123,-0.6415},
  {0.56563,-0.42434,0.56563,-0.42434}};
// Pós-rotações inversas (x,y,z,w) do mesmo esqueleto.
constexpr double PostQInverse[UnityHumanBoneCount][4]{
  {0,0,0,1},{0.48977,-0.50952,0.51876,0.48105},{0.51876,-0.48105,0.48977,0.50952},
  {-0.51894,0.48097,0.50616,0.49312},{-0.50616,0.49312,0.51894,0.48097},{-0.707107,0,-0.707107,0},{-0.707107,0,-0.707107,0},
  {-0.46815,0.52994,-0.46815,-0.52994},{-0.52661,0.47189,-0.52661,-0.47189},{-0.46642,0.5316,-0.46748,-0.5304},{0.5,-0.5,0.5,0.5},
  {-0.523995,0.469295,-0.557075,-0.441435},{0.46929,-0.524,-0.44143,-0.55708},
  {0.513635,-0.486185,-0.509345,-0.490275},{0.486185,-0.513635,0.490275,0.509345},
  {0.519596,-0.479517,-0.520728,-0.478471},{0.479517,-0.519596,0.478471,0.520728},
  {0.520725,-0.478465,-0.479515,-0.519595},{0.478465,-0.520725,0.519595,0.479515},
  {-0.500002,0.500002,0.500002,0.500002},{-0.500002,0.500002,0.500002,0.500002},
  {-0.500002,0.500002,0.500002,0.500002},{-0.500002,0.500002,0.500002,0.500002},{0,0.707107,0.707107,0},
  {0.56005,-0.437881,0.528429,0.464077},{0.541247,-0.458295,0.513379,0.483179},{0.541247,-0.458295,0.513379,0.483179},
  {0.53845,-0.45868,-0.46056,-0.53625},{0.53604,-0.46316,-0.47877,-0.51857},{0.53604,-0.46316,-0.47877,-0.51857},
  {0.52555,-0.47434,-0.492,-0.50669},{0.536385,-0.463085,-0.514795,-0.482515},{0.536385,-0.463085,-0.514795,-0.482515},
  {0.50517,-0.49482,-0.50264,-0.49731},{0.494155,-0.505555,-0.487985,-0.511945},{0.494155,-0.505555,-0.487985,-0.511945},
  {0.502345,-0.497645,-0.501995,-0.497995},{0.47756,-0.52241,-0.50314,-0.49585},{0.47756,-0.52241,-0.50314,-0.49585},
  {0.437905,-0.559994,-0.463881,-0.528644},{0.458337,-0.5412,-0.483,-0.513558},{0.458337,-0.5412,-0.483,-0.513558},
  {0.45868,-0.53845,0.53625,0.46056},{0.463165,-0.536035,0.518565,0.478775},{0.463165,-0.536035,0.518565,0.478775},
  {0.47434,-0.52555,0.50669,0.492},{0.4631,-0.53638,0.48252,0.5148},{0.4631,-0.53638,0.48252,0.5148},
  {0.49482,-0.50517,0.49731,0.50264},{0.505555,-0.494155,0.511935,0.487995},{0.505555,-0.494155,0.511935,0.487995},
  {0.49764,-0.50235,0.498,0.50199},{0.52241,-0.47756,0.49586,0.50313},{0.52241,-0.47756,0.49586,0.50313},
  {-0.56563,0.42434,-0.56563,-0.42434}};
constexpr double Signs[UnityHumanBoneCount][3]{
  {1,1,1},{1,1,1},{-1,-1,1},{1,-1,-1},{-1,1,-1},{1,1,1},{-1,-1,1},{1,1,1},{1,1,1},{1,1,1},{1,1,1},
  {1,1,-1},{-1,1,1},{1,1,-1},{-1,1,1},{1,1,-1},{-1,1,1},{1,1,-1},{-1,1,1},{1,1,1},{-1,-1,1},
  {-1,1,-1},{1,-1,-1},{1,1,1},
  {1,-1,1},{1,-1,1},{1,-1,1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},
  {1,1,-1},{1,1,-1},{1,1,-1},{1,1,-1},{1,1,-1},{1,1,-1},
  {-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{1,-1,1},{1,-1,1},{1,-1,1},{1,-1,1},{1,-1,1},{1,-1,1},
  {-1,1,1},{-1,1,1},{-1,1,1},{-1,1,1},{-1,1,1},{-1,1,1},{1,1,1}};
// Corpo (centro de massa): ordem HumanTrait.GetBoneIndexToMono, pais, posições
// locais do Xbot em escala humana, comprimentos e massas.
constexpr u32 BodyCount=25;
constexpr u32 BodyBone[BodyCount]{0,1,2,3,4,5,6,7,8,54,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23};
constexpr u32 BodyParent[BodyCount]{0,0,0,1,2,3,4,0,7,8,9,10,9,9,12,13,14,15,16,17,5,6,11,11,11};
constexpr double BodyPosition[BodyCount][3]{
  {0,1,0},{0.078713,-0.064749,-0.01534},{-0.078713,-0.064749,-0.01534},{0,0.42551,0.003327},{0,0.425511,0.003317},
  {0,0.426025,0.029196},{0,0.426025,0.029188},{0,0.097642,0.001261},{0,0.096701,-0.009598},{0,0.087269,-0.013171},
  {0,0.159882,-0.02413},{0,0.092236,0.016159},{0.043831,0.104972,-0.025203},{-0.043826,0.104974,-0.025203},
  {-0.021406,0.101581,-0.005031},{0.021406,0.101586,-0.005033},{0,0.267001,0},{0,0.267001,0},{0,0.271675,0},
  {0,0.271675,-0.000001},{0,0.102715,-0.083708},{0,0.102715,-0.083708},{0.02,0.04,0},{-0.02,0.04,0},{0,0.02,0}};
constexpr double BodyLength[BodyCount]{
  0.10307032899633,0.42552330971818,0.4255239384901,0.42702378815645,0.4270232737067,0.13250420705574,0.13250419276546,
  0.09717579233836,0.0882577559016,0.1665140084667,0.09364062941212,0.1726370036846,0.1039340043854,0.10393849867558,
  0.26700124954128,0.26700124954128,0.27167462539484,0.27167462539484,0.0901436357993,0.09013411133379,
  0.0889776497306,0.0889776497306,0.02,0.02,0.01};
constexpr double BodyMass[BodyCount]{
  0.145455,0.121212,0.121212,0.0484849,0.0484849,0.00969697,0.00969697,0.030303,0.145455,0.145455,0.0121212,0.0484849,
  0.00606061,0.00606061,0.0242424,0.0242424,0.0181818,0.0181818,0.00606061,0.00606061,0.00242424,0.00242424,0,0,0};
// Distribuição da torção (extraAffectedByBones): o osso recebe metade da
// torção do pai no referencial pós do pai, e metade da própria.
constexpr std::pair<u32,u32> TwistFrom[]{{3,1},{5,3},{4,2},{6,4},{15,13},{17,15},{16,14},{18,16}};

// ---- esqueleto de referência (perfil humanoide da Godot) -----------------
// Pai de cada osso humano no esqueleto de referência (-1 no quadril).
constexpr i32 ReferenceParent[UnityHumanBoneCount]{
  -1,0,0,1,2,3,4,0,7,54,9,54,54,11,12,13,14,15,16,5,6,10,10,10,
  17,24,25,17,27,28,17,30,31,17,33,34,17,36,37,
  18,39,40,18,42,43,18,45,46,18,48,49,18,51,52,8};
// Rotação de descanso local de cada osso (base por linhas da Transform3D da Godot).
constexpr double ReferenceBasis[UnityHumanBoneCount][9]{
  {1,0,0,0,1,0,0,0,1},{-1,0,0,0,-1,0,0,0,1},{-1,0,0,0,-1,0,0,0,1},{-1,0,0,0,1,0,0,0,-1},{-1,0,0,0,1,0,0,0,-1},
  {-1,0,0,0,0,-1,0,-1,0},{-1,0,0,0,0,-1,0,-1,0},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},
  {0,1,0,0,0,1,1,0,0},{0,-1,0,0,0,1,-1,0,0},{-1,0,0,0,1,0,0,0,-1},{-1,0,0,0,1,0,0,0,-1},
  {0,0,-1,0,1,0,1,0,0},{0,0,1,0,1,0,-1,0,0},{0,0,1,0,1,0,-1,0,0},{0,0,-1,0,1,0,1,0,0},
  {-1,0,0,0,1,0,0,0,-1},{-1,0,0,0,1,0,0,0,-1},{1,0,0,0,0,-1,0,1,0},{1,0,0,0,0,-1,0,1,0},{-1,0,0,0,0,1,0,1,0},
  {0,-0.577,0.816,0,0.816,0.577,-1,0,0},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},
  {1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},
  {1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},
  {0,0.577,-0.816,0,0.816,0.577,1,0,0},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},
  {1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},
  {1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},{1,0,0,0,1,0,0,0,1},
  {1,0,0,0,1,0,0,0,1}};
Q referenceLocal(u32 bone) {
  const auto &m=ReferenceBasis[bone];
  return fromBasis({m[0],m[3],m[6]},{m[1],m[4],m[7]},{m[2],m[5],m[8]});
}
// Ordem em que pais vêm antes dos filhos no esqueleto de referência.
constexpr u32 ReferenceOrder[UnityHumanBoneCount]{
  0,1,2,3,4,5,6,19,20,7,8,54,9,10,21,22,23,11,12,13,14,15,16,17,18,
  24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53};

Q fromArray(const double q[4]) {return norm(Q{q[0],q[1],q[2],q[3]});}
Q swingTwist(double x,double y,double z) {
  const double yz=std::sqrt(y*y+z*z);
  const double sinc=std::abs(yz)<1e-8?.5:std::sin(yz/2)/yz;
  const double swingW=std::cos(yz/2),twistW=std::cos(x/2),twistX=std::sin(x/2);
  return {swingW*twistX,(z*twistX+y*twistW)*sinc,(z*twistW-y*twistX)*sinc,swingW*twistW};
}
// calculate_humanoid_rotation: músculo normalizado -> ângulo no intervalo padrão
// com o sinal do eixo, balanço/torção entre a pré e a pós-rotação do osso.
Q muscleRotation(u32 bone,double m0,double m1,double m2,bool fromPostQ=false) {
  double m[3]{m0,m1,m2};
  for(u32 i=0;i<3;++i) {
    const i32 muscle=MuscleFromBone[bone][i];
    if(muscle<0) {m[i]=0;continue;}
    const double degrees=m[i]>=0?MuscleDefaultMax[muscle]:-MuscleDefaultMin[muscle];
    m[i]*=degrees*3.14159265358979323846/180*Signs[bone][i];
  }
  const Q pre=fromPostQ?conj(fromArray(PostQInverse[bone])):fromArray(PreQ[bone]);
  return norm(mul(mul(pre,swingTwist(m[0],-m[1],-m[2])),fromArray(PostQInverse[bone])));
}

// Pose de referência completa de um quadro.
void referencePose(const std::array<std::array<double,3>,UnityHumanBoneCount> &muscles,V rootT,Q rootQ,
                   std::array<Q,UnityHumanBoneCount> &local,std::array<Q,UnityHumanBoneCount> &global,V &hips) {
  // Locais com a distribuição de torção (o que a Unity escreve nos ossos).
  local[0]={};
  for(u32 bone=1;bone<UnityHumanBoneCount;++bone) {
    auto own=muscles[bone];Q pre;
    for(const auto &[target,from]:TwistFrom)if(target==bone) {
      own[0]*=.5;pre=muscleRotation(from,muscles[from][0]*.5,0,0,true);
    }
    local[bone]=norm(mul(pre,muscleRotation(bone,own[0],own[1],own[2])));
  }
  // Corpo para o centro de massa: rotações sem a distribuição, como o Unidot.
  std::array<V,BodyCount> position{};std::array<Q,BodyCount> rotation{};
  position[0]={BodyPosition[0][0],BodyPosition[0][1],BodyPosition[0][2]};rotation[0]={};
  for(u32 body=1;body<BodyCount;++body) {
    const u32 bone=BodyBone[body],parent=BodyParent[body];
    const auto &m=muscles[bone];
    const V offset{BodyPosition[body][0],BodyPosition[body][1],BodyPosition[body][2]};
    position[body]=position[parent]+rotate(rotation[parent],offset);
    rotation[body]=norm(mul(rotation[parent],muscleRotation(bone,m[0],m[1],m[2])));
  }
  V x=(position[14]+position[1])-(position[15]+position[2]);
  V y=(position[14]+position[15])-(position[1]+position[2]);
  x=unit(x);y=unit(y);const V z=unit(cross(x,y));x=cross(y,z);
  const Q massQ=fromBasis(x,y,z);
  double sum=1e-6;V massT{};
  for(u32 body=0;body<BodyCount;++body)if(BodyMass[body]>0) {
    const V center=position[body]+rotate(rotation[body],V{0,BodyLength[body]/2,0});
    massT=massT+center*BodyMass[body];sum+=BodyMass[body];
  }
  massT=massT*(1/sum);
  const Q delta=norm(mul(rootQ,conj(massQ)));
  hips=rootT-rotate(delta,massT-position[0]);
  global[0]=delta;
  for(const u32 bone:ReferenceOrder)if(bone)global[bone]=norm(mul(global[static_cast<u32>(ReferenceParent[bone])],local[bone]));
}

// ---- matrizes do alvo (coluna-maior) -------------------------------------
using M=std::array<double,16>;
M compose(V t,Q q,V s) {
  const double xx=q.x*q.x,yy=q.y*q.y,zz=q.z*q.z,xy=q.x*q.y,xz=q.x*q.z,yz=q.y*q.z,wx=q.w*q.x,wy=q.w*q.y,wz=q.w*q.z;
  return {(1-2*(yy+zz))*s.x,2*(xy+wz)*s.x,2*(xz-wy)*s.x,0,
          2*(xy-wz)*s.y,(1-2*(xx+zz))*s.y,2*(yz+wx)*s.y,0,
          2*(xz+wy)*s.z,2*(yz-wx)*s.z,(1-2*(xx+yy))*s.z,0,
          t.x,t.y,t.z,1};
}
M multiply(const M &a,const M &b) {
  M out{};
  for(u32 c=0;c<4;++c)for(u32 r=0;r<4;++r) {double v=0;for(u32 k=0;k<4;++k)v+=a[k*4+r]*b[c*4+k];out[c*4+r]=v;}
  return out;
}
V point(const M &m,V p) {return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};}
bool inverse(const M &m,M &out) {
  const double a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],i=m[10];
  const double A=e*i-f*h,B=-(d*i-f*g),C=d*h-e*g,det=a*A+b*B+c*C;
  if(!std::isfinite(det)||std::abs(det)<1e-12)return false;
  const double inv=1/det;
  const double r00=A*inv,r01=-(b*i-c*h)*inv,r02=(b*f-c*e)*inv;
  const double r10=B*inv,r11=(a*i-c*g)*inv,r12=-(a*f-c*d)*inv;
  const double r20=C*inv,r21=-(a*h-b*g)*inv,r22=(a*e-b*d)*inv;
  const V t{m[12],m[13],m[14]};
  out={r00,r10,r20,0,r01,r11,r21,0,r02,r12,r22,0,
       -(r00*t.x+r01*t.y+r02*t.z),-(r10*t.x+r11*t.y+r12*t.z),-(r20*t.x+r21*t.y+r22*t.z),1};
  return true;
}
} // namespace

std::string_view unityHumanBoneName(u32 bone) {return bone<UnityHumanBoneCount?BoneName[bone]:std::string_view{};}
i32 unityHumanBoneIndex(std::string_view humanName) {
  for(u32 bone=0;bone<UnityHumanBoneCount;++bone)if(humanName==BoneName[bone])return static_cast<i32>(bone);
  return -1;
}
UnityHumanoidAttribute unityHumanoidAttribute(std::string_view attribute) {
  UnityHumanoidAttribute out;
  if(attribute.size()==7&&(attribute.starts_with("RootT.")||attribute.starts_with("RootQ."))) {
    const char axis=attribute[6];const bool rotation=attribute[4]=='Q';
    const i32 index=axis=='x'?0:axis=='y'?1:axis=='z'?2:axis=='w'&&rotation?3:-1;
    if(index>=0) {out.kind=rotation?UnityHumanoidAttribute::Kind::RootQ:UnityHumanoidAttribute::Kind::RootT;out.axis=static_cast<u32>(index);}
    return out;
  }
  std::string_view name=attribute;
  for(const auto &[alias,canonical]:TraitMapping)if(attribute==alias)name=canonical;
  for(u32 muscle=0;muscle<UnityHumanMuscleCount;++muscle)if(name==MuscleName[muscle]) {
    for(u32 bone=0;bone<UnityHumanBoneCount;++bone)for(u32 axis=0;axis<3;++axis)if(MuscleFromBone[bone][axis]==static_cast<i32>(muscle)) {
      out.kind=UnityHumanoidAttribute::Kind::Muscle;out.bone=bone;out.axis=axis;return out;
    }
  }
  // Alvos de IK (pés, mãos), MotionT/Q e graus de translação: o passo de IK
  // da Unity é opcional por estado; a pose sai só dos músculos e do corpo.
  for(const char *prefix:{"LeftFoot","RightFoot","LeftHand","RightHand","Motion"})
    if(attribute.starts_with(prefix)&&(attribute.find("T.")!=std::string_view::npos||attribute.find("Q.")!=std::string_view::npos))
      {out.kind=UnityHumanoidAttribute::Kind::Ignored;return out;}
  if(attribute.find("TDOF")!=std::string_view::npos)out.kind=UnityHumanoidAttribute::Kind::Ignored;
  return out;
}

void unityHumanoidReferencePose(const std::array<std::array<float,3>,UnityHumanBoneCount> &muscles,
    const float rootT[3],const float rootQ[4],UnityHumanoidPose &out) {
  std::array<std::array<double,3>,UnityHumanBoneCount> values{};
  for(u32 bone=0;bone<UnityHumanBoneCount;++bone)for(u32 axis=0;axis<3;++axis)values[bone][axis]=muscles[bone][axis];
  std::array<Q,UnityHumanBoneCount> local{},global{};V hips;
  // Unity (mão esquerda) para a referência (mão direita): X espelhado.
  referencePose(values,{-rootT[0],rootT[1],rootT[2]},norm(Q{rootQ[0],-rootQ[1],-rootQ[2],rootQ[3]}),local,global,hips);
  for(u32 bone=0;bone<UnityHumanBoneCount;++bone) {
    out.local[bone]={local[bone].x,local[bone].y,local[bone].z,local[bone].w};
    out.global[bone]={global[bone].x,global[bone].y,global[bone].z,global[bone].w};
  }
  out.hips={hips.x,hips.y,hips.z};
}

UnityHumanoidTarget unityHumanoidReferenceSkeleton() {
  // Origens locais do perfil humanoide da Godot (metros, eixo +Y ao longo do osso).
  static constexpr double Origin[UnityHumanBoneCount][3]{
    {0,1,0},{0.1,0,0},{-0.1,0,0},{0,0.375,0},{0,0.375,0},{0,0.375,0},{0,0.375,0},{0,0.1,0},{0,0.1,0},{0,0.1,0},{0,0.1,0},
    {0.05,0.1,0},{-0.05,0.1,0},{0,0.05,0},{0,0.05,0},{0,0.25,0},{0,0.25,0},{0,0.25,0},{0,0.25,0},{0,0.15,0},{0,0.15,0},
    {0.05,0.15,0},{-0.05,0.15,0},{0,0.05,0.05},
    {-0.025,0.025,0},{0,0.043,0},{0,0.043,0},{-0.025,0.075,0},{0,0.05,0},{0,0.025,0},{0,0.075,0},{0,0.075,0},{0,0.025,0},
    {0.025,0.075,0},{0,0.05,0},{0,0.025,0},{0.05,0.05,0},{0,0.05,0},{0,0.025,0},
    {0.025,0.025,0},{0,0.043,0},{0,0.043,0},{0.025,0.075,0},{0,0.05,0},{0,0.025,0},{0,0.075,0},{0,0.075,0},{0,0.025,0},
    {-0.025,0.075,0},{0,0.05,0},{0,0.025,0},{-0.05,0.05,0},{0,0.05,0},{0,0.025,0},{0,0.1,0}};
  UnityHumanoidTarget target;
  target.nodes.push_back({});
  std::array<i32,UnityHumanBoneCount> index{};index.fill(-1);
  for(const u32 bone:ReferenceOrder) {
    UnityHumanoidTarget::Node node;node.human=static_cast<i32>(bone);
    node.parent=bone?index[static_cast<u32>(ReferenceParent[bone])]:0;
    node.path=std::string(BoneName[bone]);
    const Q q=referenceLocal(bone);
    node.rotation[0]=static_cast<float>(q.x);node.rotation[1]=static_cast<float>(q.y);node.rotation[2]=static_cast<float>(q.z);node.rotation[3]=static_cast<float>(q.w);
    for(u32 c=0;c<3;++c)node.translation[c]=static_cast<float>(Origin[bone][c]);
    index[bone]=static_cast<i32>(target.nodes.size());target.nodes.push_back(std::move(node));
  }
  return target;
}

bool appendUnityHumanoidTracks(const UnityHumanoidCurves &curves,const UnityHumanoidTarget &target,
    float duration,u32 sampleRate,AnimationClipAsset &clip,UnityHumanoidReport &report,std::string &error) {
  const auto fail=[&](std::string message){error=std::move(message);return false;};
  report={};
  if(!curves.muscleCurves&&!curves.hasRootQ&&!curves.hasRootT)return fail("Clipe Humanoid sem curvas de músculo nem do corpo.");
  if(target.nodes.empty()||target.nodes[0].parent!=-1)return fail("Personagem alvo sem hierarquia.");
  if(!std::isfinite(duration)||duration<=0||sampleRate<1||sampleRate>240)return fail("Duração ou taxa do clipe Humanoid inválida.");
  std::array<i32,UnityHumanBoneCount> node{};node.fill(-1);
  for(usize i=0;i<target.nodes.size();++i) {
    const auto &n=target.nodes[i];
    if(i&&(n.parent<0||static_cast<usize>(n.parent)>=i))return fail("Hierarquia do alvo fora de ordem.");
    if(n.human>=0) {
      if(n.human>=static_cast<i32>(UnityHumanBoneCount)||node[static_cast<u32>(n.human)]>=0)return fail("Osso humano repetido ou inválido no alvo.");
      node[static_cast<u32>(n.human)]=static_cast<i32>(i);
    }
  }
  if(node[0]<0)return fail("O personagem não tem o osso Hips mapeado (Avatar Humanoid).");
  // Descanso real (bind) para a reprodução e pose em T do Avatar como
  // referência do redirecionamento, ambos no espaço do dono.
  const usize count=target.nodes.size();
  std::vector<Q> restLocal(count),restGlobal(count);std::vector<M> restMatrix(count);
  std::vector<V> translation(count),scale(count);
  u32 tposeNodes=0;
  for(usize i=0;i<count;++i) {
    const auto &n=target.nodes[i];
    translation[i]={n.translation[0],n.translation[1],n.translation[2]};scale[i]={n.scale[0],n.scale[1],n.scale[2]};
    restLocal[i]=norm(Q{n.rotation[0],n.rotation[1],n.rotation[2],n.rotation[3]});
    if(!i) {restGlobal[i]={};restMatrix[i]=compose({},{},{1,1,1});continue;}
    const auto p=static_cast<usize>(n.parent);
    const Q local=n.hasTpose?norm(Q{n.tposeRotation[0],n.tposeRotation[1],n.tposeRotation[2],n.tposeRotation[3]}):restLocal[i];
    const V offset=n.hasTpose?V{n.tposeTranslation[0],n.tposeTranslation[1],n.tposeTranslation[2]}:translation[i];
    tposeNodes+=n.hasTpose;
    restGlobal[i]=norm(mul(restGlobal[p],local));
    restMatrix[i]=multiply(restMatrix[p],compose(offset,local,scale[i]));
  }
  if(tposeNodes)report.notes.push_back("Pose em T do Avatar da Unity em "+std::to_string(tposeNodes)+" ossos como referência.");
  const auto restPosition=[&](u32 human,V &out){const i32 i=node[human];if(i<0)return false;const auto &m=restMatrix[static_cast<usize>(i)];out={m[12],m[13],m[14]};return true;};
  // Alinhamento referência -> alvo pelo corpo (esquerda e cima), o mesmo
  // referencial do centro de massa. Sem braços e pernas, sem giro.
  Q align;V up{0,1,0};
  V leftArm,rightArm,leftLeg,rightLeg;
  if(restPosition(13,leftArm)&&restPosition(14,rightArm)&&restPosition(1,leftLeg)&&restPosition(2,rightLeg)) {
    V x=unit((leftArm+leftLeg)-(rightArm+rightLeg)),y=unit((leftArm+rightArm)-(leftLeg+rightLeg));
    const V z=unit(cross(x,y));x=cross(y,z);align=fromBasis(x,y,z);up=y;
  } else report.notes.push_back("Braços ou pernas sem osso mapeado: orientação do corpo tomada como a da referência.");
  V hipsRest;restPosition(0,hipsRest);
  const double humanScale=dot(hipsRest,up);
  if(!std::isfinite(humanScale)||humanScale<=1e-6)return fail("Altura do quadril do personagem inválida para a escala humana.");
  // Descanso global da referência.
  std::array<Q,UnityHumanBoneCount> referenceRest{};
  for(const u32 bone:ReferenceOrder) referenceRest[bone]=bone?norm(mul(referenceRest[static_cast<u32>(ReferenceParent[bone])],referenceLocal(bone))):Q{};
  // Trilhas genéricas de Transform dos ossos humanos saem: a Unity as ignora.
  std::set<std::string> humanPaths;for(const auto &n:target.nodes)if(n.human>=0)humanPaths.insert(n.path);
  std::set<u64> humanBindings;for(const auto &b:clip.bindings)if(humanPaths.count(b.path))humanBindings.insert(b.id);
  const auto before=clip.tracks.size();
  std::erase_if(clip.tracks,[&](const AnimationClipTrack &t){return humanBindings.count(t.binding)&&t.path!=AnimationPath::Weights;});
  if(clip.tracks.size()!=before)report.notes.push_back("Curvas de Transform de ossos humanos ignoradas, como na Unity.");
  const auto bindingFor=[&](const std::string &path)->u64 {
    for(const auto &b:clip.bindings)if(b.path==path)return b.id;
    AnimationClipBinding b;b.id=clip.nextId++;b.path=path;const auto slash=path.rfind('/');b.name=path.substr(slash==std::string::npos?0:slash+1);
    clip.bindings.push_back(b);return b.id;
  };
  // Amostras: taxa do clipe, de 0 até a duração exata.
  const u32 frames=std::max<u32>(1,static_cast<u32>(std::lround(double(duration)*sampleRate)));
  std::vector<double> times;times.reserve(frames+1);
  for(u32 f=0;f<=frames;++f)times.push_back(std::min<double>(duration,double(f)/sampleRate));
  if(times.back()<duration)times.push_back(duration);
  const usize keys=times.size();
  std::vector<usize> outputs;for(usize i=1;i<count;++i)if(target.nodes[i].human>=0)outputs.push_back(i);
  if((outputs.size()*4+3)*keys>AnimationClipAsset::MaximumKeys)return fail("Clipe Humanoid excede o orçamento de chaves.");
  std::vector<std::vector<Q>> rotations(count);std::vector<V> hipsTrack;hipsTrack.reserve(keys);
  const usize hipsNode=static_cast<usize>(node[0]);
  const auto sample=[&](const AnimationCurve &curve,double time,double fallback) {
    AnimationCurveSample s;return !curve.keys.empty()&&sampleValidatedAnimationCurve(curve,time,s)?s.value:fallback;
  };
  std::vector<Q> poseLocal(count),poseGlobal(count);std::vector<M> poseMatrix(count);
  for(const double time:times) {
    std::array<std::array<double,3>,UnityHumanBoneCount> muscles{};
    for(u32 bone=0;bone<UnityHumanBoneCount;++bone)for(u32 axis=0;axis<3;++axis)
      if(curves.present[bone][axis])muscles[bone][axis]=sample(curves.muscles[bone][axis],time,0);
    V rootT{0,1,0};Q rootQ;
    if(curves.hasRootT) rootT={-sample(curves.rootT[0],time,0),sample(curves.rootT[1],time,1),sample(curves.rootT[2],time,0)};
    if(curves.hasRootQ) rootQ=norm(Q{sample(curves.rootQ[0],time,0),-sample(curves.rootQ[1],time,0),-sample(curves.rootQ[2],time,0),sample(curves.rootQ[3],time,1)});
    std::array<Q,UnityHumanBoneCount> local{},global{};V hips;
    referencePose(muscles,rootT,rootQ,local,global,hips);
    // Alvo: osso humano recebe o mesmo giro de mundo que o da referência fez
    // desde o descanso; os demais ficam no descanso local.
    poseGlobal[0]={};poseMatrix[0]=restMatrix[0];
    const V hipsTarget=rotate(align,hips*humanScale);
    for(usize i=1;i<count;++i) {
      const auto &n=target.nodes[i];const auto p=static_cast<usize>(n.parent);
      if(n.human>=0) {
        const u32 bone=static_cast<u32>(n.human);
        const Q delta=norm(mul(mul(align,mul(global[bone],conj(referenceRest[bone]))),conj(align)));
        poseGlobal[i]=norm(mul(delta,restGlobal[i]));poseLocal[i]=norm(mul(conj(poseGlobal[p]),poseGlobal[i]));
      } else {poseLocal[i]=restLocal[i];poseGlobal[i]=norm(mul(poseGlobal[p],poseLocal[i]));}
      V t=translation[i];
      if(i==hipsNode) {M parentInverse;if(!inverse(poseMatrix[p],parentInverse))return fail("Pai do quadril sem inversa.");t=point(parentInverse,hipsTarget);hipsTrack.push_back(t);}
      poseMatrix[i]=multiply(poseMatrix[p],compose(t,poseLocal[i],scale[i]));
    }
    for(const auto i:outputs) {
      Q q=poseLocal[i];
      if(!rotations[i].empty()) {const Q &prev=rotations[i].back();if(prev.x*q.x+prev.y*q.y+prev.z*q.z+prev.w*q.w<0)q={-q.x,-q.y,-q.z,-q.w};}
      rotations[i].push_back(q);
    }
  }
  const auto curve=[&](const std::vector<double> &values) {
    AnimationCurve out;out.keys.reserve(values.size());
    for(usize k=0;k<values.size();++k) {
      AnimationCurveKey key;key.id=clip.nextId++;key.time=static_cast<float>(times[k]);key.value=static_cast<float>(values[k]);
      key.incoming=key.outgoing=AnimationTangentMode::Linear;out.keys.push_back(key);
    }
    return out;
  };
  for(const auto i:outputs) {
    AnimationClipTrack track;track.id=clip.nextId++;track.binding=bindingFor(target.nodes[i].path);track.path=AnimationPath::Rotation;
    track.rotationMode=AnimationRotationMode::Quaternion;
    for(u32 c=0;c<4;++c) {
      std::vector<double> values;values.reserve(keys);
      for(const auto &q:rotations[i])values.push_back(c==0?q.x:c==1?q.y:c==2?q.z:q.w);
      track.curves.push_back(curve(values));
    }
    clip.tracks.push_back(std::move(track));
  }
  {
    AnimationClipTrack track;track.id=clip.nextId++;track.binding=bindingFor(target.nodes[hipsNode].path);track.path=AnimationPath::Translation;
    for(u32 c=0;c<3;++c) {
      std::vector<double> values;values.reserve(keys);
      for(const auto &t:hipsTrack)values.push_back(c==0?t.x:c==1?t.y:t.z);
      track.curves.push_back(curve(values));
    }
    clip.tracks.push_back(std::move(track));
  }
  clip.duration=std::max(clip.duration,duration);
  report.bones=static_cast<u32>(outputs.size());report.samples=static_cast<u32>(keys);
  report.notes.push_back("Humanoid: "+std::to_string(report.bones)+" ossos do personagem, "+std::to_string(keys)+
      " amostras a "+std::to_string(sampleRate)+" Hz; escala humana "+std::to_string(humanScale).substr(0,5)+".");
  return true;
}
} // namespace ae::resources
