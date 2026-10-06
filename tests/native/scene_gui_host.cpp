// Standalone runtime proof: deliberately no editor headers or editor library.
#include "runtime/scene_gui.h"
#include "ui/ui_instance_builder.h"
#include <fstream>
#include <cstdio>
using namespace ae;
int main(int argc,char **argv) {
  if(argc!=2){std::fprintf(stderr,"usage: aether_scene_gui_host document.aeui\n");return 1;}
  ui::GuiDocument authored;std::string error;std::ifstream input(argv[1]);
  if(!authored.read(input,error)||authored.nodes().empty()){std::fprintf(stderr,"%s\n",error.c_str());return 2;}
  const auto asset=resources::assetGuidFromSeed("standalone-ui");runtime::SceneGraph scene;
  for(u32 j=0;j<2;++j) {
    auto id=scene.createEntity(scene.root(),runtime::ObjectKind::Folder,j?"WorldPanel":"HUD");auto node=*scene.find(id);
    auto *canvas=static_cast<scene::UiCanvas*>(node.components.add(scene::UiCanvas::descriptor));canvas->document=asset;canvas->mode=j;
    node.transform.position[2]=5;if(!scene.applyEntityValues(id,node))return 3;
  }
  runtime::GameWorld world;if(!world.load(scene))return 4;runtime::SceneGui gui;u32 loads=0;
  gui.configure([&](resources::AssetGuid id,ui::GuiDocument &out,std::string &){++loads;out=authored;return id==asset;});
  const float eye[3]{};const auto camera=renderer::buildPerspectiveFrustum(eye,0,0,1.5f);
  gui.advance(world,1.0/60);gui.prepare(world,camera,{0,0,900,600},{0,0,900,600});
  if(gui.instances().size()!=2||loads!=1||!gui.diagnostic().empty()){std::fprintf(stderr,"%s\n",gui.diagnostic().c_str());return 5;}
  auto &first=*gui.instances()[0];auto &second=*gui.instances()[1];const auto id=authored.nodes().front().id;
  first.runtime.setText(id,"only HUD");if(second.runtime.document().find(id)->text!=authored.find(id)->text)return 6;
  ui::UiDrawList draw;draw.begin({0,0,900,600},ui::fallbackFontMetrics());gui.draw(draw,ui::fallbackFontMetrics());
  ui::UiFont font;ui::UiIconAtlas icons;std::vector<ui::UiInstance> instances;
  const auto emitted=ui::buildUiInstances(draw,font,icons,16384,instances);if(!emitted.emitted||emitted.dropped)return 7;
  std::printf("PASS editor-free SceneGui host: %zu independent instances, %u immutable asset read, %u render instances\n",gui.instances().size(),loads,emitted.emitted);return 0;
}
