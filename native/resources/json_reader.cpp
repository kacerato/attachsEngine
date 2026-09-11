#include "resources/json_reader.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace ae::resources {
namespace {
bool whitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
} // namespace

struct JsonDocument::Parser {
  std::string_view text;
  usize cursor = 0;
  JsonDocument *document = nullptr;

  void skip() { while (cursor < text.size() && whitespace(text[cursor])) ++cursor; }
  bool done() const { return cursor >= text.size(); }
  char peek() const { return text[cursor]; }
  bool literal(std::string_view expected) {
    if (text.size() - cursor < expected.size()) return false;
    if (text.compare(cursor, expected.size(), expected) != 0) return false;
    cursor += expected.size();
    return true;
  }

  u32 addString(std::string value) {
    document->strings_.push_back(std::move(value));
    return static_cast<u32>(document->strings_.size() - 1);
  }

  bool readHex4(u32 &out) {
    if (text.size() - cursor < 4) return false;
    out = 0;
    for (u32 i = 0; i < 4; ++i) {
      const char c = text[cursor + i];
      u32 digit = 0;
      if (c >= '0' && c <= '9') digit = static_cast<u32>(c - '0');
      else if (c >= 'a' && c <= 'f') digit = static_cast<u32>(c - 'a') + 10;
      else if (c >= 'A' && c <= 'F') digit = static_cast<u32>(c - 'A') + 10;
      else return false;
      out = (out << 4) | digit;
    }
    cursor += 4;
    return true;
  }

  void appendUtf8(std::string &out, u32 codepoint) {
    if (codepoint < 0x80) out.push_back(static_cast<char>(codepoint));
    else if (codepoint < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
  }

  bool readString(std::string &out) {
    if (done() || peek() != '"') return false;
    ++cursor;
    out.clear();
    while (true) {
      if (done()) return false;
      const char c = text[cursor++];
      if (c == '"') return true;
      if (static_cast<unsigned char>(c) < 0x20) return false;
      if (c != '\\') { out.push_back(c); continue; }
      if (done()) return false;
      const char escape = text[cursor++];
      switch (escape) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
          u32 codepoint = 0;
          if (!readHex4(codepoint)) return false;
          // Par substituto: um alto sem o baixo produziria UTF-8 inválido num
          // nome de nó que depois vai para a hierarquia do editor.
          if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            u32 low = 0;
            if (!literal("\\u") || !readHex4(low) || low < 0xDC00 || low > 0xDFFF) return false;
            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
          } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) return false;
          appendUtf8(out, codepoint);
          break;
        }
        default: return false;
      }
    }
  }

  bool readNumber(double &out) {
    const auto start = cursor;
    if (!done() && peek() == '-') ++cursor;
    if (done() || peek() < '0' || peek() > '9') return false;
    // Zero à esquerda é inválido em JSON e costuma indicar um arquivo truncado
    // ou reescrito à mão.
    if (peek() == '0') ++cursor;
    else while (!done() && peek() >= '0' && peek() <= '9') ++cursor;
    if (!done() && peek() == '.') {
      ++cursor;
      if (done() || peek() < '0' || peek() > '9') return false;
      while (!done() && peek() >= '0' && peek() <= '9') ++cursor;
    }
    if (!done() && (peek() == 'e' || peek() == 'E')) {
      ++cursor;
      if (!done() && (peek() == '+' || peek() == '-')) ++cursor;
      if (done() || peek() < '0' || peek() > '9') return false;
      while (!done() && peek() >= '0' && peek() <= '9') ++cursor;
    }
    const auto length = cursor - start;
    if (length >= 64) return false;
    char buffer[64];
    std::memcpy(buffer, text.data() + start, length);
    buffer[length] = 0;
    char *end = nullptr;
    // `strtod` e não `from_chars`: a conversão de ponto flutuante de
    // `from_chars` não está disponível em todas as bibliotecas padrão que este
    // projeto compila, e uma importação não pode depender de qual delas é.
    out = std::strtod(buffer, &end);
    if (end != buffer + length || !std::isfinite(out)) return false;
    return true;
  }

  bool readValue(u32 depth, u32 &nodeIndex) {
    if (depth > MaximumDepth) return false;
    if (document->nodes_.size() >= MaximumNodes) return false;
    skip();
    if (done()) return false;
    nodeIndex = static_cast<u32>(document->nodes_.size());
    document->nodes_.push_back({});
    const char c = peek();
    if (c == '{' || c == '[') {
      const bool object = c == '{';
      ++cursor;
      std::vector<u32> children;
      skip();
      if (done()) return false;
      if (peek() == (object ? '}' : ']')) ++cursor;
      else while (true) {
        std::string key;
        if (object) {
          skip();
          if (!readString(key)) return false;
          skip();
          if (done() || peek() != ':') return false;
          ++cursor;
        }
        u32 child = 0;
        if (!readValue(depth + 1, child)) return false;
        document->nodes_[child].key = object ? addString(std::move(key)) : 0;
        children.push_back(child);
        skip();
        if (done()) return false;
        if (peek() == ',') { ++cursor; continue; }
        if (peek() == (object ? '}' : ']')) { ++cursor; break; }
        return false;
      }
      auto &node = document->nodes_[nodeIndex];
      node.kind = object ? Kind::Object : Kind::Array;
      node.childBegin = static_cast<u32>(document->children_.size());
      node.childCount = static_cast<u32>(children.size());
      document->children_.insert(document->children_.end(), children.begin(), children.end());
      return true;
    }
    if (c == '"') {
      std::string value;
      if (!readString(value)) return false;
      auto &node = document->nodes_[nodeIndex];
      node.kind = Kind::String;
      node.text = addString(std::move(value));
      return true;
    }
    if (c == 't' || c == 'f') {
      const bool value = c == 't';
      if (!literal(value ? "true" : "false")) return false;
      auto &node = document->nodes_[nodeIndex];
      node.kind = Kind::Boolean;
      node.boolean = value;
      return true;
    }
    if (c == 'n') {
      if (!literal("null")) return false;
      document->nodes_[nodeIndex].kind = Kind::Null;
      return true;
    }
    double value = 0;
    if (!readNumber(value)) return false;
    auto &node = document->nodes_[nodeIndex];
    node.kind = Kind::Number;
    node.number = value;
    return true;
  }
};

bool JsonDocument::parse(std::string_view text, JsonDocument &out) {
  JsonDocument candidate;
  // Índice 0 do pool é a chave vazia: todo nó tem `key` válido sem um caso
  // especial para "sem chave".
  candidate.strings_.emplace_back();
  Parser parser{text, 0, &candidate};
  u32 rootIndex = 0;
  if (!parser.readValue(0, rootIndex) || rootIndex != 0) return false;
  parser.skip();
  // Lixo depois do valor raiz é arquivo corrompido ou concatenado, não um
  // documento com sobra inofensiva.
  if (!parser.done()) return false;
  out = std::move(candidate);
  return true;
}

} // namespace ae::resources
