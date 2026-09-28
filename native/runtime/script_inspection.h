#pragma once
#include "core/base.h"
#include <string>

namespace ae::runtime {
// Transient inspection state. Never part of authored scenes or runtime values.
struct ScriptFieldIssue {
  u64 instance=0;
  std::string property,description;
  bool isNull=false;
};
}
