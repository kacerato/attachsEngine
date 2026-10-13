#include "resources/gltf_export.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sstream>

namespace ae::resources {
namespace {
void jsonString(std::ostringstream &o,const std::string &s) {
  o<<'"';
  for(const unsigned char c:s) {
    if(c=='"'||c=='\\')o<<'\\'<<c;
    else if(c<0x20) {char b[8];std::snprintf(b,sizeof b,"\\u%04x",c);o<<b;}
    else o<<c;
  }
  o<<'"';
}
void number(std::ostringstream &o,float v) {char b[32];std::snprintf(b,sizeof b,"%.9g",static_cast<double>(v));o<<b;}
void append(std::vector<u8> &bin,const std::vector<float> &values) {
  while(bin.size()%4)bin.push_back(0);
  const auto at=bin.size();bin.resize(at+values.size()*4);std::memcpy(bin.data()+at,values.data(),values.size()*4);
}
void put32(std::vector<u8> &out,u32 v) {for(u32 i=0;i<4;++i)out.push_back(static_cast<u8>(v>>(8*i)));}
} // namespace

bool writeGlb(const std::vector<GltfExportNode> &nodes,const std::vector<GltfExportAnimation> &animations,
              std::vector<u8> &out,std::string &diagnostic) {
  out.clear();diagnostic.clear();
  if(nodes.empty()) {diagnostic="Nada a exportar: nenhum nó";return false;}
  for(usize i=0;i<nodes.size();++i) {
    const auto &n=nodes[i];
    if(n.parent>=static_cast<i32>(i)||n.parent<-1) {diagnostic="Hierarquia fora de ordem (pai depois do filho)";return false;}
    for(float v:n.translation)if(!std::isfinite(v)) {diagnostic="Translação não finita em "+n.name;return false;}
    for(float v:n.rotation)if(!std::isfinite(v)) {diagnostic="Rotação não finita em "+n.name;return false;}
    for(float v:n.scale)if(!std::isfinite(v)) {diagnostic="Escala não finita em "+n.name;return false;}
  }
  struct View {usize offset,length;};
  struct Accessor {u32 view;usize count;const char *type;bool minmax;float min,max;};
  std::vector<u8> bin;std::vector<View> views;std::vector<Accessor> accessors;
  const auto add=[&](const std::vector<float> &values,usize count,const char *type,bool minmax) {
    while(bin.size()%4)bin.push_back(0);
    const usize offset=bin.size();append(bin,values);
    views.push_back({offset,values.size()*4});
    float mn=std::numeric_limits<float>::max(),mx=std::numeric_limits<float>::lowest();
    if(minmax)for(float v:values){mn=std::min(mn,v);mx=std::max(mx,v);}
    accessors.push_back({static_cast<u32>(views.size()-1),count,type,minmax,mn,mx});
    return static_cast<u32>(accessors.size()-1);
  };
  std::ostringstream animationsJson;
  for(usize a=0;a<animations.size();++a) {
    const auto &anim=animations[a];
    std::ostringstream samplers,channels;u32 used=0;
    for(const auto &c:anim.channels) {
      const u32 width=c.path==AnimationPath::Rotation?4:3;
      if(c.path==AnimationPath::Weights||c.node>=nodes.size()||c.times.empty()||c.values.size()!=c.times.size()*width) {diagnostic="Canal inválido em "+anim.name;return false;}
      for(usize k=1;k<c.times.size();++k)if(!(c.times[k]>c.times[k-1])) {diagnostic="Tempos fora de ordem em "+anim.name;return false;}
      for(float v:c.values)if(!std::isfinite(v)) {diagnostic="Valor não finito em "+anim.name;return false;}
      const u32 input=add(c.times,c.times.size(),"SCALAR",true);
      const u32 output=add(c.values,c.times.size(),width==4?"VEC4":"VEC3",false);
      if(used){samplers<<',';channels<<',';}
      samplers<<"{\"input\":"<<input<<",\"output\":"<<output<<",\"interpolation\":\"LINEAR\"}";
      channels<<"{\"sampler\":"<<used<<",\"target\":{\"node\":"<<c.node<<",\"path\":\""<<(c.path==AnimationPath::Translation?"translation":c.path==AnimationPath::Rotation?"rotation":"scale")<<"\"}}";
      ++used;
    }
    if(!used)continue;
    if(animationsJson.tellp()>0)animationsJson<<',';
    animationsJson<<"{\"name\":";jsonString(animationsJson,anim.name);
    animationsJson<<",\"samplers\":["<<samplers.str()<<"],\"channels\":["<<channels.str()<<"]}";
  }
  std::ostringstream j;
  j<<"{\"asset\":{\"version\":\"2.0\",\"generator\":\"Astra Engine\"},\"scene\":0,\"scenes\":[{\"nodes\":[";
  bool first=true;for(usize i=0;i<nodes.size();++i)if(nodes[i].parent<0){if(!first)j<<',';j<<i;first=false;}
  j<<"]}],\"nodes\":[";
  for(usize i=0;i<nodes.size();++i) {
    const auto &n=nodes[i];if(i)j<<',';
    j<<"{\"name\":";jsonString(j,n.name);
    std::ostringstream kids;bool any=false;
    for(usize c=0;c<nodes.size();++c)if(nodes[c].parent==static_cast<i32>(i)){if(any)kids<<',';kids<<c;any=true;}
    if(any)j<<",\"children\":["<<kids.str()<<"]";
    j<<",\"translation\":[";number(j,n.translation[0]);j<<',';number(j,n.translation[1]);j<<',';number(j,n.translation[2]);j<<"]";
    j<<",\"rotation\":[";for(u32 k=0;k<4;++k){if(k)j<<',';number(j,n.rotation[k]);}j<<"]";
    j<<",\"scale\":[";number(j,n.scale[0]);j<<',';number(j,n.scale[1]);j<<',';number(j,n.scale[2]);j<<"]}";
  }
  j<<"]";
  if(animationsJson.tellp()>0)j<<",\"animations\":["<<animationsJson.str()<<"]";
  while(bin.size()%4)bin.push_back(0);
  if(!bin.empty()) {
    j<<",\"buffers\":[{\"byteLength\":"<<bin.size()<<"}],\"bufferViews\":[";
    for(usize v=0;v<views.size();++v){if(v)j<<',';j<<"{\"buffer\":0,\"byteOffset\":"<<views[v].offset<<",\"byteLength\":"<<views[v].length<<"}";}
    j<<"],\"accessors\":[";
    for(usize a=0;a<accessors.size();++a) {
      const auto &x=accessors[a];if(a)j<<',';
      j<<"{\"bufferView\":"<<x.view<<",\"componentType\":5126,\"count\":"<<x.count<<",\"type\":\""<<x.type<<"\"";
      if(x.minmax){j<<",\"min\":[";number(j,x.min);j<<"],\"max\":[";number(j,x.max);j<<"]";}
      j<<"}";
    }
    j<<"]";
  }
  j<<"}";
  std::string json=j.str();while(json.size()%4)json.push_back(' ');
  const u32 total=12+8+static_cast<u32>(json.size())+(bin.empty()?0:8+static_cast<u32>(bin.size()));
  put32(out,0x46546C67);put32(out,2);put32(out,total);
  put32(out,static_cast<u32>(json.size()));put32(out,0x4E4F534A);out.insert(out.end(),json.begin(),json.end());
  if(!bin.empty()){put32(out,static_cast<u32>(bin.size()));put32(out,0x004E4942);out.insert(out.end(),bin.begin(),bin.end());}
  return true;
}
} // namespace ae::resources
