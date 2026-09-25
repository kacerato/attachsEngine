#pragma once
#include "core/base.h"
#include "renderer/authoring_texture.h"

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace ae::renderer {
// Leitura dos níveis de textura que estão só no derivado em disco (bloco C),
// fora da thread de render: o streaming pede, segue desenhando com o nível que
// já tem, e sobe o nível quando os bytes ficam prontos. Uma thread, fila FIFO,
// sem pedido duplicado; `clear` descarta tudo quando a biblioteca muda (as
// texturas pedidas são imutáveis e ficam vivas pelo próprio ponteiro).
class TextureLevelReader final {
public:
  enum class Status : u8 { Unknown, Pending, Ready, Failed };
  TextureLevelReader() = default;
  ~TextureLevelReader();
  TextureLevelReader(const TextureLevelReader &) = delete;
  TextureLevelReader &operator=(const TextureLevelReader &) = delete;

  // Níveis [from, levels) de `texture`, identificados por `key` (quem chama
  // escolhe: textura + geração da biblioteca). Não bloqueia.
  void request(u64 key, SharedAuthoringTexture texture, u32 from);
  // Estado do pedido (`key`, `from`). Pronto: move os bytes para `out` e esquece
  // o pedido. Falhou: esquece também (um pedido novo tenta de novo).
  Status take(u64 key, u32 from, std::vector<u8> &out);
  // Descarta pedidos na fila e resultados; uma leitura em curso termina e é descartada.
  void clear();
  usize outstanding() const;

private:
  struct Job {
    u64 key = 0;
    u32 from = 0;
    SharedAuthoringTexture texture;
    u64 epoch = 0;
  };
  struct Result {
    Status status = Status::Pending;
    std::vector<u8> bytes;
  };
  void run();
  mutable std::mutex lock_;
  std::condition_variable wake_;
  std::deque<Job> queue_;
  std::map<std::pair<u64, u32>, Result> results_;
  std::thread worker_;
  bool stop_ = false;
  u64 epoch_ = 0;
};
} // namespace ae::renderer
