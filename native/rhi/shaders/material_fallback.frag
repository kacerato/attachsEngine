#version 450
#extension GL_GOOGLE_include_directive : require
layout(set=0,binding=0) uniform sampler2D textures[5];
#define MAP(i) textures[i]
#include "material_shading.glsl"
