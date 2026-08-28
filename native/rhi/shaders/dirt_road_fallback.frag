#version 450
#extension GL_GOOGLE_include_directive : require
layout(set=0,binding=0) uniform sampler2D baseMap;
layout(set=0,binding=1) uniform sampler2D normalMap;
layout(set=0,binding=2) uniform sampler2D mrMap;
layout(set=0,binding=3) uniform sampler2D emissiveMap;
#define BASE_MAP baseMap
#define NORMAL_MAP normalMap
#define MR_MAP mrMap
#define EMISSIVE_MAP emissiveMap
#include "dirt_road_shading.glsl"
