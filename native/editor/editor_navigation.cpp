// Navegação no editor (bloco J): bake em segundo plano, recurso .navmesh no
// projeto e a vista da Superfície para o Inspector e o viewport.
//
// O bake usa o mesmo caminho do Play para montar a física da cena (inclusive a
// forma dos colisores Malha) e coleta os triângulos dos corpos estáticos na
// thread principal. Só o Recast roda na thread de trabalho, sobre a cópia da
// geometria; cancelar não toca na malha anterior. Assar de novo a mesma
// Superfície reescreve o mesmo recurso (mesma identidade), como a Unity faz
// com o asset da NavMeshSurface.
#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "core/sha256.h"
#include "runtime/scene_navigation.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <thread>

namespace ae::editor {

struct EditorSession::NavigationBakeJob {
  EditorEntityId surface=0;
  u64 instance=0;
  navigation::BakeGeometry geometry;
  navigation::BakeSettings settings;
  navigation::BakeProgress progress;
  navigation::NavMeshData result;
  navigation::BakeStatus status=navigation::BakeStatus::Failed;
  std::string error;
  std::atomic<bool> done{false};
  std::thread thread;
  ~NavigationBakeJob() {progress.cancel=true;if(thread.joinable()) thread.join();}
};

namespace {
const scene::NavSurface *surfaceComponent(const EditorDocument &document,EditorEntityId id,u64 instance) {
  const auto *entity=document.find(id);
  const auto *component=entity?(instance?entity->components.findInstance(instance):entity->components.find(scene::NavSurface::descriptor)):nullptr;
  return component&&&component->type()==&scene::NavSurface::descriptor?&scene::navSurface(*component):nullptr;
}
std::string fileStem(std::string name) {
  for(auto &c:name) if(c=='/'||c=='\\'||c==':'||c=='"'||c=='.'||c=='*'||c=='?'||c=='<'||c=='>'||c=='|') c='_';
  return name.empty()?std::string("Superfície"):name;
}
} // namespace

void EditorSession::loadNavMeshes() {
  navMeshes_.clear();navigationOverlayGuid_={};navigationViewRevision_=~u64{0};
  if(files_.rootPath().empty()) return;
  for(const auto &record:assets_.records()) if(record.type==resources::AssetType::NavMesh) {
    std::filesystem::path path;std::vector<u8> bytes;navigation::NavMeshData data;std::string error;
    if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),record.path,path)||
       !EditorImportTransaction::read(path,bytes,navigation::NavMeshData::MaximumBytes)||
       !navigation::NavMeshData::deserialize({reinterpret_cast<const char*>(bytes.data()),bytes.size()},data,&error)||data.guid!=record.guid) {
      reportProblem(EditorConsoleSeverity::Warning,"Navmesh ausente ou inválida: "+record.path+(error.empty()?"":" · "+error));continue;
    }
    navMeshes_.push_back(std::move(data));
  }
}

bool EditorSession::navigationBaking() const noexcept {return navigationBake_&&!navigationBake_->done.load();}
void EditorSession::waitNavigationBake() {
  if(navigationBake_&&navigationBake_->thread.joinable()) navigationBake_->thread.join();
  pumpNavigationBake();
}

bool EditorSession::startNavigationBake(EditorEntityId surface,u64 instance) {
  if(navigationBake_) {state_.status="Já existe um bake de navegação em andamento";return false;}
  if(isPlaying()||playMirrorOpen_||history_.isOpen()) {state_.status="Pare o Play para assar a navegação";return false;}
  if(files_.rootPath().empty()) {state_.status="Abra um projeto para guardar a malha de navegação";return false;}
  const auto *component=surfaceComponent(document_,surface,instance);
  if(!component) {state_.status="Selecione uma Superfície de navegação";return false;}
  auto job=std::make_shared<NavigationBakeJob>();
  job->surface=surface;job->instance=instance;job->settings=runtime::navigationBakeSettings(*component);
  if(!job->settings.valid()) {state_.status="Medidas de agente ou de célula inválidas para o bake";return false;}
  std::string error;
  if(!EditorPlayScene::collectNavigationGeometry(document_,mapScene_,surface,job->geometry,error)) {
    state_.status=error;reportProblem(EditorConsoleSeverity::Error,"Bake de navegação: "+error);return false;
  }
  if(!job->geometry.triangleCount()) {
    state_.status="Nenhum colisor estático para assar nesta coleta";
    reportProblem(EditorConsoleSeverity::Warning,state_.status);return false;
  }
  auto *raw=job.get();
  job->thread=std::thread([raw]{
    raw->status=navigation::bake(raw->geometry,raw->settings,raw->progress,raw->result,raw->error);
    raw->done=true;
  });
  navigationBake_=std::move(job);
  state_.status="Assando navegação…";
  return true;
}

void EditorSession::pumpNavigationBake() {
  if(!navigationBake_||!navigationBake_->done.load()) return;
  auto job=std::move(navigationBake_);
  if(job->thread.joinable()) job->thread.join();
  navigationViewRevision_=~u64{0};
  using S=navigation::BakeStatus;
  if(job->status==S::Cancelled) {state_.status="Bake cancelado; a malha anterior foi preservada";return;}
  if(job->status!=S::Ok) {
    state_.status="Bake de navegação: "+job->error;
    reportProblem(job->status==S::Empty?EditorConsoleSeverity::Warning:EditorConsoleSeverity::Error,state_.status);return;
  }
  const auto *entity=document_.find(job->surface);
  const auto *current=surfaceComponent(document_,job->surface,job->instance);
  if(!entity||!current) {state_.status="A Superfície foi removida durante o bake; resultado descartado";return;}
  if(isPlaying()||playMirrorOpen_||history_.isOpen()||files_.rootPath().empty()) {state_.status="Bake concluído com o editor ocupado; asse de novo";return;}
  // Mesma Superfície, mesmo recurso: o bake novo substitui o conteúdo.
  const auto *existing=current->data.valid()?assets_.find(current->data):nullptr;
  if(existing&&existing->type!=resources::AssetType::NavMesh) existing=nullptr;
  std::string path;resources::AssetGuid guid{};
  if(existing) {path=existing->path;guid=existing->guid;}
  else {
    const auto stem=fileStem(entity->name);
    path="Navegação/"+stem+".navmesh";
    for(u32 n=2;assets_.findByPath(path)||files_.exists(path);++n) path="Navegação/"+stem+" "+std::to_string(n)+".navmesh";
    guid=resources::assetGuidFromSeed("navmesh:"+path+":"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+
                                      ":"+std::to_string(++importInstanceCounter_));
  }
  auto data=std::move(job->result);
  data.guid=guid;data.name=std::string(entity->name);
  const auto bytes=data.serialize();
  std::filesystem::path absolute;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),path,absolute)) {state_.status="Destino da navmesh inválido";return;}
  std::string expected;
  {std::vector<u8> previous;std::error_code e;
   if(std::filesystem::exists(absolute,e)&&EditorImportTransaction::read(absolute,previous,navigation::NavMeshData::MaximumBytes)) expected=Sha256::hex(previous);}
  resources::AssetRecord record=existing?*existing:resources::AssetRecord{};
  record.guid=guid;record.type=resources::AssetType::NavMesh;record.path=path;
  record.contentHash=Sha256::hex({reinterpret_cast<const u8*>(bytes.data()),bytes.size()});
  auto next=assets_;
  if(existing) next.remove(guid);
  if(!next.add(record)) {state_.status="O registro recusou a navmesh";return;}
  std::string diagnostic;
  EditorImportTransaction transaction(files_.rootPath());
  if(!transaction.begin(path,expected,diagnostic)) {state_.status=diagnostic;return;}
  if(!transaction.commit({reinterpret_cast<const u8*>(bytes.data()),bytes.size()},next.serialize())) {
    state_.status=transaction.rollback()?"Gravação recusada; projeto preservado":"Recuperação pendente no journal";return;
  }
  assets_=std::move(next);assetRegistryDirty_=true;files_.rebuildTree();
  const auto stats=data.stats;const float seconds=data.bakeSeconds;
  const auto found=std::find_if(navMeshes_.begin(),navMeshes_.end(),[&](const auto &m){return m.guid==guid;});
  if(found!=navMeshes_.end()) *found=std::move(data);else navMeshes_.push_back(std::move(data));
  navigationOverlayGuid_={};
  // A atribuição do recurso é autoria: um passo de desfazer.
  if(current->data!=guid) {
    auto values=*entity;
    auto *edit=values.components.editInstance(job->instance);
    if(edit&&&edit->type()==&scene::NavSurface::descriptor) {scene::navSurface(*edit).data=guid;history_.applyValues(document_,entity->id,values);}
  }
  char text[160];
  std::snprintf(text,sizeof text,"Malha assada · %u polígonos · %.0f m² · %.1f s",stats.polygons,static_cast<double>(stats.area),static_cast<double>(seconds));
  state_.status=text;reportProblem(EditorConsoleSeverity::Info,std::string(text)+" · "+path);
}

bool EditorSession::clearNavigationBake(EditorEntityId surface,u64 instance) {
  if(isPlaying()||playMirrorOpen_||history_.isOpen()) return false;
  const auto *entity=document_.find(surface);
  const auto *current=surfaceComponent(document_,surface,instance);
  if(!entity||!current||!current->data.valid()) return false;
  auto values=*entity;
  auto *edit=values.components.editInstance(instance?instance:entity->components.find(scene::NavSurface::descriptor)->instanceId());
  if(!edit||&edit->type()!=&scene::NavSurface::descriptor) return false;
  scene::navSurface(*edit).data={};
  if(!history_.applyValues(document_,entity->id,values)) return false;
  state_.status="Malha desvinculada; o arquivo continua no projeto";navigationViewRevision_=~u64{0};
  return true;
}

void EditorSession::refreshNavigationView() {
  auto &view=navigationView_;
  const auto &document=*state_.document;
  const auto *entity=document.find(state_.selection);
  const scene::ComponentValue *component=entity?entity->components.findInstance(state_.expandedNative):nullptr;
  if(!component||&component->type()!=&scene::NavSurface::descriptor) component=entity?entity->components.find(scene::NavSurface::descriptor):nullptr;
  if(!component) {
    view.surface=0;view.instance=0;view.triangles.clear();view.edges.clear();view.areas.clear();navigationOverlayGuid_={};
    view.baking=navigationBaking();view.progress=0;return;
  }
  const auto &surface=scene::navSurface(*component);
  const bool changed=view.surface!=entity->id||view.instance!=component->instanceId();
  view.surface=entity->id;view.instance=component->instanceId();
  view.baking=navigationBake_&&navigationBake_->surface==entity->id&&!navigationBake_->done.load();
  if(view.baking) {
    const u32 total=navigationBake_->progress.total.load();
    view.progress=total?static_cast<float>(navigationBake_->progress.done.load())/static_cast<float>(total):0.f;
  } else view.progress=0;
  const auto mesh=std::find_if(navMeshes_.begin(),navMeshes_.end(),[&](const auto &m){return m.guid==surface.data;});
  view.hasData=surface.data.valid()&&mesh!=navMeshes_.end();
  view.missing=surface.data.valid()&&mesh==navMeshes_.end();
  view.stats=view.hasData?mesh->stats:navigation::NavMeshStats{};
  view.bakeSeconds=view.hasData?mesh->bakeSeconds:0;
  if(!view.hasData) {view.triangles.clear();view.edges.clear();view.areas.clear();navigationOverlayGuid_={};}
  else if(navigationOverlayGuid_!=surface.data) {
    navigation::NavWorld world;std::string error;
    if(world.load(*mesh,{},1,error)) {world.triangles(view.triangles,view.areas);world.edges(view.edges);}
    navigationOverlayGuid_=surface.data;
  }
  // Desatualizada: a geometria coletada hoje não é a do bake. Recalcula quando
  // a cena muda, sem repetir a montagem da física a cada quadro.
  const u64 revision=document_.revision();
  if(changed) navigationViewRevision_=~u64{0};
  if(!view.hasData||isPlaying()||view.baking) {view.staleKnown=false;view.stale=false;}
  else if(navigationViewRevision_!=revision) {
    navigationViewRevision_=revision;
    navigation::BakeGeometry geometry;std::string error;
    view.staleKnown=EditorPlayScene::collectNavigationGeometry(document_,mapScene_,entity->id,geometry,error);
    view.stale=view.staleKnown&&navigation::bakeHash(geometry,runtime::navigationBakeSettings(surface))!=mesh->sourceHash;
  }
  if(view.baking) {char text[80];std::snprintf(text,sizeof text,"Assando · %.0f%%",static_cast<double>(view.progress*100));view.status=text;}
  else if(view.missing) view.status="Recurso .navmesh ausente; asse de novo";
  else if(!view.hasData) view.status="Sem malha assada";
  else if(view.stale) view.status="Desatualizada: a cena mudou desde o bake";
  else view.status="Assada e atual";
}

bool EditorSession::handleNavigationWidget(u32 widget) {
  namespace w=navigation_widget;
  switch(widget-widgetId(EditorWidget::NavigationBase)) {
    case w::Bake: return startNavigationBake(navigationView_.surface,navigationView_.instance);
    case w::Cancel: if(navigationBake_) navigationBake_->progress.cancel=true;return true;
    case w::Clear: return clearNavigationBake(navigationView_.surface,navigationView_.instance);
    case w::ShowMesh: navigationView_.showMesh=!navigationView_.showMesh;return true;
    default: return false;
  }
}

} // namespace ae::editor
