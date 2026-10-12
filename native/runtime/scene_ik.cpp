#include "runtime/scene_ik_impl.h"

namespace ae::runtime {
SceneIk::SceneIk():solver_(std::make_unique<SceneIkSolver>()) {}
SceneIk::~SceneIk()=default;
const std::vector<SceneIkTypes::Diagnostic> &SceneIk::diagnostics() const {return solver_->diagnostics();}
void SceneIk::reset() {solver_->reset();}
bool SceneIk::advance(GameWorld &world,const GroundQuery &ground) {return solver_->advance(world,ground);}
WorldStatus SceneIk::hit(GameWorld &world,ObjectId object,const float direction[3],float force) {return solver_->hit(world,object,direction,force);}
WorldStatus SceneIk::fire(GameWorld &world,ObjectId object,float magnitude) {return solver_->fire(world,object,magnitude);}
WorldStatus SceneIk::interactionStart(GameWorld &world,ObjectId system,ObjectId effector,ObjectId object,bool &started) {return solver_->interactionStart(world,system,effector,object,started);}
WorldStatus SceneIk::interactionStop(GameWorld &world,ObjectId system,ObjectId effector) {return solver_->interactionStop(world,system,effector);}
void SceneIk::collision(GameWorld &world,ObjectId object,const float normal[3]) {solver_->collision(world,object,normal);}
std::vector<SceneIkTypes::Event> SceneIk::takeEvents() {return solver_->takeEvents();}
} // namespace ae::runtime
