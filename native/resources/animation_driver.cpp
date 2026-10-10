#include "resources/animation_driver.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <unordered_map>

namespace ae::resources {
namespace {
u32 arity(AnimationDriverOp op) {
  return op==AnimationDriverOp::Source||op==AnimationDriverOp::Constant?0:
         op==AnimationDriverOp::Add||op==AnimationDriverOp::Multiply?2:1;
}
bool fail(std::string &error,const char *message,u64 id=0) {
  error=message;if(id)error+=" · node "+std::to_string(id);return false;
}
}
bool compileAnimationDrivers(const AnimationDriverGraph &graph,CompiledAnimationDrivers &out,std::string &error) {
  error.clear();
  if(graph.nodes.empty()||graph.nodes.size()>AnimationDriverGraph::MaximumNodes||
     graph.sources>AnimationDriverGraph::MaximumSources||graph.outputs.empty()||
     graph.outputs.size()>AnimationDriverGraph::MaximumOutputs)return fail(error,"Driver graph exceeds its budget or has no outputs");
  std::unordered_map<u64,u32> indices;usize curveKeys=0;
  for(u32 i=0;i<graph.nodes.size();++i) {
    const auto &n=graph.nodes[i];
    if(!n.id||!indices.emplace(n.id,i).second||static_cast<u32>(n.operation)>7)return fail(error,"Driver node identity or operation is invalid",n.id);
    if(!std::isfinite(n.a)||!std::isfinite(n.b)||!std::isfinite(n.c)||!std::isfinite(n.d))return fail(error,"Driver constants must be finite",n.id);
    const u32 count=arity(n.operation);
    if((count==0&&(n.first||n.second))||(count==1&&(!n.first||n.second))||(count==2&&(!n.first||!n.second)))return fail(error,"Driver input count does not match its operation",n.id);
    if((n.operation==AnimationDriverOp::Source&&n.source>=graph.sources)||
       (n.operation!=AnimationDriverOp::Source&&n.source))return fail(error,"Driver source index is invalid",n.id);
    if(n.operation==AnimationDriverOp::Clamp&&n.a>n.b)return fail(error,"Driver clamp limits are reversed",n.id);
    if(n.operation==AnimationDriverOp::Remap&&n.a==n.b)return fail(error,"Driver remap input interval has zero length",n.id);
    if(n.operation==AnimationDriverOp::Curve) {
      if(n.curve.keys.size()>AnimationDriverGraph::MaximumCurveKeys-curveKeys||!validAnimationCurve(n.curve))return fail(error,"Driver curve is invalid or exceeds the key budget",n.id);
      curveKeys+=n.curve.keys.size();
    } else if(!n.curve.keys.empty())return fail(error,"Only curve nodes may own curve keys",n.id);
  }
  std::vector<std::vector<u32>> next(graph.nodes.size());std::vector<u32> degree(graph.nodes.size());
  for(u32 i=0;i<graph.nodes.size();++i)for(const auto dependency:{graph.nodes[i].first,graph.nodes[i].second})if(dependency) {
    const auto found=indices.find(dependency);if(found==indices.end())return fail(error,"Driver input node is absent",dependency);
    next[found->second].push_back(i);++degree[i];
  }
  using Ready=std::pair<u64,u32>;std::priority_queue<Ready,std::vector<Ready>,std::greater<Ready>> ready;
  for(u32 i=0;i<degree.size();++i)if(!degree[i])ready.push({graph.nodes[i].id,i});
  CompiledAnimationDrivers candidate;candidate.sources_=graph.sources;candidate.nodes_.reserve(graph.nodes.size());
  std::vector<u32> compiled(graph.nodes.size());
  while(!ready.empty()) {
    const auto i=ready.top().second;ready.pop();CompiledAnimationDrivers::Node node;node.data=graph.nodes[i];
    if(node.data.first)node.first=compiled[indices.at(node.data.first)];
    if(node.data.second)node.second=compiled[indices.at(node.data.second)];
    compiled[i]=static_cast<u32>(candidate.nodes_.size());candidate.nodes_.push_back(std::move(node));
    for(const auto target:next[i])if(!--degree[target])ready.push({graph.nodes[target].id,target});
  }
  if(candidate.nodes_.size()!=graph.nodes.size()) {
    for(u32 i=0;i<degree.size();++i)if(degree[i])return fail(error,"Driver dependency cycle",graph.nodes[i].id);
  }
  for(const auto id:graph.outputs) {
    const auto found=indices.find(id);if(found==indices.end())return fail(error,"Driver output node is absent",id);
    candidate.outputs_.push_back(compiled[found->second]);
  }
  out=std::move(candidate);return true;
}
bool CompiledAnimationDrivers::evaluate(std::span<const double> sources,std::span<double> scratch,
                                      std::span<double> outputs,std::string &error) const {
  error.clear();
  if(nodes_.empty()||sources.size()!=sources_||scratch.size()<nodes_.size()||outputs.size()!=outputs_.size())return fail(error,"Driver evaluation storage or source count is invalid");
  // Scratch and outputs must be distinct so an error cannot leak partial output.
  const auto overlap=[](auto a,auto b){if(a.empty()||b.empty())return false;
    const auto startA=reinterpret_cast<uintptr_t>(a.data()),startB=reinterpret_cast<uintptr_t>(b.data());
    return startA<startB+b.size_bytes()&&startB<startA+a.size_bytes();};
  if(overlap(scratch,outputs)||overlap(scratch,sources))return fail(error,"Driver scratch aliases input or output storage");
  for(const double source:sources)if(!std::isfinite(source))return fail(error,"Driver source is not finite");
  for(usize i=0;i<nodes_.size();++i) {
    const auto &compiled=nodes_[i];const auto &n=compiled.data;double value=0;
    const double first=arity(n.operation)?scratch[compiled.first]:0;
    switch(n.operation) {
    case AnimationDriverOp::Source:value=sources[n.source];break;
    case AnimationDriverOp::Constant:value=n.a;break;
    case AnimationDriverOp::Add:value=first+scratch[compiled.second];break;
    case AnimationDriverOp::Multiply:value=first*scratch[compiled.second];break;
    case AnimationDriverOp::ScaleBias:value=first*n.a+n.b;break;
    case AnimationDriverOp::Clamp:value=std::clamp(first,n.a,n.b);break;
    case AnimationDriverOp::Remap:value=n.c+(first-n.a)/(n.b-n.a)*(n.d-n.c);break;
    case AnimationDriverOp::Curve: {AnimationCurveSample sample;if(!sampleValidatedAnimationCurve(n.curve,first,sample))return fail(error,"Driver curve sampling failed",n.id);value=sample.value;break;}
    }
    if(!std::isfinite(value))return fail(error,"Driver result overflow",n.id);
    scratch[i]=value;
  }
  for(usize i=0;i<outputs_.size();++i)outputs[i]=scratch[outputs_[i]];
  return true;
}
}
