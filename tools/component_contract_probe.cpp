// A deterministic before/after snapshot of the actual compiled descriptors.
// This checks generated bindings and archive compatibility, not GPU/device behavior.
#include "scene/component_reflection.h"
#include "scene/component_api_csharp.h"
#include "scene/component_properties.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <cstring>

int main(int argc, char **argv) {
  if (argc == 3 && (std::strcmp(argv[1], "--write-api") == 0 || std::strcmp(argv[1], "--check-api") == 0)) {
    const auto expected = ae::scene::componentCSharpApi();
    std::ifstream existing(argv[2], std::ios::binary);
    std::ostringstream input; input << existing.rdbuf();
    std::string normalized;
    for (char c : input.str()) if (c != '\r') normalized += c;
    if (existing && normalized == expected) { std::cout << "C# component API is current\n"; return 0; }
    if (std::strcmp(argv[1], "--check-api") == 0) { std::cerr << "stale or missing C# component API\n"; return 1; }
    std::ofstream output(argv[2], std::ios::binary); output << expected;
    return output ? 0 : 2;
  }
  if (argc != 2) { std::cerr << "usage: component_contract_probe output\n"; return 2; }
  const auto issues = ae::scene::auditComponentContracts();
  for (const auto &issue : issues)
    std::cerr << issue.typeId << '.' << issue.propertyId << ": " << issue.message << '\n';
  if (!issues.empty()) return 1;
  std::ofstream out(argv[1], std::ios::binary);
  if (!out) return 2;
  out << ae::scene::componentMatrixMarkdown() << '\n' << ae::scene::componentCSharpApi();
  unsigned properties = 0, enumOptions = 0;
  for (const auto &schema : ae::scene::componentSchemas) {
    const auto &type = *schema.type;
    auto value = type.create();
    std::ostringstream saved; value->write(saved);
    auto loaded = type.create(); std::istringstream input(saved.str());
    const bool readable = loaded->read(input, type.version);
    // Unconfigured drafts (e.g. a behavior without a script) need not be loadable.
    // Record that state for differential comparison; valid defaults must roundtrip.
    if (!readable && value->valid()) { std::cerr << "roundtrip failed: " << type.id << '\n'; return 1; }
    std::ostringstream restored; loaded->write(restored);
    if (readable && saved.str() != restored.str()) { std::cerr << "archive changed: " << type.id << '\n'; return 1; }
    out << "\nARCHIVE " << type.id << ' ' << type.version << ' ' << readable << ' ' << saved.str() << '\n';
    for (const auto &p : type.numbers) {
      for (float n : {p.minimum, p.maximum, (p.minimum + p.maximum) * .5f}) {
        auto edited = value->clone();
        if (p.write) {
          auto *slot = p.write(*edited);
          if (slot) *slot = n;
        }
        out << "NUMBER " << p.id << ' ' << p.read(*edited) << ' ' << edited->valid()
            << ' ' << p.presentation.isVisible(*edited) << ' ' << p.presentation.isEditable(*edited) << ' ';
        edited->write(out); out << '\n';
      }
      ++properties;
    }
    for (const auto &p : type.enums) {
      for (const auto &option : p.options) {
        auto edited = value->clone();
        if (p.write) p.write(*edited, option.value);
        out << "ENUM " << p.id << ' ' << option.value << ' ' << option.name << ' '
            << p.read(*edited) << ' ' << edited->valid() << ' ';
        edited->write(out); out << '\n';
        ++enumOptions;
      }
    }
    for (const auto &p : type.booleans) {
      auto edited = value->clone(); if (p.write) p.write(*edited, !p.read(*value));
      out << "BOOL " << p.id << ' ' << p.read(*edited) << ' ' << edited->valid() << ' ';
      edited->write(out); out << '\n'; ++properties;
    }
  }
  out.flush(); if (!out) return 2;
  std::cout << ae::scene::componentSchemas.size() << " schemas, " << properties
            << " numeric/boolean bindings, " << enumOptions << " enum options; contracts and default archive roundtrips passed\n";
}
