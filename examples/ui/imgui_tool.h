#pragma once
#include "ui/gui_workbench.h"
#include "imgui.h"

// Register from the embedding engine/editor. Runs in the same owned ImGui
// context and renderer as the UI authoring workspace; no second overlay backend.
inline void addGuiExampleTool(ae::ui::GuiWorkbench &workbench) {
  workbench.addTool("Criacao por codigo",[](ae::ui::GuiWorkbench &gui) {
    ImGui::TextWrapped("Esta ferramenta cria um elemento real no documento, com historico e salvamento.");
    if(ImGui::Button("Adicionar botao")) {
      gui.history().begin(gui.document());
      const auto id=gui.document().create(ae::ui::GuiKind::Button);
      if(id) {
        auto node=*gui.document().find(id);node.text="Criado por ImGui";
        std::string error;gui.document().update(node,error);gui.select(id);
      }
      gui.history().commit(gui.document());
    }
  });
}
