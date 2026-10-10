#pragma once
#include "resources/animation_curve.h"
#include <string>

namespace ae::resources {
// Scalar driver math. Property/Animator sources and output ownership are
// resolved by the scene consumer, never by raw pointers in the graph resource.
enum class AnimationDriverOp : u8 { Source,Constant,Add,Multiply,ScaleBias,Clamp,Remap,Curve };
struct AnimationDriverNode {
  u64 id=0;
  AnimationDriverOp operation=AnimationDriverOp::Source;
  u64 first=0,second=0;
  u32 source=0;
  double a=0,b=1,c=0,d=1;
  AnimationCurve curve;
};
struct AnimationDriverGraph {
  static constexpr usize MaximumNodes=512,MaximumSources=256,MaximumOutputs=128,MaximumCurveKeys=8192;
  std::vector<AnimationDriverNode> nodes;
  // Entries correspond to the caller's typed output binding list. Duplicate
  // bindings are rejected there; several outputs may intentionally share math.
  std::vector<u64> outputs;
  u32 sources=0;
};
class CompiledAnimationDrivers final {
public:
  usize scratchSize() const {return nodes_.size();}
  usize outputSize() const {return outputs_.size();}
  u32 sourceCount() const {return sources_;}
  // Caller owns fixed scratch/output storage. No allocation on successful
  // evaluation. Failure never changes outputs; scratch is temporary by contract.
  bool evaluate(std::span<const double> sources,std::span<double> scratch,
                std::span<double> outputs,std::string &error) const;
private:
  friend bool compileAnimationDrivers(const AnimationDriverGraph &,CompiledAnimationDrivers &,std::string &);
  struct Node {AnimationDriverNode data;u32 first=0,second=0;};
  std::vector<Node> nodes_;
  std::vector<u32> outputs_;
  u32 sources_=0;
};
// Compiles dependency order and resolves IDs once. Cycles, unknown IDs,
// meaningless inputs, invalid remaps/curves and budgets fail before publication.
bool compileAnimationDrivers(const AnimationDriverGraph &,CompiledAnimationDrivers &,std::string &error);
}
