#include "editor/editor_archive.h"
#include "editor/editor_properties.h"
#include "platform/atomic_asset_file.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace ae::editor {
namespace {
constexpr usize kMaximumArchiveBytes = 32 * 1024 * 1024;
struct ReadState { std::string_view text; usize offset=0; };
int readBytes(void *context, void *buffer, size_t capacity) {
  auto &state=*static_cast<ReadState *>(context);
  const usize size=std::min(capacity,state.text.size()-state.offset);
  std::memcpy(buffer,state.text.data()+state.offset,size);state.offset+=size;
  return static_cast<int>(size);
}
}
std::string serializeEditorDocument(const EditorDocument &document, u64 fingerprint) {
  std::ostringstream stream;stream.imbue(std::locale::classic());
  stream << "AETHER_EDITOR 6 " << fingerprint << ' ' << document.entityCount() << '\n';
  stream << std::setprecision(std::numeric_limits<float>::max_digits10);
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto id:ids) {
    const auto &e=*document.find(id);
    stream << e.id << ' ' << e.parent << ' ' << static_cast<u32>(e.kind) << ' ' << std::quoted(e.name);
    for(float v:e.transform.position) stream << ' ' << v;
    for(float v:e.transform.rotationDegrees) stream << ' ' << v;
    for(float v:e.transform.scale) stream << ' ' << v;
    stream << ' ' << e.active << ' ' << e.visible << ' ' << e.castShadow << ' ' << e.receiveShadow
           << ' ' << e.isStatic << ' ' << e.layer << ' ' << e.assetId << ' ' << e.material.enabled;
    for(u32 i=9;i<editorNumericProperties.size();++i) stream << ' ' << editorPropertyValue(e,i);
    stream << ' ' << e.waterEnabled << ' ' << e.waterSpectrumEnabled << ' ' << e.waterLayoutEnabled
           << ' ' << e.route.count << ' ' << e.waterPhysicsEnabled << ' ' << e.rigidBodyEnabled << ' ' << e.waterInfinite << '\n';
  }
  return stream.str();
}
bool deserializeEditorDocument(std::string_view text,u64 fingerprint,EditorDocument &document) {
  if(text.size()>kMaximumArchiveBytes) return false;
  std::istringstream stream{std::string(text)};stream.imbue(std::locale::classic());
  std::string magic;u32 version=0,count=0;u64 stored=0;
  if(!(stream>>magic>>version>>stored>>count) || magic!="AETHER_EDITOR" || (version<1 || version>6) ||
     stored!=fingerprint || count==0 || count>EditorDocument::kMaximumEntities) return false;
  EditorDocument prepared;
  for(u32 index=0;index<count;++index) {
    EditorEntity e;u32 kind=0;std::string name;
    if(!(stream>>e.id>>e.parent>>kind>>std::quoted(name)) || name.empty() ||
       name.size()>=kEditorNameCapacity || name.find('\0')!=std::string::npos ||
       e.id==0 || e.id>EditorDocument::kMaximumEntities || kind>static_cast<u32>(EditorEntityKind::Effect)) return false;
    e.kind=static_cast<EditorEntityKind>(kind);assignEntityName(e,name);
    for(float &v:e.transform.position) if(!(stream>>v)) return false;
    for(float &v:e.transform.rotationDegrees) if(!(stream>>v)) return false;
    for(float &v:e.transform.scale) if(!(stream>>v)) return false;
    if(!(stream>>e.active>>e.visible>>e.castShadow>>e.receiveShadow>>e.isStatic>>e.layer>>e.assetId) ||
       !isTransformValid(e.transform)) return false;
    if(version>=2) {
      bool enabled=false;if(!(stream>>enabled)) return false;
      const u32 propertyCount=version>=6?editorNumericProperties.size():version==5?67:version==4?58:version==3?37:24;
      for(u32 i=9;i<propertyCount;++i) {
        float value;if(!(stream>>value) || !setEditorPropertyValue(e,i,value)) return false;
      }
      e.material.enabled=enabled;
      if(version>=3 && !(stream>>e.waterEnabled)) return false;
      if(version>=4 && !(stream>>e.waterSpectrumEnabled)) return false;
      if(version>=5 && !(stream>>e.waterLayoutEnabled)) return false;
      if(version>=6 && !(stream>>e.route.count>>e.waterPhysicsEnabled>>e.rigidBodyEnabled>>e.waterInfinite)) return false;
    }
    if(index==0) {
      if(e.id!=prepared.root() || e.parent!=0 || e.kind!=EditorEntityKind::Folder ||
         !prepared.applyEntityValues(e.id,e)) return false;
    } else if(!prepared.restoreEntity(e,static_cast<u32>(prepared.childrenOf(e.parent).size()))) return false;
  }
  stream>>std::ws;if(!stream.eof()) return false;
  document=std::move(prepared);return true;
}
bool saveEditorDocument(const char *path,const EditorDocument &document,u64 fingerprint) {
  const auto text=serializeEditorDocument(document,fingerprint);
  EditorDocument check;if(!deserializeEditorDocument(text,fingerprint,check)) return false;
  ReadState state{text};return platform::replaceAssetFile(path,text.size(),readBytes,&state);
}
bool loadEditorDocument(const char *path,u64 fingerprint,EditorDocument &document) {
  if(!path) return false;
  FILE *file=std::fopen(path,"rb");if(!file) return false;
  std::string text;char bytes[4096];bool ok=true;
  for(;;) {
    const usize n=std::fread(bytes,1,sizeof(bytes),file);
    if(text.size()+n>kMaximumArchiveBytes){ok=false;break;}
    text.append(bytes,n);if(n<sizeof(bytes)){ok=std::ferror(file)==0;break;}
  }
  std::fclose(file);
  return ok && deserializeEditorDocument(text,fingerprint,document);
}
}
