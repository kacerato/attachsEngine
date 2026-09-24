#include "resources/gltf_folder_source.h"
#include "core/sha256.h"

#include <fstream>

namespace ae::resources {
bool readGltfFolderFile(const std::filesystem::path &directory, const std::string &relative, std::vector<u8> &bytes,
                        u64 maximum, u64 prefix) {
  bytes.clear();
  if (directory.empty() || relative.empty() || relative.find('\0') != std::string::npos) return false;
  std::filesystem::path path = directory;
  std::error_code error;
  usize begin = 0;
  while (begin <= relative.size()) {
    const auto end = std::min(relative.find('/', begin), relative.size());
    const auto segment = relative.substr(begin, end - begin);
    // O caminho já vem normalizado; conferir de novo custa nada e fecha a pasta.
    if (segment.empty() || segment == "." || segment == "..") return false;
    path /= std::filesystem::path(std::u8string(segment.begin(), segment.end()));
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(path, error)) || (error && end < relative.size()))
      return false;
    error.clear();
    begin = end + 1;
  }
  const auto size = std::filesystem::file_size(path, error);
  if (error || (!prefix && size > maximum) || !std::filesystem::is_regular_file(path, error)) return false;
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  bytes.resize(static_cast<usize>(prefix ? std::min(prefix, static_cast<u64>(size)) : size));
  return bytes.empty() || static_cast<bool>(input.read(reinterpret_cast<char *>(bytes.data()),
                                                       static_cast<std::streamsize>(bytes.size())));
}

std::string serializeGltfFolderManifest(std::string_view mainName, std::span<const u8> main,
                                        std::span<const GltfFolderFileRecord> files) {
  // Nomes entre aspas com escape mínimo: o manifesto é lido por pessoas e por
  // ferramentas; espaço em nome de arquivo é comum em pastas de artista.
  const auto quoted = [](std::string_view text) {
    std::string out = "\"";
    for (const char c : text) {
      if (c == '"' || c == '\\') out.push_back('\\');
      out.push_back(c == '\n' ? ' ' : c);
    }
    return out + "\"";
  };
  std::string text = "ASTRA_GLTF_DEPS 2\nmain " + quoted(mainName) + " " + std::to_string(main.size()) + " " +
                     Sha256::hex(main) + "\n";
  for (const auto &file : files)
    text += "folder " + quoted(file.relative) + " " + std::to_string(file.bytes) + " " + file.sha256 + "\n";
  return text;
}

std::string gltfFolderContentHash(std::span<const u8> main, std::string_view manifest) {
  std::vector<u8> joined(main.begin(), main.end());
  joined.push_back('\n');
  joined.insert(joined.end(), manifest.begin(), manifest.end());
  return Sha256::hex(joined);
}

namespace {
struct FolderBridge {
  const GltfImportProgress *outer = nullptr;
  std::filesystem::path directory;
};
} // namespace

bool importGltfFolder(std::span<const u8> main, const std::filesystem::path &directory, u64 maximumPackedBytes,
                      const GltfImportLimits &limits, const GltfImportProgress &progress, GltfImport &out,
                      std::vector<u8> &packed) {
  out = {};
  packed.clear();
  FolderBridge bridge{&progress, directory};
  GltfFolder folder;
  folder.context = &bridge;
  folder.read = [](void *context, const std::string &relative, std::vector<u8> &bytes) {
    return readGltfFolderFile(static_cast<FolderBridge *>(context)->directory, relative, bytes);
  };
  GltfPackage package;
  std::string diagnostic;
  if (!packGltfFolder(main, folder, maximumPackedBytes, package, diagnostic)) {
    out.diagnostic = diagnostic;
    return false;
  }
  GltfImportProgress wrapped;
  wrapped.context = &bridge;
  if (progress.report)
    wrapped.report = [](void *context, float fraction, const char *stage) {
      const auto &outer = *static_cast<FolderBridge *>(context)->outer;
      outer.report(outer.context, fraction, stage);
    };
  if (progress.cancelled)
    wrapped.cancelled = [](void *context) {
      const auto &outer = *static_cast<FolderBridge *>(context)->outer;
      return outer.cancelled(outer.context);
    };
  wrapped.externalFile = [](void *context, std::string_view uri, u64 prefix, std::vector<u8> &bytes) {
    std::string relative, refusal;
    return gltfRelativeUri(uri, relative, refusal) &&
           readGltfFolderFile(static_cast<FolderBridge *>(context)->directory, relative, bytes,
                              GltfFolderMaximumFileBytes, prefix);
  };
  if (!importGlb(package.glb, limits, wrapped, out)) return false;
  packed = std::move(package.glb);
  return true;
}
} // namespace ae::resources
