#include "platform/android/android_model_picker.h"

#include <jni.h>
#include <mutex>
#include <optional>

namespace {
std::mutex mutex;
// `sequence` é o que impede uma resposta atrasada de um pedido cancelado ser
// aplicada ao pedido seguinte.
ae::u64 sequence = 0;
bool pending = false;
std::optional<ae::platform::android::ModelPickerResult> result;
// Teto do que atravessa a fronteira JNI de uma vez. Um GLB maior que isto não é
// recusado por capricho: ele seria copiado inteiro para a heap do processo três
// vezes (array Java, cópia nativa, projeto) antes de qualquer validação.
constexpr jsize kMaximumBytes = 128 * 1024 * 1024;
} // namespace

namespace ae::platform::android {

void requestModelPick() {
  std::lock_guard lock(mutex);
  // Pedir de novo enquanto um pedido está aberto NÃO é ignorado: o seletor pode
  // ter sido encerrado pelo sistema sem devolver nada, e o usuário ficaria com
  // um botão que não responde mais. O número de sequência novo faz a resposta
  // atrasada do pedido anterior ser descartada.
  pending = true;
  ++sequence;
  result.reset();
}

bool modelPickPending() {
  std::lock_guard lock(mutex);
  return pending;
}

bool takeModelPickResult(ModelPickerResult &out) {
  std::lock_guard lock(mutex);
  if (!result) return false;
  out = std::move(*result);
  result.reset();
  return true;
}

} // namespace ae::platform::android

extern "C" JNIEXPORT jlong JNICALL
Java_dev_aether_editor_ModelPicker_poll(JNIEnv *, jclass) {
  std::lock_guard lock(mutex);
  // Zero é "nada pedido". O token identifica ESTE pedido e volta no submit.
  return pending && !result ? static_cast<jlong>(sequence) : 0;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_aether_editor_ModelPicker_submit(JNIEnv *env, jclass, jlong token, jbyteArray bytes,
                                          jbyteArray name, jbyteArray diagnostic) {
  ae::platform::android::ModelPickerResult reply;
  if (bytes) {
    const auto size = env->GetArrayLength(bytes);
    if (size < 0 || size > kMaximumBytes) {
      reply.diagnostic = "O arquivo é grande demais para esta importação.";
    } else {
      reply.bytes.resize(static_cast<ae::usize>(size));
      env->GetByteArrayRegion(bytes, 0, size, reinterpret_cast<jbyte *>(reply.bytes.data()));
      reply.accepted = true;
    }
  }
  const auto text = [env](jbyteArray array) {
    std::string value;
    if (!array) return value;
    const auto size = env->GetArrayLength(array);
    if (size <= 0 || size > 4096) return value;
    value.resize(static_cast<ae::usize>(size));
    env->GetByteArrayRegion(array, 0, size, reinterpret_cast<jbyte *>(value.data()));
    return value;
  };
  reply.displayName = text(name);
  if (reply.diagnostic.empty()) reply.diagnostic = text(diagnostic);
  if (!reply.diagnostic.empty()) reply.accepted = false;

  std::lock_guard lock(mutex);
  if (!pending || static_cast<ae::u64>(token) != sequence) return;
  pending = false;
  result = std::move(reply);
}
