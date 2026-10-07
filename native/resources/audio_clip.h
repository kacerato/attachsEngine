#pragma once
#include "core/base.h"
#include <span>
#include <string>
#include <vector>
namespace ae::resources {
// Immutable decoded stereo float PCM at 48 kHz. Conversion is import/load work,
// not performed in the real-time callback. Shared voices retain its owner.
struct AudioClip {
  static constexpr u32 SampleRate=48000,Channels=2;
  static constexpr usize MaximumFileBytes=32u*1024u*1024u,MaximumSamples=4u*1024u*1024u;
  std::vector<float> samples;
  u64 frames() const {return samples.size()/Channels;}
  double seconds() const {return double(frames())/SampleRate;}
  bool valid() const {return !samples.empty() && samples.size()%Channels==0 && samples.size()<=MaximumSamples;}
};
bool decodeWaveClip(std::span<const u8> bytes,AudioClip &out,std::string &error);
// WAV longo, lido em blocos durante o Play (streaming). Confere o formato e
// devolve a duração em quadros de 48 kHz sem decodificar o arquivo inteiro.
inline constexpr u64 MaximumStreamFileBytes=1024ull*1024ull*1024ull;
inline constexpr const char *WaveStreamImporter="WAV_STREAM_F32_48000_V1";
bool inspectWaveStream(const std::string &path,u64 &frames,std::string &error);
}
