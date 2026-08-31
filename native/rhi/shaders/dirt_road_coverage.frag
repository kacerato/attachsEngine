#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
layout(set=0,binding=0) uniform sampler2D textures[];
// Coverage draws use the same material-batched push-constant invariant as the
// shading pass; the descriptor index is dynamically uniform across the draw.
#define BASE_MAP textures[frame.textureIndices.x]
#include "dirt_road_coverage_shading.glsl"
