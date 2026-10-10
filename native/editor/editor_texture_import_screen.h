#pragma once
#include "editor/editor_screen.h"
namespace ae::editor {
inline constexpr u32 TextureImportPageCount=5;
inline u32 textureImportPageCount(const EditorScreenState &state) {return state.textureImportTiff?8:TextureImportPageCount;}
void buildTextureImportWorkspace(const EditorScreenState &state,const ui::UiTheme &theme,
    ui::UiDrawList &list,ui::UiInputRouter &router,ui::UiRect area);
}
