// Host-only CLR/native integration fixture. Uses the production session,
// resources, ABI and compositor; never linked into the Android app.
#include "editor/editor_animation_authoring.h"
#include "editor/editor_session.h"
#include "runtime/scene_animation.h"
#include "scene/light.h"
#include "scene/animator.h"
#include <limits>
#include <cstring>
#include <array>
#include <cstddef>

#define AE_AUTHOR_TEST_EXPORT extern "C" __declspec(dllexport)
namespace {
struct Fixture {
  ae::editor::EditorSession session;
  std::unique_ptr<ae::editor::AnimationAuthoringScope> scope;
  ae::editor::EditorEntityId owner=0,child=0;
  alignas(ae::editor::AnimationAuthorAccess) std::array<ae::u8,offsetof(ae::editor::AnimationAuthorAccess,bake)> abi1;
  alignas(ae::editor::AnimationAuthorAccess) std::array<ae::u8,offsetof(ae::editor::AnimationAuthorAccess,sampleComposed)> abi2;
  alignas(ae::editor::AnimationAuthorAccess) std::array<ae::u8,offsetof(ae::editor::AnimationAuthorAccess,bakeAdvanced)> abi3;
  alignas(ae::editor::AnimationAuthorAccess) std::array<ae::u8,offsetof(ae::editor::AnimationAuthorAccess,previewClip)> abi4;
};
}
AE_AUTHOR_TEST_EXPORT void *author_fixture_create(const ae::u8 *root,int length) {
  if(!root||length<=0||length>32768)return nullptr;
  {
    auto fixture=std::make_unique<Fixture>();std::string path(reinterpret_cast<const char*>(root),static_cast<ae::usize>(length));
    if(!fixture->session.setProjectDirectory(path.c_str()))return nullptr;
    fixture->owner=fixture->session.document().createEntity(fixture->session.document().root(),ae::runtime::ObjectKind::Folder,"Rotor");
    fixture->child=fixture->session.document().createEntity(fixture->owner,ae::runtime::ObjectKind::Folder,"A/B");
    auto authored=*fixture->session.document().find(fixture->owner);
    auto *light=static_cast<ae::scene::Light*>(authored.components.add(ae::scene::Light::descriptor));light->intensity=1;
    auto *animator=static_cast<ae::scene::Animator*>(authored.components.add(ae::scene::Animator::descriptor));
    ae::scene::AnimatorParameter speed;speed.id=animator->allocateId();speed.name="Velocidade";speed.value=3;animator->parameters.push_back(speed);
    if(!fixture->session.document().applyEntityValues(fixture->owner,authored))return nullptr;
    fixture->session.setSelection(fixture->owner);return fixture.release();
  }
}
AE_AUTHOR_TEST_EXPORT const ae::editor::AnimationAuthorAccess *author_fixture_access(void *handle) {
  if(!handle)return nullptr;
  auto &fixture=*static_cast<Fixture*>(handle);
  fixture.scope=std::make_unique<ae::editor::AnimationAuthoringScope>(fixture.session);return &fixture.scope->access();
}
AE_AUTHOR_TEST_EXPORT void author_fixture_end(void *handle) {if(handle)static_cast<Fixture*>(handle)->scope.reset();}
AE_AUTHOR_TEST_EXPORT const void *author_fixture_access_v1(void *handle) {
  const auto *access=author_fixture_access(handle);if(!access)return nullptr;
  auto &fixture=*static_cast<Fixture*>(handle);auto legacy=*access;legacy.version=1;legacy.size=fixture.abi1.size();
  std::memcpy(fixture.abi1.data(),&legacy,fixture.abi1.size());return fixture.abi1.data();
}
AE_AUTHOR_TEST_EXPORT const void *author_fixture_access_v2(void *handle) {
  const auto *access=author_fixture_access(handle);if(!access)return nullptr;
  auto &fixture=*static_cast<Fixture*>(handle);auto legacy=*access;legacy.version=2;legacy.size=fixture.abi2.size();
  std::memcpy(fixture.abi2.data(),&legacy,fixture.abi2.size());return fixture.abi2.data();
}
AE_AUTHOR_TEST_EXPORT void author_fixture_destroy(void *handle) {delete static_cast<Fixture*>(handle);}
AE_AUTHOR_TEST_EXPORT const void *author_fixture_access_v3(void *handle) {
  const auto *access=author_fixture_access(handle);if(!access)return nullptr;
  auto &fixture=*static_cast<Fixture*>(handle);auto legacy=*access;legacy.version=3;legacy.size=fixture.abi3.size();
  std::memcpy(fixture.abi3.data(),&legacy,fixture.abi3.size());return fixture.abi3.data();
}
AE_AUTHOR_TEST_EXPORT int author_fixture_history(void *handle,int action) {
  if(!handle)return -1;
  auto &fixture=*static_cast<Fixture*>(handle);
  if(action==1)return fixture.session.history().undo(fixture.session.document())?1:0;
  if(action==2)return fixture.session.history().redo(fixture.session.document())?1:0;
  return static_cast<int>(fixture.session.history().undoDepth());
}
AE_AUTHOR_TEST_EXPORT const void *author_fixture_access_v4(void *handle) {
  const auto *access=author_fixture_access(handle);if(!access)return nullptr;
  auto &fixture=*static_cast<Fixture*>(handle);auto legacy=*access;legacy.version=4;legacy.size=fixture.abi4.size();
  std::memcpy(fixture.abi4.data(),&legacy,fixture.abi4.size());return fixture.abi4.data();
}
AE_AUTHOR_TEST_EXPORT const void *author_fixture_access_v5(void *handle) {
  const auto *access=author_fixture_access(handle);if(!access)return nullptr;
  // ABI 6 appends commands, not function pointers. Keep an independent legacy
  // table so changing its advertised version cannot mutate the production scope.
  static thread_local ae::editor::AnimationAuthorAccess legacy;
  legacy=*access;legacy.version=5;legacy.size=offsetof(ae::editor::AnimationAuthorAccess,copyCues);return &legacy;
}
AE_AUTHOR_TEST_EXPORT const void *author_fixture_access_v7(void *handle) {
  const auto *access=author_fixture_access(handle);if(!access)return nullptr;
  static thread_local ae::editor::AnimationAuthorAccess legacy;
  legacy=*access;legacy.version=7;legacy.size=offsetof(ae::editor::AnimationAuthorAccess,configurePoseRecording);return &legacy;
}
AE_AUTHOR_TEST_EXPORT const void *author_fixture_access_v8(void *handle) {
  const auto *access=author_fixture_access(handle);if(!access)return nullptr;
  static thread_local ae::editor::AnimationAuthorAccess legacy;
  legacy=*access;legacy.version=8;legacy.size=offsetof(ae::editor::AnimationAuthorAccess,poseSelection);return &legacy;
}
AE_AUTHOR_TEST_EXPORT int author_fixture_sample(void *handle,float time,float *values) {
  if(!handle||!values)return 0;
  auto &fixture=*static_cast<Fixture*>(handle);
  for(const auto &entry:fixture.session.assets().records())if(fixture.session.animationClipAsset(entry.guid)) {
    ae::runtime::SceneAnimator animator;animator.begin(fixture.session.document(),fixture.session.mapScene());
    ae::runtime::SceneAnimator::ExternalSample sample;sample.owner=fixture.owner;sample.clip=entry.guid;sample.time=time;sample.weight=1;
    animator.setExternalSamples({sample});if(!animator.advance(0,{}))return 0;
    values[0]=fixture.session.document().find(fixture.owner)->transform.position[0];
    values[1]=fixture.session.document().find(fixture.child)->transform.position[0];return 1;
  }
  return 0;
}
AE_AUTHOR_TEST_EXPORT float author_fixture_property(void *handle,float time) {
  if(handle) {
    auto &fixture=*static_cast<Fixture*>(handle);
    for(const auto &entry:fixture.session.assets().records())if(fixture.session.animationClipAsset(entry.guid)) {
      auto graph=fixture.session.document();ae::runtime::SceneAnimator animator;animator.begin(graph,fixture.session.mapScene());
      ae::runtime::SceneAnimator::ExternalSample sample;sample.owner=fixture.owner;sample.clip=entry.guid;sample.time=time;sample.weight=1;
      animator.setExternalSamples({sample});if(!animator.advance(0,{}))break;
      const auto *component=graph.find(fixture.owner)->components.find(ae::scene::Light::descriptor);
      if(component)return static_cast<const ae::scene::Light*>(component)->intensity;
    }
  }
  return std::numeric_limits<float>::quiet_NaN();
}
AE_AUTHOR_TEST_EXPORT const void *author_fixture_access_v9(void *handle) {
  const auto *access=author_fixture_access(handle);if(!access)return nullptr;
  static thread_local ae::editor::AnimationAuthorAccess legacy;
  legacy=*access;legacy.version=9;legacy.size=offsetof(ae::editor::AnimationAuthorAccess,arrangeSelection);return &legacy;
}
AE_AUTHOR_TEST_EXPORT const void *author_fixture_access_v10(void *handle) {
  const auto *access=author_fixture_access(handle);if(!access)return nullptr;
  static thread_local ae::editor::AnimationAuthorAccess legacy;
  legacy=*access;legacy.version=10;legacy.size=offsetof(ae::editor::AnimationAuthorAccess,sampleRegion);return &legacy;
}
AE_AUTHOR_TEST_EXPORT int author_fixture_layout(int index) {
  switch(index) {
    case 0:return sizeof(ae::editor::AnimationAuthorAccess);
    case 1:return sizeof(ae::editor::AnimationAuthorKey);
    case 2:return sizeof(ae::editor::AnimationAuthorCommand);
    case 3:return sizeof(ae::editor::AnimationAuthorAddress);
    case 4:return sizeof(ae::editor::AnimationAuthorBakeSettings);
    case 5:return sizeof(ae::editor::AnimationAuthorBakeReport);
    case 6:return sizeof(ae::editor::AnimationAuthorBakeRequest);
    case 7:return sizeof(ae::editor::AnimationAuthorProperty);
    case 8:return sizeof(ae::editor::AnimationAuthorPropertyValue);
    case 9:return sizeof(ae::editor::AnimationAuthorDriverParameter);
    default:return -1;
  }
}
