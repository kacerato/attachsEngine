#pragma once
#include "editor/editor_session.h"
#include "editor/editor_screen.h"
#include "ui/ui_font.h"
#include <algorithm>

namespace ae::test {
// O Inspector é uma coluna rolável (ITsMagic/Unity): o widget pedido pode estar
// abaixo da janela. Procura na tela e, se não achar, desce a coluna como o dedo
// faria até ele aparecer; a coluna fica rolada onde ele está.
inline ui::UiPoint findEditorWidget(editor::EditorSession &s,const ui::UiFont &font,u32 widget) {
  const auto scan=[&](editor::EditorScreenLayout &layout)->ui::UiPoint {
    ui::UiInputRouter router;ui::UiDrawList list;list.begin(s.screen().surface,font.metrics(ui::UiFontWeight::Regular));
    layout=editor::buildEditorScreen(s.screen(),ui::defaultTheme(),list,router);
    for(float y=2;y<s.screen().surface.height;y+=4)for(float x=2;x<s.screen().surface.width;x+=4) {
      const auto hit=router.hitTest({x,y});
      if(hit.target==ui::UiPointerTarget::Widget&&hit.widgetId==widget)return {x,y};
    }
    return {-1,-1};
  };
  s.update();editor::EditorScreenLayout layout;
  auto at=scan(layout);if(at.x>=0)return at;
  const auto window=layout.inspectorScrollWindow;const float content=layout.inspectorScrollContent;
  if(window.isEmpty()||content<=window.height)return at;
  auto &state=const_cast<editor::EditorScreenState&>(s.screen());
  const float original=state.inspectorScroll,limit=content-window.height,step=std::max(40.f,window.height*.6f);
  for(float offset=0;;offset=std::min(limit,offset+step)) {
    state.inspectorScroll=offset;at=scan(layout);
    if(at.x>=0){s.update();return at;}
    if(offset>=limit)break;
  }
  state.inspectorScroll=original;s.update();return at;
}
}
