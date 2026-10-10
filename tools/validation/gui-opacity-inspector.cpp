// Executable UI inspection: product GuiWorkbench, actual pointer routing and
// native draw instances. Raster output is host evidence, never GPU/device proof.
#include "ui/gui_workbench.h"
#include "ui/ui_instance_builder.h"
#include "ui_software_raster.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cstdlib>
using namespace ae;
namespace {
std::vector<u8> read(const std::filesystem::path &path) {
  std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};
}
}
int main(int argc,char **argv) {
  if(argc!=3&&argc!=5)return 1;
  const auto root=std::filesystem::absolute(argv[1]),folder=std::filesystem::absolute(argv[2]);
  std::filesystem::create_directories(folder);
  ui::UiFont font;ui::UiIconAtlas icons;
  const auto fontBytes=read(root/"assets/astra-visual/ui/astra-ui-font.aeuf");
  const auto iconBytes=read(root/"assets/astra-visual/ui/astra-ui-icons.aeui");
  if(!font.load(fontBytes)||!icons.load(iconBytes))return 2;
  ui::GuiWorkbench workbench;ui::UiDrawList list;
  if(!workbench.immediate().setFont(read(root/"assets/astra-visual/ui/gui-inter.ttf")))return 3;
  const auto group=workbench.create(ui::GuiKind::Panel);workbench.select(group);
  const ui::UiRect surface{0,0,655,300};
  const auto frame=[&] {
    list.begin(surface,font.metrics(ui::UiFontWeight::Regular));workbench.drawInspector(surface,surface,list,"Canvas",1,1.f/60);
  };
  const auto capture=[&](const char *name) {
    std::vector<ui::UiInstance> instances;ui::buildUiInstances(list,font,icons,65536,instances);
    test::UiSoftwareTarget target;target.resize(655,300,.1f,.1f,.1f);
    const auto &im=workbench.immediate();
    test::rasterizeUi(instances,font,icons,target,im.atlas(),im.atlasWidth(),im.atlasHeight());
    std::ofstream out(folder/(std::string(name)+".ppm"),std::ios::binary);out<<"P6\n655 300\n255\n";
    for(usize i=0;i<target.pixels.size();i+=4)for(usize c=0;c<3;++c)
      out.put(static_cast<char>(std::clamp(target.pixels[i+c],0.f,1.f)*255+.5f));
    return bool(out);
  };
  frame();frame();if(!capture("gui-opacity-before"))return 4;
  if(argc==5) {
    const ui::UiPoint point{std::strtof(argv[3],nullptr),std::strtof(argv[4],nullptr)};
    if(!workbench.pointer({71,ui::UiPointerPhase::Down,point}))return 5;
    frame();
    if(!workbench.pointer({71,ui::UiPointerPhase::Up,point}))return 6;
    frame();frame();
    const float opacity=workbench.document().find(group)->opacity;
    if(!(opacity>0&&opacity<1)||!workbench.history().canUndo())return 7;
    if(!capture("gui-opacity-after"))return 8;
    std::ofstream archive(folder/"gui-opacity-authored.aeui");workbench.document().write(archive);archive.close();
    ui::GuiDocument reopened;std::string error;std::ifstream input(folder/"gui-opacity-authored.aeui");
    if(!reopened.read(input,error)||reopened.find(group)->opacity!=opacity)return 9;
    if(!workbench.history().undo(workbench.document())||workbench.document().find(group)->opacity!=1||
       !workbench.history().redo(workbench.document())||workbench.document().find(group)->opacity!=opacity)return 10;
    std::printf("PASS opacity touch %.2f -> archive -> cold reopen -> Undo/Redo at 655x300\n",opacity);
  }
  return workbench.immediate().rejectedCommands()?11:0;
}
