#pragma once
namespace ae::editor { class EditorSession; }
namespace ae::platform::android {
// Called only by the session owner. JNI only exchanges bounded messages.
void updateEditorTextInput(editor::EditorSession &session);
}
