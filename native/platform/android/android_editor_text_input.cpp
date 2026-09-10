#include "platform/android/android_editor_text_input.h"
#include "editor/editor_session.h"
#include <jni.h>
#include <mutex>
#include <optional>

namespace {
std::mutex mutex;
ae::editor::EditorTextEdit request;
ae::u64 sequence=0;
bool visible=false, invalid=false;
struct Reply { ae::u64 sequence; bool accept; std::string text; };
std::optional<Reply> reply;
bool same(const ae::editor::EditorTextEdit &a,const ae::editor::EditorTextEdit &b) {
  return a.purpose==b.purpose && a.entity==b.entity && a.field==b.field &&
      a.version.epoch==b.version.epoch && a.version.revision==b.version.revision &&
      a.bufferId==b.bufferId && a.bufferRevision==b.bufferRevision && a.componentInstance==b.componentInstance &&
      a.propertyId==b.propertyId && a.propertyType==b.propertyType;
}
}
namespace ae::platform::android {
void updateEditorTextInput(editor::EditorSession &session) {
  std::lock_guard lock(mutex);
  if(reply) {
    if(visible && reply->sequence==sequence)
      invalid=!session.completeTextEdit(request,reply->text,reply->accept);
    reply.reset();visible=false;
  }
  const auto pending=session.pendingTextEdit();
  if(pending.purpose==editor::EditorTextPurpose::None) {visible=false;invalid=false;return;}
  if(!visible || !same(request,pending)) {
    request=pending;visible=true;++sequence;
  }
}
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_dev_aether_editor_EditorTextInput_poll(JNIEnv *env,jclass) {
  std::lock_guard lock(mutex);
  if(!visible) return nullptr;
  const bool number=request.purpose==ae::editor::EditorTextPurpose::Number;
  const bool code=request.purpose==ae::editor::EditorTextPurpose::Code;
  const bool property=request.purpose==ae::editor::EditorTextPurpose::ScriptProperty;
  const std::string values[]{std::to_string(sequence),code?"code":number?"number":"text",
    invalid?"Alteração recusada; revise o campo":code?"Editar código":number?"Editar valor":
      property?("Campo · "+request.propertyType):
      request.purpose==ae::editor::EditorTextPurpose::ScriptName?"Nova classe C#":
      request.purpose==ae::editor::EditorTextPurpose::Rename?"Renomear objeto":"Pesquisar",
    request.text,code?"524288":property?"4096":number?"47":"63"};
  jclass bytes=env->FindClass("[B");if(!bytes) return nullptr;
  auto result=env->NewObjectArray(5,bytes,nullptr);env->DeleteLocalRef(bytes);
  if(!result) return nullptr;
  for(int i=0;i<5;++i) {
    auto value=env->NewByteArray(static_cast<jsize>(values[i].size()));if(!value) return nullptr;
    env->SetByteArrayRegion(value,0,static_cast<jsize>(values[i].size()),
        reinterpret_cast<const jbyte *>(values[i].data()));
    env->SetObjectArrayElement(result,i,value);env->DeleteLocalRef(value);
  }
  return result;
}
extern "C" JNIEXPORT void JNICALL
Java_dev_aether_editor_EditorTextInput_submit(JNIEnv *env,jclass,jlong id,jbyteArray bytes,jboolean accept) {
  if(!bytes || env->GetArrayLength(bytes)>static_cast<jsize>(ae::editor::EditorCodeWorkspace::MaximumFileBytes)) return;
  const auto size=env->GetArrayLength(bytes);std::string text(static_cast<size_t>(size),'\0');
  env->GetByteArrayRegion(bytes,0,size,reinterpret_cast<jbyte *>(text.data()));
  std::lock_guard lock(mutex);
  if(visible && static_cast<ae::u64>(id)==sequence && !reply)
    reply=Reply{static_cast<ae::u64>(id),accept!=0,std::move(text)};
}
