#pragma once
// Relatorio da importacao de projeto UMotion Pro (editor_umotion_import.cpp).
#include "core/base.h"
#include <string>
#include <vector>

namespace ae::editor {
struct UMotionImportReport {
  std::vector<std::string> clips;   // caminhos dos .aeclip criados
  unsigned bones=0,helpers=0,ikChains=0,childOf=0,tracks=0,propertyTracks=0,cues=0,layers=0;
  double restError=0;               // erro relativo do ajuste de referencial no repouso
  std::vector<std::string> silencedAnimations; // animacoes do personagem que deixaram de iniciar sozinhas
};
} // namespace ae::editor
