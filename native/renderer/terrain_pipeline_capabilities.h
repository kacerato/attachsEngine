#pragma once
#include "renderer/terrain_surface_table.h"
#include <array>
#include <compare>
#include <limits>
namespace ae::renderer {
inline constexpr u32 TerrainPipelineCapabilitiesAbi=1;
inline constexpr u32 TerrainCapabilityAdvanced=1, TerrainCapabilityShared=2,
    TerrainCapabilitySnow=4,TerrainCapabilityStaticFx=8,TerrainCapabilityKnown=15;
inline constexpr u32 TerrainCapabilityGeneric=std::numeric_limits<u32>::max();
enum class TerrainPipelineFamily:u32 {Opaque,Coverage,Blend};
struct TerrainPipelineCapabilitiesKey {
 u32 abi=TerrainPipelineCapabilitiesAbi,mask=TerrainCapabilityGeneric;
 TerrainPipelineFamily family=TerrainPipelineFamily::Opaque;
 // Same order as MaterialSpecialization IDs0..5. Includes distant feature masks,
 // clipping, environment, detail and point-light paths, not just material ID.
 std::array<u32,6> existingSpecialization{};
 // Device-local render-pass/layout/shader/raster state generation. Must change
 // whenever any creation state outside the six existing constants changes.
 u64 pipelineEpoch=0;
 auto operator<=>(const TerrainPipelineCapabilitiesKey &) const=default;
 bool specialized()const{return abi==TerrainPipelineCapabilitiesAbi&&mask<=TerrainCapabilityKnown&&pipelineEpoch&&u32(family)<=u32(TerrainPipelineFamily::Blend);}
};
// Input is ONE unrelocated, validated publication table, never a table pointer
// from a different package. Properties remain runtime SSBO data; only capabilities
// enter this key. Paint bytes, texture slots, factors and revisions do not.
inline u32 terrainPipelineCapabilities(std::span<const float> canonical){
 if(canonical.size()<8||canonical[1]!=1)return TerrainCapabilityGeneric;
 std::vector<float> checked(canonical.begin(),canonical.end());std::string reason;
 if(!relocateTerrainSurfaceTable(checked,0,reason))return TerrainCapabilityGeneric;
 u32 mask=canonical[2]>.5f?TerrainCapabilityAdvanced:0;
 if(canonical[5]>.5f){const usize root=usize(canonical[5])-1;
  // Root6/Dynamic and Control32 table2 are deliberately generic until their
  // producers/ABI tests explicitly extend this proof. No guessed bit layout.
  if(canonical[root*4]<1||canonical[root*4]>5)return TerrainCapabilityGeneric;
  if(canonical[root*4+1]>.5f||canonical[root*4+2]>.5f)mask|=TerrainCapabilityShared;
  if(canonical[root*4+7]>.5f)mask|=TerrainCapabilitySnow;
  if(canonical[root*4]>=3)mask|=TerrainCapabilityStaticFx;
 }
 return mask;
}
inline TerrainPipelineCapabilitiesKey terrainPipelineKey(std::span<const float> canonical,
 TerrainPipelineFamily family,std::array<u32,6> existing,u64 epoch){
 TerrainPipelineCapabilitiesKey key;key.mask=terrainPipelineCapabilities(canonical);
 key.family=family;key.existingSpecialization=existing;key.pipelineEpoch=epoch;return key;
}
}
