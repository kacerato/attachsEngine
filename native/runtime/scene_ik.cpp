#include "runtime/scene_ik_impl.h"

namespace ae::runtime {
SceneIk::SceneIk():solver_(std::make_unique<SceneIkSolver>()) {}
SceneIk::~SceneIk()=default;
const std::vector<SceneIkTypes::Diagnostic> &SceneIk::diagnostics() const {return solver_->diagnostics();}
void SceneIk::reset() {solver_->reset();}
bool SceneIk::advance(GameWorld &world,const GroundQuery &ground) {return solver_->advance(world,ground);}
} // namespace ae::runtime
