#include "editor/editor_animation_authoring.h"
#include "editor/editor_session.h"
#include "resources/animation_clip_clipboard.h"
#include "resources/animation_clip_bake.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <thread>
#include <sstream>
#include <iomanip>

namespace ae::editor {
namespace {
using Asset=resources::AnimationClipAsset;
using Address=resources::AnimationKeyAddress;
std::atomic<u64> nextAuthorHandle{1};
u64 allocateAuthorHandle() {
  auto candidate=nextAuthorHandle.load();
  while(candidate) {
    const auto next=candidate==std::numeric_limits<u64>::max()?u64{0}:candidate+1;
    if(nextAuthorHandle.compare_exchange_weak(candidate,next))return candidate;
  }
  return 0;
}
usize keyCount(const Asset &asset) {
  usize count=0;for(const auto &track:asset.tracks)for(const auto &curve:track.curves)count+=curve.keys.size();return count;
}
int copyText(std::string_view text,u8 *buffer,int capacity) {
  if(capacity<0||text.size()>static_cast<usize>(std::numeric_limits<int>::max()))return -1;
  if(buffer&&static_cast<usize>(capacity)>=text.size())std::copy(text.begin(),text.end(),buffer);
  return static_cast<int>(text.size());
}
bool textArgument(const u8 *text,int length,usize maximum) {return length>=0&&static_cast<usize>(length)<=maximum&&(text||!length);}
std::string textValue(const u8 *text,int length) {return length?std::string(reinterpret_cast<const char*>(text),static_cast<usize>(length)):std::string();}
bool bakeRequest(const AnimationAuthorBakeRequest *wire,resources::AnimationBakeSettings &settings) {
  if(!wire||wire->reserved||wire->reserved2||wire->eulerPolicy>1||wire->settings.reserved||(wire->settings.flags&~1u))return false;
  const auto &base=wire->settings;settings.sampleRate=base.sampleRate;settings.verificationSteps=base.verificationSteps;
  settings.maximumFrames=base.maximumFrames;settings.rotation=base.rotation;settings.reduce=base.flags&1;settings.tolerance=base.tolerance;
  settings.eulerReferenceExplicit=wire->eulerPolicy==1;std::copy_n(wire->eulerReference,3,settings.eulerReference);return true;
}
}
struct AnimationAuthoringScope::Impl {
  struct Draft {
    u64 token;Asset asset;std::string snapshot;
    resources::AnimationClip compiled{};bool runtimeReady=false;
    void changed() {snapshot.clear();compiled={};runtimeReady=false;}
  };
  struct Clipboard {u64 token;resources::AnimationKeyClipboard value;};
  EditorSession &session;
  std::string project,error;
  u64 epoch;
  std::thread::id thread;
  std::vector<Draft> drafts;
  std::vector<Clipboard> clipboards;
  std::vector<resources::AssetGuid> authoredCatalog,importedCatalog;
  bool authoredCatalogReady=false,importedCatalogReady=false;
  AnimationAuthorAccess api;
  explicit Impl(EditorSession &value):session(value),project(value.codeProjectRoot()),epoch(value.sceneVersion().epoch),thread(std::this_thread::get_id()) {
    api.context=this;
    api.selected=[](void *c)->u64 {auto &s=*static_cast<Impl*>(c);return s.ready()?s.session.selection():0;};
    api.count=[](void *c,u32 imported)->int {
      auto &s=*static_cast<Impl*>(c);if(!s.ready()||imported>1)return -1;
      const auto count=s.catalog(imported!=0).size();return s.error.empty()?static_cast<int>(count):-1;
    };
    api.at=[](void *c,u32 imported,u32 index,resources::AssetGuid *out)->int {
      auto &s=*static_cast<Impl*>(c);if(!s.ready()||imported>1||!out)return 0;
      const auto &entries=s.catalog(imported!=0);if(!s.error.empty())return 0;
      if(index>=entries.size())return s.reject("Índice de clipe inexistente");
      *out=entries[index];return 1;
    };
    api.create=[](void *c,u64 owner,float duration,u32 mode,resources::AssetGuid *out)->int {
      auto &s=*static_cast<Impl*>(c);if(!s.ready()||!out||mode>2||owner>std::numeric_limits<EditorEntityId>::max())return 0;
      const bool ok=s.session.createAnimationClip(static_cast<EditorEntityId>(owner),duration,*out,s.error,static_cast<resources::AnimationRotationMode>(mode));
      if(ok)s.authoredCatalogReady=false;
      return ok?1:0;
    };
    api.extract=[](void *c,resources::AssetGuid source,u64 owner,resources::AssetGuid *out)->int {
      auto &s=*static_cast<Impl*>(c);if(!s.ready()||!out||owner>std::numeric_limits<EditorEntityId>::max())return 0;
      const bool ok=s.session.extractAnimationClip(source,static_cast<EditorEntityId>(owner),*out,s.error);
      if(ok)s.authoredCatalogReady=false;
      return ok?1:0;
    };
    api.begin=[](void *c,resources::AssetGuid guid,u32 revision)->u64 {
      auto &s=*static_cast<Impl*>(c);if(!s.ready())return 0;
      const auto *live=s.session.animationClipAsset(guid);
      if(!live||(revision&&revision!=live->revision)) {s.reject("Clipe ausente ou revisão de autoria vencida");return 0;}
      if(s.drafts.size()>=8||!s.budget(*live,0)) {s.reject("Limite de rascunhos de animação atingido");return 0;}
      const auto token=allocateAuthorHandle();if(!token) {s.reject("Identidades de autoria esgotadas");return 0;}
      s.drafts.push_back({token,*live,{}});return token;
    };
    api.snapshot=[](void *c,u64 token,u8 *buffer,int capacity)->int {
      auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);if(!draft)return -1;
      if(draft->snapshot.empty())draft->snapshot=draft->asset.serialize();
      return copyText(draft->snapshot,buffer,capacity);
    };
    api.apply=[](void *c,u64 token,const AnimationAuthorCommand *command,const float *values,int count,const u8 *text,int length,u64 *result)->int {
      auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);
      if(!draft)return 0;
      if(!command||command->reserved||command->operation>static_cast<u32>(AnimationAuthorOperation::LayerCopyBase)||
         count<0||count>static_cast<int>(resources::MaximumMorphTargets)||(!values&&count)||!textArgument(text,length,256))return s.reject("Argumento de autoria inválido");
      auto candidate=draft->asset;u64 id=0;bool ok=false;
      const auto operation=static_cast<AnimationAuthorOperation>(command->operation);
      switch(operation) {
        case AnimationAuthorOperation::PutKey: {
          const auto &wire=command->key;
          if(wire.incoming>6||wire.outgoing>6||(wire.flags&~7u))return s.reject("Modo ou flags de tangente inválidos");
          resources::AnimationCurveKey key;key.id=wire.id;key.time=wire.time;key.value=wire.value;
          key.inSlope=wire.inSlope;key.outSlope=wire.outSlope;key.inWeight=wire.inWeight;key.outWeight=wire.outWeight;
          key.incoming=static_cast<resources::AnimationTangentMode>(wire.incoming);key.outgoing=static_cast<resources::AnimationTangentMode>(wire.outgoing);
          key.broken=wire.flags&1;key.weightedIn=wire.flags&2;key.weightedOut=wire.flags&4;
          ok=candidate.putKey(command->track,command->component,key,id,s.error);break;
        }
        case AnimationAuthorOperation::SplitKey:ok=candidate.splitKey(command->track,command->component,static_cast<float>(command->first),id,s.error);break;
        case AnimationAuthorOperation::EraseKey:ok=candidate.eraseKey(command->track,command->component,command->key.id,s.error);break;
        case AnimationAuthorOperation::PutPose:ok=candidate.putPose(command->track,static_cast<float>(command->first),{values,static_cast<usize>(count)},s.error);break;
        case AnimationAuthorOperation::Retime:ok=candidate.retime(command->first,s.error);break;
        case AnimationAuthorOperation::Reverse:ok=candidate.reverse(s.error);break;
        case AnimationAuthorOperation::Crop:ok=candidate.crop(static_cast<float>(command->first),static_cast<float>(command->second),s.error);break;
        case AnimationAuthorOperation::RotationMode:
          if(command->mode>2)return s.reject("Modo de rotação inválido");
          ok=candidate.changeRotationMode(command->track,static_cast<resources::AnimationRotationMode>(command->mode),s.error);break;
        case AnimationAuthorOperation::RemoveTrack:ok=candidate.removeTrack(command->track,s.error);break;
        case AnimationAuthorOperation::Name:candidate.name=textValue(text,length);ok=candidate.valid(&s.error);break;
        case AnimationAuthorOperation::DisplayRate:candidate.displayRate=command->mode;ok=candidate.valid(&s.error);break;
        case AnimationAuthorOperation::LayerAdd:
          if(command->mode>1)return s.reject("Mistura de camada inválida");
          ok=candidate.addLayer(textValue(text,length),static_cast<resources::AnimationAuthorBlend>(command->mode),id,s.error);break;
        case AnimationAuthorOperation::LayerConfigure:
          if(command->mode>1||(command->key.flags&~3u))return s.reject("Flags de camada inválidas");
          ok=candidate.configureLayer(command->track,textValue(text,length),static_cast<resources::AnimationAuthorBlend>(command->mode),
            static_cast<float>(command->first),static_cast<float>(command->second),command->key.flags&1,command->key.flags&2,s.error);break;
        case AnimationAuthorOperation::LayerMove:ok=candidate.moveLayer(command->track,command->mode,s.error);break;
        case AnimationAuthorOperation::LayerDuplicate:ok=candidate.duplicateLayer(command->track,id,s.error);break;
        case AnimationAuthorOperation::LayerRemove:ok=candidate.removeLayer(command->track,s.error);break;
        case AnimationAuthorOperation::LayerCopyBase:ok=candidate.copyBaseToTrack(command->track,s.error);break;
        case AnimationAuthorOperation::LayerAddTrack:
          if(command->mode>1||command->component>3)return s.reject("Modo de cópia ou rotação inválido");
          ok=candidate.addLayerTrack(command->key.id,command->track,command->mode!=0,id,s.error,command->component?std::optional{static_cast<resources::AnimationRotationMode>(command->component-1)}:std::nullopt);break;
      }
      if(!ok||!s.budget(candidate,token))return 0;
      draft->asset=std::move(candidate);draft->changed();if(result)*result=id;return 1;
    };
    api.addTrack=[](void *c,u64 token,resources::AssetGuid sourceNode,const u8 *path,int pathLength,const u8 *name,int nameLength,u32 property,u32 mode,const float *values,int count,u64 *out)->int {
      auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);
      if(!draft)return 0;
      if(!out||property>3||mode>2||count<0||count>static_cast<int>(resources::MaximumMorphTargets)||(!values&&count)||
         !textArgument(path,pathLength,1024)||!textArgument(name,nameLength,256))return s.reject("Binding ou propriedade de autoria inválidos");
      resources::AnimationClipBinding binding{0,sourceNode,textValue(path,pathLength),textValue(name,nameLength)};
      auto candidate=draft->asset;u64 id=0;
      if(!candidate.addTrack(binding,static_cast<resources::AnimationPath>(property),static_cast<resources::AnimationRotationMode>(mode),{values,static_cast<usize>(count)},id,s.error)||!s.budget(candidate,token))return 0;
      draft->asset=std::move(candidate);draft->changed();*out=id;return 1;
    };
    api.commit=[](void *c,u64 token)->int {
      auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);if(!draft)return 0;
      const auto value=draft->asset;
      if(!s.session.editAnimationClipAsset(value.guid,value.revision,[&](auto &candidate){candidate=value;return true;},s.error))return 0;
      std::erase_if(s.drafts,[&](const auto &entry){return entry.token==token;});return 1;
    };
    api.cancel=[](void *c,u64 token)->int {
      auto &s=*static_cast<Impl*>(c);if(!s.find(token))return 0;
      std::erase_if(s.drafts,[&](const auto &entry){return entry.token==token;});return 1;
    };
    api.copy=[](void *c,u64 token,const AnimationAuthorAddress *addresses,int count)->u64 {
      auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);std::vector<Address> selection;
      if(!draft||!s.addresses(addresses,count,selection)||s.clipboards.size()>=8)return 0;
      resources::AnimationKeyClipboard clipboard;
      if(!clipboard.copy(draft->asset,selection,s.error))return 0;
      usize total=clipboard.size();for(const auto &entry:s.clipboards)total+=entry.value.size();
      if(total>Asset::MaximumKeys) {s.reject("Limite de chaves no clipboard de autoria atingido");return 0;}
      const auto id=allocateAuthorHandle();if(!id) {s.reject("Identidades de autoria esgotadas");return 0;}
      s.clipboards.push_back({id,std::move(clipboard)});return id;
    };
    api.paste=[](void *c,u64 token,u64 clipboard,float time,u32 mode)->int {
      auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);if(!draft||mode>2)return 0;
      const auto found=std::find_if(s.clipboards.begin(),s.clipboards.end(),[&](const auto &entry){return entry.token==clipboard;});
      if(found==s.clipboards.end())return s.reject("Clipboard não pertence a esta invocação de autoria");
      auto candidate=draft->asset;std::vector<Address> selection;
      if(!found->value.paste(candidate,time,static_cast<resources::AnimationPasteMode>(mode),selection,s.error)||!s.budget(candidate,token))return 0;
      draft->asset=std::move(candidate);draft->changed();return 1;
    };
    api.transformSelection=[](void *c,u64 token,const AnimationAuthorAddress *addresses,int count,double pivot,double scale,double offset)->int {
      auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);std::vector<Address> selection;
      if(!draft||!s.addresses(addresses,count,selection))return 0;
      auto candidate=draft->asset;
      if(!resources::transformAnimationKeyTimes(candidate,selection,pivot,scale,offset,s.error))return 0;
      draft->asset=std::move(candidate);draft->changed();return 1;
    };
    api.eraseSelection=[](void *c,u64 token,const AnimationAuthorAddress *addresses,int count)->int {
      auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);std::vector<Address> selection;
      if(!draft||!s.addresses(addresses,count,selection))return 0;
      auto candidate=draft->asset;
      if(!resources::eraseAnimationKeySelection(candidate,selection,s.error))return 0;
      draft->asset=std::move(candidate);draft->changed();return 1;
    };
    api.sample=[](void *c,u64 token,u64 trackId,float time,float *values,int capacity)->int {
      auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);
      if(!draft||!std::isfinite(time)||time<0||time>draft->asset.duration)return -1;
      const auto found=std::find_if(draft->asset.tracks.begin(),draft->asset.tracks.end(),[&](const auto &track){return track.id==trackId;});
      if(found==draft->asset.tracks.end()) {s.reject("Canal inexistente");return -1;}
      const int count=found->path==resources::AnimationPath::Rotation?4:static_cast<int>(found->components());
      if(!values||capacity<count)return count;
      resources::AnimationChannel channel;channel.path=found->path;channel.rotationMode=found->rotationMode;
      channel.weightCount=found->weightCount;channel.curves=found->curves;
      if(!resources::sampleAnimationChannel(channel,time,{values,static_cast<usize>(count)})) {s.reject("Canal não produz pose válida");return -1;}
      return count;
    };
    api.sampleComposed=[](void *c,u64 token,u64 trackId,float time,float *values,int capacity)->int {
      auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);
      if(!draft||!std::isfinite(time)||time<0||time>draft->asset.duration)return -1;
      const auto *track=draft->asset.track(trackId);if(!track) {s.reject("Canal inexistente");return -1;}
      const int count=track->path==resources::AnimationPath::Rotation?4:static_cast<int>(track->components());
      if(!values||capacity<count)return count;
      if(!draft->runtimeReady) {if(!draft->asset.compile(draft->compiled,&s.error))return -1;draft->runtimeReady=true;}
      const auto binding=std::find_if(draft->asset.bindings.begin(),draft->asset.bindings.end(),[&](const auto &b){return b.id==track->binding;});
      const auto node=static_cast<u32>(binding-draft->asset.bindings.begin());
      for(const auto &channel:draft->compiled.channels)if(channel.node==node&&channel.path==track->path) {
        if(!resources::sampleValidatedAnimationChannel(channel,time,{values,static_cast<usize>(count)})) {s.reject("Composição não produz pose válida");return -1;}
        return count;
      }
      s.reject("Propriedade composta ausente");return -1;
    };
    api.sampleCurve=[](void *c,u64 token,u64 trackId,u32 component,double time,double *value,double *derivative)->int {
      auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);if(!draft||!value||!derivative)return 0;
      const auto *track=draft->asset.track(trackId);resources::AnimationCurveSample sample;
      if(!track||component>=track->curves.size()||!resources::sampleValidatedAnimationCurve(track->curves[component],time,sample))return s.reject("Curva ou tempo inválidos");
      *value=sample.value;*derivative=sample.derivative;return 1;
    };
    api.diagnostic=[](void *c,u8 *buffer,int capacity)->int {
      const auto &s=*static_cast<Impl*>(c);return s.thread==std::this_thread::get_id()?copyText(s.error,buffer,capacity):-1;
    };
    api.releaseClipboard=[](void *c,u64 token)->int {
      auto &s=*static_cast<Impl*>(c);if(!s.ready())return 0;
      const auto old=s.clipboards.size();std::erase_if(s.clipboards,[&](const auto &entry){return entry.token==token;});
      return s.clipboards.size()!=old?1:s.reject("Clipboard encerrado ou pertencente a outro comando");
    };
    api.bake=[](void *c,u64 token,u64 track,const AnimationAuthorBakeSettings *wire,AnimationAuthorBakeReport *out)->int {
      auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);if(!draft)return 0;
      if(!wire||!out||wire->reserved||(wire->flags&~1u))return s.reject("Flags de bake inválidas");
      resources::AnimationBakeSettings settings;settings.sampleRate=wire->sampleRate;settings.verificationSteps=wire->verificationSteps;
      settings.maximumFrames=wire->maximumFrames;settings.rotation=wire->rotation;settings.reduce=wire->flags&1;settings.tolerance=wire->tolerance;
      auto candidate=draft->asset;resources::AnimationBakeReport report;
      if(!resources::bakeAnimationClipTrack(candidate,track,settings,report,s.error)||!s.budget(candidate,token))return 0;
      draft->asset=std::move(candidate);draft->changed();
      *out={report.inputKeys,report.outputKeys,report.sampledFrames,report.verifiedSamples,report.maximumError};return 1;
    };
    api.bakeAdvanced=[](void *c,u64 token,u64 track,const AnimationAuthorBakeRequest *wire,AnimationAuthorBakeReport *out)->int {
      auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);if(!draft)return 0;
      resources::AnimationBakeSettings settings;
      if(!out||!bakeRequest(wire,settings))return s.reject("Configuração de conversão inválida");
      auto candidate=draft->asset;resources::AnimationBakeReport report;
      if(!resources::bakeAnimationClipTrack(candidate,track,settings,report,s.error)||!s.budget(candidate,token))return 0;
      draft->asset=std::move(candidate);draft->changed();
      *out={report.inputKeys,report.outputKeys,report.sampledFrames,report.verifiedSamples,report.maximumError};return 1;
    };
    api.consolidate=[](void *c,u64 token,const u8 *name,int length,const AnimationAuthorBakeRequest *wire,resources::AssetGuid *out,AnimationAuthorBakeReport *result)->int {
      auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);if(!draft)return 0;
      resources::AnimationBakeSettings settings;
      if(!out||!result||!textArgument(name,length,256)||!bakeRequest(wire,settings))return s.reject("Configuração de consolidação inválida");
      resources::AssetGuid guid;resources::AnimationBakeReport report;
      if(!s.session.createConsolidatedAnimationClip(draft->asset,textValue(name,length),settings,guid,report,s.error))return 0;
      s.authoredCatalogReady=false;*out=guid;*result={report.inputKeys,report.outputKeys,report.sampledFrames,report.verifiedSamples,report.maximumError};return 1;
    };
    api.previewClip=[](void *c,resources::AssetGuid guid,u64 owner,float time)->int {
      auto &s=*static_cast<Impl*>(c);if(!s.ready()||owner>std::numeric_limits<runtime::ObjectId>::max())return 0;
      const auto *asset=s.session.animationClipAsset(guid);
      if(!asset||!std::isfinite(time)||time<0||time>asset->duration)return s.reject("Clipe ou tempo de preview inválido");
      return s.session.openAnimationClip(guid,static_cast<runtime::ObjectId>(owner),s.error)&&s.session.seekAnimationClip(time,s.error)?1:0;
    };
    api.seekPreview=[](void *c,float time)->int {auto &s=*static_cast<Impl*>(c);return s.ready()&&s.session.seekAnimationClip(time,s.error)?1:0;};
    api.stagePose=[](void *c,u64 track,const float *values,int count)->int {
      auto &s=*static_cast<Impl*>(c);if(!s.ready())return 0;
      if(!values||count<1||count>static_cast<int>(resources::MaximumMorphTargets))return s.reject("Componentes da pose inválidos");
      return s.session.previewAnimationClipPose(track,std::span(values,static_cast<usize>(count)),s.error)?1:0;
    };
    api.recordPose=[](void *c)->int {auto &s=*static_cast<Impl*>(c);return s.ready()&&s.session.recordAnimationClipPose(s.error)?1:0;};
    api.cancelPose=[](void *c)->int {auto &s=*static_cast<Impl*>(c);if(!s.ready())return 0;s.session.cancelAnimationClipPose();return 1;};
    api.closePreview=[](void *c)->int {auto &s=*static_cast<Impl*>(c);if(!s.ready())return 0;s.session.closeAnimationClip();return 1;};
  }
  bool ready() {
    // A foreign thread must not even write the diagnostic owned by the editor.
    if(thread!=std::this_thread::get_id())return false;
    if(project.empty()||project!=session.codeProjectRoot()||epoch!=session.sceneVersion().epoch||session.isPlaying())return reject("Contexto de autoria vencido, sem projeto ou em Play");
    error.clear();return true;
  }
  int reject(const char *message) {error=message;return 0;}
  Draft *find(u64 token) {
    if(!ready())return nullptr;
    for(auto &draft:drafts)if(draft.token==token)return &draft;
    reject("Rascunho encerrado ou pertencente a outra invocação");return nullptr;
  }
  bool budget(const Asset &candidate,u64 replaced) {
    usize total=keyCount(candidate);for(const auto &draft:drafts)if(draft.token!=replaced)total+=keyCount(draft.asset);
    if(total>Asset::MaximumKeys)return reject("Limite conjunto de chaves nos rascunhos atingido");
    return true;
  }
  bool addresses(const AnimationAuthorAddress *input,int count,std::vector<Address> &out) {
    if(!input||count<=0||count>static_cast<int>(Asset::MaximumKeys))return reject("Seleção de autoria inválida");
    for(int i=0;i<count;++i) {if(input[i].reserved)return reject("Flags de seleção desconhecidas");out.push_back({input[i].track,input[i].component,input[i].key});}
    return true;
  }
  const std::vector<resources::AssetGuid> &catalog(bool imported) {
    auto &result=imported?importedCatalog:authoredCatalog;
    auto &loaded=imported?importedCatalogReady:authoredCatalogReady;
    if(loaded)return result;
    result.clear();
    if(imported)for(const auto &source:session.mapScene().animationSources())if(source)result.insert(result.end(),source->clipIds.begin(),source->clipIds.end());
    if(!imported)for(const auto &record:session.assets().records())if(session.animationClipAsset(record.guid))result.push_back(record.guid);
    std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());
    if(result.size()>65536) {result.clear();reject("Catálogo excede o limite de clipes por comando");}
    else loaded=true;
    return result;
  }
};
AnimationAuthoringScope::AnimationAuthoringScope(EditorSession &session):impl_(std::make_unique<Impl>(session)) {}
AnimationAuthoringScope::~AnimationAuthoringScope()=default;
const AnimationAuthorAccess &AnimationAuthoringScope::access() const {return impl_->api;}
bool EditorSession::requestEditorTools(std::string_view id) {
  if(files_.rootPath().empty()||isPlaying()||state_.codeBuildBusy||state_.codeToolsBusy||!state_.codeCompilerAvailable) {
    state_.status="Ferramentas exigem um projeto em edição, fora da compilação";return false;
  }
  if(!id.empty()&&(editorToolCatalogRoot_!=files_.rootPath()||editorToolCatalogEpoch_!=sceneVersion().epoch||
      std::none_of(state_.codeToolCatalog.begin(),state_.codeToolCatalog.end(),[&](const auto &tool){return tool.id==id;}))) {
    state_.status="Lista de ferramentas vencida; abra novamente";return false;
  }
  editorToolRequest_={files_.rootPath(),std::string(id),sceneVersion().epoch};state_.codeToolsBusy=true;
  if(id.empty()) {state_.codeToolCatalog.clear();editorToolCatalogRoot_.clear();state_.status="Carregando ferramentas publicadas";}
  else {state_.codeMenu=false;state_.status="Executando ferramenta do editor";}
  return true;
}
void EditorSession::completeEditorToolRequest(const EditorToolRequest &request,bool accepted,std::string_view report) {
  if(request.root!=files_.rootPath()||request.epoch!=sceneVersion().epoch)return;
  state_.codeToolsBusy=false;
  if(!accepted||report.size()>2*1024*1024) {
    state_.status=report.empty()?"Ferramenta indisponível; confira o código publicado":std::string(report.substr(0,4096));
    reportProblem(EditorConsoleSeverity::Error,state_.status);return;
  }
  if(request.id.empty()) {
    std::istringstream input{std::string(report)};std::string signature;u32 version=0,count=0;
    std::vector<EditorToolInfo> tools;
    bool valid=static_cast<bool>(input>>signature>>version>>count)&&signature=="ASTRA_EDITOR_COMMANDS"&&version==1&&count<=256;
    for(u32 index=0;valid&&index<count;++index) {
      EditorToolInfo tool;
      valid=static_cast<bool>(input>>std::quoted(tool.id)>>std::quoted(tool.label))&&!tool.id.empty()&&tool.id.size()<=4096&&!tool.label.empty()&&tool.label.size()<=256&&
          std::none_of(tools.begin(),tools.end(),[&](const auto &other){return other.id==tool.id;});
      if(valid)tools.push_back(std::move(tool));
    }
    input>>std::ws;valid=valid&&input.eof();
    if(!valid) {state_.status="Catálogo de ferramentas inválido";reportProblem(EditorConsoleSeverity::Error,state_.status);return;}
    state_.codeToolCatalog=std::move(tools);editorToolCatalogRoot_=request.root;editorToolCatalogEpoch_=request.epoch;
    state_.status=state_.codeToolCatalog.empty()?"Nenhuma ferramenta no código publicado":"Ferramentas publicadas carregadas";
  } else {
    state_.status=std::string(report.substr(0,4096));reportProblem(EditorConsoleSeverity::Info,state_.status);
  }
}
}
