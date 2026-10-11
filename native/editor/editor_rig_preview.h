#pragma once
#include "runtime/rig_instance.h"

namespace ae::editor {
// The preview owns an evaluated copy. Editing references/controls does not
// mutate the deformation skin or the authoring scene behind the user's back.
class EditorRigPreview final {
public:
  bool begin(const runtime::SceneGraph &,runtime::ObjectId root,const resources::RigAsset &,u64 epoch,std::string &error);
  bool control(u64,const resources::RigTransform &,std::string &error);
  bool mirror(u8 channels,bool referenceMode,std::string &error);
  bool pose(const resources::RigPose &,float weight,std::string &error,bool acceptRevisedRig=false);
  bool changes(const runtime::SceneGraph &live,u64 epoch,std::vector<runtime::RigSceneChange> &,std::string &error) const;
  void cancel();
  bool active() const {return owner_!=0;}
  bool pending() const;
  const runtime::SceneGraph &scene() const {return scene_;}
  const runtime::RigBindings &bindings() const {return bindings_;}
  const resources::CompiledRig &compiled() const {return compiled_;}
  const resources::RigAsset &rig() const {return asset_;}
  const std::vector<resources::RigTransform> &localPose() const {return local_;}
private:
  bool publish(std::vector<resources::RigTransform>,std::span<const u8>,std::string &error);
  runtime::SceneGraph scene_;
  resources::RigAsset asset_;
  resources::CompiledRig compiled_;
  runtime::RigBindings bindings_;
  runtime::ObjectId owner_=0;
  u64 epoch_=0;
  std::vector<resources::RigTransform> local_;
  std::vector<runtime::Transform> original_;
  std::vector<u8> mask_;
};
} // namespace ae::editor
