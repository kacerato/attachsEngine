#include "platform/android/android_editor_text_input.h"
#include "editor/editor_session.h"
#include "editor/editor_theme.h"
#include <jni.h>
#include <mutex>
#include <optional>
#include <algorithm>
#include <cstddef>
#include <string>
#include <deque>
#include <sstream>
#include <cmath>
#include <iomanip>

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
struct CodeMessage {
  int kind=0; // delta, selection/composition, history, recovery
  ae::u64 id=0,revision=0,serial=0;
  ae::u32 start=0,erased=0,end=0;
  float x=0,y=0;
  bool flag=false,focus=false;
  std::string text;
};
std::deque<CodeMessage> codeMessages;
std::size_t queuedCodeBytes=0;
ae::u64 codeAck=0,codeId=0,codeRevision=0,codeRejected=0;
std::string codeMeta,codeText;
bool codeVisible=false;
bool codeWorkspace=false;
std::string consoleCopy;
ae::ui::UiRect codeHitRect;
bool codePointerCaptured=false;
std::optional<ae::platform::android::EditorLanguageQuery> languagePending;
ae::u64 languageToken=0,languageGeneration=0;
std::string languageReply;
std::string jsonString(std::string_view input) {
  std::ostringstream out;out<<'"';
  for(unsigned char c:input) {
    if(c=='"'||c=='\\') out<<'\\'<<c;
    else if(c<32) out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;
    else out<<c;
  }
  out<<'"';return out.str();
}

void updateCode(ae::editor::EditorSession &session) {
  session.usePlatformCodeView(true);
  while(!codeMessages.empty()) {
    auto message=std::move(codeMessages.front());codeMessages.pop_front();
    queuedCodeBytes-=message.text.size();
    if(message.kind==0) {
      if(!session.applyCodeDelta(message.id,message.revision,message.start,message.erased,message.text,message.flag)) codeRejected=message.serial;
    }
    else if(message.kind==1) {
      const auto *buffer=session.screen().code?session.screen().code->active():nullptr;
      if(buffer && buffer->id==message.id && buffer->revision==message.revision) {
        session.setCodeViewState(message.id,message.revision,message.start,message.end,message.x,message.y,message.focus);
        session.setCodeComposition(message.flag && message.focus);
      }
    } else if(message.kind==2) {
      const auto *buffer=session.screen().code?session.screen().code->active():nullptr;
      if(buffer && buffer->id==message.id && buffer->revision==message.revision) session.codeHistoryAction(message.flag);
    } else if(message.kind==3) session.recoverCodeDraft(message.text);
    else if(message.kind==4 || message.kind==5) {
      const auto *workspace=session.screen().code;
      const auto *buffer=workspace?workspace->active():nullptr;
      if(buffer && buffer->id==message.id && buffer->revision==message.revision) {
        if(message.kind==5) session.navigateCode(message.text,message.start,message.end);
        else if(message.start<=2) {
          std::ostringstream json;
          json<<"{\"Root\":"<<jsonString(session.codeProjectRoot())<<",\"File\":"<<jsonString(buffer->path)
              <<",\"Position\":"<<message.erased<<",\"Operation\":"<<message.start<<",\"Query\":"<<jsonString(message.text)<<",\"Documents\":[";
          bool first=true;
          for(const auto &document:workspace->buffers()) if(document.path.ends_with(".cs")) {
            if(!first) json<<',';first=false;
            json<<"{\"Path\":"<<jsonString(document.path)<<",\"Text\":"<<jsonString(document.text)<<'}';
          }
          json<<"]}";
          languageToken=message.serial;languageReply.clear();
          languagePending=ae::platform::android::EditorLanguageQuery{message.serial,message.id,message.revision,workspace->generation(),json.str()};
        }
      }
    }
    codeAck=std::max(codeAck,message.serial);
  }
  const auto &state=session.screen();
  languageGeneration=state.code?state.code->generation():0;
  codeWorkspace=state.workspace==ae::editor::EditorWorkspace::Code;
  if(auto copied=session.takeConsoleCopy();!copied.empty()) consoleCopy=std::move(copied);
  const auto *buffer=state.code?state.code->active():nullptr;
  const auto &rect=session.layout().codeBody;
  codeVisible=!state.importPanel && !state.codeRecoveryPending && state.workspace==ae::editor::EditorWorkspace::Code && buffer && !rect.isEmpty() &&
      !state.codeMenu && !state.codeFiles && !state.choosingTemplate &&
      session.pendingTextEdit().purpose==ae::editor::EditorTextPurpose::None;
  codeHitRect=rect;
  if(!buffer) {session.setCodeComposition(false);codeId=codeRevision=0;codeText.clear();return;}
  if(codeId!=buffer->id || state.workspace!=ae::editor::EditorWorkspace::Code) session.setCodeComposition(false);
  if(codeId!=buffer->id || codeRevision!=buffer->revision) {
    codeId=buffer->id;codeRevision=buffer->revision;codeText=buffer->text;
  }
  const auto &color=ae::editor::editorCodeTheme().color;
  std::ostringstream meta;
  meta<<codeId<<' '<<codeRevision<<' '<<codeAck<<' '<<buffer->viewRevision<<' '
      <<buffer->selectionStart<<' '<<buffer->selectionEnd<<' '<<buffer->scrollX<<' '<<buffer->scrollY<<' '
      <<rect.x/state.surface.width<<' '<<rect.y/state.surface.height<<' '
      <<rect.width/state.surface.width<<' '<<rect.height/state.surface.height<<' '
      <<color.surface<<' '<<color.text<<' '<<color.textMuted<<' '<<color.line<<' '
      <<color.accent<<' '<<color.warning<<' '<<buffer->path.ends_with(".cs")<<' '
      <<buffer->undo.empty()<<' '<<buffer->redo.empty()<<' '<<codeRejected<<' '<<state.surface.width;
  const auto &accessory=session.layout().codeAccessory;
  meta<<' '<<accessory.x/state.surface.width<<' '<<accessory.y/state.surface.height<<' '
      <<accessory.width/state.surface.width<<' '<<accessory.height/state.surface.height<<' '<<state.surface.height<<' '<<state.codeSearchRequest;
  codeMeta=meta.str();
}
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
std::optional<EditorLanguageQuery> takeEditorLanguageQuery() {
  std::lock_guard lock(mutex);auto result=std::move(languagePending);languagePending.reset();return result;
}
void completeEditorLanguageQuery(const EditorLanguageQuery &query,std::string result) {
  std::lock_guard lock(mutex);
  if(query.token!=languageToken || query.id!=codeId || query.revision!=codeRevision ||
      query.generation!=languageGeneration || !codeWorkspace) return;
  languageReply="{\"Token\":"+std::to_string(query.token)+",\"Buffer\":"+std::to_string(query.id)+
      ",\"Revision\":"+std::to_string(query.revision)+",\"Result\":"+result+"}";
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_dev_aether_editor_EditorCodeInput_takeLanguage(JNIEnv *env,jclass) {
  std::lock_guard lock(mutex);if(languageReply.empty()) return nullptr;
  auto result=env->NewByteArray(static_cast<jsize>(languageReply.size()));if(!result) return nullptr;
  env->SetByteArrayRegion(result,0,static_cast<jsize>(languageReply.size()),reinterpret_cast<const jbyte*>(languageReply.data()));
  languageReply.clear();return result;
}
bool editorCodePanelVisible() {
  std::lock_guard lock(mutex);
  return codeVisible;
}
bool editorCodeOwnsPointer(float x,float y,int action) {
  std::lock_guard lock(mutex);
  if(action==0) codePointerCaptured=codeVisible && x>=codeHitRect.x && x<codeHitRect.right() &&
      y>=codeHitRect.y && y<codeHitRect.bottom();
  const bool owned=codePointerCaptured;
  if(action==1 || action==3) codePointerCaptured=false;
  return owned;
}
void updateEditorTextInput(editor::EditorSession &session) {
  std::lock_guard lock(mutex);
  session.setPlatformImeFraction(imeFraction);
  updateCode(session);
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

extern "C" JNIEXPORT jobjectArray JNICALL
Java_dev_aether_editor_EditorCodeInput_poll(JNIEnv *env,jclass,jlong id,jlong revision) {
  std::lock_guard lock(mutex);
  if(!codeVisible) return nullptr;
  const bool changed=static_cast<ae::u64>(id)!=codeId || static_cast<ae::u64>(revision)!=codeRevision;
  jclass bytes=env->FindClass("[B");if(!bytes) return nullptr;
  auto result=env->NewObjectArray(2,bytes,nullptr);env->DeleteLocalRef(bytes);
  if(!result) return nullptr;
  const std::string *values[]{&codeMeta,changed?&codeText:nullptr};
  for(int i=0;i<2;++i) if(values[i]) {
    auto value=env->NewByteArray(static_cast<jsize>(values[i]->size()));if(!value) return nullptr;
    env->SetByteArrayRegion(value,0,static_cast<jsize>(values[i]->size()),reinterpret_cast<const jbyte*>(values[i]->data()));
    env->SetObjectArrayElement(result,i,value);env->DeleteLocalRef(value);
  }
  return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_aether_editor_EditorCodeInput_workspace(JNIEnv *,jclass) {
  std::lock_guard lock(mutex);return codeWorkspace;
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_dev_aether_editor_EditorCodeInput_takeCopy(JNIEnv *env,jclass) {
  std::lock_guard lock(mutex);
  if(consoleCopy.empty()) return nullptr;
  auto result=env->NewByteArray(static_cast<jsize>(consoleCopy.size()));
  if(!result) return nullptr;
  env->SetByteArrayRegion(result,0,static_cast<jsize>(consoleCopy.size()),reinterpret_cast<const jbyte*>(consoleCopy.data()));
  consoleCopy.clear();return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_aether_editor_EditorCodeInput_send(JNIEnv *env,jclass,jint kind,jlong id,jlong revision,
    jlong serial,jint start,jint erased,jbyteArray bytes,jint end,jfloat x,jfloat y,jboolean flag,jboolean focus) {
  if(kind<0 || kind>5 || start<0 || erased<0 || end<0 || !std::isfinite(x) || !std::isfinite(y)) return false;
  CodeMessage message;message.kind=kind;message.id=id;message.revision=revision;message.serial=serial;
  message.start=start;message.erased=erased;message.end=end;message.x=x;message.y=y;message.flag=flag;message.focus=focus;
  if(bytes) {
    const auto size=env->GetArrayLength(bytes);
    if(size>static_cast<jsize>(ae::editor::EditorCodeWorkspace::MaximumFileBytes)) return false;
    message.text.resize(size);env->GetByteArrayRegion(bytes,0,size,reinterpret_cast<jbyte*>(message.text.data()));
  }
  std::lock_guard lock(mutex);
  if(codeMessages.size()>=256 || queuedCodeBytes+message.text.size()>1024*1024) return false;
  queuedCodeBytes+=message.text.size();codeMessages.push_back(std::move(message));return true;
}
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_dev_aether_editor_EditorTextInput_poll(JNIEnv *env,jclass) {
  std::lock_guard lock(mutex);
  if(!visible) return nullptr;
  const bool number=request.purpose==ae::editor::EditorTextPurpose::Number || request.purpose==ae::editor::EditorTextPurpose::CodeLine;
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
