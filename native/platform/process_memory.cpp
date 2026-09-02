#include "platform/process_memory.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace ae::platform {
namespace {

bool readKilobytes(std::string_view text, std::string_view key, u64 &out) {
  const usize position = text.find(key);
  if (position == std::string_view::npos) return false;
  usize cursor = position + key.size();
  while (cursor < text.size() && (text[cursor] == ' ' || text[cursor] == '\t')) ++cursor;
  if (cursor == text.size() || text[cursor] < '0' || text[cursor] > '9') return false;
  u64 kilobytes = 0;
  while (cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9') {
    const u64 digit = static_cast<u64>(text[cursor] - '0');
    if (kilobytes > (std::numeric_limits<u64>::max() - digit) / 10) return false;
    kilobytes = kilobytes * 10 + digit;
    ++cursor;
  }
  if (kilobytes > std::numeric_limits<u64>::max() / 1024) return false;
  out = kilobytes * 1024;
  return true;
}

#if defined(__linux__) || defined(__ANDROID__)
bool readTextFile(const char *path, char *buffer, usize capacity, std::string_view &out) {
  if (path == nullptr || buffer == nullptr || capacity < 2) return false;
  FILE *file = std::fopen(path, "rb");
  if (file == nullptr) return false;
  const usize size = std::fread(buffer, 1, capacity - 1, file);
  const bool readFailed = std::ferror(file) != 0;
  const bool truncated = !std::feof(file);
  std::fclose(file);
  if (readFailed || truncated || size == 0) return false;
  buffer[size] = '\0';
  out = std::string_view(buffer, size);
  return true;
}
#endif

} // namespace

bool parseProcStatus(std::string_view text, ProcessMemorySnapshot &out) {
  ProcessMemorySnapshot parsed{};
  const bool hasResident = readKilobytes(text, "VmRSS:", parsed.residentBytes);
  readKilobytes(text, "VmSize:", parsed.virtualBytes);
  readKilobytes(text, "VmHWM:", parsed.peakResidentBytes);
  readKilobytes(text, "RssAnon:", parsed.anonymousBytes);
  readKilobytes(text, "RssFile:", parsed.fileBytes);
  readKilobytes(text, "RssShmem:", parsed.sharedBytes);
  readKilobytes(text, "VmSwap:", parsed.swapBytes);
  parsed.valid = hasResident;
  out = parsed;
  return parsed.valid;
}

bool parseProcMemInfo(std::string_view text, SystemMemorySnapshot &out) {
  SystemMemorySnapshot parsed{};
  const bool hasTotal = readKilobytes(text, "MemTotal:", parsed.totalBytes);
  const bool hasAvailable = readKilobytes(text, "MemAvailable:", parsed.availableBytes);
  parsed.valid = hasTotal && hasAvailable && parsed.totalBytes > 0 &&
                 parsed.availableBytes <= parsed.totalBytes;
  out = parsed;
  return parsed.valid;
}

bool readProcessMemorySnapshot(ProcessMemorySnapshot &out) {
#if defined(__linux__) || defined(__ANDROID__)
  char buffer[8192];
  std::string_view text;
  if (!readTextFile("/proc/self/status", buffer, sizeof(buffer), text)) {
    out = {};
    return false;
  }
  return parseProcStatus(text, out);
#else
  out = {};
  return false;
#endif
}

bool readSystemMemorySnapshot(SystemMemorySnapshot &out) {
#if defined(__linux__) || defined(__ANDROID__)
  char buffer[8192];
  std::string_view text;
  if (!readTextFile("/proc/meminfo", buffer, sizeof(buffer), text)) {
    out = {};
    return false;
  }
  return parseProcMemInfo(text, out);
#else
  out = {};
  return false;
#endif
}

} // namespace ae::platform
