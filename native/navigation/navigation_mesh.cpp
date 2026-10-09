#include "navigation/navigation_mesh.h"

#include <DetourCommon.h>
#include <DetourCrowd.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <DetourTileCache.h>
#include <DetourTileCacheBuilder.h>
#include <Recast.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

namespace ae::navigation {
namespace {

constexpr char Magic[8]{'A','S','T','R','A','N','A','V'};
constexpr u32 MaximumLayersPerTile=8;
constexpr u32 MaximumTiles=4096;
constexpr int MaximumObstacles=256;
constexpr int MaximumPathPolygons=512;
constexpr int MaximumCorners=256;

u64 fnv(const void *data,usize size,u64 hash=1469598103934665603ull) {
  const auto *bytes=static_cast<const u8 *>(data);
  for(usize i=0;i<size;++i) {hash^=bytes[i];hash*=1099511628211ull;}
  return hash;
}
u32 nextPow2(u32 v) {v=std::max(v,1u)-1;v|=v>>1;v|=v>>2;v|=v>>4;v|=v>>8;v|=v>>16;return v+1;}
u32 ilog2(u32 v) {u32 r=0;while(v>1) {v>>=1;++r;}return r;}
bool finite3(const float *v) {return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);}

// O formato guarda as camadas sem compressão: cópia simples. O TileCache só
// exige o contrato; um compressor real pode entrar com outra versão do formato.
struct CopyCompressor final : dtTileCacheCompressor {
  int maxCompressedSize(const int bufferSize) override {return bufferSize;}
  dtStatus compress(const unsigned char *buffer,const int bufferSize,unsigned char *compressed,
                    const int maxCompressedSize,int *compressedSize) override {
    if(bufferSize>maxCompressedSize) return DT_FAILURE|DT_BUFFER_TOO_SMALL;
    std::memcpy(compressed,buffer,static_cast<usize>(bufferSize));*compressedSize=bufferSize;return DT_SUCCESS;
  }
  dtStatus decompress(const unsigned char *compressed,const int compressedSize,unsigned char *buffer,
                      const int maxBufferSize,int *bufferSize) override {
    if(compressedSize>maxBufferSize) return DT_FAILURE|DT_BUFFER_TOO_SMALL;
    std::memcpy(buffer,compressed,static_cast<usize>(compressedSize));*bufferSize=compressedSize;return DT_SUCCESS;
  }
};

// Alocador linear do montador de tiles (o mesmo modelo do exemplo do Recast):
// tudo que um buildNavMeshTile pede é solto de uma vez no reset().
struct LinearAllocator final : dtTileCacheAlloc {
  std::vector<unsigned char> buffer;usize top=0;
  explicit LinearAllocator(usize capacity):buffer(capacity) {}
  void reset() override {top=0;}
  void *alloc(const size_t size) override {
    const usize aligned=(size+15)&~usize{15};
    if(top+aligned>buffer.size()) return nullptr;
    void *memory=buffer.data()+top;top+=aligned;return memory;
  }
  void free(void *) override {}
};

struct LinkArrays {
  std::vector<float> vertices,radii;
  std::vector<unsigned short> flags;
  std::vector<unsigned char> areas,directions;
  std::vector<unsigned int> ids;
  void assign(std::span<const OffMeshLink> links) {
    vertices.clear();radii.clear();flags.clear();areas.clear();directions.clear();ids.clear();
    for(const auto &l:links) {
      vertices.insert(vertices.end(),{l.start[0],l.start[1],l.start[2],l.end[0],l.end[1],l.end[2]});
      radii.push_back(l.radius);areas.push_back(l.area);flags.push_back(areaFlags(l.area));
      directions.push_back(l.bidirectional?DT_OFFMESH_CON_BIDIR:0);ids.push_back(l.id);
    }
  }
};

struct MeshProcess final : dtTileCacheMeshProcess {
  const LinkArrays *links=nullptr;
  void process(dtNavMeshCreateParams *params,unsigned char *polyAreas,unsigned short *polyFlags) override {
    for(int i=0;i<params->polyCount;++i) {
      if(polyAreas[i]==DT_TILECACHE_WALKABLE_AREA) polyAreas[i]=AreaWalkable;
      polyFlags[i]=areaFlags(polyAreas[i]);
    }
    if(!links||links->ids.empty()) return;
    params->offMeshConVerts=links->vertices.data();params->offMeshConRad=links->radii.data();
    params->offMeshConFlags=links->flags.data();params->offMeshConAreas=links->areas.data();
    params->offMeshConDir=links->directions.data();params->offMeshConUserID=links->ids.data();
    params->offMeshConCount=static_cast<int>(links->ids.size());
  }
};

struct Writer {
  std::string out;
  template<class T> void put(const T &v) {out.append(reinterpret_cast<const char *>(&v),sizeof(T));}
  void bytes(const void *p,usize n) {out.append(static_cast<const char *>(p),n);}
};
struct Reader {
  std::string_view in;usize at=0;
  template<class T> bool get(T &v) {if(in.size()-at<sizeof(T)) return false;std::memcpy(&v,in.data()+at,sizeof(T));at+=sizeof(T);return true;}
  bool bytes(void *p,usize n) {if(in.size()-at<n) return false;std::memcpy(p,in.data()+at,n);at+=n;return true;}
};

void settingsWrite(Writer &w,const BakeSettings &s) {
  for(const float v:{s.agentRadius,s.agentHeight,s.agentMaxClimb,s.agentMaxSlope,s.cellSize,s.cellHeight,s.minRegionArea,
                     s.edgeMaxLength,s.edgeMaxError,s.detailSampleDistance,s.detailSampleMaxError}) w.put(v);
  w.put(s.tileSize);
}
bool settingsRead(Reader &r,BakeSettings &s) {
  for(float *v:{&s.agentRadius,&s.agentHeight,&s.agentMaxClimb,&s.agentMaxSlope,&s.cellSize,&s.cellHeight,&s.minRegionArea,
                &s.edgeMaxLength,&s.edgeMaxError,&s.detailSampleDistance,&s.detailSampleMaxError}) if(!r.get(*v)) return false;
  return r.get(s.tileSize);
}

bool layerHeaderValid(const std::vector<u8> &layer) {
  if(layer.size()<sizeof(dtTileCacheLayerHeader)) return false;
  dtTileCacheLayerHeader header;std::memcpy(&header,layer.data(),sizeof(header));
  return header.magic==DT_TILECACHE_MAGIC&&header.version==DT_TILECACHE_VERSION&&header.tx>=0&&header.ty>=0&&
         header.tlayer>=0&&header.tlayer<static_cast<int>(MaximumLayersPerTile);
}

} // namespace

bool BakeSettings::valid() const {
  const float values[]{agentRadius,agentHeight,agentMaxClimb,agentMaxSlope,cellSize,cellHeight,minRegionArea,
                       edgeMaxLength,edgeMaxError,detailSampleDistance,detailSampleMaxError};
  for(const float v:values) if(!std::isfinite(v)) return false;
  return agentRadius>=0&&agentRadius<=50&&agentHeight>=.05f&&agentHeight<=100&&agentMaxClimb>=0&&agentMaxClimb<=50&&
         agentMaxSlope>=0&&agentMaxSlope<=89&&cellSize>=.01f&&cellSize<=10&&cellHeight>=.01f&&cellHeight<=10&&
         minRegionArea>=0&&minRegionArea<=10000&&edgeMaxLength>=0&&edgeMaxLength<=1000&&edgeMaxError>=.1f&&edgeMaxError<=10&&
         detailSampleDistance>=0&&detailSampleDistance<=100&&detailSampleMaxError>=0&&detailSampleMaxError<=100&&
         tileSize>=16&&tileSize<=128;
}

u64 BakeGeometry::hash() const {
  u64 h=fnv(vertices.data(),vertices.size()*sizeof(float));
  h=fnv(indices.data(),indices.size()*sizeof(u32),h);
  h=fnv(areas.data(),areas.size(),h);
  if(hasVolume) {h=fnv(volumeMin,sizeof(volumeMin),h);h=fnv(volumeMax,sizeof(volumeMax),h);}
  return h;
}
u64 bakeHash(const BakeGeometry &geometry,const BakeSettings &settings) {
  Writer w;settingsWrite(w,settings);
  return fnv(w.out.data(),w.out.size(),geometry.hash());
}

bool NavMeshData::valid() const {
  if(!settings.valid()||tilesX==0||tilesZ==0||tilesX*tilesZ>MaximumTiles||name.size()>256) return false;
  for(u32 k=0;k<3;++k) if(!std::isfinite(boundsMin[k])||!std::isfinite(boundsMax[k])||boundsMin[k]>boundsMax[k]) return false;
  for(const auto &layer:layers) if(!layerHeaderValid(layer)) return false;
  return true;
}

std::string NavMeshData::serialize() const {
  Writer w;w.bytes(Magic,sizeof(Magic));w.put(FormatVersion);w.put(guid.high);w.put(guid.low);
  w.put(static_cast<u32>(name.size()));w.bytes(name.data(),name.size());
  settingsWrite(w,settings);
  for(u32 k=0;k<3;++k) w.put(boundsMin[k]);
  for(u32 k=0;k<3;++k) w.put(boundsMax[k]);
  w.put(tilesX);w.put(tilesZ);w.put(sourceHash);w.put(bakeSeconds);
  w.put(stats.tiles);w.put(stats.layers);w.put(stats.polygons);w.put(stats.vertices);w.put(stats.area);
  w.put(static_cast<u32>(layers.size()));
  for(const auto &layer:layers) {w.put(static_cast<u32>(layer.size()));w.bytes(layer.data(),layer.size());}
  w.put(fnv(w.out.data(),w.out.size()));
  return std::move(w.out);
}

bool NavMeshData::deserialize(std::string_view bytes,NavMeshData &out,std::string *error) {
  const auto fail=[&](const char *why) {if(error) *error=why;return false;};
  if(bytes.size()>MaximumBytes) return fail("Navmesh maior que o limite");
  if(bytes.size()<sizeof(Magic)+sizeof(u64)||std::memcmp(bytes.data(),Magic,sizeof(Magic))!=0) return fail("Arquivo não é uma navmesh da Astra");
  u64 stored=0;std::memcpy(&stored,bytes.data()+bytes.size()-sizeof(u64),sizeof(u64));
  if(stored!=fnv(bytes.data(),bytes.size()-sizeof(u64))) return fail("Navmesh corrompida (soma de verificação)");
  Reader r{bytes.substr(0,bytes.size()-sizeof(u64)),sizeof(Magic)};
  NavMeshData data;u32 version=0,nameSize=0,count=0;
  if(!r.get(version)) return fail("Navmesh truncada");
  if(version!=FormatVersion) return fail("Versão de navmesh desconhecida; refaça o bake");
  if(!r.get(data.guid.high)||!r.get(data.guid.low)||!r.get(nameSize)||nameSize>256) return fail("Cabeçalho inválido");
  data.name.resize(nameSize);
  if(!r.bytes(data.name.data(),nameSize)||!settingsRead(r,data.settings)) return fail("Configuração inválida");
  for(u32 k=0;k<3;++k) if(!r.get(data.boundsMin[k])) return fail("Limites inválidos");
  for(u32 k=0;k<3;++k) if(!r.get(data.boundsMax[k])) return fail("Limites inválidos");
  if(!r.get(data.tilesX)||!r.get(data.tilesZ)||!r.get(data.sourceHash)||!r.get(data.bakeSeconds)||
     !r.get(data.stats.tiles)||!r.get(data.stats.layers)||!r.get(data.stats.polygons)||!r.get(data.stats.vertices)||
     !r.get(data.stats.area)||!r.get(count)||count>MaximumTiles*MaximumLayersPerTile) return fail("Navmesh truncada");
  data.layers.resize(count);
  for(auto &layer:data.layers) {
    u32 size=0;if(!r.get(size)||size>r.in.size()-r.at) return fail("Camada truncada");
    layer.resize(size);if(!r.bytes(layer.data(),size)) return fail("Camada truncada");
  }
  if(r.at!=r.in.size()) return fail("Dados extras no fim da navmesh");
  if(!data.valid()) return fail("Navmesh inválida");
  out=std::move(data);return true;
}

BakeStatus bake(const BakeGeometry &geometry,const BakeSettings &settings,BakeProgress &progress,NavMeshData &out,std::string &error) {
  const auto started=std::chrono::steady_clock::now();
  progress.done=0;progress.total=0;
  if(!settings.valid()) {error="Configuração de bake inválida";return BakeStatus::Failed;}
  const usize triangles=geometry.triangleCount();
  if(geometry.indices.size()%3||geometry.areas.size()!=triangles||geometry.vertices.size()%3) {error="Geometria inconsistente";return BakeStatus::Failed;}
  if(!triangles) {error="Nenhum colisor estático para assar";return BakeStatus::Empty;}
  const usize vertexCount=geometry.vertices.size()/3;
  if(vertexCount>static_cast<usize>(std::numeric_limits<int>::max())||triangles>static_cast<usize>(std::numeric_limits<int>::max()/3)) {
    error="Geometria grande demais";return BakeStatus::Failed;
  }
  for(const u32 index:geometry.indices) if(index>=vertexCount) {error="Índice fora da geometria";return BakeStatus::Failed;}
  for(const float v:geometry.vertices) if(!std::isfinite(v)) {error="Vértice inválido";return BakeStatus::Failed;}

  float bmin[3]{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
  float bmax[3]{-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()};
  std::vector<float> triangleBounds(triangles*4);   // minx, minz, maxx, maxz
  for(usize t=0;t<triangles;++t) {
    float tmin[3]{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
    float tmax[3]{-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()};
    for(u32 c=0;c<3;++c) {
      const float *v=&geometry.vertices[geometry.indices[t*3+c]*3];
      for(u32 k=0;k<3;++k) {tmin[k]=std::min(tmin[k],v[k]);tmax[k]=std::max(tmax[k],v[k]);}
    }
    triangleBounds[t*4]=tmin[0];triangleBounds[t*4+1]=tmin[2];triangleBounds[t*4+2]=tmax[0];triangleBounds[t*4+3]=tmax[2];
    for(u32 k=0;k<3;++k) {bmin[k]=std::min(bmin[k],tmin[k]);bmax[k]=std::max(bmax[k],tmax[k]);}
  }
  if(geometry.hasVolume) for(u32 k=0;k<3;++k) {bmin[k]=std::max(bmin[k],geometry.volumeMin[k]);bmax[k]=std::min(bmax[k],geometry.volumeMax[k]);}
  if(bmin[0]>=bmax[0]||bmin[2]>=bmax[2]||bmin[1]>bmax[1]) {error="Nenhuma geometria dentro do volume";return BakeStatus::Empty;}
  bmax[1]+=settings.agentHeight;   // espaço livre acima do chão mais alto

  const float cs=settings.cellSize,ch=settings.cellHeight;
  int gridW=0,gridH=0;rcCalcGridSize(bmin,bmax,cs,&gridW,&gridH);
  const int ts=static_cast<int>(settings.tileSize);
  const int tilesX=(gridW+ts-1)/ts,tilesZ=(gridH+ts-1)/ts;
  if(tilesX<=0||tilesZ<=0) {error="Área vazia";return BakeStatus::Empty;}
  if(static_cast<u32>(tilesX*tilesZ)>MaximumTiles) {error="Área grande demais para o tamanho de tile; aumente as células ou o tile";return BakeStatus::Failed;}
  const float tcs=static_cast<float>(ts)*cs;

  rcConfig cfg{};
  cfg.cs=cs;cfg.ch=ch;cfg.walkableSlopeAngle=settings.agentMaxSlope;
  cfg.walkableHeight=static_cast<int>(std::ceil(settings.agentHeight/ch));
  cfg.walkableClimb=static_cast<int>(std::floor(settings.agentMaxClimb/ch));
  cfg.walkableRadius=static_cast<int>(std::ceil(settings.agentRadius/cs));
  cfg.maxEdgeLen=static_cast<int>(settings.edgeMaxLength/cs);
  cfg.maxSimplificationError=settings.edgeMaxError;
  cfg.minRegionArea=static_cast<int>(settings.minRegionArea/(cs*cs));
  cfg.mergeRegionArea=cfg.minRegionArea*10;
  cfg.maxVertsPerPoly=DT_VERTS_PER_POLYGON;
  cfg.tileSize=ts;cfg.borderSize=cfg.walkableRadius+3;
  cfg.width=ts+cfg.borderSize*2;cfg.height=ts+cfg.borderSize*2;
  cfg.detailSampleDist=settings.detailSampleDistance<.9f?0:cs*settings.detailSampleDistance;
  cfg.detailSampleMaxError=ch*settings.detailSampleMaxError;
  if(cfg.width>255) {error="Tile grande demais para o raio do agente";return BakeStatus::Failed;}

  rcContext context(false);
  CopyCompressor compressor;
  NavMeshData data;
  data.settings=settings;data.tilesX=static_cast<u32>(tilesX);data.tilesZ=static_cast<u32>(tilesZ);
  for(u32 k=0;k<3;++k) {data.boundsMin[k]=bmin[k];data.boundsMax[k]=bmax[k];}
  progress.total=static_cast<u32>(tilesX*tilesZ);
  std::vector<int> tileTriangles;std::vector<unsigned char> tileAreas;
  const int *indices=reinterpret_cast<const int *>(geometry.indices.data());
  for(int ty=0;ty<tilesZ;++ty) for(int tx=0;tx<tilesX;++tx) {
    if(progress.cancel.load()) {error="Bake cancelado";return BakeStatus::Cancelled;}
    rcConfig tile=cfg;
    tile.bmin[0]=bmin[0]+static_cast<float>(tx)*tcs-static_cast<float>(cfg.borderSize)*cs;tile.bmin[1]=bmin[1];
    tile.bmin[2]=bmin[2]+static_cast<float>(ty)*tcs-static_cast<float>(cfg.borderSize)*cs;
    tile.bmax[0]=bmin[0]+static_cast<float>(tx+1)*tcs+static_cast<float>(cfg.borderSize)*cs;tile.bmax[1]=bmax[1];
    tile.bmax[2]=bmin[2]+static_cast<float>(ty+1)*tcs+static_cast<float>(cfg.borderSize)*cs;
    tileTriangles.clear();tileAreas.clear();
    for(usize t=0;t<triangles;++t) {
      const float *b=&triangleBounds[t*4];
      if(b[0]>tile.bmax[0]||b[2]<tile.bmin[0]||b[1]>tile.bmax[2]||b[3]<tile.bmin[2]) continue;
      tileTriangles.insert(tileTriangles.end(),{indices[t*3],indices[t*3+1],indices[t*3+2]});
      tileAreas.push_back(geometry.areas[t]);
    }
    progress.done=progress.done.load()+1;
    if(tileTriangles.empty()) continue;
    const int tileCount=static_cast<int>(tileAreas.size());
    std::vector<unsigned char> marked(tileAreas.size(),RC_NULL_AREA);
    rcMarkWalkableTriangles(&context,tile.walkableSlopeAngle,geometry.vertices.data(),static_cast<int>(vertexCount),
                            tileTriangles.data(),tileCount,marked.data());
    for(usize i=0;i<marked.size();++i)
      if(tileAreas[i]==AreaNotWalkable) marked[i]=RC_NULL_AREA;
      else if(marked[i]!=RC_NULL_AREA&&(tileAreas[i]==AreaJump||tileAreas[i]==AreaDifficult)) marked[i]=tileAreas[i];

    rcHeightfield *solid=rcAllocHeightfield();rcCompactHeightfield *compact=rcAllocCompactHeightfield();
    rcHeightfieldLayerSet *set=rcAllocHeightfieldLayerSet();
    bool ok=solid&&compact&&set&&rcCreateHeightfield(&context,*solid,tile.width,tile.height,tile.bmin,tile.bmax,cs,ch)&&
            rcRasterizeTriangles(&context,geometry.vertices.data(),static_cast<int>(vertexCount),tileTriangles.data(),
                                 marked.data(),tileCount,*solid,tile.walkableClimb);
    if(ok) {
      rcFilterLowHangingWalkableObstacles(&context,tile.walkableClimb,*solid);
      rcFilterLedgeSpans(&context,tile.walkableHeight,tile.walkableClimb,*solid);
      rcFilterWalkableLowHeightSpans(&context,tile.walkableHeight,*solid);
      ok=rcBuildCompactHeightfield(&context,tile.walkableHeight,tile.walkableClimb,*solid,*compact);
      // A rasterização funde vãos dentro da altura de degrau e fica com a
      // MAIOR área: uma faixa "não caminhável" fina sobre o chão voltaria a ser
      // chão. As áreas dos Modificadores são marcadas de novo no heightfield
      // compacto, num prisma por triângulo, antes da erosão pelo raio.
      for(usize i=0;ok&&i<tileAreas.size();++i) {
        if(tileAreas[i]==AreaWalkable) continue;
        float corners[9];float low=std::numeric_limits<float>::max(),high=-std::numeric_limits<float>::max();
        for(u32 c=0;c<3;++c) {
          const float *v=&geometry.vertices[static_cast<usize>(tileTriangles[i*3+c])*3];
          corners[c*3]=v[0];corners[c*3+1]=v[1];corners[c*3+2]=v[2];
          low=std::min(low,v[1]);high=std::max(high,v[1]);
        }
        rcMarkConvexPolyArea(&context,corners,3,low-settings.agentMaxClimb,high+settings.agentMaxClimb,
                             tileAreas[i]==AreaNotWalkable?RC_NULL_AREA:tileAreas[i],*compact);
      }
      ok=ok&&rcErodeWalkableArea(&context,tile.walkableRadius,*compact)&&
         rcBuildHeightfieldLayers(&context,*compact,tile.borderSize,tile.walkableHeight,*set);
    }
    for(int i=0;ok&&i<set->nlayers&&i<static_cast<int>(MaximumLayersPerTile);++i) {
      const rcHeightfieldLayer &layer=set->layers[i];
      dtTileCacheLayerHeader header{};
      header.magic=DT_TILECACHE_MAGIC;header.version=DT_TILECACHE_VERSION;
      header.tx=tx;header.ty=ty;header.tlayer=i;
      dtVcopy(header.bmin,layer.bmin);dtVcopy(header.bmax,layer.bmax);
      header.width=static_cast<unsigned char>(layer.width);header.height=static_cast<unsigned char>(layer.height);
      header.minx=static_cast<unsigned char>(layer.minx);header.maxx=static_cast<unsigned char>(layer.maxx);
      header.miny=static_cast<unsigned char>(layer.miny);header.maxy=static_cast<unsigned char>(layer.maxy);
      header.hmin=static_cast<unsigned short>(layer.hmin);header.hmax=static_cast<unsigned short>(layer.hmax);
      unsigned char *bytes=nullptr;int size=0;
      if(dtStatusFailed(dtBuildTileCacheLayer(&compressor,&header,layer.heights,layer.areas,layer.cons,&bytes,&size))) {ok=false;break;}
      data.layers.emplace_back(bytes,bytes+size);dtFree(bytes);
    }
    rcFreeHeightfieldLayerSet(set);rcFreeCompactHeightfield(compact);rcFreeHeightField(solid);
    if(!ok) {error="Recast recusou o tile "+std::to_string(tx)+","+std::to_string(ty);return BakeStatus::Failed;}
  }
  if(data.layers.empty()) {error="Nenhuma superfície caminhável com estas medidas de agente";return BakeStatus::Empty;}
  data.sourceHash=bakeHash(geometry,settings);
  NavWorld probe;
  if(!probe.load(data,{},1,error)) return BakeStatus::Failed;
  data.stats=probe.stats();
  data.bakeSeconds=std::chrono::duration<float>(std::chrono::steady_clock::now()-started).count();
  out=std::move(data);return BakeStatus::Ok;
}

// --- mundo de navegação ---------------------------------------------------------

struct NavWorld::Impl {
  CopyCompressor compressor;
  LinearAllocator allocator{2u*1024u*1024u};
  MeshProcess process;
  LinkArrays links;
  std::vector<OffMeshLink> linkList;
  dtNavMesh *mesh=nullptr;
  dtTileCache *cache=nullptr;
  dtNavMeshQuery *query=nullptr;
  dtCrowd *crowd=nullptr;
  std::array<AgentFilter,DT_CROWD_MAX_QUERY_FILTER_TYPE> filters{};
  u32 filterCount=1;
  float tileWidth=0,orig[3]{};
  ~Impl() {release();}
  void release() {
    if(crowd) dtFreeCrowd(crowd);
    if(query) dtFreeNavMeshQuery(query);
    if(cache) dtFreeTileCache(cache);
    if(mesh) dtFreeNavMesh(mesh);
    crowd=nullptr;query=nullptr;cache=nullptr;mesh=nullptr;filterCount=1;
  }
  static void configure(dtQueryFilter &q,const AgentFilter &f) {
    q.setIncludeFlags(f.include);q.setExcludeFlags(0);
    for(int a=0;a<DT_MAX_AREAS;++a) q.setAreaCost(a,1.f);
    q.setAreaCost(AreaJump,f.jumpCost);q.setAreaCost(AreaDifficult,f.difficultCost);
  }
  int filterSlot(const AgentFilter &f) {
    for(u32 i=0;i<filterCount;++i) if(filters[i]==f) return static_cast<int>(i);
    if(filterCount>=filters.size()) return -1;
    filters[filterCount]=f;configure(*crowd->getEditableFilter(static_cast<int>(filterCount)),f);
    return static_cast<int>(filterCount++);
  }
  bool rebuildTile(int tx,int ty) {return cache&&mesh&&dtStatusSucceed(cache->buildNavMeshTilesAt(tx,ty,mesh));}
  void tileOf(const float *p,int &tx,int &ty) const {
    tx=static_cast<int>(std::floor((p[0]-orig[0])/tileWidth));ty=static_cast<int>(std::floor((p[2]-orig[2])/tileWidth));
  }
};

NavWorld::NavWorld():impl_(std::make_unique<Impl>()) {}
NavWorld::~NavWorld()=default;
bool NavWorld::loaded() const {return impl_->mesh!=nullptr;}
void NavWorld::clear() {impl_->release();impl_->linkList.clear();impl_->links.assign({});data_.reset();}

bool NavWorld::load(const NavMeshData &data,std::span<const OffMeshLink> links,u32 maxAgents,std::string &error) {
  clear();
  if(!data.valid()||data.layers.empty()) {error="Navmesh inválida ou vazia";return false;}
  auto &i=*impl_;
  i.linkList.assign(links.begin(),links.end());i.links.assign(i.linkList);i.process.links=&i.links;
  const auto &s=data.settings;
  dtTileCacheParams tc{};
  dtVcopy(tc.orig,data.boundsMin);tc.cs=s.cellSize;tc.ch=s.cellHeight;
  tc.width=static_cast<int>(s.tileSize);tc.height=static_cast<int>(s.tileSize);
  tc.walkableHeight=s.agentHeight;tc.walkableRadius=s.agentRadius;tc.walkableClimb=s.agentMaxClimb;
  tc.maxSimplificationError=s.edgeMaxError;
  tc.maxTiles=static_cast<int>(data.layers.size()+8);tc.maxObstacles=MaximumObstacles;
  i.cache=dtAllocTileCache();
  if(!i.cache||dtStatusFailed(i.cache->init(&tc,&i.allocator,&i.compressor,&i.process))) {error="TileCache recusou a navmesh";clear();return false;}
  const u32 tileBits=std::min(ilog2(nextPow2(static_cast<u32>(data.layers.size()))),14u);
  dtNavMeshParams params{};
  dtVcopy(params.orig,data.boundsMin);
  params.tileWidth=static_cast<float>(s.tileSize)*s.cellSize;params.tileHeight=params.tileWidth;
  params.maxTiles=1<<tileBits;params.maxPolys=1<<(22-tileBits);
  i.tileWidth=params.tileWidth;dtVcopy(i.orig,data.boundsMin);
  i.mesh=dtAllocNavMesh();
  if(!i.mesh||dtStatusFailed(i.mesh->init(&params))) {error="Detour recusou a navmesh";clear();return false;}
  for(const auto &layer:data.layers) {
    auto *copy=static_cast<unsigned char *>(dtAlloc(static_cast<int>(layer.size()),DT_ALLOC_PERM));
    if(!copy) {error="Memória insuficiente para a navmesh";clear();return false;}
    std::memcpy(copy,layer.data(),layer.size());
    if(dtStatusFailed(i.cache->addTile(copy,static_cast<int>(layer.size()),DT_COMPRESSEDTILE_FREE_DATA,nullptr))) {
      dtFree(copy);error="Camada de tile inválida";clear();return false;
    }
  }
  for(u32 ty=0;ty<data.tilesZ;++ty) for(u32 tx=0;tx<data.tilesX;++tx)
    if(!i.rebuildTile(static_cast<int>(tx),static_cast<int>(ty))) {error="Falha ao montar o tile "+std::to_string(tx)+","+std::to_string(ty);clear();return false;}
  i.query=dtAllocNavMeshQuery();
  if(!i.query||dtStatusFailed(i.query->init(i.mesh,2048))) {error="Consulta de navegação recusada";clear();return false;}
  i.crowd=dtAllocCrowd();
  const float maxRadius=std::max(2.f,s.agentRadius*4);
  if(!i.crowd||!i.crowd->init(static_cast<int>(std::clamp(maxAgents,1u,512u)),maxRadius,i.mesh)) {error="Multidão recusada";clear();return false;}
  // Qualidades de desvio do exemplo do Detour: baixa, média, boa, alta.
  const int divs[4]{5,5,7,7},rings[4]{2,2,2,3},depth[4]{1,2,3,3};
  for(int q=0;q<4;++q) {
    dtObstacleAvoidanceParams avoid=*i.crowd->getObstacleAvoidanceParams(0);
    avoid.velBias=.5f;avoid.adaptiveDivs=static_cast<unsigned char>(divs[q]);
    avoid.adaptiveRings=static_cast<unsigned char>(rings[q]);avoid.adaptiveDepth=static_cast<unsigned char>(depth[q]);
    i.crowd->setObstacleAvoidanceParams(q,&avoid);
  }
  i.filters[0]=AgentFilter{};Impl::configure(*i.crowd->getEditableFilter(0),i.filters[0]);
  data_=std::make_unique<NavMeshData>(data);
  return true;
}

bool NavWorld::setLinks(std::span<const OffMeshLink> links) {
  auto &i=*impl_;if(!i.mesh) return false;
  std::vector<std::pair<int,int>> tiles;
  const auto touch=[&](const OffMeshLink &l) {int tx=0,ty=0;i.tileOf(l.start,tx,ty);tiles.emplace_back(tx,ty);};
  for(const auto &l:i.linkList) touch(l);
  for(const auto &l:links) touch(l);
  std::sort(tiles.begin(),tiles.end());tiles.erase(std::unique(tiles.begin(),tiles.end()),tiles.end());
  i.linkList.assign(links.begin(),links.end());i.links.assign(i.linkList);
  bool ok=true;
  for(const auto &[tx,ty]:tiles)
    if(tx>=0&&ty>=0&&static_cast<u32>(tx)<data_->tilesX&&static_cast<u32>(ty)<data_->tilesZ) ok&=i.rebuildTile(tx,ty);
  return ok;
}

u32 NavWorld::addCylinderObstacle(const float base[3],float radius,float height) {
  dtObstacleRef ref=0;
  if(!impl_->cache||!finite3(base)||!(radius>0)||!(height>0)) return 0;
  return dtStatusSucceed(impl_->cache->addObstacle(base,radius,height,&ref))?ref:0;
}
u32 NavWorld::addBoxObstacle(const float center[3],const float halfExtents[3],float yawRadians) {
  dtObstacleRef ref=0;
  if(!impl_->cache||!finite3(center)||!finite3(halfExtents)||!std::isfinite(yawRadians)) return 0;
  return dtStatusSucceed(impl_->cache->addBoxObstacle(center,halfExtents,yawRadians,&ref))?ref:0;
}
bool NavWorld::removeObstacle(u32 obstacle) {return impl_->cache&&obstacle&&dtStatusSucceed(impl_->cache->removeObstacle(obstacle));}
bool NavWorld::updateObstacles(bool &upToDate) {
  upToDate=true;
  return impl_->cache&&dtStatusSucceed(impl_->cache->update(0,impl_->mesh,&upToDate));
}

bool NavWorld::nearest(const float point[3],const float extents[3],float out[3]) const {
  if(!impl_->query||!finite3(point)) return false;
  dtQueryFilter filter;Impl::configure(filter,{});dtPolyRef ref=0;
  return dtStatusSucceed(impl_->query->findNearestPoly(point,extents,&filter,&ref,out))&&ref;
}

PathStatus NavWorld::findPath(const float from[3],const float to[3],const AgentFilter &f,std::vector<float> &corners) const {
  corners.clear();
  auto *q=impl_->query;if(!q||!finite3(from)||!finite3(to)) return PathStatus::Invalid;
  dtQueryFilter filter;Impl::configure(filter,f);
  const float extents[3]{2,4,2};dtPolyRef a=0,b=0;float pa[3],pb[3];
  if(dtStatusFailed(q->findNearestPoly(from,extents,&filter,&a,pa))||!a||
     dtStatusFailed(q->findNearestPoly(to,extents,&filter,&b,pb))||!b) return PathStatus::Invalid;
  dtPolyRef path[MaximumPathPolygons];int count=0;
  const auto status=q->findPath(a,b,pa,pb,&filter,path,&count,MaximumPathPolygons);
  if(dtStatusFailed(status)||!count) return PathStatus::Invalid;
  float end[3];dtVcopy(end,pb);
  if(path[count-1]!=b) q->closestPointOnPoly(path[count-1],pb,end,nullptr);
  float straight[MaximumCorners*3];int corner=0;
  if(dtStatusFailed(q->findStraightPath(pa,end,path,count,straight,nullptr,nullptr,&corner,MaximumCorners))) return PathStatus::Invalid;
  corners.assign(straight,straight+corner*3);
  return (dtStatusDetail(status,DT_PARTIAL_RESULT)||path[count-1]!=b)?PathStatus::Partial:PathStatus::Complete;
}

bool NavWorld::raycast(const float from[3],const float to[3],const AgentFilter &f,float &fraction,float hit[3]) const {
  auto *q=impl_->query;if(!q||!finite3(from)||!finite3(to)) return false;
  dtQueryFilter filter;Impl::configure(filter,f);
  const float extents[3]{1,2,1};dtPolyRef start=0;float p[3];
  if(dtStatusFailed(q->findNearestPoly(from,extents,&filter,&start,p))||!start) return false;
  float t=0,normal[3];dtPolyRef path[MaximumPathPolygons];int count=0;
  if(dtStatusFailed(q->raycast(start,p,to,&filter,&t,normal,path,&count,MaximumPathPolygons))) return false;
  fraction=std::min(t,1.f);
  for(u32 k=0;k<3;++k) hit[k]=p[k]+(to[k]-p[k])*fraction;
  return true;
}

i32 NavWorld::addAgent(const float position[3],const AgentSettings &s) {
  auto &i=*impl_;if(!i.crowd||!finite3(position)) return -1;
  const int slot=i.filterSlot(s.filter);if(slot<0) return -1;
  dtCrowdAgentParams p{};
  p.radius=s.radius;p.height=s.height;p.maxAcceleration=s.acceleration;p.maxSpeed=s.speed;
  p.collisionQueryRange=s.radius*12;p.pathOptimizationRange=s.radius*30;p.separationWeight=s.separation;
  p.updateFlags=DT_CROWD_ANTICIPATE_TURNS|DT_CROWD_OPTIMIZE_VIS|DT_CROWD_OPTIMIZE_TOPO|
                (s.separation>0?DT_CROWD_SEPARATION:0)|(s.avoidance?DT_CROWD_OBSTACLE_AVOIDANCE:0);
  p.obstacleAvoidanceType=static_cast<unsigned char>(s.avoidance?std::min(s.avoidance,4u)-1:0);
  p.queryFilterType=static_cast<unsigned char>(slot);
  return i.crowd->addAgent(position,&p);
}
bool NavWorld::updateAgent(i32 agent,const AgentSettings &s) {
  auto &i=*impl_;if(!i.crowd) return false;
  const auto *a=i.crowd->getAgent(agent);if(!a||!a->active) return false;
  const int slot=i.filterSlot(s.filter);if(slot<0) return false;
  auto p=a->params;
  p.radius=s.radius;p.height=s.height;p.maxAcceleration=s.acceleration;p.maxSpeed=s.speed;
  p.collisionQueryRange=s.radius*12;p.pathOptimizationRange=s.radius*30;p.separationWeight=s.separation;
  p.updateFlags=DT_CROWD_ANTICIPATE_TURNS|DT_CROWD_OPTIMIZE_VIS|DT_CROWD_OPTIMIZE_TOPO|
                (s.separation>0?DT_CROWD_SEPARATION:0)|(s.avoidance?DT_CROWD_OBSTACLE_AVOIDANCE:0);
  p.obstacleAvoidanceType=static_cast<unsigned char>(s.avoidance?std::min(s.avoidance,4u)-1:0);
  p.queryFilterType=static_cast<unsigned char>(slot);
  i.crowd->updateAgentParameters(agent,&p);return true;
}
void NavWorld::removeAgent(i32 agent) {if(impl_->crowd&&agent>=0) impl_->crowd->removeAgent(agent);}

bool NavWorld::setDestination(i32 agent,const float target[3]) {
  auto &i=*impl_;if(!i.crowd||!finite3(target)) return false;
  const auto *a=i.crowd->getAgent(agent);if(!a||!a->active) return false;
  const auto *filter=i.crowd->getFilter(a->params.queryFilterType);
  dtPolyRef ref=0;float nearest[3];
  const float extents[3]{std::max(2.f,a->params.radius*4),4,std::max(2.f,a->params.radius*4)};
  if(dtStatusFailed(i.query->findNearestPoly(target,extents,filter,&ref,nearest))||!ref) return false;
  return i.crowd->requestMoveTarget(agent,ref,nearest);
}
bool NavWorld::stopAgent(i32 agent) {
  auto *a=impl_->crowd?impl_->crowd->getEditableAgent(agent):nullptr;
  if(!a||!a->active||!impl_->crowd->resetMoveTarget(agent)) return false;
  dtVset(a->vel,0,0,0);dtVset(a->dvel,0,0,0);dtVset(a->nvel,0,0,0);
  return true;
}
bool NavWorld::setAgentSpeed(i32 agent,float speed) {
  auto *a=impl_->crowd?impl_->crowd->getEditableAgent(agent):nullptr;
  if(!a||!a->active||!std::isfinite(speed)||speed<0) return false;
  a->params.maxSpeed=speed;return true;
}

bool NavWorld::syncAgent(i32 agent,const float position[3]) {
  auto &i=*impl_;if(!i.crowd||!finite3(position)) return false;
  auto *a=i.crowd->getEditableAgent(agent);if(!a||!a->active) return false;
  if(a->state==DT_CROWDAGENT_STATE_OFFMESH) return true;   // o link conduz
  const auto *filter=i.crowd->getFilter(a->params.queryFilterType);
  if(dtVdist2DSqr(position,a->npos)>1.f||a->state==DT_CROWDAGENT_STATE_INVALID) {
    // Teletransporte: recoloca o corredor e pede o mesmo alvo de novo.
    dtPolyRef ref=0;float nearest[3];const float extents[3]{a->params.radius*2,a->params.height,a->params.radius*2};
    if(dtStatusFailed(i.query->findNearestPoly(position,extents,filter,&ref,nearest))||!ref) return false;
    a->corridor.reset(ref,nearest);a->boundary.reset();dtVcopy(a->npos,nearest);a->state=DT_CROWDAGENT_STATE_WALKING;
    if(a->targetState==DT_CROWDAGENT_TARGET_VALID) {float target[3];dtVcopy(target,a->targetPos);i.crowd->requestMoveTarget(agent,a->targetRef,target);}
    return true;
  }
  if(!a->corridor.movePosition(position,i.query,filter)) return false;
  dtVcopy(a->npos,a->corridor.getPos());
  return true;
}

bool NavWorld::agentState(i32 agent,AgentState &out) const {
  out={};auto &i=*impl_;if(!i.crowd) return false;
  const auto *a=i.crowd->getAgent(agent);if(!a||!a->active) return false;
  out.valid=a->state!=DT_CROWDAGENT_STATE_INVALID;out.onLink=a->state==DT_CROWDAGENT_STATE_OFFMESH;
  out.pending=a->targetState==DT_CROWDAGENT_TARGET_REQUESTING||a->targetState==DT_CROWDAGENT_TARGET_WAITING_FOR_QUEUE||
              a->targetState==DT_CROWDAGENT_TARGET_WAITING_FOR_PATH;
  out.hasPath=a->targetState==DT_CROWDAGENT_TARGET_VALID&&a->corridor.getPathCount()>0;
  out.status=a->targetState==DT_CROWDAGENT_TARGET_FAILED?PathStatus::Invalid:
             a->targetState==DT_CROWDAGENT_TARGET_VALID?(a->partial?PathStatus::Partial:PathStatus::Complete):PathStatus::None;
  dtVcopy(out.position,a->npos);dtVcopy(out.velocity,a->vel);dtVcopy(out.desiredVelocity,a->nvel);
  dtVcopy(out.target,a->targetState==DT_CROWDAGENT_TARGET_VALID?a->corridor.getTarget():a->targetPos);
  out.corners=static_cast<u32>(std::min(a->ncorners,4));
  std::memcpy(out.cornerPoints,a->cornerVerts,sizeof(float)*3*out.corners);
  return true;
}

float NavWorld::remainingDistance(i32 agent) const {
  auto &i=*impl_;if(!i.crowd) return std::numeric_limits<float>::infinity();
  const auto *a=i.crowd->getAgent(agent);
  if(!a||!a->active||a->targetState!=DT_CROWDAGENT_TARGET_VALID||!a->corridor.getPathCount()) return std::numeric_limits<float>::infinity();
  if(a->state==DT_CROWDAGENT_STATE_OFFMESH) return dtVdist(a->npos,a->corridor.getTarget());
  float straight[MaximumCorners*3];int count=0;
  if(dtStatusFailed(i.query->findStraightPath(a->npos,a->corridor.getTarget(),a->corridor.getPath(),a->corridor.getPathCount(),
                                              straight,nullptr,nullptr,&count,MaximumCorners))||!count)
    return dtVdist(a->npos,a->corridor.getTarget());
  float total=0;const float *previous=a->npos;
  for(int c=0;c<count;++c) {total+=dtVdist(previous,&straight[c*3]);previous=&straight[c*3];}
  return total;
}

void NavWorld::advance(float seconds) {
  if(impl_->crowd&&std::isfinite(seconds)&&seconds>0) impl_->crowd->update(std::min(seconds,.25f),nullptr);
}

void NavWorld::triangles(std::vector<float> &vertices,std::vector<u8> &areas) const {
  vertices.clear();areas.clear();
  const dtNavMesh *m=impl_->mesh;if(!m) return;
  for(int t=0;t<m->getMaxTiles();++t) {
    const dtMeshTile *tile=m->getTile(t);if(!tile||!tile->header) continue;
    for(int p=0;p<tile->header->polyCount;++p) {
      const dtPoly &poly=tile->polys[p];
      if(poly.getType()==DT_POLYTYPE_OFFMESH_CONNECTION) continue;
      const dtPolyDetail &detail=tile->detailMeshes[p];
      for(int j=0;j<detail.triCount;++j) {
        const unsigned char *tri=&tile->detailTris[(detail.triBase+static_cast<unsigned>(j))*4];
        for(int k=0;k<3;++k) {
          const float *v=tri[k]<poly.vertCount?&tile->verts[poly.verts[tri[k]]*3]:
                         &tile->detailVerts[(detail.vertBase+tri[k]-poly.vertCount)*3];
          vertices.insert(vertices.end(),{v[0],v[1],v[2]});
        }
        areas.push_back(poly.getArea());
      }
    }
  }
}

void NavWorld::edges(std::vector<float> &segments) const {
  segments.clear();
  const dtNavMesh *m=impl_->mesh;if(!m) return;
  for(int t=0;t<m->getMaxTiles();++t) {
    const dtMeshTile *tile=m->getTile(t);if(!tile||!tile->header) continue;
    for(int p=0;p<tile->header->polyCount;++p) {
      const dtPoly &poly=tile->polys[p];
      if(poly.getType()==DT_POLYTYPE_OFFMESH_CONNECTION) {
        const float *a=&tile->verts[poly.verts[0]*3],*b=&tile->verts[poly.verts[1]*3];
        segments.insert(segments.end(),{a[0],a[1],a[2],b[0],b[1],b[2]});continue;
      }
      for(int j=0;j<poly.vertCount;++j) {
        if(poly.neis[j]&&!(poly.neis[j]&DT_EXT_LINK)) continue;   // aresta interna
        const float *a=&tile->verts[poly.verts[j]*3],*b=&tile->verts[poly.verts[(j+1)%poly.vertCount]*3];
        segments.insert(segments.end(),{a[0],a[1],a[2],b[0],b[1],b[2]});
      }
    }
  }
}

NavMeshStats NavWorld::stats() const {
  NavMeshStats s;const dtNavMesh *m=impl_->mesh;if(!m) return s;
  s.layers=data_?static_cast<u32>(data_->layers.size()):0;
  std::vector<std::pair<int,int>> seen;
  for(int t=0;t<m->getMaxTiles();++t) {
    const dtMeshTile *tile=m->getTile(t);if(!tile||!tile->header) continue;
    seen.emplace_back(tile->header->x,tile->header->y);
    s.vertices+=static_cast<u32>(tile->header->vertCount);
    for(int p=0;p<tile->header->polyCount;++p) {
      const dtPoly &poly=tile->polys[p];
      if(poly.getType()==DT_POLYTYPE_OFFMESH_CONNECTION) continue;
      ++s.polygons;
      for(int j=2;j<poly.vertCount;++j) {
        const float *a=&tile->verts[poly.verts[0]*3],*b=&tile->verts[poly.verts[j-1]*3],*c=&tile->verts[poly.verts[j]*3];
        s.area+=std::abs(dtTriArea2D(a,b,c))*.5f;
      }
    }
  }
  std::sort(seen.begin(),seen.end());seen.erase(std::unique(seen.begin(),seen.end()),seen.end());
  s.tiles=static_cast<u32>(seen.size());
  return s;
}

} // namespace ae::navigation
