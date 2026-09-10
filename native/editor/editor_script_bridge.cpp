#include "editor/editor_script_bridge.h"
#include "editor/editor_map_scene.h"
#include "editor/editor_physics_body.h"
#include "editor/editor_character.h"
#include "scene/script_behavior.h"
#include <cmath>

namespace ae::editor {
namespace {
void jsonString(std::ostream &out,std::string_view text) {
  out<<'"';
  constexpr char hex[]="0123456789abcdef";
  for(unsigned char c:text) {
    if(c=='"'||c=='\\') out<<'\\'<<static_cast<char>(c);
    else if(c<32) out<<"\\u00"<<hex[c>>4]<<hex[c&15];
    else out<<static_cast<char>(c);
  }
  out<<'"';
}
std::string attachments(const EditorDocument &document) {
  std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(std::numeric_limits<float>::max_digits10)<<'[';
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);bool first=true;
  for(auto id:ids) {
    const auto &entity=*document.find(id);bool active=entity.active;
    for(auto parent=entity.parent;parent;parent=document.find(parent)->parent) active=active&&document.find(parent)->active;
    if(!active) continue;
    for(usize i=0;i<entity.components.size();++i) if(const auto *script=scene::scriptBehavior(entity.components.at(i))) {
      if(!first) out<<',';
      first=false;
      out<<"{\"ObjectId\":"<<id<<",\"InstanceId\":"<<script->instanceId()<<",\"TypeId\":";jsonString(out,script->scriptType);
      out<<",\"Enabled\":"<<(script->enabled?"true":"false")<<",\"Properties\":{";
      bool fieldFirst=true;
      for(const auto &p:script->properties) {
        if(!fieldFirst) out<<',';
        fieldFirst=false;jsonString(out,p.id);out<<':';
        if(p.valueType=="string") jsonString(out,p.value);
        else if(p.valueType=="asset") {out<<"{\"AssetId\":";jsonString(out,p.value);out<<'}';}
        else if(p.valueType=="object") {std::istringstream in(p.value);u64 v=0;in>>v;out<<"{\"ObjectId\":"<<v<<'}';}
        else if(p.valueType=="vector3") {
          std::istringstream in(p.value);in.imbue(std::locale::classic());float x=0,y=0,z=0;in>>x>>y>>z;
          out<<"{\"X\":"<<x<<",\"Y\":"<<y<<",\"Z\":"<<z<<'}';
        } else if(p.valueType=="float") {std::istringstream in(p.value);in.imbue(std::locale::classic());float v=0;in>>v;out<<v;}
        else if(p.valueType=="int32"||p.valueType=="enum") {std::istringstream in(p.value);i32 v=0;in>>v;out<<v;}
        else out<<p.value;
      }
      out<<"},\"PropertyTypes\":{";fieldFirst=true;
      for(const auto &p:script->properties) {if(!fieldFirst) out<<',';
        fieldFirst=false;jsonString(out,p.id);out<<':';jsonString(out,p.valueType);}
      out<<"}}";
    }
  }
  out<<']';return out.str();
}
bool transformFromAbi(const float *v,EditorTransform &transform) {
  for(u32 i=0;i<10;++i) if(!std::isfinite(v[i])) return false;
  const float norm=std::sqrt(v[3]*v[3]+v[4]*v[4]+v[5]*v[5]+v[6]*v[6]);if(norm<1e-8f) return false;
  const float x=v[3]/norm,y=v[4]/norm,z=v[5]/norm,w=v[6]/norm;
  float world[16]{(1-2*(y*y+z*z))*v[7],2*(x*y+w*z)*v[7],2*(x*z-w*y)*v[7],0,
    2*(x*y-w*z)*v[8],(1-2*(x*x+z*z))*v[8],2*(y*z+w*x)*v[8],0,
    2*(x*z+w*y)*v[9],2*(y*z-w*x)*v[9],(1-2*(x*x+y*y))*v[9],0,v[0],v[1],v[2],1};
  float identity[16]{};identity[0]=identity[5]=identity[10]=identity[15]=1;
  return editorLocalTransformForWorld(world,identity,transform);
}
}
bool EditorScriptBridge::hasScripts(const EditorDocument &document) {
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto id:ids) for(usize i=0;i<document.find(id)->components.size();++i)
    if(scene::scriptBehavior(document.find(id)->components.at(i))) return true;
  return false;
}
bool EditorScriptBridge::start(EditorDocument &document,EditorScenePhysics &physics) {
  stop();diagnostics_.clear();
  if(!hasScripts(document)) return true;
  if(!api_.available()) {diagnostics_="Runtime C# indisponível; aplique o código antes de Play";return false;}
  document_=&document;physics_=&physics;access_.context=this;
  access_.exists=[](void *c,u64 id)->int {const auto &s=*static_cast<EditorScriptBridge *>(c);return id<=std::numeric_limits<u32>::max()&&s.document_->exists(static_cast<u32>(id));};
  access_.getTransform=[](void *c,u64 id,float *out)->int {
    auto &s=*static_cast<EditorScriptBridge *>(c);if(!out||!s.access_.exists(c,id)) return 0;
    const auto &t=s.document_->find(static_cast<u32>(id))->transform;
    std::copy(t.position,t.position+3,out);std::copy(t.scale,t.scale+3,out+7);
    const float x=t.rotationDegrees[0]*.00872664626f,y=t.rotationDegrees[1]*.00872664626f,z=t.rotationDegrees[2]*.00872664626f;
    const float sx=std::sin(x),cx=std::cos(x),sy=std::sin(y),cy=std::cos(y),sz=std::sin(z),cz=std::cos(z);
    out[3]=sx*cy*cz-cx*sy*sz;out[4]=cx*sy*cz+sx*cy*sz;out[5]=cx*cy*sz-sx*sy*cz;out[6]=cx*cy*cz+sx*sy*sz;return 1;
  };
  access_.setTransform=[](void *c,u64 id,const float *value)->int {
    auto &s=*static_cast<EditorScriptBridge *>(c);if(!value||!s.access_.exists(c,id)) return 0;
    // Moving an ancestor of a physics-owned pose would desynchronize Jolt.
    // Use physical movement APIs until transform propagation into bodies exists.
    std::vector<EditorEntityId> children;s.document_->collectSubtree(static_cast<u32>(id),children);
    for(auto child:children) {const auto &e=*s.document_->find(child);if(physicsBody(e)||characterComponent(e)) return 0;
      for(usize i=0;i<e.components.size();++i) {const auto *v=e.components.at(i);if(&v->type()==&scene::Collider::descriptor&&static_cast<const scene::Collider &>(*v).enabled) return 0;}}
    EditorTransform t;return transformFromAbi(value,t)&&s.document_->setTransform(static_cast<u32>(id),t);
  };
  access_.setVelocity=[](void *c,u64 id,const float *v)->int {
    auto &s=*static_cast<EditorScriptBridge *>(c);return v&&s.access_.exists(c,id)&&s.physics_->setBodyVelocity(static_cast<u32>(id),v);
  };
  access_.moveKinematic=[](void *c,u64 id,const float *v)->int {
    auto &s=*static_cast<EditorScriptBridge *>(c);return v&&s.access_.exists(c,id)&&s.physics_->moveKinematic(static_cast<u32>(id),v);
  };
  access_.bodyForce=[](void *c,u64 id,const float *v,u32 kind)->int {
    auto &s=*static_cast<EditorScriptBridge *>(c);return v&&s.access_.exists(c,id)&&s.physics_->applyBodyForce(static_cast<u32>(id),v,kind);
  };
  access_.getVelocity=[](void *c,u64 id,float *v)->int {
    auto &s=*static_cast<EditorScriptBridge *>(c);return v&&s.access_.exists(c,id)&&s.physics_->getBodyVelocity(static_cast<u32>(id),v);
  };
  access_.log=[](void *c,u64 id,const u8 *text,int length) {
    auto &s=*static_cast<EditorScriptBridge *>(c);
    if(text&&length>0&&length<=32768) {
      const std::string_view message(reinterpret_cast<const char *>(text),static_cast<usize>(length));
      if(s.diagnostics_.size()>65536) s.diagnostics_.clear();
      s.diagnostics_+=std::to_string(id)+": "+std::string(message)+"\n";
      if(s.logSink_) s.logSink_(id,message);
    }
  };
  const auto data=attachments(document);
  if(api_.start(reinterpret_cast<const u8 *>(root_.data()),static_cast<int>(root_.size()),
      reinterpret_cast<const u8 *>(data.data()),static_cast<int>(data.size()),&access_)!=0) {
    collectDiagnostics();document_=nullptr;physics_=nullptr;return false;
  }
  running_=true;collectDiagnostics();return true;
}
void EditorScriptBridge::collectDiagnostics() {
  if(!api_.copyDiagnostics) return;
  const int size=api_.copyDiagnostics(nullptr,0);if(size<=0||size>4*1024*1024) return;
  std::string text(static_cast<usize>(size),'\0');
  if(api_.copyDiagnostics(reinterpret_cast<u8 *>(text.data()),size)==size) diagnostics_=std::move(text);
}
bool EditorScriptBridge::update(float elapsed) {if(!running_) return true;const bool ok=api_.update(elapsed)==0;collectDiagnostics();return ok;}
bool EditorScriptBridge::fixedUpdate(float elapsed) {if(!running_) return true;const bool ok=api_.fixedUpdate(elapsed)==0;collectDiagnostics();return ok;}
bool EditorScriptBridge::trigger(EditorEntityId sensor,EditorEntityId other,u32 phase) {if(!running_) return true;const bool ok=api_.trigger(sensor,other,phase)==0;collectDiagnostics();return ok;}
void EditorScriptBridge::stop() {if(running_) api_.stop();running_=false;document_=nullptr;physics_=nullptr;}
}
