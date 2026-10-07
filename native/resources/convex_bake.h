#pragma once
#include "core/base.h"
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace ae::resources {
// Authoring only: output uses the existing Mesh resource and Collider collection.
// Voxel/volume error is an approximation parameter, not a distance guarantee.
enum class ConvexBakePose : u32 { Rest, Authored, Animation };
struct ConvexBakeSettings {
  u32 maximumParts=16,voxelResolution=100000,maximumVertices=32;
  float volumeErrorPercent=1;
  u32 timeBudgetSeconds=60;
  ConvexBakePose pose=ConvexBakePose::Authored;
  float animationTime=0;
  friend bool operator==(const ConvexBakeSettings &,const ConvexBakeSettings &)=default;
};
inline bool validConvexBakeSettings(const ConvexBakeSettings &s) noexcept {
  return s.maximumParts>=2&&s.maximumParts<=32&&s.voxelResolution>=10000&&s.voxelResolution<=400000&&
    s.maximumVertices>=8&&s.maximumVertices<=64&&std::isfinite(s.volumeErrorPercent)&&
    s.volumeErrorPercent>=.1f&&s.volumeErrorPercent<=10&&s.timeBudgetSeconds>=1&&s.timeBudgetSeconds<=120&&
    static_cast<u32>(s.pose)<=static_cast<u32>(ConvexBakePose::Animation)&&
    std::isfinite(s.animationTime)&&s.animationTime>=0&&s.animationTime<=86400;
}
struct ConvexBakePart { std::vector<std::array<float,3>> vertices;std::vector<u32> indices;u64 sourceObject=0; };
// Provenance of immutable baked geometry, not a live scene object/regen recipe.
struct ConvexBakeOrigin {
  u64 object=0;u32 slot=0,firstTriangle=0,triangleCount=0;std::string meshGuid;
  std::array<float,16> relative{};
  friend bool operator==(const ConvexBakeOrigin &,const ConvexBakeOrigin &)=default;
};
struct ConvexBakeResult {
  std::vector<ConvexBakePart> parts;
  std::vector<u8> glb;
  std::string sourceGeometryHash;
  std::vector<ConvexBakeOrigin> origins;
};
// Same immutable GLB writer as baking, with truthful authoring provenance.
bool writeCollisionTopologyGlb(const ConvexBakePart &,std::string_view sourceHash,std::vector<u8> &,std::string &) noexcept;
enum class ConvexBakeStatus { Idle,Running,Ready,Cancelled,Failed };
struct ConvexBakeProgress { ConvexBakeStatus status=ConvexBakeStatus::Idle;float fraction=0;std::string stage,error; };
class ConvexBakeJob final {
public:
  ConvexBakeJob() noexcept;
  ~ConvexBakeJob();
  ConvexBakeJob(const ConvexBakeJob &)=delete;
  ConvexBakeJob &operator=(const ConvexBakeJob &)=delete;
  // Copies no scene objects. The worker owns its input and joins on destruction.
  bool start(std::vector<float> triangles,ConvexBakeSettings settings,std::string hash,
             std::vector<std::string> sourceGuids,std::string &error,
             std::vector<ConvexBakeOrigin> origins={}) noexcept;
  void cancel() noexcept;
  // Only for an already validated result from this algorithm in this process.
  // It is not a loader for untrusted or stale on-disk derived resources.
  bool reuse(const ConvexBakeResult &,std::string &) noexcept;
  ConvexBakeProgress progress() const;
  const ConvexBakeResult *result() const noexcept;
private:
  struct Work;
  std::unique_ptr<Work> work_;
};
}
