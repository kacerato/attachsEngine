// Cria um IK de dois ossos permanente a partir do estudio: alvo e direcao da
// dobra viram objetos reais na cena (editaveis, salvos, removiveis) e o
// componente fica na ponta. Uma unica operacao de Desfazer remove tudo.
#include "editor/editor_session.h"
#include "scene/ik_constraints.h"
#include "runtime/transform_math.h"

namespace ae::editor {
bool EditorSession::createTwoBoneIkForTip(EditorEntityId tip,std::string &error) {
  error.clear();
  if(isPlaying()||history_.isOpen()) {error="Pare o Play e conclua a edição";return false;}
  const auto *end=document_.find(tip);const auto *mid=end?document_.find(end->parent):nullptr;
  const auto *root=mid?document_.find(mid->parent):nullptr;
  if(!root||mid->id==document_.root()||root->id==document_.root()) {error="Selecione a ponta de uma cadeia com dois ossos acima";return false;}
  if(end->components.find(scene::TwoBoneIk::descriptor)) {error="Esta ponta já tem IK de dois ossos";return false;}
  float tipWorld[16],midWorld[16];
  if(!runtime::worldMatrix(document_,tip,tipWorld)||!runtime::worldMatrix(document_,mid->id,midWorld)) {error="Pose da cadeia indisponível";return false;}
  // Alvo e direcao ficam no topo do personagem, fora da cadeia (sem ciclo).
  EditorEntityId owner=tip;for(const auto *o=end;o&&o->parent!=document_.root();o=document_.find(o->parent))owner=o->parent;
  const std::string tipName=end->name;
  if(!history_.begin("Criar IK de dois ossos")) {error="Conclua a edição em andamento";return false;}
  bool ok=true;
  const auto place=[&](EditorEntityId id,const float world[16]) {
    float parent[16];runtime::Transform local;
    return runtime::parentWorldMatrix(document_,id,parent)&&runtime::localTransformForWorld(world,parent,local)&&
           history_.setTransform(document_,id,local);
  };
  const auto target=history_.createEntity(document_,owner,EditorEntityKind::Folder,"Alvo IK · "+tipName);
  const auto pole=history_.createEntity(document_,owner,EditorEntityKind::Folder,"Direção IK · "+tipName);
  ok=target&&pole;
  if(ok) {float targetWorld[16]{1,0,0,0,0,1,0,0,0,0,1,0,tipWorld[12],tipWorld[13],tipWorld[14],1};ok=place(target,targetWorld);}
  if(ok) {
    // Direcao inicial: o joelho/cotovelo atual afastado do eixo raiz-ponta.
    float rootWorld[16];runtime::worldMatrix(document_,root->id,rootWorld);
    float axis[3],bend[3];for(u32 c=0;c<3;++c){axis[c]=tipWorld[12+c]-rootWorld[12+c];bend[c]=midWorld[12+c]-rootWorld[12+c];}
    const float aa=axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2],ab=axis[0]*bend[0]+axis[1]*bend[1]+axis[2]*bend[2];
    float off[3];for(u32 c=0;c<3;++c)off[c]=bend[c]-(aa>1e-8f?ab/aa:0)*axis[c];
    const float len=std::sqrt(off[0]*off[0]+off[1]*off[1]+off[2]*off[2]),reach=std::sqrt(aa)*.5f+.1f;
    float poleWorld[16]{1,0,0,0,0,1,0,0,0,0,1,0,midWorld[12],midWorld[13],midWorld[14],1};
    if(len>1e-5f)for(u32 c=0;c<3;++c)poleWorld[12+c]+=off[c]/len*reach;
    ok=place(pole,poleWorld);
  }
  if(ok) {
    auto values=*document_.find(tip);
    auto *ik=static_cast<scene::TwoBoneIk*>(values.components.add(scene::TwoBoneIk::descriptor));
    ok=ik!=nullptr;
    if(ok) {ik->target=target;ik->pole=pole;ok=history_.applyValues(document_,tip,values);}
  }
  history_.end();
  if(!ok) {history_.undo(document_);error="Não foi possível criar o IK; nada foi alterado";return false;}
  return true;
}
} // namespace ae::editor
