#pragma once
#include "runtime/rig_instance.h"
#include "scene/rig_instance.h"

namespace ae::runtime {
// Explicit initial bind/rebind command; never called automatically when a
// preview opens or an animation samples. Caller publishes the component with
// its rig candidate in the same scene/resource History transaction.
bool prepareRigInstancePlacement(const resources::RigAsset &,
                                 const resources::CompiledRig &,
                                 const SceneGraph &,ObjectId root,
                                 scene::RigInstance &,std::string &error);
// Cheap guard for a previously authored placement. A hierarchy move or changed
// reference revision requires an explicit rebind, not an inferred new base.
bool validateRigInstancePlacement(const resources::CompiledRig &,
                                  const SceneGraph &,ObjectId root,
                                  const scene::RigInstance &,std::string &error);
// Revalidate a newer resource revision while preserving placement AND motion
// of the instance. This is distinct from explicitly establishing a new base.
// Missing non-root joints remain localized binding issues for masked edits.
bool prepareRigInstanceRevalidation(const resources::RigAsset &,
                                    const resources::CompiledRig &,
                                    const SceneGraph &,ObjectId root,
                                    const scene::RigInstance &previous,
                                    scene::RigInstance &,RigBindings &,
                                    std::string &error);
} // namespace ae::runtime
