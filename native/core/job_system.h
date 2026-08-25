// Job system mínimo com work-stealing.
//
// Sem exceções (compilado com -fno-exceptions): um job que falhar deve usar
// AE_CHECK ou reportar erro por saída própria, nunca lançar.
//
// Modelo: N workers, cada um com uma deque própria; um worker sem trabalho
// "rouba" do fim da deque de outro worker escolhido round-robin. `run()`
// bloqueia até que todos os jobs submetidos (incluindo os que eles mesmos
// submeterem) terminem — suficiente para o caso de uso da engine, que
// dispara um grafo de jobs por frame e espera o frame terminar.
#pragma once

#include "core/base.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace ae {

using JobFn = std::function<void()>;

class JobSystem {
public:
  // threadCount == 0 significa "usar hardware_concurrency()", com mínimo 1
  // (dispositivo big.LITTLE de núcleo único em teste, por exemplo).
  explicit JobSystem(u32 threadCount = 0);
  ~JobSystem();

  JobSystem(const JobSystem &) = delete;
  JobSystem &operator=(const JobSystem &) = delete;

  // Enfileira um job para execução assíncrona.
  void submit(JobFn job);

  // Bloqueia a thread chamadora até que todos os jobs pendentes (inclusive
  // os submetidos por outros jobs em execução) tenham terminado.
  void waitIdle();

  u32 workerCount() const { return static_cast<u32>(workers_.size()); }

private:
  struct WorkerState {
    std::deque<JobFn> queue;
    std::mutex mutex;
  };

  void workerLoop(u32 index);
  bool tryPop(u32 index, JobFn &out);
  bool trySteal(u32 thiefIndex, JobFn &out);

  std::vector<std::thread> workers_;
  std::vector<std::unique_ptr<WorkerState>> queues_;

  std::atomic<bool> stop_{false};
  std::atomic<i64> pendingJobs_{0}; // jobs enfileirados e ainda não concluídos

  std::mutex wakeMutex_;
  std::condition_variable wakeCv_;

  std::mutex idleMutex_;
  std::condition_variable idleCv_;

  std::atomic<u32> nextQueue_{0}; // distribuição round-robin de novas submissões
};

} // namespace ae
