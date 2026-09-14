#pragma once
#include "core/base.h"
#include <string>
#include <optional>
namespace ae::editor { class EditorSession; }
namespace ae::platform::android {
bool editorCodePanelVisible();
// Called only by the session owner. JNI only exchanges bounded messages.
void updateEditorTextInput(editor::EditorSession &session);
// NativeActivity must finish these events as NOT handled so ViewRootImpl
// forwards the entire gesture to the embedded EditText.
bool editorCodeOwnsPointer(float x,float y,int action);
struct EditorLanguageQuery {ae::u64 token=0,id=0,revision=0,generation=0;std::string json;};
std::optional<EditorLanguageQuery> takeEditorLanguageQuery();
void completeEditorLanguageQuery(const EditorLanguageQuery &query,std::string result);
}
