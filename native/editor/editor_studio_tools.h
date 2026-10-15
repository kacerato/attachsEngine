#pragma once
// Ferramentas de autoria do estudio (pacote B: UMotion no jeito Astra).
// Vivem fora do editor_session.h: EditorStudio e amigo da sessao, entao novas
// ferramentas so recompilam este par de arquivos e quem os chama.
#include "editor/editor_document.h"
#include <string>

namespace ae::editor {
class EditorSession;
struct EditorStudio {
  // Assistente de IK: membros (maos/pes/patas), cabeca e chao a partir da
  // hierarquia real; cria componentes permanentes e devolve o resumo.
  static bool setupIk(EditorSession &,EditorEntityId owner,std::string &summary,std::string &error);
  // Receitas de Criar › Personagem sobre o personagem do usuário (Unity
  // Animation Rigging: MultiAim/TwoBoneIK/constraints sobre o rig; Godot
  // SkeletonIK3D sob o Skeleton3D): cada uma cria os componentes e os alvos
  // numa única operação de histórico, com os ossos detectados pelo nome ou
  // pela estrutura. `All` é o assistente de IK do estúdio.
  enum class CharacterRecipe : u32 {All,LookAt,Aim,Hands,Ground,FullBody,Carry,Count};
  static bool setupCharacter(EditorSession &,EditorEntityId owner,CharacterRecipe recipe,std::string &summary,std::string &error);
  // Personagem da seleção (ou o único da cena sem seleção); 0 quando não há.
  static EditorEntityId characterOf(const runtime::SceneGraph &document,EditorEntityId selection);
  // Fixa a ponta selecionada no lugar onde esta agora, de `start` a `end`:
  // a cadeia de dois ossos e resolvida por quadro e gravada como chaves FK.
  static bool pinTip(EditorSession &,EditorEntityId tip,float start,float end,std::string &summary,std::string &error);
  // Child-Of gravado: `object` acompanha `parent` de `start` a `end` mantendo
  // o deslocamento atual, como chaves de posicao/rotacao do proprio objeto.
  static bool followObject(EditorSession &,EditorEntityId object,EditorEntityId parent,float start,float end,std::string &summary,std::string &error);
  // Root motion: cria no personagem o componente com o osso raiz encontrado
  // (nomes ou primeira bifurcacao); com o componente presente, liga/desliga.
  static bool toggleRootMotion(EditorSession &,EditorEntityId owner,std::string &summary,std::string &error);
  static EditorEntityId rootMotionBone(const runtime::SceneGraph &document,EditorEntityId owner);
  // Bake da pose final (N06): job incremental sobre o clipe aberto. `bakeStep`
  // avanca `frames` quadros; devolve verdadeiro enquanto ainda ha trabalho. O
  // resultado e um clipe novo verificado sem solvers; cancelar nao publica.
  static bool bakeStart(EditorSession &,bool rootToOwner,std::string &summary,std::string &error,float rate=30);
  static bool bakeStep(EditorSession &,u32 frames,std::string &summary,std::string &error);
  static void bakeCancel(EditorSession &);
  // Exportacao GLB do clipe aberto (esqueleto + animacao) com relatorio de
  // perdas e prova de reimportacao; grava em Exportados/.
  static bool exportGlb(EditorSession &,std::string &summary,std::string &error);
};
} // namespace ae::editor
