// Production terrain surface + production UI instances, without loading the
// editor session, physics or image codecs. This verifies layout, not gameplay.
#include "editor/editor_screen.h"
#include "editor/editor_theme.h"
#include "ui_software_raster.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <set>
using namespace ae;
int main(int argc,char **argv) {
  if(argc<2)return 2;
  const u32 width=argc>2?u32(std::atoi(argv[2])):1280,height=argc>3?u32(std::atoi(argv[3])):720;
  if(width<320 || width>2560 || height<240 || height>1440)return 2;
  const auto read=[](const char *name){std::ifstream f(name,std::ios::binary);return std::vector<u8>(std::istreambuf_iterator<char>(f),{});};
  const auto fontBytes=read("assets/astra-visual/ui/astra-ui-font.aeuf"),iconBytes=read("assets/astra-visual/ui/astra-ui-icons.aeui");
  ui::UiFont font;ui::UiIconAtlas icons;
  if(!font.load(fontBytes) || !icons.load(iconBytes))return 3;
  editor::EditorScreenState state;state.surface={0,0,float(width),float(height)};
  state.terrainOpen=true;state.terrainTab=0;state.terrainSection=2;state.terrainDraft.lodLevels=4;
  state.terrainMessage="LOD autoral · aplique para gerar";
  const auto panel=editor::terrainPanelRect(state);const auto &theme=editor::editorTheme();
  const auto metrics=font.metrics(ui::UiFontWeight::Regular);
  ui::UiDrawList list;ui::UiInputRouter router;
  const auto render=[&](){list.begin(state.surface,metrics);router.beginFrame();editor::buildTerrainSurface(state,theme,list,router,panel);};
  std::set<u32> reachable;
  const float limit=editor::terrainScrollLimit(state,panel);
  for(float scroll=0;scroll<=limit+8;scroll+=8) {
    state.terrainScroll=std::min(scroll,limit);render();
    for(float y=panel.y;y<panel.bottom();y+=4)for(float x=panel.x;x<panel.right();x+=4)
      reachable.insert(router.hitTest({x,y}).widgetId);
  }
  const u32 base=editor::widgetId(editor::EditorWidget::TerrainNumberBase);
  if(!reachable.contains(base+28) || !reachable.contains(base+29))return 4;
  state.terrainScroll=argc>4?std::strtof(argv[4],nullptr):0;render();
  std::vector<ui::UiInstance> instances;
  const auto built=ui::buildUiInstances(list,font,icons,65536,instances);
  if(built.dropped || built.missingGlyphRuns)return 5;
  test::UiSoftwareTarget image;image.resize(width,height,.098f,.11f,.13f);
  test::rasterizeUi(instances,font,icons,image);
  std::FILE *file=std::fopen(argv[1],"wb");if(!file)return 6;
  std::fprintf(file,"P6\n%u %u\n255\n",width,height);
  std::vector<u8> rgb(usize(width)*height*3);
  for(usize p=0;p<usize(width)*height;++p)for(u32 c=0;c<3;++c)
    rgb[p*3+c]=u8(std::clamp(image.pixels[p*4+c],0.f,1.f)*255+.5f);
  const bool ok=std::fwrite(rgb.data(),1,rgb.size(),file)==rgb.size();std::fclose(file);
  std::printf("%ux%u production surface; LOD controls reachable; %u instances; scroll %.0f/%.0f\n",width,height,built.emitted,state.terrainScroll,limit);
  return ok?0:7;
}
