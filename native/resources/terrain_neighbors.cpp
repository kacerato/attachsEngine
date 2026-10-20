#include "resources/terrain_neighbors.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <iomanip>
#include <locale>
#include <sstream>

namespace ae::resources {
namespace {
struct Sets {
  std::vector<size_t> p;
  explicit Sets(size_t n): p(n) { std::iota(p.begin(),p.end(),0); }
  size_t root(size_t i) { while(p[i]!=i) { p[i]=p[p[i]]; i=p[i]; } return i; }
  void join(size_t a,size_t b) { a=root(a); b=root(b); p[std::max(a,b)]=std::min(a,b); }
};
bool finite(double v) { return std::isfinite(v); }
template<class T> bool unsignedToken(std::istream& s,T& value) {
  s>>std::ws;int c=s.peek();
  if(c<'0'||c>'9')return false;
  return bool(s>>value);
}
bool validDescriptor(const TerrainNeighborDescriptor& d,std::string& error) {
  if(d.members.size()>4096) { error="Too many terrain group members";return false; }
  std::set<TerrainIdentity> ids;
  for(auto& m:d.members) {
    if(!m.group||!m.member||!ids.insert(m.member).second||!finite(m.x)||!finite(m.y)||!finite(m.z)) {error="Invalid or duplicate terrain member metadata";return false;}
    if(m.sourceAsset.empty()||m.sourceAsset.size()>1024||m.sourceAsset.find('\0')!=std::string::npos||m.layers.empty()||m.layers.size()>256) {error="Invalid source asset identity or layer count";return false;}
    std::set<TerrainIdentity> layers;
    for(auto id:m.layers) if(!id||!layers.insert(id).second) {error="Invalid or duplicate terrain layer metadata";return false;}
  }
  return true;
}
}
bool serializeTerrainNeighbors(const TerrainNeighborDescriptor& d,std::string& out,std::string& error) {
  if(!validDescriptor(d,error)) return false;
  std::ostringstream s;s.imbue(std::locale::classic());
  s<<"AETERRAINNEIGHBORS1 "<<d.members.size()<<'\n'<<std::setprecision(17);
  for(auto& m:d.members) {
    s<<m.group<<' '<<m.member<<' '<<m.x<<' '<<m.y<<' '<<m.z<<' '<<std::quoted(m.sourceAsset)<<' '<<m.layers.size();
    for(auto id:m.layers)s<<' '<<id;
    s<<'\n';
  }
  auto text=s.str();
  if(text.size()>4*1024*1024) {error="Terrain group metadata exceeds byte budget";return false;}
  out=std::move(text);error.clear();return true;
}
bool deserializeTerrainNeighbors(std::string_view text,TerrainNeighborDescriptor& out,std::string& error) {
  auto fail=[&](){error="Invalid or truncated terrain group metadata";return false;};
  if(text.size()>4*1024*1024||text.find('\0')!=std::string_view::npos)return fail();
  std::istringstream s{std::string(text)};s.imbue(std::locale::classic());
  std::string version;size_t count=0;
  if(!(s>>version)||!unsignedToken(s,count)||version!="AETERRAINNEIGHBORS1"||count>4096)return fail();
  TerrainNeighborDescriptor d;
  for(size_t i=0;i<count;++i) {
    TerrainNeighborMember m;size_t layers=0;
    // Require quotes, rather than accepting std::quoted's unquoted fallback.
    if(!unsignedToken(s,m.group)||!unsignedToken(s,m.member)||!(s>>m.x>>m.y>>m.z))return fail();
    s>>std::ws;if(s.peek()!='"'||!(s>>std::quoted(m.sourceAsset))||!unsignedToken(s,layers)||layers==0||layers>256||m.sourceAsset.size()>1024)return fail();
    for(size_t l=0;l<layers;++l) { TerrainIdentity id=0;if(!unsignedToken(s,id))return fail();m.layers.push_back(id); }
    d.members.push_back(std::move(m));
  }
  s>>std::ws;if(!s.eof())return fail();
  if(!validDescriptor(d,error))return false;
  out=std::move(d);error.clear();return true;
}
bool bindTerrainNeighbor(const TerrainNeighborMember& m,const TerrainField& f,
    std::span<const float> weights,TerrainNeighborTile& out,std::string& error) {
  if(!validDescriptor(TerrainNeighborDescriptor{{m}},error))return false;
  TerrainNeighborTile tile;tile.group=m.group;tile.member=m.member;
  tile.x=m.x;tile.y=m.y;tile.z=m.z;tile.layers=m.layers;tile.field=f;
  tile.weights.assign(weights.begin(),weights.end());
  std::vector<TerrainNeighborLink> links;
  if(!terrainNeighborLinks(std::span<const TerrainNeighborTile>(&tile,1),links,error))return false;
  out=std::move(tile);return true;
}
bool terrainNeighborLinks(std::span<const TerrainNeighborTile> tiles,
    std::vector<TerrainNeighborLink>& out,std::string& error,double tol) {
  auto fail=[&](const char* s){error=s; return false;};
  if(!finite(tol)||tol<0) return fail("Invalid neighbor tolerance");
  std::set<TerrainIdentity> ids;
  for(const auto& t:tiles) {
    const auto& f=t.field;
    if(!t.group||!t.member||!ids.insert(t.member).second) return fail("Invalid or duplicate terrain identity");
    if(!finite(t.x)||!finite(t.y)||!finite(t.z)||!finite(f.step)||f.step<=0||tol>=f.step*.25||f.width<2||f.depth<2||f.height>1||!f.density.empty()) return fail("Neighbors require finite heightfield grids and bounded tolerance");
    size_t n=size_t(f.width)*f.depth;
    if(f.heights.size()!=n||t.layers.empty()||t.weights.size()/t.layers.size()!=n||t.weights.size()%t.layers.size()) return fail("Invalid terrain sample or weight dimensions");
    std::set<TerrainIdentity> layers;
    for(auto id:t.layers) if(!id||!layers.insert(id).second) return fail("Invalid or duplicate layer identity");
    for(float h:f.heights) if(!finite(h)) return fail("Non-finite terrain height");
    for(size_t i=0;i<n;++i) {
      double sum=0;
      for(size_t l=0;l<t.layers.size();++l) { float w=t.weights[i*t.layers.size()+l]; if(!finite(w)||w<0) return fail("Invalid terrain layer weight"); sum+=w; }
      if(std::abs(sum-1)>1e-4) return fail("Terrain weights must be normalized");
    }
  }
  std::vector<TerrainNeighborLink> links;
  std::set<std::pair<TerrainIdentity,u32>> occupied;
  auto near=[&](double a,double b){return std::abs(a-b)<=tol;};
  for(size_t i=0;i<tiles.size();++i) for(size_t j=i+1;j<tiles.size();++j) {
    auto& a=tiles[i]; auto& b=tiles[j]; if(a.group!=b.group) continue;
    double ax=a.x+(a.field.width-1)*double(a.field.step),az=a.z+(a.field.depth-1)*double(a.field.step);
    double bx=b.x+(b.field.width-1)*double(b.field.step),bz=b.z+(b.field.depth-1)*double(b.field.step);
    int side=-1;
    bool zOverlap=std::min(az,bz)-std::max(a.z,b.z)>tol;
    bool xOverlap=std::min(ax,bx)-std::max(a.x,b.x)>tol;
    if(xOverlap&&zOverlap) return fail("Overlapping terrain members");
    if(zOverlap&&(near(ax,b.x)||near(bx,a.x))) {
      if(!near(a.z,b.z)||!near(az,bz)) return fail("Neighbor size mismatch or partial edge");
      if(a.field.depth!=b.field.depth||!near(a.field.step,b.field.step)) return fail("Neighbor resolution mismatch");
      side=near(ax,b.x)?2:0;
    } else if(xOverlap&&(near(az,b.z)||near(bz,a.z))) {
      if(!near(a.x,b.x)||!near(ax,bx)) return fail("Neighbor size mismatch or partial edge");
      if(a.field.width!=b.field.width||!near(a.field.step,b.field.step)) return fail("Neighbor resolution mismatch");
      side=near(az,b.z)?1:3;
    }
    if(side<0) continue;
    if(!occupied.insert({a.member,u32(side)}).second||!occupied.insert({b.member,u32((side+2)%4)}).second) return fail("Ambiguous terrain edge");
    links.push_back({a.member,b.member,TerrainNeighborSide(side)});
  }
  std::sort(links.begin(),links.end(),[](auto& a,auto& b){return std::pair(a.first,a.second)<std::pair(b.first,b.second);});
  out=std::move(links); error.clear(); return true;
}
bool reconcileTerrainNeighbors(std::vector<TerrainNeighborTile>& tiles,std::string& error,double tol) {
  std::vector<TerrainNeighborLink> links;
  if(!terrainNeighborLinks(tiles,links,error,tol)) return false;
  auto next=tiles;
  std::map<TerrainIdentity,size_t> index;
  std::vector<size_t> offset(next.size()+1);
  for(size_t i=0;i<next.size();++i) { index[next[i].member]=i; offset[i+1]=offset[i]+next[i].field.heights.size(); }
  Sets components(next.size()), samples(offset.back());
  for(auto& l:links) {
    size_t a=index[l.first],b=index[l.second]; components.join(a,b);
    auto& af=next[a].field; auto& bf=next[b].field;
    bool horizontal=l.side==TerrainNeighborSide::Left||l.side==TerrainNeighborSide::Right;
    size_t count=horizontal?af.depth:af.width;
    for(size_t k=0;k<count;++k) {
      size_t ai,bi;
      if(horizontal) { ai=k*af.width+(l.side==TerrainNeighborSide::Right?af.width-1:0); bi=k*bf.width+(l.side==TerrainNeighborSide::Left?bf.width-1:0); }
      else { ai=(l.side==TerrainNeighborSide::Top?af.depth-1:0)*af.width+k; bi=(l.side==TerrainNeighborSide::Bottom?bf.depth-1:0)*bf.width+k; }
      samples.join(offset[a]+ai,offset[b]+bi);
    }
  }
  std::map<size_t,std::set<TerrainIdentity>> unions;
  for(size_t i=0;i<next.size();++i) unions[components.root(i)].insert(next[i].layers.begin(),next[i].layers.end());
  for(size_t i=0;i<next.size();++i) {
    auto& t=next[i]; auto& ids=unions[components.root(i)];
    std::vector<TerrainIdentity> ordered(ids.begin(),ids.end());
    std::vector<float> weights(t.field.heights.size()*ordered.size(),0);
    for(size_t l=0;l<t.layers.size();++l) {
      size_t dest=std::lower_bound(ordered.begin(),ordered.end(),t.layers[l])-ordered.begin();
      for(size_t s=0;s<t.field.heights.size();++s) weights[s*ordered.size()+dest]=t.weights[s*t.layers.size()+l];
    }
    t.layers=std::move(ordered); t.weights=std::move(weights);
  }
  struct Sum { double height=0; size_t count=0; std::vector<double> weights; };
  std::map<size_t,Sum> sums;
  // Stable identity ordering makes floating-point accumulation independent of
  // caller tile order, including corners shared by more than two members.
  for(auto [id,i]:index) {
    auto& t=next[i];
    for(size_t s=0;s<t.field.heights.size();++s) {
      auto& sum=sums[samples.root(offset[i]+s)];
      sum.height+=double(t.field.heights[s])+t.y; ++sum.count;
      if(sum.weights.empty()) sum.weights.resize(t.layers.size());
      for(size_t l=0;l<t.layers.size();++l) sum.weights[l]+=t.weights[s*t.layers.size()+l];
    }
  }
  for(size_t i=0;i<next.size();++i) {
    auto& t=next[i];
    for(size_t s=0;s<t.field.heights.size();++s) {
      auto& sum=sums[samples.root(offset[i]+s)]; if(sum.count==1) continue;
      double h=sum.height/sum.count-t.y;
      if(!finite(float(h))) { error="Reconciled local height exceeds float range"; return false; }
      t.field.heights[s]=float(h);
      for(size_t l=0;l<t.layers.size();++l) t.weights[s*t.layers.size()+l]=float(sum.weights[l]/sum.count);
    }
  }
  tiles=std::move(next); error.clear(); return true;
}
} // namespace ae::resources
