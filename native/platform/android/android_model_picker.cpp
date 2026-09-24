#include "platform/android/android_model_picker.h"

#include <algorithm>
#include <jni.h>
#include <mutex>
#include <optional>

namespace {
std::mutex mutex;
// `sequence` é o que impede uma resposta atrasada de um pedido cancelado ser
// aplicada ao pedido seguinte.
ae::u64 sequence = 0;
bool pending = false;
bool allowCompanions = true;
bool folderMode = false;

// Cópia de pasta (S0): um pedido por vez, com o próprio número de sequência.
struct CopyJob {
  ae::u64 token = 0;
  bool active = false, cancel = false;
  ae::platform::android::FolderCopyRequest request;
  ae::platform::android::FolderCopyState state;
};
std::mutex copyMutex;
ae::u64 copySequence = 0;
CopyJob copyJob;
std::optional<ae::platform::android::ModelPickerResult> result;
// Teto do que atravessa a fronteira JNI de uma vez. Um GLB maior que isto não é
// recusado por capricho: ele seria copiado inteiro para a heap do processo três
// vezes (array Java, cópia nativa, projeto) antes de qualquer validação.
constexpr jsize kMaximumBytes = 128 * 1024 * 1024;
} // namespace

namespace ae::platform::android {

void requestFolderPick() {
  std::lock_guard lock(mutex);
  pending = true;
  allowCompanions = false;
  folderMode = true;
  ++sequence;
  result.reset();
}

u64 requestFolderCopy(FolderCopyRequest request) {
  std::lock_guard lock(copyMutex);
  copyJob = {};
  copyJob.token = ++copySequence;
  copyJob.active = true;
  copyJob.state.totalBytes = request.totalBytes;
  copyJob.request = std::move(request);
  return copyJob.token;
}

bool folderCopyState(u64 token, FolderCopyState &out) {
  std::lock_guard lock(copyMutex);
  if (token != copyJob.token) return false;
  out = copyJob.state;
  return true;
}

void cancelFolderCopy(u64 token) {
  std::lock_guard lock(copyMutex);
  if (token == copyJob.token) copyJob.cancel = true;
}

void requestModelPick(bool companions) {
  std::lock_guard lock(mutex);
  folderMode = false;
  // Pedir de novo enquanto um pedido está aberto NÃO é ignorado: o seletor pode
  // ter sido encerrado pelo sistema sem devolver nada, e o usuário ficaria com
  // um botão que não responde mais. O número de sequência novo faz a resposta
  // atrasada do pedido anterior ser descartada.
  pending = true;
  allowCompanions = companions;
  ++sequence;
  result.reset();
}

void cancelModelPick() {
  std::lock_guard lock(mutex);pending=false;folderMode=false;++sequence;result.reset();
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

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_aether_editor_ModelPicker_allowMultiple(JNIEnv *, jclass, jlong token) {
  std::lock_guard lock(mutex);
  return pending && static_cast<ae::u64>(token)==sequence && allowCompanions ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_aether_editor_ModelPicker_submitMany(JNIEnv *env, jclass, jlong token, jobjectArray contents,
                                              jobjectArray names, jbyteArray diagnostic) {
  {
    std::lock_guard lock(mutex);
    if(!pending || static_cast<ae::u64>(token)!=sequence) return;
    if(!allowCompanions) {
      ae::platform::android::ModelPickerResult rejected;
      rejected.diagnostic="Escolha uma única imagem por importação.";
      result=std::move(rejected);pending=false;return;
    }
  }
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

namespace {
std::string jniText(JNIEnv *env, jbyteArray array, jsize maximum = 4096) {
  std::string value;
  if (!array) return value;
  const auto size = env->GetArrayLength(array);
  if (size <= 0 || size > maximum) return value;
  value.resize(static_cast<ae::usize>(size));
  env->GetByteArrayRegion(array, 0, size, reinterpret_cast<jbyte *>(value.data()));
  return value;
}
jobjectArray jniTexts(JNIEnv *env, const std::vector<std::string> &values) {
  const auto type = env->FindClass("[B");
  if (!type) return nullptr;
  auto array = env->NewObjectArray(static_cast<jsize>(values.size()), type, nullptr);
  for (jsize i = 0; array && i < static_cast<jsize>(values.size()); ++i) {
    const auto &value = values[static_cast<ae::usize>(i)];
    auto bytes = env->NewByteArray(static_cast<jsize>(value.size()));
    if (!bytes) return nullptr;
    env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(value.size()), reinterpret_cast<const jbyte *>(value.data()));
    env->SetObjectArrayElement(array, i, bytes);
    env->DeleteLocalRef(bytes);
  }
  return array;
}
} // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_aether_editor_ModelPicker_pickFolder(JNIEnv *, jclass, jlong token) {
  std::lock_guard lock(mutex);
  return pending && static_cast<ae::u64>(token) == sequence && folderMode ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_aether_editor_ModelPicker_submitFolder(JNIEnv *env, jclass, jlong token, jobjectArray paths, jlongArray sizes,
                                                jobjectArray mainPaths, jobjectArray mainContents, jbyteArray folderName,
                                                jbyteArray diagnostic) {
  ae::platform::android::ModelPickerResult reply;
  reply.folder = true;
  reply.diagnostic = jniText(env, diagnostic);
  reply.folderName = jniText(env, folderName, 1024);
  if (reply.diagnostic.empty() && paths && sizes && mainPaths && mainContents) {
    const auto count = env->GetArrayLength(paths);
    if (count <= 0 || count != env->GetArrayLength(sizes) || count > 8192) {
      reply.diagnostic = "Listagem da pasta inválida.";
    } else {
      std::vector<jlong> lengths(static_cast<ae::usize>(count));
      env->GetLongArrayRegion(sizes, 0, count, lengths.data());
      for (jsize i = 0; i < count; ++i) {
        const auto path = static_cast<jbyteArray>(env->GetObjectArrayElement(paths, i));
        reply.folderFiles.push_back({jniText(env, path, 2048), lengths[static_cast<ae::usize>(i)]});
        if (path) env->DeleteLocalRef(path);
      }
      // Principal: entre os candidatos, os de MENOR profundidade; exatamente um.
      // Escolher o `.gltf` mais raso cobre quem escolhe a pasta acima da do
      // modelo (o zip do Sponza traz `main_sponza/…gltf`).
      const auto mains = env->GetArrayLength(mainPaths);
      std::vector<std::pair<std::string, jsize>> candidates;
      for (jsize i = 0; i < mains; ++i) {
        const auto path = static_cast<jbyteArray>(env->GetObjectArrayElement(mainPaths, i));
        candidates.emplace_back(jniText(env, path, 2048), i);
        if (path) env->DeleteLocalRef(path);
      }
      const auto depth = [](const std::string &path) { return std::count(path.begin(), path.end(), '/'); };
      long shallowest = -1;
      for (const auto &[path, index] : candidates)
        if (shallowest < 0 || depth(path) < shallowest) shallowest = depth(path);
      std::vector<std::pair<std::string, jsize>> chosen;
      for (const auto &candidate : candidates)
        if (depth(candidate.first) == shallowest) chosen.push_back(candidate);
      if (chosen.empty()) {
        reply.diagnostic = "Nenhum .gltf ou .glb nesta pasta.";
      } else if (chosen.size() > 1) {
        reply.diagnostic = "A pasta tem " + std::to_string(chosen.size()) + " arquivos principais (";
        for (ae::usize i = 0; i < chosen.size() && i < 4; ++i) reply.diagnostic += (i ? ", " : "") + chosen[i].first;
        reply.diagnostic += chosen.size() > 4 ? ", …)" : ")";
        reply.diagnostic += "; escolha a pasta de um único modelo.";
      } else {
        const auto content = static_cast<jbyteArray>(env->GetObjectArrayElement(mainContents, chosen[0].second));
        const auto size = content ? env->GetArrayLength(content) : -1;
        if (size <= 0 || size > kMaximumBytes) {
          reply.diagnostic = "O arquivo principal da pasta não pôde ser lido.";
        } else {
          reply.bytes.resize(static_cast<ae::usize>(size));
          env->GetByteArrayRegion(content, 0, size, reinterpret_cast<jbyte *>(reply.bytes.data()));
          reply.mainRelative = chosen[0].first;
          const auto slash = reply.mainRelative.rfind('/');
          reply.displayName = slash == std::string::npos ? reply.mainRelative : reply.mainRelative.substr(slash + 1);
          reply.accepted = true;
        }
        if (content) env->DeleteLocalRef(content);
      }
    }
  }
  if (!reply.diagnostic.empty()) {
    reply.accepted = false;
    reply.bytes.clear();
  }
  std::lock_guard lock(mutex);
  if (!pending || static_cast<ae::u64>(token) != sequence) return;
  pending = false;
  folderMode = false;
  result = std::move(reply);
}

extern "C" JNIEXPORT jlong JNICALL
Java_dev_aether_editor_ModelPicker_pollCopy(JNIEnv *, jclass) {
  std::lock_guard lock(copyMutex);
  return copyJob.active && !copyJob.state.finished ? static_cast<jlong>(copyJob.token) : 0;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_dev_aether_editor_ModelPicker_copySources(JNIEnv *env, jclass, jlong token) {
  std::vector<std::string> values;
  {
    std::lock_guard lock(copyMutex);
    if (static_cast<ae::u64>(token) != copyJob.token) return nullptr;
    values = copyJob.request.sources;
  }
  return jniTexts(env, values);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_dev_aether_editor_ModelPicker_copyTargets(JNIEnv *env, jclass, jlong token) {
  std::vector<std::string> values;
  {
    std::lock_guard lock(copyMutex);
    if (static_cast<ae::u64>(token) != copyJob.token) return nullptr;
    values = copyJob.request.targets;
  }
  return jniTexts(env, values);
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_dev_aether_editor_ModelPicker_copyDestination(JNIEnv *env, jclass, jlong token) {
  std::string value;
  {
    std::lock_guard lock(copyMutex);
    if (static_cast<ae::u64>(token) != copyJob.token) return nullptr;
    value = copyJob.request.destination;
  }
  auto bytes = env->NewByteArray(static_cast<jsize>(value.size()));
  if (bytes) env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(value.size()), reinterpret_cast<const jbyte *>(value.data()));
  return bytes;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_aether_editor_ModelPicker_copyCancelled(JNIEnv *, jclass, jlong token) {
  std::lock_guard lock(copyMutex);
  return static_cast<ae::u64>(token) != copyJob.token || copyJob.cancel ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_aether_editor_ModelPicker_copyProgress(JNIEnv *, jclass, jlong token, jlong done, jlong, jint files) {
  std::lock_guard lock(copyMutex);
  if (static_cast<ae::u64>(token) != copyJob.token) return;
  copyJob.state.doneBytes = done > 0 ? static_cast<ae::u64>(done) : 0;
  copyJob.state.files = files > 0 ? static_cast<ae::u32>(files) : 0;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_aether_editor_ModelPicker_submitCopy(JNIEnv *env, jclass, jlong token, jobjectArray hashes, jbyteArray diagnostic) {
  ae::platform::android::FolderCopyState finished;
  finished.finished = true;
  finished.diagnostic = jniText(env, diagnostic);
  std::lock_guard lock(copyMutex);
  if (static_cast<ae::u64>(token) != copyJob.token) return;
  finished.doneBytes = copyJob.state.doneBytes;
  finished.totalBytes = copyJob.state.totalBytes;
  finished.files = copyJob.state.files;
  if (!hashes && finished.diagnostic.empty()) finished.cancelled = true;
  if (hashes) {
    const auto count = env->GetArrayLength(hashes);
    if (count != static_cast<jsize>(copyJob.request.sources.size())) {
      finished.diagnostic = "Cópia devolveu um número de arquivos diferente do pedido.";
    } else {
      for (jsize i = 0; i < count; ++i) {
        const auto hash = static_cast<jbyteArray>(env->GetObjectArrayElement(hashes, i));
        finished.sha256.push_back(jniText(env, hash, 64));
        if (hash) env->DeleteLocalRef(hash);
      }
    }
  }
  copyJob.state = std::move(finished);
  copyJob.active = false;
}
