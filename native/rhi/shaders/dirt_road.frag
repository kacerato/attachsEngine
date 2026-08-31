#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
layout(set=0,binding=0) uniform sampler2D textures[];
// textureIndices come from push constants written once per material batch.
// Every lane in a draw therefore observes the same descriptor index. Marking
// these expressions nonuniform forced the driver to keep the more expensive
// per-lane descriptor path even though buildIndirectBatches groups commands by
// material. The unsized array still needs descriptor indexing, but its index is
// dynamically uniform by construction.
#define BASE_MAP textures[frame.textureIndices.x]
#define NORMAL_MAP textures[frame.textureIndices.y]
#define MR_MAP textures[frame.textureIndices.z]
#define EMISSIVE_MAP textures[frame.textureIndices.w]
#include "dirt_road_shading.glsl"
