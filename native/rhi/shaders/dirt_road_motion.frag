#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
layout(set=0,binding=0) uniform sampler2D textures[];
#define BASE_MAP textures[frame.textureIndices.x]
#include "dirt_road_motion_common.glsl"
