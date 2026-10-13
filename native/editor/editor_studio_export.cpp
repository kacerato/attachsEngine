// N06: exportacao do clipe aberto como GLB (esqueleto + animacao) com
// relatorio de perdas e prova de reimportacao: o arquivo escrito e lido de
// volta pelo importador da engine e comparado com o clipe original em tempos
// intermediarios, independentes das chaves exportadas.
#include "editor/editor_studio_tools.h"
#include "editor/editor_session.h"
#include "editor/editor_studio_state.h"
#include "editor/editor_import_transaction.h"
#include "resources/gltf_export.h"
#include "resources/gltf_import.h"
#include "runtime/transform_math.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>

namespace ae::editor {
namespace {
void trs(const runtime::Transform &t,float out[10]) {
  float q[4];runtime::transformRotationQuaternion(t,q);
  out[0]=t.position[0];out[1]=t.position[1];out[2]=t.position[2];
  out[3]=q[0];out[4]=q[1];out[5]=q[2];out[6]=q[3];out[7]=t.scale[0];out[8]=t.scale[1];out[9]=t.scale[2];
}
} // namespace

bool EditorStudio::exportGlb(EditorSession &s,std::string &summary,std::string &error) {
  summary.clear();error.clear();
  s.refreshAnimationClip();
  const auto *live=s.animationClipAsset(s.state_.clipGuid);
  if(!live||!s.clipPreview_.active()||s.clipPoseDraft_||s.isPlaying()) {error="Abra o clipe e grave a pose pendente";return false;}
  const auto owner=s.state_.clipOwner;const auto &preview=s.clipPreview_.scene();
  if(!preview.find(owner)) {error="Personagem ausente";return false;}
  std::vector<EditorEntityId> objects;preview.collectSubtree(owner,objects);
  if(objects.size()>8192) {error="Hierarquia grande demais para exportar";return false;}
  // Nos: o personagem e a raiz; pais sempre antes dos filhos.
  std::vector<resources::GltfExportNode> nodes;
  for(usize i=0;i<objects.size();++i) {
    const auto *o=preview.find(objects[i]);resources::GltfExportNode n;n.name=o->name;
    if(i)for(usize p=0;p<i;++p)if(objects[p]==o->parent)n.parent=static_cast<i32>(p);
    float v[10];trs(s.document_.find(objects[i])?s.document_.find(objects[i])->transform:o->transform,v);
    std::copy(v,v+3,n.translation);std::copy(v+3,v+7,n.rotation);std::copy(v+7,v+10,n.scale);
    nodes.push_back(n);
  }
  // Amostragem pelo preview (camadas, mute/solo e referencias ja compostos).
  const float rate=static_cast<float>(std::clamp<u32>(live->displayRate,1,120));
  const u32 frames=std::max(2u,static_cast<u32>(std::ceil(live->duration*rate))+1);
  std::vector<float> times;for(u32 i=0;i<frames;++i)times.push_back(live->duration*float(i)/float(frames-1));
  std::vector<std::vector<std::array<float,10>>> samples(objects.size());
  const float now=s.state_.clipTime;std::string seek;
  for(const float t:times) {
    if(!s.clipPreview_.seek(t,seek)) {s.clipPreview_.seek(now,seek);error="Amostragem falhou: "+seek;return false;}
    for(usize i=0;i<objects.size();++i){std::array<float,10> v{};trs(s.clipPreview_.scene().find(objects[i])->transform,v.data());samples[i].push_back(v);}
  }
  resources::GltfExportAnimation animation;animation.name=live->name;
  u32 channels=0;
  for(usize i=0;i<objects.size();++i)for(const auto path:{resources::AnimationPath::Translation,resources::AnimationPath::Rotation,resources::AnimationPath::Scale}) {
    const u32 first=path==resources::AnimationPath::Translation?0:path==resources::AnimationPath::Rotation?3:7,width=path==resources::AnimationPath::Rotation?4:3;
    bool varies=false;
    for(const auto &v:samples[i])for(u32 k=0;k<width;++k)varies=varies||std::abs(v[first+k]-samples[i][0][first+k])>1e-6f;
    if(!varies)continue;
    resources::GltfExportChannel c;c.node=static_cast<u32>(i);c.path=path;c.times=times;
    std::array<float,4> previous{};
    for(usize f=0;f<samples[i].size();++f) {
      std::array<float,4> v{};for(u32 k=0;k<width;++k)v[k]=samples[i][f][first+k];
      // Quaternions no mesmo hemisferio: o LINEAR do glTF faz slerp pelo arco curto.
      if(width==4&&f&&v[0]*previous[0]+v[1]*previous[1]+v[2]*previous[2]+v[3]*previous[3]<0)for(auto &x:v)x=-x;
      previous=v;c.values.insert(c.values.end(),v.begin(),v.begin()+width);
    }
    animation.channels.push_back(std::move(c));++channels;
  }
  std::vector<u8> bytes;
  if(!resources::writeGlb(nodes,{animation},bytes,error)) {s.clipPreview_.seek(now,seek);return false;}
  // Prova de reimportacao, comparada ao clipe em tempos intermediarios.
  resources::GltfImport imported;
  resources::GltfImportLimits limits;limits.allowAnimationOnly=true;
  if(!resources::importGlb(bytes,limits,{},imported)||imported.animations.empty()) {s.clipPreview_.seek(now,seek);error="Reimportação recusou o arquivo: "+imported.diagnostic;return false;}
  float maxPosition=0,maxAngle=0;u32 compared=0;
  const auto &clip=imported.animations.front();
  for(u32 f=0;f+1<frames;++f) {
    const float t=(times[f]+times[f+1])*.5f;
    if(!s.clipPreview_.seek(t,seek))break;
    for(const auto &c:clip.channels) {
      if(c.node>=imported.nodes.size())continue;
      const auto found=std::find_if(objects.begin(),objects.end(),[&](EditorEntityId id){return s.clipPreview_.scene().find(id)->name==imported.nodes[c.node].name;});
      if(found==objects.end())continue;
      float expected[10];trs(s.clipPreview_.scene().find(*found)->transform,expected);
      float got[4]{};if(!resources::sampleAnimationChannel(c,t,got))continue;
      if(c.path==resources::AnimationPath::Rotation) {
        const float d=std::abs(got[0]*expected[3]+got[1]*expected[4]+got[2]*expected[5]+got[3]*expected[6]);
        maxAngle=std::max(maxAngle,2*std::acos(std::clamp(d,0.f,1.f))*57.2957795f);
      } else {
        const u32 first=c.path==resources::AnimationPath::Translation?0:7;
        maxPosition=std::max(maxPosition,std::hypot(got[0]-expected[first],got[1]-expected[first+1],got[2]-expected[first+2]));
      }
      ++compared;
    }
  }
  s.clipPreview_.seek(now,seek);
  // Arquivo e relatorio em Exportados/.
  std::string stem=live->name;for(auto &c:stem)if(c=='/'||c=='\\'||c==':'||c=='"'||c=='*'||c=='?'||c=='<'||c=='>'||c=='|'||static_cast<unsigned char>(c)<32)c='_';
  const auto root=EditorImportTransaction::fromUtf8(s.files_.rootPath());
  std::error_code ec;std::filesystem::create_directories(root/"Exportados",ec);
  const auto glbPath=root/"Exportados"/EditorImportTransaction::fromUtf8(stem+".glb");
  {std::ofstream file(glbPath,std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
   if(!file) {error="Não foi possível gravar Exportados/"+stem+".glb";return false;}}
  std::string lost;
  if(!live->cues.empty())lost+=std::to_string(live->cues.size())+" eventos (cues); ";
  if(!live->regions.empty())lost+=std::to_string(live->regions.size())+" regiões; ";
  if(!live->properties.empty())lost+=std::to_string(live->properties.size())+" trilhas de propriedade; ";
  if(!live->drivers.empty())lost+="drivers; ";
  if(live->layers.size()>1)lost+=std::to_string(live->layers.size())+" camadas compostas numa só; ";
  char report[512];
  std::snprintf(report,sizeof report,"GLB exportado · Exportados/%s.glb · %zu nós · %u canais · %u quadros a %.0f Hz · reimportação: %.2f mm / %.3f° em %u amostras intermediárias%s%s",
                stem.c_str(),nodes.size(),channels,frames,rate,maxPosition*1000,maxAngle,compared,lost.empty()?"":" · sem equivalente no glTF: ",lost.c_str());
  {std::ofstream text(root/"Exportados"/EditorImportTransaction::fromUtf8(stem+".relatorio.txt"),std::ios::trunc);
   text<<report<<"\nSaída: esqueleto (nós) e animação TRS LINEAR, metros, Y para cima, quaternions XYZW.\n"
       <<"Não incluído nesta saída: malhas, skins e materiais (exportar o modelo importado preserva a fonte original).\n";}
  summary=report;
  return true;
}
} // namespace ae::editor
