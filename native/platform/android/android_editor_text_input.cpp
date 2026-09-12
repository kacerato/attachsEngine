#include "platform/android/android_editor_text_input.h"
#include "editor/editor_session.h"
#include <jni.h>
#include <mutex>
#include <optional>
#include <algorithm>
#include <cstddef>
#include <string>

namespace {
std::mutex mutex;
ae::editor::EditorTextEdit request;
ae::u64 sequence=0;
bool visible=false, invalid=false;
struct Reply { ae::u64 sequence; bool accept; std::string text; };
std::optional<Reply> reply;
// Texto VIVO, a cada tecla. O `AlertDialog` so devolvia o resultado final; com
// o campo embutido o editor precisa saber o que esta sendo digitado para poder
// desenhar. Guardado como o ultimo estado conhecido e nao como uma fila: quem
// desenha quer o agora, e quadros perdidos nao precisam ser reproduzidos.
struct Draft { ae::u64 sequence; ae::u32 caret; std::string text; };
std::optional<Draft> draft;
float imeFraction=0.0f;
// Cursor inicial do pedido atual, em bytes. O editor de codigo escolhe pelo
// toque; os outros campos comecam no fim do texto.
std::size_t seedCaret=0;
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
  session.setPlatformImeFraction(imeFraction);
  // O rascunho vale antes da resposta: a ultima tecla e a confirmacao chegam no
  // mesmo quadro com frequencia, e aplicar o commit primeiro faria o campo
  // piscar o texto anterior.
  if(draft && visible && draft->sequence==sequence) {
    session.updateTextDraft(request,draft->text,draft->caret);
    draft.reset();
  } else if(draft && (!visible || draft->sequence!=sequence)) draft.reset();
  if(reply) {
    if(visible && reply->sequence==sequence)
      invalid=!session.completeTextEdit(request,reply->text,reply->accept);
    reply.reset();visible=false;
  }
  const auto pending=session.pendingTextEdit();
  if(pending.purpose==editor::EditorTextPurpose::None) {visible=false;invalid=false;return;}
  if(!visible || !same(request,pending)) {
    request=pending;visible=true;++sequence;
    // So o editor de codigo escolhe onde o cursor comeca -- pelo toque na
    // linha. Um campo curto comeca no fim do texto, e nao num cursor que sobrou
    // da edicao anterior.
    seedCaret=pending.purpose==editor::EditorTextPurpose::Code
        ? session.screen().platformCaret : pending.text.size();
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
  const auto caret=std::min<std::size_t>(seedCaret,request.text.size());
  const std::string values[]{std::to_string(sequence),code?"code":number?"number":"text",
    invalid?"Alteração recusada; revise o campo":code?"Editar código":number?"Editar valor":
      property?("Campo · "+request.propertyType):
      request.purpose==ae::editor::EditorTextPurpose::ScriptName?"Nova classe C#":
      request.purpose==ae::editor::EditorTextPurpose::ResourceName?"Renomear recurso":
      request.purpose==ae::editor::EditorTextPurpose::Rename?"Renomear objeto":"Pesquisar",
    request.text,code?"524288":property?"4096":number?"47":"63",
    // Onde o cursor comeca, em BYTES. Para o codigo ele vem do toque -- a linha
    // que o dedo escolheu --, e nao do fim do arquivo.
    std::to_string(caret)};
  jclass bytes=env->FindClass("[B");if(!bytes) return nullptr;
  auto result=env->NewObjectArray(6,bytes,nullptr);env->DeleteLocalRef(bytes);
  if(!result) return nullptr;
  for(int i=0;i<6;++i) {
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

// Cada tecla, composicao ou colagem. O texto inteiro e nao um delta: a ponte
// nao mantem estado proprio, entao um evento perdido nao dessincroniza nada --
// o proximo ja carrega a verdade. Barato porque o campo e curto; o editor de
// codigo continua no caminho antigo, que e o que o M05.1 ainda nao cobre.
extern "C" JNIEXPORT void JNICALL
Java_dev_aether_editor_EditorTextInput_push(JNIEnv *env,jclass,jlong id,jbyteArray bytes,jint caret) {
  if(!bytes || env->GetArrayLength(bytes)>static_cast<jsize>(ae::editor::EditorCodeWorkspace::MaximumFileBytes)) return;
  const auto size=env->GetArrayLength(bytes);std::string text;text.resize(static_cast<size_t>(size));
  env->GetByteArrayRegion(bytes,0,size,reinterpret_cast<jbyte *>(text.data()));
  std::lock_guard lock(mutex);
  if(visible && static_cast<ae::u64>(id)==sequence)
    draft=Draft{static_cast<ae::u64>(id),caret<0?0u:static_cast<ae::u32>(caret),std::move(text)};
}

// Quanto da janela o teclado ocupa, como FRACAO. O Android mede em pixels
// fisicos e a superficie do editor e logica; converter no meio do caminho seria
// mais uma unidade para errar.
extern "C" JNIEXPORT void JNICALL
Java_dev_aether_editor_EditorTextInput_ime(JNIEnv *,jclass,jfloat fraction) {
  std::lock_guard lock(mutex);
  imeFraction=fraction;
}
