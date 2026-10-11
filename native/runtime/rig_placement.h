#pragma once
#include "resources/rig_asset.h"

namespace ae::runtime {

// Placement belongs to a binding, not the reusable rig or its deformation
// skeleton. All values here are in the actual root object's parent space.
// This also works for a rig nested under an independently moved scene object.
inline bool makeRigPlacement(const resources::RigTransform &instanceRoot,
                             const resources::RigTransform &referenceRoot,
                             resources::RigTransform &out) {
  if(!resources::validRigTransform(instanceRoot)||!resources::validRigTransform(referenceRoot))return false;
  const resources::RigTransform identity;
  const auto inverse=resources::relativeRigTransform(identity,referenceRoot);
  const auto placement=resources::composeRigTransform(instanceRoot,inverse);
  if(!resources::validRigTransform(placement))return false;
  out=placement;return true;
}

inline bool rigRootToResource(const resources::RigTransform &sceneRoot,
                              const resources::RigTransform &placement,
                              resources::RigTransform &out) {
  if(!resources::validRigTransform(sceneRoot)||!resources::validRigTransform(placement))return false;
  const auto value=resources::relativeRigTransform(sceneRoot,placement);
  if(!resources::validRigTransform(value))return false;
  out=value;return true;
}

inline bool rigRootToScene(const resources::RigTransform &resourceRoot,
                           const resources::RigTransform &placement,
                           resources::RigTransform &out) {
  if(!resources::validRigTransform(resourceRoot)||!resources::validRigTransform(placement))return false;
  const auto value=resources::composeRigTransform(placement,resourceRoot);
  if(!resources::validRigTransform(value))return false;
  out=value;return true;
}

} // namespace ae::runtime
