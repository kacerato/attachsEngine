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

void cancelModelPick() {
  std::lock_guard lock(mutex);pending=false;++sequence;result.reset();
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

extern "C" JNIEXPORT void JNICALL
Java_dev_aether_editor_ModelPicker_submitMany(JNIEnv *env, jclass, jlong token, jobjectArray contents,
                                              jobjectArray names, jbyteArray diagnostic) {
  ae::platform::android::ModelPickerResult reply;
  const auto text = [env](jbyteArray array) {
    std::string value;
    if (!array) return value;
    const auto size = env->GetArrayLength(array);
    if (size <= 0 || size > 4096) return value;
    value.resize(static_cast<ae::usize>(size));
    env->GetByteArrayRegion(array, 0, size, reinterpret_cast<jbyte *>(value.data()));
    return value;
  };
  reply.diagnostic = text(diagnostic);
  if (reply.diagnostic.empty() && contents && names) {
    const auto count = env->GetArrayLength(contents);
    if (count <= 0 || count != env->GetArrayLength(names) || count > 64) {
      reply.diagnostic = "Seleção de arquivos inválida.";
    } else {
      std::vector<ae::platform::android::ModelPickerResult::Companion> files;
      ae::u64 total = 0;
      for (jsize i = 0; i < count && reply.diagnostic.empty(); ++i) {
        const auto content = static_cast<jbyteArray>(env->GetObjectArrayElement(contents, i));
        const auto name = static_cast<jbyteArray>(env->GetObjectArrayElement(names, i));
        const auto size = content ? env->GetArrayLength(content) : -1;
        total += size > 0 ? static_cast<ae::u64>(size) : 0;
        if (size < 0 || total > static_cast<ae::u64>(kMaximumBytes)) {
          reply.diagnostic = "Os arquivos escolhidos juntos são grandes demais para esta importação.";
        } else {
          ae::platform::android::ModelPickerResult::Companion file;
          file.name = text(name);
          file.bytes.resize(static_cast<ae::usize>(size));
          if (size) env->GetByteArrayRegion(content, 0, size, reinterpret_cast<jbyte *>(file.bytes.data()));
          files.push_back(std::move(file));
        }
        if (content) env->DeleteLocalRef(content);
        if (name) env->DeleteLocalRef(name);
      }
      // Principal: o único .gltf; sem .gltf, o único .glb. Dois candidatos não
      // são escolhidos por adivinhação.
      const auto lower = [](std::string value) {
        for (auto &c : value) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        return value;
      };
      int main = -1, gltf = 0, glb = 0;
      for (ae::usize i = 0; i < files.size(); ++i) gltf += lower(files[i].name).ends_with(".gltf");
      for (ae::usize i = 0; i < files.size(); ++i) glb += lower(files[i].name).ends_with(".glb");
      const std::string wanted = gltf ? ".gltf" : ".glb";
      if (reply.diagnostic.empty() && ((gltf ? gltf : glb) != 1))
        reply.diagnostic = gltf || glb ? "Escolha um único arquivo principal (.gltf ou .glb) junto com as dependências dele."
                                       : "Nenhum .gltf ou .glb entre os arquivos escolhidos.";
      for (ae::usize i = 0; reply.diagnostic.empty() && i < files.size(); ++i)
        if (lower(files[i].name).ends_with(wanted)) main = static_cast<int>(i);
      if (reply.diagnostic.empty() && main >= 0) {
        reply.bytes = std::move(files[static_cast<ae::usize>(main)].bytes);
        reply.displayName = files[static_cast<ae::usize>(main)].name;
        for (ae::usize i = 0; i < files.size(); ++i)
          if (static_cast<int>(i) != main) reply.companions.push_back(std::move(files[i]));
        reply.accepted = true;
      }
    }
  }
  if (!reply.diagnostic.empty()) {
    reply.accepted = false;
    reply.bytes.clear();
    reply.companions.clear();
  }

  std::lock_guard lock(mutex);
  if (!pending || static_cast<ae::u64>(token) != sequence) return;
  pending = false;
  result = std::move(reply);
}
