#version 450
#extension GL_GOOGLE_include_directive : require

// Same contract as the bindless coverage-shading variant. Keeping the fallback
// separate prevents capability fallback from silently losing early depth tests.
layout(early_fragment_tests) in;
layout(set=0,binding=0) uniform sampler2D baseMap;
layout(set=0,binding=1) uniform sampler2D normalMap;
layout(set=0,binding=2) uniform sampler2D metallicRoughnessMap;
layout(set=0,binding=3) uniform sampler2D emissiveMap;
#define BASE_MAP baseMap
#define NORMAL_MAP normalMap
#define MR_MAP metallicRoughnessMap
#define EMISSIVE_MAP emissiveMap
#include "dirt_road_shading.glsl"
