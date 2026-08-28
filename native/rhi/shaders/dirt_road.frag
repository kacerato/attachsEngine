#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
layout(set=0,binding=0) uniform sampler2D textures[];
#define BASE_MAP textures[nonuniformEXT(frame.textureIndices.x)]
#define NORMAL_MAP textures[nonuniformEXT(frame.textureIndices.y)]
#define MR_MAP textures[nonuniformEXT(frame.textureIndices.z)]
#define EMISSIVE_MAP textures[nonuniformEXT(frame.textureIndices.w)]
#include "dirt_road_shading.glsl"
