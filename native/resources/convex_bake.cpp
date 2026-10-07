// The only exception-enabled boundary around pinned V-HACD. Exceptions never
// escape into the engine's -fno-exceptions targets.
#define ENABLE_VHACD_IMPLEMENTATION 1
#include "third_party/vhacd/VHACD.h"
#include "resources/convex_bake.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <stdexcept>

namespace ae::resources {
namespace {
const char *bakeStage(std::string_view phase) noexcept {
  if(phase=="COMPUTE_BOUNDS_OF_INPUT_MESH"||phase=="REINDEXING_INPUT_MESH"||phase=="CREATE_RAYCAST_MESH")return "Preparando a geometria";
  if(phase=="VOXELIZING_INPUT_MESH")return "Amostrando o volume";
  if(phase=="BUILD_INITIAL_CONVEX_HULL")return "Calculando a forma inicial";
  if(phase=="PERFORMING_DECOMPOSITION")return "Separando as cavidades";
  if(phase=="INITIALIZING_CONVEX_HULLS_FOR_MERGING"||phase=="COMPUTING_COST_MATRIX"||phase=="MERGING_CONVEX_HULLS")return "Ajustando as partes ao orçamento";
  if(phase=="FINALIZING_RESULTS")return "Finalizando as partes";
  return "Gerando as partes";
}
void word(std::vector<u8> &out,u32 v){for(u32 k=0;k<4;++k)out.push_back(static_cast<u8>(v>>(k*8)));}
void makeGlb(ConvexBakeResult &result,const ConvexBakeSettings &settings,const std::vector<std::string> &sources,bool topology=false) {
  std::vector<u8> binary;std::ostringstream views,accessors,meshes,nodes,roots;
  views.imbue(std::locale::classic());accessors.imbue(std::locale::classic());
  accessors<<std::setprecision(std::numeric_limits<float>::max_digits10);
  for(usize i=0;i<result.parts.size();++i) {
    const auto &p=result.parts[i];if(i){views<<',';accessors<<',';meshes<<',';nodes<<',';roots<<',';}
    const auto offset=binary.size();float low[3]{INFINITY,INFINITY,INFINITY},high[3]{-INFINITY,-INFINITY,-INFINITY};
    for(const auto &v:p.vertices)for(u32 k=0;k<3;++k){word(binary,std::bit_cast<u32>(v[k]));low[k]=std::min(low[k],v[k]);high[k]=std::max(high[k],v[k]);}
    const auto indexOffset=binary.size();for(auto v:p.indices)word(binary,v);
    views<<"{\"buffer\":0,\"byteOffset\":"<<offset<<",\"byteLength\":"<<p.vertices.size()*12<<"},"
      <<"{\"buffer\":0,\"byteOffset\":"<<indexOffset<<",\"byteLength\":"<<p.indices.size()*4<<"}";
    accessors<<"{\"bufferView\":"<<i*2<<",\"componentType\":5126,\"count\":"<<p.vertices.size()<<",\"type\":\"VEC3\",\"min\":["<<low[0]<<','<<low[1]<<','<<low[2]<<"],\"max\":["<<high[0]<<','<<high[1]<<','<<high[2]<<"]},"
      <<"{\"bufferView\":"<<i*2+1<<",\"componentType\":5125,\"count\":"<<p.indices.size()<<",\"type\":\"SCALAR\"}";
    meshes<<"{\"name\":\""<<(topology?"Collision edit ":"Convex part ")<<i+1<<"\",\"extras\":{\"sourceObjectId\":\""<<p.sourceObject<<"\"},\"primitives\":[{\"attributes\":{\"POSITION\":"<<i*2<<"},\"indices\":"<<i*2+1<<"}]}";
    nodes<<"{\"name\":\""<<(topology?"Collision edit ":"Convex part ")<<i+1<<"\",\"mesh\":"<<i<<"}";roots<<i;
  }
  std::ostringstream json;json.imbue(std::locale::classic());json<<std::setprecision(std::numeric_limits<float>::max_digits10);
  if(topology)json<<"{\"asset\":{\"version\":\"2.0\",\"generator\":\"attachsEngine collision topology\",\"extras\":{\"collisionEdit\":{\"sourceGeometryHash\":\""<<result.sourceGeometryHash<<"\",\"animationTime\":"<<settings.animationTime<<",\"sourceGuids\":[";
  else json<<"{\"asset\":{\"version\":\"2.0\",\"generator\":\"attachsEngine V-HACD 4 f900e423\",\"extras\":{\"collisionBake\":{\"sourceGeometryHash\":\""<<result.sourceGeometryHash<<"\",\"maximumParts\":"<<settings.maximumParts<<",\"voxelResolution\":"<<settings.voxelResolution<<",\"maximumVertices\":"<<settings.maximumVertices<<",\"volumeErrorPercent\":"<<settings.volumeErrorPercent<<",\"timeBudgetSeconds\":"<<settings.timeBudgetSeconds<<",\"pose\":\""<<(settings.pose==ConvexBakePose::Authored?"authored":settings.pose==ConvexBakePose::Animation?"animation":"rest")<<"\",\"animationTime\":"<<settings.animationTime<<",\"sourceGuids\":[";
  for(usize i=0;i<sources.size();++i){if(i)json<<',';json<<'"'<<sources[i]<<'"';}
  json<<"],\"authoringSources\":["<<std::setprecision(std::numeric_limits<float>::max_digits10);
  for(usize i=0;i<result.origins.size();++i) {
    if(i)json<<',';const auto &origin=result.origins[i];
    json<<"{\"objectId\":\""<<origin.object<<"\",\"slot\":"<<origin.slot<<",\"firstTriangle\":"<<origin.firstTriangle<<",\"triangleCount\":"<<origin.triangleCount<<",\"meshGuid\":\""<<origin.meshGuid<<"\",\"relativeMatrix\":[";
    for(u32 k=0;k<16;++k){if(k)json<<',';json<<origin.relative[k];}json<<"]}";
  }
  json<<"]}}},\"buffers\":[{\"byteLength\":"<<binary.size()<<"}],\"bufferViews\":["<<views.str()<<"],\"accessors\":["<<accessors.str()<<"],\"meshes\":["<<meshes.str()<<"],\"nodes\":["<<nodes.str()<<"],\"scenes\":[{\"nodes\":["<<roots.str()<<"]}],\"scene\":0}";
  auto text=json.str();while(text.size()%4)text+=' ';
  auto &out=result.glb;word(out,0x46546c67);word(out,2);word(out,static_cast<u32>(28+text.size()+binary.size()));
  word(out,static_cast<u32>(text.size()));word(out,0x4e4f534a);out.insert(out.end(),text.begin(),text.end());
  word(out,static_cast<u32>(binary.size()));word(out,0x004e4942);out.insert(out.end(),binary.begin(),binary.end());
}
}
bool writeCollisionTopologyGlb(const ConvexBakePart &part,std::string_view hash,std::vector<u8> &out,std::string &error) noexcept {
  if(part.vertices.empty()||part.vertices.size()>300000||part.indices.empty()||part.indices.size()%3||part.indices.size()>300000||hash.size()!=64){error="Geometria ou origem fora do contrato";return false;}
  for(const auto &p:part.vertices)for(auto n:p)if(!std::isfinite(n)){error="Vértice não finito";return false;}
  for(auto i:part.indices)if(i>=part.vertices.size()){error="Índice inválido";return false;}
  try {ConvexBakeResult r;r.parts.push_back(part);r.sourceGeometryHash=hash;makeGlb(r,{}, {},true);out=std::move(r.glb);error.clear();return true;}
  catch(...){error="Não foi possível serializar a malha de colisão";return false;}
}
struct ConvexBakeJob::Work final : VHACD::IVHACD::IUserCallback {
  std::thread worker;std::atomic<bool> requested{false};std::atomic<ConvexBakeStatus> status{ConvexBakeStatus::Idle};
  mutable std::mutex mutex;std::mutex engineMutex;float fraction=0;std::string stage,error;ConvexBakeResult output;
  VHACD::IVHACD *engine=nullptr;std::chrono::steady_clock::time_point began;u32 budget=0;bool timedOut=false;
  usize groupIndex=0,groupCount=1;
  void releaseEngine() {std::lock_guard lock(engineMutex);if(engine){engine->Release();engine=nullptr;}}
  void requestCancel() noexcept {requested.store(true);std::lock_guard lock(engineMutex);if(engine)engine->Cancel();}
  void Update(double overall,double,const char *phase,const char *) override {
    {std::lock_guard lock(mutex);fraction=static_cast<float>((groupIndex+overall/100)/groupCount);stage="Fonte "+std::to_string(groupIndex+1)+"/"+std::to_string(groupCount)+" · "+bakeStage(phase?phase:"");}
    timedOut=std::chrono::steady_clock::now()-began>std::chrono::seconds(budget);
    if(requested.load()||timedOut)engine->Cancel();
  }
  void run(std::vector<float> soup,ConvexBakeSettings settings,std::string hash,std::vector<std::string> sources,std::vector<ConvexBakeOrigin> origins) noexcept {
    try {
      began=std::chrono::steady_clock::now();budget=settings.timeBudgetSeconds;
      std::map<u64,std::vector<float>> solids;
      if(origins.empty())solids[0]=std::move(soup);
      else {
        std::vector<u8> covered(soup.size()/9,0);
        for(const auto &origin:origins) {
          if(origin.firstTriangle>covered.size()||origin.triangleCount>covered.size()-origin.firstTriangle)throw std::runtime_error("Intervalo da fonte fora da geometria.");
          if(!origin.triangleCount)continue;
          auto &solid=solids[origin.object];
          for(u32 i=origin.firstTriangle;i<origin.firstTriangle+origin.triangleCount;++i)if(covered[i]++)throw std::runtime_error("Intervalos das fontes se sobrepõem.");
          solid.insert(solid.end(),soup.begin()+origin.firstTriangle*9,soup.begin()+(origin.firstTriangle+origin.triangleCount)*9);
        }
        if(std::any_of(covered.begin(),covered.end(),[](u8 count){return count!=1;}))throw std::runtime_error("Geometria sem origem identificada.");
      }
      if(solids.empty()||solids.size()>settings.maximumParts)throw std::runtime_error("O orçamento precisa de pelo menos uma parte por objeto fonte. Escolha menos objetos ou aumente as partes.");
      output.sourceGeometryHash=std::move(hash);output.origins=std::move(origins);groupCount=solids.size();
      for(const auto &[sourceObject,solid]:solids) {
      // Weld by exact coordinate; -0 is normalized. Require a closed oriented
      // manifold instead of silently voxelizing holes as solid geometry.
      std::map<std::array<float,3>,u32> ids;std::vector<float> points;std::vector<u32> triangles;
      std::map<std::pair<u32,u32>,std::pair<u32,i32>> edges;
      for(usize i=0;i<solid.size();i+=9) {
        if(requested.load()){output={};status.store(ConvexBakeStatus::Cancelled);return;}
        if(i%2304==0&&std::chrono::steady_clock::now()-began>std::chrono::seconds(budget))
          throw std::runtime_error("Limite de tempo excedido na preparação; reduza a geometria ou aumente o orçamento.");
        u32 triangle[3];for(u32 v=0;v<3;++v){std::array<float,3> p{solid[i+v*3],solid[i+v*3+1],solid[i+v*3+2]};
          for(auto &k:p){if(!std::isfinite(k))throw std::runtime_error("Vértice não finito.");if(k==0)k=0;}
          const auto [found,inserted]=ids.emplace(p,static_cast<u32>(ids.size()));triangle[v]=found->second;
          if(inserted)points.insert(points.end(),p.begin(),p.end());}
        if(triangle[0]==triangle[1]||triangle[0]==triangle[2]||triangle[1]==triangle[2])continue;
        triangles.insert(triangles.end(),triangle,triangle+3);
        for(u32 v=0;v<3;++v){const auto a=triangle[v],b=triangle[(v+1)%3];auto &edge=edges[{std::min(a,b),std::max(a,b)}];++edge.first;edge.second+=a<b?1:-1;}
      }
      if(points.size()<12||triangles.size()<12)throw std::runtime_error("A malha não contém um sólido.");
      usize checkedEdges=0;
      for(const auto &[key,edge]:edges){
        (void)key;if(edge.first!=2||edge.second)throw std::runtime_error("Decomposição exige malha fechada e orientada. Corrija bordas abertas, faces duplicadas ou orientação.");
        if(++checkedEdges%1024==0) {
          if(requested.load()){output={};status.store(ConvexBakeStatus::Cancelled);return;}
          if(std::chrono::steady_clock::now()-began>std::chrono::seconds(budget))
            throw std::runtime_error("Limite de tempo excedido na preparação; reduza a geometria ou aumente o orçamento.");
        }
      }
      {std::lock_guard lock(engineMutex);engine=VHACD::CreateVHACD();}if(!engine)throw std::runtime_error("V-HACD indisponível.");
      VHACD::IVHACD::Parameters p;p.m_callback=this;p.m_asyncACD=false;p.m_maxConvexHulls=settings.maximumParts;
      const auto partBudget=static_cast<u32>(settings.maximumParts/groupCount+(groupIndex<settings.maximumParts%groupCount));p.m_maxConvexHulls=partBudget;
      p.m_resolution=settings.voxelResolution;p.m_maxNumVerticesPerCH=settings.maximumVertices;
      p.m_minimumVolumePercentErrorAllowed=settings.volumeErrorPercent;
      const bool computed=engine->Compute(points.data(),static_cast<u32>(points.size()/3),triangles.data(),static_cast<u32>(triangles.size()/3),p);
      if(requested.load()||timedOut){releaseEngine();
        if(timedOut){std::lock_guard lock(mutex);error="Limite de tempo excedido; reduza a resolução ou aumente o orçamento.";status.store(ConvexBakeStatus::Failed);}
        else status.store(ConvexBakeStatus::Cancelled);return;}
      const auto count=engine->GetNConvexHulls();if(!computed||!count||count>partBudget)throw std::runtime_error("Decomposição não produziu cascos válidos no orçamento.");
      for(u32 i=0;i<count;++i){VHACD::IVHACD::ConvexHull hull;if(!engine->GetConvexHull(i,hull)||hull.m_points.size()<4||hull.m_points.size()>settings.maximumVertices||!std::isfinite(hull.m_volume)||hull.m_volume<=0||hull.m_triangles.size()<4||hull.m_triangles.size()>256)throw std::runtime_error("Casco gerado sem volume ou fora do orçamento.");
        ConvexBakePart part;part.sourceObject=sourceObject;for(const auto &v:hull.m_points){std::array<float,3> point{static_cast<float>(v.mX),static_cast<float>(v.mY),static_cast<float>(v.mZ)};
          for(float coordinate:point)if(!std::isfinite(coordinate))throw std::runtime_error("Casco com vértice não finito.");part.vertices.push_back(point);}
        for(const auto &t:hull.m_triangles)if(t.mI0>=part.vertices.size()||t.mI1>=part.vertices.size()||t.mI2>=part.vertices.size())throw std::runtime_error("Índices de casco inválidos.");
        for(const auto &t:hull.m_triangles){part.indices.push_back(t.mI0);part.indices.push_back(t.mI1);part.indices.push_back(t.mI2);}output.parts.push_back(std::move(part));}
      releaseEngine();
      ++groupIndex;
      }
      // Immutable bake resource; stable part order for its saved revision.
      std::stable_sort(output.parts.begin(),output.parts.end(),[](const auto &a,const auto &b){auto low=[](const auto &p){auto v=p.vertices.front();for(const auto &q:p.vertices)for(u32 k=0;k<3;++k)v[k]=std::min(v[k],q[k]);return v;};const auto al=low(a),bl=low(b);return al!=bl?al<bl:a.sourceObject<b.sourceObject;});
      makeGlb(output,settings,sources);if(requested.load()){output={};status.store(ConvexBakeStatus::Cancelled);return;}
      {std::lock_guard lock(mutex);fraction=1;stage="Prévia pronta";}status.store(ConvexBakeStatus::Ready);
    } catch(const std::exception &e){if(engine){releaseEngine();}output={};{std::lock_guard lock(mutex);error=e.what();}status.store(ConvexBakeStatus::Failed);}
      catch(...){if(engine){releaseEngine();}output={};{std::lock_guard lock(mutex);error="Falha de memória ou decomposição.";}status.store(ConvexBakeStatus::Failed);}
  }
};
ConvexBakeJob::ConvexBakeJob() noexcept :work_(new(std::nothrow) Work){}
ConvexBakeJob::~ConvexBakeJob(){cancel();if(work_&&work_->worker.joinable())work_->worker.join();}
void ConvexBakeJob::cancel() noexcept {if(work_)work_->requestCancel();}
bool ConvexBakeJob::reuse(const ConvexBakeResult &result,std::string &error) noexcept {
  try {
    if(!work_||work_->status.load()!=ConvexBakeStatus::Idle||result.parts.empty()||result.parts.size()>32||result.glb.empty()||result.sourceGeometryHash.size()!=64) {
      error="Resultado de cache inválido";return false;
    }
    for(const auto &part:result.parts) {
      if(part.vertices.size()<4||part.vertices.size()>64||part.indices.empty()||part.indices.size()%3||part.indices.size()>768){error="Casco em cache inválido";return false;}
      for(auto index:part.indices)if(index>=part.vertices.size()){error="Índice em cache inválido";return false;}
      for(const auto &point:part.vertices)for(float value:point)if(!std::isfinite(value)){error="Vértice em cache inválido";return false;}
    }
    work_->output=result;work_->fraction=1;work_->stage="Prévia reutilizada · geometria e parâmetros idênticos";
    work_->status.store(ConvexBakeStatus::Ready);error.clear();return true;
  } catch(...) {error="Memória insuficiente para reutilizar a prévia";return false;}
}
bool ConvexBakeJob::start(std::vector<float> triangles,ConvexBakeSettings settings,std::string hash,std::vector<std::string> sources,std::string &error,std::vector<ConvexBakeOrigin> origins) noexcept {
  try {
    if(!work_||work_->status.load()!=ConvexBakeStatus::Idle||!validConvexBakeSettings(settings)||triangles.empty()||triangles.size()%9||triangles.size()>900000||hash.size()!=64||hash.find_first_not_of("0123456789abcdef")!=std::string::npos){error="Parâmetros ou geometria fora do orçamento (100 mil triângulos).";return false;}
    for(const auto &s:sources)if(s.size()!=32||s.find_first_not_of("0123456789abcdef")!=std::string::npos){error="Identidade da fonte inválida.";return false;}
    if(origins.size()>128){error="Limite de 128 fontes por bake.";return false;}
    for(const auto &origin:origins) {
      if(!origin.object||(!origin.meshGuid.empty()&&(origin.meshGuid.size()!=32||origin.meshGuid.find_first_not_of("0123456789abcdef")!=std::string::npos))){error="Origem da geometria inválida.";return false;}
      for(float value:origin.relative)if(!std::isfinite(value)){error="Matriz de origem não finita.";return false;}
    }
    work_->status.store(ConvexBakeStatus::Running);work_->worker=std::thread(&Work::run,work_.get(),std::move(triangles),settings,std::move(hash),std::move(sources),std::move(origins));error.clear();return true;
  }catch(...){if(work_)work_->status.store(ConvexBakeStatus::Failed);error="Não foi possível criar o worker de decomposição.";return false;}
}
ConvexBakeProgress ConvexBakeJob::progress() const {if(!work_)return{ConvexBakeStatus::Failed,0,{},"Memória indisponível."};std::lock_guard lock(work_->mutex);return{work_->status.load(),work_->fraction,work_->stage,work_->error};}
const ConvexBakeResult *ConvexBakeJob::result() const noexcept{return work_&&work_->status.load()==ConvexBakeStatus::Ready?&work_->output:nullptr;}
}
