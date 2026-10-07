#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include <chrono>
#include <fstream>
namespace ae::editor {
namespace {
// Hash de um arquivo grande sem carregá-lo inteiro.
bool hashFile(const std::filesystem::path &path,std::string &hash,u64 &size) {
  std::ifstream input(path,std::ios::binary);if(!input) return false;
  Sha256 sha;std::vector<u8> block(1u<<20);size=0;
  while(input) {
    input.read(reinterpret_cast<char*>(block.data()),static_cast<std::streamsize>(block.size()));
    const auto got=static_cast<usize>(input.gcount());if(!got) break;
    sha.update(std::span<const u8>(block.data(),got));size+=got;
  }
  if(input.bad()) return false;
  hash=Sha256::hex(sha.digest());return true;
}
i64 modifiedTicks(const std::filesystem::path &path) {
  std::error_code ec;const auto time=std::filesystem::last_write_time(path,ec);
  return ec?0:static_cast<i64>(time.time_since_epoch().count());
}
bool streamRecord(const resources::AssetRecord &record) {return record.importerParameters==resources::WaveStreamImporter;}
}

bool EditorSession::importWaveClip(const std::string &relative,resources::AssetGuid &asset,std::string &error){
  error.clear();asset={};
  if(isPlaying()||history_.isOpen()){error="Conclua a edição e pare Play antes de importar WAV";return false;}
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());std::filesystem::path path;std::vector<u8> bytes;
  if(!relative.ends_with(".wav")||relative.starts_with(".astra/")||!EditorImportTransaction::safePath(root,relative,path)){error="Escolha WAV existente dentro do projeto";return false;}
  std::error_code ec;const auto fileSize=std::filesystem::file_size(path,ec);
  if(ec){error="Escolha WAV existente dentro do projeto";return false;}
  const auto *known=assets_.findByPath(relative);
  if(known&&known->type!=resources::AssetType::AudioClip){error="Caminho pertence a outro tipo de recurso";return false;}
  const auto newGuid=[&]{return resources::assetGuidFromSeed("audio:"+relative+":"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));};
  // Até 32 MiB e ~43 s: decodificado e conferido inteiro, publicado pelo journal.
  auto clip=std::make_shared<resources::AudioClip>();std::string decodeError;
  const bool small=fileSize<=resources::AudioClip::MaximumFileBytes&&EditorImportTransaction::read(path,bytes,resources::AudioClip::MaximumFileBytes);
  if(small&&resources::decodeWaveClip(bytes,*clip,decodeError)){
    const auto hash=Sha256::hex(bytes);
    const auto guid=known?known->guid:newGuid();
    auto registry=assets_;resources::AssetRecord record;record.guid=guid;record.type=resources::AssetType::AudioClip;record.path=record.source=relative;record.contentHash=hash;record.importerVersion=1;record.importerParameters="WAV_PCM_STEREO_F32_48000_V1";
    if(!(known?registry.publishImport(guid,hash,1,record.importerParameters,{},{}):registry.add(record))){error="Registro recusou WAV";return false;}
    EditorImportTransaction transaction(files_.rootPath());
    if(!transaction.begin(relative,hash,error))return false;
    if(!transaction.commit(bytes,registry.serialize())){error=transaction.rollback()?"Falha ao publicar WAV; registro anterior restaurado":"Recuperação de WAV pendente no journal";return false;}
    assets_=std::move(registry);assetRegistryDirty_=false;std::erase_if(audioClips_,[&](const auto &v){return v.guid==guid;});
    std::erase_if(audioStreams_,[&](const auto &v){return v.guid==guid;});
    asset=guid;files_.rebuildTree();state_.selectedFile=relative;
    state_.status="WAV registrado · "+std::to_string(clip->seconds())+" s · 48 kHz estéreo · "+relative;
    return true;
  }
  // Maior que o orçamento de memória: registrado para streaming. O arquivo não é
  // reescrito; o registro fica pendente e vai junto com o próximo salvamento.
  if(fileSize>resources::MaximumStreamFileBytes){error="WAV acima de 1 GiB";return false;}
  if(small&&decodeError.find("orçamento")==std::string::npos){error=decodeError;return false;}
  u64 frames=0;if(!resources::inspectWaveStream(path.string(),frames,error))return false;
  std::string hash;u64 size=0;if(!hashFile(path,hash,size)){error="Não foi possível ler o WAV inteiro";return false;}
  const auto guid=known?known->guid:newGuid();
  auto registry=assets_;resources::AssetRecord record;record.guid=guid;record.type=resources::AssetType::AudioClip;record.path=record.source=relative;record.contentHash=hash;record.importerVersion=1;record.importerParameters=resources::WaveStreamImporter;
  if(!(known?registry.publishImport(guid,hash,1,record.importerParameters,{},{}):registry.add(record))){error="Registro recusou WAV";return false;}
  assets_=std::move(registry);assetRegistryDirty_=true;
  std::erase_if(audioClips_,[&](const auto &v){return v.guid==guid;});std::erase_if(audioStreams_,[&](const auto &v){return v.guid==guid;});
  audioStreams_.push_back({guid,hash,size,modifiedTicks(path)});
  asset=guid;files_.rebuildTree();state_.selectedFile=relative;
  state_.status="WAV longo para streaming · "+std::to_string(static_cast<double>(frames)/resources::AudioClip::SampleRate)+" s · use Carregamento Streaming · salve para gravar o registro";
  return true;
}
std::shared_ptr<const resources::AudioClip> EditorSession::loadAudioClip(resources::AssetGuid asset,std::string &error){
  error.clear();const auto *record=assets_.find(asset);
  if(!record||record->type!=resources::AssetType::AudioClip){error="Clipe WAV não registrado neste projeto";return {};}
  if(streamRecord(*record)){error="Clipe longo: escolha Carregamento Streaming na Fonte de áudio";return {};}
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
bool EditorSession::resolveAudioStream(resources::AssetGuid asset,std::string &out,std::string &error){
  error.clear();out.clear();const auto *record=assets_.find(asset);
  if(!record||record->type!=resources::AssetType::AudioClip){error="Clipe WAV não registrado neste projeto";return false;}
  std::filesystem::path path;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),record->path,path)){error="WAV fora do projeto";return false;}
  std::error_code ec;const auto size=std::filesystem::file_size(path,ec);
  if(ec){error="WAV do streaming não está mais no projeto";return false;}
  const auto modified=modifiedTicks(path);
  // O hash completo só é refeito quando tamanho ou data do arquivo mudam.
  const auto known=std::find_if(audioStreams_.begin(),audioStreams_.end(),[&](const auto &v){return v.guid==asset;});
  if(known==audioStreams_.end()||known->size!=size||known->modified!=modified||known->hash!=record->contentHash){
    std::string hash;u64 bytes=0;
    if(!hashFile(path,hash,bytes)){error="Não foi possível ler o WAV do streaming";return false;}
    if(hash!=record->contentHash){error="WAV mudou no disco; selecione o arquivo para reimportar";return false;}
    std::erase_if(audioStreams_,[&](const auto &v){return v.guid==asset;});
    audioStreams_.push_back({asset,hash,bytes,modified});
  }
  out=path.string();return true;
}
bool EditorSession::audioClipAvailable(resources::AssetGuid asset,std::string &error){
  const auto *record=assets_.find(asset);
  if(record&&record->type==resources::AssetType::AudioClip&&streamRecord(*record)){std::string path;return resolveAudioStream(asset,path,error);}
  return bool(loadAudioClip(asset,error));
}
}
