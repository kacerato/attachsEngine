#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include <chrono>
namespace ae::editor {
bool EditorSession::importWaveClip(const std::string &relative,resources::AssetGuid &asset,std::string &error){
  error.clear();asset={};
  if(isPlaying()||history_.isOpen()){error="Conclua a edição e pare Play antes de importar WAV";return false;}
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());std::filesystem::path path;std::vector<u8> bytes;
  if(!relative.ends_with(".wav")||relative.starts_with(".astra/")||!EditorImportTransaction::safePath(root,relative,path)||
     !EditorImportTransaction::read(path,bytes,resources::AudioClip::MaximumFileBytes)){error="Escolha WAV existente dentro do projeto";return false;}
  auto clip=std::make_shared<resources::AudioClip>();if(!resources::decodeWaveClip(bytes,*clip,error))return false;
  const auto hash=Sha256::hex(bytes);const auto *known=assets_.findByPath(relative);
  if(known&&known->type!=resources::AssetType::AudioClip){error="Caminho pertence a outro tipo de recurso";return false;}
  const auto guid=known?known->guid:resources::assetGuidFromSeed("audio:"+relative+":"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));
  auto registry=assets_;resources::AssetRecord record;record.guid=guid;record.type=resources::AssetType::AudioClip;record.path=record.source=relative;record.contentHash=hash;record.importerVersion=1;record.importerParameters="WAV_PCM_STEREO_F32_48000_V1";
  if(!(known?registry.publishImport(guid,hash,1,record.importerParameters,{},{}):registry.add(record))){error="Registro recusou WAV";return false;}
  EditorImportTransaction transaction(files_.rootPath());
  if(!transaction.begin(relative,hash,error))return false;
  if(!transaction.commit(bytes,registry.serialize())){error=transaction.rollback()?"Falha ao publicar WAV; registro anterior restaurado":"Recuperação de WAV pendente no journal";return false;}
  assets_=std::move(registry);assetRegistryDirty_=false;std::erase_if(audioClips_,[&](const auto &v){return v.guid==guid;});
  asset=guid;files_.rebuildTree();state_.selectedFile=relative;
  state_.status="WAV registrado · "+std::to_string(clip->seconds())+" s · 48 kHz estéreo · "+relative;
  // Cache is filled by bounded load on use. Import inspection itself releases PCM.
  return true;
}
std::shared_ptr<const resources::AudioClip> EditorSession::loadAudioClip(resources::AssetGuid asset,std::string &error){
  error.clear();const auto *record=assets_.find(asset);
  if(!record||record->type!=resources::AssetType::AudioClip){error="Clipe WAV não registrado neste projeto";return {};}
  for(const auto &cached:audioClips_)if(cached.guid==asset&&cached.hash==record->contentHash)return cached.clip;
  std::filesystem::path path;std::vector<u8> bytes;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),record->path,path)||!EditorImportTransaction::read(path,bytes,resources::AudioClip::MaximumFileBytes)) {error="Não foi possível ler WAV do projeto";return {};}
  if(Sha256::hex(bytes)!=record->contentHash){error="WAV mudou no disco; selecione o arquivo para reimportar";return {};}
  auto clip=std::make_shared<resources::AudioClip>();if(!resources::decodeWaveClip(bytes,*clip,error))return {};
  std::erase_if(audioClips_,[&](const auto &v){return v.guid==asset;});
  usize samples=clip->samples.size();for(const auto &c:audioClips_)samples+=c.clip->samples.size();
  if(samples>32u*1024u*1024u){error="Clipes decodificados excedem cache de 128 MiB; reabra o projeto para liberar";return {};}
  audioClips_.push_back({asset,record->contentHash,clip});return clip;
}
}
