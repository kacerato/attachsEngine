// Fila de links externos pedidos pelo editor (ajuda do componente), consultada
// por dev.aether.editor.ExternalLinks na thread principal Java.
#include "platform/android/android_external_links.h"

#include <jni.h>
#include <mutex>

namespace {
std::mutex mutex;
std::string pending;
} // namespace

namespace ae::platform::android {
void requestExternalLink(std::string link) {
  // Só https atravessa: o editor não pede outro esquema, e um link vindo de dado
  // autoral não pode virar `intent:` ou `file:` no sistema.
  if (link.rfind("https://", 0) != 0 || link.size() > 2048) return;
  std::lock_guard lock(mutex);
  pending = std::move(link);
}
} // namespace ae::platform::android

extern "C" JNIEXPORT jbyteArray JNICALL
Java_dev_aether_editor_ExternalLinks_poll(JNIEnv *env, jclass) {
  std::string link;
  {
    std::lock_guard lock(mutex);
    link.swap(pending);
  }
  if (link.empty()) return nullptr;
  auto array = env->NewByteArray(static_cast<jsize>(link.size()));
  if (array) env->SetByteArrayRegion(array, 0, static_cast<jsize>(link.size()), reinterpret_cast<const jbyte *>(link.data()));
  return array;
}
