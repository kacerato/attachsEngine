#include "editor/editor_session.h"
#include "editor/editor_archive.h"
#include "editor/editor_import_transaction.h"
#include "scene/script_behavior.h"
#include "scene/camera.h"
#include "resources/gltf_import.h"
#include "ui_software_raster.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
using namespace ae;
static std::vector<u8> read(const std::filesystem::path &path) {
  std::ifstream s(path,std::ios::binary|std::ios::ate);if(!s)return{};
  const auto size=s.tellg();if(size<0)return{};std::vector<u8> out(static_cast<usize>(size));s.seekg(0);
  s.read(reinterpret_cast<char*>(out.data()),size);return out;
}
int main(int argc,char **argv) {
  if(argc<2)return 1;
  const std::filesystem::path root=AETHER_REPOSITORY_ROOT;
  if(std::string_view(argv[1])=="verify-images") {
    if(argc!=4)return 25;
    const auto project=std::filesystem::absolute(argv[2]);
    editor::EditorSession session;
    if(!session.setProjectDirectory(project.generic_string().c_str()))return 26;
    usize count=0;
    for(const auto &file:std::filesystem::directory_iterator(project/argv[3])) {
      if(file.path().extension()!=".png")continue;
      auto &doc=session.gui().document();doc=ui::GuiDocument{};
      const auto id=doc.create(ui::GuiKind::Image);auto node=*doc.find(id);
      node.image=file.path().lexically_relative(project).generic_string();std::string error;
      if(!doc.update(node,error))return 27;
      session.refreshGuiImages();const auto *image=session.guiImages().find(node.image);
      if(!image || !image->error.empty() || !image->width || !image->height) {
        std::fprintf(stderr,"IMAGE %s: %s\n",node.image.c_str(),image?image->error.c_str():"not loaded");return 28;
      }
      ++count;
    }
    std::printf("PASS native image directory: decoded=%zu\n",count);return count?0:29;
  }
  if(std::string_view(argv[1])=="verify-library") {
    if(argc!=3)return 19;
    const auto project=std::filesystem::absolute(argv[2]);
    editor::EditorSession session;
    if(!session.setProjectDirectory(project.generic_string().c_str()))return 20;
    usize pages=0,images=0,models=0;
    for(const auto &file:std::filesystem::directory_iterator(project/"UI")) {
      if(!file.path().filename().string().starts_with("page-"))continue;
      std::ifstream input(file.path());std::string error;
      if(!session.gui().document().read(input,error)) {std::fprintf(stderr,"%s: %s\n",file.path().string().c_str(),error.c_str());return 21;}
      session.refreshGuiImages();
      for(const auto &[path,image]:session.guiImages().entries()) {
        if(!image.error.empty() || !image.width || !image.height) {std::fprintf(stderr,"IMAGE %s: %s\n",path.c_str(),image.error.c_str());return 22;}
        ++images;
      }
      ++pages;
    }
    std::printf("Native pages: %zu; decoded images: %zu\n",pages,images);std::fflush(stdout);
    for(const auto &file:std::filesystem::directory_iterator(project/"models/synty")) {
      if(file.path().extension()!=".glb")continue;
      resources::GltfImport model;
      if(!resources::importGlb(read(file.path()),{}, {},model) || model.draws.empty() || model.textures.empty() || model.skippedTextures) {
        std::fprintf(stderr,"GLB %s: %s omitted=%u\n",file.path().string().c_str(),model.diagnostic.c_str(),model.skippedTextures);return 23;
      }
      ++models;
      if(models%50==0) {std::printf("Native textured models: %zu\n",models);std::fflush(stdout);}
    }
    std::printf("PASS native library: pages=%zu decoded_images=%zu textured_models=%zu\n",pages,images,models);
    return pages==22 && images==520 && models==520?0:24;
  }
  if(std::string_view(argv[1])=="verify-storage") {
    if(argc!=3)return 13;
    const auto project=std::filesystem::absolute(argv[2]).generic_string();
    editor::EditorSession session;
    if(!session.setProjectDirectory(project.c_str()) || session.gui().document().nodes().size()!=6)return 14;
    auto node=*session.gui().document().find(session.gui().document().findByName("start"));node.text="Persistiu";
    std::string error;
    if(!session.gui().document().update(node,error) || !session.gui().setResource("UI/custom.aeui") || !session.gui().save())return 15;
    editor::EditorSession reopened;
    if(!reopened.setProjectDirectory(project.c_str()))return 16;
    const auto *loaded=reopened.gui().document().find(reopened.gui().document().findByName("start"));
    if(!loaded || loaded->text!="Persistiu" || (reopened.gui().setResource("../outside.aeui") && reopened.gui().save()))return 17;
    std::printf("PASS real EditorSession storage: custom resource, save, project reopen, unsafe path rejection\n");return 0;
  }
  if(std::string_view(argv[1])=="write-project") {
    if(argc<3 || argc>4)return 6;
    const auto out=std::filesystem::absolute(argv[2]);std::error_code ec;
    if(std::filesystem::exists(out,ec)) {std::fprintf(stderr,"Use a new output directory\n");return 7;}
    for(const auto *folder:{"Scripts","scenes","UI","images"}) std::filesystem::create_directories(out/folder,ec);
    if(ec)return 8;
    editor::EditorDocument doc;
    const bool states=argc>3 && std::string_view(argv[3])=="states";
    const bool behavior=states || (argc>3 && std::string_view(argv[3])=="behavior");
    const char *source=states?"Scripts/GuiStatesActions.cs":behavior?"Scripts/GuiImageActions.cs":"Scripts/GuiMenu.cs";
    const auto id=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"GuiMenu");auto object=*doc.find(id);
    auto *script=static_cast<scene::ScriptBehavior *>(object.components.add(scene::ScriptBehavior::descriptor));
    if(!script)return 9;
    script->scriptType=states?"example.gui.states-actions":behavior?"example.gui.image-actions":"example.gui.menu";
    script->source=source;
    if(!doc.applyEntityValues(id,object))return 10;
    if(argc>3 && std::string_view(argv[3])=="expansion") {
      const auto camera=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"UICamera");auto entity=*doc.find(camera);entity.transform.position[2]=-6;
      if(!entity.components.add(scene::Camera::descriptor) || !doc.applyEntityValues(camera,entity))return 18;
    }
    const auto scene=editor::serializeEditorDocument(doc,0);editor::EditorDocument reopened;
    if(!editor::deserializeEditorDocument(scene,0,reopened))return 11;
    std::ostringstream descriptor;
    descriptor<<"{\"format\":\"ASTRA-PROJECT-1\",\"resourceSource\":\"independent\",\"project\":{\"name\":"<<std::quoted(out.filename().string())<<",\"template\":\"empty\",\"scenes\":1,\"assets\":0},\"mainScene\":\"scenes/editor.aescene\",\"editorScene\":\"scenes/editor.aescene\"}";
    if(!editor::EditorImportTransaction::writeText(out/"project.json",descriptor.str()) ||
       !editor::EditorImportTransaction::writeText(out/"scenes/editor.aescene",scene) ||
       !editor::EditorImportTransaction::write(out/source,read(root/"examples/ui"/std::filesystem::path(source).filename())) ||
       !editor::EditorImportTransaction::write(out/"UI/main.aeui",read(root/(states?"examples/ui/states-actions.aeui":behavior?"examples/ui/behavior.aeui":"examples/ui/main.aeui")))) return 12;
    if(behavior && !editor::EditorImportTransaction::write(out/"images/banner.png",read(root/"examples/ui/images/banner.png")))return 12;
    std::printf("GUI project written: %s\n",out.generic_string().c_str());return 0;
  }
  const u32 width=argc>2?static_cast<u32>(std::atoi(argv[2])):1280,height=argc>3?static_cast<u32>(std::atoi(argv[3])):720;
  const auto fontBytes=read(root/"assets/astra-visual/ui/astra-ui-font.aeuf");
  const auto iconBytes=read(root/"assets/astra-visual/ui/astra-ui-icons.aeui");
  ui::UiFont font;ui::UiIconAtlas icons;
  if(!font.load(fontBytes) || !icons.load(iconBytes)) return 2;
  editor::EditorSession session;session.initialize(&font,&icons);session.setSurface({0,0,float(width),float(height)},{});session.openGui();
  if(!session.gui().immediate().setFont(read(root/"assets/astra-visual/ui/gui-inter.ttf"))) return 5;
  if(argc>4) {
    const auto folder=std::filesystem::path(argv[4]).parent_path();
    const auto project=folder.filename()=="UI"?folder.parent_path():folder;
    if(!session.setProjectDirectory(project.string().c_str()))return 14;
    std::ifstream input(argv[4]);std::string error;
    if(!session.gui().document().read(input,error)) {std::fprintf(stderr,"%s\n",error.c_str());return 3;}
    session.gui().select(session.gui().document().findByName(argc>6?argv[6]:"volume"));
  }
  if(argc>5 && std::string_view(argv[5])=="preview") session.gui().setPreview(true);
  session.update();session.update();
  test::UiSoftwareTarget target;target.resize(width,height,0.1f,0.1f,0.1f);
  const auto &im=session.immediateGui();
  const auto &guiImages=session.guiImages();
  test::rasterizeUi(session.instances(),font,icons,target,im.atlas(),im.atlasWidth(),im.atlasHeight(),guiImages.pixels(),guiImages.size());
  std::ofstream output(argv[1],std::ios::binary);output<<"P6\n"<<width<<' '<<height<<"\n255\n";
  for(usize i=0;i<target.pixels.size();i+=4) for(usize c=0;c<3;++c) output.put(static_cast<char>(std::clamp(target.pixels[i+c],0.0f,1.0f)*255+0.5f));
  std::printf("%ux%u elements=%u instances=%u rejected_imgui=%u canvas=%.0fx%.0f\n",width,height,
    static_cast<u32>(session.gui().document().nodes().size()),static_cast<u32>(session.instances().size()),im.rejectedCommands(),
    session.gui().canvas().width,session.gui().canvas().height);
  return output?0:4;
}
