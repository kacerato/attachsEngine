#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
namespace ae::resources {
// Single writer per directory; callers synchronize put/recover/collect.
// Opaque serialized recipes: callers retain revision/undo metadata and live
// roots. Inspired by Unity 6000.0 Undo.RegisterCompleteObjectUndo's
// recorded-state contract.
class TerrainHistoryStore {
public:
  struct Limits {
    std::size_t maxBytes = 64 * 1024 * 1024;
    unsigned maxDeltaDepth = 16;
  };
  explicit TerrainHistoryStore(std::filesystem::path directory);
  TerrainHistoryStore(std::filesystem::path directory, Limits limits);
  // Immutable SHA-256 IDs. False means I/O, corruption or configured limits.
  bool put(const std::string &recipe, const std::string &base, std::string &id);
  bool read(const std::string &id, std::string &recipe) const;
  // Only removes interrupted .aethist.aether-tmp writes within this dedicated
  // directory.
  bool recover(std::size_t &removed);
  // All roots must resolve before any deletion. Protected roots also retain
  // bases.
  bool collect(const std::vector<std::string> &liveRoots,
               const std::vector<std::string> &protectedRoots,
               std::size_t &removed);

private:
  std::filesystem::path directory_;
  Limits limits_;
  bool load(const std::string &, unsigned, std::vector<std::string> *,
            std::string &) const;
  bool ready_ = false;
};
} // namespace ae::resources
