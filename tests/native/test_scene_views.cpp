// Vistas salvas da cena (G6-A): o enquadramento é dado autoral.
//
// Protegido aqui: o nome endereça a vista (vazio, repetido ou com aspas é
// recusado); ida e volta pelo arquivo preserva alvo, distância, ângulos e
// lente; arquivo ilegível não entra pela metade; a sessão volta à MESMA
// câmera; e salvar, renomear e excluir têm um passo de Desfazer.
#include "harness.h"
#include "editor/editor_archive.h"
#include "editor/editor_session.h"
#include "runtime/scene_views.h"

#include <cmath>
#include <sstream>

using namespace ae;
using namespace ae::editor;

namespace {
runtime::SceneView made(const char *name, float x, float distance, float yaw) {
  runtime::SceneView view;
  view.name = name;
  view.target[0] = x;
  view.target[1] = 1.5f;
  view.target[2] = -3.25f;
  view.distance = distance;
  view.yaw = yaw;
  view.pitch = 0.35f;
  view.verticalFov = 1.0471976f;  // 60°, a lente da Scene view
  return view;
}
} // namespace

AE_TEST(scene_view_names_address_the_framing) {
  runtime::SceneViews views;
  AE_EXPECT_TRUE(views.add(made("Interior",1,6,0.2f)),"vista com nome entra na lista");
  AE_EXPECT_TRUE(!views.add(made("Interior",9,9,0.9f)),"nome repetido é recusado");
  AE_EXPECT_TRUE(!views.add(made("",0,6,0)),"nome vazio é recusado");
  AE_EXPECT_TRUE(!views.add(made("Vista \"boa\"",0,6,0)),"aspas quebrariam o arquivo");
  // Distância zero ou negativa não é câmera de órbita: seria o alvo dentro da
  // lente, sem enquadramento nenhum.
  AE_EXPECT_TRUE(!views.add(made("Grudada",0,0,0)),"distância zero é recusada");
  AE_EXPECT_EQ(views.count(),1u,"nenhuma recusa deixou lixo na lista");
  // Atualizar é gravar por cima da de mesmo nome, não criar outra.
  AE_EXPECT_TRUE(views.replace(made("Interior",4,8,0.7f)),"atualizar a vista existente");
  AE_EXPECT_EQ(views.count(),1u,"atualizar não duplica a vista");
  AE_EXPECT_EQ(views.find("Interior")->distance,8.0f,"o enquadramento novo é o que vale");
  AE_EXPECT_TRUE(views.rename(0,"Sala"),"renomear pelo índice da lista");
  AE_EXPECT_TRUE(views.add(made("Externa",0,30,1.2f)),"segunda vista");
  AE_EXPECT_TRUE(!views.rename(1,"Sala"),"renomear para um nome que já existe é recusado");
  AE_EXPECT_TRUE(views.remove(0) && views.count()==1 && views.at(0)->name=="Externa","excluir tira só a escolhida");
  AE_EXPECT_TRUE(!views.remove(7),"índice fora da lista não exclui nada");
}

AE_TEST(scene_views_survive_the_file_exactly) {
  runtime::SceneViews views;
  AE_EXPECT_TRUE(views.add(made("Interior com nome longo",2.5f,4.5f,-1.25f)),"vista autoral");
  AE_EXPECT_TRUE(views.add(made("Externa",-12,42,2.5f)),"segunda vista");
  std::ostringstream out;
  out << std::setprecision(9);
  views.write(out);
  std::istringstream in(out.str());
  runtime::SceneViews restored;
  AE_EXPECT_TRUE(restored.read(in),"o arquivo volta a ser lista");
  AE_EXPECT_TRUE(restored==views,"alvo, distância, ângulos e lente voltam iguais");
  // Vista ilegível recusa o arquivo inteiro: perder UM enquadramento em
  // silêncio é perder justamente a comparação que ele guardava.
  std::istringstream truncated("2 \"Interior\" 0 0 0 6");
  runtime::SceneViews partial;
  AE_EXPECT_TRUE(!partial.read(truncated),"lista cortada é recusada");
  AE_EXPECT_TRUE(partial.empty(),"nada entra pela metade");
  std::istringstream repeated("2 \"A\" 0 0 0 6 0 0 0 \"A\" 1 1 1 7 0 0 0");
  runtime::SceneViews duplicated;
  AE_EXPECT_TRUE(!duplicated.read(repeated),"arquivo com nome repetido é recusado");
}

AE_TEST(scene_views_travel_in_the_scene_archive) {
  EditorDocument document;
  runtime::SceneViews views;
  AE_EXPECT_TRUE(views.add(made("Interior",1.5f,7.25f,0.8f)),"vista autoral");
  document.setViews(views);
  const auto text = serializeEditorDocument(document,0);
  AE_EXPECT_TRUE(text.starts_with("AETHER_EDITOR 17 "),"versão atual preserva vistas, tags e grupos");
  EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(text,0,restored),"o arquivo abre");
  AE_EXPECT_TRUE(restored.views()==views,"a vista volta com a cena");
  // Cena sem vista nenhuma não deixa a seção quebrada para trás.
  EditorDocument empty;
  EditorDocument reopened;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(empty,0),0,reopened),"cena sem vistas abre");
  AE_EXPECT_TRUE(reopened.views().empty(),"nenhuma vista foi inventada");
}

AE_TEST(session_returns_to_the_same_framing_and_undo_covers_it) {
  EditorSession session;
  AE_EXPECT_TRUE(session.importMap({}, {}, false),"cena independente");
  const float interior[3]{1.0f,1.6f,-2.0f};
  session.setCameraPose(interior,0.9f,0.4f);
  const auto saved = session.camera();
  AE_EXPECT_TRUE(session.saveSceneView("Interior"),"salvar a vista atual");
  AE_EXPECT_EQ(session.document().views().count(),1u,"a vista entrou no documento");
  const float outro[3]{-40.0f,9.0f,30.0f};
  session.setCameraPose(outro,-2.0f,1.1f);
  AE_EXPECT_TRUE(std::fabs(session.camera().yaw-saved.yaw)>0.5f,"a câmera realmente saiu do lugar");
  AE_EXPECT_TRUE(session.applySceneView(0),"voltar para a vista salva");
  const auto back = session.camera();
  AE_EXPECT_TRUE(std::fabs(back.target[0]-saved.target[0])<1e-4f &&
                 std::fabs(back.target[1]-saved.target[1])<1e-4f &&
                 std::fabs(back.target[2]-saved.target[2])<1e-4f,"o alvo orbitado é o mesmo");
  AE_EXPECT_TRUE(std::fabs(back.distance-saved.distance)<1e-4f &&
                 std::fabs(back.yaw-saved.yaw)<1e-4f &&
                 std::fabs(back.pitch-saved.pitch)<1e-4f,"distância e ângulos são os mesmos");
  AE_EXPECT_TRUE(!session.applySceneView(9),"índice fora da lista não mexe na câmera");

  AE_EXPECT_TRUE(session.renameSceneView(0,"Sala"),"renomear a vista");
  AE_EXPECT_TRUE(session.document().views().at(0)->name=="Sala","o nome novo vale");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"desfazer o renomear");
  AE_EXPECT_TRUE(session.document().views().at(0)->name=="Interior","o nome antigo voltou");
  AE_EXPECT_TRUE(session.deleteSceneView(0) && session.document().views().empty(),"excluir a vista");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"desfazer o excluir");
  AE_EXPECT_TRUE(session.document().views().count()==1 &&
                 session.document().views().at(0)->name=="Interior","a vista excluída voltou inteira");
  AE_EXPECT_TRUE(session.history().undo(session.document()) && session.document().views().empty(),
                 "desfazer o salvar tira a vista");
  AE_EXPECT_TRUE(session.history().redo(session.document()) && session.document().views().count()==1,
                 "refazer devolve a vista");
}
