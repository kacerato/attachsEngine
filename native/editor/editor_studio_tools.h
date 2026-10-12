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
  // Fixa a ponta selecionada no lugar onde esta agora, de `start` a `end`:
  // a cadeia de dois ossos e resolvida por quadro e gravada como chaves FK.
  static bool pinTip(EditorSession &,EditorEntityId tip,float start,float end,std::string &summary,std::string &error);
  // Child-Of gravado: `object` acompanha `parent` de `start` a `end` mantendo
  // o deslocamento atual, como chaves de posicao/rotacao do proprio objeto.
  static bool followObject(EditorSession &,EditorEntityId object,EditorEntityId parent,float start,float end,std::string &summary,std::string &error);
};
} // namespace ae::editor
