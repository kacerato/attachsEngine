#include "terrain_history_store.h"
#include "core/sha256.h"
#include "platform/atomic_asset_file.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <set>

namespace ae::resources {
namespace {
std::string sha(const std::string &s) {
  return Sha256::hex(
      std::span<const u8>(reinterpret_cast<const u8 *>(s.data()), s.size()));
}
// Refuse aliases through symlinked ancestors, including after construction.
bool safeDirectory(const std::filesystem::path &path,
                   bool allowMissing = false) {
  std::error_code ec;
  auto p = std::filesystem::absolute(path, ec);
  if (ec)
    return false;
  while (!p.empty()) {
    auto status = std::filesystem::symlink_status(p, ec);
    if (ec) {
      if (!allowMissing || ec != std::errc::no_such_file_or_directory)
        return false;
      ec.clear();
    } else if (std::filesystem::is_symlink(status) ||
               (!std::filesystem::is_directory(status) &&
                std::filesystem::exists(status)))
      return false;
    auto parent = p.parent_path();
    if (parent == p)
      break;
    p = std::move(parent);
  }
  return true;
}
bool valid(const std::string &id) {
  return id.size() == 64 &&
         id.find_first_not_of("0123456789abcdef") == std::string::npos;
}
void number(std::string &s, std::uint64_t n) {
  for (int i = 0; i < 8; ++i)
    s += char(n >> (i * 8));
}
bool number(const std::string &s, std::size_t &p, std::uint64_t &n) {
  if (p > s.size() || s.size() - p < 8)
    return false;
  n = 0;
  for (int i = 0; i < 8; ++i)
    n |= std::uint64_t(static_cast<unsigned char>(s[p++])) << (i * 8);
  return true;
}
} // namespace
TerrainHistoryStore::TerrainHistoryStore(std::filesystem::path p)
    : TerrainHistoryStore(std::move(p), Limits{}) {}
TerrainHistoryStore::TerrainHistoryStore(std::filesystem::path p, Limits l)
    : directory_(std::move(p)), limits_(l) {
  std::error_code ec;
  if (!safeDirectory(directory_, true))
    return;
  std::filesystem::create_directories(directory_, ec);
  ready_ = !ec && l.maxBytes > 0 && l.maxBytes <= 1024ull * 1024 * 1024 &&
           l.maxDeltaDepth <= 64 && safeDirectory(directory_);
}
bool TerrainHistoryStore::load(const std::string &id, unsigned depth,
                               std::vector<std::string> *refs,
                               std::string &result) const {
  if (!ready_ || !safeDirectory(directory_) || !valid(id) ||
      depth > limits_.maxDeltaDepth)
    return false;
  auto path = directory_ / (id + ".aethist");
  std::error_code ec;
  if (std::filesystem::is_symlink(path, ec) || ec)
    return false;
  auto size = std::filesystem::file_size(path, ec);
  if (ec || size > limits_.maxBytes + 256)
    return false;
  std::ifstream f(path, std::ios::binary);
  std::string s(static_cast<std::size_t>(size), '\0');
  f.read(s.data(), s.size());
  if (!f || sha(s) != id || s.size() < 13 || s.substr(0, 4) != "ATH1")
    return false;
  std::size_t p = 5;
  std::uint64_t total;
  if (!number(s, p, total) || total > limits_.maxBytes)
    return false;
  if (s[4] == 'F') {
    if (s.size() - p != total)
      return false;
    result = s.substr(p);
  } else {
    if (s[4] != 'D' || s.size() - p < 64)
      return false;
    auto base = s.substr(p, 64);
    p += 64;
    std::uint64_t prefix, suffix;
    std::string old;
    if (!number(s, p, prefix) || !number(s, p, suffix) ||
        !load(base, depth + 1, refs, old) || prefix > old.size() ||
        suffix > old.size() - prefix || prefix > total ||
        suffix > total - prefix || s.size() - p != total - prefix - suffix)
      return false;
    result =
        old.substr(0, prefix) + s.substr(p) + old.substr(old.size() - suffix);
  }
  if (refs)
    refs->push_back(id);
  return true;
}
bool TerrainHistoryStore::read(const std::string &id,
                               std::string &recipe) const {
  std::string next;
  if (!load(id, 0, nullptr, next))
    return false;
  recipe = std::move(next);
  return true;
}
bool TerrainHistoryStore::put(const std::string &recipe,
                              const std::string &base, std::string &out) {
  if (!ready_ || !safeDirectory(directory_) || recipe.size() > limits_.maxBytes)
    return false;
  std::string s = "ATH1F";
  number(s, recipe.size());
  s += recipe;
  if (!base.empty()) {
    std::vector<std::string> refs;
    std::string old;
    if (!load(base, 0, &refs, old))
      return false;
    if (refs.size() <= limits_.maxDeltaDepth) {
      std::size_t a = 0, b = 0;
      while (a < old.size() && a < recipe.size() && old[a] == recipe[a])
        ++a;
      while (b < old.size() - a && b < recipe.size() - a &&
             old[old.size() - 1 - b] == recipe[recipe.size() - 1 - b])
        ++b;
      std::string d = "ATH1D";
      number(d, recipe.size());
      d += base;
      number(d, a);
      number(d, b);
      d += recipe.substr(a, recipe.size() - a - b);
      if (d.size() < s.size())
        s = std::move(d);
    }
  }
  auto id = sha(s);
  auto dest = directory_ / (id + ".aethist");
  std::error_code ec;
  bool exists = std::filesystem::exists(dest, ec);
  if (ec)
    return false;
  if (exists) {
    std::string current;
    if (!read(id, current) || current != recipe)
      return false;
    out = id;
    return true;
  }
  // One writer per store, matching replaceAssetFile's documented contract.
  auto tmp = dest;
  tmp += ".aether-tmp";
  auto status = std::filesystem::symlink_status(tmp, ec);
  if (ec && ec != std::errc::no_such_file_or_directory)
    return false;
  if (!ec && std::filesystem::exists(status))
    return false;
  struct Input {
    const std::string &bytes;
    std::size_t offset = 0;
  } input{s};
  auto readBytes = [](void *context, void *buffer,
                      std::size_t capacity) -> int {
    auto &in = *static_cast<Input *>(context);
    auto count = std::min(capacity, in.bytes.size() - in.offset);
    std::memcpy(buffer, in.bytes.data() + in.offset, count);
    in.offset += count;
    return static_cast<int>(count);
  };
  auto utf8 = dest.u8string();
  if (!platform::replaceAssetFile(reinterpret_cast<const char *>(utf8.c_str()),
                                  s.size(), readBytes, &input))
    return false;
  out = id;
  return true;
}

bool TerrainHistoryStore::recover(std::size_t &n) {
  n = 0;
  if (!ready_ || !safeDirectory(directory_))
    return false;
  std::error_code ec;
  std::filesystem::directory_iterator it(directory_, ec), end;
  if (ec)
    return false;
  while (it != end) {
    auto e = *it;
    auto name = e.path().filename().string();
    auto status = e.symlink_status(ec);
    if (ec)
      return false;
    if (std::filesystem::is_regular_file(status) && name.size() == 83 &&
        valid(name.substr(0, 64)) && name.substr(64) == ".aethist.aether-tmp") {
      if (std::filesystem::remove(e.path(), ec))
        ++n;
      if (ec)
        return false;
    }
    it.increment(ec);
    if (ec)
      return false;
  }
  return true;
}
bool TerrainHistoryStore::collect(
    const std::vector<std::string> &roots,
    const std::vector<std::string> &protectedRoots, std::size_t &n) {
  n = 0;
  if (!ready_ || !safeDirectory(directory_))
    return false;
  std::vector<std::string> refs;
  std::string ignored;
  for (const auto &id : roots)
    if (!load(id, 0, &refs, ignored))
      return false;
  for (const auto &id : protectedRoots)
    if (!load(id, 0, &refs, ignored))
      return false;
  std::set<std::string> live(refs.begin(), refs.end());
  std::vector<std::filesystem::path> dead;
  std::error_code ec;
  std::filesystem::directory_iterator it(directory_, ec), end;
  if (ec)
    return false;
  while (it != end) {
    auto e = *it;
    auto id = e.path().stem().string();
    auto status = e.symlink_status(ec);
    if (ec)
      return false;
    if (std::filesystem::is_regular_file(status) &&
        e.path().extension() == ".aethist" && valid(id) && !live.count(id))
      dead.push_back(e.path());
    it.increment(ec);
    if (ec)
      return false;
  }
  for (const auto &p : dead) {
    if (std::filesystem::remove(p, ec))
      ++n;
    if (ec)
      return false;
  }
  return true;
}
} // namespace ae::resources
