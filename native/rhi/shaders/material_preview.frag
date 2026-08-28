#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
layout(set=0,binding=0) uniform sampler2D textures[];
#define MAP(i) textures[nonuniformEXT(frame.materialIndex + uint(i))]
#include "material_shading.glsl"
