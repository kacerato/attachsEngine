#include "resources/audio_clip.h"
#include "miniaudio.h"
#include <cmath>
#include <cstring>
namespace ae::resources {
bool decodeWaveClip(std::span<const u8> bytes,AudioClip &out,std::string &error) {
  error.clear();
  if(bytes.size()<44 || bytes.size()>AudioClip::MaximumFileBytes ||
     std::memcmp(bytes.data(),"RIFF",4)!=0 || std::memcmp(bytes.data()+8,"WAVE",4)!=0) {
    error="WAV RIFF ausente, inválido ou acima de 32 MiB";return false;
  }
  // Validate the RIFF chunk envelope before asking the decoder to consume it.
  const auto read32=[](const u8 *p){return u32(p[0])|(u32(p[1])<<8)|(u32(p[2])<<16)|(u32(p[3])<<24);};
  if(u64(read32(bytes.data()+4))+8!=bytes.size()) {error="Comprimento RIFF não corresponde ao arquivo";return false;}
  bool format=false,data=false;usize sampleBytes=0;u32 frameBytes=0;
  for(usize p=12;p<bytes.size();) {
    if(bytes.size()-p<8) {error="Chunk WAV truncado";return false;}
    const usize size=read32(bytes.data()+p+4);const usize begin=p+8;
    if(size>bytes.size()-begin) {error="Dados WAV truncados";return false;}
    if(std::memcmp(bytes.data()+p,"fmt ",4)==0) {
      if(format || size<16) {error="Formato WAV duplicado ou incompleto";return false;}
      const auto *f=bytes.data()+begin;const u32 tag=u32(f[0])|(u32(f[1])<<8),channels=u32(f[2])|(u32(f[3])<<8);
      const u32 rate=read32(f+4),bits=u32(f[14])|(u32(f[15])<<8),align=u32(f[12])|(u32(f[13])<<8);
      if((tag!=1 && tag!=3) || channels<1 || channels>2 || rate<8000 || rate>192000 ||
         (tag==1 && bits!=8 && bits!=16 && bits!=24 && bits!=32) || (tag==3 && bits!=32) ||
         align!=channels*(bits/8) || read32(f+8)!=rate*align) {error="WAV requer PCM 8/16/24/32 ou float32, mono/estéreo e 8–192 kHz";return false;}
      format=true;frameBytes=align;
    }
    if(std::memcmp(bytes.data()+p,"data",4)==0) {if(data || !size) {error="Dados WAV duplicados ou vazios";return false;}data=true;sampleBytes=size;}
    const usize padding=size&1u;
    if(padding>bytes.size()-begin-size) {error="Padding WAV ausente";return false;}
    p=begin+size+padding;
  }
  if(!format || !data) {error="WAV sem formato ou amostras";return false;}
  if(sampleBytes%frameBytes) {error="WAV contém quadro PCM incompleto";return false;}
  auto config=ma_decoder_config_init(ma_format_f32,AudioClip::Channels,AudioClip::SampleRate);
  ma_decoder decoder{};
  if(ma_decoder_init_memory(bytes.data(),bytes.size(),&config,&decoder)!=MA_SUCCESS) {error="Decoder recusou WAV";return false;}
  ma_uint64 frames=0;
  if(ma_decoder_get_length_in_pcm_frames(&decoder,&frames)!=MA_SUCCESS || !frames || frames>AudioClip::MaximumSamples/AudioClip::Channels) {
    ma_decoder_uninit(&decoder);error="WAV excede orçamento de PCM decodificado (16 MiB)";return false;
  }
  AudioClip prepared;prepared.samples.resize(usize(frames)*AudioClip::Channels);ma_uint64 read=0;
  const auto result=ma_decoder_read_pcm_frames(&decoder,prepared.samples.data(),frames,&read);ma_decoder_uninit(&decoder);
  if((result!=MA_SUCCESS && result!=MA_AT_END) || read!=frames) {error="Não foi possível decodificar todo WAV";return false;}
  for(float value:prepared.samples) if(!std::isfinite(value) || std::abs(value)>1) {error="PCM não finito ou fora de [-1,1]";return false;}
  out=std::move(prepared);return true;
}
}
