#pragma once
#include "runtime/game_world.h"
#include <memory>
#include <string>
#include <vector>
namespace ae::runtime {
struct Physics2DFilter {u32 layerMask=0xffffffffu;bool includeSensors=false;bool includeStatic=true;bool includeDynamic=true;ObjectId ignore=0;};
struct Physics2DHit {ObjectId object=0;u64 colliderInstance=0;float point[2]{},normal[2]{},fraction=0;bool sensor=false;};
struct Physics2DEvent {ObjectId first=0,second=0;u64 firstCollider=0,secondCollider=0;u32 phase=0;bool sensor=false,hasNormal=false;float normal[2]{};};
struct PhysicsFieldSample;
// Independent XY/MKS solver. No Jolt restricted-body aliases or render dependency.
class ScenePhysics2D final {
public:
 ScenePhysics2D();~ScenePhysics2D();ScenePhysics2D(const ScenePhysics2D&)=delete;ScenePhysics2D&operator=(const ScenePhysics2D&)=delete;
 bool start(GameWorld &world);bool rebuild(GameWorld &world);
 void stop(GameWorld *world=nullptr);void releaseObject(ObjectId id,GameWorld *world=nullptr);
 bool advance(double elapsed,GameWorld &world,bool (*beforeStep)(void*,float)=nullptr,void *context=nullptr,bool (*event)(void*,const Physics2DEvent&)=nullptr);
 const std::string &error() const;u32 bodyCount() const;u32 jointCount() const;bool sleeping(ObjectId object) const;
 bool setGravity(float x,float y);
 bool velocity(ObjectId object,float out[2],float &angularDegrees) const;
 bool setVelocity(ObjectId object,const float value[2],float angularDegrees);
 bool addForce(ObjectId object,const float value[2]);bool addImpulse(ObjectId object,const float value[2]);
 bool addTorque(ObjectId object,float value);bool addAngularImpulse(ObjectId object,float value);
 bool moveKinematic(ObjectId object,const float position[2],float rotationDegrees);
 bool rayCast(const float origin[2],const float translation[2],const Physics2DFilter &filter,Physics2DHit &out) const;
 u32 rayCastAll(const float origin[2],const float translation[2],const Physics2DFilter &filter,Physics2DHit *out,u32 capacity) const;
 u32 overlapCircle(const float center[2],float radius,const Physics2DFilter &filter,Physics2DHit *out,u32 capacity) const;
 WorldStatus fieldQuery(const GameWorld &world,ObjectHandle owner,u64 instance,u32 operation,const float *point,u32 layer,PhysicsFieldSample &out) const;
private:
 bool applyPhysicsFields(GameWorld &world,float dt);
 struct Impl;std::unique_ptr<Impl> impl_;
};
}
