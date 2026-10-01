#include "scene/physics2d_event_connection.h"
#include "runtime/scene_physics2d.h"
#include "scene/physics2d_components.h"
#include "runtime/physics_field2d_sample.h"
#include <box2d/box2d.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <limits>
namespace ae::runtime {
namespace {constexpr float radians=.017453292519943295f;bool finite2(const float v[2]){return v&&std::isfinite(v[0])&&std::isfinite(v[1]);}}
struct ScenePhysics2D::Impl {
 struct Shape {ObjectId object;u64 instance;u32 layer;bool sensor;scene::Body2DMotion motion;};
 struct Body {b2BodyId id{};scene::Body2D authored{};std::string signature;std::vector<u64> shapes;float scaleX=1,scaleY=1;};
 struct Draft {ObjectId object;Transform pose;scene::Body2D body;bool explicitBody=false;std::vector<scene::Collider2D> shapes;std::string signature;std::vector<scene::Joint2D> joints;};
 struct Joint {b2JointId id{};ObjectId owner=0,target=0;std::string signature;};
 std::map<std::pair<ObjectId,u64>,Joint> joints;b2BodyId ground{};
 b2WorldId solver{};u32 worldId=0;double accumulator=0;b2Vec2 gravity{0,-9.81f};std::string error;
 std::map<ObjectId,Body> bodies;std::map<u64,Shape> shapes;
 std::map<std::pair<u64,u64>,Physics2DEvent> touching;std::vector<Physics2DEvent> pending;
 u64 fieldRevision=std::numeric_limits<u64>::max();
 std::vector<std::pair<ObjectId,u64>> fieldCandidates;std::vector<PhysicsField2DFrame> fieldFrames;
 bool planar(const Transform &pose){return std::abs(std::remainder(pose.rotationDegrees[0],360.f))<.001f&&std::abs(std::remainder(pose.rotationDegrees[1],360.f))<.001f&&pose.scale[0]>.00001f&&pose.scale[1]>.00001f;}
 bool accept(b2ShapeId id,const Physics2DFilter &f) const {
  auto it=shapes.find(b2StoreShapeId(id));if(it==shapes.end())return false;const auto &s=it->second;
  return s.object!=f.ignore&&(f.layerMask&(1u<<s.layer))&&(!s.sensor||f.includeSensors)&&(s.motion==scene::Body2DMotion::Static?f.includeStatic:f.includeDynamic);
 }
 Physics2DHit hit(b2ShapeId id,b2Vec2 point={},b2Vec2 normal={},float fraction=0) const {
  const auto &s=shapes.at(b2StoreShapeId(id));Physics2DHit h;h.object=s.object;h.colliderInstance=s.instance;h.sensor=s.sensor;h.point[0]=point.x;h.point[1]=point.y;h.normal[0]=normal.x;h.normal[1]=normal.y;h.fraction=fraction;return h;
 }
 void retire(ObjectId object,GameWorld *world){
  for(auto it=joints.begin();it!=joints.end();)if(it->second.owner==object||it->second.target==object){b2DestroyJoint(it->second.id);it=joints.erase(it);}else ++it;
  auto body=bodies.find(object);if(body==bodies.end())return;
  for(auto it=touching.begin();it!=touching.end();)if(it->second.first==object||it->second.second==object){auto exit=it->second;exit.phase=2;exit.hasNormal=false;pending.push_back(exit);it=touching.erase(it);}else ++it;
  for(auto id:body->second.shapes){shapes.erase(id);}
  b2DestroyBody(body->second.id);bodies.erase(body);
  if(world&&world->running()&&world->worldId()==worldId&&world->authorityOf(world->handle(object))==TransformAuthority::PhysicsBody2D)world->setAuthority(object,TransformAuthority::Free);
 }
 void captureEvents(){
  std::set<std::pair<u64,u64>> entered;
  auto begin=[&](b2ShapeId a,b2ShapeId b,bool sensor,const b2Vec2 *normal){
   const u64 x=b2StoreShapeId(a),y=b2StoreShapeId(b);auto i=shapes.find(x),j=shapes.find(y);if(i==shapes.end()||j==shapes.end())return;
   const auto key=std::minmax(x,y);Physics2DEvent e;e.first=i->second.object;e.second=j->second.object;e.firstCollider=i->second.instance;e.secondCollider=j->second.instance;e.sensor=sensor;e.phase=0;e.hasNormal=normal!=nullptr;if(normal){e.normal[0]=normal->x;e.normal[1]=normal->y;}
   if(!touching.contains(key)){touching[key]=e;pending.push_back(e);entered.insert(key);}
  };
  auto end=[&](b2ShapeId a,b2ShapeId b){const u64 x=b2StoreShapeId(a),y=b2StoreShapeId(b);const std::pair<u64,u64> key{std::min(x,y),std::max(x,y)};auto it=touching.find(key);if(it!=touching.end()){auto e=it->second;e.phase=2;e.hasNormal=false;pending.push_back(e);touching.erase(it);}};
  const auto sensor=b2World_GetSensorEvents(solver);for(int i=0;i<sensor.beginCount;++i)begin(sensor.beginEvents[i].sensorShapeId,sensor.beginEvents[i].visitorShapeId,true,nullptr);for(int i=0;i<sensor.endCount;++i)end(sensor.endEvents[i].sensorShapeId,sensor.endEvents[i].visitorShapeId);
  const auto contacts=b2World_GetContactEvents(solver);for(int i=0;i<contacts.beginCount;++i)begin(contacts.beginEvents[i].shapeIdA,contacts.beginEvents[i].shapeIdB,false,&contacts.beginEvents[i].manifold.normal);for(int i=0;i<contacts.endCount;++i)end(contacts.endEvents[i].shapeIdA,contacts.endEvents[i].shapeIdB);
  for(const auto &[key,value]:touching)if(!entered.contains(key)){auto e=value;e.phase=1;e.hasNormal=false;pending.push_back(e);}
 }
};
ScenePhysics2D::ScenePhysics2D():impl_(std::make_unique<Impl>()){}
ScenePhysics2D::~ScenePhysics2D(){stop();}
const std::string &ScenePhysics2D::error() const{return impl_->error;}
u32 ScenePhysics2D::bodyCount() const{return static_cast<u32>(impl_->bodies.size());}
u32 ScenePhysics2D::jointCount() const{return static_cast<u32>(impl_->joints.size());}
bool ScenePhysics2D::sleeping(ObjectId object) const{auto it=impl_->bodies.find(object);return it!=impl_->bodies.end()&&!b2Body_IsAwake(it->second.id);}
void ScenePhysics2D::stop(GameWorld *world){auto &p=*impl_;while(!p.bodies.empty())p.retire(p.bodies.begin()->first,world);if(b2World_IsValid(p.solver))b2DestroyWorld(p.solver);p.solver={};p.worldId=0;p.accumulator=0;p.shapes.clear();p.touching.clear();p.pending.clear();p.joints.clear();p.ground={};p.fieldCandidates.clear();p.fieldFrames.clear();p.fieldRevision=std::numeric_limits<u64>::max();}
void ScenePhysics2D::releaseObject(ObjectId object,GameWorld *world){impl_->retire(object,world);}
bool ScenePhysics2D::start(GameWorld &world){stop(&world);auto &p=*impl_;p.error.clear();if(!world.running()){p.error="Physics2D requires a running GameWorld";return false;}auto def=b2DefaultWorldDef();def.gravity=p.gravity;p.solver=b2CreateWorld(&def);p.worldId=world.worldId();if(!b2World_IsValid(p.solver)){p.error="Box2D world creation failed";return false;}if(!rebuild(world)){stop(&world);return false;}return true;}
bool ScenePhysics2D::rebuild(GameWorld &world){
 auto &p=*impl_;p.error.clear();if(!world.running()||world.worldId()!=p.worldId||!b2World_IsValid(p.solver)){p.error="Physics2D world mismatch";return false;}
 std::vector<ObjectId> objects;world.graph().collectSubtree(world.graph().root(),objects);std::vector<Impl::Draft> drafts;std::set<ObjectId> alive;
 for(auto object:objects){const auto *o=world.graph().find(object);if(!o)continue;Impl::Draft d;d.object=object;
  if(const auto *c=o->components.find(scene::Body2D::descriptor)){d.body=*static_cast<const scene::Body2D*>(c);d.explicitBody=true;}else d.body.motion=scene::Body2DMotion::Static;
  for(usize i=0;i<o->components.size();++i)if(o->components.at(i)->type().id==scene::Collider2D::descriptor.id)d.shapes.push_back(*static_cast<const scene::Collider2D*>(o->components.at(i)));
  for(usize i=0;i<o->components.size();++i)if(o->components.at(i)->type().id==scene::Joint2D::descriptor.id)d.joints.push_back(*static_cast<const scene::Joint2D*>(o->components.at(i)));
  if(const auto *f=o->components.find(scene::ConstantForce2D::descriptor);f&&static_cast<const scene::ConstantForce2D*>(f)->enabled&&d.body.motion!=scene::Body2DMotion::Dynamic){p.error="ConstantForce2D requires Dynamic Body2D";return false;}
  for(const auto &joint:d.joints)if(joint.enabled){
   if(!d.explicitBody){p.error="Joint2D requires explicit owner Body2D";return false;}
   const auto *target=world.graph().find(static_cast<ObjectId>(joint.target));const auto *targetBody=target?target->components.find(scene::Body2D::descriptor):nullptr;
   if(!joint.worldAnchor&&(!targetBody||joint.target==object)){p.error="Joint2D connected body is missing or self";return false;}
   if(d.body.motion!=scene::Body2DMotion::Dynamic&&(joint.worldAnchor||static_cast<const scene::Body2D*>(targetBody)->motion!=scene::Body2DMotion::Dynamic)){p.error="Joint2D needs at least one Dynamic body";return false;}
  }
  if(world.graph().activeInHierarchy(object))for(usize i=0;i<o->components.size();++i) {
   const auto *value=o->components.at(i);if(&value->type()!=&scene::PhysicsEventConnection2D::descriptor)continue;
   const auto &connection=static_cast<const scene::PhysicsEventConnection2D&>(*value);
   if(!connection.enabled||connection.action==0)continue;
   const bool sensorEvent=connection.event<3;
   const bool compatible=std::any_of(d.shapes.begin(),d.shapes.end(),[&](const auto &shape){return shape.sensor==sensorEvent;});
   if(!d.explicitBody||!compatible){p.error="Conexão física 2D requer Body2D e forma compatível com sensor/contato no objeto "+std::to_string(object);return false;}
  }
  if(d.shapes.empty()){if(d.explicitBody){p.error="Body2D requires a Collider2D on object "+std::to_string(object);return false;}continue;}
  if(world.worldTransform(world.handle(object),d.pose)!=WorldStatus::Ok||!p.planar(d.pose)){p.error="Physics2D rejects tilt, shear, zero scale or reflection on object "+std::to_string(object);return false;}
  const auto authority=world.authorityOf(world.handle(object));if(authority!=TransformAuthority::Free&&authority!=TransformAuthority::PhysicsBody2D){p.error="Physics2D cannot own a 3D/character pose";return false;}
  std::ostringstream signature;signature.precision(std::numeric_limits<float>::max_digits10);d.body.write(signature);signature<<' '<<d.explicitBody<<' '<<d.pose.scale[0]<<' '<<d.pose.scale[1]<<' '<<o->layer<<' '<<world.graph().layers().maskFor(o->layer);
  for(const auto &c:d.shapes){if(c.shape!=scene::Collider2DShape::Box&&std::abs(d.pose.scale[0]-d.pose.scale[1])>1e-5f){p.error="Circle/capsule need uniform XY world scale";return false;}signature<<' '<<c.instanceId()<<' ';c.write(signature);}
  d.signature=signature.str();alive.insert(object);drafts.push_back(std::move(d));
 }
 for(const auto &draft:drafts)for(const auto &joint:draft.joints){
  if(joint.enabled&&!joint.worldAnchor&&!alive.contains(static_cast<ObjectId>(joint.target))){p.error="Joint2D target has no executable shapes";return false;}
 }
 // All authoring constraints are validated before changing any live body.
 std::vector<ObjectId> removed;for(const auto &[id,b]:p.bodies)if(!alive.contains(id))removed.push_back(id);for(auto id:removed)p.retire(id,&world);
 for(const auto &d:drafts){auto old=p.bodies.find(d.object);if(old!=p.bodies.end()&&old->second.signature==d.signature)continue;
  b2Vec2 velocity{d.body.velocityX,d.body.velocityY};float angular=d.body.angularVelocityDegrees*radians;
  if(old!=p.bodies.end()&&old->second.authored.motion==d.body.motion){const auto &a=old->second.authored;if(a.velocityX==d.body.velocityX&&a.velocityY==d.body.velocityY)velocity=b2Body_GetLinearVelocity(old->second.id);if(a.angularVelocityDegrees==d.body.angularVelocityDegrees)angular=b2Body_GetAngularVelocity(old->second.id);}
  auto bd=b2DefaultBodyDef();bd.type=d.body.motion==scene::Body2DMotion::Static?b2_staticBody:d.body.motion==scene::Body2DMotion::Kinematic?b2_kinematicBody:b2_dynamicBody;bd.position={d.pose.position[0],d.pose.position[1]};bd.rotation=b2MakeRot(d.pose.rotationDegrees[2]*radians);bd.linearVelocity=velocity;bd.angularVelocity=angular;bd.linearDamping=d.body.linearDamping;bd.angularDamping=d.body.angularDamping;bd.gravityScale=d.body.gravityScale;bd.fixedRotation=d.body.fixedRotation;bd.enableSleep=d.body.allowSleep;bd.isEnabled=world.graph().activeInHierarchy(d.object);
  const auto newBody=b2CreateBody(p.solver,&bd);Impl::Body built;built.id=newBody;built.authored=d.body;built.signature=d.signature;built.scaleX=d.pose.scale[0];built.scaleY=d.pose.scale[1];const auto *owner=world.graph().find(d.object);
  for(const auto &c:d.shapes){auto sd=b2DefaultShapeDef();sd.density=1;sd.material.friction=c.friction;sd.material.restitution=c.restitution;sd.isSensor=c.sensor;sd.enableSensorEvents=true;sd.enableContactEvents=true;sd.filter.categoryBits=uint64_t(1)<<owner->layer;sd.filter.maskBits=world.graph().layers().maskFor(owner->layer);const b2Vec2 offset{c.offsetX*d.pose.scale[0],c.offsetY*d.pose.scale[1]};b2ShapeId shape{};
   if(c.shape==scene::Collider2DShape::Box){const auto box=b2MakeOffsetBox(c.halfX*d.pose.scale[0],c.halfY*d.pose.scale[1],offset,b2MakeRot(0));shape=b2CreatePolygonShape(newBody,&sd,&box);}
   else if(c.shape==scene::Collider2DShape::Circle){const b2Circle circle{offset,c.radius*d.pose.scale[0]};shape=b2CreateCircleShape(newBody,&sd,&circle);}
   else {const b2Capsule capsule{{offset.x,offset.y-c.capsuleHalfLength*d.pose.scale[1]},{offset.x,offset.y+c.capsuleHalfLength*d.pose.scale[1]},c.radius*d.pose.scale[0]};shape=b2CreateCapsuleShape(newBody,&sd,&capsule);}
   const u64 id=b2StoreShapeId(shape);built.shapes.push_back(id);p.shapes.emplace(id,Impl::Shape{d.object,c.instanceId(),owner->layer,c.sensor,d.body.motion});
  }
  if(d.body.motion==scene::Body2DMotion::Dynamic){auto mass=b2Body_GetMassData(newBody);if(mass.mass>0){mass.rotationalInertia*=d.body.mass/mass.mass;mass.mass=d.body.mass;b2Body_SetMassData(newBody,mass);}}
  if(old!=p.bodies.end()){p.retire(d.object,&world);}
  p.bodies.emplace(d.object,std::move(built));world.setAuthority(d.object,d.body.motion==scene::Body2DMotion::Dynamic?TransformAuthority::PhysicsBody2D:TransformAuthority::Free);
 }
 std::set<std::pair<ObjectId,u64>> jointAlive;
 for(const auto &d:drafts)for(const auto &c:d.joints){if(!c.enabled)continue;
  auto a=p.bodies.find(d.object);auto b=p.bodies.find(static_cast<ObjectId>(c.target));if(a==p.bodies.end()||(!c.worldAnchor&&b==p.bodies.end())){p.error="Joint2D target has no executable shapes";return false;}
  if(c.worldAnchor&&!b2Body_IsValid(p.ground)){auto def=b2DefaultBodyDef();p.ground=b2CreateBody(p.solver,&def);}
  const b2BodyId bodyA=a->second.id,bodyB=c.worldAnchor?p.ground:b->second.id;
  const b2Vec2 anchorA{c.anchorAX*a->second.scaleX,c.anchorAY*a->second.scaleY};
  const b2Vec2 anchorB{c.anchorBX*(c.worldAnchor?1.f:b->second.scaleX),c.anchorBY*(c.worldAnchor?1.f:b->second.scaleY)};
  const std::pair<ObjectId,u64> key{d.object,c.instanceId()};jointAlive.insert(key);std::ostringstream signature;signature.precision(std::numeric_limits<float>::max_digits10);c.write(signature);signature<<' '<<b2StoreBodyId(bodyA)<<' '<<b2StoreBodyId(bodyB);const auto text=signature.str();auto old=p.joints.find(key);if(old!=p.joints.end()&&old->second.signature==text)continue;if(old!=p.joints.end()){b2DestroyJoint(old->second.id);p.joints.erase(old);}
  b2JointId id{};
#define AE_JOINT_FRAMES(def) def.bodyIdA=bodyA;def.bodyIdB=bodyB;def.localAnchorA=anchorA;def.localAnchorB=anchorB;def.collideConnected=c.collideConnected
  if(c.kind==scene::Joint2DKind::Weld){auto def=b2DefaultWeldJointDef();AE_JOINT_FRAMES(def);def.referenceAngle=c.referenceAngleDegrees*radians;def.linearHertz=c.linearHertz;def.angularHertz=c.angularHertz;def.linearDampingRatio=c.linearDampingRatio;def.angularDampingRatio=c.angularDampingRatio;id=b2CreateWeldJoint(p.solver,&def);}
  else if(c.kind==scene::Joint2DKind::Revolute){auto def=b2DefaultRevoluteJointDef();AE_JOINT_FRAMES(def);def.referenceAngle=c.referenceAngleDegrees*radians;def.enableLimit=c.limitEnabled;def.lowerAngle=c.lowerLimit*radians;def.upperAngle=c.upperLimit*radians;def.enableMotor=c.motorEnabled;def.motorSpeed=c.motorAngularSpeedDegrees*radians;def.maxMotorTorque=c.maxMotorTorque;def.enableSpring=c.springEnabled;def.hertz=c.springHertz;def.dampingRatio=c.springDamping;def.targetAngle=c.springTargetAngleDegrees*radians;id=b2CreateRevoluteJoint(p.solver,&def);}
  else if(c.kind==scene::Joint2DKind::Prismatic){auto def=b2DefaultPrismaticJointDef();AE_JOINT_FRAMES(def);def.referenceAngle=c.referenceAngleDegrees*radians;def.localAxisA={std::cos(c.axisAngleDegrees*radians),std::sin(c.axisAngleDegrees*radians)};def.enableLimit=c.limitEnabled;def.lowerTranslation=c.lowerLimit;def.upperTranslation=c.upperLimit;def.enableMotor=c.motorEnabled;def.motorSpeed=c.motorSpeed;def.maxMotorForce=c.maxMotorForce;def.enableSpring=c.springEnabled;def.hertz=c.springHertz;def.dampingRatio=c.springDamping;def.targetTranslation=c.springTargetTranslation;id=b2CreatePrismaticJoint(p.solver,&def);}
  else {auto def=b2DefaultDistanceJointDef();AE_JOINT_FRAMES(def);def.length=c.length;def.enableSpring=c.springEnabled;def.hertz=c.springHertz;def.dampingRatio=c.springDamping;def.enableLimit=c.limitEnabled&&c.springEnabled;def.minLength=c.minLength;def.maxLength=c.maxLength;def.enableMotor=c.motorEnabled&&c.springEnabled;def.motorSpeed=c.motorSpeed;def.maxMotorForce=c.maxMotorForce;id=b2CreateDistanceJoint(p.solver,&def);}
#undef AE_JOINT_FRAMES
  if(!b2Joint_IsValid(id)){p.error="Box2D joint creation failed";return false;}p.joints.emplace(key,Impl::Joint{id,d.object,c.worldAnchor?0:static_cast<ObjectId>(c.target),text});
 }
 for(auto it=p.joints.begin();it!=p.joints.end();)if(!jointAlive.contains(it->first)){b2DestroyJoint(it->second.id);it=p.joints.erase(it);}else ++it;
 return true;
}
bool ScenePhysics2D::setGravity(float x,float y){if(!std::isfinite(x)||!std::isfinite(y))return false;impl_->gravity={x,y};if(b2World_IsValid(impl_->solver))b2World_SetGravity(impl_->solver,impl_->gravity);return true;}
#include "runtime/scene_physics2d_fields.inl"
bool ScenePhysics2D::advance(double elapsed,GameWorld &world,bool(*before)(void*,float),void *context,bool(*event)(void*,const Physics2DEvent&)){
 auto &p=*impl_;if(!std::isfinite(elapsed)||elapsed<0||world.worldId()!=p.worldId||!b2World_IsValid(p.solver))return false;p.accumulator+=std::min(elapsed,.25);constexpr float dt=1.f/60.f;
 while(p.accumulator+1e-9>=1.0/60.0){if(before&&!before(context,dt))return false;
  bool rescale=false;for(const auto &[object,body]:p.bodies){Transform pose;if(world.worldTransform(world.handle(object),pose)==WorldStatus::Ok&&(std::abs(pose.scale[0]-body.scaleX)>1e-5f||std::abs(pose.scale[1]-body.scaleY)>1e-5f)){rescale=true;break;}}if(rescale&&!rebuild(world))return false;
  std::vector<ObjectId> removed;for(auto &[object,body]:p.bodies){if(!world.alive(world.handle(object))){removed.push_back(object);continue;}const bool active=world.activeInHierarchy(world.handle(object));if(active&&!b2Body_IsEnabled(body.id))b2Body_Enable(body.id);if(!active&&b2Body_IsEnabled(body.id))b2Body_Disable(body.id);if(!active)continue;
   Transform pose;if(world.worldTransform(world.handle(object),pose)!=WorldStatus::Ok||!p.planar(pose)){p.error="Physics2D pose became non-planar";return false;}
   if(body.authored.motion!=scene::Body2DMotion::Dynamic){const auto position=b2Body_GetPosition(body.id);const float angle=b2Rot_GetAngle(b2Body_GetRotation(body.id));if(std::abs(position.x-pose.position[0])>1e-5f||std::abs(position.y-pose.position[1])>1e-5f||std::abs(std::remainder(angle-pose.rotationDegrees[2]*radians,6.2831853f))>1e-5f)b2Body_SetTransform(body.id,{pose.position[0],pose.position[1]},b2MakeRot(pose.rotationDegrees[2]*radians));}
  }for(auto object:removed)p.retire(object,&world);
  for(const auto &[object,body]:p.bodies){if(!world.activeInHierarchy(world.handle(object)))continue;const auto *component=world.graph().find(object)->components.find(scene::ConstantForce2D::descriptor);if(!component)continue;const auto &force=*static_cast<const scene::ConstantForce2D*>(component);if(!force.enabled)continue;if(body.authored.motion!=scene::Body2DMotion::Dynamic){p.error="ConstantForce2D became enabled on a non-Dynamic body";return false;}const auto local=b2RotateVector(b2Body_GetRotation(body.id),{force.relativeForceX,force.relativeForceY});const b2Vec2 total{force.forceX+local.x,force.forceY+local.y};if(total.x!=0||total.y!=0)b2Body_ApplyForceToCenter(body.id,total,true);if(force.torque!=0)b2Body_ApplyTorque(body.id,force.torque,true);}
  if(!applyPhysicsFields(world,dt))return false;
  b2World_Step(p.solver,dt,4);p.accumulator-=1.0/60.0;p.captureEvents();
  for(const auto &[object,body]:p.bodies){if(!world.graph().activeInHierarchy(object)||body.authored.motion==scene::Body2DMotion::Static)continue;Transform pose;if(world.worldTransform(world.handle(object),pose)!=WorldStatus::Ok)return false;const auto position=b2Body_GetPosition(body.id);pose.position[0]=position.x;pose.position[1]=position.y;pose.rotationDegrees[2]=b2Rot_GetAngle(b2Body_GetRotation(body.id))/radians;float matrix[16],parent[16];Transform local;transformMatrix(pose,matrix);if(!parentWorldMatrix(world.graph(),object,parent)||!localTransformForWorld(matrix,parent,local)||!world.poseGraph().setTransform(object,local)){p.error="Physics2D publication cannot represent parent-relative pose";return false;}}
  auto events=std::move(p.pending);p.pending.clear();if(event)for(const auto &e:events)if(!event(context,e))return false;
 }
 return true;
}
bool ScenePhysics2D::velocity(ObjectId object,float out[2],float &angular) const{auto it=impl_->bodies.find(object);if(it==impl_->bodies.end()||!out)return false;const auto v=b2Body_GetLinearVelocity(it->second.id);out[0]=v.x;out[1]=v.y;angular=b2Body_GetAngularVelocity(it->second.id)/radians;return true;}
bool ScenePhysics2D::setVelocity(ObjectId object,const float v[2],float angular){auto it=impl_->bodies.find(object);if(it==impl_->bodies.end()||it->second.authored.motion==scene::Body2DMotion::Static||!finite2(v)||!std::isfinite(angular))return false;b2Body_SetLinearVelocity(it->second.id,{v[0],v[1]});b2Body_SetAngularVelocity(it->second.id,angular*radians);return true;}
bool ScenePhysics2D::addForce(ObjectId object,const float v[2]){auto it=impl_->bodies.find(object);if(it==impl_->bodies.end()||it->second.authored.motion!=scene::Body2DMotion::Dynamic||!finite2(v))return false;b2Body_ApplyForceToCenter(it->second.id,{v[0],v[1]},true);return true;}
bool ScenePhysics2D::addImpulse(ObjectId object,const float v[2]){auto it=impl_->bodies.find(object);if(it==impl_->bodies.end()||it->second.authored.motion!=scene::Body2DMotion::Dynamic||!finite2(v))return false;b2Body_ApplyLinearImpulseToCenter(it->second.id,{v[0],v[1]},true);return true;}
bool ScenePhysics2D::addTorque(ObjectId object,float value){auto it=impl_->bodies.find(object);if(it==impl_->bodies.end()||it->second.authored.motion!=scene::Body2DMotion::Dynamic||!std::isfinite(value))return false;b2Body_ApplyTorque(it->second.id,value,true);return true;}
bool ScenePhysics2D::addAngularImpulse(ObjectId object,float value){auto it=impl_->bodies.find(object);if(it==impl_->bodies.end()||it->second.authored.motion!=scene::Body2DMotion::Dynamic||!std::isfinite(value))return false;b2Body_ApplyAngularImpulse(it->second.id,value,true);return true;}
bool ScenePhysics2D::moveKinematic(ObjectId object,const float position[2],float rotation){auto it=impl_->bodies.find(object);if(it==impl_->bodies.end()||it->second.authored.motion!=scene::Body2DMotion::Kinematic||!finite2(position)||!std::isfinite(rotation))return false;b2Body_SetTargetTransform(it->second.id,{ {position[0],position[1]},b2MakeRot(rotation*radians)},1.f/60.f);return true;}
u32 ScenePhysics2D::rayCastAll(const float origin[2],const float translation[2],const Physics2DFilter &filter,Physics2DHit *out,u32 capacity) const{
 if(!finite2(origin)||!finite2(translation)||!b2World_IsValid(impl_->solver)||(capacity&&!out))return 0;
 struct Context {const Impl *impl;const Physics2DFilter *filter;std::vector<Physics2DHit> hits;};Context context{impl_.get(),&filter,{}};
 auto collect=[](b2ShapeId id,b2Vec2 point,b2Vec2 normal,float fraction,void *raw)->float{auto &c=*static_cast<Context*>(raw);if(c.impl->accept(id,*c.filter))c.hits.push_back(c.impl->hit(id,point,normal,fraction));return 1;};
 auto query=b2DefaultQueryFilter();query.categoryBits=UINT64_MAX;query.maskBits=filter.layerMask;b2World_CastRay(impl_->solver,{origin[0],origin[1]},{translation[0],translation[1]},query,collect,&context);
 std::sort(context.hits.begin(),context.hits.end(),[](const auto &a,const auto &b){if(a.fraction!=b.fraction)return a.fraction<b.fraction;if(a.object!=b.object)return a.object<b.object;return a.colliderInstance<b.colliderInstance;});
 for(u32 i=0;i<std::min(capacity,static_cast<u32>(context.hits.size()));++i){out[i]=context.hits[i];}
 return static_cast<u32>(context.hits.size());
}
bool ScenePhysics2D::rayCast(const float origin[2],const float translation[2],const Physics2DFilter &filter,Physics2DHit &out) const{return rayCastAll(origin,translation,filter,&out,1)>0;}
u32 ScenePhysics2D::overlapCircle(const float center[2],float radius,const Physics2DFilter &filter,Physics2DHit *out,u32 capacity) const{
 if(!finite2(center)||!std::isfinite(radius)||radius<=0||!b2World_IsValid(impl_->solver)||(capacity&&!out))return 0;
 struct Context {const Impl *impl;const Physics2DFilter *filter;std::vector<Physics2DHit> hits;};Context context{impl_.get(),&filter,{}};
 auto collect=[](b2ShapeId id,void *raw)->bool{auto &c=*static_cast<Context*>(raw);if(c.impl->accept(id,*c.filter))c.hits.push_back(c.impl->hit(id));return true;};
 const b2Vec2 position{center[0],center[1]};const auto proxy=b2MakeProxy(&position,1,radius);auto query=b2DefaultQueryFilter();query.categoryBits=UINT64_MAX;query.maskBits=filter.layerMask;b2World_OverlapShape(impl_->solver,&proxy,query,collect,&context);
 std::sort(context.hits.begin(),context.hits.end(),[](const auto &a,const auto &b){return a.object==b.object?a.colliderInstance<b.colliderInstance:a.object<b.object;});
 for(u32 i=0;i<std::min(capacity,static_cast<u32>(context.hits.size()));++i){out[i]=context.hits[i];}
 return static_cast<u32>(context.hits.size());
}
}
