#include "renderer/texture_level_reader.h"

namespace ae::renderer {
TextureLevelReader::~TextureLevelReader() {
  {
    std::lock_guard<std::mutex> hold(lock_);
    stop_ = true;
    queue_.clear();
  }
  wake_.notify_all();
  if (worker_.joinable()) worker_.join();
}

void TextureLevelReader::request(u64 key, SharedAuthoringTexture texture, u32 from) {
  if (!texture) return;
  {
    std::lock_guard<std::mutex> hold(lock_);
    if (stop_ || results_.contains({key, from})) return;
    results_[{key, from}] = {};
    queue_.push_back({key, from, std::move(texture), epoch_});
    // A thread nasce no primeiro pedido: quem não usa textura parcial não paga por ela.
    if (!worker_.joinable()) worker_ = std::thread([this] {run();});
  }
  wake_.notify_one();
}

TextureLevelReader::Status TextureLevelReader::take(u64 key, u32 from, std::vector<u8> &out) {
  std::lock_guard<std::mutex> hold(lock_);
  const auto found = results_.find({key, from});
  if (found == results_.end()) return Status::Unknown;
  const Status status = found->second.status;
  if (status == Status::Ready) out = std::move(found->second.bytes);
  if (status != Status::Pending) results_.erase(found);
  return status;
}

void TextureLevelReader::clear() {
  std::lock_guard<std::mutex> hold(lock_);
  queue_.clear();
  results_.clear();
  ++epoch_;
}

usize TextureLevelReader::outstanding() const {
  std::lock_guard<std::mutex> hold(lock_);
  usize pending = 0;
  for (const auto &[key, result] : results_) pending += result.status == Status::Pending;
  return pending;
}

void TextureLevelReader::run() {
  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> hold(lock_);
      wake_.wait(hold, [this] {return stop_ || !queue_.empty();});
      if (stop_) return;
      job = std::move(queue_.front());
      queue_.pop_front();
    }
    std::vector<u8> scratch;
    std::span<const u8> levels;
    const bool read = readAuthoringTextureLevels(*job.texture, job.from, scratch, levels);
    std::vector<u8> bytes;
    if (read && levels.data() == scratch.data() && levels.size() == scratch.size()) bytes = std::move(scratch);
    else if (read) bytes.assign(levels.begin(), levels.end());
    std::lock_guard<std::mutex> hold(lock_);
    // Biblioteca trocada durante a leitura: o resultado não vale mais.
    if (job.epoch != epoch_) continue;
    const auto found = results_.find({job.key, job.from});
    if (found == results_.end()) continue;
    found->second.status = read ? Status::Ready : Status::Failed;
    found->second.bytes = std::move(bytes);
  }
}
} // namespace ae::renderer
