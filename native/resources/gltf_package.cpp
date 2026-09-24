#include "resources/gltf_package.h"
#include "core/sha256.h"
#include "resources/json_reader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ae::resources {
namespace {
using Node = JsonDocument::Node;
using Kind = JsonDocument::Kind;

constexpr u32 kGlbMagic = 0x46546C67, kChunkJson = 0x4E4F534A, kChunkBin = 0x004E4942;

u32 readU32(std::span<const u8> bytes, usize offset) {
  u32 value = 0;
  std::memcpy(&value, bytes.data() + offset, 4);
  return value;
}

void putU32(std::vector<u8> &out, u32 value) {
  for (u32 i = 0; i < 4; ++i) out.push_back(static_cast<u8>(value >> (i * 8)));
}

bool isGlb(std::span<const u8> bytes) { return bytes.size() >= 12 && readU32(bytes, 0) == kGlbMagic; }

bool splitGlb(std::span<const u8> bytes, std::string_view &json, std::span<const u8> &binary) {
  json = {};
  binary = {};
  if (bytes.size() < 20 || readU32(bytes, 0) != kGlbMagic || readU32(bytes, 4) != 2) return false;
  const u64 declared = std::min<u64>(readU32(bytes, 8), bytes.size());
  usize cursor = 12;
  while (cursor + 8 <= declared) {
    const u64 length = readU32(bytes, cursor);
    const u32 type = readU32(bytes, cursor + 4);
    cursor += 8;
    if (length > declared - cursor) return false;
    if (type == kChunkJson && json.empty())
      json = std::string_view(reinterpret_cast<const char *>(bytes.data() + cursor), static_cast<usize>(length));
    else if (type == kChunkBin && binary.empty())
      binary = bytes.subspan(cursor, static_cast<usize>(length));
    cursor += static_cast<usize>(length + ((4 - length % 4) % 4));
  }
  return !json.empty();
}

std::string_view mainJson(std::span<const u8> main, std::span<const u8> &binary, bool &glb) {
  glb = isGlb(main);
  std::string_view json;
  if (glb) {
    if (!splitGlb(main, json, binary)) return {};
    return json;
  }
  binary = {};
  return std::string_view(reinterpret_cast<const char *>(main.data()), main.size());
}

void appendEscaped(std::string &out, std::string_view text) {
  out.push_back('"');
  for (const unsigned char c : text) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if (c < 0x20) {
          char code[8];
          std::snprintf(code, sizeof code, "\\u%04x", c);
          out += code;
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }
  out.push_back('"');
}

void appendNumber(std::string &out, double value) {
  char text[40];
  if (std::fabs(value) < 9007199254740992.0 && value == static_cast<double>(static_cast<long long>(value)))
    std::snprintf(text, sizeof text, "%lld", static_cast<long long>(value));
  else
    std::snprintf(text, sizeof text, "%.17g", value);
  out += text;
}

void appendValue(std::string &out, const JsonDocument &document, const Node &node) {
  switch (node.kind) {
    case Kind::Null: out += "null"; break;
    case Kind::Boolean: out += node.boolean ? "true" : "false"; break;
    case Kind::Number: appendNumber(out, node.number); break;
    case Kind::String: appendEscaped(out, document.textOf(node)); break;
    case Kind::Array:
      out.push_back('[');
      for (u32 i = 0; i < node.childCount; ++i) {
        if (i) out.push_back(',');
        appendValue(out, document, *document.child(node, i));
      }
      out.push_back(']');
      break;
    case Kind::Object:
      out.push_back('{');
      for (u32 i = 0; i < node.childCount; ++i) {
        const auto &member = *document.child(node, i);
        if (i) out.push_back(',');
        appendEscaped(out, document.keyOf(member));
        out.push_back(':');
        appendValue(out, document, member);
      }
      out.push_back('}');
      break;
  }
}

int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool decodeBase64(std::string_view text, std::vector<u8> &out) {
  out.clear();
  out.reserve(text.size() / 4 * 3);
  u32 accumulator = 0;
  int bits = 0;
  usize padding = 0;
  for (const char c : text) {
    int value;
    if (c >= 'A' && c <= 'Z') value = c - 'A';
    else if (c >= 'a' && c <= 'z') value = c - 'a' + 26;
    else if (c >= '0' && c <= '9') value = c - '0' + 52;
    else if (c == '+') value = 62;
    else if (c == '/') value = 63;
    else if (c == '=') { ++padding; continue; }
    else return false;
    if (padding) return false; // dado depois do preenchimento
    accumulator = (accumulator << 6) | static_cast<u32>(value);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<u8>(accumulator >> bits));
    }
  }
  return padding <= 2;
}

std::string mimeFor(std::string_view uri) {
  const auto dot = uri.rfind('.');
  if (dot == std::string_view::npos) return {};
  std::string extension(uri.substr(dot + 1));
  for (auto &c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (extension == "png") return "image/png";
  if (extension == "jpg" || extension == "jpeg") return "image/jpeg";
  if (extension == "ktx2") return "image/ktx2";
  if (extension == "webp") return "image/webp";
  return {};
}

enum class Resolution { Found, Missing, Refused };

struct Resolver {
  std::span<const GltfPackageFile> files;
  std::vector<bool> used;
  std::vector<std::vector<u8>> decoded; // data: URIs e arquivos lidos da pasta
  std::vector<std::string> missing;
  std::string refusal;
  const GltfFolder *folder = nullptr;   // pasta: casa pelo caminho relativo

  Resolution resolve(std::string_view uri, std::span<const u8> &bytes, GltfDependency &dependency) {
    dependency = {};
    dependency.uri = std::string(uri.substr(0, 512));
    if (uri.starts_with("data:")) {
      const auto comma = uri.find(',');
      if (comma == std::string_view::npos || uri.substr(0, comma).find(";base64") == std::string_view::npos) {
        refusal = "URI data: sem base64 não é lida.";
        return Resolution::Refused;
      }
      decoded.emplace_back();
      if (!decodeBase64(uri.substr(comma + 1), decoded.back())) {
        refusal = "URI data: com base64 inválido.";
        return Resolution::Refused;
      }
      bytes = decoded.back();
      dependency.dataUri = true;
      return Resolution::Found;
    }
    std::string relative;
    if (!gltfRelativeUri(uri, relative, refusal)) return Resolution::Refused;
    if (folder) {
      decoded.emplace_back();
      if (!folder->read || !folder->read(folder->context, relative, decoded.back())) {
        decoded.pop_back();
        if (std::find(missing.begin(), missing.end(), relative) == missing.end()) missing.push_back(relative);
        return Resolution::Missing;
      }
      bytes = decoded.back();
      dependency.file = relative;
      return Resolution::Found;
    }
    const auto slash = relative.rfind('/');
    const std::string name = slash == std::string::npos ? relative : relative.substr(slash + 1);
    i64 match = -1;
    for (usize f = 0; f < files.size(); ++f)
      if (files[f].name == name) {
        if (match >= 0) {
          refusal = "Dois arquivos escolhidos se chamam " + name + "; a dependência é ambígua.";
          return Resolution::Refused;
        }
        match = static_cast<i64>(f);
      }
    if (match < 0) {
      if (std::find(missing.begin(), missing.end(), name) == missing.end()) missing.push_back(name);
      return Resolution::Missing;
    }
    used[static_cast<usize>(match)] = true;
    bytes = files[static_cast<usize>(match)].bytes;
    dependency.file = name;
    return Resolution::Found;
  }
};

bool meshoptFallback(const JsonDocument &document, const Node &buffer) {
  const auto *extensions = document.member(buffer, "extensions");
  if (!extensions || extensions->kind != Kind::Object) return false;
  for (const auto name : {std::string_view("EXT_meshopt_compression"), std::string_view("KHR_meshopt_compression")})
    if (const auto *extension = document.member(*extensions, name); extension && extension->kind == Kind::Object)
      if (document.boolean(*extension, "fallback", false)) return true;
  return false;
}
} // namespace

bool gltfNeedsPackage(std::span<const u8> main) {
  std::span<const u8> binary;
  bool glb = false;
  const auto json = mainJson(main, binary, glb);
  if (!glb) return !main.empty();
  JsonDocument document;
  if (json.empty() || !JsonDocument::parse(json, document) || !document.root()) return false;
  const auto &root = *document.root();
  for (const auto field : {std::string_view("buffers"), std::string_view("images")})
    if (const auto *list = document.member(root, field); list && list->kind == Kind::Array)
      for (u32 i = 0; i < list->childCount; ++i)
        if (!document.string(*document.child(*list, i), "uri").empty()) return true;
  return false;
}

namespace {
bool packImpl(std::span<const u8> main, Resolver &resolver, bool externalImages, u64 maximumBytes,
              GltfPackage &out, std::string &diagnostic) {
  out = {};
  diagnostic.clear();
  std::span<const u8> glbBinary;
  bool glb = false;
  const auto json = mainJson(main, glbBinary, glb);
  if (json.empty()) {
    diagnostic = glb ? "GLB sem bloco JSON legível." : "Arquivo glTF vazio.";
    return false;
  }
  JsonDocument document;
  if (!JsonDocument::parse(json, document) || !document.root() || document.root()->kind != Kind::Object) {
    diagnostic = "O JSON do glTF está mal formado.";
    return false;
  }
  const auto &root = *document.root();
  const auto *buffers = document.member(root, "buffers");
  const auto *views = document.member(root, "bufferViews");
  const auto *images = document.member(root, "images");
  if (buffers && buffers->kind != Kind::Array) buffers = nullptr;
  if (views && views->kind != Kind::Array) views = nullptr;
  if (images && images->kind != Kind::Array) images = nullptr;

  std::vector<u8> packed;
  std::vector<u64> bases;
  const auto record = [&](GltfDependency dependency, std::span<const u8> bytes) {
    for (const auto &existing : out.dependencies)
      if (existing.uri == dependency.uri) return;
    dependency.bytes = bytes.size();
    dependency.sha256 = Sha256::hex(bytes);
    out.dependencies.push_back(std::move(dependency));
  };
  const auto append = [&](std::span<const u8> bytes) {
    if (packed.size() + bytes.size() + 3 > maximumBytes) return false;
    packed.insert(packed.end(), bytes.begin(), bytes.end());
    while (packed.size() % 4) packed.push_back(0);
    return true;
  };

  const u32 bufferCount = buffers ? buffers->childCount : 0;
  for (u32 i = 0; i < bufferCount; ++i) {
    const auto &buffer = *document.child(*buffers, i);
    if (buffer.kind != Kind::Object) {
      diagnostic = "Buffer inválido no glTF.";
      return false;
    }
    const auto length = static_cast<u64>(document.number(buffer, "byteLength", 0));
    const auto uri = document.string(buffer, "uri");
    bases.push_back(packed.size());
    std::span<const u8> bytes;
    if (!uri.empty()) {
      GltfDependency dependency;
      const auto resolution = resolver.resolve(uri, bytes, dependency);
      if (resolution == Resolution::Refused) {
        diagnostic = resolver.refusal;
        return false;
      }
      if (resolution == Resolution::Missing) continue;
      if (bytes.size() < length) {
        diagnostic = "O arquivo " + (dependency.file.empty() ? std::string("data:") : dependency.file) +
                     " é menor que o byteLength declarado pelo glTF.";
        return false;
      }
      record(dependency, bytes.first(static_cast<usize>(length)));
    } else if (glb && i == 0) {
      if (glbBinary.size() < length) {
        diagnostic = "Bloco binário do GLB menor que o buffer declarado.";
        return false;
      }
      bytes = glbBinary;
    } else if (meshoptFallback(document, buffer)) {
      continue; // reserva sem dados: só as extensões meshopt apontam para cá
    } else {
      diagnostic = "Buffer " + std::to_string(i) + " sem uri e fora do bloco binário do GLB.";
      return false;
    }
    if (!append(bytes.first(static_cast<usize>(length)))) {
      diagnostic = "As dependências juntas passam do limite de bytes desta importação.";
      return false;
    }
  }

  const u32 viewCount = views ? views->childCount : 0;
  std::vector<i64> imageViews(images ? images->childCount : 0, -1);
  std::vector<std::pair<u64, u64>> appendedViews; // (offset, length)
  // Pasta: a imagem continua por URI; quem lê é o importador, uma de cada vez.
  for (u32 i = 0; images && !externalImages && i < images->childCount; ++i) {
    const auto &image = *document.child(*images, i);
    const auto uri = image.kind == Kind::Object ? document.string(image, "uri") : std::string_view{};
    if (uri.empty()) continue;
    std::span<const u8> bytes;
    GltfDependency dependency;
    const auto resolution = resolver.resolve(uri, bytes, dependency);
    if (resolution == Resolution::Refused) {
      diagnostic = resolver.refusal;
      return false;
    }
    if (resolution == Resolution::Missing) continue;
    const u64 offset = packed.size();
    if (!append(bytes)) {
      diagnostic = "As dependências juntas passam do limite de bytes desta importação.";
      return false;
    }
    record(dependency, bytes);
    imageViews[i] = static_cast<i64>(viewCount + appendedViews.size());
    appendedViews.emplace_back(offset, bytes.size());
  }

  if (!resolver.missing.empty()) {
    diagnostic = "Faltam arquivos referenciados pelo glTF: ";
    for (usize i = 0; i < resolver.missing.size() && i < 6; ++i) diagnostic += (i ? ", " : "") + resolver.missing[i];
    if (resolver.missing.size() > 6) diagnostic += ", …";
    diagnostic += resolver.folder ? std::string(". A pasta do modelo precisa conter esses arquivos nos caminhos declarados.")
                                  : std::string(". Selecione-os junto com o arquivo principal (seleção múltipla no seletor).");
    return false;
  }
  for (const bool used : resolver.used) out.unusedFiles += used ? 0 : 1;

  // Views: reendereçadas para o buffer único; as de meshopt também na extensão.
  const auto baseOf = [&](const Node &object) -> u64 {
    const auto index = document.index(object, "buffer");
    return index >= 0 && static_cast<usize>(index) < bases.size() ? bases[static_cast<usize>(index)] : 0;
  };
  for (u32 i = 0; i < viewCount; ++i) {
    const auto index = document.index(*document.child(*views, i), "buffer");
    if (index < 0 || index >= static_cast<i64>(bufferCount)) {
      diagnostic = "bufferView " + std::to_string(i) + " aponta para buffer inexistente.";
      return false;
    }
  }
  std::string text;
  const auto writeAddressed = [&](const Node &object, const auto &extensionWriter) {
    text.push_back('{');
    bool first = true, hadOffset = false;
    const auto key = [&](std::string_view name) {
      if (!first) text.push_back(',');
      first = false;
      appendEscaped(text, name);
      text.push_back(':');
    };
    const u64 base = baseOf(object);
    for (u32 m = 0; m < object.childCount; ++m) {
      const auto &member = *document.child(object, m);
      const auto name = document.keyOf(member);
      key(name);
      if (name == "buffer") text.push_back('0');
      else if (name == "byteOffset") { appendNumber(text, static_cast<double>(base) + member.number); hadOffset = true; }
      else if (name == "extensions" && member.kind == Kind::Object) extensionWriter(member);
      else appendValue(text, document, member);
    }
    if (!hadOffset && base) {
      key("byteOffset");
      appendNumber(text, static_cast<double>(base));
    }
    text.push_back('}');
  };
  const auto plainExtensions = [&](const Node &extensions) { appendValue(text, document, extensions); };
  const auto viewExtensions = [&](const Node &extensions) {
    text.push_back('{');
    for (u32 e = 0; e < extensions.childCount; ++e) {
      const auto &extension = *document.child(extensions, e);
      const auto name = document.keyOf(extension);
      if (e) text.push_back(',');
      appendEscaped(text, name);
      text.push_back(':');
      if ((name == "EXT_meshopt_compression" || name == "KHR_meshopt_compression") && extension.kind == Kind::Object)
        writeAddressed(extension, plainExtensions);
      else
        appendValue(text, document, extension);
    }
    text.push_back('}');
  };
  const auto writeViews = [&]() {
    text.push_back('[');
    for (u32 i = 0; i < viewCount; ++i) {
      if (i) text.push_back(',');
      writeAddressed(*document.child(*views, i), viewExtensions);
    }
    for (usize i = 0; i < appendedViews.size(); ++i) {
      if (viewCount || i) text.push_back(',');
      text += "{\"buffer\":0,\"byteOffset\":";
      appendNumber(text, static_cast<double>(appendedViews[i].first));
      text += ",\"byteLength\":";
      appendNumber(text, static_cast<double>(appendedViews[i].second));
      text.push_back('}');
    }
    text.push_back(']');
  };
  const auto writeImages = [&]() {
    text.push_back('[');
    for (u32 i = 0; i < images->childCount; ++i) {
      if (i) text.push_back(',');
      const auto &image = *document.child(*images, i);
      if (imageViews[i] < 0) {
        appendValue(text, document, image);
        continue;
      }
      text.push_back('{');
      bool first = true, hasMime = false;
      std::string_view uri;
      for (u32 m = 0; m < image.childCount; ++m) {
        const auto &member = *document.child(image, m);
        const auto name = document.keyOf(member);
        if (name == "uri") { uri = document.textOf(member); continue; }
        hasMime |= name == "mimeType";
        if (!first) text.push_back(',');
        first = false;
        appendEscaped(text, name);
        text.push_back(':');
        appendValue(text, document, member);
      }
      if (!first) text.push_back(',');
      text += "\"bufferView\":";
      appendNumber(text, static_cast<double>(imageViews[i]));
      if (const auto mime = mimeFor(uri); !hasMime && !mime.empty()) {
        text += ",\"mimeType\":";
        appendEscaped(text, mime);
      }
      text.push_back('}');
    }
    text.push_back(']');
  };

  text.push_back('{');
  bool first = true, wroteBuffers = false, wroteViews = false;
  const auto rootKey = [&](std::string_view name) {
    if (!first) text.push_back(',');
    first = false;
    appendEscaped(text, name);
    text.push_back(':');
  };
  const auto writeBuffers = [&]() {
    text += "[{\"byteLength\":";
    appendNumber(text, static_cast<double>(packed.size()));
    text += "}]";
  };
  for (u32 i = 0; i < root.childCount; ++i) {
    const auto &member = *document.child(root, i);
    const auto name = document.keyOf(member);
    rootKey(name);
    if (name == "buffers") { writeBuffers(); wroteBuffers = true; }
    else if (name == "bufferViews" && views) { writeViews(); wroteViews = true; }
    else if (name == "images" && images) writeImages();
    else appendValue(text, document, member);
  }
  if (!wroteBuffers && !packed.empty()) { rootKey("buffers"); writeBuffers(); }
  if (!wroteViews && !appendedViews.empty()) { rootKey("bufferViews"); writeViews(); }
  text.push_back('}');
  while (text.size() % 4) text.push_back(' ');

  const u64 total = 12 + 8 + text.size() + (packed.empty() ? 0 : 8 + packed.size());
  if (total > maximumBytes) {
    diagnostic = "O glTF empacotado passa do limite de bytes desta importação.";
    return false;
  }
  out.glb.reserve(static_cast<usize>(total));
  putU32(out.glb, kGlbMagic);
  putU32(out.glb, 2);
  putU32(out.glb, static_cast<u32>(total));
  putU32(out.glb, static_cast<u32>(text.size()));
  putU32(out.glb, kChunkJson);
  out.glb.insert(out.glb.end(), text.begin(), text.end());
  if (!packed.empty()) {
    putU32(out.glb, static_cast<u32>(packed.size()));
    putU32(out.glb, kChunkBin);
    out.glb.insert(out.glb.end(), packed.begin(), packed.end());
  }
  return true;
}

} // namespace

bool gltfRelativeUri(std::string_view uri, std::string &relative, std::string &refusal) {
  relative.clear();
  refusal.clear();
  if (uri.starts_with("data:")) return false;
  // Esquema (http:, https:, file:, content:…) antes de qualquer '/', ou caminho
  // absoluto: nunca rede implícita nem busca fora do que o usuário escolheu.
  const auto colon = uri.find(':'), slash = uri.find('/');
  if (uri.starts_with('/') || uri.find('\\') != std::string_view::npos ||
      (colon != std::string_view::npos && (slash == std::string_view::npos || colon < slash))) {
    refusal = "URI absoluta ou de rede não é lida (" + std::string(uri.substr(0, 128)) +
              "); a importação não acessa a rede nem busca arquivos pelo aparelho.";
    return false;
  }
  std::string path;
  for (usize i = 0; i < uri.size(); ++i) {
    if (uri[i] == '%') {
      const int high = i + 2 < uri.size() ? hexDigit(uri[i + 1]) : -1, low = i + 2 < uri.size() ? hexDigit(uri[i + 2]) : -1;
      if (high < 0 || low < 0) {
        refusal = "URI com codificação percentual inválida: " + std::string(uri.substr(0, 128)) + ".";
        return false;
      }
      path.push_back(static_cast<char>(high * 16 + low));
      i += 2;
    } else {
      path.push_back(uri[i]);
    }
  }
  if (path.find('\0') != std::string::npos || path.find('\\') != std::string::npos) {
    refusal = "URI com caractere inválido: " + std::string(uri.substr(0, 128)) + ".";
    return false;
  }
  usize begin = 0;
  while (begin <= path.size()) {
    const auto end = std::min(path.find('/', begin), path.size());
    const auto segment = std::string_view(path).substr(begin, end - begin);
    if (segment == "..") {
      refusal = "URI sai da pasta do arquivo (" + std::string(uri.substr(0, 128)) + ").";
      relative.clear();
      return false;
    }
    if (!segment.empty() && segment != ".") {
      if (!relative.empty()) relative.push_back('/');
      relative += segment;
    }
    begin = end + 1;
  }
  if (relative.empty() || relative.back() == '/') {
    refusal = "URI vazia ou sem nome de arquivo.";
    relative.clear();
    return false;
  }
  return true;
}

bool listGltfFolderDependencies(std::span<const u8> main, std::vector<GltfFolderDependency> &out,
                                std::string &diagnostic) {
  out.clear();
  diagnostic.clear();
  std::span<const u8> binary;
  bool glb = false;
  const auto json = mainJson(main, binary, glb);
  JsonDocument document;
  if (json.empty() || !JsonDocument::parse(json, document) || !document.root() || document.root()->kind != Kind::Object) {
    diagnostic = "O JSON do glTF está mal formado.";
    return false;
  }
  const auto &root = *document.root();
  for (const auto field : {std::string_view("buffers"), std::string_view("images")}) {
    const auto *list = document.member(root, field);
    if (!list || list->kind != Kind::Array) continue;
    for (u32 i = 0; i < list->childCount; ++i) {
      const auto &item = *document.child(*list, i);
      const auto uri = item.kind == Kind::Object ? document.string(item, "uri") : std::string_view{};
      if (uri.empty() || uri.starts_with("data:")) continue;
      GltfFolderDependency dependency;
      if (!gltfRelativeUri(uri, dependency.relative, diagnostic)) return false;
      if (std::any_of(out.begin(), out.end(), [&](const auto &known) { return known.relative == dependency.relative; }))
        continue;
      dependency.uri = std::string(uri.substr(0, 512));
      dependency.image = field == "images";
      out.push_back(std::move(dependency));
    }
  }
  return true;
}

bool packGltf(std::span<const u8> main, std::span<const GltfPackageFile> companions, u64 maximumBytes,
              GltfPackage &out, std::string &diagnostic) {
  Resolver resolver{companions, std::vector<bool>(companions.size(), false), {}, {}, {}};
  return packImpl(main, resolver, false, maximumBytes, out, diagnostic);
}

bool packGltfFolder(std::span<const u8> main, const GltfFolder &folder, u64 maximumBytes, GltfPackage &out,
                    std::string &diagnostic) {
  Resolver resolver{{}, {}, {}, {}, {}};
  resolver.folder = &folder;
  return packImpl(main, resolver, true, maximumBytes, out, diagnostic);
}

std::string serializeGltfManifest(std::string_view mainName, std::span<const u8> main, const GltfPackage &package) {
  std::string text = "ASTRA_GLTF_DEPS 1\nmain ";
  appendEscaped(text, mainName);
  text += " " + std::to_string(main.size()) + " " + Sha256::hex(main) + "\n";
  for (const auto &dependency : package.dependencies) {
    text += dependency.dataUri ? "data " : "file ";
    appendEscaped(text, dependency.uri);
    text.push_back(' ');
    appendEscaped(text, dependency.file);
    text += " " + std::to_string(dependency.bytes) + " " + dependency.sha256 + "\n";
  }
  text += "packed " + std::to_string(package.glb.size()) + " " + Sha256::hex(package.glb) + "\n";
  return text;
}
} // namespace ae::resources
