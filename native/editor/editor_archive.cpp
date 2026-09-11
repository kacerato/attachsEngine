#include "scene/script_behavior.h"
#include "editor/editor_route_component.h"
#include "editor/editor_archive.h"
#include "editor/editor_properties.h"
#include "editor/editor_component_catalog.h"
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
// Historical wire limits must not grow when current Inspector metadata grows.
constexpr u32 kV6V7PropertyCount=207;
constexpr u32 kV8ScalarPropertyCount=79;
struct ReadState { std::string_view text; usize offset=0; };
int readBytes(void *context, void *buffer, size_t capacity) {
  auto &state=*static_cast<ReadState *>(context);
  const usize size=std::min(capacity,state.text.size()-state.offset);
  std::memcpy(buffer,state.text.data()+state.offset,size);state.offset+=size;
  return static_cast<int>(size);
}
}
EditorComponentRegistry defaultEditorComponentRegistry() {
  static const auto types=[] {
    std::array<const EditorComponentType*,editorComponentCatalog.size()+4> result{};
    result[0]=&EditorRouteComponent::descriptor;result[1]=&EditorWaterBodyComponent::descriptor;
    for(usize i=0;i<editorComponentCatalog.size();++i) result[i+2]=editorComponentCatalog[i].type;
    result[result.size()-2]=&scene::ScriptBehavior::descriptor;
    result.back()=&LegacyWaterSettings::descriptor;
    return result;
  }();
  return types;
}
std::string serializeEditorDocument(const EditorDocument &document, u64 fingerprint) {
  std::ostringstream stream;stream.imbue(std::locale::classic());
  stream << "AETHER_EDITOR 11 " << fingerprint << ' ' << document.entityCount() << '\n';
  stream << std::setprecision(std::numeric_limits<float>::max_digits10);
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto id:ids) {
    const auto &e=*document.find(id);
    stream << e.id << ' ' << e.parent << ' ' << static_cast<u32>(e.kind) << ' ' << std::quoted(e.name);
    for(float v:e.transform.position) stream << ' ' << v;
    for(float v:e.transform.rotationDegrees) stream << ' ' << v;
    for(float v:e.transform.scale) stream << ' ' << v;
    stream << ' ' << e.active << ' ' << e.visible << ' ' << e.castShadow << ' ' << e.receiveShadow
           << ' ' << e.isStatic << ' ' << e.layer << ' ' << 0 << ' ' << false;
    // v10 keeps historical wire slots empty; mesh/material live only in records.
    const EditorEntity defaults;
    u32 changes=0;
    for(u32 i=20;i<RoutePropertyBase;++i)
      if((i<WaterBodyPropertyBase || i>=PhysicsPropertyBase) && editorPropertyValue(e,i)!=editorPropertyValue(defaults,i)) ++changes;
    stream << ' ' << changes;
    for(u32 i=20;i<RoutePropertyBase;++i)
      if((i<WaterBodyPropertyBase || i>=PhysicsPropertyBase) && editorPropertyValue(e,i)!=editorPropertyValue(defaults,i))
        stream << ' ' << i << ' ' << editorPropertyValue(e,i);
    const auto &water=waterSettings(e);
    stream << ' ' << water.enabled << ' ' << water.spectrumEnabled << ' ' << water.layoutEnabled
           << ' ' << true << ' ' << e.rigidBodyEnabled << ' ' << false;
    // Until the historical scalar section is retired, emit exactly one wire
    // representation. The optional payload is storage, not a second authority.
    auto records=e.components;records.remove(LegacyWaterSettings::descriptor);
    if(!records.write(stream,true)) return {};
    stream << '\n';
  }
  // Seção de CENA, depois das entidades: as camadas de gameplay do projeto.
  // Um arquivo v10 simplesmente não a tem e abre com as camadas padrão, que são
  // o comportamento anterior a este recurso — todas interagindo.
  stream << "LAYERS";document.layers().write(stream);stream << '\n';
  return stream.str();
}
bool deserializeEditorDocument(std::string_view text,u64 fingerprint,EditorDocument &document,EditorComponentRegistry registry) {
  if(text.size()>kMaximumArchiveBytes) return false;
  std::istringstream stream{std::string(text)};stream.imbue(std::locale::classic());
  std::string magic;u32 version=0,count=0;u64 stored=0;
  if(!(stream>>magic>>version>>stored>>count) || magic!="AETHER_EDITOR" || (version<1 || version>11) ||
     stored!=fingerprint || count==0 || count>EditorDocument::kMaximumEntities) return false;
  EditorDocument prepared;
  for(u32 index=0;index<count;++index) {
    EditorEntity e,legacyAppearance;u32 kind=0,legacyAsset=0;bool legacyMaterialEnabled=false;std::string name;
    if(!(stream>>e.id>>e.parent>>kind>>std::quoted(name)) || name.empty() ||
       name.size()>=kEditorNameCapacity || name.find('\0')!=std::string::npos ||
       e.id==0 || e.id>EditorDocument::kMaximumEntities || kind>static_cast<u32>(EditorEntityKind::Effect)) return false;
    e.kind=static_cast<EditorEntityKind>(kind);assignEntityName(e,name);
    for(float &v:e.transform.position) if(!(stream>>v)) return false;
    for(float &v:e.transform.rotationDegrees) if(!(stream>>v)) return false;
    for(float &v:e.transform.scale) if(!(stream>>v)) return false;
    if(!(stream>>e.active>>e.visible>>e.castShadow>>e.receiveShadow>>e.isStatic>>e.layer>>legacyAsset) ||
       !isTransformValid(e.transform)) return false;
    if(version>=2) {
      bool enabled=false;if(!(stream>>enabled)) return false;
      if(version>=7) {
        u32 changes=0;
        const u32 propertyLimit=version>=8?kV8ScalarPropertyCount:kV6V7PropertyCount;
        if(!(stream>>changes) || changes>propertyLimit-9) return false;
        std::array<bool,editorNumericProperties.size()> seen{};
        for(u32 entry=0;entry<changes;++entry) {
          u32 property=0;float value=0;
          if(!(stream>>property>>value) || property<9 || property>=propertyLimit ||
             seen[property] || (version>=10 && property<20) || !setEditorPropertyValue(property<20?legacyAppearance:e,property,value)) return false;
          seen[property]=true;
        }
      } else {
      const u32 propertyCount=version>=6?kV6V7PropertyCount:version==5?67:version==4?58:version==3?37:24;
      for(u32 i=9;i<propertyCount;++i) {
        float value;if(!(stream>>value) || !setEditorPropertyValue(i<20?legacyAppearance:e,i,value)) return false;
      }
      }
      legacyMaterialEnabled=enabled;
      bool waterEnabled=false,spectrumEnabled=false,layoutEnabled=false;
      if(version>=3 && !(stream>>waterEnabled)) return false;
      if(version>=4 && !(stream>>spectrumEnabled)) return false;
      if(version>=5 && !(stream>>layoutEnabled)) return false;
      if(waterEnabled||spectrumEnabled||layoutEnabled||!waterSettings(e).defaults()) {
        auto *water=editWaterSettings(e);if(!water) return false;
        water->enabled=waterEnabled;water->spectrumEnabled=spectrumEnabled;water->layoutEnabled=layoutEnabled;
      }
      pruneDefaultWaterSettings(e);
      if(version>=8) {
        bool physicsEnabled=true,infinite=false;
        if(!(stream>>physicsEnabled>>e.rigidBodyEnabled>>infinite) || !setWaterBodyFlags(e,physicsEnabled,infinite)) return false;
        // Old v8 records contain body fields in the scalar section. New writers
        // emit only defaults there and put the authored data in a component.
        EditorComponents records;
        if(!records.read(stream,registry,scene::UnknownComponentPolicy::Preserve,version>=9) || !records.merge(e.components)) return false;
        e.components=std::move(records);
      } else if(version>=6) {
        u32 routeCount=0;
        bool physicsEnabled=true,infinite=false;
        if(!(stream>>routeCount>>physicsEnabled>>e.rigidBodyEnabled>>infinite) || !setWaterBodyFlags(e,physicsEnabled,infinite)) return false;
        if(routeCount || hasWaterRoute(e)) {
          auto *route=editWaterRoute(e);if(!route) return false;
          route->count=routeCount;
        }
      }
    }
    if(version>=10) {
      // Reject a second representation instead of guessing which one wins.
      if(legacyAsset || legacyMaterialEnabled) return false;
    } else {
      const scene::MeshRenderer defaults;const auto *appearance=meshRenderer(legacyAppearance);
      const bool hasMaterialValues=appearance && std::any_of(scene::meshRendererNumbers.begin(),scene::meshRendererNumbers.end(),
          [&](const auto &property){return property.read(*appearance)!=property.read(defaults);});
      if(legacyAsset || legacyMaterialEnabled || hasMaterialValues) {
        if(e.components.find(scene::MeshRenderer::descriptor)) return false;
        auto *mesh=editMeshRenderer(e);if(!mesh) return false;
        mesh->mesh=legacyAsset;mesh->material=meshMaterial(legacyAppearance);
        mesh->material.enabled=legacyMaterialEnabled;
      }
      if(e.kind==EditorEntityKind::Camera && !cameraComponent(e) && !editCamera(e)) return false;
    }
    if(!e.components.registeredWith(registry)) return false;
    if(index==0) {
      if(e.id!=prepared.root() || e.parent!=0 || e.kind!=EditorEntityKind::Folder ||
         !prepared.applyEntityValues(e.id,e)) return false;
    } else if(!prepared.restoreEntity(e,static_cast<u32>(prepared.childrenOf(e.parent).size()))) return false;
  }
  if(version>=11) {
    std::string section;runtime::GameplayLayers layers;
    if(!(stream>>section) || section!="LAYERS" || !layers.read(stream)) return false;
    prepared.setLayers(layers);
  }
  stream>>std::ws;if(!stream.eof()) return false;
  document=std::move(prepared);return true;
}
bool saveEditorDocument(const char *path,const EditorDocument &document,u64 fingerprint,EditorComponentRegistry registry) {
  const auto text=serializeEditorDocument(document,fingerprint);
  EditorDocument check;if(!deserializeEditorDocument(text,fingerprint,check,registry)) return false;
  ReadState state{text};return platform::replaceAssetFile(path,text.size(),readBytes,&state);
}
bool loadEditorDocument(const char *path,u64 fingerprint,EditorDocument &document,EditorComponentRegistry registry) {
  if(!path) return false;
  FILE *file=std::fopen(path,"rb");if(!file) return false;
  std::string text;char bytes[4096];bool ok=true;
  for(;;) {
    const usize n=std::fread(bytes,1,sizeof(bytes),file);
    if(text.size()+n>kMaximumArchiveBytes){ok=false;break;}
    text.append(bytes,n);if(n<sizeof(bytes)){ok=std::ferror(file)==0;break;}
  }
  std::fclose(file);
  return ok && deserializeEditorDocument(text,fingerprint,document,registry);
}
}
