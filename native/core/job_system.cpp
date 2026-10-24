#include "core/job_system.h"

#include <memory>

namespace ae {

JobSystem::JobSystem(u32 threadCount) {
  if (threadCount == 0) {
    unsigned hw = std::thread::hardware_concurrency();
    threadCount = hw > 0 ? hw : 1;
  }
  queues_.reserve(threadCount);
  for (u32 i = 0; i < threadCount; ++i) {
    queues_.push_back(std::make_unique<WorkerState>());
  }
  workers_.reserve(threadCount);
  for (u32 i = 0; i < threadCount; ++i) {
    workers_.emplace_back([this, i] { workerLoop(i); });
  }
}

JobSystem::~JobSystem() {
  {
    // Sob o mutex do sono: sem ele, um worker que acabou de ver o predicado
    // falso perde o aviso, dorme para sempre e o join trava.
    std::lock_guard<std::mutex> lock(wakeMutex_);
    stop_.store(true, std::memory_order_release);
  }
  wakeCv_.notify_all();
  for (auto &t : workers_) {
    if (t.joinable()) t.join();
  }
}

void JobSystem::submit(JobFn job) {
  u32 idx = nextQueue_.fetch_add(1, std::memory_order_relaxed) %
            static_cast<u32>(queues_.size());
  // Pendente antes de visível: um worker não pode concluir (e decrementar) um
  // job que ainda não foi contado.
  pendingJobs_.fetch_add(1, std::memory_order_acq_rel);
  {
    std::lock_guard<std::mutex> lock(queues_[idx]->mutex);
    queues_[idx]->queue.push_back(std::move(job));
  }
  {
    std::lock_guard<std::mutex> lock(wakeMutex_);
    queuedJobs_.fetch_add(1, std::memory_order_acq_rel);
  }
  wakeCv_.notify_one();
}

bool JobSystem::tryPop(u32 index, JobFn &out) {
  auto &state = *queues_[index];
  std::lock_guard<std::mutex> lock(state.mutex);
  if (state.queue.empty()) return false;
  // O próprio worker consome do início (LIFO local reduziria latência de
  // cache, mas FIFO local mantém ordem previsível — suficiente aqui).
  out = std::move(state.queue.front());
  state.queue.pop_front();
  queuedJobs_.fetch_sub(1, std::memory_order_acq_rel);
  return true;
}

bool JobSystem::trySteal(u32 thiefIndex, JobFn &out) {
  u32 n = static_cast<u32>(queues_.size());
  for (u32 offset = 1; offset < n; ++offset) {
    u32 victim = (thiefIndex + offset) % n;
    auto &state = *queues_[victim];
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.queue.empty()) {
      // Rouba do fim: minimiza colisão com o dono, que consome do início.
      out = std::move(state.queue.back());
      state.queue.pop_back();
      queuedJobs_.fetch_sub(1, std::memory_order_acq_rel);
      return true;
    }
  }
  return false;
}

void JobSystem::workerLoop(u32 index) {
  while (!stop_.load(std::memory_order_acquire)) {
    JobFn job;
    bool got = tryPop(index, job);
    if (!got) got = trySteal(index, job);

    if (got) {
      job();
      job = nullptr;
      if (pendingJobs_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        // Última pendência concluída: acorda quem espera em waitIdle().
        std::lock_guard<std::mutex> lock(idleMutex_);
        idleCv_.notify_all();
      }
      continue;
    }

    std::unique_lock<std::mutex> lock(wakeMutex_);
    wakeCv_.wait(lock, [this] {
      return stop_.load(std::memory_order_acquire) || queuedJobs_.load(std::memory_order_acquire) > 0;
    });
  }
}

void JobSystem::waitIdle() {
  std::unique_lock<std::mutex> lock(idleMutex_);
  idleCv_.wait(lock, [this] {
    return pendingJobs_.load(std::memory_order_acquire) == 0;
  });
}

} // namespace ae
