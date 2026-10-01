#pragma once
#include "runtime/game_world.h"
#include "resources/curve3d.h"
#include <map>
#include <array>
namespace ae::runtime {
class ScenePaths final {
public:
 enum class Issue {MissingPath,InvalidCurve,Cycle,Authority,CompetingWriter,VerticalTangent,InvalidPose,DependencyLimit};
 struct Diagnostic {ObjectId object;Issue issue;};
 const std::vector<Diagnostic>&diagnostics()const{return diagnostics_;}
 static const char *issueText(Issue);
 void reset();
 bool advance(GameWorld&,double seconds);
 bool restart(GameWorld&,ObjectId);bool stop(GameWorld&,ObjectId);
 bool length(GameWorld&,ObjectId,double&);
 bool progress(GameWorld&,ObjectId,double&);
 bool playing(GameWorld&,ObjectId,bool&);
 bool sample(GameWorld&,ObjectId path,double distance,std::array<float,3>&position,std::array<float,3>&tangent,bool wrap=false);
 bool sampleFrame(GameWorld&,ObjectId path,double distance,resources::BakedCurve3D::Frame&,bool wrap=false);
private:
 struct CurveCache {resources::Curve3D source;std::array<float,16>matrix{};resources::BakedCurve3D baked;bool ready=false,attempted=false;};
 struct FollowState {u64 instance=0,target=0;float initial=0;double distance=0;bool playing=false,initialized=false,pendingReset=false;};
 std::map<ObjectId,CurveCache>curves_;std::map<ObjectId,FollowState>followers_;
 std::vector<ObjectId>candidates_;std::vector<Diagnostic>diagnostics_;u32 world_=0;u64 revision_=0;
 bool prepare(GameWorld&,ObjectId);
 FollowState *ensureFollower(GameWorld&,ObjectId);
};
}
