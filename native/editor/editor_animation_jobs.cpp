#include "editor/editor_animation_jobs.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>

namespace ae::editor {
namespace {
std::atomic<u64> nextJobHandle{1};
u64 allocateHandle() {
  auto current=nextJobHandle.load();
  while(current) {
    const auto next=current==std::numeric_limits<u64>::max()?u64{0}:current+1;
    if(nextJobHandle.compare_exchange_weak(current,next))return current;
  }
  return 0;
}
}
struct EditorAnimationJobs::Impl {
  struct Entry {
    AnimationJobRequest request;
    AnimationJobStatus status;
    std::shared_ptr<const AnimationJobResult> result;
    std::atomic<bool> cancel{false};
    usize retainedBytes=0;
  };
  mutable std::mutex mutex;
  std::condition_variable ready;
  std::vector<std::shared_ptr<Entry>> entries;
  std::deque<std::shared_ptr<Entry>> queue;
  // Invalidation can detach a running job from the visible list. Its snapshot
  // still counts against the mobile budget until the worker releases it.
  std::shared_ptr<Entry> running;
  const std::thread::id owner=std::this_thread::get_id();
  bool stopping=false;
  std::thread worker;
  explicit Impl():worker([this]{run();}) {}
  ~Impl() {
    {std::lock_guard lock(mutex);stopping=true;for(const auto &entry:entries)entry->cancel=true;if(running)running->cancel=true;}
    ready.notify_one();worker.join();
  }
  auto find(u64 handle) const {
    return std::find_if(entries.begin(),entries.end(),[&](const auto &entry){return entry->status.handle==handle;});
  }
  void run() {
    for(;;) {
      std::shared_ptr<Entry> entry;
      {
        std::unique_lock lock(mutex);ready.wait(lock,[&]{return stopping||!queue.empty();});
        if(stopping)return;
        entry=queue.front();queue.pop_front();
        if(entry->cancel){entry->status.state=AnimationJobState::Cancelled;entry->request.source={};entry->retainedBytes=0;continue;}
        entry->status.state=AnimationJobState::Running;running=entry;
      }
      const auto &request=entry->request;
      auto candidate=request.source;resources::AnimationBakeReport report;std::string error;
      const auto cancelled=[&]{return entry->cancel.load(std::memory_order_relaxed);};
      const bool ok=request.kind==AnimationJobKind::BakeTrack?
          resources::bakeAnimationClipTrack(candidate,request.track,request.settings,report,error,cancelled):
          resources::consolidateAnimationClip(request.source,request.destination,request.name,request.settings,candidate,report,error,cancelled);
      std::shared_ptr<const AnimationJobResult> result;
      const usize outputBytes=ok&&!cancelled()?candidate.serialize().size():0;
      if(ok&&!cancelled()&&outputBytes<=MaximumSourceBytes)result=std::make_shared<const AnimationJobResult>(AnimationJobResult{request.ticket,request.kind,std::move(candidate),request.path,report});
      {
        std::lock_guard lock(mutex);
        if(entry->cancel) {
          if(entry->status.state!=AnimationJobState::Stale) {entry->status.state=AnimationJobState::Cancelled;entry->status.diagnostic="Bake cancelado; nada publicado";}
        }
        else if(!ok||!result) {entry->status.state=AnimationJobState::Failed;entry->status.diagnostic=error.empty()?"Resultado excede o orçamento de memória do job":std::move(error);}
        else {entry->result=std::move(result);entry->retainedBytes=outputBytes;entry->status.report=report;entry->status.state=AnimationJobState::Ready;}
        entry->request.source={};
        if(!entry->result)entry->retainedBytes=0;
        running.reset();
      }
    }
  }
};
EditorAnimationJobs::EditorAnimationJobs():impl_(std::make_unique<Impl>()) {}
EditorAnimationJobs::~EditorAnimationJobs()=default;
u64 EditorAnimationJobs::enqueue(AnimationJobRequest request,std::string &error) {
  error.clear();
  if(std::this_thread::get_id()!=impl_->owner) {error="Crie jobs na thread do editor";return 0;}
  if(request.ticket.project.empty()||!request.ticket.epoch||!request.ticket.owner||!request.ticket.sceneRevision||
     request.ticket.source!=request.source.guid||request.ticket.sourceRevision!=request.source.revision||
     !request.source.valid(&error)) {if(error.empty())error="Snapshot ou ownership do job inválido";return 0;}
  if(request.kind!=AnimationJobKind::BakeTrack&&request.kind!=AnimationJobKind::Consolidate) {error="Operação assíncrona não suportada";return 0;}
  if(request.kind==AnimationJobKind::BakeTrack&&!request.source.track(request.track)) {error="Canal de bake ausente";return 0;}
  if(request.kind==AnimationJobKind::Consolidate&&(!request.destination.valid()||request.destination==request.source.guid||request.path.empty())) {error="Consolidação exige identidade e caminho novos";return 0;}
  const auto bytes=request.source.serialize().size();
  std::lock_guard lock(impl_->mutex);usize retained=0;
  for(const auto &entry:impl_->entries)retained+=entry->retainedBytes;
  const bool detached=impl_->running&&impl_->find(impl_->running->status.handle)==impl_->entries.end();
  if(detached)retained+=impl_->running->retainedBytes;
  if(impl_->entries.size()+usize(detached)>=MaximumJobs||bytes>MaximumSourceBytes||retained>MaximumSourceBytes-bytes) {error="Limite de jobs ou de memória atingido; libere um resultado";return 0;}
  const auto handle=allocateHandle();if(!handle) {error="Identidades de job esgotadas";return 0;}
  auto entry=std::make_shared<Impl::Entry>();entry->request=std::move(request);entry->retainedBytes=bytes;
  entry->status.handle=handle;entry->status.state=AnimationJobState::Queued;entry->status.kind=entry->request.kind;
  impl_->entries.push_back(entry);impl_->queue.push_back(std::move(entry));impl_->ready.notify_one();return handle;
}
bool EditorAnimationJobs::status(u64 handle,AnimationJobStatus &out) const {
  std::lock_guard lock(impl_->mutex);const auto found=impl_->find(handle);if(found==impl_->entries.end())return false;
  out=(*found)->status;return true;
}
bool EditorAnimationJobs::cancel(u64 handle) {
  if(std::this_thread::get_id()!=impl_->owner)return false;
  std::lock_guard lock(impl_->mutex);const auto found=impl_->find(handle);if(found==impl_->entries.end())return false;
  auto &entry=**found;
  if(entry.status.state!=AnimationJobState::Queued&&entry.status.state!=AnimationJobState::Running&&entry.status.state!=AnimationJobState::Ready)return false;
  entry.cancel=true;
  if(entry.status.state!=AnimationJobState::Running) {entry.status.state=AnimationJobState::Cancelled;entry.status.diagnostic="Bake cancelado; nada publicado";entry.result.reset();}
  return true;
}
std::shared_ptr<const AnimationJobResult> EditorAnimationJobs::result(u64 handle,const AnimationJobTicket &ticket,std::string &error) {
  error.clear();
  if(std::this_thread::get_id()!=impl_->owner) {error="Publique jobs na thread do editor";return {};}
  std::lock_guard lock(impl_->mutex);const auto found=impl_->find(handle);
  if(found==impl_->entries.end()) {error="Job ausente ou liberado";return {};}
  auto &entry=**found;
  if(entry.request.ticket!=ticket) {entry.cancel=true;entry.result.reset();entry.status.state=AnimationJobState::Stale;entry.status.diagnostic="Projeto, cena, proprietário ou recurso mudou";error=entry.status.diagnostic;return {};}
  if(entry.status.state!=AnimationJobState::Ready) {error=entry.status.diagnostic.empty()?"Job ainda não está pronto para publicar":entry.status.diagnostic;return {};}
  return entry.result;
}
bool EditorAnimationJobs::published(u64 handle) {
  if(std::this_thread::get_id()!=impl_->owner)return false;
  std::lock_guard lock(impl_->mutex);const auto found=impl_->find(handle);
  if(found==impl_->entries.end()||(*found)->status.state!=AnimationJobState::Ready)return false;
  (*found)->status.state=AnimationJobState::Published;(*found)->result.reset();(*found)->retainedBytes=0;return true;
}
bool EditorAnimationJobs::release(u64 handle) {
  if(std::this_thread::get_id()!=impl_->owner)return false;
  std::lock_guard lock(impl_->mutex);const auto found=impl_->find(handle);if(found==impl_->entries.end())return false;
  if((*found)->status.state==AnimationJobState::Running)return false;
  (*found)->cancel=true;
  std::erase_if(impl_->queue,[&](const auto &entry){return entry->status.handle==handle;});
  impl_->entries.erase(found);return true;
}
void EditorAnimationJobs::invalidate() {
  if(std::this_thread::get_id()!=impl_->owner)return;
  std::lock_guard lock(impl_->mutex);
  for(const auto &entry:impl_->entries) {entry->cancel=true;entry->result.reset();}
  if(impl_->running)impl_->running->cancel=true;
  impl_->queue.clear();impl_->entries.clear();
}
} // namespace ae::editor
