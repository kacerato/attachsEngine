#pragma once
// Ossos humanoides pelo nome, para montar interacoes num personagem importado.
// Referencia: Unity 6000.0, Avatar > Configure > Mapping > Automap
// (https://docs.unity3d.com/6000.0/Documentation/Manual/ConfiguringtheAvatar.html):
// o mapeamento automatico procura os ossos pelo nome e pela hierarquia. Aqui so
// os ossos que a interacao usa: maos, antebracos, bracos, quadril, peito e pes.
// Aceita nomes em ingles (Kyle, Mixamo, Blender .L/.R) e em portugues (os
// modelos da Astra). Sem palpite de eixo: a pose vem da cena.
#include "editor/editor_document.h"
#include <array>
#include <cctype>
#include <string>
#include <vector>

namespace ae::editor {

struct HumanoidBones final {
  // [0] esquerda, [1] direita.
  EditorEntityId hand[2]{},forearm[2]{},upperArm[2]{},foot[2]{};
  EditorEntityId hips=0,chest=0;
  bool arms() const {return hand[0]&&hand[1]&&forearm[0]&&forearm[1]&&upperArm[0]&&upperArm[1];}
};

namespace humanoid_detail {
// Minusculas ASCII, acentos do portugues dobrados e palavras separadas por
// pontuacao e por troca de caixa (LeftHand -> left hand).
inline std::vector<std::string> words(const std::string &name) {
  std::vector<std::string> out;std::string word;
  const auto flush=[&]{if(!word.empty())out.push_back(word);word.clear();};
  char previous=0;
  for(usize i=0;i<name.size();++i) {
    unsigned char c=static_cast<unsigned char>(name[i]);
    if(c==0xC3&&i+1<name.size()) {
      const unsigned char n=static_cast<unsigned char>(name[++i]);
      const char folded=(n>=0xA0&&n<=0xA5)||(n>=0x80&&n<=0x85)?'a':(n>=0xA8&&n<=0xAB)||(n>=0x88&&n<=0x8B)?'e':
          (n>=0xAC&&n<=0xAF)||(n>=0x8C&&n<=0x8F)?'i':(n>=0xB2&&n<=0xB6)||(n>=0x92&&n<=0x96)?'o':
          (n>=0xB9&&n<=0xBC)||(n>=0x99&&n<=0x9C)?'u':(n==0xA7||n==0x87)?'c':0;
      if(folded){word.push_back(folded);previous='a';}else{flush();previous=0;}
      continue;
    }
    if(!std::isalnum(c)){flush();previous=0;continue;}
    if(std::isupper(c)&&previous&&(std::islower(static_cast<unsigned char>(previous))||std::isdigit(static_cast<unsigned char>(previous))))flush();
    word.push_back(static_cast<char>(std::tolower(c)));previous=static_cast<char>(c);
  }
  flush();return out;
}
inline bool has(const std::vector<std::string> &w,const char *word) {for(const auto &x:w)if(x==word)return true;return false;}
inline bool contains(const std::vector<std::string> &w,const char *part) {for(const auto &x:w)if(x.find(part)!=std::string::npos)return true;return false;}
// 0 esquerda, 1 direita, -1 sem lado.
inline int side(const std::vector<std::string> &w) {
  if(contains(w,"left")||has(w,"l")||has(w,"esq")||has(w,"esquerdo")||has(w,"esquerda"))return 0;
  if(contains(w,"right")||has(w,"r")||has(w,"dir")||has(w,"direito")||has(w,"direita"))return 1;
  return -1;
}
enum class Role {None,Hand,Forearm,UpperArm,Foot,Hips,Chest};
inline Role role(const std::vector<std::string> &w) {
  // Visual e dedos nunca sao o osso procurado.
  for(const char *skip:{"visual","finger","index","middle","ring","pinky","thumb","dedo","polegar","twist","end","toe"})if(contains(w,skip))return Role::None;
  if(contains(w,"forearm")||contains(w,"lowerarm")||contains(w,"elbow")||contains(w,"antebraco")||(has(w,"fore")&&has(w,"arm"))||(has(w,"lower")&&has(w,"arm")))return Role::Forearm;
  if(contains(w,"hand")||contains(w,"wrist")||has(w,"mao")||has(w,"pulso"))return Role::Hand;
  if(contains(w,"upperarm")||(has(w,"upper")&&has(w,"arm"))||has(w,"arm")||has(w,"braco"))return Role::UpperArm;
  if(contains(w,"foot")||contains(w,"ankle")||has(w,"pe")||has(w,"tornozelo"))return Role::Foot;
  if(has(w,"hips")||has(w,"hip")||has(w,"pelvis")||has(w,"pelve")||has(w,"quadril"))return Role::Hips;
  if(contains(w,"upperchest")||has(w,"chest")||has(w,"ribs")||has(w,"spine2")||has(w,"spine02")||has(w,"peito")||has(w,"torax"))return Role::Chest;
  return Role::None;
}
} // namespace humanoid_detail

// Procura na subarvore de `root`; o primeiro nome de cada papel (ordem da
// hierarquia) vence. Antebraco e braco precisam ser ancestrais da mao.
// Preenche referencias vazias de Sistema de interacao, IK de corpo inteiro e
// Grounder do corpo com os ossos do humanoide (editor_carry_setup.cpp).
bool prefillHumanoidReferences(const EditorDocument &document,EditorEntityId owner,scene::ComponentValue &value);

inline HumanoidBones findHumanoidBones(const runtime::SceneGraph &graph,EditorEntityId root) {
  using namespace humanoid_detail;
  HumanoidBones bones;std::vector<EditorEntityId> ids;graph.collectSubtree(root,ids);
  for(const auto id:ids) {
    const auto *e=graph.find(id);if(!e||id==root)continue;
    const auto w=words(e->name);const auto r=role(w);const int s=side(w);
    const auto set=[&](EditorEntityId &slot){if(!slot)slot=id;};
    if(r==Role::Hips)set(bones.hips);
    else if(r==Role::Chest)set(bones.chest);
    else if(s>=0) {
      if(r==Role::Hand)set(bones.hand[s]);
      else if(r==Role::Forearm)set(bones.forearm[s]);
      else if(r==Role::UpperArm)set(bones.upperArm[s]);
      else if(r==Role::Foot)set(bones.foot[s]);
    }
  }
  for(u32 s=0;s<2;++s) {
    const auto ancestor=[&](EditorEntityId a,EditorEntityId b){return a&&b&&graph.isDescendantOf(b,a);};
    if(!ancestor(bones.forearm[s],bones.hand[s])||!ancestor(bones.upperArm[s],bones.forearm[s]))bones.hand[s]=bones.forearm[s]=bones.upperArm[s]=0;
  }
  return bones;
}

} // namespace ae::editor
